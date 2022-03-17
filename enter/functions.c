/**
 * @file
 * Enter functions
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
 * @page enter_functions Enter functions
 *
 * Enter functions
 */

#include "config.h"
#include "gui/lib.h"
#include "functions.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "opcodes.h"

struct EnterWindowData;

/**
 * op_editor_complete - Complete filename or alias - Implements ::enter_function_t - @ingroup enter_function_api
 *
 * This function handles:
 * - OP_EDITOR_COMPLETE
 * - OP_EDITOR_COMPLETE_QUERY
 */
static int op_editor_complete(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_history_down - Scroll down through the history list - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_history_down(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_history_search - Search through the history list - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_history_search(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_history_up - Scroll up through the history list - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_history_up(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_mailbox_cycle - Cycle among incoming mailboxes - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_mailbox_cycle(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * op_editor_backspace - Delete the char in front of the cursor - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_backspace(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_backward_char - Move the cursor one character to the left - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_backward_char(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_backward_word - Move the cursor to the beginning of the word - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_backward_word(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_bol - Jump to the beginning of the line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_bol(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_capitalize_word - Capitalize the word - Implements ::enter_function_t - @ingroup enter_function_api
 * This function handles:
 * - OP_EDITOR_CAPITALIZE_WORD
 * - OP_EDITOR_DOWNCASE_WORD
 * - OP_EDITOR_UPCASE_WORD
 */
static int op_editor_capitalize_word(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_delete_char - Delete the char under the cursor - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_delete_char(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_eol - Jump to the end of the line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_eol(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_forward_char - Move the cursor one character to the right - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_forward_char(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_forward_word - Move the cursor to the end of the word - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_forward_word(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_kill_eol - Delete chars from cursor to end of line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_eol(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_kill_eow - Delete chars from the cursor to the end of the word - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_eow(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_kill_line - Delete all chars on the line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_line(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_kill_word - Delete the word in front of the cursor - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_word(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_quote_char - Quote the next typed key - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_quote_char(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

/**
 * op_editor_transpose_chars - Transpose character under cursor with previous - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_transpose_chars(struct EnterWindowData *wdata, int op)
{
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * EnterFunctions - All the NeoMutt functions that Enter supports
 */
struct EnterFunction EnterFunctions[] = {
  // clang-format off
  { OP_EDITOR_BACKSPACE,          op_editor_backspace },
  { OP_EDITOR_BACKWARD_CHAR,      op_editor_backward_char },
  { OP_EDITOR_BACKWARD_WORD,      op_editor_backward_word },
  { OP_EDITOR_BOL,                op_editor_bol },
  { OP_EDITOR_CAPITALIZE_WORD,    op_editor_capitalize_word },
  { OP_EDITOR_COMPLETE,           op_editor_complete },
  { OP_EDITOR_COMPLETE_QUERY,     op_editor_complete },
  { OP_EDITOR_DELETE_CHAR,        op_editor_delete_char },
  { OP_EDITOR_DOWNCASE_WORD,      op_editor_capitalize_word },
  { OP_EDITOR_EOL,                op_editor_eol },
  { OP_EDITOR_FORWARD_CHAR,       op_editor_forward_char },
  { OP_EDITOR_FORWARD_WORD,       op_editor_forward_word },
  { OP_EDITOR_HISTORY_DOWN,       op_editor_history_down },
  { OP_EDITOR_HISTORY_SEARCH,     op_editor_history_search },
  { OP_EDITOR_HISTORY_UP,         op_editor_history_up },
  { OP_EDITOR_KILL_EOL,           op_editor_kill_eol },
  { OP_EDITOR_KILL_EOW,           op_editor_kill_eow },
  { OP_EDITOR_KILL_LINE,          op_editor_kill_line },
  { OP_EDITOR_KILL_WORD,          op_editor_kill_word },
  { OP_EDITOR_MAILBOX_CYCLE,      op_editor_mailbox_cycle },
  { OP_EDITOR_QUOTE_CHAR,         op_editor_quote_char },
  { OP_EDITOR_TRANSPOSE_CHARS,    op_editor_transpose_chars },
  { OP_EDITOR_UPCASE_WORD,        op_editor_capitalize_word },
  { 0, NULL },
  // clang-format on
};

/**
 * enter_function_dispatcher - Perform an Enter function
 * @param wdata Enter window data
 * @param op  Operation to perform, e.g. OP_ENTER_NEXT
 * @retval num #FunctionRetval, e.g. #FR_SUCCESS
 */
int enter_function_dispatcher(struct EnterWindowData *wdata, int op)
{
  if (!wdata)
    return FR_UNKNOWN;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; EnterFunctions[i].op != OP_NULL; i++)
  {
    const struct EnterFunction *fn = &EnterFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(wdata, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
