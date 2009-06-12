/*
 * Copyright (C) 1996-7,2007 Michael R. Elkins <me@mutt.org>
 * Copyright (c) 1998-2003 Thomas Roessler <roessler@does-not-exist.org>
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
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mime.h"
#include "pgp.h"
#include "pager.h"
#include "sort.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <locale.h>

#ifdef CRYPT_BACKEND_CLASSIC_PGP

struct pgp_cache
{
  char *what;
  char *dflt;
  struct pgp_cache *next;
};

static struct pgp_cache *id_defaults = NULL;

static char trust_flags[] = "?- +";

static char *pgp_key_abilities (int flags)
{
  static char buff[3];

  if (!(flags & KEYFLAG_CANENCRYPT))
    buff[0] = '-';
  else if (flags & KEYFLAG_PREFER_SIGNING)
    buff[0] = '.';
  else
    buff[0] = 'e';

  if (!(flags & KEYFLAG_CANSIGN))
    buff[1] = '-';
  else if (flags & KEYFLAG_PREFER_ENCRYPTION)
    buff[1] = '.';
  else
    buff[1] = 's';

  buff[2] = '\0';

  return buff;
}

static char pgp_flags (int flags)
{
  if (flags & KEYFLAG_REVOKED)
    return 'R';
  else if (flags & KEYFLAG_EXPIRED)
    return 'X';
  else if (flags & KEYFLAG_DISABLED)
    return 'd';
  else if (flags & KEYFLAG_CRITICAL)
    return 'c';
  else 
    return ' ';
}

static pgp_key_t pgp_principal_key (pgp_key_t key)
{
  if (key->flags & KEYFLAG_SUBKEY && key->parent)
    return key->parent;
  else
    return key;
}

/*
 * Format an entry on the PGP key selection menu.
 * 
 * %n	number
 * %k	key id		%K 	key id of the principal key
 * %u	user id
 * %a	algorithm	%A      algorithm of the princ. key
 * %l	length		%L	length of the princ. key
 * %f	flags		%F 	flags of the princ. key
 * %c	capabilities	%C	capabilities of the princ. key
 * %t	trust/validity of the key-uid association
 * %[...] date of key using strftime(3)
 */

typedef struct pgp_entry
{
  size_t num;
  pgp_uid_t *uid;
} pgp_entry_t;

static const char *pgp_entry_fmt (char *dest,
				  size_t destlen,
				  size_t col,
				  char op,
				  const char *src,
				  const char *prefix,
				  const char *ifstring,
				  const char *elsestring,
				  unsigned long data,
				  format_flag flags)
{
  char fmt[16];
  pgp_entry_t *entry;
  pgp_uid_t *uid;
  pgp_key_t key, pkey;
  int kflags = 0;
  int optional = (flags & M_FORMAT_OPTIONAL);

  entry = (pgp_entry_t *) data;
  uid   = entry->uid;
  key   = uid->parent;
  pkey  = pgp_principal_key (key);

  if (isupper ((unsigned char) op))
    key = pkey;

  kflags = key->flags | (pkey->flags & KEYFLAG_RESTRICTIONS)
    | uid->flags;

  switch (ascii_tolower (op))
  {
    case '[':

      {
	const char *cp;
	char buf2[SHORT_STRING], *p;
	int do_locales;
	struct tm *tm;
	size_t len;

	p = dest;

	cp = src;
	if (*cp == '!')
	{
	  do_locales = 0;
	  cp++;
	}
	else
	  do_locales = 1;

	len = destlen - 1;
	while (len > 0 && *cp != ']')
	{
	  if (*cp == '%')
	  {
	    cp++;
	    if (len >= 2)
	    {
	      *p++ = '%';
	      *p++ = *cp;
	      len -= 2;
	    }
	    else
	      break; /* not enough space */
	    cp++;
	  }
	  else
	  {
	    *p++ = *cp++;
	    len--;
	  }
	}
	*p = 0;

	if (do_locales && Locale)
	  setlocale (LC_TIME, Locale);

	tm = localtime (&key->gen_time);

	strftime (buf2, sizeof (buf2), dest, tm);

	if (do_locales)
	  setlocale (LC_TIME, "C");

	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, buf2);
	if (len > 0)
	  src = cp + 1;
      }
      break;
    case 'n':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	snprintf (dest, destlen, fmt, entry->num);
      }
      break;
    case 'k':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, _pgp_keyid (key));
      }
      break;
    case 'u':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, uid->addr);
      }
      break;
    case 'a':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, key->algorithm);
      }
      break;
    case 'l':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	snprintf (dest, destlen, fmt, key->keylen);
      }
      break;
    case 'f':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sc", prefix);
	snprintf (dest, destlen, fmt, pgp_flags (kflags));
      }
      else if (!(kflags & (KEYFLAG_RESTRICTIONS)))
        optional = 0;
      break;
    case 'c':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, pgp_key_abilities (kflags));
      }
      else if (!(kflags & (KEYFLAG_ABILITIES)))
        optional = 0;
      break;
    case 't':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sc", prefix);
	snprintf (dest, destlen, fmt, trust_flags[uid->trust & 0x03]);
      }
      else if (!(uid->trust & 0x03))
        /* undefined trust */
        optional = 0;
      break;
    default:
      *dest = '\0';
  }

  if (optional)
    mutt_FormatString (dest, destlen, col, ifstring, mutt_attach_fmt, data, 0);
  else if (flags & M_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, col, elsestring, mutt_attach_fmt, data, 0);
  return (src);
}

