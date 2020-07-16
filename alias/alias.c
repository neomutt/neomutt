/**
 * @file
 * Representation of a single alias to an email address
 *
 * @authors
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @page alias_alias Representation of a single alias to an email address
 *
 * Representation of a single alias to an email address
 */

#include "config.h"
#include <stddef.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "alias.h"
#include "lib.h"
#include "maillist.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "reverse.h"
#include "send/lib.h"

struct AliasList Aliases = TAILQ_HEAD_INITIALIZER(Aliases); ///< List of all the user's email aliases

/**
 * write_safe_address - Defang malicious email addresses
 * @param fp File to write to
 * @param s  Email address to defang
 *
 * if someone has an address like
 *      From: Michael `/bin/rm -f ~` Elkins <me@mutt.org>
 * and the user creates an alias for this, NeoMutt could wind up executing
 * the backticks because it writes aliases like
 *      alias me Michael `/bin/rm -f ~` Elkins <me@mutt.org>
 * To avoid this problem, use a backslash (\) to quote any backticks.  We also
 * need to quote backslashes as well, since you could defeat the above by
 * doing
 *      From: Michael \`/bin/rm -f ~\` Elkins <me@mutt.org>
 * since that would get aliased as
 *      alias me Michael \\`/bin/rm -f ~\\` Elkins <me@mutt.org>
 * which still gets evaluated because the double backslash is not a quote.
 *
 * Additionally, we need to quote ' and " characters - otherwise, neomutt will
 * interpret them on the wrong parsing step.
 *
 * $ wants to be quoted since it may indicate the start of an environment
 * variable.
 */
static void write_safe_address(FILE *fp, char *s)
{
  while (*s)
  {
    if ((*s == '\\') || (*s == '`') || (*s == '\'') || (*s == '"') || (*s == '$'))
      fputc('\\', fp);
    fputc(*s, fp);
    s++;
  }
}

/**
 * expand_aliases_r - Expand aliases, recursively
 * @param[in]  al   Address List
 * @param[out] expn Alias List
 *
 * ListHead expn is used as temporary storage for already-expanded aliases.
 */
static void expand_aliases_r(struct AddressList *al, struct ListHead *expn)
{
  struct Address *a = TAILQ_FIRST(al);
  while (a)
  {
    if (!a->group && !a->personal && a->mailbox && !strchr(a->mailbox, '@'))
    {
      struct AddressList *alias = alias_lookup(a->mailbox);
      if (alias)
      {
        bool duplicate = false;
        struct ListNode *np = NULL;
        STAILQ_FOREACH(np, expn, entries)
        {
          if (mutt_str_equal(a->mailbox, np->data)) /* alias already found */
          {
            mutt_debug(LL_DEBUG1, "loop in alias found for '%s'\n", a->mailbox);
            duplicate = true;
            break;
          }
        }

        if (duplicate)
        {
          // We've already seen this alias, so drop it
          struct Address *next = TAILQ_NEXT(a, entries);
          TAILQ_REMOVE(al, a, entries);
          mutt_addr_free(&a);
          a = next;
          continue;
        }

        // Keep a list of aliases that we've already seen
        mutt_list_insert_head(expn, mutt_str_dup(a->mailbox));

        /* The alias may expand to several addresses,
         * some of which may themselves be aliases.
         * Create a copy and recursively expand any aliases within. */
        struct AddressList copy = TAILQ_HEAD_INITIALIZER(copy);
        mutt_addrlist_copy(&copy, alias, false);
        expand_aliases_r(&copy, expn);

        /* Next, move the expanded addresses
         * from the copy into the original list (before the alias) */
        struct Address *a2 = NULL, *tmp = NULL;
        TAILQ_FOREACH_SAFE(a2, &copy, entries, tmp)
        {
          TAILQ_INSERT_BEFORE(a, a2, entries);
        }
        a = TAILQ_PREV(a, AddressList, entries);
        // Finally, remove the alias itself
        struct Address *next = TAILQ_NEXT(a, entries);
        TAILQ_REMOVE(al, next, entries);
        mutt_addr_free(&next);
      }
      else
      {
        struct passwd *pw = getpwnam(a->mailbox);
        if (pw)
        {
          char namebuf[256];

          mutt_gecos_name(namebuf, sizeof(namebuf), pw);
          mutt_str_replace(&a->personal, namebuf);
        }
      }
    }
    a = TAILQ_NEXT(a, entries);
  }

  const char *fqdn = NULL;
  if (C_UseDomain && (fqdn = mutt_fqdn(true, NeoMutt->sub)))
  {
    /* now qualify all local addresses */
    mutt_addrlist_qualify(al, fqdn);
  }
}

