/*
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_regex.h"
#include "mutt_curses.h"
#include "mutt_idna.h"

#include <string.h>
#include <ctype.h>

ADDRESS *mutt_lookup_alias (const char *s)
{
  ALIAS *t = Aliases;

  for (; t; t = t->next)
    if (!mutt_strcasecmp (s, t->name))
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
	  if (mutt_strcmp (a->mailbox, u->data) == 0) /* alias already found */
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
	  w = rfc822_cpy_adr (t, 0);
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

	if (pw)
	{
	  char namebuf[STRING];
	  
	  mutt_gecos_name (namebuf, sizeof (namebuf), pw);
	  mutt_str_replace (&a->personal, namebuf);
	  
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
  return (mutt_remove_duplicates (t));
}

void mutt_expand_aliases_env (ENVELOPE *env)
{
  env->from = mutt_expand_aliases (env->from);
  env->to = mutt_expand_aliases (env->to);
  env->cc = mutt_expand_aliases (env->cc);
  env->bcc = mutt_expand_aliases (env->bcc);
  env->reply_to = mutt_expand_aliases (env->reply_to);
  env->mail_followup_to = mutt_expand_aliases (env->mail_followup_to);
}


/* 
 * if someone has an address like
 *	From: Michael `/bin/rm -f ~` Elkins <me@mutt.org>
 * and the user creates an alias for this, Mutt could wind up executing
 * the backticks because it writes aliases like
 *	alias me Michael `/bin/rm -f ~` Elkins <me@mutt.org>
 * To avoid this problem, use a backslash (\) to quote any backticks.  We also
 * need to quote backslashes as well, since you could defeat the above by
 * doing
 *	From: Michael \`/bin/rm -f ~\` Elkins <me@mutt.org>
 * since that would get aliased as
 *	alias me Michael \\`/bin/rm -f ~\\` Elkins <me@mutt.org>
 * which still gets evaluated because the double backslash is not a quote.
 * 
 * Additionally, we need to quote ' and " characters - otherwise, mutt will
 * interpret them on the wrong parsing step.
 * 
 * $ wants to be quoted since it may indicate the start of an environment
 * variable.
 */

static void write_safe_address (FILE *fp, char *s)
{
  while (*s)
  {
    if (*s == '\\' || *s == '`' || *s == '\'' || *s == '"'
	|| *s == '$')
      fputc ('\\', fp);
    fputc (*s, fp);
    s++;
  }
}

ADDRESS *mutt_get_address (ENVELOPE *env, char **pfxp)
{
  ADDRESS *adr;
  char *pfx = NULL;

  if (mutt_addr_is_user (env->from))
  {
    if (env->to && !mutt_is_mail_list (env->to))
    {
      pfx = "To";
      adr = env->to;
    }
    else
    {
      pfx = "Cc";
      adr = env->cc;
    }
  }
  else if (env->reply_to && !mutt_is_mail_list (env->reply_to))
  {
    pfx = "Reply-To";
    adr = env->reply_to;
  }
  else
  {
    adr = env->from;
    pfx = "From";
  }

  if (pfxp) *pfxp = pfx;

  return adr;
}

static void recode_buf (char *buf, size_t buflen)
{
  char *s;

  if (!ConfigCharset || !*ConfigCharset || !Charset)
    return;
  s = safe_strdup (buf);
  if (!s)
    return;
  if (mutt_convert_string (&s, Charset, ConfigCharset, 0) == 0)
    strfcpy (buf, s, buflen);
  FREE(&s);
}

