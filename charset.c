/*
 * Copyright (C) 1999 Thomas Roessler <roessler@guug.de>
 *
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 *
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA
 *     02139, USA.
 */

/*
 * This module deals with POSIX.2 character set definition files.
 */


#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include "mutt.h"
#include "charset.h"

/* Where are character set definition files located? */

#define CHARMAPS_DIR "/usr/share/i18n/charmaps"


/* Define this if you want any dprint () statements in this code */

#undef CHARSET_DEBUG

#ifndef CHARSET_DEBUG
# undef dprint
# define dprint(a, b) (void) a
#endif


/* Module-global variables */

static HASH *Translations = NULL;
static HASH *Charsets = NULL;
static HASH *CharsetAliases = NULL;

/* Function Prototypes */

static CHARDESC *chardesc_new (void);
static CHARDESC *repr2descr (int repr, CHARSET * cs);

static CHARMAP *charmap_new (void);
static CHARMAP *parse_charmap_header (FILE * fp);
static CHARSET *charset_new (size_t hash_size);

static CHARSET_MAP *build_translation (CHARSET * from, CHARSET * to);

static char translate_character (CHARSET * to, const char *symbol);

static int load_charset (const char *filename, CHARSET ** csp, short multbyte);
static int parse_charmap_line (char *line, CHARMAP * m, CHARDESC ** descrp);
static int _cd_compar (const void *a, const void *b);

static void canonical_charset (char *dest, size_t dlen, const char *name);
static void chardesc_free (CHARDESC ** cdp);
static void charmap_free (CHARMAP ** cp);
static void charset_free (CHARSET ** csp);
static void fix_symbol (char *symbol, CHARMAP * m);

static void canonical_charset (char *dest, size_t dlen, const char *name)
{
  int i;

  if (!strncasecmp (name, "x-", 2))
    name = name + 2;

  for (i = 0; name[i] && i < dlen - 1; i++)
  {
    if (strchr ("_/. ", name[i]))
      dest[i] = '-';
    else
      dest[i] = tolower (name[i]);
  }

  dest[i] = '\0';
}

static CHARSET *charset_new (size_t hash_size)
{
  CHARSET *cp = safe_malloc (sizeof (CHARSET));
  short i;

  cp->n_symb = 256;
  cp->u_symb = 0;
  cp->multbyte = 1;
  cp->symb_to_repr = hash_create (hash_size);
  cp->description = safe_malloc (cp->n_symb * sizeof (CHARDESC *));

  for (i = 0; i < cp->n_symb; i++)
    cp->description[i] = NULL;

  return cp;
}

static void charset_free (CHARSET ** csp)
{
  CHARSET *cs = *csp;
  size_t i;

  for (i = 0; i < cs->n_symb; i++)
    chardesc_free (&cs->description[i]);

  safe_free ((void **) &cs->description);

  hash_destroy (&cs->symb_to_repr, NULL);
  safe_free ((void **) csp);
}

static CHARMAP *charmap_new (void)
{
  CHARMAP *m = safe_malloc (sizeof (CHARMAP));

  m->charset = NULL;
  m->escape_char = '\\';
  m->comment_char = '#';
  m->multbyte = 1;
  m->aliases = NULL;
  
  return m;
}

static void charmap_free (CHARMAP ** cp)
{
  if (!cp || !*cp)
    return;

  mutt_free_list (&(*cp)->aliases);
  safe_free ((void **) &(*cp)->charset);
  safe_free ((void **) cp);

  return;
}

static CHARDESC *chardesc_new (void)
{
  CHARDESC *p = safe_malloc (sizeof (CHARDESC));

  p->symbol = NULL;
  p->repr = -1;

  return p;
}

static void chardesc_free (CHARDESC ** cdp)
{
  if (!cdp || !*cdp)
    return;


  safe_free ((void **) &(*cdp)->symbol);
  safe_free ((void **) cdp);

  return;
}

