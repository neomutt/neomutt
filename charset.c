static const char rcsid[]="$Id$";
/*
 * Copyright (C) 1998 Ruslan Ermilov <ru@ucb.crimea.ua>,
 *                    Thomas Roessler <roessler@guug.de>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "mutt.h"
#include "charset.h"

#include <string.h>
#include <ctype.h>

static HASH *Translations = NULL;
static HASH *Charsets = NULL;
static HASH *CharsetAliases = NULL;

static CHARSET *mutt_new_charset(void)
{
  CHARSET *chs;
  
  chs          = safe_malloc(sizeof(CHARSET));
  chs->map     = NULL;
  
  return chs;
}

#if 0

static void mutt_free_charset(CHARSET **chsp)
{
  CHARSET *chs = *chsp;
  
  safe_free((void **) &chs->map);
  safe_free((void **) chsp);
}

#endif

static void canonical_charset(char *dest, size_t dlen, const char *name)
{
  int i;
  
  if(!mutt_strncasecmp(name, "x-", 2))
    name = name + 2;
  
  for(i = 0; name[i] && i < dlen - 1; i++)
  {
    if(strchr("_/. ", name[i]))
      dest[i] = '-';
    else
      dest[i] = tolower(name[i]);
  }
  
  dest[i] = '\0';
}

static CHARSET *load_charset(const char *name)
{
  char path[_POSIX_PATH_MAX];
  char buffer[SHORT_STRING];
  CHARSET *chs;
  FILE *fp = NULL;
  int i;

  chs = mutt_new_charset();

  snprintf(path, sizeof(path), "%s/charsets/%s", SHAREDIR, name);
  if((fp = fopen(path, "r")) == NULL)
    goto bail;
  
  if(fgets(buffer, sizeof(buffer), fp) == NULL)
    goto bail;
  
  if(mutt_strcmp(buffer, CHARSET_MAGIC) != 0)
    goto bail;

  chs->map  = safe_malloc(sizeof(CHARSET_MAP));
  
  for(i = 0; i < 256; i++)
  {
    if(fscanf(fp, "%i", &(*chs->map)[i]) != 1)
    {
      safe_free((void **) &chs->map);
      break;
    }
  }

  bail:
  
  if(fp) fclose(fp);
  return chs;
}

static HASH *load_charset_aliases(void)
{
  FILE *fp;
  char buffer[LONG_STRING];
  char *t;
  HASH *charset_aliases;
  
  sprintf(buffer, "%s/charsets/charsets.alias", SHAREDIR);
  if(!(fp = fopen(buffer, "r")))
     return NULL;

  charset_aliases = hash_create(256);
  
  while(fgets(buffer, sizeof(buffer), fp))
  {
    if((t = strchr(buffer, '\n')))
      *t = '\0';

    if(!(t = strchr(buffer, ' ')))
      continue;
    
    *t++ = '\0';
    hash_insert(charset_aliases, safe_strdup(buffer), safe_strdup(t), 1);
  }
  fclose(fp);
  return charset_aliases;
}

static void init_charsets()
{
  if(Charsets) return;

  Charsets       = hash_create(128);
  Translations   = hash_create(256);
  CharsetAliases = load_charset_aliases();
}

CHARSET *mutt_get_charset(const char *name)
{
  CHARSET *charset;
  char buffer[SHORT_STRING];
  char *real_charset;
  
  if (!name || !*name)
    return (NULL);

  init_charsets();
  canonical_charset(buffer, sizeof(buffer), name);

  if(!CharsetAliases || !(real_charset = hash_find(CharsetAliases, buffer)))
    real_charset = buffer;

  if(!(charset = hash_find(Charsets, real_charset)))
  {
    charset = load_charset(real_charset);
    hash_insert(Charsets, safe_strdup(real_charset), charset, 1);
  }
  return charset;
}

static int translate_char(CHARSET_MAP *to, int ch)
{
  int i;

  if (ch == -1) return '?';
  for (i = 0; i < 256; i++)
  {
    if ((*to)[i] == ch) 
      return i;
  }
  return '?';
}

CHARSET_MAP *build_translation(CHARSET_MAP *from, CHARSET_MAP *to)
{
  int i;
  CHARSET_MAP *map = safe_malloc(sizeof(CHARSET_MAP));

  for(i = 0; i < 256; i++)
    (*map)[i] = translate_char(to, (*from)[i]);

  return map;
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

    if(!from_cs->map || !to_cs->map)
      return NULL;
    
    if((map = build_translation(from_cs->map, to_cs->map)))
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

void mutt_decode_utf8_string(char *str, CHARSET *chs)
{
  char *s, *t;
  int ch, i;
  CHARSET_MAP *map = NULL;
  
  if(chs)
    map = chs->map;
  
  for( s = t = str; *t; s++)
  {
    t = utf_to_unicode(&ch, t);

    if(!map)
    {
      *s = (char) ch;
    }
    else
    {
      for(i = 0, *s = '\0'; i < 256; i++)
      {
	if((*map)[i] == ch)
	{
	  *s = i;
	  break;
	}
      }
    }
      
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

