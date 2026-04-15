/**
 * @file
 * Definition of the Hcache Module
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
 * @page hcache_module Definition of the Hcache Module
 *
 * Definition of the Hcache Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef HcacheVars[];
extern struct ConfigDef HcacheVarsComp[];

/**
 * hcache_init - Initialise a Module - Implements Module::init()
 */
static bool hcache_init(struct NeoMutt *n)
{
  struct HcacheModuleData *mod_data = MUTT_MEM_CALLOC(1, struct HcacheModuleData);
  neomutt_set_module_data(n, MODULE_ID_HCACHE, mod_data);

  return true;
}

/**
 * hcache_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool hcache_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = true;

  rc &= cs_register_variables(cs, HcacheVars);

#if defined(USE_HCACHE_COMPRESSION)
  rc &= cs_register_variables(cs, HcacheVarsComp);
#endif

  return rc;
}

/**
 * hcache_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool hcache_cleanup(struct NeoMutt *n)
{
  struct HcacheModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_HCACHE);
  ASSERT(mod_data);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleHcache - Module for the Hcache library
 */
const struct Module ModuleHcache = {
  MODULE_ID_HCACHE,
  "hcache",
  hcache_init,
  NULL, // config_define_types
  hcache_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  hcache_cleanup,
};
