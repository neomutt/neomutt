/**
 * @file
 * Mailing-list functions
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MLIST_FUNCTIONS_H
#define MUTT_MLIST_FUNCTIONS_H

#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "email/lib.h"

struct KeyEvent;
struct Mailbox;
struct Menu;
struct MuttWindow;

/**
 * struct ListAction - A mailing-list action in the dialog
 */
struct ListAction
{
  const char *name; ///< Label for the action
  int op;           ///< Op code
  size_t offset;    ///< Offset into struct Rfc2369ListHeaders
};

/**
 * struct ListEntry - A mailing-list action in the dialog
 */
struct ListEntry
{
  const struct ListAction *action; ///< Action definition
  const char *value;               ///< URI to use
};
ARRAY_HEAD(ListEntryArray, struct ListEntry);

/// Mailing-list actions shown in the dialog
extern const struct ListAction ListActions[];

/// Number of entries in #ListActions
extern const int ListActionsCount;

/**
 * struct ListData - Private data for the Mailing-list action dialog
 */
struct ListData
{
  struct Menu *menu;                 ///< Dialog menu
  struct Mailbox *mailbox;           ///< Source mailbox
  struct Rfc2369ListHeaders headers; ///< Parsed List-* headers
  struct ListEntryArray entries;     ///< Menu rows
  int label_width;                   ///< Width of the longest action label
  bool done;                         ///< Exit the dialog
};

/**
 * @defgroup mlist_function_api Mailing-list Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Mailing-list Function
 *
 * @param ld    Mailing-list dialog state
 * @param event Event to process
 * @retval enum #FunctionRetval
 *
 * @pre ld    is not NULL
 * @pre event is not NULL
 */
typedef int (*mlist_function_t)(struct ListData *ld, const struct KeyEvent *event);

/**
 * struct MlistFunction - A list dialog function
 */
struct MlistFunction
{
  int op;                    ///< Op code
  mlist_function_t function; ///< Function to call
};

struct ListHead *mlist_action_value(struct Rfc2369ListHeaders *headers, const struct ListAction *action);
int              mlist_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event);
bool             mlist_open_url(const char *url);

#endif /* MUTT_MLIST_FUNCTIONS_H */
