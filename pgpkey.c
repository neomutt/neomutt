/*
 * Copyright (C) 1996,1997 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (c) 1998 Thomas Roessler <roessler@guug.de>
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

static struct pgp_cache {
  char *what;
  char *dflt;
  struct pgp_cache *next;
} * id_defaults = NULL;

typedef struct
{
  KEYINFO *k;
  PGPUID *a;
} pgp_key_t;

static char trust_flags[] = "?- +";

static char *pgp_key_abilities(int flags)
{
  static char buff[3];
  
  if(!(flags & KEYFLAG_CANENCRYPT))
    buff[0] = '-';
  else if(flags & KEYFLAG_PREFER_SIGNING)
    buff[0] = '.';
  else
    buff[0] = 'e';
  
  if(!(flags & KEYFLAG_CANSIGN))
    buff[1] = '-';
  else if(flags & KEYFLAG_PREFER_ENCRYPTION)
    buff[1] = '.';
  else
    buff[1] = 's';
  
  buff[2] = '\0';
  
  return buff;
}

static void pgp_entry (char *s, size_t l, MUTTMENU *menu, int num)
{
  pgp_key_t *KeyTable = (pgp_key_t *) menu->data;

  snprintf (s, l, "%4d %c%c %4d/0x%s %-4s %2s  %s",
	    num + 1,
	    trust_flags[KeyTable[num].a->trust & 0x03],
	     (KeyTable[num].k->flags & KEYFLAG_CRITICAL ?
	     'c' : ' '),
	    KeyTable[num].k->keylen, 
	    _pgp_keyid(KeyTable[num].k),
	    KeyTable[num].k->algorithm, 
	    pgp_key_abilities(KeyTable[num].k->flags),
	    KeyTable[num].a->addr);
}

static int pgp_search (MUTTMENU *m, regex_t *re, int n)
{
  char buf[LONG_STRING];
  
  pgp_entry (buf, sizeof (buf), m, n);
  return (regexec (re, buf, 0, NULL, 0));
}

static int pgp_compare (const void *a, const void *b)
{
  int r;
  
  pgp_key_t *s = (pgp_key_t *) a;
  pgp_key_t *t = (pgp_key_t *) b;

  if((r = strcasecmp (s->a->addr, t->a->addr)) != 0)
    return r;
  else
    return strcasecmp(pgp_keyid(s->k), pgp_keyid(t->k));
}

static KEYINFO *pgp_select_key (struct pgp_vinfo *pgp,
				LIST *keys, 
				ADDRESS *p, const char *s)
{
  int keymax;
  pgp_key_t *KeyTable;
  MUTTMENU *menu;
  LIST *a;
  int i;  
  int done = 0;
  LIST *l;
  char helpstr[SHORT_STRING], buf[LONG_STRING];
  char cmd[LONG_STRING], tempfile[_POSIX_PATH_MAX];
  FILE *fp, *devnull;
  pid_t thepid;
  KEYINFO *info;
  int savedmenu = CurrentMenu;
  
  
  for (i = 0, l = keys; l; l = l->next)
  {
    int did_main_key = 0;
    
    info = (KEYINFO *) l->data;
    a = info->address;
   retry1:
    for (; a; i++, a = a->next)
      ;

    if(!did_main_key && info->flags & KEYFLAG_SUBKEY && info->mainkey)
    {
      did_main_key = 1;
      a = info->mainkey->address;
      goto retry1;
    }
  }
    
  if (i == 0) return NULL;

  keymax = i;
  KeyTable = safe_malloc (sizeof (pgp_key_t) * i);

  for (i = 0, l = keys; l; l = l->next)
  {
    int did_main_key = 0;
    info = (KEYINFO *)l->data;
    a = info->address;
   retry2:    
    for (; a ; i++, a = a->next)
    {
      KeyTable[i].k = (KEYINFO *) l->data;
      KeyTable[i].a = (PGPUID *)a->data;
    }
    if(!did_main_key && info->flags & KEYFLAG_SUBKEY && info->mainkey)
    {
      did_main_key = 1;
      a = info->mainkey->address;
      goto retry2;
    }
  }
  
  qsort (KeyTable, i, sizeof (pgp_key_t), pgp_compare);

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
  menu->search = pgp_search;
  menu->menu = CurrentMenu = MENU_PGP;
  menu->help = helpstr;
  menu->data = KeyTable;

  strfcpy (buf, _("PGP keys matching "), sizeof (buf));
  if (p)
    strfcpy (buf, p->mailbox, sizeof (buf) - strlen (buf));
  else
    strcat (buf, s);
  menu->title = buf;

  info = NULL;
  
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
	
        if((thepid = pgp->invoke_verify_key(pgp, NULL, NULL, NULL, -1,
					   fileno(fp), fileno(devnull), 
					   pgp_keyid(KeyTable[menu->current].k))) == -1)
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
        snprintf(cmd, sizeof(cmd), _("Key ID: 0x%s"), pgp_keyid(KeyTable[menu->current].k));
	mutt_do_pager (cmd, tempfile, 0, NULL);
	menu->redraw = REDRAW_FULL;
	
	break;

      case OP_VIEW_ID:
        
        mutt_message (KeyTable[menu->current].a->addr);
        break;
      
      case OP_GENERIC_SELECT_ENTRY:

	if (option (OPTPGPCHECKTRUST) && 
	    (KeyTable[menu->current].a->trust & 0x03) < 3) 
	{
	  char *s = "";
	  char buff[LONG_STRING];

	  switch (KeyTable[menu->current].a->trust & 0x03)
	  {
	  case 0: s = N_("This ID's trust level is undefined."); break;
	  case 1: s = N_("This ID is not trusted."); break;
	  case 2: s = N_("This ID is only marginally trusted."); break;
	  }

	  snprintf (buff, sizeof(buff), _("%s Do you really want to use it?"),
		    _(s));

	  if (mutt_yesorno (buff, 0) != 1)
	  {
	    mutt_clear_error ();
	    break;
	  }
	}

        info = KeyTable[menu->current].k;
	done = 1;
	break;

      case OP_EXIT:

        info = NULL;
	done = 1;
	break;
    }
  }

  mutt_menuDestroy (&menu);
  CurrentMenu = savedmenu;
  safe_free ((void **) &KeyTable);

  return (info);
}

char *pgp_ask_for_key (struct pgp_vinfo *pgp, KEYINFO *db, char *tag, char *whatfor,
		       short abilities, char **alg)
{
  KEYINFO *key;
  char *key_id;
  char resp[SHORT_STRING];
  struct pgp_cache *l = NULL;

  resp[0] = 0;
  if (whatfor) 
  {

    for (l = id_defaults; l; l = l->next)
      if (!strcasecmp (whatfor, NONULL(l->what)))
      {
	strcpy (resp, NONULL(l->dflt));
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
	safe_free ((void **)&l->dflt);
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

    if ((key = ki_getkeybystr (pgp, resp, db, abilities)))
    {
      key_id = safe_strdup(pgp_keyid (key));

      if (alg) 
	*alg = safe_strdup(pgp_pkalg_to_mic(key->algorithm));
      
      return (key_id);
    }
    BEEP ();
  }
  /* not reached */
}