static void pgp_entry (char *s, size_t l, MUTTMENU * menu, int num)
{
  pgp_uid_t **KeyTable = (pgp_uid_t **) menu->data;
  pgp_entry_t entry;

  entry.uid = KeyTable[num];
  entry.num = num + 1;

  mutt_FormatString (s, l, 0, NONULL (PgpEntryFormat), pgp_entry_fmt, 
		     (unsigned long) &entry, M_FORMAT_ARROWCURSOR);
}

static int _pgp_compare_address (const void *a, const void *b)
{
  int r;

  pgp_uid_t **s = (pgp_uid_t **) a;
  pgp_uid_t **t = (pgp_uid_t **) b;

  if ((r = mutt_strcasecmp ((*s)->addr, (*t)->addr)))
    return r > 0;
  else
    return (mutt_strcasecmp (_pgp_keyid ((*s)->parent),
			     _pgp_keyid ((*t)->parent)) > 0);
}

static int pgp_compare_address (const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_pgp_compare_address (a, b)
				       : _pgp_compare_address (a, b));
}



static int _pgp_compare_keyid (const void *a, const void *b)
{
  int r;

  pgp_uid_t **s = (pgp_uid_t **) a;
  pgp_uid_t **t = (pgp_uid_t **) b;

  if ((r = mutt_strcasecmp (_pgp_keyid ((*s)->parent), 
			    _pgp_keyid ((*t)->parent))))
    return r > 0;
  else
    return (mutt_strcasecmp ((*s)->addr, (*t)->addr)) > 0;
}

static int pgp_compare_keyid (const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_pgp_compare_keyid (a, b)
				       : _pgp_compare_keyid (a, b));
}

static int _pgp_compare_date (const void *a, const void *b)
{
  int r;
  pgp_uid_t **s = (pgp_uid_t **) a;
  pgp_uid_t **t = (pgp_uid_t **) b;

  if ((r = ((*s)->parent->gen_time - (*t)->parent->gen_time)))
    return r > 0;
  return (mutt_strcasecmp ((*s)->addr, (*t)->addr)) > 0;
}

static int pgp_compare_date (const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_pgp_compare_date (a, b)
				       : _pgp_compare_date (a, b));
}

static int _pgp_compare_trust (const void *a, const void *b)
{
  int r;

  pgp_uid_t **s = (pgp_uid_t **) a;
  pgp_uid_t **t = (pgp_uid_t **) b;

  if ((r = (((*s)->parent->flags & (KEYFLAG_RESTRICTIONS))
	    - ((*t)->parent->flags & (KEYFLAG_RESTRICTIONS)))))
    return r > 0;
  if ((r = ((*s)->trust - (*t)->trust)))
    return r < 0;
  if ((r = ((*s)->parent->keylen - (*t)->parent->keylen)))
    return r < 0;
  if ((r = ((*s)->parent->gen_time - (*t)->parent->gen_time)))
    return r < 0;
  if ((r = mutt_strcasecmp ((*s)->addr, (*t)->addr)))
    return r > 0;
  return (mutt_strcasecmp (_pgp_keyid ((*s)->parent), 
			   _pgp_keyid ((*t)->parent))) > 0;
}

