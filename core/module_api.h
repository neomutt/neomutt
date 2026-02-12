/**
 * @file
 * Module API
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page core_module_api Module API
 *
 * Module API
 */

#ifndef MUTT_CORE_MODULE_H
#define MUTT_CORE_MODULE_H

#include <stdbool.h>

struct CommandArray;
struct ConfigSet;
struct NeoMutt;

/**
 * enum ModuleId - Unique Module Ids
 */
enum ModuleId
{
  // These two have priority
  MODULE_ID_MAIN,          ///< #ModuleMain,      @ref lib_main
  MODULE_ID_GUI,           ///< #ModuleGui,       @ref lib_gui

  MODULE_ID_ADDRESS,       ///< #ModuleAddress,   @ref lib_address
  MODULE_ID_ALIAS,         ///< #ModuleAlias,     @ref lib_alias
  MODULE_ID_ATTACH,        ///< #ModuleAttach,    @ref lib_attach
  MODULE_ID_AUTOCRYPT,     ///< #ModuleAutocrypt, @ref lib_autocrypt
  MODULE_ID_BCACHE,        ///< #ModuleBcache,    @ref lib_bcache
  MODULE_ID_BROWSER,       ///< #ModuleBrowser,   @ref lib_browser
  MODULE_ID_COLOR,         ///< #ModuleColor,     @ref lib_color
  MODULE_ID_COMMANDS,      ///< #ModuleCommands,  @ref lib_commands
  MODULE_ID_COMPLETE,      ///< #ModuleComplete,  @ref lib_complete
  MODULE_ID_COMPMBOX,      ///< #ModuleCompmbox,  @ref lib_compmbox
  MODULE_ID_COMPOSE,       ///< #ModuleCompose,   @ref lib_compose
  MODULE_ID_COMPRESS,      ///< #ModuleCompress,  @ref lib_compress
  MODULE_ID_CONFIG,        ///< #ModuleConfig,    @ref lib_config
  MODULE_ID_CONN,          ///< #ModuleConn,      @ref lib_conn
  MODULE_ID_CONVERT,       ///< #ModuleConvert,   @ref lib_convert
  MODULE_ID_CORE,          ///< #ModuleCore,      @ref lib_core
  MODULE_ID_EDITOR,        ///< #ModuleEditor,    @ref lib_editor
  MODULE_ID_EMAIL,         ///< #ModuleEmail,     @ref lib_email
  MODULE_ID_ENVELOPE,      ///< #ModuleEnvelope,  @ref lib_envelope
  MODULE_ID_EXPANDO,       ///< #ModuleExpando,   @ref lib_expando
  MODULE_ID_HCACHE,        ///< #ModuleHcache,    @ref lib_hcache
  MODULE_ID_HELPBAR,       ///< #ModuleHelpbar,   @ref lib_helpbar
  MODULE_ID_HISTORY,       ///< #ModuleHistory,   @ref lib_history
  MODULE_ID_HOOKS,         ///< #ModuleHooks,     @ref lib_hooks
  MODULE_ID_IMAP,          ///< #ModuleImap,      @ref lib_imap
  MODULE_ID_INDEX,         ///< #ModuleIndex,     @ref lib_index
  MODULE_ID_KEY,           ///< #ModuleKey,       @ref lib_key
  MODULE_ID_LUA,           ///< #ModuleLua,       @ref lib_lua
  MODULE_ID_MAILDIR,       ///< #ModuleMaildir,   @ref lib_maildir
  MODULE_ID_MBOX,          ///< #ModuleMbox,      @ref lib_mbox
  MODULE_ID_MENU,          ///< #ModuleMenu,      @ref lib_menu
  MODULE_ID_MH,            ///< #ModuleMh,        @ref lib_mh
  MODULE_ID_MUTT,          ///< #ModuleMutt,      @ref lib_mutt
  MODULE_ID_NCRYPT,        ///< #ModuleNcrypt,    @ref lib_ncrypt
  MODULE_ID_NNTP,          ///< #ModuleNntp,      @ref lib_nntp
  MODULE_ID_NOTMUCH,       ///< #ModuleNotmuch,   @ref lib_notmuch
  MODULE_ID_PAGER,         ///< #ModulePager,     @ref lib_pager
  MODULE_ID_PARSE,         ///< #ModuleParse,     @ref lib_parse
  MODULE_ID_PATTERN,       ///< #ModulePattern,   @ref lib_pattern
  MODULE_ID_POP,           ///< #ModulePop,       @ref lib_pop
  MODULE_ID_POSTPONE,      ///< #ModulePostpone,  @ref lib_postpone
  MODULE_ID_PROGRESS,      ///< #ModuleProgress,  @ref lib_progress
  MODULE_ID_QUESTION,      ///< #ModuleQuestion,  @ref lib_question
  MODULE_ID_SEND,          ///< #ModuleSend,      @ref lib_send
  MODULE_ID_SIDEBAR,       ///< #ModuleSidebar,   @ref lib_sidebar
  MODULE_ID_STORE,         ///< #ModuleStore,     @ref lib_store
  MODULE_ID_MAX,
};

/**
 * @defgroup module_api NeoMutt library API
 *
 * Allow libraries to initialise themselves.
 */
struct Module
{
  const char *name;                   ///< Name of the library module

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

#endif /* MUTT_CORE_MODULE_H */
