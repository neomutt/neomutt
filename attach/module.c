/**
 * @file
 * Definition of the Attach Module
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
 * @page attach_module Definition of the Attach Module
 *
 * Definition of the Attach Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "commands.h"
#include "module_data.h"
#include "mutt_attach.h"

extern struct ConfigDef AttachVars[];

extern const struct Command AttachCommands[];

/**
 * attach_init - Initialise a Module - Implements Module::init()
 */
static bool attach_init(struct NeoMutt *n)
{
  struct AttachModuleData *mod_data = MUTT_MEM_CALLOC(1, struct AttachModuleData);
  neomutt_set_module_data(n, MODULE_ID_ATTACH, mod_data);

  mod_data->notify = notify_new();
  notify_set_parent(mod_data->notify, n->notify);

  STAILQ_INIT(&mod_data->attach_allow);
  STAILQ_INIT(&mod_data->attach_exclude);
  STAILQ_INIT(&mod_data->inline_allow);
  STAILQ_INIT(&mod_data->inline_exclude);

  STAILQ_INIT(&mod_data->mime_lookup);
  STAILQ_INIT(&mod_data->temp_attachments);

  mod_data->attachments_notify = notify_new();
  notify_set_parent(mod_data->attachments_notify, NeoMutt->notify);

  return true;
}

/**
 * attach_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool attach_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, AttachVars);
}

/**
 * attach_commands_register - Register NeoMutt Commands - Implements Module::commands_register()
 */
static bool attach_commands_register(struct NeoMutt *n, struct CommandArray *ca)
{
  return commands_register(ca, AttachCommands);
}

/**
 * attach_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool attach_cleanup(struct NeoMutt *n)
{
  struct AttachModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_ATTACH);
  ASSERT(mod_data);

  notify_free(&mod_data->notify);
  notify_free(&mod_data->attachments_notify);

  /* Lists of AttachMatch */
  mutt_list_free_type(&mod_data->attach_allow, (list_free_t) attachmatch_free);
  mutt_list_free_type(&mod_data->attach_exclude, (list_free_t) attachmatch_free);
  mutt_list_free_type(&mod_data->inline_allow, (list_free_t) attachmatch_free);
  mutt_list_free_type(&mod_data->inline_exclude, (list_free_t) attachmatch_free);

  mutt_list_free(&mod_data->mime_lookup);

  mutt_temp_attachments_cleanup(&mod_data->temp_attachments);

  FREE(&mod_data);
  return true;
}

/**
 * ModuleAttach - Module for the Attach library
 */
const struct Module ModuleAttach = {
  MODULE_ID_ATTACH,
  "attach",
  attach_init,
  NULL, // config_define_types
  attach_config_define_variables,
  attach_commands_register,
  NULL, // gui_init
  NULL, // gui_cleanup
  attach_cleanup,
};
