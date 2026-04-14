/**
 * @file
 * Definition of the Editor Module
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
 * @page editor_module Definition of the Editor Module
 *
 * Definition of the Editor Module
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "module_data.h"

/**
 * editor_init - Initialise a Module - Implements Module::init()
 */
static bool editor_init(struct NeoMutt *n)
{
  struct EditorModuleData *md = MUTT_MEM_CALLOC(1, struct EditorModuleData);
  neomutt_set_module_data(n, MODULE_ID_EDITOR, md);

  return true;
}

/**
 * editor_cleanup - Clean up a Module - Implements Module::cleanup()
 */
static bool editor_cleanup(struct NeoMutt *n)
{
  struct EditorModuleData *md = neomutt_get_module_data(n, MODULE_ID_EDITOR);
  ASSERT(md);

  FREE(&md);
  return true;
}

/**
 * ModuleEditor - Module for the Editor library
 */
const struct Module ModuleEditor = {
  MODULE_ID_EDITOR,
  "editor",
  editor_init,
  NULL, // config_define_types
  NULL, // config_define_variables
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  editor_cleanup,
};
