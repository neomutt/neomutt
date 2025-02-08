/**
 * @file
 * Definition of the Hcache Module
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page hc_module Definition of the Hcache Module
 *
 * Definition of the Hcache Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

extern struct ConfigDef HcacheVars[];
extern struct ConfigDef HcacheVarsComp[];
extern struct ConfigDef HcacheVarsComp2[];
extern struct ConfigDef HcacheVarsPage[];

/**
 * hcache_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool hcache_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = true;

#if defined(USE_HCACHE)
  rc &= cs_register_variables(cs, HcacheVars);
#endif

#if defined(USE_HCACHE_COMPRESSION)
  rc &= cs_register_variables(cs, HcacheVarsComp);
#endif

#if defined(HAVE_QDBM) && defined(HAVE_TC) && defined(HAVE_KC)
  rc &= cs_register_variables(cs, HcacheVarsComp2);
#endif

#if defined(HAVE_GDBM) && defined(HAVE_BDB)
  rc &= cs_register_variables(cs, HcacheVarsPage);
#endif

  return rc;
}

/**
 * ModuleHcache - Module for the Hcache library
 */
const struct Module ModuleHcache = {
  "hcache",
  NULL, // init
  NULL, // config_define_types
  hcache_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
  NULL, // mod_data
};
