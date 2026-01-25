/**
 * @file
 * Container for Accounts, Notifications
 *
 * @authors
 * Copyright (C) 2019-2026 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
 * @page core_neomutt NeoMutt object
 *
 * Container for Accounts, Notifications
 */

#include "config.h"
#include <errno.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "neomutt.h"
#include "account.h"
#include "mailbox.h"
#include "muttlib.h"

struct NeoMutt *NeoMutt = NULL; ///< Global NeoMutt object

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

#define CONFIG_INIT_VARS(CS, NAME)                                             \
  bool config_init_##NAME(struct ConfigSet *cs);                               \
  config_init_##NAME(CS)

/**
 * init_env - Initialise the Environment
 * @param n    NeoMutt
 * @param envp External environment
 * @retval true Success
 */
static bool init_env(struct NeoMutt *n, char **envp)
{
  if (!n)
    return false;

  mutt_str_replace(&n->username, mutt_str_getenv("USER"));
  mutt_str_replace(&n->home_dir, mutt_str_getenv("HOME"));

  envlist_free(&n->env);
  n->env = envlist_init(envp);

  return true;
}

/**
 * init_locale - Initialise the Locale/NLS settings
 * @param n NeoMutt
 * @retval true Success
 */
static bool init_locale(struct NeoMutt *n)
{
  if (!n)
    return false;

  setlocale(LC_ALL, "");

#ifdef ENABLE_NLS
  const char *domdir = mutt_str_getenv("TEXTDOMAINDIR");
  if (domdir)
    bindtextdomain(PACKAGE, domdir);
  else
    bindtextdomain(PACKAGE, MUTTLOCALEDIR);
  textdomain(PACKAGE);
#endif

  n->time_c_locale = duplocale(LC_GLOBAL_LOCALE);
  if (n->time_c_locale)
    n->time_c_locale = newlocale(LC_TIME_MASK, "C", n->time_c_locale);

  if (!n->time_c_locale)
  {
    mutt_error("%s", strerror(errno)); // LCOV_EXCL_LINE
    return false;                      // LCOV_EXCL_LINE
  }

#ifndef LOCALES_HACK
  /* Do we have a locale definition? */
  if (mutt_str_getenv("LC_ALL") || mutt_str_getenv("LANG") || mutt_str_getenv("LC_CTYPE"))
  {
    OptLocales = true;
  }
#endif

  return true;
}

#ifdef ENABLE_NLS
/**
 * localise_config - Localise some config
 * @param cs Config Set
 */
static void localise_config(struct ConfigSet *cs)
{
  struct Buffer *value = buf_pool_get();
  struct HashElemArray hea = get_elem_list(cs, GEL_ALL_CONFIG);
  struct HashElem **hep = NULL;

  ARRAY_FOREACH(hep, &hea)
  {
    struct HashElem *he = *hep;
    if (!(he->type & D_L10N_STRING))
      continue;

    buf_reset(value);
    cs_he_initial_get(cs, he, value);

    // Lookup the translation
    const char *l10n = gettext(buf_string(value));
    config_he_set_initial(cs, he, l10n);
  }

  ARRAY_FREE(&hea);
  buf_pool_release(&value);
}
#endif

/**
 * reset_tilde - Temporary measure
 * @param cs Config Set
 */
static void reset_tilde(struct ConfigSet *cs)
{
  static const char *names[] = { "folder", "mbox", "postponed", "record" };

  struct Buffer *value = buf_pool_get();
  for (size_t i = 0; i < countof(names); i++)
  {
    struct HashElem *he = cs_get_elem(cs, names[i]);
    if (!he)
      continue;
    buf_reset(value);
    cs_he_initial_get(cs, he, value);
    expand_path(value, false);
    config_he_set_initial(cs, he, value->data);
  }
  buf_pool_release(&value);
}

/**
 * init_config - Initialise the config system
 * @param n Neomutt
 * @retval true Success
 *
 * Set up the config variables in three stages:
 * - Create the config types
 * - Create the config variables
 * - Set some run-time defaults
 */
