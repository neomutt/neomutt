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

struct AttachPrivateData;
struct MuttWindow;

/**
 * @defgroup attach_function_api Attachment Function API
 * @ingroup dispatcher_api
 *
 * Prototype for an Attachment Function
 *
 * @param priv   Private Attach data
 * @param op     Operation to perform, e.g. OP_ATTACHMENT_COLLAPSE
 * @retval enum #FunctionRetval
 */
typedef int (*attach_function_t)(struct AttachPrivateData *priv, int op);

/**
 * struct AttachFunction - A NeoMutt function
 */
struct AttachFunction
{
  int op;                     ///< Op code, e.g. OP_ATTACHMENT_COLLAPSE
  attach_function_t function; ///< Function to call
};

int attach_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_ATTACH_FUNCTIONS_H */
