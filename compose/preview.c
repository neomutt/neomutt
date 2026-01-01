/**
 * @file
 * Compose message preview
 *
 * @authors
 * Copyright (C) 2024-2025 Dennis Schön <mail@dennis-schoen.de>
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
 * @page compose_preview Message Preview Window
 *
 * The Message Preview Window displays a preview of the email body. The content
 * can be scrolled with PAGEUP/PAGEDOWN.
 *
 * ## Windows
 *
 * | Name           | Type      | See Also             |
 * | :------------- | :-------- | :------------------- |
 * | Preview Window | WT_CUSTOM | preview_window_new() |
 *
 * **Parent**
 * - @ref compose_dlg_compose
 *
 * **Children**
 *
 * None.
 *
 * ## Data
 * - #PreviewWindowData
 *
 * The Preview Window stores its data (#PreviewWindowData) in MuttWindow::wdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type               | Handler                    |
 * | :----------------------- | :------------------------- |
 * | #NT_COLOR                | preview_color_observer()   |
 * | #NT_EMAIL (#NT_ENVELOPE) | preview_email_observer()   |
 * | #NT_WINDOW               | preview_window_observer()  |
 * | MuttWindow::recalc()     | preview_recalc()           |
 * | MuttWindow::repaint()    | preview_repaint()          |
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "color/lib.h"

// Maxixum body size in bytes to show in preview.
const long MAX_PREVIEW_BODY_SIZE = 1024 * 1024 * 5;

/**
 * struct PreviewWindowData - Data to fill the Preview Window
 */
struct PreviewWindowData
{
  struct Email *email;    ///< Email being composed
  int scroll_offset;      ///< Scroll offset
  struct MuttWindow *win; ///< Window holding the message preview
  struct MuttWindow *bar; ///< Status bar above the preview window
  bool more_content;      ///< Is there more content to scroll down to?
};

/**
 * @defgroup preview_function_api Preview Function API
 * @ingroup dispatcher_api
 *
 * Prototype for a Preview Function
 *
 * @param wdata Preview Window data
 * @param op    Operation to perform, e.g. OP_NEXT_PAGE
 * @retval enum #FunctionRetval
 */
typedef int (*preview_function_t)(struct PreviewWindowData *wdata, int op);

/**
 * struct PreviewFunction - A message preview function
 */
struct PreviewFunction
{
  int op;                      ///< Op code, e.g. OP_NEXT_PAGE
  preview_function_t function; ///< Function to call
};

/**
 * preview_wdata_free - Free the Preview Data - Implements MuttWindow::wdata_free() - @ingroup window_wdata_free
 */
static void preview_wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * preview_wdata_new - Create new Preview Data
 * @retval ptr New Preview Data
 */
static struct PreviewWindowData *preview_wdata_new(void)
{
  struct PreviewWindowData *wdata = mutt_mem_calloc(1, sizeof(struct PreviewWindowData));

  return wdata;
}

/**
 * draw_preview - Write the message preview to the compose window
 * @param win    Window to draw on
 * @param wdata  Preview Window data
 */
static void draw_preview(struct MuttWindow *win, struct PreviewWindowData *wdata)
{
  struct Email *e = wdata->email;

  // Reset preview window and status bar.
  mutt_window_clear(win);
  sbar_set_title(wdata->bar, _("-- Preview"));

  // Check for valid content type and disposition
  if ((e->body->disposition != DISP_INLINE) || (e->body->type != TYPE_TEXT))
  {
    mutt_error(_("Only inline attachments with content-type text/* can be previewed"));
    return;
  }

  // Ensure file isn't to too large.
  long file_size = mutt_file_get_size(e->body->filename);
  if (file_size > MAX_PREVIEW_BODY_SIZE)
  {
    mutt_error(_("Email too large to preview"));
    return;
  }

  FILE *fp = mutt_file_fopen(e->body->filename, "r");
  if (!fp)
  {
    mutt_perror("%s", e->body->filename);
    return;
  }

  wdata->more_content = false;

  int content_lines = 0; // number of (wrapped) content lines
  int row = 0;           // window row to print
  char *line = NULL;
  size_t line_len = 0;
  while ((line = mutt_file_read_line(line, &line_len, fp, NULL, MUTT_RL_NO_FLAGS)))
  {
    size_t pos = 0;
    bool text_left = true;
    while (text_left)
    {
      /* Text wrapping loop
       *
       * Note: We need to do the text wrapping also for text outside the visible
       *       area to ensure the scrolling works correctly.
       */

      content_lines++;

      line = mutt_str_expand_tabs(line, &line_len, 8);

      // Check how much of the string fits into the window width.
      size_t width = 0;
      size_t bytes = mutt_wstr_trunc(&line[pos], line_len - pos, win->state.cols, &width);

      // If it doesn't fill the full width we're done wrapping.
      if ((win->state.cols - width) > 0)
        text_left = false;

      // Only move the cursor and print if this line is currently visible.
      if ((content_lines >= wdata->scroll_offset) && (row < win->state.rows))
      {
        int rc = mutt_window_move(win, row, 0);
        if (rc == ERR)
          mutt_warning(_("Failed to move cursor!"));

        mutt_paddstr(win, win->state.cols, &line[pos]);

        row++;
      }

      // Advance position in string.
      pos += bytes;
    }
  }

  FREE(&line);
  mutt_file_fclose(&fp);

  // Show the scroll percentage in the status bar
  if ((content_lines != 0) && (content_lines > win->state.rows))
  {
    char title[256] = { 0 };
    double percent = 100.0;
    if ((wdata->scroll_offset + row) < content_lines)
      percent = 100.0 / content_lines * (wdata->scroll_offset + row);

    // TODO: having the percentage right-aligned would be nice
    snprintf(title, sizeof(title), _("-- Preview (%.0f%%)"), percent);
    sbar_set_title(wdata->bar, title);

    if (content_lines > (wdata->scroll_offset + row))
      wdata->more_content = true;
  }
}

