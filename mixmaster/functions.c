/**
 * @file
 * Mixmaster functions
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
 * @page mixmaster_functions Mixmaster functions
 *
 * Mixmaster functions
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "opcodes.h"
#include "private_data.h"
#include "win_chain.h"
#include "win_hosts.h"

/**
 * op_exit - exit this menu - Implements ::mixmaster_function_t - @ingroup mixmaster_function_api
 */
static int op_exit(struct MixmasterPrivateData *priv, int op)
{
  return FR_NO_ACTION;
}

/**
 * op_mix_append - append a remailer to the chain - Implements ::mixmaster_function_t - @ingroup mixmaster_function_api
 */
static int op_mix_append(struct MixmasterPrivateData *priv, int op)
{
  if (!win_chain_append(priv->win_chain, win_hosts_get_selection(priv->win_hosts)))
    return FR_ERROR;

  return FR_SUCCESS;
}

/**
 * op_mix_chain_next - select the next element of the chain - Implements ::mixmaster_function_t - @ingroup mixmaster_function_api
 */
static int op_mix_chain_next(struct MixmasterPrivateData *priv, int op)
{
  if (win_chain_next(priv->win_chain))
    return FR_SUCCESS;

  return FR_ERROR;
}

/**
 * op_mix_chain_prev - select the previous element of the chain - Implements ::mixmaster_function_t - @ingroup mixmaster_function_api
 */
static int op_mix_chain_prev(struct MixmasterPrivateData *priv, int op)
{
  if (win_chain_prev(priv->win_chain))
    return FR_SUCCESS;

  return FR_ERROR;
}

/**
 * op_mix_delete - delete a remailer from the chain - Implements ::mixmaster_function_t - @ingroup mixmaster_function_api
 */
static int op_mix_delete(struct MixmasterPrivateData *priv, int op)
{
  if (!win_chain_delete(priv->win_chain))
    return FR_ERROR;

  return FR_SUCCESS;
}

/**
 * op_mix_insert - insert a remailer into the chain - Implements ::mixmaster_function_t - @ingroup mixmaster_function_api
 */
static int op_mix_insert(struct MixmasterPrivateData *priv, int op)
{
  if (!win_chain_insert(priv->win_chain, win_hosts_get_selection(priv->win_hosts)))
    return FR_ERROR;

  return FR_SUCCESS;
}

/**
 * op_mix_use - accept the chain constructed - Implements ::mixmaster_function_t - @ingroup mixmaster_function_api
 */
static int op_mix_use(struct MixmasterPrivateData *priv, int op)
{
  // If we don't have a chain yet, insert the current item
  if (win_chain_get_length(priv->win_chain) == 0)
    op_mix_insert(priv, op);

  if (win_chain_validate(priv->win_chain))
    return FR_DONE;

  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * MixmasterFunctions - All the NeoMutt functions that the Mixmaster supports
 */
static const struct MixmasterFunction MixmasterFunctions[] = {
  // clang-format off
  { OP_EXIT,                 op_exit           },
  { OP_GENERIC_SELECT_ENTRY, op_mix_append     },
  { OP_MIX_APPEND,           op_mix_append     },
  { OP_MIX_CHAIN_NEXT,       op_mix_chain_next },
  { OP_MIX_CHAIN_PREV,       op_mix_chain_prev },
  { OP_MIX_DELETE,           op_mix_delete     },
  { OP_MIX_INSERT,           op_mix_insert     },
  { OP_MIX_USE,              op_mix_use        },
  { 0, NULL },
  // clang-format on
};

/**
 * mix_function_dispatcher - Perform a Mixmaster function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int mix_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct MixmasterPrivateData *priv = win->wdata;
  int rc = FR_UNKNOWN;
  for (size_t i = 0; MixmasterFunctions[i].op != OP_NULL; i++)
  {
    const struct MixmasterFunction *fn = &MixmasterFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(priv, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