/* generate a public key attachment */

BODY *pgp_make_key_attachment (char * tempf)
{
  BODY *att;
  char buff[LONG_STRING];
  char tempfb[_POSIX_PATH_MAX];
  char *id;
  FILE *tempfp;
  FILE *devnull;
  struct stat sb;
  pid_t thepid;
  KEYINFO *db;
  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_EXPORT);

  if(!pgp)
    return NULL;
  
  unset_option (OPTPGPCHECKTRUST);
  
  db = pgp->read_pubring(pgp);
  id = pgp_ask_for_key (pgp, db, _("Please enter the key ID: "), NULL, 0, NULL);
  pgp_close_keydb(&db);
  
  if(!id)
    return NULL;

  if (!tempf) {
    mutt_mktemp (tempfb);
    tempf = tempfb;
  }

  if ((tempfp = safe_fopen (tempf, tempf == tempfb ? "w" : "a")) == NULL) {
    mutt_perror _("Can't create temporary file");
    safe_free ((void **)&id);
    return NULL;
  }

  if ((devnull = fopen ("/dev/null", "w")) == NULL) {
    mutt_perror _("Can't open /dev/null");
    safe_free ((void **)&id);
    fclose (tempfp);
    if (tempf == tempfb) unlink (tempf);
    return NULL;
  }

  if ((thepid = pgp->invoke_export(pgp,
				   NULL, NULL, NULL, -1, 
				   fileno(tempfp), fileno(devnull), id)) == -1)
  {
    mutt_perror _("Can't create filter");
    unlink (tempf);
    fclose (tempfp);
    fclose (devnull);
    safe_free ((void **)&id);
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

  safe_free ((void **)&id);  
  return att;
}

