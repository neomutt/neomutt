/**
 * @file
 * Definition of the Nntp Module
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
 * @page nntp_module Definition of the Nntp Module
 *
 * Definition of the Nntp Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef NntpVars[];

/**
 * nntp_init - Initialise a Module - Implements Module::init()
 */
static bool nntp_init(struct NeoMutt *n)
{
  struct NntpModuleData *mod_data = MUTT_MEM_CALLOC(1, struct NntpModuleData);
  neomutt_set_module_data(n, MODULE_ID_NNTP, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * nntp_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool nntp_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, NntpVars);
}

/**
 * nntp_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool nntp_cleanup(struct NeoMutt *n)
{
  struct NntpModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_NNTP);
  ASSERT(mod_data);

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleNntp - Module for the Nntp library
 */
const struct Module ModuleNntp = {
  MODULE_ID_NNTP,
  "nntp",
  nntp_init,
  NULL, // config_define_types
  nntp_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  nntp_cleanup,
};