static bool init_config(struct NeoMutt *n)
{
  if (!n)
    return false;

  n->cs = cs_new(500);

  n->sub = cs_subset_new(NULL, NULL, n->notify);
  n->sub->scope = SET_SCOPE_NEOMUTT;
  n->sub->cs = n->cs;

  bool rc = true;

  // Set up the Config Types
  for (int i = 0; n->modules[i]; i++)
  {
    const struct Module *mod = n->modules[i];

    if (mod->config_define_types)
    {
      mutt_debug(LL_DEBUG3, "%s:config_define_types()\n", mod->name);
      rc &= mod->config_define_types(n, n->sub->cs);
    }
  }

  if (!rc)
    return false;

  // Define the Config Variables
  for (int i = 0; n->modules[i]; i++)
  {
    const struct Module *mod = n->modules[i];

    if (mod->config_define_variables)
    {
      mutt_debug(LL_DEBUG3, "%s:config_define_variables()\n", mod->name);
      rc &= mod->config_define_variables(n, n->sub->cs);
    }
  }

  if (!rc)
    return false;

  // Post-processing
#ifdef ENABLE_NLS
  localise_config(n->sub->cs);
#endif
  reset_tilde(n->sub->cs);

  /* Unset suspend by default if we're the session leader */
  if (getsid(0) == getpid())
    config_str_set_initial(n->sub->cs, "suspend", "no");

  return rc;
}

/**
 * init_commands - Initialise the NeoMutt commands
 * @param n Neomutt
 * @retval true Success
 */
static bool init_commands(struct NeoMutt *n)
{
  if (!n)
    return false;

  if (!n->modules)
    return true;

  struct CommandArray *ca = &n->commands;

  bool rc = true;

  // Set up the Config Types
  for (int i = 0; n->modules[i]; i++)
  {
    const struct Module *mod = n->modules[i];

    if (mod->commands_register)
    {
      mutt_debug(LL_DEBUG3, "%s:commands_register()\n", mod->name);
      rc &= mod->commands_register(n, ca);
    }
  }

  return rc;
}

/**
 * init_modules - Initialise the Modules
 * @param n Neomutt
 * @retval true Success
 */
static bool init_modules(struct NeoMutt *n)
{
  if (!n)
    return false;

  if (!n->modules)
    return true;

  bool rc = true;

  // Initialise the Modules
  for (int i = 0; n->modules[i]; i++)
  {
    const struct Module *mod = n->modules[i];

    if (mod->init)
    {
      mutt_debug(LL_DEBUG3, "%s:init()\n", mod->name);
      rc &= mod->init(n);
    }
  }

  return rc;
}

/**
 * neomutt_new - Create the main NeoMutt object
 * @retval ptr New NeoMutt
 */
struct NeoMutt *neomutt_new(void)
{
  return MUTT_MEM_CALLOC(1, struct NeoMutt);
}

/**
 * neomutt_init - Initialise NeoMutt
 * @param n       NeoMutt
 * @param envp    External environment
 * @param modules Library modules to initialise
 * @retval true Success
 */
bool neomutt_init(struct NeoMutt *n, char **envp, const struct Module **modules)
{
  if (!n)
    return false;

  n->modules = modules;

  if (!init_env(n, envp))
    return false;

  if (!init_locale(n))
    return false;

  if (!init_config(n))
    return false;

  if (!init_commands(n))
    return false;

  if (!init_modules(n))
    return false;

  ARRAY_INIT(&n->accounts);
  n->notify = notify_new();

  n->notify_timeout = notify_new();
  notify_set_parent(n->notify_timeout, n->notify);

  n->notify_resize = notify_new();
  notify_set_parent(n->notify_resize, n->notify);

  n->groups = groups_new();

  // Change the current umask, and save the original one
  n->user_default_umask = umask(077);
  mutt_debug(LL_DEBUG1, "user's umask %03o\n", n->user_default_umask);
  mutt_debug(LL_DEBUG3, "umask set to 077\n");

  return true;
}

/**
 * cleanup_modules - Clean up each of the Modules
 * @param n NeoMutt
 */
static void cleanup_modules(struct NeoMutt *n)
{
  if (!n || !n->modules)
    return;
}

/**
 * neomutt_cleanup - Clean up NeoMutt and Modules
 * @param n NeoMutt
 */
void neomutt_cleanup(struct NeoMutt *n)
{
  if (!n)
    return;

  cleanup_modules(n);
}

/**
 * neomutt_free - Free a NeoMutt
 * @param[out] ptr NeoMutt to free
 */