static int pgp_compare_trust (const void *a, const void *b)
{
  return ((PgpSortKeys & SORT_REVERSE) ? !_pgp_compare_trust (a, b)
				       : _pgp_compare_trust (a, b));
}

static int pgp_key_is_valid (pgp_key_t k)
{
  pgp_key_t pk = pgp_principal_key (k);
  if (k->flags & KEYFLAG_CANTUSE)
    return 0;
  if (pk->flags & KEYFLAG_CANTUSE)
    return 0;

  return 1;
}

static int pgp_id_is_strong (pgp_uid_t *uid)
{
  if ((uid->trust & 3) < 3)
    return 0;
  /* else */
  return 1;
}

static int pgp_id_is_valid (pgp_uid_t *uid)
{
  if (!pgp_key_is_valid (uid->parent))
    return 0;
  if (uid->flags & KEYFLAG_CANTUSE)
    return 0;
  /* else */
  return 1;
}

#define PGP_KV_VALID  	1
#define PGP_KV_ADDR   	2
#define PGP_KV_STRING 	4
#define PGP_KV_STRONGID 8

#define PGP_KV_MATCH (PGP_KV_ADDR|PGP_KV_STRING)

static int pgp_id_matches_addr (ADDRESS *addr, ADDRESS *u_addr, pgp_uid_t *uid)
{
  int rv = 0;

  if (pgp_id_is_valid (uid))
    rv |= PGP_KV_VALID;

  if (pgp_id_is_strong (uid))
    rv |= PGP_KV_STRONGID;

  if (addr->mailbox && u_addr->mailbox
      && mutt_strcasecmp (addr->mailbox, u_addr->mailbox) == 0)
    rv |= PGP_KV_ADDR;

  if (addr->personal && u_addr->personal
      && mutt_strcasecmp (addr->personal, u_addr->personal) == 0)
    rv |= PGP_KV_STRING;

  return rv;
}

