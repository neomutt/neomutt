/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
#include "mutt_regex.h"
#include "mutt_curses.h"

#include <pwd.h>
#include <string.h>

ADDRESS *mutt_lookup_alias (const char *s)
{
  ALIAS *t = Aliases;

  for (; t; t = t->next)
    if (!strcasecmp (s, t->name))
      return (t->addr);
  return (NULL);   /* no such alias */
}

static ADDRESS *mutt_expand_aliases_r (ADDRESS *a, LIST **expn)
{
  ADDRESS *head = NULL, *last = NULL, *t, *w;
  LIST *u;
  char i;
  const char *fqdn;
  
  while (a)
  {
    if (!a->group && !a->personal && a->mailbox && strchr (a->mailbox, '@') == NULL)
    {
      t = mutt_lookup_alias (a->mailbox);

      if (t)
      {	
        i = 0;
        for (u = *expn; u; u = u->next)
	{
	  if (strcmp (a->mailbox, u->data) == 0) /* alias already found */
	  {
	    dprint (1, (debugfile, "mutt_expand_aliases_r(): loop in alias found for '%s'\n", a->mailbox));
	    i = 1;
	    break;
	  }
	}

        if (!i)
	{
          u = safe_malloc (sizeof (LIST));
          u->data = safe_strdup (a->mailbox);
          u->next = *expn;
          *expn = u;
	  w = rfc822_cpy_adr (t);
	  w = mutt_expand_aliases_r (w, expn);
	  if (head)
	    last->next = w;
	  else
	    head = last = w;
	  while (last && last->next)
	    last = last->next;
        }
	t = a;
	a = a->next;
	t->next = NULL;
	rfc822_free_address (&t);
	continue;
      }
      else
      {
	struct passwd *pw = getpwnam (a->mailbox);
	char buffer[256], *p;

	if (pw)
	{
	  strfcpy (buffer, pw->pw_gecos, sizeof (buffer));
	  if ((p = strchr (buffer, ',')))
	    *p = 0;
	  a->personal = safe_strdup (buffer);
#ifdef EXACT_ADDRESS
	  FREE (&a->val);
#endif
	}
      }
    }

    if (head)
    {
      last->next = a;
      last = last->next;
    }
    else
      head = last = a;
    a = a->next;
    last->next = NULL;
  }

  if (option (OPTUSEDOMAIN) && (fqdn = mutt_fqdn(1)))
  {
    /* now qualify all local addresses */
    rfc822_qualify (head, fqdn);
  }

  return (head);
}

ADDRESS *mutt_expand_aliases (ADDRESS *a)
{
  ADDRESS *t;
  LIST *expn = NULL; /* previously expanded aliases to avoid loops */

  t = mutt_expand_aliases_r (a, &expn);
  mutt_free_list (&expn);
  return (t);
}

/* if someone has an address like
 *	From: Michael `/bin/rm -f ~` Elkins <me@cs.hmc.edu>
 * and the user creates an alias for this, Mutt could wind up executing
 * the backtics because it writes aliases like
 *	alias me Michael `/bin/rm -f ~` Elkins <me@cs.hmc.edu>
 * To avoid this problem, use a backslash (\) to quote any backtics.  We also
 * need to quote backslashes as well, since you could defeat the above by
 * doing
 *	From: Michael \`/bin/rm -f ~\` Elkins <me@cs.hmc.edu>
 * since that would get aliased as
 *	alias me Michael \\`/bin/rm -f ~\\` Elkins <me@cs.hmc.edu>
 * which still gets evaluated because the double backslash is not a quote.
 */
static void write_safe_address (FILE *fp, char *s)
{
  while (*s)
  {
    if (*s == '\\' || *s == '`')
      fputc ('\\', fp);
    fputc (*s, fp);
    s++;
  }
}

void mutt_create_alias (ENVELOPE *cur, ADDRESS *iadr)
{
  ALIAS *new, *t;
  char buf[LONG_STRING], prompt[SHORT_STRING], *pc;
  FILE *rc;
  ADDRESS *adr = NULL;

  if (cur)
  {
    if (mutt_addr_is_user (cur->from))
    {
      if (cur->to && !mutt_is_mail_list (cur->to))
	adr = cur->to;
      else
	adr = cur->cc;
    }
    else if (cur->reply_to && !mutt_is_mail_list (cur->reply_to))
      adr = cur->reply_to;
    else
      adr = cur->from;

  }
  else if (iadr)
  {
    adr = iadr;
  }

  if (adr && adr->mailbox)
  {
    strfcpy (buf, adr->mailbox, sizeof (buf));
    if ((pc = strchr (buf, '@')))
      *pc = 0;
  }
  else
    buf[0] = '\0';
  
  /* add a new alias */
  if (mutt_get_field (_("Alias as: "), buf, sizeof (buf), 0) != 0 || !buf[0])
    return;

  /* check to see if the user already has an alias defined */
  if (mutt_lookup_alias (buf))
  {
    mutt_error _("You already have an alias defined with that name!");
    return;
  }

  new = safe_calloc (1, sizeof (ALIAS));
  new->name = safe_strdup (buf);

  if (adr)
    strfcpy (buf, adr->mailbox, sizeof (buf));
  else
    buf[0] = 0;

  do
  {
    if (mutt_get_field (_("Address: "), buf, sizeof (buf), 0) != 0 || !buf[0])
    {
      mutt_free_alias (&new);
      return;
    }
    
    if((new->addr = rfc822_parse_adrlist (new->addr, buf)) == NULL)
      BEEP ();
  }
  while(new->addr == NULL);
  
  if (adr && adr->personal && !mutt_is_mail_list (adr))
    strfcpy (buf, adr->personal, sizeof (buf));
  else
    buf[0] = 0;

  if (mutt_get_field (_("Personal name: "), buf, sizeof (buf), 0) != 0)
  {
    mutt_free_alias (&new);
    return;
  }
  new->addr->personal = safe_strdup (buf);

  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), new->addr);
  snprintf (prompt, sizeof (prompt), _("[%s = %s] Accept?"), new->name, buf);
  if (mutt_yesorno (prompt, 1) != 1)
  {
    mutt_free_alias (&new);
    return;
  }

  if ((t = Aliases))
  {
    while (t->next)
      t = t->next;
    t->next = new;
  }
  else
    Aliases = new;

  strfcpy (buf, NONULL (AliasFile), sizeof (buf));
  if (mutt_get_field (_("Save to file: "), buf, sizeof (buf), M_FILE) != 0)
    return;
  mutt_expand_path (buf, sizeof (buf));
  if ((rc = fopen (buf, "a")))
  {
    buf[0] = 0;
    rfc822_write_address (buf, sizeof (buf), new->addr);
    fprintf (rc, "alias %s ", new->name);
    write_safe_address (rc, buf);
    fputc ('\n', rc);
    fclose (rc);
    mutt_message _("Alias added.");
  }
  else
    mutt_perror (buf);
}

