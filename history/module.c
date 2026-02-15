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
  // struct HistoryModuleData *md = MUTT_MEM_CALLOC(1, struct HistoryModuleData);
  // neomutt_set_module_data(n, MODULE_ID_HISTORY, md);

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
  // struct HistoryModuleData *md = neomutt_get_module_data(n, MODULE_ID_HISTORY);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * history_gui_cleanup - Clean up the GUI - Implements Module::gui_cleanup()
 */
void history_gui_cleanup(struct NeoMutt *n)
{
  mutt_hist_cleanup();
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
  NULL, // gui_init
  history_gui_cleanup,
  history_cleanup,
};
