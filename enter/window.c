/**
 * @file
 * GUI ask the user to enter a string
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2011,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2001 Edmund Grimley Evans <edmundo@rano.org>
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
 * @page enter_window GUI ask the user to enter a string
 *
 * GUI ask the user to enter a string
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "complete/lib.h"
#include "enter/lib.h"
#include "history/lib.h"
#include "menu/lib.h"
#include "color/color.h"
#include "functions.h"
#include "globals.h"
#include "keymap.h"
#include "muttlib.h"
#include "opcodes.h"
#include "wdata.h"

/// Help Bar for the Command Line Editor
static const struct Mapping EditorHelp[] = {
  // clang-format off
  { N_("Complete"),    OP_EDITOR_COMPLETE },
  { N_("Hist Up"),     OP_EDITOR_HISTORY_UP },
  { N_("Hist Down"),   OP_EDITOR_HISTORY_DOWN },
  { N_("Hist Search"), OP_EDITOR_HISTORY_SEARCH },
  { N_("Begin Line"),  OP_EDITOR_BOL },
  { N_("End Line"),    OP_EDITOR_EOL },
  { N_("Kill Line"),   OP_EDITOR_KILL_LINE },
  { N_("Kill Word"),   OP_EDITOR_KILL_WORD },
  { NULL, 0 },
  // clang-format on
};

/**
 * my_addwch - Display one wide character on screen
 * @param win Window
 * @param wc  Character to display
 * @retval OK  Success
 * @retval ERR Failure
 */
static int my_addwch(struct MuttWindow *win, wchar_t wc)
{
  int n = wcwidth(wc);
  if (IsWPrint(wc) && (n > 0))
    return mutt_addwch(win, wc);
  if (!(wc & ~0x7f))
    return mutt_window_printf(win, "^%c", ((int) wc + 0x40) & 0x7f);
  if (!(wc & ~0xffff))
    return mutt_window_printf(win, "\\u%04x", (int) wc);
  return mutt_window_printf(win, "\\u%08x", (int) wc);
}

/**
 * self_insert - Insert a normal character
 * @param wdata Enter window data
 * @param ch    Raw keypress
 * @retval true If done (enter pressed)
 */
bool self_insert(struct EnterWindowData *wdata, int ch)
{
  if (!wdata)
    return true;

  wdata->tabs = 0;
  wchar_t wc = 0;

  /* quietly ignore all other function keys */
  if (ch & ~0xff)
    return false;

  /* gather the octets into a wide character */
  {
    char c = ch;
    size_t k = mbrtowc(&wc, &c, 1, wdata->mbstate);
    if (k == (size_t) (-2))
      return false;
    else if ((k != 0) && (k != 1))
    {
      memset(wdata->mbstate, 0, sizeof(*wdata->mbstate));
      return false;
    }
  }

  if (wdata->first && (wdata->flags & MUTT_COMP_CLEAR))
  {
    wdata->first = false;
    if (IsWPrint(wc)) /* why? */
    {
      wdata->state->curpos = 0;
      wdata->state->lastchar = 0;
    }
  }

  if ((wc == '\r') || (wc == '\n'))
  {
    /* Convert from wide characters */
    mutt_mb_wcstombs(wdata->buf, wdata->buflen, wdata->state->wbuf, wdata->state->lastchar);
    if (!wdata->pass)
      mutt_hist_add(wdata->hclass, wdata->buf, true);

    if (wdata->multiple)
    {
      char **tfiles = NULL;
      *wdata->numfiles = 1;
      tfiles = mutt_mem_calloc(*wdata->numfiles, sizeof(char *));
      mutt_expand_path(wdata->buf, wdata->buflen);
      tfiles[0] = mutt_str_dup(wdata->buf);
      *wdata->files = tfiles;
    }
    return true;
  }
  else if (wc && ((wc < ' ') || IsWPrint(wc))) /* why? */
  {
    if (wdata->state->lastchar >= wdata->state->wbuflen)
    {
      wdata->state->wbuflen = wdata->state->lastchar + 20;
      mutt_mem_realloc(&wdata->state->wbuf, wdata->state->wbuflen * sizeof(wchar_t));
    }
    memmove(wdata->state->wbuf + wdata->state->curpos + 1,
            wdata->state->wbuf + wdata->state->curpos,
            (wdata->state->lastchar - wdata->state->curpos) * sizeof(wchar_t));
    wdata->state->wbuf[wdata->state->curpos++] = wc;
    wdata->state->lastchar++;
  }
  else
  {
    mutt_flushinp();
    mutt_beep(false);
  }

  return false;
}

