/**
 * @file
 * Attachment functions
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

#ifndef MUTT_ATTACH_FUNCTIONS_H
#define MUTT_ATTACH_FUNCTIONS_H

struct KeyEvent;
struct MuttWindow;

/**
 * struct AttachFunctionData - Data passed to Attach worker functions
 */
struct AttachFunctionData
{
  struct NeoMutt *n; ///< NeoMutt application data
  struct AttachPrivateData *priv; ///< Attach private data
};

/**
 * @defgroup attach_function_api Attach Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Attach Function
 *
 * @param fdata   Attach Function context data
 * @param event Event to process
 * @retval enum #FunctionRetval
 *
 * @pre fdata   is not NULL
 * @pre event is not NULL
 */
typedef int (*attach_function_t)(struct AttachFunctionData *fdata, const struct KeyEvent *event);

/**
 * struct AttachFunction - A NeoMutt function
 */
struct AttachFunction
{
  int op;                     ///< Op code, e.g. OP_ATTACH_COLLAPSE
  attach_function_t function; ///< Function to call
};

int attach_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event);

#endif /* MUTT_ATTACH_FUNCTIONS_H */