static char *mutt_stristr (char *haystack, char *needle)
{
  char *p, *q;

  if (!haystack)
    return NULL;
  if (!needle)
    return (haystack);

  while (*(p = haystack))
  {
    for (q = needle ; *p && *q && tolower (*p) == tolower (*q) ; p++, q++)
      ;
    if (!*q)
      return (haystack);
    haystack++;
  }
  return NULL;
}

KEYINFO *ki_getkeybyaddr (struct pgp_vinfo *pgp, 
			  ADDRESS *a, KEYINFO *k, short abilities)
{
  ADDRESS *r, *p;
  LIST *l = NULL, *t = NULL;
  LIST *q;
  int weak = 0;
  int weak_association;
  int match;
  int did_main_key;
  PGPUID *u;
  
  for ( ; k ; k = k->next)
  {
    if(k->flags & (KEYFLAG_REVOKED | KEYFLAG_EXPIRED | KEYFLAG_DISABLED)) 
      continue;
    
    if(abilities && !(k->flags & abilities))
      continue;
    
    q = k->address;
    did_main_key = 0;
    weak_association = 1;
    match = 0;
    
    retry:
    
    for (;q ; q = q->next)
    {
      u = (PGPUID *) q->data;
      r = rfc822_parse_adrlist(NULL, u->addr);
      
      for(p = r; p && weak_association; p = p->next) 
      {
	if ((p->mailbox && a->mailbox &&
	     strcasecmp (p->mailbox, a->mailbox) == 0) ||
	    (a->personal && p->personal &&
	     strcasecmp (a->personal, p->personal) == 0))
	{
	  match = 1;

	  if(((u->trust & 0x03) == 3) &&
	     (p->mailbox && a->mailbox && !strcasecmp(p->mailbox, a->mailbox)))
	    weak_association = 0;
	}
      }
      rfc822_free_address(&r);
    }

    if(match)
    {
      t = mutt_new_list ();
      t->data = (void *) k;
      t->next = l;
      l = t;
      
      if(weak_association)
	weak = 1;
      
    }

    if(!did_main_key && !match && k->flags & KEYFLAG_SUBKEY && k->mainkey)
    {
      did_main_key = 1;
      q = k->mainkey->address;
      goto retry;
    }
  }
  
  if (l)
  {
    if (l->next || weak)
    {
      /* query for which key the user wants */
      k = pgp_select_key (pgp, l, a, NULL);
    }
    else
      k = (KEYINFO *)l->data;

    /* mutt_free_list() frees the .data member, so clear the pointers */

    for(t = l; t; t = t->next)
      t->data = NULL;
    
    mutt_free_list (&l);
  }

  return (k);
}

KEYINFO *ki_getkeybystr (struct pgp_vinfo *pgp,
			 char *p, KEYINFO *k, short abilities)
{
  LIST *t = NULL, *l = NULL;
  LIST *a;
  
  for(; k; k = k->next)
  {
    int did_main_key = 0;
    
    if(k->flags & (KEYFLAG_REVOKED | KEYFLAG_EXPIRED | KEYFLAG_DISABLED)) 
      continue;
    
    if(abilities && !(k->flags & abilities))
      continue;

    a = k->address;
    
    retry:
    
    for(; a ; a = a->next) 
    {

      if (!*p || strcasecmp (p, pgp_keyid(k)) == 0 ||
	  (!strncasecmp(p, "0x", 2) && !strcasecmp(p+2, pgp_keyid(k))) ||
	  (option(OPTPGPLONGIDS) && !strncasecmp(p, "0x", 2) &&
	   !strcasecmp(p+2, k->keyid+8)) ||
	  mutt_stristr(((PGPUID *)a->data)->addr,p))
      {
	t = mutt_new_list ();
	t->data = (void *)k;
	t->next = l;
	l = t;
	break;
      }
    }
    
    if(!did_main_key && k->flags & KEYFLAG_SUBKEY && k->mainkey)
    {
      did_main_key = 1;
      a = k->mainkey->address;
      goto retry;
    }
  }

  if (l)
  {
    k = pgp_select_key (pgp, l, NULL, p);
    set_option(OPTNEEDREDRAW);

    for(t = l; t; t = t->next)
      t->data = NULL;

    mutt_free_list (&l);
  }

  return (k);
}



#endif /* _PGPPATH */
