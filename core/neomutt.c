/**
 * @file
 * Container for Accounts, Notifications
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "neomutt.h"
#include "account.h"
#include "mailbox.h"

/**
 * init_locale - Initialise the Locale/NLS settings
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

  return true;
}

/**
 * init_modules - Initialise the Modules
 * @param n       Neomutt
 * @param modules Library modules to initialise
 * @retval true Success
 */
static bool init_modules(struct NeoMutt *n, const struct Module **modules)
{
  if (!n)
    return false;

  if (!modules)
    return true;

  n->modules = modules;

  bool rc = true;

  // Initialise the Modules
  for (int i = 0; modules[i]; i++)
  {
    const struct Module *mod = modules[i];

    if (mod->init)
    {
      mutt_debug(LL_DEBUG3, "%s:init()\n", mod->name);
      rc &= mod->init(n);
    }
  }

  return rc;
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

  n->sub = cs_subset_new(NULL, NULL, n->notify);
  n->sub->scope = SET_SCOPE_NEOMUTT;
  n->sub->cs = cs_new(500);

  if (!n->modules)
    return true;

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
 * neomutt_new - Create the main NeoMutt object
 * @retval ptr New NeoMutt
 */
struct NeoMutt *neomutt_new(void)
{
  struct NeoMutt *n = MUTT_MEM_CALLOC(1, struct NeoMutt);

  return n;
}

/**
 * neomutt_init - Initialise NeoMutt
 * @param n       NeoMutt
 * @param modules Library modules to initialise
 * @retval true Success
 */
bool neomutt_init(struct NeoMutt *n, const struct Module **modules)
{
  if (!n)
    return false;

  MuttLogger = log_disp_queue;
  mutt_debug(LL_DEBUG1, "first log message\n");

  if (!init_locale(n))
    return false;

  if (!init_modules(n, modules))
    return false;

  if (!init_config(n))
    return false;

  if (!init_commands(n))
    return false;

  TAILQ_INIT(&n->accounts);
  n->notify = notify_new();

  n->notify_timeout = notify_new();
  notify_set_parent(n->notify_timeout, n->notify);

  n->notify_resize = notify_new();
  notify_set_parent(n->notify_resize, n->notify);

  // Change the current umask, and save the original one
  n->user_default_umask = umask(077);
  mutt_debug(LL_DEBUG1, "user's umask %03o\n", NeoMutt->user_default_umask);
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

  // Clean up the Modules
  for (int i = 0; n->modules[i]; i++)
  {
    const struct Module *mod = n->modules[i];

    if (mod->cleanup)
    {
      mutt_debug(LL_DEBUG3, "%s:cleanup()\n", mod->name);
      mod->cleanup(n);
    }
  }
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

  neomutt_account_remove(n, NULL);

  if (n->sub)
  {
    cs_free(&n->sub->cs);
    cs_subset_free(&n->sub);
  }

  notify_free(&n->notify_resize);
  notify_free(&n->notify_timeout);
  notify_free(&n->notify);

  if (n->time_c_locale)
    freelocale(n->time_c_locale);

  ARRAY_FREE(&n->commands);

  FREE(&n->home_dir);
  FREE(&n->username);

  envlist_free(&n->env);

  FREE(ptr);
}

/**
 * neomutt_gui_init - Initialise NeoMutt's GUI
 * @param n NeoMutt
 * @retval true Success
 */
bool neomutt_gui_init(struct NeoMutt *n)
{
  if (!n || !n->modules)
    return false;

  bool rc = true;

  // Initialise the GUI
  for (int i = 0; n->modules[i]; i++)
  {
    const struct Module *mod = n->modules[i];

    if (mod->gui_init)
    {
      mutt_debug(LL_DEBUG3, "%s:gui_init()\n", mod->name);
      rc &= mod->gui_init(n);
    }
  }

  return true;
}

/**
 * neomutt_gui_cleanup - Clean up NeoMutt's GUI
 * @param n NeoMutt
 */
void neomutt_gui_cleanup(struct NeoMutt *n)
{
  if (!n || !n->modules)
    return;

  // Clean up the GUI
  for (int i = 0; n->modules[i]; i++)
  {
    const struct Module *mod = n->modules[i];

    if (mod->gui_cleanup)
    {
      mutt_debug(LL_DEBUG3, "%s:gui_cleanup()\n", mod->name);
      mod->gui_cleanup(n);
    }
  }
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

  TAILQ_INSERT_TAIL(&n->accounts, a, entries);
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
 * @retval true Account was removed
 *
 * @note If a is NULL, all the Accounts will be removed
 */
bool neomutt_account_remove(struct NeoMutt *n, const struct Account *a)
{
  if (!n || TAILQ_EMPTY(&n->accounts))
    return false;

  if (!a)
  {
    mutt_debug(LL_NOTIFY, "NT_ACCOUNT_DELETE_ALL\n");
    struct EventAccount ev_a = { NULL };
    notify_send(n->notify, NT_ACCOUNT, NT_ACCOUNT_DELETE_ALL, &ev_a);
  }

  bool result = false;
  struct Account *np = NULL;
  struct Account *tmp = NULL;
  TAILQ_FOREACH_SAFE(np, &n->accounts, entries, tmp)
  {
    if (a && (np != a))
      continue;

    TAILQ_REMOVE(&n->accounts, np, entries);
    account_free(&np);
    result = true;
    if (a)
      break;
  }
  return result;
}

/**
 * neomutt_mailboxlist_clear - Free a Mailbox List
 * @param ml Mailbox List to free
 *
 * @note The Mailboxes aren't freed
 */
void neomutt_mailboxlist_clear(struct MailboxList *ml)
{
  if (!ml)
    return;

  struct MailboxNode *mn = NULL;
  struct MailboxNode *tmp = NULL;
  STAILQ_FOREACH_SAFE(mn, ml, entries, tmp)
  {
    STAILQ_REMOVE(ml, mn, MailboxNode, entries);
    FREE(&mn);
  }
}

/**
 * neomutt_mailboxlist_get_all - Get a List of all Mailboxes
 * @param head List to store the Mailboxes
 * @param n    NeoMutt
 * @param type Type of Account to match, see #MailboxType
 * @retval num Number of Mailboxes in the List
 *
 * @note If type is #MUTT_MAILBOX_ANY then all Mailbox types will be matched
 */
size_t neomutt_mailboxlist_get_all(struct MailboxList *head, struct NeoMutt *n,
                                   enum MailboxType type)
{
  if (!n)
    return 0;

  size_t count = 0;
  struct Account *a = NULL;
  struct MailboxNode *mn = NULL;

  TAILQ_FOREACH(a, &n->accounts, entries)
  {
    if ((type > MUTT_UNKNOWN) && (a->type != type))
      continue;

    STAILQ_FOREACH(mn, &a->mailboxes, entries)
    {
      struct MailboxNode *mn2 = MUTT_MEM_CALLOC(1, struct MailboxNode);
      mn2->mailbox = mn->mailbox;
      STAILQ_INSERT_TAIL(head, mn2, entries);
      count++;
    }
  }

  return count;
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
