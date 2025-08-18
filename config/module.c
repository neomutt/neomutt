/**
 * @file
 * Definition of the Config Module
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
 * @page config_module Definition of the Config Module
 *
 * Definition of the Config Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "core/lib.h"
#include "set.h"

extern const struct ConfigSetType CstBool;
extern const struct ConfigSetType CstEnum;
extern const struct ConfigSetType CstLong;
extern const struct ConfigSetType CstMbtable;
extern const struct ConfigSetType CstMyVar;
extern const struct ConfigSetType CstNumber;
extern const struct ConfigSetType CstPath;
extern const struct ConfigSetType CstQuad;
extern const struct ConfigSetType CstRegex;
extern const struct ConfigSetType CstSlist;
extern const struct ConfigSetType CstSort;
extern const struct ConfigSetType CstString;

/**
 * config_config_define_types - Set up Config Types - Implements Module::config_define_types()
 */
static bool config_config_define_types(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = true;

  rc &= cs_register_type(cs, &CstBool);
  rc &= cs_register_type(cs, &CstEnum);
  rc &= cs_register_type(cs, &CstLong);
  rc &= cs_register_type(cs, &CstMbtable);
  rc &= cs_register_type(cs, &CstMyVar);
  rc &= cs_register_type(cs, &CstNumber);
  rc &= cs_register_type(cs, &CstPath);
  rc &= cs_register_type(cs, &CstQuad);
  rc &= cs_register_type(cs, &CstRegex);
  rc &= cs_register_type(cs, &CstSlist);
  rc &= cs_register_type(cs, &CstSort);
  rc &= cs_register_type(cs, &CstString);

  return rc;
}

/**
 * ModuleConfig - Module for the Config library
 */
const struct Module ModuleConfig = {
  "config",
  NULL, // init
  config_config_define_types,
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
  NULL, // mod_data
};
