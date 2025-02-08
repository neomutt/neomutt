/**
 * @file
 * Definition of the Ncrypt Module
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
 * @page ncrypt_module Definition of the Ncrypt Module
 *
 * Definition of the Ncrypt Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"
#include "lib.h"

extern struct ConfigDef NcryptVars[];
extern struct ConfigDef NcryptVarsGpgme[];
extern struct ConfigDef NcryptVarsPgp[];
extern struct ConfigDef NcryptVarsSmime[];

/**
 * ncrypt_init - Initialise a Module - Implements Module::init()
 */
static bool ncrypt_init(struct NeoMutt *n)
{
  return true;
}

/**
 * ncrypt_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool ncrypt_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = true;

  rc &= cs_register_variables(cs, NcryptVars);

#if defined(CRYPT_BACKEND_GPGME)
  rc &= cs_register_variables(cs, NcryptVarsGpgme);
#endif

#if defined(CRYPT_BACKEND_CLASSIC_PGP)
  rc &= cs_register_variables(cs, NcryptVarsPgp);
#endif

#if defined(CRYPT_BACKEND_CLASSIC_SMIME)
  rc &= cs_register_variables(cs, NcryptVarsSmime);
#endif

  return rc;
}

/**
 * ncrypt_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static void ncrypt_cleanup(struct NeoMutt *n)
{
  crypt_cleanup();
  crypto_module_cleanup();
}

/**
 * ModuleNcrypt - Module for the Ncrypt library
 */
const struct Module ModuleNcrypt = {
  "ncrypt",
  ncrypt_init,
  NULL, // config_define_types
  ncrypt_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  ncrypt_cleanup,
  NULL, // mod_data
};
