/**
 * @file
 * Smime functions
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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
 * @page smime_functions Smime functions
 *
 * Smime functions
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "smime_functions.h"
#include "menu/lib.h"
#include "question/lib.h"
#include "mutt_logging.h"
#include "smime.h"

/**
 * op_exit - Exit this menu - Implements ::smime_function_t - @ingroup smime_function_api
 */
static int op_exit(struct SmimeData *sd, int op)
{
  sd->done = true;
  return FR_SUCCESS;
}

/**
 * op_generic_select_entry - Select the current entry - Implements ::smime_function_t - @ingroup smime_function_api
 */
static int op_generic_select_entry(struct SmimeData *sd, int op)
{
  const int index = menu_get_index(sd->menu);
  struct SmimeKey *cur_key = sd->table[index];
  if (cur_key->trust != 't')
  {
    const char *s = "";
    switch (cur_key->trust)
    {
      case 'e':
      case 'i':
      case 'r':
        s = _("ID is expired/disabled/revoked. Do you really want to use the key?");
        break;
      case 'u':
        s = _("ID has undefined validity. Do you really want to use the key?");
        break;
      case 'v':
        s = _("ID is not trusted. Do you really want to use the key?");
        break;
    }

    char buf[1024] = { 0 };
    snprintf(buf, sizeof(buf), "%s", s);

    if (query_yesorno(buf, MUTT_NO) != MUTT_YES)
    {
      mutt_clear_error();
      return FR_NO_ACTION;
    }
  }

  sd->key = cur_key;
  sd->done = true;
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * SmimeFunctions - All the NeoMutt functions that the Smime supports
 */
static const struct SmimeFunction SmimeFunctions[] = {
  // clang-format off
  { OP_EXIT,                   op_exit },
  { OP_GENERIC_SELECT_ENTRY,   op_generic_select_entry },
  { 0, NULL },
  // clang-format on
};

/**
 * smime_function_dispatcher - Perform a Smime function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int smime_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg)
    return FR_ERROR;

  struct SmimeData *sd = dlg->wdata;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; SmimeFunctions[i].op != OP_NULL; i++)
  {
    const struct SmimeFunction *fn = &SmimeFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(sd, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