void neomutt_free(struct NeoMutt **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct NeoMutt *n = *ptr;

  neomutt_accounts_free(n);
  notify_free(&n->notify_resize);
  notify_free(&n->notify_timeout);
  notify_free(&n->notify);
  if (n->time_c_locale)
    freelocale(n->time_c_locale);

  groups_free(&n->groups);

  FREE(&n->home_dir);
  FREE(&n->username);

  envlist_free(&n->env);
  if (n->sub)
  {
    struct ConfigSet *cs = n->sub->cs;
    cs_subset_free(&n->sub);
    cs_free(&cs);
  }

  FREE(ptr);
}

/**
 * neomutt_account_add - Add an Account to the global list
 * @param n NeoMutt
 * @param a Account to add
 * @retval true Account was added
 */
bool neomutt_account_add(struct NeoMutt *n, struct Account *a)
{
  if (!n || !a)
    return false;

  ARRAY_ADD(&n->accounts, a);
  notify_set_parent(a->notify, n->notify);

  mutt_debug(LL_NOTIFY, "NT_ACCOUNT_ADD: %s %p\n",
             mailbox_get_type_name(a->type), (void *) a);
  struct EventAccount ev_a = { a };
  notify_send(n->notify, NT_ACCOUNT, NT_ACCOUNT_ADD, &ev_a);
  return true;
}

/**
 * neomutt_account_remove - Remove an Account from the global list
 * @param n NeoMutt
 * @param a Account to remove
 */
void neomutt_account_remove(struct NeoMutt *n, struct Account *a)
{
  if (!n || !a || ARRAY_EMPTY(&n->accounts))
    return;

  struct Account **ap = NULL;
  ARRAY_FOREACH(ap, &n->accounts)
  {
    if ((*ap) != a)
      continue;

    ARRAY_REMOVE(&n->accounts, ap);
    account_free(&a);
    break;
  }
}

/**
 * neomutt_accounts_free - - Free all the Accounts
 * @param n NeoMutt
 */
void neomutt_accounts_free(struct NeoMutt *n)
{
  if (!n)
    return;

  if (!ARRAY_EMPTY(&n->accounts))
  {
    mutt_debug(LL_NOTIFY, "NT_ACCOUNT_DELETE_ALL\n");
    struct EventAccount ev_a = { NULL };
    notify_send(n->notify, NT_ACCOUNT, NT_ACCOUNT_DELETE_ALL, &ev_a);

    struct Account **ap = NULL;
    ARRAY_FOREACH(ap, &n->accounts)
    {
      account_free(ap);
    }
  }

  ARRAY_FREE(&n->accounts);
}

/**
 * neomutt_mailboxes_get - Get an Array of matching Mailboxes
 * @param n    NeoMutt
 * @param type Type of Account to match, see #MailboxType
 * @retval obj Array of Mailboxes
 *
 * @note If type is #MUTT_MAILBOX_ANY then all Mailbox types will be matched
 */
struct MailboxArray neomutt_mailboxes_get(struct NeoMutt *n, enum MailboxType type)
{
  struct MailboxArray ma = ARRAY_HEAD_INITIALIZER;

  if (!n)
    return ma;

  struct Account **ap = NULL;
  struct Mailbox **mp = NULL;

  ARRAY_FOREACH(ap, &n->accounts)
  {
    struct Account *a = *ap;
    if ((type > MUTT_UNKNOWN) && (a->type != type))
      continue;

    ARRAY_FOREACH(mp, &a->mailboxes)
    {
      ARRAY_ADD(&ma, *mp);
    }
  }

  return ma;
}

/**
 * mutt_file_fopen_masked_full - Wrapper around mutt_file_fopen_full()
 * @param path  Filename
 * @param mode  Mode e.g. "r" readonly; "w" read-write
 * @param file  Source file
 * @param line  Source line number
 * @param func  Source function
 * @retval ptr  FILE handle
 * @retval NULL Error, see errno
 *
 * Apply the user's umask, then call mutt_file_fopen_full().
 */
FILE *mutt_file_fopen_masked_full(const char *path, const char *mode,
                                  const char *file, int line, const char *func)
{
  // Set the user's umask (saved on startup)
  mode_t old_umask = umask(NeoMutt->user_default_umask);
  mutt_debug(LL_DEBUG3, "umask set to %03o\n", NeoMutt->user_default_umask);

  // The permissions will be limited by the umask
  FILE *fp = mutt_file_fopen_full(path, mode, 0666, file, line, func);

  umask(old_umask); // Immediately restore the umask
  mutt_debug(LL_DEBUG3, "umask set to %03o\n", old_umask);

  return fp;
}
