/**
 * @file
 * Definition of the Pattern Module
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
 * @page pattern_module Definition of the Pattern Module
 *
 * Definition of the Pattern Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "private.h"
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef PatternVars[];

/**
 * pattern_init - Initialise a Module - Implements Module::init()
 */
static bool pattern_init(struct NeoMutt *n)
{
  struct PatternModuleData *mod_data = MUTT_MEM_CALLOC(1, struct PatternModuleData);

  // clang-format off
  mod_data->range_regexes[RANGE_K_REL]  = (struct RangeRegex){ RANGE_REL_RX,  1, 3, 0, { 0 } };
  mod_data->range_regexes[RANGE_K_ABS]  = (struct RangeRegex){ RANGE_ABS_RX,  1, 3, 0, { 0 } };
  mod_data->range_regexes[RANGE_K_LT]   = (struct RangeRegex){ RANGE_LT_RX,   1, 2, 0, { 0 } };
  mod_data->range_regexes[RANGE_K_GT]   = (struct RangeRegex){ RANGE_GT_RX,   2, 1, 0, { 0 } };
  mod_data->range_regexes[RANGE_K_BARE] = (struct RangeRegex){ RANGE_BARE_RX, 1, 1, 0, { 0 } };
  // clang-format on

  neomutt_set_module_data(n, MODULE_ID_PATTERN, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * pattern_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool pattern_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, PatternVars);
}

/**
 * pattern_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool pattern_cleanup(struct NeoMutt *n, void *data)
{
  struct PatternModuleData *mod_data = data;

  notify_free(&mod_data->notify);

  for (int i = 0; i < RANGE_K_INVALID; i++)
  {
    if (mod_data->range_regexes[i].ready)
      regfree(&mod_data->range_regexes[i].cooked);
  }

  FREE(&mod_data);
  return true;
}

/**
 * ModulePattern - Module for the Pattern library
 */
const struct Module ModulePattern = {
  MODULE_ID_PATTERN,
  "pattern",
  pattern_init,
  NULL, // config_define_types
  pattern_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  pattern_cleanup,
};