static CHARMAP *parse_charmap_header (FILE * fp)
{
  char buffer[1024];
  char *t, *u;
  CHARMAP *m = charmap_new ();

  while (fgets (buffer, sizeof (buffer), fp))
  {
    if ((t = strchr (buffer, '\n')))
      *t = '\0';
    else
    {
      charmap_free (&m);
      return NULL;
    }

    if (!strncmp (buffer, "CHARMAP", 7))
      break;

    if (*buffer == m->comment_char)
    {
      if ((t = strtok (buffer + 1, "\t ")) && !strcasecmp (t, "alias"))
      {
	char _tmp[SHORT_STRING];
	while ((t = strtok(NULL, "\t, ")))
	{
	  canonical_charset (_tmp, sizeof (_tmp), t);
	  m->aliases = mutt_add_list (m->aliases, _tmp);
	}
      }
      continue;
    }

    if (!(t = strtok (buffer, "\t ")))
      continue;

    if (!(u = strtok (NULL, "\t ")))
    {
      charmap_free (&m);
      return NULL;
    }

    if (!strcmp (t, "<code_set_name>"))
    {
      safe_free ((void **) &m->charset);
      canonical_charset (u, strlen (u) + 1, u);
      m->charset = safe_strdup (u);
    }
    else if (!strcmp (t, "<comment_char>"))
    {
      m->comment_char = *u;
    }
    else if (!strcmp (t, "<escape_char>"))
    {
      m->escape_char = *u;
    }
    else if (!strcmp (t, "<mb_cur_max>"))
    {
      m->multbyte = strtol (u, NULL, 0);
    }
  }

  return m;
}

/* Properly handle escape characters within a symbol. */

static void fix_symbol (char *symbol, CHARMAP * m)
{
  char *s, *d;

  for (s = symbol, d = symbol; *s; *d++ = *s++)
  {
    if (*s == m->escape_char)
      s++;
  }

  *d = *s;
}

enum
{
  CL_DESCR,
  CL_END,
  CL_COMMENT,
  CL_ERROR
};

static int parse_charmap_line (char *line, CHARMAP * m, CHARDESC ** descrp)
{
  char *t, *u;
  short n;
  CHARDESC *descr;

  if (*line == m->comment_char)
    return CL_COMMENT;

  descr = *descrp = chardesc_new ();

  if (!strncmp (line, "END CHARMAP", 11))
  {
    chardesc_free (descrp);
    return CL_END;
  }

  for (t = line; *t && isspace (*t); t++)
    ;

  if (*t++ != '<')
  {
    chardesc_free (descrp);
    return CL_ERROR;
  }

  for (u = t; *u && *u != '>'; u++)
  {
    if (*u == m->escape_char && u[1])
      u++;
  }

  if (*u != '>')
  {
    chardesc_free (descrp);
    return CL_ERROR;
  }

  *u++ = '\0';
  descr->symbol = safe_strdup (t);
  fix_symbol (descr->symbol, m);

  for (t = u; *t && isspace (*t); t++)
    ;

  for (u = t; *u && !isspace (*u); u++)
    ;

  *u++ = 0;
  descr->repr = 0;

  for (n = 0; *t == m->escape_char && n < m->multbyte; n++)
  {
    switch (*++t)
    {
    case 'x':
      descr->repr = descr->repr * 256 + strtol (++t, &t, 16);
      break;
    case 'd':
      descr->repr = descr->repr * 256 + strtol (++t, &t, 10);
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      descr->repr = descr->repr * 256 + strtol (t, &t, 8);
      break;
    default:
      chardesc_free (descrp);
      return CL_ERROR;
    }
  }

  if (!n)
  {
    chardesc_free (descrp);
    return CL_ERROR;
  }

  return CL_DESCR;
}

static int _cd_compar (const void *a, const void *b)
{
  const CHARDESC *ap, *bp;
  int i;

  ap = * (CHARDESC **) a;
  bp = * (CHARDESC **) b;

  i = ap->repr - bp->repr;

  dprint (98, (debugfile, "_cd_compar: { %x, %s }, { %x, %s } -> %d\n",
	       ap->repr, ap->symbol, bp->repr, bp->symbol, i));
  
  return i;
}

/*
 * Load a character set description into memory.
 * 
 * The multibyte parameter tells us whether we are going
 * to accept multibyte character sets.
 */

