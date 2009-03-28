/*
 * Copyright (C) 2003,2005,2008 Thomas Roessler <roessler@does-not-exist.org>
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
#include "charset.h"
#include "mutt_idna.h"

/* The low-level interface we use. */

#ifdef HAVE_LIBIDN

/* check whether an address is an IDN */

static int check_idn (ADDRESS *ap)
{
  char *p = 0;

  if (!ap || !ap->mailbox)
    return 0;

  if (!ap->idn_checked)
  {
    ap->idn_checked = 1;
    for (p = strchr (ap->mailbox, '@'); p && *p; p = strchr (p, '.')) 
      if (ascii_strncasecmp (++p, "xn--", 4) == 0)
      {
	ap->is_idn = 1;
	break;
      }
  }
  
  return ap->is_idn;
}

static int mutt_idna_to_local (const char *in, char **out, int flags)
{
  *out = NULL;

  if (!option (OPTUSEIDN))
    goto notrans;

  if (!in)
    goto notrans;

  /* Is this the right function?  Interesting effects with some bad identifiers! */
  if (idna_to_unicode_8z8z (in, out, IDNA_ALLOW_UNASSIGNED) != IDNA_SUCCESS)
    goto notrans;

  /* we don't want charset-hook effects, so we set flags to 0 */
  if (mutt_convert_string (out, "utf-8", Charset, 0) == -1)
    goto notrans;

  /* 
   * make sure that we can convert back and come out with the same
   * domain name. 
   */
  
  if ((flags & MI_MAY_BE_IRREVERSIBLE) == 0)
  {
    int irrev = 0;
    char *t2 = NULL;
    char *tmp = safe_strdup (*out);

    /* we don't want charset-hook effects, so we set flags to 0 */
    if (mutt_convert_string (&tmp, Charset, "utf-8", 0) == -1)
      irrev = 1;
    if (!irrev && idna_to_ascii_8z (tmp, &t2, IDNA_ALLOW_UNASSIGNED) != IDNA_SUCCESS)
      irrev = 1;
    if (!irrev && ascii_strcasecmp (t2, in))
    {
      dprint (1, (debugfile, "mutt_idna_to_local: Not reversible. in = '%s', t2 = '%s'.\n",
		  in, t2));
      irrev = 1;
    }
    
    FREE (&t2);
    FREE (&tmp);

    if (irrev)
      goto notrans;
  }

  return 0;
  
 notrans:
  FREE (out);		/* __FREE_CHECKED__ */
  *out = safe_strdup (in);
  return 1;
}

static int mutt_local_to_idna (const char *in, char **out)
{
  int rv = 0;
  char *tmp = safe_strdup (in);
  *out = NULL;

  if (!in)
  {
    *out = NULL;
    return -1;
  }
  
  /* we don't want charset-hook effects, so we set flags to 0 */
  if (mutt_convert_string (&tmp, Charset, "utf-8", 0) == -1)
    rv = -1;
  if (!rv && idna_to_ascii_8z (tmp, out, IDNA_ALLOW_UNASSIGNED) != IDNA_SUCCESS)
    rv = -2;
  
  FREE (&tmp);
  if (rv < 0)
  {
    FREE (out);		/* __FREE_CHECKED__ */
    *out = safe_strdup (in);
  }
  return rv;
}

/* higher level functions */

static int mbox_to_udomain (const char *mbx, char **user, char **domain)
{
  static char *buff = NULL;
  char *p;
  
  mutt_str_replace (&buff, mbx);
  
  p = strchr (buff, '@');
  if (!p || !p[1])
    return -1;
  *p = '\0';
  *user = buff;
  *domain  = p + 1;
  return 0;
}