/**
 * recode_buf - Convert some text between two character sets
 * @param buf    Buffer to convert
 * @param buflen Length of buffer
 *
 * The 'from' charset is controlled by the 'charset'        config variable.
 * The 'to'   charset is controlled by the 'config_charset' config variable.
 */
static void recode_buf(char *buf, size_t buflen)
{
  if (!C_ConfigCharset || !C_Charset)
    return;

  char *s = mutt_str_dup(buf);
  if (!s)
    return;
  if (mutt_ch_convert_string(&s, C_Charset, C_ConfigCharset, 0) == 0)
    mutt_str_copy(buf, s, buflen);
  FREE(&s);
}

/**
 * check_alias_name - Sanity-check an alias name
 * @param s       Alias to check
 * @param dest    Buffer for the result
 * @param destlen Length of buffer
 * @retval  0 Success
 * @retval -1 Error
 *
 * Only characters which are non-special to both the RFC822 and the neomutt
 * configuration parser are permitted.
 */
static int check_alias_name(const char *s, char *dest, size_t destlen)
{
  wchar_t wc;
  mbstate_t mb;
  size_t l;
  int rc = 0;
  bool dry = !dest || !destlen;

  memset(&mb, 0, sizeof(mbstate_t));

  if (!dry)
    destlen--;
  for (; s && *s && (dry || destlen) && (l = mbrtowc(&wc, s, MB_CUR_MAX, &mb)) != 0;
       s += l, destlen -= l)
  {
    bool bad = (l == (size_t)(-1)) || (l == (size_t)(-2)); /* conversion error */
    bad = bad || (!dry && l > destlen); /* too few room for mb char */
    if (l == 1)
      bad = bad || (!strchr("-_+=.", *s) && !iswalnum(wc));
    else
      bad = bad || !iswalnum(wc);
    if (bad)
    {
      if (dry)
        return -1;
      if (l == (size_t)(-1))
        memset(&mb, 0, sizeof(mbstate_t));
      *dest++ = '_';
      rc = -1;
    }
    else if (!dry)
    {
      memcpy(dest, s, l);
      dest += l;
    }
  }
  if (!dry)
    *dest = '\0';
  return rc;
}

/**
 * string_is_address - Does an email address match a user and domain?
 * @param str    Address string to test
 * @param user   User name
 * @param domain Domain name
 * @retval true They match
 */
static bool string_is_address(const char *str, const char *user, const char *domain)
{
  char buf[1024];

  snprintf(buf, sizeof(buf), "%s@%s", NONULL(user), NONULL(domain));
  if (mutt_istr_equal(str, buf))
    return true;

  return false;
}

/**
 * alias_lookup - Find an Alias
 * @param name Alias name to find
 * @retval ptr  Address for the Alias
 * @retval NULL No such Alias
 *
 * @note The search is case-insensitive
 */
struct AddressList *alias_lookup(const char *name)
{
  struct Alias *a = NULL;

  TAILQ_FOREACH(a, &Aliases, entries)
  {
    if (mutt_istr_equal(name, a->name))
      return &a->addr;
  }
  return NULL;
}

/**
 * mutt_expand_aliases - Expand aliases in a List of Addresses
 * @param al AddressList
 *
 * Duplicate addresses are dropped
 */
