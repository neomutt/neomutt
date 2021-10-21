/**
 * @file
 * Index functions
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_INDEX_FUNCTIONS_H
#define MUTT_INDEX_FUNCTIONS_H

struct IndexPrivateData;
struct IndexSharedData;
struct MuttWindow;

/**
 * enum IndexRetval - Possible return values for Index functions
 */
enum IndexRetval
{
  IR_UNKNOWN   = -7, ///< Unknown key
  IR_CONTINUE  = -6, ///< Return to the Pager
  IR_DONE      = -5, ///< Exit the Index
  IR_NOT_IMPL  = -4, ///< Invalid function - feature not enabled
  IR_NO_ACTION = -3, ///< Valid function - no action performed
  IR_ERROR     = -2, ///< Valid function - error occurred
  IR_SUCCESS   = -1, ///< Valid function - successfully performed
};

/**
 * @defgroup index_function_api Index Function API
 *
 * Prototype for an Index Function
 *
 * @param shared Shared Index data
 * @param priv   Private Index data
 * @param op     Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval num #IndexRetval or opcode, e.g. OP_JUMP
 */
typedef int (*index_function_t)(struct IndexSharedData *shared, struct IndexPrivateData *priv, int op);

/**
 * struct IndexFunction - A NeoMutt function
 */
struct IndexFunction
{
  int op;                    ///< Op code, e.g. OP_MAIN_LIMIT
  index_function_t function; ///< Function to call
  int flags;                 ///< Prerequisites for the function, e.g. #CHECK_IN_MAILBOX
};

int index_function_dispatcher(struct MuttWindow *win_index, int op);

extern struct IndexFunction IndexFunctions[];

#endif /* MUTT_INDEX_FUNCTIONS_H */
