/**
 * @file
 * Global functions
 *
 * @authors
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page gui_global Global functions
 *
 * Global functions
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "global.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pager/lib.h"
#include "curs_lib.h"
#include "external.h"
#include "mutt_curses.h"
#include "mutt_mailbox.h"
#include "mutt_window.h"
#include "opcodes.h"
#include "rootwin.h"
#include "screenshot.h"
#include "version.h"

/**
 * op_check_stats - Calculate message statistics for all mailboxes - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_check_stats(struct MuttWindow *win, const struct KeyEvent *event)
{
  mutt_mailbox_check(get_current_mailbox(),
                     MUTT_MAILBOX_CHECK_POSTPONED | MUTT_MAILBOX_CHECK_STATS);
  return FR_SUCCESS;
}

/**
 * op_enter_command - Enter a neomuttrc command - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_enter_command(struct MuttWindow *win, const struct KeyEvent *event)
{
  mutt_enter_command(win);
  window_redraw(NULL);
  return FR_SUCCESS;
}

/**
 * op_redraw - Clear and redraw the screen - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_redraw(struct MuttWindow *win, const struct KeyEvent *event)
{
  clearok(stdscr, true);
  mutt_resize_screen();
  window_invalidate_all();
  window_redraw(NULL);
  return FR_SUCCESS;
}

/**
 * screenshot_prepare_window - Queue redraws needed for screenshot capture
 * @param win Window tree to inspect
 */
static void screenshot_prepare_window(struct MuttWindow *win)
{
  if (!win)
    return;

  if ((win->type == WT_MENU) && win->wdata)
  {
    struct Menu *menu = win->wdata;
    menu->redraw |= MENU_REDRAW_FULL;
  }

  struct MuttWindow **wp = NULL;
  ARRAY_FOREACH(wp, &win->children)
  {
    screenshot_prepare_window(*wp);
  }
}

/**
 * op_screenshot - Write an HTML screenshot - Implements ::global_function_t - @ingroup global_function_api
 */
int op_screenshot(struct MuttWindow *win, const struct KeyEvent *event)
{
  struct GuiModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_GUI);
  if (!mod_data || !mod_data->root_window)
    return FR_ERROR;

  if (ScreenshotActive)
    return FR_ERROR;

  char stamp[32] = { 0 };
  mutt_date_localtime_format(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", mutt_date_now());

  char path[64] = { 0 };
  snprintf(path, sizeof(path), "screenshot-%s.html", stamp);

  ScreenshotActive = screenshot_new(mod_data->root_window->state.rows, mod_data->root_window->state.cols);
  if (!ScreenshotActive)
    return FR_ERROR;

  screenshot_prepare_window(mod_data->root_window);
  window_invalidate_all();
  window_redraw(NULL);
  if (screenshot_end_and_write(&ScreenshotActive, path))
    mutt_message("Screenshot saved: %s", path);
  else
    mutt_error("Screenshot failed: %s", path);
  return FR_SUCCESS;
}

/**
 * op_shell_escape - Invoke a command in a subshell - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_shell_escape(struct MuttWindow *win, const struct KeyEvent *event)
{
  if (mutt_shell_escape())
  {
    struct Mailbox *m_cur = get_current_mailbox();
    if (m_cur)
    {
      m_cur->last_checked = 0; // force a check on the next mx_mbox_check() call
      mutt_mailbox_check(m_cur, MUTT_MAILBOX_CHECK_POSTPONED);
    }
  }
  return FR_SUCCESS;
}

/**
 * op_show_log_messages - Show log (and debug) messages - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_show_log_messages(struct MuttWindow *win, const struct KeyEvent *event)
{
  struct Buffer *tempfile = buf_pool_get();
  buf_mktemp(tempfile);

  FILE *fp = mutt_file_fopen(buf_string(tempfile), "a+");
  if (!fp)
  {
    mutt_perror("fopen");
    buf_pool_release(&tempfile);
    return FR_ERROR;
  }

  char buf[32] = { 0 };
  struct LogLine *ll = NULL;
  const struct LogLineList lll = log_queue_get();
  STAILQ_FOREACH(ll, &lll, entries)
  {
    mutt_date_localtime_format(buf, sizeof(buf), "%H:%M:%S", ll->time);
    fprintf(fp, "[%s]<%c> %s", buf, LogLevelAbbr[ll->level + 3], ll->message);
    if (ll->level <= LL_MESSAGE)
      fputs("\n", fp);
  }

  mutt_file_fclose(&fp);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = "messages";
  pview.flags = MUTT_PAGER_LOGS | MUTT_PAGER_BOTTOM;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  buf_pool_release(&tempfile);

  return FR_SUCCESS;
}

/**
 * op_version - Show the NeoMutt version number - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_version(struct MuttWindow *win, const struct KeyEvent *event)
{
  mutt_message("NeoMutt %s", mutt_make_version());
  return FR_SUCCESS;
}

/**
 * op_what_key - display the keycode for a key press - Implements ::global_function_t - @ingroup global_function_api
 */
static int op_what_key(struct MuttWindow *win, const struct KeyEvent *event)
{
  mw_what_key();
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * GlobalFunctions - All the NeoMutt functions that the Global supports
 */
static const struct GlobalFunction GlobalFunctions[] = {
  // clang-format off
  { OP_CHECK_STATS,           op_check_stats },
  { OP_ENTER_COMMAND,         op_enter_command },
  { OP_REDRAW,                op_redraw },
  { OP_SCREENSHOT,            op_screenshot },
  { OP_SHELL_ESCAPE,          op_shell_escape },
  { OP_SHOW_LOG_MESSAGES,     op_show_log_messages },
  { OP_VERSION,               op_version },
  { OP_WHAT_KEY,              op_what_key },
  { 0, NULL },
  // clang-format on
};

/**
 * global_function_dispatcher - Perform a Global function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 *
 * @note @a win Should be the currently focused Window
 */
int global_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  if (!event || !win)
    return FR_UNKNOWN;

  const int op = event->op;
  int rc = FR_UNKNOWN;
  for (size_t i = 0; GlobalFunctions[i].op != OP_NULL; i++)
  {
    const struct GlobalFunction *fn = &GlobalFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(win, event);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  dispatcher_flush_on_error(rc);
  return FR_SUCCESS; // Whatever the outcome, we handled it
}
