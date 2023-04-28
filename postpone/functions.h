/**
 * @file
 * Postponed Email Functions
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

#ifndef MUTT_POSTPONE_FUNCTIONS_H
#define MUTT_POSTPONE_FUNCTIONS_H

#include <stdbool.h>

struct MuttWindow;

/**
 * struct PostponeData - Data to pass to the Postpone Functions
 */
struct PostponeData
{
  bool done;                ///< Should we close the Dialog?
  struct Mailbox *mailbox;  ///< Postponed Mailbox
  struct Menu *menu;        ///< Postponed Menu
  struct Email *email;      ///< Selected Email
};

/**
 * @defgroup postpone_function_api Postpone Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Postpone Function
 *
 * @param menu   Menu
 * @param op     Operation to perform, e.g. OP_DELETE
 * @retval enum #FunctionRetval
 */
typedef int (*postpone_function_t)(struct PostponeData *pd, int op);

/**
 * struct PostponeFunction - A NeoMutt function
 */
struct PostponeFunction
{
  int op;                      ///< Op code, e.g. OP_DELETE
  postpone_function_t function; ///< Function to call
};

int postpone_function_dispatcher(struct MuttWindow *win, int op);

#endif /* MUTT_POSTPONE_FUNCTIONS_H */
