/**
 * @file
 * Editor functions
 *
 * @authors
 * Copyright (C) 2022-2023 Richard Russon <rich@flatcap.org>
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
 * @page editor_functions Editor functions
 *
 * Editor functions
 */

#include "config.h"
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "complete/lib.h"
#include "history/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "enter.h"
#include "functions.h"
#include "protos.h"
#include "state.h"
#include "wdata.h"
#endif

// clang-format off
/**
 * OpEditor - Functions for the Editor Menu
 */
const struct MenuFuncOp OpEditor[] = { /* map: editor */
  { "backspace",                     OP_EDITOR_BACKSPACE },
  { "backward-char",                 OP_EDITOR_BACKWARD_CHAR },
  { "backward-word",                 OP_EDITOR_BACKWARD_WORD },
  { "bol",                           OP_EDITOR_BOL },
  { "capitalize-word",               OP_EDITOR_CAPITALIZE_WORD },
  { "complete",                      OP_EDITOR_COMPLETE },
  { "complete-query",                OP_EDITOR_COMPLETE_QUERY },
  { "delete-char",                   OP_EDITOR_DELETE_CHAR },
  { "downcase-word",                 OP_EDITOR_DOWNCASE_WORD },
  { "eol",                           OP_EDITOR_EOL },
  { "forward-char",                  OP_EDITOR_FORWARD_CHAR },
  { "forward-word",                  OP_EDITOR_FORWARD_WORD },
  { "help",                          OP_HELP },
  { "history-down",                  OP_EDITOR_HISTORY_DOWN },
  { "history-search",                OP_EDITOR_HISTORY_SEARCH },
  { "history-up",                    OP_EDITOR_HISTORY_UP },
  { "kill-eol",                      OP_EDITOR_KILL_EOL },
  { "kill-eow",                      OP_EDITOR_KILL_EOW },
  { "kill-line",                     OP_EDITOR_KILL_LINE },
  { "kill-whole-line",               OP_EDITOR_KILL_WHOLE_LINE },
  { "kill-word",                     OP_EDITOR_KILL_WORD },
  { "mailbox-cycle",                 OP_EDITOR_MAILBOX_CYCLE },
  { "quote-char",                    OP_EDITOR_QUOTE_CHAR },
  { "redraw-screen",                 OP_REDRAW },
  { "transpose-chars",               OP_EDITOR_TRANSPOSE_CHARS },
  { "upcase-word",                   OP_EDITOR_UPCASE_WORD },
  // Deprecated
  { "buffy-cycle",                   OP_EDITOR_MAILBOX_CYCLE },
  { NULL, 0 },
};

/**
 * EditorDefaultBindings - Key bindings for the Editor Menu
 */
const struct MenuOpSeq EditorDefaultBindings[] = { /* map: editor */
  { OP_EDITOR_BACKSPACE,                   "<backspace>" },
  { OP_EDITOR_BACKSPACE,                   "\010" },           // <Ctrl-H>
  { OP_EDITOR_BACKSPACE,                   "\177" },           // <Backspace>
  { OP_EDITOR_BACKWARD_CHAR,               "<left>" },
  { OP_EDITOR_BACKWARD_CHAR,               "\002" },           // <Ctrl-B>
  { OP_EDITOR_BACKWARD_WORD,               "\033b" },          // <Alt-b>
  { OP_EDITOR_BOL,                         "<home>" },
  { OP_EDITOR_BOL,                         "\001" },           // <Ctrl-A>
  { OP_EDITOR_CAPITALIZE_WORD,             "\033c" },          // <Alt-c>
  { OP_EDITOR_COMPLETE,                    "\t" },             // <Tab>
  { OP_EDITOR_COMPLETE_QUERY,              "\024" },           // <Ctrl-T>
  { OP_EDITOR_DELETE_CHAR,                 "<delete>" },
  { OP_EDITOR_DELETE_CHAR,                 "\004" },           // <Ctrl-D>
  { OP_EDITOR_DOWNCASE_WORD,               "\033l" },          // <Alt-l>
  { OP_EDITOR_EOL,                         "<end>" },
  { OP_EDITOR_EOL,                         "\005" },           // <Ctrl-E>
  { OP_EDITOR_FORWARD_CHAR,                "<right>" },
  { OP_EDITOR_FORWARD_CHAR,                "\006" },           // <Ctrl-F>
  { OP_EDITOR_FORWARD_WORD,                "\033f" },          // <Alt-f>
  { OP_EDITOR_HISTORY_DOWN,                "<down>" },
  { OP_EDITOR_HISTORY_DOWN,                "\016" },           // <Ctrl-N>
  { OP_EDITOR_HISTORY_SEARCH,              "\022" },           // <Ctrl-R>
  { OP_EDITOR_HISTORY_UP,                  "<up>" },
  { OP_EDITOR_HISTORY_UP,                  "\020" },           // <Ctrl-P>
  { OP_EDITOR_KILL_EOL,                    "\013" },           // <Ctrl-K>
  { OP_EDITOR_KILL_EOW,                    "\033d" },          // <Alt-d>
  { OP_EDITOR_KILL_LINE,                   "\025" },           // <Ctrl-U>
  { OP_EDITOR_KILL_WORD,                   "\027" },           // <Ctrl-W>
  { OP_EDITOR_MAILBOX_CYCLE,               " " },              // <Space>
  { OP_EDITOR_QUOTE_CHAR,                  "\026" },           // <Ctrl-V>
  { OP_EDITOR_UPCASE_WORD,                 "\033u" },          // <Alt-u>
  { OP_HELP,                               "\033?" },          // <Alt-?>
  { OP_REDRAW,                             "\014" },           // <Ctrl-L>
  { 0, NULL },
};
// clang-format on