static pgp_key_t pgp_select_key (pgp_key_t keys,
                                 ADDRESS * p, const char *s)
{
  int keymax;
  pgp_uid_t **KeyTable;
  MUTTMENU *menu;
  int i, done = 0;
  char helpstr[LONG_STRING], buf[LONG_STRING], tmpbuf[STRING];
  char cmd[LONG_STRING], tempfile[_POSIX_PATH_MAX];
  FILE *fp, *devnull;
  pid_t thepid;
  pgp_key_t kp;
  pgp_uid_t *a;
  int (*f) (const void *, const void *);

  int unusable = 0;

  keymax = 0;
  KeyTable = NULL;

  for (i = 0, kp = keys; kp; kp = kp->next)
  {
    if (!option (OPTPGPSHOWUNUSABLE) && (kp->flags & KEYFLAG_CANTUSE))
    {
      unusable = 1;
      continue;
    }

    for (a = kp->address; a; a = a->next)
    {
      if (!option (OPTPGPSHOWUNUSABLE) && (a->flags & KEYFLAG_CANTUSE))
      {
	unusable = 1;
	continue;
      }

      if (i == keymax)
      {
	keymax += 5;
	safe_realloc (&KeyTable, sizeof (pgp_uid_t *) * keymax);
      }

      KeyTable[i++] = a;
    }
  }

  if (!i && unusable)
  {
    mutt_error _("All matching keys are expired, revoked, or disabled.");
    mutt_sleep (1);
    return NULL;
  }

  switch (PgpSortKeys & SORT_MASK)
  {
    case SORT_DATE:
      f = pgp_compare_date;
      break;
    case SORT_KEYID:
      f = pgp_compare_keyid;
      break;
    case SORT_ADDRESS:
      f = pgp_compare_address;
      break;
    case SORT_TRUST:
    default:
      f = pgp_compare_trust;
      break;
  }
  qsort (KeyTable, i, sizeof (pgp_uid_t *), f);

  helpstr[0] = 0;
  mutt_make_help (buf, sizeof (buf), _("Exit  "), MENU_PGP, OP_EXIT);
  strcat (helpstr, buf);	/* __STRCAT_CHECKED__ */
  mutt_make_help (buf, sizeof (buf), _("Select  "), MENU_PGP,
		  OP_GENERIC_SELECT_ENTRY);
  strcat (helpstr, buf);	/* __STRCAT_CHECKED__ */
  mutt_make_help (buf, sizeof (buf), _("Check key  "), MENU_PGP, OP_VERIFY_KEY);
  strcat (helpstr, buf);	/* __STRCAT_CHECKED__ */
  mutt_make_help (buf, sizeof (buf), _("Help"), MENU_PGP, OP_HELP);
  strcat (helpstr, buf);	/* __STRCAT_CHECKED__ */

  menu = mutt_new_menu (MENU_PGP);
  menu->max = i;
  menu->make_entry = pgp_entry;
  menu->help = helpstr;
  menu->data = KeyTable;

  if (p)
    snprintf (buf, sizeof (buf), _("PGP keys matching <%s>."), p->mailbox);
  else
    snprintf (buf, sizeof (buf), _("PGP keys matching \"%s\"."), s);


  menu->title = buf;

  kp = NULL;

  mutt_clear_error ();

  while (!done)
  {
    switch (mutt_menuLoop (menu))
    {

    case OP_VERIFY_KEY:

      mutt_mktemp (tempfile);
      if ((devnull = fopen ("/dev/null", "w")) == NULL)	/* __FOPEN_CHECKED__ */
      {
	mutt_perror _("Can't open /dev/null");
	break;
      }
      if ((fp = safe_fopen (tempfile, "w")) == NULL)
      {
	safe_fclose (&devnull);
	mutt_perror _("Can't create temporary file");
	break;
      }

      mutt_message _("Invoking PGP...");

      snprintf (tmpbuf, sizeof (tmpbuf), "0x%s", pgp_keyid (pgp_principal_key (KeyTable[menu->current]->parent)));

      if ((thepid = pgp_invoke_verify_key (NULL, NULL, NULL, -1,
		    fileno (fp), fileno (devnull), tmpbuf)) == -1)
      {
	mutt_perror _("Can't create filter");
	unlink (tempfile);
	safe_fclose (&fp);
	safe_fclose (&devnull);
      }

      mutt_wait_filter (thepid);
      safe_fclose (&fp);
      safe_fclose (&devnull);
      mutt_clear_error ();
      snprintf (cmd, sizeof (cmd), _("Key ID: 0x%s"), 
		pgp_keyid (pgp_principal_key (KeyTable[menu->current]->parent)));
      mutt_do_pager (cmd, tempfile, 0, NULL);
      menu->redraw = REDRAW_FULL;

      break;

    case OP_VIEW_ID:

      mutt_message ("%s", KeyTable[menu->current]->addr);
      break;

    case OP_GENERIC_SELECT_ENTRY:


      /* XXX make error reporting more verbose */

      if (option (OPTPGPCHECKTRUST))
	if (!pgp_key_is_valid (KeyTable[menu->current]->parent))
	{
	  mutt_error _("This key can't be used: expired/disabled/revoked.");
	  break;
	}

      if (option (OPTPGPCHECKTRUST) &&
	  (!pgp_id_is_valid (KeyTable[menu->current])
	   || !pgp_id_is_strong (KeyTable[menu->current])))
      {
	char *s = "";
	char buff[LONG_STRING];
	
	if (KeyTable[menu->current]->flags & KEYFLAG_CANTUSE)
	  s = N_("ID is expired/disabled/revoked.");
	else switch (KeyTable[menu->current]->trust & 0x03)
	{
	  case 0:
	    s = N_("ID has undefined validity.");
	    break;
	  case 1:
	    s = N_("ID is not valid.");
	    break;
	  case 2:
	    s = N_("ID is only marginally valid.");
	    break;
	}

	snprintf (buff, sizeof (buff), _("%s Do you really want to use the key?"),
		  _(s));

	if (mutt_yesorno (buff, M_NO) != M_YES)
	{
	  mutt_clear_error ();
	  break;
	}
      }

# if 0
      kp = pgp_principal_key (KeyTable[menu->current]->parent);
# else
      kp = KeyTable[menu->current]->parent;
# endif
      done = 1;
      break;

    case OP_EXIT:

      kp = NULL;
      done = 1;
      break;
    }
  }

  mutt_menuDestroy (&menu);
  FREE (&KeyTable);

  set_option (OPTNEEDREDRAW);

  return (kp);
}

