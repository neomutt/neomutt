/**
 * @file
 * Definition of the History Module
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
 * @page history_module Definition of the History Module
 *
 * Definition of the History Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"
#include "module_data.h"

extern struct ConfigDef HistoryVars[];

/**
 * history_init - Initialise a Module - Implements Module::init()
 */
static bool history_init(struct NeoMutt *n)
{
  struct HistoryModuleData *mod_data = MUTT_MEM_CALLOC(1, struct HistoryModuleData);
  neomutt_set_module_data(n, MODULE_ID_HISTORY, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * history_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool history_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, HistoryVars);
}

/**
 * history_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool history_cleanup(struct NeoMutt *n)
{
  struct HistoryModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_HISTORY);
  ASSERT(mod_data);

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * history_gui_init - Initialise the GUI - Implements Module::gui_init()
 */
static bool history_gui_init(struct NeoMutt *n)
{
  struct HistoryModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_HISTORY);
  mutt_hist_init(mod_data);
  mutt_hist_read_file();
  return true;
}

/**
 * history_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
static void history_gui_cleanup(struct NeoMutt *n)
{
  struct HistoryModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_HISTORY);
  mutt_hist_cleanup(mod_data);
}

/**
 * ModuleHistory - Module for the History library
 */
const struct Module ModuleHistory = {
  MODULE_ID_HISTORY,
  "history",
  history_init,
  NULL, // config_define_types
  history_config_define_variables,
  NULL, // commands_register
  history_gui_init,
  history_gui_cleanup,
  history_cleanup,
};