/**
 * replace_part - Search and replace on a buffer
 * @param es    Current state of the input buffer
 * @param from  Starting point for the replacement
 * @param buf   Replacement string
 */
void replace_part(struct EnterState *es, size_t from, const char *buf)
{
  /* Save the suffix */
  size_t savelen = es->lastchar - es->curpos;
  wchar_t *savebuf = NULL;

  if (savelen)
  {
    savebuf = mutt_mem_calloc(savelen, sizeof(wchar_t));
    memcpy(savebuf, es->wbuf + es->curpos, savelen * sizeof(wchar_t));
  }

  /* Convert to wide characters */
  es->curpos = mutt_mb_mbstowcs(&es->wbuf, &es->wbuflen, from, buf);

  if (savelen)
  {
    /* Make space for suffix */
    if (es->curpos + savelen > es->wbuflen)
    {
      es->wbuflen = es->curpos + savelen;
      mutt_mem_realloc(&es->wbuf, es->wbuflen * sizeof(wchar_t));
    }

    /* Restore suffix */
    memcpy(es->wbuf + es->curpos, savebuf, savelen * sizeof(wchar_t));
    FREE(&savebuf);
  }

  es->lastchar = es->curpos + savelen;
}

// -----------------------------------------------------------------------------

/**
 * op_editor_complete - Complete filename or alias - Implements ::enter_function_t - @ingroup enter_function_api
 *
 * This function handles:
 * - OP_EDITOR_COMPLETE
 * - OP_EDITOR_COMPLETE_QUERY
 */
static int op_editor_complete(struct EnterWindowData *wdata, int op)
{
  if (wdata->tabs == 0)
  {
    if (wdata->cd)
      completion_data_reset(wdata->cd);
    else
      wdata->cd = completion_data_new();
  }

  wdata->tabs++;
  wdata->redraw = ENTER_REDRAW_LINE;

  if (wdata->comp_api && wdata->comp_api->complete)
    return wdata->comp_api->complete(wdata, op);

  return FR_NO_ACTION;
}

// -----------------------------------------------------------------------------

