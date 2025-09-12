/**
 * @file
 * Definition of the Maildir Module
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
 * @page maildir_module Definition of the Maildir Module
 *
 * Definition of the Maildir Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

extern struct ConfigDef MaildirVars[];
extern struct ConfigDef MaildirVarsHcache[];

/**
 * maildir_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool maildir_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  bool rc = cs_register_variables(cs, MaildirVars);

#if defined(USE_HCACHE)
  rc |= cs_register_variables(cs, MaildirVarsHcache);
#endif

  return rc;
}

/**
 * ModuleMaildir - Module for the Maildir library
 */
const struct Module ModuleMaildir = {
  "maildir",
  NULL, // init
  NULL, // config_define_types
  maildir_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
  NULL, // mod_data
};
