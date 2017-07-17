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
#include "crypt_mod.h"
#include "lib.h"

/**
 * struct CryptModule - A crypto plugin module
 *
 * A type of a variable to keep track of registered crypto modules.
 */
struct CryptModule
{
  crypt_module_specs_t specs;
  struct CryptModule *next, **prevp;
};

static struct CryptModule *modules;

/**
 * crypto_module_register - Register a new crypto module
 */
void crypto_module_register(crypt_module_specs_t specs)
{
  struct CryptModule *module_new = safe_malloc(sizeof(*module_new));

  module_new->specs = specs;
  module_new->next = modules;
  if (modules)
    modules->prevp = &module_new->next;
  modules = module_new;
}

/**
 * crypto_module_lookup - Lookup a crypto module by name
 *
 * Return the crypto module specs for IDENTIFIER.
 * This function is usually used via the CRYPT_MOD_CALL[_CHECK] macros.
 */
crypt_module_specs_t crypto_module_lookup(int identifier)
{
  struct CryptModule *module = modules;

  while (module && (module->specs->identifier != identifier))
    module = module->next;

  return module ? module->specs : NULL;
}