/*
 * This routine looks to see if the user has an alias defined for the given
 * address.
 */
ADDRESS *alias_reverse_lookup (ADDRESS *a)
{
  ALIAS *t = Aliases;
  ADDRESS *ap;

  if (!a || !a->mailbox)
    return NULL;

  for (; t; t = t->next)
  {
    /* cycle through all addresses if this is a group alias */
    for (ap = t->addr; ap; ap = ap->next)
    {
      if (!ap->group && ap->mailbox &&
	  strcasecmp (ap->mailbox, a->mailbox) == 0)
	return ap;
    }
  }
  return 0;
}

/* alias_complete() -- alias completion routine
 *
 * given a partial alias, this routine attempts to fill in the alias
 * from the alias list as much as possible
 */
int mutt_alias_complete (char *s, size_t buflen)
{
  ALIAS *a = Aliases;
  ALIAS *a_list = NULL, *a_cur = NULL;
  char bestname[STRING];
  int i;

  memset (bestname, 0, sizeof (bestname));

#define min(a,b)        ((a<b)?a:b)

  while (a)
  {
    if (a->name && strstr (a->name, s) == a->name)
    {
      if (!bestname[0]) /* init */
	strfcpy (bestname, a->name, min (strlen (a->name) + 1, sizeof (bestname)));
      else
      {
	for (i = 0 ; a->name[i] && a->name[i] == bestname[i] ; i++)
	  ;
	bestname[i] = 0;
      }
    }
    a = a->next;
  }

  if ((bestname[0] == 0) || /* if we didn't find anything */
      (s[0] == 0))          /* or we weren't given anything */
  {
    mutt_alias_menu (s, buflen, Aliases);
    return 0;
  }
  else
  {
    if (strcmp (bestname, s) == 0) /* add anything to the completion? */
    {
      /* build alias list and show it */
      a = Aliases;
      while (a)
      {
	if (a->name && (strstr (a->name, s) == a->name))
	{
	  if (!a_list)  /* init */
	    a_cur = a_list = (ALIAS *) safe_malloc (sizeof (ALIAS));
	  else
	  {
	    a_cur->next = (ALIAS *) safe_malloc (sizeof (ALIAS));
	    a_cur = a_cur->next;
	  }
	  memcpy (a_cur, a, sizeof (ALIAS));
	  a_cur->next = NULL;
	}
	a = a->next;
      }

      s[0] = 0; /* reset string before passing to alias_menu */
      mutt_alias_menu (s, buflen, a_list);

      /* free the alias list */
      while (a_list)
      {
	a_cur = a_list;
	a_list = a_list->next;
	safe_free ((void **) &a_cur);
      }

      return 0;
    }
    else /* we are adding something to the completion */
      strfcpy (s, bestname, strlen (bestname) + 1);
  }

  return 1;
}

static int string_is_address(const char *str, const char *u, const char *d)
{
  char buf[LONG_STRING];
  
  snprintf(buf, sizeof(buf), "%s@%s", NONULL(u), NONULL(d));
  if (strcasecmp(str, buf) == 0)
    return 1;
  
  return 0;
}

/* returns TRUE if the given address belongs to the user. */
int mutt_addr_is_user (ADDRESS *addr)
{
  /* NULL address is assumed to be the user. */
  if (!addr)
    return 1;
  if (!addr->mailbox)
    return 0;

  if (strcasecmp (addr->mailbox, NONULL(Username)) == 0)
    return 1;
  if(string_is_address(addr->mailbox, Username, Hostname))
    return 1;
  if(string_is_address(addr->mailbox, Username, mutt_fqdn(0)))
    return 1;
  if(string_is_address(addr->mailbox, Username, mutt_fqdn(1)))
    return 1;

  if (Alternates.pattern &&
      regexec (Alternates.rx, addr->mailbox, 0, NULL, 0) == 0)
    return 1;
  
  return 0;
}

/* returns 1 if the list of address contains a known mailing list */
int mutt_is_list_recipient (ADDRESS *a)
{
  for (; a; a = a->next)
    if (mutt_is_mail_list (a))
      return 1;
  return 0;
}
