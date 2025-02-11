/**
 * @file
 * Container for Accounts, Notifications
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CORE_NEOMUTT_H
#define MUTT_CORE_NEOMUTT_H

#include <locale.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include "account.h"
#include "command.h"
#include "mailbox.h"
#ifdef __APPLE__
#include <xlocale.h>
#endif

struct ConfigSet;
struct NeoMutt;

/**
 * @defgroup module_api NeoMutt library API
 *
 * Allow libraries to initialise themselves.
 */
struct Module
{
  const char *name;                   /// Name of the library module

  /**
   * @defgroup module_init init()
   * @ingroup module_api
   *
   * init - Initialise a Module
   * @retval true Success
   */
  bool (*init)(struct NeoMutt *n);

  /**
   * @defgroup module_config_define_types config_define_types()
   * @ingroup module_api
   *
   * config_define_types - Set up Config Types
   * @param cs Config Set
   * @retval true Success
   *
   * @pre cs is not NULL
   */
  bool (*config_define_types)(struct NeoMutt *n, struct ConfigSet *cs);

  /**
   * @defgroup module_config_define_variables config_define_variables()
   * @ingroup module_api
   *
   * config_define_variables - Define the Config Variables
   * @param cs Config Set
   * @retval true Success
   *
   * @pre cs is not NULL
   */
  bool (*config_define_variables)(struct NeoMutt *n, struct ConfigSet *cs);

  /**
   * @defgroup module_commands_register commands_register()
   * @ingroup module_api
   *
   * commands_register - Register NeoMutt Commands
   * @param ca Command Array
   * @retval true Success
   *
   * @pre ca is not NULL
   */
  bool (*commands_register)(struct NeoMutt *n, struct CommandArray *ca);

  /**
   * @defgroup module_gui_init gui_init()
   * @ingroup module_api
   *
   * gui_init - Initialise the GUI
   * @retval true Success
   */
  bool (*gui_init)(struct NeoMutt *n);

  /**
   * @defgroup module_gui_cleanup gui_cleanup()
   * @ingroup module_api
   *
   * gui_cleanup - Clean up the GUI
   *
   * @retval true Success
   */
  void (*gui_cleanup)(struct NeoMutt *n);

  /**
   * @defgroup module_cleanup cleanup()
   * @ingroup module_api
   *
   * cleanup - Clean up a Module
   * @retval true Success
   */
  void (*cleanup)(struct NeoMutt *n);

  void *mod_data;                     ///< Module specific data
};

/**
 * struct NeoMutt - Container for Accounts, Notifications
 */
struct NeoMutt
{
  struct Notify *notify;         ///< Notifications handler
  struct Notify *notify_resize;  ///< Window resize notifications handler
  struct Notify *notify_timeout; ///< Timeout notifications handler
  struct ConfigSubset *sub;      ///< Inherited config items
  struct AccountList accounts;   ///< List of all Accounts
  locale_t time_c_locale;        ///< Current locale but LC_TIME=C
  mode_t user_default_umask;     ///< User's default file writing permissions (inferred from umask)
  struct CommandArray commands;  ///< NeoMutt commands

  char *home_dir;                ///< User's home directory
  char *username;                ///< User's login name
  char **env;                    ///< Private copy of the environment variables
};

extern struct NeoMutt *NeoMutt;

/**
 * enum NotifyGlobal - Events not associated with an object
 *
 * Observers of #NT_GLOBAL will not be passed any Event data.
 */
enum NotifyGlobal
{
  NT_GLOBAL_STARTUP = 1, ///< NeoMutt is initialised
  NT_GLOBAL_SHUTDOWN,    ///< NeoMutt is about to close
  NT_GLOBAL_COMMAND,     ///< A NeoMutt command
};

bool            neomutt_account_add   (struct NeoMutt *n, struct Account *a);
bool            neomutt_account_remove(struct NeoMutt *n, const struct Account *a);
void            neomutt_free          (struct NeoMutt **ptr);
struct NeoMutt *neomutt_new           (struct ConfigSet *cs);

void   neomutt_mailboxlist_clear  (struct MailboxList *ml);
size_t neomutt_mailboxlist_get_all(struct MailboxList *head, struct NeoMutt *n, enum MailboxType type);

// Similar to mutt_file_fopen, but with the proper permissions inferred from
#define mutt_file_fopen_masked(PATH, MODE) mutt_file_fopen_masked_full(PATH, MODE, __FILE__, __LINE__, __func__)

#endif /* MUTT_CORE_NEOMUTT_H */