void mutt_expand_aliases(struct AddressList *al)
{
  struct ListHead expn; /* previously expanded aliases to avoid loops */

  STAILQ_INIT(&expn);
  expand_aliases_r(al, &expn);
  mutt_list_free(&expn);
  mutt_addrlist_dedupe(al);
}

/**
 * mutt_expand_aliases_env - Expand aliases in all the fields of an Envelope
 * @param env Envelope to expand
 */
void mutt_expand_aliases_env(struct Envelope *env)
{
  mutt_expand_aliases(&env->from);
  mutt_expand_aliases(&env->to);
  mutt_expand_aliases(&env->cc);
  mutt_expand_aliases(&env->bcc);
  mutt_expand_aliases(&env->reply_to);
  mutt_expand_aliases(&env->mail_followup_to);
}

/**
 * mutt_get_address - Get an Address from an Envelope
 * @param[in]  env    Envelope to examine
 * @param[out] prefix Prefix for the Address, e.g. "To:"
 * @retval ptr AddressList in the Envelope
 *
 * @note The caller must NOT free the returned AddressList
 */
struct AddressList *mutt_get_address(struct Envelope *env, const char **prefix)
{
  struct AddressList *al = NULL;
  const char *pfx = NULL;

  if (mutt_addr_is_user(TAILQ_FIRST(&env->from)))
  {
    if (!TAILQ_EMPTY(&env->to) && !mutt_is_mail_list(TAILQ_FIRST(&env->to)))
    {
      pfx = "To";
      al = &env->to;
    }
    else
    {
      pfx = "Cc";
      al = &env->cc;
    }
  }
  else if (!TAILQ_EMPTY(&env->reply_to) && !mutt_is_mail_list(TAILQ_FIRST(&env->reply_to)))
  {
    pfx = "Reply-To";
    al = &env->reply_to;
  }
  else
  {
    al = &env->from;
    pfx = "From";
  }

  if (prefix)
    *prefix = pfx;

  return al;
}

/**
 * alias_create - Create a new Alias from an Address
 * @param al Address to use
 */