static int load_charset (const char *filename, CHARSET ** csp, short multbyte)
{
  CHARDESC *cd = NULL;
  CHARSET *cs = NULL;
  CHARMAP *m = NULL;
  FILE *fp;
  char buffer[1024];
  int i;
  int rv = -1;

  cs = *csp = charset_new (multbyte ? 1031 : 257);

  dprint (2, (debugfile, "load_charset: Trying to open: %s\n", filename));
  
  if ((fp = fopen (filename, "r")) == NULL)
  {
    char _filename[_POSIX_PATH_MAX];

    snprintf (_filename, sizeof (_filename), "%s/%s", CHARMAPS_DIR, filename);
    dprint (2, (debugfile, "load_charset: Trying to open: %s\n", _filename));
    
    if ((fp = fopen (_filename, "r")) == NULL)
    {
      dprint (2, (debugfile, "load_charset: Failed.\n"));
      goto bail;
    }
  }
      
  if ((m = parse_charmap_header (fp)) == NULL)
    goto bail;

  /* Don't handle multibyte character sets unless explicitly requested
   * to do so.
   */

  if (m->multbyte > 1 && !multbyte)
  {
    dprint (2, (debugfile, "load_charset: m->multbyte == %d\n",
		(int) m->multbyte));
    goto bail;
  }

  cs->multbyte = m->multbyte;

  while (fgets (buffer, sizeof (buffer), fp) != NULL)
  {
    i = parse_charmap_line (buffer, m, &cd);

    if (i == CL_END)
      break;
    else if (i == CL_DESCR)
    {
      dprint (5, (debugfile, "load_charset: Got character description: < %s > -> %x\n",
		  cd->symbol, cd->repr));
      hash_delete (cs->symb_to_repr, cd->symbol, NULL, NULL);
      hash_insert (cs->symb_to_repr, cd->symbol, cd, 0);

      if (!multbyte)
      {
	if (0 <= cd->repr && cd->repr < 256)
	{
	  if (cs->description[cd->repr])
	    chardesc_free (&cs->description[cd->repr]);
	  else
	    cs->u_symb++;

	  cs->description[cd->repr] = cd;
	  cd = NULL;
	}
      }
      else
      {
	if (cs->u_symb == cs->n_symb)
	{
	  size_t new_size = cs->n_symb + 256;
	  size_t i;

	  safe_realloc ((void **) &cs->description, new_size * sizeof (CHARDESC *));
	  for (i = cs->u_symb; i < new_size; i++)
	    cs->description[i] = NULL;
	  cs->n_symb = new_size;
	}

	cs->description[cs->u_symb++] = cd;
	cd = NULL;
      }
    }

    chardesc_free (&cd);
  }

  if (multbyte)
    qsort (cs->description, cs->u_symb, sizeof (CHARDESC *), _cd_compar);

  rv = 0;

bail:
  charmap_free (&m);
  if (fp)
    fclose (fp);
  if (rv)
    charset_free (csp);

  return rv;
}

static CHARDESC *repr2descr (int repr, CHARSET * cs)
{
  CHARDESC key;

  if (!cs || repr < 0)
    return NULL;

  if (cs->multbyte == 1)
  {
    if (repr < 256)
      return cs->description[repr];
    else
      return NULL;
  }

  key.repr = repr;
  return bsearch (&key, cs->description, cs->u_symb,
		  sizeof (CHARDESC *), _cd_compar);
}

/* Build a translation table.  If a character cannot be
 * translated correctly, we try to find an approximation
 * from the portable charcter set.
 *
 * Note that this implies the assumption that the portable
 * character set can be used without any conversion.
 *
 * Should be safe on POSIX systems.
 */

static char translate_character (CHARSET * to, const char *symbol)
{
  CHARDESC *cdt;

  if ((cdt = hash_find (to->symb_to_repr, symbol)))
    return (char) cdt->repr;
  else
    return *symbol;
}

static CHARSET_MAP *build_translation (CHARSET * from, CHARSET * to)
{
  int i;
  CHARSET_MAP *map;
  CHARDESC *cd;

  /* This is for 8-bit character sets. */

  if (!from || !to || from->multbyte > 1 || to->multbyte > 1)
    return NULL;

  map = safe_malloc (sizeof (CHARSET_MAP));
  for (i = 0; i < 256; i++)
  {
    if (!(cd = repr2descr (i, from)))
      (*map)[i] = '?';
    else
      (*map)[i] = translate_character (to, cd->symbol);
  }

  return map;
}

/* Currently, just scan the various charset definition files.
 * On the long run, we should cache this stuff in a file.
 */

