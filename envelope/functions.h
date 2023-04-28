/**
 * @file
 * Envelope functions
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

#ifndef MUTT_ENVELOPE_FUNCTIONS_H
#define MUTT_ENVELOPE_FUNCTIONS_H

struct EnvelopeWindowData;

/**
 * @defgroup envelope_function_api Envelope Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Envelope Function
 *
 * @param wdata Envelope Window data
 * @param op    Operation to perform, e.g. OP_ENVELOPE_EDIT_FROM
 * @retval enum #FunctionRetval
 */
typedef int (*envelope_function_t)(struct EnvelopeWindowData *wdata, int op);

/**
 * struct EnvelopeFunction - A NeoMutt Envelope function
 */
struct EnvelopeFunction
{
  int op;                       ///< Op code, e.g. OP_ENVELOPE_EDIT_FROM
  envelope_function_t function; ///< Function to call
};

void update_crypt_info(struct EnvelopeWindowData *wdata);

#endif /* MUTT_ENVELOPE_FUNCTIONS_H */
