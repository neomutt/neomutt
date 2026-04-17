/**
 * @file
 * Register crypto modules
 *
 * @authors
 * Copyright (C) 2017-2018 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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
 * @page ncrypt_crypt_mod Register crypto modules
 *
 * Register crypto modules
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "crypt_mod.h"
#include "lib.h"
#include "module_data.h"

/**
 * crypto_module_register - Register a new crypto module
 * @param specs API functions
 */
void crypto_module_register(const struct CryptModuleSpecs *specs)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  struct CryptModule *module = MUTT_MEM_CALLOC(1, struct CryptModule);
  module->specs = specs;
  STAILQ_INSERT_HEAD(&mod_data->crypt_modules, module, entries);
}

/**
 * crypto_module_lookup - Lookup a crypto module by name
 * @param identifier Name, e.g. #APPLICATION_PGP
 * @retval ptr Crypto module
 *
 * This function is usually used via the CRYPT_MOD_CALL[_CHECK] macros.
 */
const struct CryptModuleSpecs *crypto_module_lookup(int identifier)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  const struct CryptModule *module = NULL;
  STAILQ_FOREACH(module, &mod_data->crypt_modules, entries)
  {
    if (module->specs->identifier == identifier)
    {
      return module->specs;
    }
  }
  return NULL;
}

/**
 * crypto_module_cleanup - Clean up the crypto modules
 */
void crypto_module_cleanup(void)
{
  struct NcryptModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_NCRYPT);
  struct CryptModule *np = NULL, *tmp = NULL;
  STAILQ_FOREACH_SAFE(np, &mod_data->crypt_modules, entries, tmp)
  {
    STAILQ_REMOVE(&mod_data->crypt_modules, np, CryptModule, entries);
    FREE(&np);
  }
}
