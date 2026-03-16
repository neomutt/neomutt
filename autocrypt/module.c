/**
 * @file
 * Definition of the Autocrypt Module
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
 * @page autocrypt_module Definition of the Autocrypt Module
 *
 * Definition of the Autocrypt Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "module_data.h"

extern struct ConfigDef AutocryptVars[];

/**
 * autocrypt_init - Initialise a Module - Implements Module::init()
 */
static bool autocrypt_init(struct NeoMutt *n)
{
  // struct AutocryptModuleData *md = MUTT_MEM_CALLOC(1, struct AutocryptModuleData);
  // neomutt_set_module_data(n, MODULE_ID_AUTOCRYPT, md);

  return true;
}

/**
 * autocrypt_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool autocrypt_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = false;

#if defined(USE_AUTOCRYPT)
  rc |= cs_register_variables(cs, AutocryptVars);
#endif

  return rc;
}

/**
 * autocrypt_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool autocrypt_cleanup(struct NeoMutt *n)
{
  // struct AutocryptModuleData *md = neomutt_get_module_data(n, MODULE_ID_AUTOCRYPT);
  // ASSERT(md);

  // FREE(&md);
  return true;
}

/**
 * ModuleAutocrypt - Module for the Autocrypt library
 */
const struct Module ModuleAutocrypt = {
  MODULE_ID_AUTOCRYPT,
  "autocrypt",
  autocrypt_init,
  NULL, // config_define_types
  autocrypt_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  autocrypt_cleanup,
};