void mutt_create_alias (ENVELOPE *cur, ADDRESS *iadr)
{
  ALIAS *new, *t;
  char buf[LONG_STRING], tmp[LONG_STRING], prompt[SHORT_STRING], *pc;
  char *err = NULL;
  char fixed[LONG_STRING];
  FILE *rc;
  ADDRESS *adr = NULL;

  if (cur)
  {
    adr = mutt_get_address (cur, NULL);
  }
  else if (iadr)
  {
    adr = iadr;
  }

  if (adr && adr->mailbox)
  {
    strfcpy (tmp, adr->mailbox, sizeof (tmp));
    if ((pc = strchr (tmp, '@')))
      *pc = 0;
  }
  else
    tmp[0] = '\0';

  /* Don't suggest a bad alias name in the event of a strange local part. */
  mutt_check_alias_name (tmp, buf, sizeof (buf));
  
retry_name:
  /* L10N: prompt to add a new alias */
  if (mutt_get_field (_("Alias as: "), buf, sizeof (buf), 0) != 0 || !buf[0])
    return;

  /* check to see if the user already has an alias defined */
  if (mutt_lookup_alias (buf))
  {
    mutt_error _("You already have an alias defined with that name!");
    return;
  }
  
  if (mutt_check_alias_name (buf, fixed, sizeof (fixed)))
  {
    switch (mutt_yesorno (_("Warning: This alias name may not work.  Fix it?"), MUTT_YES))
    {
      case MUTT_YES:
      	strfcpy (buf, fixed, sizeof (buf));
	goto retry_name;
      case -1: 
	return;
    }
  }
  
  new       = safe_calloc (1, sizeof (ALIAS));
  new->self = new;
  new->name = safe_strdup (buf);

  mutt_addrlist_to_local (adr);
  
  if (adr)
    strfcpy (buf, adr->mailbox, sizeof (buf));
  else
    buf[0] = 0;

  mutt_addrlist_to_intl (adr, NULL);
  
  do
  {
    if (mutt_get_field (_("Address: "), buf, sizeof (buf), 0) != 0 || !buf[0])
    {
      mutt_free_alias (&new);
      return;
    }
    
    if((new->addr = rfc822_parse_adrlist (new->addr, buf)) == NULL)
      BEEP ();
    if (mutt_addrlist_to_intl (new->addr, &err))
    {
      mutt_error (_("Error: '%s' is a bad IDN."), err);
      mutt_sleep (2);
      continue;
    }
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
  rfc822_write_address (buf, sizeof (buf), new->addr, 1);
  snprintf (prompt, sizeof (prompt), _("[%s = %s] Accept?"), new->name, buf);
  if (mutt_yesorno (prompt, MUTT_YES) != MUTT_YES)
  {
    mutt_free_alias (&new);
    return;
  }

  mutt_alias_add_reverse (new);
  
  if ((t = Aliases))
  {
    while (t->next)
      t = t->next;
    t->next = new;
  }
  else
    Aliases = new;

  strfcpy (buf, NONULL (AliasFile), sizeof (buf));
  if (mutt_get_field (_("Save to file: "), buf, sizeof (buf), MUTT_FILE) != 0)
    return;
  mutt_expand_path (buf, sizeof (buf));
  if ((rc = fopen (buf, "a+")))
  {
    /* terminate existing file with \n if necessary */
    if (fseek (rc, 0, SEEK_END))
      goto fseek_err;
    if (ftell(rc) > 0)
    {
      if (fseek (rc, -1, SEEK_CUR) < 0)
	goto fseek_err;
      if (fread(buf, 1, 1, rc) != 1)
      {
	mutt_perror (_("Error reading alias file"));
	safe_fclose (&rc);
	return;
      }
      if (fseek (rc, 0, SEEK_END) < 0)
	goto fseek_err;
      if (buf[0] != '\n')
	fputc ('\n', rc);
    }

    if (mutt_check_alias_name (new->name, NULL, 0))
      mutt_quote_filename (buf, sizeof (buf), new->name);
    else
      strfcpy (buf, new->name, sizeof (buf));
    recode_buf (buf, sizeof (buf));
    fprintf (rc, "alias %s ", buf);
    buf[0] = 0;
    rfc822_write_address (buf, sizeof (buf), new->addr, 0);
    recode_buf (buf, sizeof (buf));
    write_safe_address (rc, buf);
    fputc ('\n', rc);
    safe_fclose (&rc);
    mutt_message _("Alias added.");
  }
  else
    mutt_perror (buf);

  return;
  
  fseek_err:
  mutt_perror (_("Error seeking in alias file"));
  safe_fclose (&rc);
  return;
}

/* 
 * Sanity-check an alias name:  Only characters which are non-special to both
 * the RFC 822 and the mutt configuration parser are permitted.
 */

int mutt_check_alias_name (const char *s, char *dest, size_t destlen)
{
  wchar_t wc;
  mbstate_t mb;
  size_t l;
  int rv = 0, bad = 0, dry = !dest || !destlen;

  memset (&mb, 0, sizeof (mbstate_t));

  if (!dry)
    destlen--;
  for (; s && *s && (dry || destlen) &&
       (l = mbrtowc (&wc, s, MB_CUR_MAX, &mb)) != 0;
       s += l, destlen -= l)
  {
    bad = l == (size_t)(-1) || l == (size_t)(-2); /* conversion error */
    bad = bad || (!dry && l > destlen);		/* too few room for mb char */
    if (l == 1)
      bad = bad || (strchr ("-_+=.", *s) == NULL && !iswalnum (wc));
    else
      bad = bad || !iswalnum (wc);
    if (bad)
    {
      if (dry)
	return -1;
      if (l == (size_t)(-1))
        memset (&mb, 0, sizeof (mbstate_t));
      *dest++ = '_';
      rv = -1;
    }
    else if (!dry)
    {
      memcpy (dest, s, l);
      dest += l;
    }
  }
  if (!dry)
    *dest = 0;
  return rv;
}

/*
 * This routine looks to see if the user has an alias defined for the given
 * address.
 */
ADDRESS *alias_reverse_lookup (ADDRESS *a)
{
  if (!a || !a->mailbox)
      return NULL;
  
  return hash_find (ReverseAlias, a->mailbox);
}

void mutt_alias_add_reverse (ALIAS *t)
{
  ADDRESS *ap;
  if (!t)
    return;
  
  for (ap = t->addr; ap; ap = ap->next)
  {
    if (!ap->group && ap->mailbox)
      hash_insert (ReverseAlias, ap->mailbox, ap, 1);
  }
}

void mutt_alias_delete_reverse (ALIAS *t)
{
  ADDRESS *ap;
  if (!t)
    return;
  
  for (ap = t->addr; ap; ap = ap->next)
  {
    if (!ap->group && ap->mailbox)
      hash_delete (ReverseAlias, ap->mailbox, ap, NULL);
  }
}

/* alias_complete() -- alias completion routine
 *
 * given a partial alias, this routine attempts to fill in the alias
 * from the alias list as much as possible. if given empty search string
 * or found nothing, present all aliases
 */
int mutt_alias_complete (char *s, size_t buflen)
{
  ALIAS *a = Aliases;
  ALIAS *a_list = NULL, *a_cur = NULL;
  char bestname[HUGE_STRING];
  int i;

#ifndef min
#define min(a,b)        ((a<b)?a:b)
#endif

  if (s[0] != 0) /* avoid empty string as strstr argument */
  {
    memset (bestname, 0, sizeof (bestname));

    while (a)
    {
      if (a->name && strstr (a->name, s) == a->name)
      {
	if (!bestname[0]) /* init */
	  strfcpy (bestname, a->name,
		   min (mutt_strlen (a->name) + 1, sizeof (bestname)));
	else
	{
	  for (i = 0 ; a->name[i] && a->name[i] == bestname[i] ; i++)
	    ;
	  bestname[i] = 0;
	}
      }
      a = a->next;
    }

    if (bestname[0] != 0)
    {
      if (mutt_strcmp (bestname, s) != 0)
      {
	/* we are adding something to the completion */
	strfcpy (s, bestname, mutt_strlen (bestname) + 1);
	return 1;
      }

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
    }
  }

  bestname[0] = 0;
  mutt_alias_menu (bestname, sizeof(bestname), a_list ? a_list : Aliases);
  if (bestname[0] != 0)
    strfcpy (s, bestname, buflen);

  /* free the alias list */
  while (a_list)
  {
    a_cur = a_list;
    a_list = a_list->next;
    FREE (&a_cur);
  }

  /* remove any aliases marked for deletion */
  a_list = NULL;
  for (a_cur = Aliases; a_cur;)
  {
    if (a_cur->del)
    {
      if (a_list)
	a_list->next = a_cur->next;
      else
	Aliases = a_cur->next;
      
      a_cur->next = NULL;
      mutt_free_alias (&a_cur);
      
      if (a_list)
	a_cur = a_list;
      else
	a_cur = Aliases;
    }
    else
    {
      a_list = a_cur;
      a_cur  = a_cur->next;
    }
  }
  
  return 0;
}

static int string_is_address(const char *str, const char *u, const char *d)
{
  char buf[LONG_STRING];
  
  snprintf(buf, sizeof(buf), "%s@%s", NONULL(u), NONULL(d));
  if (ascii_strcasecmp(str, buf) == 0)
    return 1;
  
  return 0;
}

/* returns TRUE if the given address belongs to the user. */
int mutt_addr_is_user (ADDRESS *addr)
{
  const char *fqdn;

  /* NULL address is assumed to be the user. */
  if (!addr)
  {
    dprint (5, (debugfile, "mutt_addr_is_user: yes, NULL address\n"));
    return 1;
  }
  if (!addr->mailbox)
  {
    dprint (5, (debugfile, "mutt_addr_is_user: no, no mailbox\n"));
    return 0;
  }

  if (ascii_strcasecmp (addr->mailbox, Username) == 0)
  {
    dprint (5, (debugfile, "mutt_addr_is_user: yes, %s = %s\n", addr->mailbox, Username));
    return 1;
  }
  if (string_is_address(addr->mailbox, Username, Hostname))
  {
    dprint (5, (debugfile, "mutt_addr_is_user: yes, %s = %s @ %s \n", addr->mailbox, Username, Hostname));
    return 1;
  }
  fqdn = mutt_fqdn (0);
  if (string_is_address(addr->mailbox, Username, fqdn))
  {
    dprint (5, (debugfile, "mutt_addr_is_user: yes, %s = %s @ %s \n", addr->mailbox, Username, NONULL(fqdn)));
    return 1;
  }
  fqdn = mutt_fqdn (1);
  if (string_is_address(addr->mailbox, Username, fqdn))
  {
    dprint (5, (debugfile, "mutt_addr_is_user: yes, %s = %s @ %s \n", addr->mailbox, Username, NONULL(fqdn)));
    return 1;
  }

  if (From && !ascii_strcasecmp (From->mailbox, addr->mailbox))
  {
    dprint (5, (debugfile, "mutt_addr_is_user: yes, %s = %s\n", addr->mailbox, From->mailbox));
    return 1;
  }

  if (mutt_match_rx_list (addr->mailbox, Alternates))
  {
    dprint (5, (debugfile, "mutt_addr_is_user: yes, %s matched by alternates.\n", addr->mailbox));
    if (mutt_match_rx_list (addr->mailbox, UnAlternates))
      dprint (5, (debugfile, "mutt_addr_is_user: but, %s matched by unalternates.\n", addr->mailbox));
    else
      return 1;
  }
  
  dprint (5, (debugfile, "mutt_addr_is_user: no, all failed.\n"));
  return 0;
}