static HASH *load_charset_aliases (void)
{
  HASH *charset_aliases;
  CHARMAP *m;
  DIR *dp;
  FILE *fp;
  struct dirent *de;

  if ((dp = opendir (CHARMAPS_DIR)) == NULL)
    return NULL;

  charset_aliases = hash_create(127);

  while ((de = readdir (dp)))
  {
    char fnbuff[_POSIX_PATH_MAX];

    if (*de->d_name == '.')
      continue;

    snprintf (fnbuff, sizeof (fnbuff), "%s/%s", CHARMAPS_DIR, de->d_name);
    dprint (2, (debugfile, "load_charset_aliases: Opening %s\n", fnbuff));
    if ((fp = fopen (fnbuff, "r")) == NULL)
      continue;
    
    if ((m = parse_charmap_header (fp)) != NULL)
    {
      LIST *lp;
      char buffer[LONG_STRING];
      
      canonical_charset (buffer, sizeof (buffer), de->d_name);
      m->aliases = mutt_add_list (m->aliases, buffer);

      if (m->charset)
	m->aliases = mutt_add_list (m->aliases, m->charset);
      
      for (lp = m->aliases; lp; lp = lp->next)
      {
	if (lp->data)
	{
	  dprint (2, (debugfile, "load_charset_aliases: %s -> %s\n",
		      lp->data, de->d_name));
	  if (hash_find (charset_aliases, lp->data))
	  {
	    dprint (2, (debugfile, "load_charset_aliases: %s already mapped.\n",
			lp->data));
	  }
	  else
	    hash_insert (charset_aliases, safe_strdup (lp->data), safe_strdup (de->d_name), 0);
	}
      }

      charmap_free (&m);
    }
    
    fclose (fp);
  }
    
  closedir (dp);
  return charset_aliases;
}

static void init_charsets ()
{
  if (Charsets) return;

  Charsets       = hash_create (127);
  Translations   = hash_create (127);
  CharsetAliases = load_charset_aliases ();
}

CHARSET *mutt_get_charset (const char *name)
{
  CHARSET *charset;
  char buffer[SHORT_STRING];
  char *real_charset;
  
  if (!name || !*name)
    return (NULL);

  init_charsets();
  canonical_charset(buffer, sizeof(buffer), name);

  dprint (2, (debugfile, "mutt_get_charset: Looking for %s\n", buffer));
  
  if(!CharsetAliases || !(real_charset = hash_find(CharsetAliases, buffer)))
    real_charset = buffer;

  dprint (2, (debugfile, "mutt_get_charset: maps to: %s\n", real_charset));
  
  if(!(charset = hash_find (Charsets, real_charset)))
  {
    dprint (2, (debugfile, "mutt_get_charset: Need to load.\n"));
    if (load_charset(real_charset, &charset, 0) == 0)
      hash_insert(Charsets, safe_strdup(real_charset), charset, 1);
    else
      charset = NULL;
  }
  return charset;
}

CHARSET_MAP *mutt_get_translation(const char *_from, const char *_to)
{
  char from_canon[SHORT_STRING];
  char to_canon[SHORT_STRING];
  char key[SHORT_STRING];
  char *from, *to;
  CHARSET *from_cs, *to_cs;
  CHARSET_MAP *map;

  if(!_from || !_to)
    return NULL;
  
  init_charsets();

  canonical_charset(from_canon, sizeof(from_canon), _from);
  canonical_charset(to_canon, sizeof(to_canon), _to);

  if(!CharsetAliases || !(from = hash_find(CharsetAliases, from_canon)))
    from = from_canon;
  if(!CharsetAliases || !(to = hash_find(CharsetAliases, to_canon)))
    to = to_canon;
  
  /* quick check for the identity mapping */
  if((from == to) || ((*from == *to) && !mutt_strcmp(from, to)))
    return NULL;
  
  snprintf(key, sizeof(key), "%s %s", from, to);
  if((map = hash_find(Translations, key)) == NULL)
  {
    from_cs = mutt_get_charset(from);
    to_cs   = mutt_get_charset(to);

    if((map = build_translation(from_cs, to_cs)))
      hash_insert(Translations, safe_strdup(key), map, 1);
  }
  return map;
}

unsigned char mutt_display_char(unsigned char ch, CHARSET_MAP *map)
{
  if (!map || !ch)
    return ch;
  
  return (unsigned char) (*map)[ch];
}

int mutt_display_string(char *str, CHARSET_MAP *map)
{
  if(!map)
    return -1;

  while ((*str = mutt_display_char((unsigned char)*str, map)))
    str++;
  
  return 0;
}

