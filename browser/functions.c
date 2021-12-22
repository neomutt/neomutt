/**
 * @file
 * Browser functions
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

/**
 * @page browser_functions Browser functions
 *
 * Browser functions
 */

#include "config.h"
#include "gui/lib.h"
#include "functions.h"
#include "index/lib.h"
#include "opcodes.h"
#include "private_data.h"

static const char *Not_available_in_this_menu = N_("Not available in this menu");

/**
 * destroy_state - Free the BrowserState
 * @param state State to free
 *
 * Frees up the memory allocated for the local-global variables.
 */
void destroy_state(struct BrowserState *state)
{
  struct FolderFile *ff = NULL;
  ARRAY_FOREACH(ff, &state->entry)
  {
    FREE(&ff->name);
    FREE(&ff->desc);
  }
  ARRAY_FREE(&state->entry);

#ifdef USE_IMAP
  FREE(&state->folder);
#endif
}

/**
 * op_browser_new_file - Select a new file in this directory - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_browser_new_file(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

#if defined(USE_IMAP) || defined(USE_NNTP)
/**
 * op_browser_subscribe - Subscribe to current mbox (IMAP/NNTP only) - Implements ::browser_function_t - @ingroup browser_function_api
 *
 * This function handles:
 * - OP_BROWSER_SUBSCRIBE
 * - OP_BROWSER_UNSUBSCRIBE
 */
static int op_browser_subscribe(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

/**
 * op_browser_tell - Display the currently selected file's name - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_browser_tell(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

#ifdef USE_IMAP
/**
 * op_browser_toggle_lsub - Toggle view all/subscribed mailboxes (IMAP only) - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_browser_toggle_lsub(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

/**
 * op_browser_view_file - View file - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_browser_view_file(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

#ifdef USE_NNTP
/**
 * op_catchup - Mark all articles in newsgroup as read - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_catchup(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

/**
 * op_change_directory - Change directories - Implements ::browser_function_t - @ingroup browser_function_api
 *
 * This function handles:
 * - OP_GOTO_PARENT
 * - OP_CHANGE_DIRECTORY
 */
static int op_change_directory(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

#ifdef USE_IMAP
/**
 * op_create_mailbox - Create a new mailbox (IMAP only) - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_create_mailbox(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

#ifdef USE_IMAP
/**
 * op_delete_mailbox - Delete the current mailbox (IMAP only) - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_delete_mailbox(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

/**
 * op_enter_mask - Enter a file mask - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_enter_mask(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

/**
 * op_exit - Exit this menu - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_exit(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

/**
 * op_generic_select_entry - Select the current entry - Implements ::browser_function_t - @ingroup browser_function_api
 *
 * This function handles:
 * - OP_DESCEND_DIRECTORY
 * - OP_GENERIC_SELECT_ENTRY
 */
static int op_generic_select_entry(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

#ifdef USE_NNTP
/**
 * op_load_active - Load list of all newsgroups from NNTP server - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_load_active(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

/**
 * op_mailbox_list - List mailboxes with new mail - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_mailbox_list(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

#ifdef USE_IMAP
/**
 * op_rename_mailbox - Rename the current mailbox (IMAP only) - Implements ::browser_function_t - @ingroup browser_function_api
 */
static int op_rename_mailbox(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

/**
 * op_sort - Sort messages - Implements ::browser_function_t - @ingroup browser_function_api
 *
 * This function handles:
 * - OP_SORT
 * - OP_SORT_REVERSE
 */
static int op_sort(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

#ifdef USE_NNTP
/**
 * op_subscribe_pattern - Subscribe to newsgroups matching a pattern - Implements ::browser_function_t - @ingroup browser_function_api
 *
 * This function handles:
 * - OP_BROWSER_SUBSCRIBE
 * - OP_SUBSCRIBE_PATTERN
 * - OP_BROWSER_UNSUBSCRIBE
 * - OP_UNSUBSCRIBE_PATTERN
 */
static int op_subscribe_pattern(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}
#endif

/**
 * op_toggle_mailboxes - Toggle whether to browse mailboxes or all files - Implements ::browser_function_t - @ingroup browser_function_api
 *
 * This function handles:
 * - OP_CHECK_NEW
 * - OP_TOGGLE_MAILBOXES
 */
static int op_toggle_mailboxes(struct BrowserPrivateData *priv, int op)
{
  return FR_ERROR;
}

/**
 * BrowserFunctions - All the NeoMutt functions that the Browser supports
 */
struct BrowserFunction BrowserFunctions[] = {
  // clang-format off
  { OP_BROWSER_GOTO_FOLDER,  op_toggle_mailboxes },
  { OP_BROWSER_NEW_FILE,     op_browser_new_file },
#if defined(USE_IMAP) || defined(USE_NNTP)
  { OP_BROWSER_SUBSCRIBE,    op_browser_subscribe },
#endif
  { OP_BROWSER_TELL,         op_browser_tell },
#ifdef USE_IMAP
  { OP_BROWSER_TOGGLE_LSUB,  op_browser_toggle_lsub },
#endif
#if defined(USE_IMAP) || defined(USE_NNTP)
  { OP_BROWSER_UNSUBSCRIBE,  op_browser_subscribe },
#endif
  { OP_BROWSER_VIEW_FILE,    op_browser_view_file },
#ifdef USE_NNTP
  { OP_CATCHUP,              op_catchup },
#endif
  { OP_CHANGE_DIRECTORY,     op_change_directory },
  { OP_CHECK_NEW,            op_toggle_mailboxes },
#ifdef USE_IMAP
  { OP_CREATE_MAILBOX,       op_create_mailbox },
  { OP_DELETE_MAILBOX,       op_delete_mailbox },
#endif
  { OP_DESCEND_DIRECTORY,    op_generic_select_entry },
  { OP_ENTER_MASK,           op_enter_mask },
  { OP_EXIT,                 op_exit },
  { OP_GENERIC_SELECT_ENTRY, op_generic_select_entry },
  { OP_GOTO_PARENT,          op_change_directory },
#ifdef USE_NNTP
  { OP_LOAD_ACTIVE,          op_load_active },
#endif
  { OP_MAILBOX_LIST,         op_mailbox_list },
#ifdef USE_IMAP
  { OP_RENAME_MAILBOX,       op_rename_mailbox },
#endif
  { OP_SORT,                 op_sort },
  { OP_SORT_REVERSE,         op_sort },
#ifdef USE_NNTP
  { OP_SUBSCRIBE_PATTERN,    op_subscribe_pattern },
#endif
  { OP_TOGGLE_MAILBOXES,     op_toggle_mailboxes },
#ifdef USE_NNTP
  { OP_UNCATCHUP,            op_catchup },
  { OP_UNSUBSCRIBE_PATTERN,  op_subscribe_pattern },
#endif
  { 0, NULL },
  // clang-format on
};

/**
 * browser_function_dispatcher - Perform a Browser function
 * @param win_browser Window for the Index
 * @param op          Operation to perform, e.g. OP_GOTO_PARENT
 * @retval num #IndexRetval, e.g. #FR_SUCCESS
 */
int browser_function_dispatcher(struct MuttWindow *win_browser, int op)
{
  if (!win_browser)
  {
    mutt_error(_(Not_available_in_this_menu));
    return FR_ERROR;
  }

  struct BrowserPrivateData *priv = win_browser->parent->wdata;
  if (!priv)
    return FR_ERROR;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; BrowserFunctions[i].op != OP_NULL; i++)
  {
    const struct BrowserFunction *fn = &BrowserFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(priv, op);
      break;
    }
  }

  return rc;
}
