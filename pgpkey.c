/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (c) 1998,1999 Thomas Roessler <roessler@guug.de>
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
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mime.h"
#include "pgp.h"
#include "pager.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef _PGPPATH

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

static pgp_key_t *pgp_principal_key (pgp_key_t *key)
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
 * %u	uiser id
 * %a	algorithm	%A      algorithm of the princ. key
 * %l	length		%L	length of the princ. key
 * %f	flags		%F 	flags of the princ. key
 * %c	capabilities	%C	capabilities of the princ. key
 * %t	trust/validity of the key-uid association
 */

typedef struct pgp_entry
{
  size_t num;
  pgp_uid_t *uid;
} pgp_entry_t;

static const char *pgp_entry_fmt (char *dest,
				  size_t destlen,
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
  pgp_key_t *key, *pkey;
  int kflags;
  int optional = (flags & M_FORMAT_OPTIONAL);

  entry = (pgp_entry_t *) data;
  uid   = entry->uid;
  key   = uid->parent;
  pkey  = pgp_principal_key (key);

  if (isupper (op))
  {
    key = pkey;
    kflags = pkey->flags;
  }
  else
    /* a subkey inherits the principal key's usage restrictions. */
    kflags = key->flags | (pkey->flags & KEYFLAG_RESTRICTIONS);
  
  switch (tolower (op))
  {
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
    mutt_FormatString (dest, destlen, ifstring, mutt_attach_fmt, data, 0);
  else if (flags & M_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, elsestring, mutt_attach_fmt, data, 0);
  return (src);
}
      
static void pgp_entry (char *s, size_t l, MUTTMENU * menu, int num)
{
  pgp_uid_t **KeyTable = (pgp_uid_t **) menu->data;
  pgp_entry_t entry;
  
  entry.uid = KeyTable[num];
  entry.num = num + 1;

  mutt_FormatString (s, l, NONULL (PgpEntryFormat), pgp_entry_fmt, 
		     (unsigned long) &entry, M_FORMAT_ARROWCURSOR);
}
  
static int pgp_compare (const void *a, const void *b)
{
  int r;

  pgp_uid_t **s = (pgp_uid_t **) a;
  pgp_uid_t **t = (pgp_uid_t **) b;

  if ((r = mutt_strcasecmp ((*s)->addr, (*t)->addr)) != 0)
    return r;
  else
    return mutt_strcasecmp (pgp_keyid ((*s)->parent), pgp_keyid ((*t)->parent));
}

static pgp_key_t *pgp_select_key (struct pgp_vinfo *pgp,
				  pgp_key_t *keys,
				  ADDRESS * p, const char *s)
{
  int keymax;
  pgp_uid_t **KeyTable;
  MUTTMENU *menu;
  int i, done = 0;
  char helpstr[SHORT_STRING], buf[LONG_STRING];
  char cmd[LONG_STRING], tempfile[_POSIX_PATH_MAX];
  FILE *fp, *devnull;
  pid_t thepid;
  pgp_key_t *kp;
  pgp_uid_t *a;

  for (i = 0, kp = keys; kp; kp = kp->next)
  {
    if (!option (OPTPGPSHOWUNUSABLE) && (kp->flags & KEYFLAG_CANTUSE))
      continue;
    
    for (a = kp->address; a; i++, a = a->next)
      ;
  }

  if (i == 0)
    return NULL;

  keymax = i;
  KeyTable = safe_malloc (sizeof (pgp_key_t *) * i);

  for (i = 0, kp = keys; kp; kp = kp->next)
  {
    if (!option (OPTPGPSHOWUNUSABLE) && (kp->flags & KEYFLAG_CANTUSE))
      continue;
	
    for (a = kp->address; a; i++, a = a->next)
      KeyTable[i] = a;
  }

  qsort (KeyTable, i, sizeof (pgp_key_t *), pgp_compare);

  helpstr[0] = 0;
  mutt_make_help (buf, sizeof (buf), _("Exit  "), MENU_PGP, OP_EXIT);
  strcat (helpstr, buf);
  mutt_make_help (buf, sizeof (buf), _("Select  "), MENU_PGP,
		  OP_GENERIC_SELECT_ENTRY);
  strcat (helpstr, buf);
  mutt_make_help (buf, sizeof (buf), _("Check key  "), MENU_PGP, OP_VERIFY_KEY);
  strcat (helpstr, buf);
  mutt_make_help (buf, sizeof (buf), _("Help"), MENU_PGP, OP_HELP);
  strcat (helpstr, buf);

  menu = mutt_new_menu ();
  menu->max = keymax;
  menu->make_entry = pgp_entry;
  menu->menu = MENU_PGP;
  menu->help = helpstr;
  menu->data = KeyTable;

  strfcpy (buf, _("PGP keys matching "), sizeof (buf));
  if (p)
    strfcpy (buf, p->mailbox, sizeof (buf) - mutt_strlen (buf));
  else
    strcat (buf, s);
  menu->title = buf;

  kp = NULL;

  while (!done)
  {
    switch (mutt_menuLoop (menu))
    {

    case OP_VERIFY_KEY:

      mutt_mktemp (tempfile);
      if ((devnull = fopen ("/dev/null", "w")) == NULL)
      {
	mutt_perror _("Can't open /dev/null");
	break;
      }
      if ((fp = safe_fopen (tempfile, "w")) == NULL)
      {
	fclose (devnull);
	mutt_perror _("Can't create temporary file");
	break;
      }

      mutt_message _("Invoking PGP...");

      if ((thepid = pgp->invoke_verify_key (pgp, NULL, NULL, NULL, -1,
		    fileno (fp), fileno (devnull),
		    pgp_keyid (pgp_principal_key (KeyTable[menu->current]->parent)))) == -1)
      {
	mutt_perror _("Can't create filter");
	unlink (tempfile);
	fclose (fp);
	fclose (devnull);
      }

      mutt_wait_filter (thepid);
      fclose (fp);
      fclose (devnull);
      mutt_clear_error ();
      snprintf (cmd, sizeof (cmd), _("Key ID: 0x%s"), 
		pgp_keyid (pgp_principal_key (KeyTable[menu->current]->parent)));
      mutt_do_pager (cmd, tempfile, 0, NULL);
      menu->redraw = REDRAW_FULL;

      break;

    case OP_VIEW_ID:

      mutt_message (KeyTable[menu->current]->addr);
      break;

    case OP_GENERIC_SELECT_ENTRY:


      /* XXX make error reporting more verbose */
      
      if (option (OPTPGPCHECKTRUST))
      {
	pgp_key_t *key, *principal;
	
	key = KeyTable[menu->current]->parent;
	principal = pgp_principal_key (key);
	
	if ((key->flags | principal->flags) & KEYFLAG_CANTUSE)
	{
	  mutt_error _("This key can't be used: expired/disabled/revoked.");
	  break;
	}
      }
      
      if (option (OPTPGPCHECKTRUST) &&
	  (KeyTable[menu->current]->trust & 0x03) < 3)
      {
	char *s = "";
	char buff[LONG_STRING];

	switch (KeyTable[menu->current]->trust & 0x03)
	{
	case 0:
	  s = N_("This ID's trust level is undefined.");
	  break;
	case 1:
	  s = N_("This ID is not trusted.");
	  break;
	case 2:
	  s = N_("This ID is only marginally trusted.");
	  break;
	}

	snprintf (buff, sizeof (buff), _("%s Do you really want to use it?"),
		  _(s));

	if (mutt_yesorno (buff, 0) != 1)
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
  safe_free ((void **) &KeyTable);

  set_option (OPTNEEDREDRAW);
  
  return (kp);
}

pgp_key_t *pgp_ask_for_key (struct pgp_vinfo *pgp, char *tag, char *whatfor,
			    short abilities, pgp_ring_t keyring)
{
  pgp_key_t *key;
  char resp[SHORT_STRING];
  struct pgp_cache *l = NULL;

  resp[0] = 0;
  if (whatfor)
  {

    for (l = id_defaults; l; l = l->next)
      if (!mutt_strcasecmp (whatfor, l->what))
      {
	strcpy (resp, NONULL (l->dflt));
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
      {
	safe_free ((void **) &l->dflt);
	l->dflt = safe_strdup (resp);
      }
      else
      {
	l = safe_malloc (sizeof (struct pgp_cache));
	l->next = id_defaults;
	id_defaults = l;
	l->what = safe_strdup (whatfor);
	l->dflt = safe_strdup (resp);
      }
    }

    if ((key = pgp_getkeybystr (pgp, resp, abilities, keyring)))
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
  char tempfb[_POSIX_PATH_MAX];
  char *id;
  FILE *tempfp;
  FILE *devnull;
  struct stat sb;
  pid_t thepid;
  pgp_key_t *key;
  struct pgp_vinfo *pgp = pgp_get_vinfo (PGP_EXPORT);

  if (!pgp)
    return NULL;

  unset_option (OPTPGPCHECKTRUST);

  key = pgp_ask_for_key (pgp, _("Please enter the key ID: "), NULL, 0, PGP_PUBRING);

  if (!key)    return NULL;

  id = safe_strdup (pgp_keyid (pgp_principal_key(key)));
  pgp_free_key (&key);
  
  if (!tempf)
  {
    mutt_mktemp (tempfb);
    tempf = tempfb;
  }

  if ((tempfp = safe_fopen (tempf, tempf == tempfb ? "w" : "a")) == NULL)
  {
    mutt_perror _("Can't create temporary file");
    safe_free ((void **) &id);
    return NULL;
  }

  if ((devnull = fopen ("/dev/null", "w")) == NULL)
  {
    mutt_perror _("Can't open /dev/null");
    safe_free ((void **) &id);
    fclose (tempfp);
    if (tempf == tempfb)
      unlink (tempf);
    return NULL;
  }

  mutt_message _("Invoking pgp...");
  
  if ((thepid = 
       pgp->invoke_export (pgp, NULL, NULL, NULL, -1,
			   fileno (tempfp), fileno (devnull), id)) == -1)
  {
    mutt_perror _("Can't create filter");
    unlink (tempf);
    fclose (tempfp);
    fclose (devnull);
    safe_free ((void **) &id);
    return NULL;
  }

  mutt_wait_filter (thepid);

  fclose (tempfp);
  fclose (devnull);

  att = mutt_new_body ();
  att->filename = safe_strdup (tempf);
  att->unlink = 1;
  att->type = TYPEAPPLICATION;
  att->subtype = safe_strdup ("pgp-keys");
  snprintf (buff, sizeof (buff), _("PGP Key 0x%s."), id);
  att->description = safe_strdup (buff);
  mutt_update_encoding (att);

  stat (tempf, &sb);
  att->length = sb.st_size;

  safe_free ((void **) &id);
  return att;
}

static LIST *pgp_add_string_to_hints (LIST *hints, const char *str)
{
  char *scratch = safe_strdup (str);
  char *t;
  
  t = strtok (scratch, " \n");
  while (t)
  {
    hints = mutt_add_list (hints, t);
    t = strtok (NULL, " \n");
  }

  safe_free ((void **) &scratch);
  return hints;
}


pgp_key_t *pgp_getkeybyaddr (struct pgp_vinfo * pgp,
			  ADDRESS * a, short abilities, pgp_ring_t keyring)
{
  ADDRESS *r, *p;
  LIST *hints = NULL;
  int weak = 0;
  int weak_association, kflags;
  int match;
  pgp_uid_t *q;
  pgp_key_t *keys, *k, *kn, *pk;
  pgp_key_t *matches = NULL;
  pgp_key_t **last = &matches;
  
  if (a && a->mailbox)
    hints = pgp_add_string_to_hints (hints, a->mailbox);
  if (a && a->personal)
    hints = pgp_add_string_to_hints (hints, a->personal);

  mutt_message (_("Looking for keys matching \"%s\"..."), a->mailbox);
  keys = pgp->get_candidates (pgp, keyring, hints);

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

    pkey = pgp_principal_key (k);
    kflags = k->flags | pkey->flags;
    
    q = k->address;
    weak_association = 1;
    match = 0;

    for (; q; q = q->next)
    {
      r = rfc822_parse_adrlist (NULL, q->addr);


      for (p = r; p && weak_association; p = p->next)
      {
	if ((p->mailbox && a->mailbox &&
	     mutt_strcasecmp (p->mailbox, a->mailbox) == 0) ||
	    (a->personal && p->personal &&
	     mutt_strcasecmp (a->personal, p->personal) == 0))
	{
	  match = 1;

	  if (((q->trust & 0x03) == 3) &&
	      !(kflags & KEYFLAG_CANTUSE) &&
	      (p->mailbox && a->mailbox &&
	       !mutt_strcasecmp (p->mailbox, a->mailbox)))
	    weak_association = 0;
	}
      }
      rfc822_free_address (&r);
    }

    if (match)
    {
      pgp_key_t *_p, *_k;

      _k = pgp_principal_key (k);
      
      *last = _k;
      kn = pgp_remove_key (&keys, _k);

      /* start with k, not with _k: k is always a successor of _k. */
      
      for (_p = k; _p; _p = _p->next)
      {
	if (!_p->next)
	{
	  last = &_p->next;
	  break;
	}
      }
    }
  }

  pgp_free_key (&keys);
  
  if (matches)
  {
    if (matches->next || weak)
    {
      /* query for which key the user wants */
      k = pgp_select_key (pgp, matches, a, NULL);
      if (k) 
	pgp_remove_key (&matches, k);

      pgp_free_key (&matches);
    }
    else
      k = matches;
    
    return k;
  }

  return NULL;
}

pgp_key_t *pgp_getkeybystr (struct pgp_vinfo * pgp,
			 char *p, short abilities, pgp_ring_t keyring)
{
  LIST *hints = NULL;
  pgp_key_t *keys;
  pgp_key_t *matches = NULL;
  pgp_key_t **last = &matches;
  pgp_key_t *k, *kn;
  pgp_uid_t *a;
  short match;

  mutt_message (_("Looking for keys matching \"%s\"..."), p);
  
  hints = pgp_add_string_to_hints (hints, p);
  keys = pgp->get_candidates (pgp, keyring, hints);
  mutt_free_list (&hints);

  if (!keys)
    return NULL;
  
  
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
      pgp_key_t *_p, *_k;

      _k = pgp_principal_key (k);
      
      *last = _k;
      kn = pgp_remove_key (&keys, _k);

      /* start with k, not with _k: k is always a successor of _k. */
      
      for (_p = k; _p; _p = _p->next)
      {
	if (!_p->next)
	{
	  last = &_p->next;
	  break;
	}
      }
    }
  }

  pgp_free_key (&keys);

  if (matches)
  {
    k = pgp_select_key (pgp, matches, NULL, p);
    if (k) 
      pgp_remove_key (&matches, k);
    
    pgp_free_key (&matches);
    return k;
  }

  return NULL;
}



#endif /* _PGPPATH */