/*************************************************************/
/* UTF-8 support                                             */

int mutt_is_utf8(const char *s)
{
  char buffer[SHORT_STRING];

  if(!s) 
    return 0;

  canonical_charset(buffer, sizeof(buffer), s);
  return !mutt_strcmp(buffer, "utf-8");
}
  
/* macros for the various bit maps we need */

#define IOOOOOOO 0x80
#define IIOOOOOO 0xc0
#define IIIOOOOO 0xe0
#define IIIIOOOO 0xf0
#define IIIIIOOO 0xf8
#define IIIIIIOO 0xfc
#define IIIIIIIO 0xfe
#define IIIIIIII 0xff

static struct unicode_mask
{
  int mask;
  int value;
  short len;
}
unicode_masks[] = 
{
  { IOOOOOOO,	    0,   1 },
  { IIIOOOOO, IIOOOOOO,  2 },
  { IIIIOOOO, IIIOOOOO,  3 },
  { IIIIIOOO, IIIIOOOO,  4 },
  { IIIIIIOO, IIIIIOOO,  5 },
  { IIIIIIIO, IIIIIIOO,  6 },
  {        0,	     0,  0 }
};


static char *utf_to_unicode(int *out, char *in)
{
  struct unicode_mask *um = NULL;
  short i;
  
  for(i = 0; unicode_masks[i].mask; i++)
  {
    if((*in & unicode_masks[i].mask) == unicode_masks[i].value)
    {
      um = &unicode_masks[i];
      break;
    }
  }
  
  if(!um)
  {
    *out = (int) '?';
    return in + 1;
  }

  for(i = 1; i < um->len; i++)
  {
    if((in[i] & IIOOOOOO) != IOOOOOOO)
    {
      *out = (int) '?';
      return in + i;
    }
  }
  
  *out = ((int)in[0]) & ~um->mask & 0xff;
  for(i = 1; i < um->len; i++)
    *out = (*out << 6) | (((int)in[i]) & ~IIOOOOOO & 0xff);

  if(!*out) 
    *out = '?';
  
  return in + um->len;
}

static CHARSET *Unicode = NULL;

void mutt_decode_utf8_string(char *str, CHARSET *chs)
{
  char *s, *t;
  CHARDESC *cd;
  int ch;

  /* Hack */
  
  if (!Unicode)
  {
    if (load_charset ("ISO_10646", &Unicode, 1) == -1)
      Unicode = NULL;
  }
  
  for (s = t = str; *t; s++)
  {
    t = utf_to_unicode(&ch, t);

    /* handle us-ascii characters directly */
    if (0 <= ch && ch < 128)
      *s = ch;
    else if ((cd = repr2descr (ch, Unicode)) && (ch = translate_character (chs, cd->symbol)) != -1)
      *s = ch;
    else
      *s = '?';

    if(!*s) *s = '?';
  }
  
  *s = '\0';
}

static char *sfu_buffer = NULL;
static size_t sfu_blen = 0;
static size_t sfu_bp = 0;

static void _state_utf8_flush(STATE *s, CHARSET *chs)
{
  char *t;
  if(!sfu_buffer || !sfu_bp)
    return;
  
  sfu_buffer[sfu_bp] = '\0';
  
  mutt_decode_utf8_string(sfu_buffer, chs);
  for(t = sfu_buffer; *t; t++)
  {
    /* This may lead to funny-looking output if 
     * there are embedded CRs, NLs or similar things
     * - but these would constitute illegal 
     * UTF8 encoding anyways, so we don't care.
     */

    state_prefix_putc(*t, s);
  }
  sfu_bp = 0;
}
    
void state_fput_utf8(STATE *st, char u, CHARSET *chs)
{
  if((u & 0x80) == 0 || (sfu_bp && (u & IIOOOOOO) != IOOOOOOO))
    _state_utf8_flush(st, chs);
     
  if((u & 0x80) == 0)
  {
    if(u) state_prefix_putc(u, st);
  }
  else
  {
    if(sfu_bp + 1 >= sfu_blen)
    {
      sfu_blen = (sfu_blen + 80) * 2;
      safe_realloc((void **) &sfu_buffer, sfu_blen);
    }
    sfu_buffer[sfu_bp++] = u;
  }
}