/**
 * op_editor_history_down - Scroll down through the history list - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_history_down(struct EnterWindowData *wdata, int op)
{
  wdata->state->curpos = wdata->state->lastchar;
  if (mutt_hist_at_scratch(wdata->hclass))
  {
    buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);
    mutt_hist_save_scratch(wdata->hclass, buf_string(wdata->buffer));
  }
  replace_part(wdata->state, 0, mutt_hist_next(wdata->hclass));
  wdata->redraw = ENTER_REDRAW_INIT;
  return FR_SUCCESS;
}

/**
 * op_editor_history_search - Search through the history list - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_history_search(struct EnterWindowData *wdata, int op)
{
  wdata->state->curpos = wdata->state->lastchar;
  buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);
  mutt_hist_complete(wdata->buffer->data, wdata->buffer->dsize, wdata->hclass);
  replace_part(wdata->state, 0, wdata->buffer->data);
  return FR_CONTINUE;
}

/**
 * op_editor_history_up - Scroll up through the history list - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_history_up(struct EnterWindowData *wdata, int op)
{
  wdata->state->curpos = wdata->state->lastchar;
  if (mutt_hist_at_scratch(wdata->hclass))
  {
    buf_mb_wcstombs(wdata->buffer, wdata->state->wbuf, wdata->state->curpos);
    mutt_hist_save_scratch(wdata->hclass, buf_string(wdata->buffer));
  }
  replace_part(wdata->state, 0, mutt_hist_prev(wdata->hclass));
  wdata->redraw = ENTER_REDRAW_INIT;
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * op_editor_backspace - Delete the char in front of the cursor - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_backspace(struct EnterWindowData *wdata, int op)
{
  int rc = editor_backspace(wdata->state);

  if ((rc == FR_ERROR) && editor_buffer_is_empty(wdata->state))
  {
    const bool c_abort_backspace = cs_subset_bool(NeoMutt->sub, "abort_backspace");
    if (c_abort_backspace)
    {
      buf_reset(wdata->buffer);
      wdata->done = true;
      rc = FR_SUCCESS;
    }
  }

  return rc;
}

/**
 * op_editor_backward_char - Move the cursor one character to the left - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_backward_char(struct EnterWindowData *wdata, int op)
{
  return editor_backward_char(wdata->state);
}

/**
 * op_editor_backward_word - Move the cursor to the beginning of the word - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_backward_word(struct EnterWindowData *wdata, int op)
{
  return editor_backward_word(wdata->state);
}

/**
 * op_editor_bol - Jump to the beginning of the line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_bol(struct EnterWindowData *wdata, int op)
{
  return editor_bol(wdata->state);
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
  enum EnterCase ec;
  switch (op)
  {
    case OP_EDITOR_CAPITALIZE_WORD:
      ec = EC_CAPITALIZE;
      break;
    case OP_EDITOR_DOWNCASE_WORD:
      ec = EC_DOWNCASE;
      break;
    case OP_EDITOR_UPCASE_WORD:
      ec = EC_UPCASE;
      break;
    default:
      return FR_ERROR;
  }
  return editor_case_word(wdata->state, ec);
}

/**
 * op_editor_delete_char - Delete the char under the cursor - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_delete_char(struct EnterWindowData *wdata, int op)
{
  return editor_delete_char(wdata->state);
}

/**
 * op_editor_eol - Jump to the end of the line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_eol(struct EnterWindowData *wdata, int op)
{
  int rc = editor_eol(wdata->state);
  wdata->redraw = ENTER_REDRAW_INIT;
  return rc;
}

/**
 * op_editor_forward_char - Move the cursor one character to the right - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_forward_char(struct EnterWindowData *wdata, int op)
{
  return editor_forward_char(wdata->state);
}

/**
 * op_editor_forward_word - Move the cursor to the end of the word - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_forward_word(struct EnterWindowData *wdata, int op)
{
  return editor_forward_word(wdata->state);
}

/**
 * op_editor_kill_eol - Delete chars from cursor to end of line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_eol(struct EnterWindowData *wdata, int op)
{
  return editor_kill_eol(wdata->state);
}

/**
 * op_editor_kill_eow - Delete chars from the cursor to the end of the word - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_eow(struct EnterWindowData *wdata, int op)
{
  return editor_kill_eow(wdata->state);
}

/**
 * op_editor_kill_line - Delete all chars on the line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_line(struct EnterWindowData *wdata, int op)
{
  return editor_kill_line(wdata->state);
}

/**
 * op_editor_kill_whole_line - Delete all chars on the line - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_whole_line(struct EnterWindowData *wdata, int op)
{
  return editor_kill_whole_line(wdata->state);
}

/**
 * op_editor_kill_word - Delete the word in front of the cursor - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_kill_word(struct EnterWindowData *wdata, int op)
{
  return editor_kill_word(wdata->state);
}

/**
 * op_editor_quote_char - Quote the next typed key - Implements ::enter_function_t - @ingroup enter_function_api
 *
 * As part of the line-editor, this function uses the message window.
 *
 * @sa #gui_mw
 */
static int op_editor_quote_char(struct EnterWindowData *wdata, int op)
{
  struct KeyEvent event = { 0, OP_NULL };
  do
  {
    window_redraw(NULL);
    event = mutt_getch(GETCH_NO_FLAGS);
  } while ((event.op == OP_TIMEOUT) || (event.op == OP_REPAINT));

  if (event.op != OP_ABORT)
  {
    if (self_insert(wdata, event.ch))
    {
      wdata->done = true;
      return FR_SUCCESS;
    }
  }
  return FR_SUCCESS;
}

/**
 * op_editor_transpose_chars - Transpose character under cursor with previous - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_editor_transpose_chars(struct EnterWindowData *wdata, int op)
{
  return editor_transpose_chars(wdata->state);
}

/**
 * op_help - Display Help - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_help(struct EnterWindowData *wdata, int op)
{
  mutt_help(MENU_EDITOR);
  return FR_SUCCESS;
}

/**
 * op_redraw - Redraw the screen - Implements ::enter_function_t - @ingroup enter_function_api
 */
static int op_redraw(struct EnterWindowData *wdata, int op)
{
  clearok(stdscr, true);
  mutt_resize_screen();
  window_invalidate_all();
  window_redraw(NULL);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * EnterFunctions - All the NeoMutt functions that Enter supports
 */
static const struct EnterFunction EnterFunctions[] = {
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
  { OP_EDITOR_KILL_WHOLE_LINE,    op_editor_kill_whole_line },
  { OP_EDITOR_KILL_WORD,          op_editor_kill_word },
  { OP_EDITOR_MAILBOX_CYCLE,      op_editor_complete },
  { OP_EDITOR_QUOTE_CHAR,         op_editor_quote_char },
  { OP_EDITOR_TRANSPOSE_CHARS,    op_editor_transpose_chars },
  { OP_EDITOR_UPCASE_WORD,        op_editor_capitalize_word },
  { OP_HELP,                      op_help },
  { OP_REDRAW,                    op_redraw },
  { 0, NULL },
  // clang-format on
};

/**
 * enter_function_dispatcher - Perform an Enter function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int enter_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  struct EnterWindowData *wdata = win->wdata;

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

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