int mutt_addrlist_to_idna (ADDRESS *a, char **err)
{
  char *user = NULL, *domain = NULL;
  char *tmp = NULL;
  int e = 0;
  
  if (err)
    *err = NULL;

  for (; a; a = a->next)
  {
    if (!a->mailbox)
      continue;
    if (mbox_to_udomain (a->mailbox, &user, &domain) == -1)
      continue;
    
    if (mutt_local_to_idna (domain, &tmp) < 0)
    {
      e = 1;
      if (err)
	*err = safe_strdup (domain);
    }
    else
    {
      safe_realloc (&a->mailbox, mutt_strlen (user) + mutt_strlen (tmp) + 2);
      sprintf (a->mailbox, "%s@%s", NONULL(user), NONULL(tmp)); /* __SPRINTF_CHECKED__ */
      a->idn_checked = 0;
    }
    
    FREE (&tmp);
    
    if (e)
      return -1;
  }
  
  return 0;
}

int mutt_addrlist_to_local (ADDRESS *a)
{
  char *user, *domain;
  char *tmp = NULL;
  
  for (; a; a = a->next)
  {
    if (!a->mailbox)
      continue;
    if (!check_idn (a))
      continue;
    if (mbox_to_udomain (a->mailbox, &user, &domain) == -1)
      continue;
    if (mutt_idna_to_local (domain, &tmp, 0) == 0)
    {
      safe_realloc (&a->mailbox, mutt_strlen (user) + mutt_strlen (tmp) + 2);
      sprintf (a->mailbox, "%s@%s", NONULL (user), NONULL (tmp)); /* __SPRINTF_CHECKED__ */
      a->idn_checked = 0;
    }
    
    FREE (&tmp);
  }
  
  return 0;
}

/* convert just for displaying purposes */
const char *mutt_addr_for_display (ADDRESS *a)
{
  static char *buff = NULL;
  char *tmp = NULL;
  /* user and domain will be either allocated or reseted to the NULL in
   * the mbox_to_udomain(), but for safety... */
  char *domain = NULL;
  char *user = NULL;
  
  FREE (&buff);

  if (!check_idn (a))
    return a->mailbox;
  if (mbox_to_udomain (a->mailbox, &user, &domain) != 0)
    return a->mailbox;
  if (mutt_idna_to_local (domain, &tmp, MI_MAY_BE_IRREVERSIBLE) != 0)
  {
    FREE (&tmp);
    return a->mailbox;
  }
  
  safe_realloc (&buff, mutt_strlen (tmp) + mutt_strlen (user) + 2);
  sprintf (buff, "%s@%s", NONULL(user), NONULL(tmp)); /* __SPRINTF_CHECKED__ */
  FREE (&tmp);
  return buff;
}

/* Convert an ENVELOPE structure */

void mutt_env_to_local (ENVELOPE *e)
{
  mutt_addrlist_to_local (e->return_path);
  mutt_addrlist_to_local (e->from);
  mutt_addrlist_to_local (e->to);
  mutt_addrlist_to_local (e->cc);
  mutt_addrlist_to_local (e->bcc);
  mutt_addrlist_to_local (e->reply_to);
  mutt_addrlist_to_local (e->mail_followup_to);
}

/* Note that `a' in the `env->a' expression is macro argument, not
 * "real" name of an `env' compound member.  Real name will be substituted
 * by preprocessor at the macro-expansion time.
 */
#define H_TO_IDNA(a)	\
  if (mutt_addrlist_to_idna (env->a, err) && !e) \
  { \
     if (tag) *tag = #a; e = 1; err = NULL; \
  }

int mutt_env_to_idna (ENVELOPE *env, char **tag, char **err)
{
  int e = 0;
  H_TO_IDNA(return_path);
  H_TO_IDNA(from);
  H_TO_IDNA(to);
  H_TO_IDNA(cc);
  H_TO_IDNA(bcc);
  H_TO_IDNA(reply_to);
  H_TO_IDNA(mail_followup_to);
  return e;
}

#undef H_TO_IDNA

#endif /* HAVE_LIBIDN */
