/**
 * @file
 * Definition of the Mutt Module
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
 * @page mutt_module Definition of the Mutt Module
 *
 * Definition of the Mutt Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "core/lib.h"
#include "memory.h"
#include "module_data.h"
#include "notify.h"
#include "signal2.h"

/**
 * mutt_init - Initialise a Module - Implements Module::init()
 */
static bool mutt_init(struct NeoMutt *n)
{
  struct MuttModuleData *mod_data = MUTT_MEM_CALLOC(1, struct MuttModuleData);
  neomutt_set_module_data(n, MODULE_ID_MUTT, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * mutt_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool mutt_cleanup(struct NeoMutt *n, void *data)
{
  struct MuttModuleData *mod_data = data;

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleMutt - Module for the Mutt library
 */
const struct Module ModuleMutt = {
  MODULE_ID_MUTT, "mutt", mutt_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  mutt_cleanup,
};
