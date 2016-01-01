/*
 * Copyright (C) 2003,2005,2008-2009 Thomas Roessler <roessler@does-not-exist.org>
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

#ifdef HAVE_LIBIDN
static int check_idn (char *domain)
{
  if (! domain)
    return 0;

  if (ascii_strncasecmp (domain, "xn--", 4) == 0)
    return 1;

  while ((domain = strchr (domain, '.')) != NULL)
  {
    if (ascii_strncasecmp (++domain, "xn--", 4) == 0)
      return 1;
  }

  return 0;
}
#endif /* HAVE_LIBIDN */

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

static int addr_is_local (ADDRESS *a)
{
  return (a->intl_checked && !a->is_intl);
}

static int addr_is_intl (ADDRESS *a)
{
  return (a->intl_checked && a->is_intl);
}

static void set_local_mailbox (ADDRESS *a, char *local_mailbox)
{
  FREE (&a->mailbox);
  a->mailbox = local_mailbox;
  a->intl_checked = 1;
  a->is_intl = 0;
}

static void set_intl_mailbox (ADDRESS *a, char *intl_mailbox)
{
  FREE (&a->mailbox);
  a->mailbox = intl_mailbox;
  a->intl_checked = 1;
  a->is_intl = 1;
}

static char *intl_to_local (char *orig_user, char *orig_domain, int flags)
{
  char *local_user = NULL, *local_domain = NULL, *mailbox = NULL;
  char *reversed_user = NULL, *reversed_domain = NULL;
  char *tmp = NULL;
#ifdef HAVE_LIBIDN
  int is_idn_encoded = 0;
#endif /* HAVE_LIBIDN */

  local_user = safe_strdup (orig_user);
  local_domain = safe_strdup (orig_domain);

#ifdef HAVE_LIBIDN
  is_idn_encoded = check_idn (local_domain);
  if (is_idn_encoded && option (OPTIDNDECODE))
  {
    if (idna_to_unicode_8z8z (local_domain, &tmp, IDNA_ALLOW_UNASSIGNED) != IDNA_SUCCESS)
      goto cleanup;
    mutt_str_replace (&local_domain, tmp);
    FREE (&tmp);
  }
#endif /* HAVE_LIBIDN */

  /* we don't want charset-hook effects, so we set flags to 0 */
  if (mutt_convert_string (&local_user, "utf-8", Charset, 0) == -1)
    goto cleanup;

  if (mutt_convert_string (&local_domain, "utf-8", Charset, 0) == -1)
    goto cleanup;

  /*
   * make sure that we can convert back and come out with the same
   * user and domain name.
   */
  if ((flags & MI_MAY_BE_IRREVERSIBLE) == 0)
  {
    reversed_user = safe_strdup (local_user);

    if (mutt_convert_string (&reversed_user, Charset, "utf-8", 0) == -1)
    {
      dprint (1, (debugfile,
                  "intl_to_local: Not reversible. Charset conv to utf-8 failed for user = '%s'.\n",
                  reversed_user));
      goto cleanup;
    }

    if (ascii_strcasecmp (orig_user, reversed_user))
    {
      dprint (1, (debugfile, "intl_to_local: Not reversible. orig = '%s', reversed = '%s'.\n",
                  orig_user, reversed_user));
      goto cleanup;
    }

    reversed_domain = safe_strdup (local_domain);

    if (mutt_convert_string (&reversed_domain, Charset, "utf-8", 0) == -1)
    {
      dprint (1, (debugfile,
                  "intl_to_local: Not reversible. Charset conv to utf-8 failed for domain = '%s'.\n",
                  reversed_domain));
      goto cleanup;
    }

#ifdef HAVE_LIBIDN
    /* If the original domain was UTF-8, idna encoding here could
     * produce a non-matching domain!  Thus we only want to do the
     * idna_to_ascii_8z() if the original domain was IDNA encoded.
     */
    if (is_idn_encoded && option (OPTIDNDECODE))
    {
      if (idna_to_ascii_8z (reversed_domain, &tmp, IDNA_ALLOW_UNASSIGNED) != IDNA_SUCCESS)
      {
        dprint (1, (debugfile,
                    "intl_to_local: Not reversible. idna_to_ascii_8z failed for domain = '%s'.\n",
                    reversed_domain));
        goto cleanup;
      }
      mutt_str_replace (&reversed_domain, tmp);
    }
#endif /* HAVE_LIBIDN */

    if (ascii_strcasecmp (orig_domain, reversed_domain))
    {
      dprint (1, (debugfile, "intl_to_local: Not reversible. orig = '%s', reversed = '%s'.\n",
                  orig_domain, reversed_domain));
      goto cleanup;
    }
  }

  mailbox = safe_malloc (mutt_strlen (local_user) + mutt_strlen (local_domain) + 2);
  sprintf (mailbox, "%s@%s", NONULL(local_user), NONULL(local_domain)); /* __SPRINTF_CHECKED__ */

cleanup:
  FREE (&local_user);
  FREE (&local_domain);
  FREE (&tmp);
  FREE (&reversed_domain);
  FREE (&reversed_user);

  return mailbox;
}