/**
 * mutt_buffer_get_field - Ask the user for a string
 * @param[in]  field    Prompt
 * @param[in]  buf      Buffer for the result
 * @param[in]  complete Flags, see #CompletionFlags
 * @param[in]  multiple Allow multiple selections
 * @param[in]  m        Mailbox
 * @param[out] files    List of files selected
 * @param[out] numfiles Number of files selected
 * @retval 0  Selection made
 * @retval -1 Aborted
 */
int mutt_buffer_get_field(const char *field, struct Buffer *buf, CompletionFlags complete,
                          bool multiple, struct Mailbox *m, char ***files, int *numfiles)
{
  struct MuttWindow *win = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED,
                                           MUTT_WIN_SIZE_UNLIMITED, 1);
  // win->recalc = enter_window_recalc;
  // win->repaint = enter_window_repaint;
  win->actions |= WA_RECALC;

  msgcont_push_window(win);

  const bool old_oime = OptIgnoreMacroEvents;
  if (complete & MUTT_COMP_UNBUFFERED)
    OptIgnoreMacroEvents = true;

  int rc = 0;
  int col = 0;

  struct EnterState *es = enter_state_new();

  const struct Mapping *old_help = win->help_data;
  int old_menu = win->help_menu;

  win->help_data = EditorHelp;
  win->help_menu = MENU_EDITOR;
  struct MuttWindow *old_focus = window_set_focus(win);

  enum MuttCursorState cursor = mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);
  window_redraw(win);
  do
  {
    if (SigWinch)
    {
      SigWinch = false;
      mutt_resize_screen();
      clearok(stdscr, true);
      window_redraw(NULL);
    }
    mutt_window_clearline(win, 0);
    mutt_curses_set_normal_backed_color_by_id(MT_COLOR_PROMPT);
    mutt_window_addstr(win, field);
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
    mutt_refresh();
    mutt_window_get_coords(win, &col, NULL);

    int width = win->state.cols - col - 1;
    mbstate_t mbstate = { 0 };

    // clang-format off
    struct EnterWindowData wdata = { buf->data, buf->dsize, col, complete,
      multiple, m, files, numfiles, es, ENTER_REDRAW_NONE,
      (complete & MUTT_COMP_PASS), true, 0, NULL, 0, &mbstate, 0, false, NULL };
    // clang-format on
    win->wdata = &wdata;

    if (es->wbuf[0] == L'\0')
    {
      /* Initialise wbuf from buf */
      wdata.state->wbuflen = 0;
      wdata.state->lastchar = mutt_mb_mbstowcs(&wdata.state->wbuf,
                                               &wdata.state->wbuflen, 0, wdata.buf);
      wdata.redraw = ENTER_REDRAW_INIT;
    }
    else
    {
      wdata.redraw = ENTER_REDRAW_LINE;
      wdata.first = false;
    }

    if (wdata.flags & MUTT_COMP_FILE)
      wdata.hclass = HC_FILE;
    else if (wdata.flags & MUTT_COMP_FILE_MBOX)
      wdata.hclass = HC_MBOX;
    else if (wdata.flags & MUTT_COMP_FILE_SIMPLE)
      wdata.hclass = HC_CMD;
    else if (wdata.flags & MUTT_COMP_ALIAS)
      wdata.hclass = HC_ALIAS;
    else if (wdata.flags & MUTT_COMP_COMMAND)
      wdata.hclass = HC_COMMAND;
    else if (wdata.flags & MUTT_COMP_PATTERN)
      wdata.hclass = HC_PATTERN;
    else
      wdata.hclass = HC_OTHER;

    do
    {
      window_set_focus(win);
      if (!wdata.pass)
      {
        if (wdata.redraw == ENTER_REDRAW_INIT)
        {
          /* Go to end of line */
          wdata.state->curpos = wdata.state->lastchar;
          wdata.state->begin = mutt_mb_width_ceiling(
              wdata.state->wbuf, wdata.state->lastchar,
              mutt_mb_wcswidth(wdata.state->wbuf, wdata.state->lastchar) - width + 1);
        }
        if ((wdata.state->curpos < wdata.state->begin) ||
            (mutt_mb_wcswidth(wdata.state->wbuf + wdata.state->begin,
                              wdata.state->curpos - wdata.state->begin) >= width))
        {
          wdata.state->begin = mutt_mb_width_ceiling(
              wdata.state->wbuf, wdata.state->lastchar,
              mutt_mb_wcswidth(wdata.state->wbuf, wdata.state->curpos) - (width / 2));
        }
        mutt_window_move(win, wdata.col, 0);
        int w = 0;
        for (size_t i = wdata.state->begin; i < wdata.state->lastchar; i++)
        {
          w += mutt_mb_wcwidth(wdata.state->wbuf[i]);
          if (w > width)
            break;
          my_addwch(win, wdata.state->wbuf[i]);
        }
        mutt_window_clrtoeol(win);
        mutt_window_move(win,
                         wdata.col +
                             mutt_mb_wcswidth(wdata.state->wbuf + wdata.state->begin,
                                              wdata.state->curpos - wdata.state->begin),
                         0);
      }

      // Restore the cursor position after drawing the screen
      int r = 0, c = 0;
      mutt_window_get_coords(win, &c, &r);
      window_redraw(NULL);
      mutt_window_move(win, c, r);

      struct KeyEvent event = km_dokey_event(MENU_EDITOR);
      if (event.op < 0)
      {
        rc = (SigWinch && (event.op == OP_TIMEOUT)) ? 1 : -1;
        goto bye;
      }

      if (event.op == OP_NULL)
      {
        if (complete & MUTT_COMP_PASS)
          mutt_debug(LL_DEBUG1, "Got char *\n");
        else
          mutt_debug(LL_DEBUG1, "Got char %c (0x%02x)\n", event.ch, event.ch);
        if (self_insert(&wdata, event.ch))
        {
          rc = 0;
          goto bye;
        }
        continue;
      }
      else
      {
        mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(event.op),
                   event.op);
      }

      wdata.first = false;
      if ((event.op != OP_EDITOR_COMPLETE) && (event.op != OP_EDITOR_COMPLETE_QUERY))
        wdata.tabs = 0;
      wdata.redraw = ENTER_REDRAW_LINE;
      int rc_disp = enter_function_dispatcher(win, event.op);
      switch (rc_disp)
      {
        case FR_NO_ACTION:
        {
          if (self_insert(&wdata, event.ch))
          {
            rc = 0;
            goto bye;
          }
          break;
        }
        case FR_CONTINUE: // repaint
          rc = 1;
          goto bye;

        case FR_SUCCESS:
          break;

        case FR_UNKNOWN:
        case FR_ERROR:
        default:
          mutt_beep(false);
      }
    } while (!wdata.done);

  bye:
    mutt_hist_reset_state(wdata.hclass);
    FREE(&wdata.tempbuf);
    completion_data_free(&wdata.cd);
  } while (rc == 1);
  mutt_curses_set_cursor(cursor);

  msgcont_pop_window();

  win->help_data = old_help;
  win->help_menu = old_menu;
  mutt_window_move(win, 0, 0);
  mutt_window_clearline(win, 0);
  window_set_focus(old_focus);
  mutt_window_free(&win);

  if (rc == 0)
    mutt_buffer_fix_dptr(buf);
  else
    mutt_buffer_reset(buf);

  enter_state_free(&es);

  OptIgnoreMacroEvents = old_oime;
  return rc;
}