pgp_key_t pgp_ask_for_key (char *tag, char *whatfor,
                           short abilities, pgp_ring_t keyring)
{
  pgp_key_t key;
  char resp[SHORT_STRING];
  struct pgp_cache *l = NULL;

  mutt_clear_error ();

  resp[0] = 0;
  if (whatfor)
  {

    for (l = id_defaults; l; l = l->next)
      if (!mutt_strcasecmp (whatfor, l->what))
      {
	strfcpy (resp, NONULL (l->dflt), sizeof (resp));
	break;
      }
  }


  FOREVER
  {
    resp[0] = 0;
    if (mutt_get_field (tag, resp, sizeof (resp), M_CLEAR) != 0)
      return NULL;

    if (whatfor)
    {
      if (l)
	mutt_str_replace (&l->dflt, resp);
      else
      {
	l = safe_malloc (sizeof (struct pgp_cache));
	l->next = id_defaults;
	id_defaults = l;
	l->what = safe_strdup (whatfor);
	l->dflt = safe_strdup (resp);
      }
    }

    if ((key = pgp_getkeybystr (resp, abilities, keyring)))
      return key;

    BEEP ();
  }
  /* not reached */
}

/* generate a public key attachment */

BODY *pgp_make_key_attachment (char *tempf)
{
  BODY *att;
  char buff[LONG_STRING];
  char tempfb[_POSIX_PATH_MAX], tmp[STRING];
  FILE *tempfp;
  FILE *devnull;
  struct stat sb;
  pid_t thepid;
  pgp_key_t key;
  unset_option (OPTPGPCHECKTRUST);

  key = pgp_ask_for_key (_("Please enter the key ID: "), NULL, 0, PGP_PUBRING);

  if (!key)    return NULL;

  snprintf (tmp, sizeof (tmp), "0x%s", pgp_keyid (pgp_principal_key (key)));
  pgp_free_key (&key);

  if (!tempf)
  {
    mutt_mktemp (tempfb);
    tempf = tempfb;
  }

  if ((tempfp = safe_fopen (tempf, tempf == tempfb ? "w" : "a")) == NULL)
  {
    mutt_perror _("Can't create temporary file");
    return NULL;
  }

  if ((devnull = fopen ("/dev/null", "w")) == NULL)	/* __FOPEN_CHECKED__ */
  {
    mutt_perror _("Can't open /dev/null");
    safe_fclose (&tempfp);
    if (tempf == tempfb)
      unlink (tempf);
    return NULL;
  }

  mutt_message _("Invoking PGP...");


  if ((thepid = 
       pgp_invoke_export (NULL, NULL, NULL, -1,
			   fileno (tempfp), fileno (devnull), tmp)) == -1)
  {
    mutt_perror _("Can't create filter");
    unlink (tempf);
    safe_fclose (&tempfp);
    safe_fclose (&devnull);
    return NULL;
  }

  mutt_wait_filter (thepid);

  safe_fclose (&tempfp);
  safe_fclose (&devnull);

  att = mutt_new_body ();
  att->filename = safe_strdup (tempf);
  att->unlink = 1;
  att->use_disp = 0;
  att->type = TYPEAPPLICATION;
  att->subtype = safe_strdup ("pgp-keys");
  snprintf (buff, sizeof (buff), _("PGP Key %s."), tmp);
  att->description = safe_strdup (buff);
  mutt_update_encoding (att);

  stat (tempf, &sb);
  att->length = sb.st_size;

  return att;
}

static LIST *pgp_add_string_to_hints (LIST *hints, const char *str)
{
  char *scratch;
  char *t;

  if ((scratch = safe_strdup (str)) == NULL)
    return hints;

  for (t = strtok (scratch, " ,.:\"()<>\n"); t;
       		t = strtok (NULL, " ,.:\"()<>\n"))
  {
    if (strlen (t) > 3)
      hints = mutt_add_list (hints, t);
  }

  FREE (&scratch);
  return hints;
}

static pgp_key_t *pgp_get_lastp (pgp_key_t p)
{
  for (; p; p = p->next)
    if (!p->next)
      return &p->next;

  return NULL;
}