static char *local_to_intl (char *user, char *domain)
{
  char *intl_user = NULL, *intl_domain = NULL;
  char *mailbox = NULL;
  char *tmp = NULL;

  intl_user = safe_strdup (user);
  intl_domain = safe_strdup (domain);

  /* we don't want charset-hook effects, so we set flags to 0 */
  if (mutt_convert_string (&intl_user, Charset, "utf-8", 0) == -1)
    goto cleanup;

  if (mutt_convert_string (&intl_domain, Charset, "utf-8", 0) == -1)
    goto cleanup;

#ifdef HAVE_LIBIDN
  if (option (OPTIDNENCODE))
  {
    if (idna_to_ascii_8z (intl_domain, &tmp, IDNA_ALLOW_UNASSIGNED) != IDNA_SUCCESS)
      goto cleanup;
    mutt_str_replace (&intl_domain, tmp);
  }
#endif /* HAVE_LIBIDN */

  mailbox = safe_malloc (mutt_strlen (intl_user) + mutt_strlen (intl_domain) + 2);
  sprintf (mailbox, "%s@%s", NONULL(intl_user), NONULL(intl_domain)); /* __SPRINTF_CHECKED__ */

cleanup:
  FREE (&intl_user);
  FREE (&intl_domain);
  FREE (&tmp);

  return mailbox;
}

/* higher level functions */

int mutt_addrlist_to_intl (ADDRESS *a, char **err)
{
  char *user = NULL, *domain = NULL;
  char *intl_mailbox = NULL;
  int rv = 0;

  if (err)
    *err = NULL;

  for (; a; a = a->next)
  {
    if (!a->mailbox || addr_is_intl (a))
      continue;

    if (mbox_to_udomain (a->mailbox, &user, &domain) == -1)
      continue;

    intl_mailbox = local_to_intl (user, domain);
    if (! intl_mailbox)
    {
      rv = -1;
      if (err && !*err)
        *err = safe_strdup (a->mailbox);
      continue;
    }

    set_intl_mailbox (a, intl_mailbox);
  }

  return rv;
}

int mutt_addrlist_to_local (ADDRESS *a)
{
  char *user = NULL, *domain = NULL;
  char *local_mailbox = NULL;

  for (; a; a = a->next)
  {
    if (!a->mailbox || addr_is_local (a))
      continue;

    if (mbox_to_udomain (a->mailbox, &user, &domain) == -1)
      continue;

    local_mailbox = intl_to_local (user, domain, 0);
    if (local_mailbox)
      set_local_mailbox (a, local_mailbox);
  }

  return 0;
}

/* convert just for displaying purposes */
const char *mutt_addr_for_display (ADDRESS *a)
{
  char *user = NULL, *domain = NULL;
  static char *buff = NULL;
  char *local_mailbox = NULL;

  FREE (&buff);

  if (!a->mailbox || addr_is_local (a))
    return a->mailbox;

  if (mbox_to_udomain (a->mailbox, &user, &domain) == -1)
    return a->mailbox;

  local_mailbox = intl_to_local (user, domain, MI_MAY_BE_IRREVERSIBLE);
  if (! local_mailbox)
    return a->mailbox;

  mutt_str_replace (&buff, local_mailbox);
  FREE (&local_mailbox);
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
 * Note that #a escapes and double quotes the argument.
 */
#define H_TO_INTL(a)	\
  if (mutt_addrlist_to_intl (env->a, err) && !e) \
  { \
     if (tag) *tag = #a; e = 1; err = NULL; \
  }

int mutt_env_to_intl (ENVELOPE *env, char **tag, char **err)
{
  int e = 0;
  H_TO_INTL(return_path);
  H_TO_INTL(from);
  H_TO_INTL(to);
  H_TO_INTL(cc);
  H_TO_INTL(bcc);
  H_TO_INTL(reply_to);
  H_TO_INTL(mail_followup_to);
  return e;
}

#undef H_TO_INTL
