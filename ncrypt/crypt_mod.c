/**
 * @file
 * Register crypto modules
 *
 * @authors
 * Copyright (C) 2004 g10 Code GmbH
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

#include "config.h"
#include "lib/lib.h"
#include "crypt_mod.h"
#include "queue.h"

/**
 * struct CryptModule - A crypto plugin module
 *
 * A type of a variable to keep track of registered crypto modules.
 */

STAILQ_HEAD(CryptModules, CryptModule)
modules = STAILQ_HEAD_INITIALIZER(modules);
struct CryptModule
{
  struct CryptModuleSpecs *specs;
  STAILQ_ENTRY(CryptModule) entries;
};

/**
 * crypto_module_register - Register a new crypto module
 */
void crypto_module_register(struct CryptModuleSpecs *specs)
{
  struct CryptModule *module = safe_calloc(1, sizeof(struct CryptModule));
  module->specs = specs;
  STAILQ_INSERT_HEAD(&modules, module, entries);
}

/**
 * crypto_module_lookup - Lookup a crypto module by name
 *
 * Return the crypto module specs for IDENTIFIER.
 * This function is usually used via the CRYPT_MOD_CALL[_CHECK] macros.
 */
struct CryptModuleSpecs *crypto_module_lookup(int identifier)
{
  struct CryptModule *module = NULL;
  STAILQ_FOREACH(module, &modules, entries)
  {
    if (module->specs->identifier == identifier)
    {
      return module->specs;
    }
  }
  return NULL;
}