void alias_create(struct AddressList *al)
{
  struct Address *addr = NULL;
  char buf[1024], tmp[1024] = { 0 }, prompt[2048];
  char *pc = NULL;
  char *err = NULL;
  char fixed[1024];

  if (al)
  {
    addr = TAILQ_FIRST(al);
    if (addr && addr->mailbox)
    {
      mutt_str_copy(tmp, addr->mailbox, sizeof(tmp));
      pc = strchr(tmp, '@');
      if (pc)
        *pc = '\0';
    }
  }

  /* Don't suggest a bad alias name in the event of a strange local part. */
  check_alias_name(tmp, buf, sizeof(buf));

retry_name:
  /* L10N: prompt to add a new alias */
  if ((mutt_get_field(_("Alias as: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0) ||
      (buf[0] == '\0'))
  {
    return;
  }

  /* check to see if the user already has an alias defined */
  if (alias_lookup(buf))
  {
    mutt_error(_("You already have an alias defined with that name"));
    return;
  }

  if (check_alias_name(buf, fixed, sizeof(fixed)))
  {
    switch (mutt_yesorno(_("Warning: This alias name may not work.  Fix it?"), MUTT_YES))
    {
      case MUTT_YES:
        mutt_str_copy(buf, fixed, sizeof(buf));
        goto retry_name;
      case MUTT_ABORT:
        return;
      default:; // do nothing
    }
  }

  struct Alias *alias = alias_new();
  alias->name = mutt_str_dup(buf);

  mutt_addrlist_to_local(al);

  if (addr && addr->mailbox)
    mutt_str_copy(buf, addr->mailbox, sizeof(buf));
  else
    buf[0] = '\0';

  mutt_addrlist_to_intl(al, NULL);

  do
  {
    if ((mutt_get_field(_("Address: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0) ||
        (buf[0] == '\0'))
    {
      alias_free(&alias);
      return;
    }

    mutt_addrlist_parse(&alias->addr, buf);
    if (TAILQ_EMPTY(&alias->addr))
      mutt_beep(false);
    if (mutt_addrlist_to_intl(&alias->addr, &err))
    {
      mutt_error(_("Bad IDN: '%s'"), err);
      FREE(&err);
      continue;
    }
  } while (TAILQ_EMPTY(&alias->addr));

  if (addr && addr->personal && !mutt_is_mail_list(addr))
    mutt_str_copy(buf, addr->personal, sizeof(buf));
  else
    buf[0] = '\0';

  if (mutt_get_field(_("Personal name: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) != 0)
  {
    alias_free(&alias);
    return;
  }
  mutt_str_replace(&TAILQ_FIRST(&alias->addr)->personal, buf);

  buf[0] = '\0';
  if (mutt_get_field(_("Comment: "), buf, sizeof(buf), MUTT_COMP_NO_FLAGS) == 0)
    mutt_str_replace(&alias->comment, buf);

  buf[0] = '\0';
  mutt_addrlist_write(&alias->addr, buf, sizeof(buf), true);
  if (alias->comment)
  {
    snprintf(prompt, sizeof(prompt), "[%s = %s # %s] %s", alias->name, buf,
             alias->comment, _("Accept?"));
  }
  else
  {
    snprintf(prompt, sizeof(prompt), "[%s = %s] %s", alias->name, buf, _("Accept?"));
  }
  if (mutt_yesorno(prompt, MUTT_YES) != MUTT_YES)
  {
    alias_free(&alias);
    return;
  }

  alias_reverse_add(alias);
  TAILQ_INSERT_TAIL(&Aliases, alias, entries);

  mutt_str_copy(buf, C_AliasFile, sizeof(buf));
  if (mutt_get_field(_("Save to file: "), buf, sizeof(buf), MUTT_FILE | MUTT_CLEAR) != 0)
    return;
  mutt_expand_path(buf, sizeof(buf));
  FILE *fp_alias = fopen(buf, "a+");
  if (!fp_alias)
  {
    mutt_perror(buf);
    return;
  }

  /* terminate existing file with \n if necessary */
  if (fseek(fp_alias, 0, SEEK_END))
    goto fseek_err;
  if (ftell(fp_alias) > 0)
  {
    if (fseek(fp_alias, -1, SEEK_CUR) < 0)
      goto fseek_err;
    if (fread(buf, 1, 1, fp_alias) != 1)
    {
      mutt_perror(_("Error reading alias file"));
      mutt_file_fclose(&fp_alias);
      return;
    }
    if (fseek(fp_alias, 0, SEEK_END) < 0)
      goto fseek_err;
    if (buf[0] != '\n')
      fputc('\n', fp_alias);
  }

  if (check_alias_name(alias->name, NULL, 0))
    mutt_file_quote_filename(alias->name, buf, sizeof(buf));
  else
    mutt_str_copy(buf, alias->name, sizeof(buf));
  recode_buf(buf, sizeof(buf));
  fprintf(fp_alias, "alias %s ", buf);
  buf[0] = '\0';
  mutt_addrlist_write(&alias->addr, buf, sizeof(buf), false);
  recode_buf(buf, sizeof(buf));
  write_safe_address(fp_alias, buf);
  if (alias->comment)
    fprintf(fp_alias, " # %s", alias->comment);
  fputc('\n', fp_alias);
  if (mutt_file_fsync_close(&fp_alias) != 0)
    mutt_perror(_("Trouble adding alias"));
  else
    mutt_message(_("Alias added"));

  return;

fseek_err:
  mutt_perror(_("Error seeking in alias file"));
  mutt_file_fclose(&fp_alias);
}

/**
 * mutt_addr_is_user - Does the address belong to the user
 * @param addr Address to check
 * @retval true if the given address belongs to the user
 */
bool mutt_addr_is_user(const struct Address *addr)
{
  if (!addr)
  {
    mutt_debug(LL_DEBUG5, "no, NULL address\n");
    return false;
  }
  if (!addr->mailbox)
  {
    mutt_debug(LL_DEBUG5, "no, no mailbox\n");
    return false;
  }

  if (mutt_istr_equal(addr->mailbox, Username))
  {
    mutt_debug(LL_DEBUG5, "#1 yes, %s = %s\n", addr->mailbox, Username);
    return true;
  }
  if (string_is_address(addr->mailbox, Username, ShortHostname))
  {
    mutt_debug(LL_DEBUG5, "#2 yes, %s = %s @ %s\n", addr->mailbox, Username, ShortHostname);
    return true;
  }
  const char *fqdn = mutt_fqdn(false, NeoMutt->sub);
  if (string_is_address(addr->mailbox, Username, fqdn))
  {
    mutt_debug(LL_DEBUG5, "#3 yes, %s = %s @ %s\n", addr->mailbox, Username, NONULL(fqdn));
    return true;
  }
  fqdn = mutt_fqdn(true, NeoMutt->sub);
  if (string_is_address(addr->mailbox, Username, fqdn))
  {
    mutt_debug(LL_DEBUG5, "#4 yes, %s = %s @ %s\n", addr->mailbox, Username, NONULL(fqdn));
    return true;
  }

  if (C_From && mutt_istr_equal(C_From->mailbox, addr->mailbox))
  {
    mutt_debug(LL_DEBUG5, "#5 yes, %s = %s\n", addr->mailbox, C_From->mailbox);
    return true;
  }

  if (mutt_regexlist_match(&Alternates, addr->mailbox))
  {
    mutt_debug(LL_DEBUG5, "yes, %s matched by alternates\n", addr->mailbox);
    if (mutt_regexlist_match(&UnAlternates, addr->mailbox))
      mutt_debug(LL_DEBUG5, "but, %s matched by unalternates\n", addr->mailbox);
    else
      return true;
  }

  mutt_debug(LL_DEBUG5, "no, all failed\n");
  return false;
}

/**
 * alias_new - Create a new Alias
 * @retval ptr Newly allocated Alias
 *
 * Free the result with alias_free()
 */
struct Alias *alias_new(void)
{
  struct Alias *a = mutt_mem_calloc(1, sizeof(struct Alias));
  TAILQ_INIT(&a->addr);
  return a;
}

/**
 * alias_free - Free an Alias
 * @param[out] ptr Alias to free
 */
void alias_free(struct Alias **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Alias *alias = *ptr;

  struct EventAlias ea = { alias };
  notify_send(NeoMutt->notify, NT_ALIAS, NT_ALIAS_DELETED, &ea);

  FREE(&alias->name);
  FREE(&alias->comment);
  mutt_addrlist_clear(&(alias->addr));
  FREE(ptr);
}

/**
 * aliaslist_free - Free a List of Aliases
 * @param al AliasList to free
 */
void aliaslist_free(struct AliasList *al)
{
  struct Alias *np = NULL, *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, al, entries, tmp)
  {
    TAILQ_REMOVE(al, np, entries);
    alias_free(&np);
  }
  TAILQ_INIT(al);
}

/**
 * alias_init - Set up the Alias globals
 */
void alias_init(void)
{
  /* reverse alias keys need to be strdup'ed because of idna conversions */
  ReverseAliases = mutt_hash_new(1031, MUTT_HASH_STRCASECMP | MUTT_HASH_STRDUP_KEYS |
                                           MUTT_HASH_ALLOW_DUPS);
}

/**
 * alias_shutdown - Clean up the Alias globals
 */
void alias_shutdown(void)
{
  struct Alias *np = NULL;
  TAILQ_FOREACH(np, &Aliases, entries)
  {
    alias_reverse_delete(np);
  }
  aliaslist_free(&Aliases);
  mutt_hash_free(&ReverseAliases);
}
