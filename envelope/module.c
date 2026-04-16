/**
 * @file
 * Definition of the Envelope Module
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
 * @page envelope_module Definition of the Envelope Module
 *
 * Definition of the Envelope Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "module_data.h"

/**
 * envelope_init - Initialise a Module - Implements Module::init()
 */
static bool envelope_init(struct NeoMutt *n)
{
  struct EnvelopeModuleData *mod_data = MUTT_MEM_CALLOC(1, struct EnvelopeModuleData);
  neomutt_set_module_data(n, MODULE_ID_ENVELOPE, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  return true;
}

/**
 * envelope_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool envelope_cleanup(struct NeoMutt *n)
{
  struct EnvelopeModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_ENVELOPE);
  ASSERT(mod_data);

  notify_free(&mod_data->notify);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleEnvelope - Module for the Envelope library
 */
const struct Module ModuleEnvelope = {
  MODULE_ID_ENVELOPE,
  "envelope",
  envelope_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  envelope_cleanup,
};