/**
 * preview_color_observer - Notification that a Color has changed - Implements ::observer_t - @ingroup observer_api
 */
static int preview_color_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_COLOR)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventColor *ev_c = nc->event_data;
  struct MuttWindow *win = nc->global_data;

  enum ColorId cid = ev_c->cid;

  switch (cid)
  {
    case MT_COLOR_BOLD:
    case MT_COLOR_NORMAL:
    case MT_COLOR_STATUS:
    case MT_COLOR_MAX: // Sent on `uncolor *`
      mutt_debug(LL_DEBUG5, "color done, request WA_REPAINT\n");
      win->actions |= WA_REPAINT;
      break;

    default:
      break;
  }
  return 0;
}

/**
 * preview_email_observer - Notification that the Email has changed - Implements ::observer_t - @ingroup observer_api
 */
static int preview_email_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_EMAIL)
    return 0;
  if (!nc->global_data)
    return -1;

  struct MuttWindow *win = nc->global_data;

  win->actions |= WA_RECALC;
  mutt_debug(LL_DEBUG5, "email done, request WA_RECALC\n");
  return 0;
}

/**
 * preview_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 */
static int preview_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct MuttWindow *win = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win)
    return 0;

  if (nc->event_subtype == NT_WINDOW_STATE)
  {
    win->actions |= WA_RECALC;
    mutt_debug(LL_DEBUG5, "window state done, request WA_RECALC\n");
  }
  else if (nc->event_subtype == NT_WINDOW_DELETE)
  {
    struct PreviewWindowData *wdata = win->wdata;

    mutt_color_observer_remove(preview_color_observer, win);
    notify_observer_remove(win->notify, preview_window_observer, win);
    notify_observer_remove(wdata->email->notify, preview_email_observer, win);
    mutt_debug(LL_DEBUG5, "window delete done\n");
  }

  return 0;
}

/**
 * preview_repaint - Repaint the Window - Implements MuttWindow::repaint() - @ingroup window_repaint
 */
static int preview_repaint(struct MuttWindow *win)
{
  struct PreviewWindowData *wdata = win->wdata;
  draw_preview(win, wdata);

  mutt_debug(LL_DEBUG5, "repaint done\n");
  return 0;
}

/**
 * preview_recalc - Recalculate the Window data - Implements MuttWindow::recalc() - @ingroup window_recalc
 */
static int preview_recalc(struct MuttWindow *win)
{
  if (!win)
    return -1;

  win->actions |= WA_REPAINT;
  mutt_debug(LL_DEBUG5, "recalc done, request WA_REPAINT\n");
  return 0;
}

/**
 * preview_window_new - Create the preview window
 * @param e   Email
 * @param bar Preview Bar
 */
struct MuttWindow *preview_window_new(struct Email *e, struct MuttWindow *bar)
{
  struct MuttWindow *win = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);

  mutt_color_observer_add(preview_color_observer, win);
  notify_observer_add(win->notify, NT_WINDOW, preview_window_observer, win);
  notify_observer_add(e->notify, NT_ALL, preview_email_observer, win);

  struct PreviewWindowData *wdata = preview_wdata_new();
  wdata->email = e;
  wdata->scroll_offset = 0;
  wdata->win = win;
  wdata->bar = bar;

  win->wdata = wdata;
  win->wdata_free = preview_wdata_free;
  win->recalc = preview_recalc;
  win->repaint = preview_repaint;

  return win;
}

/**
 * preview_page_up - Show the previous page of the message - Implements ::preview_function_t - @ingroup preview_function_api
 */
static int preview_page_up(struct PreviewWindowData *wdata, int op)
{
  if (wdata->scroll_offset <= 0)
    return FR_NO_ACTION;

  wdata->scroll_offset -= MAX(wdata->win->state.rows - 1, 1);
  draw_preview(wdata->win, wdata);

  return FR_SUCCESS;
}

/**
 * preview_page_down - Show the previous page of the message - Implements ::preview_function_t - @ingroup preview_function_api
 */
static int preview_page_down(struct PreviewWindowData *wdata, int op)
{
  if (!wdata->more_content)
    return FR_NO_ACTION;

  wdata->scroll_offset += MAX(wdata->win->state.rows - 1, 1);
  draw_preview(wdata->win, wdata);

  return FR_SUCCESS;
}

/**
 * PreviewFunctions - All the functions that the preview window supports
 */
static const struct PreviewFunction PreviewFunctions[] = {
  // clang-format off
  { OP_PREVIEW_PAGE_DOWN, preview_page_down },
  { OP_PREVIEW_PAGE_UP,   preview_page_up   },
  { 0, NULL },
  // clang-format on
};

/**
 * preview_function_dispatcher - Perform a preview function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int preview_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win || !win->wdata)
    return FR_UNKNOWN;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; PreviewFunctions[i].op != OP_NULL; i++)
  {
    const struct PreviewFunction *fn = &PreviewFunctions[i];
    if (fn->op == op)
    {
      struct PreviewWindowData *wdata = win->wdata;
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
