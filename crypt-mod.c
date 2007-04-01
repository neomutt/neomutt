/*
 * Copyright (C) 2004 g10 Code GmbH
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "crypt-mod.h"

/* A type an a variable to keep track of registered crypto modules. */
typedef struct crypt_module *crypt_module_t;

struct crypt_module
{
  crypt_module_specs_t specs;
  crypt_module_t next, *prevp;
};

static crypt_module_t modules;

/* Register a new crypto module. */
void crypto_module_register (crypt_module_specs_t specs)
{
  crypt_module_t module_new = safe_malloc (sizeof (*module_new));

  module_new->specs = specs;
  module_new->next = modules;
  if (modules)
    modules->prevp = &module_new->next;
  modules = module_new;
}

/* Return the crypto module specs for IDENTIFIER.  This function is
   usually used via the CRYPT_MOD_CALL[_CHECK] macros. */
crypt_module_specs_t crypto_module_lookup (int identifier)
{
  crypt_module_t module = modules;

  while (module && (module->specs->identifier != identifier))
    module = module->next;

  return module ? module->specs : NULL;
}
