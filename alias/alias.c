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
 * @page alias_alias Alias for an email address
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
#include "enter/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "alternates.h"
#include "maillist.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "reverse.h"

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
 * Additionally, we need to quote ' and " characters, otherwise neomutt will
 * interpret them on the wrong parsing step.
 *
 * $ wants to be quoted since it may indicate the start of an environment
 * variable.
 */
static void write_safe_address(FILE *fp, const char *s)
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
          char namebuf[256] = { 0 };

          mutt_gecos_name(namebuf, sizeof(namebuf), pw);
          mutt_str_replace(&a->personal, namebuf);
        }
      }
    }
    a = TAILQ_NEXT(a, entries);
  }

  const char *fqdn = NULL;
  const bool c_use_domain = cs_subset_bool(NeoMutt->sub, "use_domain");
  if (c_use_domain && (fqdn = mutt_fqdn(true, NeoMutt->sub)))
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
  const char *const c_config_charset = cs_subset_string(NeoMutt->sub, "config_charset");
  const char *const c_charset = cs_subset_string(NeoMutt->sub, "charset");
  if (!c_config_charset || !c_charset)
    return;

  char *s = mutt_str_dup(buf);
  if (!s)
    return;
  if (mutt_ch_convert_string(&s, c_charset, c_config_charset, MUTT_ICONV_NO_FLAGS) == 0)
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
  wchar_t wc = 0;
  mbstate_t mbstate = { 0 };
  size_t l;
  int rc = 0;
  bool dry = !dest || !destlen;

  if (!dry)
    destlen--;
  for (; s && *s && (dry || destlen) && (l = mbrtowc(&wc, s, MB_CUR_MAX, &mbstate)) != 0;
       s += l, destlen -= l)
  {
    bool bad = (l == (size_t) (-1)) || (l == (size_t) (-2)); /* conversion error */
    bad = bad || (!dry && l > destlen); /* too few room for mb char */
    if (l == 1)
      bad = bad || (!strchr("-_+=.", *s) && !iswalnum(wc));
    else
      bad = bad || !iswalnum(wc);
    if (bad)
    {
      if (dry)
        return -1;
      if (l == (size_t) (-1))
        memset(&mbstate, 0, sizeof(mbstate_t));
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
  char buf[1024] = { 0 };

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
 * @param sub Config items
 */
void alias_create(struct AddressList *al, const struct ConfigSubset *sub)
{
  struct Buffer *buf = mutt_buffer_pool_get();
  struct Buffer *fixed = mutt_buffer_pool_get();
  struct Buffer *prompt = NULL;
  struct Buffer *tmp = mutt_buffer_pool_get();

  struct Address *addr = NULL;
  char *pc = NULL;
  char *err = NULL;
  FILE *fp_alias = NULL;

  if (al)
  {
    addr = TAILQ_FIRST(al);
    if (addr && addr->mailbox)
    {
      mutt_buffer_strcpy(tmp, addr->mailbox);
      pc = strchr(mutt_buffer_string(tmp), '@');
      if (pc)
        *pc = '\0';
    }
  }

  /* Don't suggest a bad alias name in the event of a strange local part. */
  check_alias_name(mutt_buffer_string(tmp), buf->data, buf->dsize);

retry_name:
  /* L10N: prompt to add a new alias */
  if ((mutt_buffer_get_field(_("Alias as: "), buf, MUTT_COMP_NO_FLAGS, false,
                             NULL, NULL, NULL) != 0) ||
      mutt_buffer_is_empty(buf))
  {
    goto done;
  }

  /* check to see if the user already has an alias defined */
  if (alias_lookup(mutt_buffer_string(buf)))
  {
    mutt_error(_("You already have an alias defined with that name"));
    goto done;
  }

  if (check_alias_name(mutt_buffer_string(buf), fixed->data, fixed->dsize))
  {
    switch (mutt_yesorno(_("Warning: This alias name may not work.  Fix it?"), MUTT_YES))
    {
      case MUTT_YES:
        mutt_buffer_copy(buf, fixed);
        goto retry_name;
      case MUTT_ABORT:
        goto done;
      default:; // do nothing
    }
  }

  struct Alias *alias = alias_new();
  alias->name = mutt_buffer_strdup(buf);

  mutt_addrlist_to_local(al);

  if (addr && addr->mailbox)
    mutt_buffer_strcpy(buf, addr->mailbox);
  else
    mutt_buffer_reset(buf);

  mutt_addrlist_to_intl(al, NULL);

  do
  {
    if ((mutt_buffer_get_field(_("Address: "), buf, MUTT_COMP_NO_FLAGS, false,
                               NULL, NULL, NULL) != 0) ||
        mutt_buffer_is_empty(buf))
    {
      alias_free(&alias);
      goto done;
    }

    mutt_addrlist_parse(&alias->addr, mutt_buffer_string(buf));
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
    mutt_buffer_strcpy(buf, addr->personal);
  else
    mutt_buffer_reset(buf);

  if (mutt_buffer_get_field(_("Personal name: "), buf, MUTT_COMP_NO_FLAGS,
                            false, NULL, NULL, NULL) != 0)
  {
    alias_free(&alias);
    goto done;
  }
  mutt_str_replace(&TAILQ_FIRST(&alias->addr)->personal, mutt_buffer_string(buf));

  mutt_buffer_reset(buf);
  if (mutt_buffer_get_field(_("Comment: "), buf, MUTT_COMP_NO_FLAGS, false,
                            NULL, NULL, NULL) == 0)
  {
    mutt_str_replace(&alias->comment, mutt_buffer_string(buf));
  }

  mutt_buffer_reset(buf);
  mutt_addrlist_write(&alias->addr, buf, true);
  prompt = mutt_buffer_pool_get();
  if (alias->comment)
  {
    mutt_buffer_printf(prompt, "[%s = %s # %s] %s", alias->name,
                       mutt_buffer_string(buf), alias->comment, _("Accept?"));
  }
  else
  {
    mutt_buffer_printf(prompt, "[%s = %s] %s", alias->name,
                       mutt_buffer_string(buf), _("Accept?"));
  }
  if (mutt_yesorno(mutt_buffer_string(prompt), MUTT_YES) != MUTT_YES)
  {
    alias_free(&alias);
    goto done;
  }

  alias_reverse_add(alias);
  TAILQ_INSERT_TAIL(&Aliases, alias, entries);

  const char *const c_alias_file = cs_subset_path(sub, "alias_file");
  mutt_buffer_strcpy(buf, c_alias_file);

  if (mutt_buffer_get_field(_("Save to file: "), buf, MUTT_COMP_FILE | MUTT_COMP_CLEAR,
                            false, NULL, NULL, NULL) != 0)
  {
    goto done;
  }
  mutt_expand_path(buf->data, buf->dsize);
  fp_alias = fopen(mutt_buffer_string(buf), "a+");
  if (!fp_alias)
  {
    mutt_perror(mutt_buffer_string(buf));
    goto done;
  }

  /* terminate existing file with \n if necessary */
  if (!mutt_file_seek(fp_alias, 0, SEEK_END))
  {
    goto done;
  }
  if (ftell(fp_alias) > 0)
  {
    if (!mutt_file_seek(fp_alias, -1, SEEK_CUR))
    {
      goto done;
    }
    if (fread(buf->data, 1, 1, fp_alias) != 1)
    {
      mutt_perror(_("Error reading alias file"));
      goto done;
    }
    if (!mutt_file_seek(fp_alias, 0, SEEK_END))
    {
      goto done;
    }
    if (buf->data[0] != '\n')
      fputc('\n', fp_alias);
  }

  if (check_alias_name(alias->name, NULL, 0))
    mutt_file_quote_filename(alias->name, buf->data, buf->dsize);
  else
    mutt_buffer_strcpy(buf, alias->name);

  recode_buf(buf->data, buf->dsize);
  fprintf(fp_alias, "alias %s ", mutt_buffer_string(buf));
  mutt_buffer_reset(buf);

  mutt_addrlist_write(&alias->addr, buf, false);
  recode_buf(buf->data, buf->dsize);
  write_safe_address(fp_alias, mutt_buffer_string(buf));
  if (alias->comment)
    fprintf(fp_alias, " # %s", alias->comment);
  fputc('\n', fp_alias);
  if (mutt_file_fsync_close(&fp_alias) != 0)
    mutt_perror(_("Trouble adding alias"));
  else
    mutt_message(_("Alias added"));

done:
  mutt_file_fclose(&fp_alias);
  mutt_buffer_pool_release(&buf);
  mutt_buffer_pool_release(&fixed);
  mutt_buffer_pool_release(&prompt);
  mutt_buffer_pool_release(&tmp);
}

/**
 * mutt_addr_is_user - Does the address belong to the user
 * @param addr Address to check
 * @retval true The given address belongs to the user
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

  const struct Address *c_from = cs_subset_address(NeoMutt->sub, "from");
  if (c_from && mutt_istr_equal(c_from->mailbox, addr->mailbox))
  {
    mutt_debug(LL_DEBUG5, "#5 yes, %s = %s\n", addr->mailbox, c_from->mailbox);
    return true;
  }

  if (mutt_alternates_match(addr->mailbox))
    return true;

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

  mutt_debug(LL_NOTIFY, "NT_ALIAS_DELETE: %s\n", alias->name);
  struct EventAlias ev_a = { alias };
  notify_send(NeoMutt->notify, NT_ALIAS, NT_ALIAS_DELETE, &ev_a);

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
  if (!al)
    return;

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
  alias_reverse_init();
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
  alias_reverse_shutdown();
}