pgp_key_t pgp_getkeybyaddr (ADDRESS * a, short abilities, pgp_ring_t keyring)
{
  ADDRESS *r, *p;
  LIST *hints = NULL;

  int multi   = 0;
  int match;

  pgp_key_t keys, k, kn;
  pgp_key_t the_valid_key = NULL;
  pgp_key_t matches = NULL;
  pgp_key_t *last = &matches;
  pgp_uid_t *q;

  if (a && a->mailbox)
    hints = pgp_add_string_to_hints (hints, a->mailbox);
  if (a && a->personal)
    hints = pgp_add_string_to_hints (hints, a->personal);

  mutt_message (_("Looking for keys matching \"%s\"..."), a->mailbox);
  keys = pgp_get_candidates (keyring, hints);

  mutt_free_list (&hints);

  if (!keys)
    return NULL;

  dprint (5, (debugfile, "pgp_getkeybyaddr: looking for %s <%s>.",
	      a->personal, a->mailbox));


  for (k = keys; k; k = kn)
  {
    kn = k->next;

    dprint (5, (debugfile, "  looking at key: %s\n",
		pgp_keyid (k)));

    if (abilities && !(k->flags & abilities))
    {
      dprint (5, (debugfile, "  insufficient abilities: Has %x, want %x\n",
		  k->flags, abilities));
      continue;
    }

    match                = 0;   /* any match 		  */

    for (q = k->address; q; q = q->next)
    {
      r = rfc822_parse_adrlist (NULL, q->addr);

      for (p = r; p; p = p->next)
      {
	int validity = pgp_id_matches_addr (a, p, q);

	if (validity & PGP_KV_MATCH)	/* something matches */
	  match = 1;

	/* is this key a strong candidate? */
	if ((validity & PGP_KV_VALID) && (validity & PGP_KV_STRONGID) 
	    && (validity & PGP_KV_ADDR))
	{
	  if (the_valid_key && the_valid_key != k)
	    multi             = 1;
	  the_valid_key       = k;
	}
      }

      rfc822_free_address (&r);
    }

    if (match)
    {
      *last  = pgp_principal_key (k);
      kn     = pgp_remove_key (&keys, *last);
      last   = pgp_get_lastp (k);
    }
  }

  pgp_free_key (&keys);

  if (matches)
  {
    if (the_valid_key && !multi)
    {
      /*
       * There was precisely one strong match on a valid ID.
       * 
       * Proceed without asking the user.
       */
      pgp_remove_key (&matches, the_valid_key);
      pgp_free_key (&matches);
      k = the_valid_key;
    }
    else 
    {
      /* 
       * Else: Ask the user.
       */
      if ((k = pgp_select_key (matches, a, NULL)))
	pgp_remove_key (&matches, k);
      pgp_free_key (&matches);
    }

    return k;
  }

  return NULL;
}

pgp_key_t pgp_getkeybystr (char *p, short abilities, pgp_ring_t keyring)
{
  LIST *hints = NULL;
  pgp_key_t keys;
  pgp_key_t matches = NULL;
  pgp_key_t *last = &matches;
  pgp_key_t k, kn;
  pgp_uid_t *a;
  short match;
  size_t l;

  if ((l = mutt_strlen (p)) && p[l-1] == '!')
    p[l-1] = 0;

  mutt_message (_("Looking for keys matching \"%s\"..."), p);

  hints = pgp_add_string_to_hints (hints, p);
  keys = pgp_get_candidates (keyring, hints);
  mutt_free_list (&hints);

  if (!keys)
    goto out;

  for (k = keys; k; k = kn)
  {
    kn = k->next;
    if (abilities && !(k->flags & abilities))
      continue;

    match = 0;

    for (a = k->address; a; a = a->next)
    {
      dprint (5, (debugfile, "pgp_getkeybystr: matching \"%s\" against key %s, \"%s\": ",
		  p, pgp_keyid (k), a->addr));
      if (!*p || mutt_strcasecmp (p, pgp_keyid (k)) == 0 ||
	  (!mutt_strncasecmp (p, "0x", 2) && !mutt_strcasecmp (p + 2, pgp_keyid (k))) ||
	  (option (OPTPGPLONGIDS) && !mutt_strncasecmp (p, "0x", 2) &&
	   !mutt_strcasecmp (p + 2, k->keyid + 8)) ||
	  mutt_stristr (a->addr, p))
      {
	dprint (5, (debugfile, "match.\n"));
	match = 1;
	break;
      }
    }

    if (match)
    {
      *last = pgp_principal_key (k);
      kn    = pgp_remove_key (&keys, *last);
      last  = pgp_get_lastp (k);
    }
  }

  pgp_free_key (&keys);

  if (matches)
  {
    if ((k = pgp_select_key (matches, NULL, p)))
      pgp_remove_key (&matches, k);

    pgp_free_key (&matches);
    if (!p[l-1])
      p[l-1] = '!';
    return k;
  }

out:
  if (!p[l-1])
    p[l-1] = '!';
  return NULL;
}

#endif /* CRYPT_BACKEND_CLASSIC_PGP */
