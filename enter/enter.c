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
 * @page enter_enter GUI ask the user to enter a string
 *
 * GUI ask the user to enter a string
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "browser/lib.h"
#include "history/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "functions.h"
#include "init.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_history.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "opcodes.h"
#include "protos.h"
#include "state.h" // IWYU pragma: keep
#include "wdata.h"

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

  wdata->state->tabs = 0;
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
 * mutt_enter_string_full - Ask the user for a string
 * @param[in]  buf      Buffer to store the string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Initial cursor position
 * @param[in]  flags    Flags, see #CompletionFlags
 * @param[in]  multiple Allow multiple matches
 * @param[in]  m        Mailbox
 * @param[out] files    List of files selected
 * @param[out] numfiles Number of files selected
 * @param[out] state    Current state (if function is called repeatedly)
 * @retval 1  Redraw the screen and call the function again
 * @retval 0  Selection made
 * @retval -1 Aborted
 */
int mutt_enter_string_full(char *buf, size_t buflen, int col, CompletionFlags flags,
                           bool multiple, struct Mailbox *m, char ***files,
                           int *numfiles, struct EnterState *state)
{
  struct MuttWindow *win = msgwin_get_window();
  if (!win)
    return -1;

  int width = win->state.cols - col - 1;
  int rc = 0;
  mbstate_t mbstate = { 0 };

  struct EnterWindowData wdata = { buf,
                                   buflen,
                                   col,
                                   flags,
                                   multiple,
                                   m,
                                   files,
                                   numfiles,
                                   state,
                                   ENTER_REDRAW_NONE,
                                   (flags & MUTT_COMP_PASS),
                                   true,
                                   0,
                                   NULL,
                                   0,
                                   &mbstate,
                                   false };

  if (wdata.state->wbuf)
  {
    /* Coming back after return 1 */
    wdata.redraw = ENTER_REDRAW_LINE;
    wdata.first = false;
  }
  else
  {
    /* Initialise wbuf from buf */
    wdata.state->wbuflen = 0;
    wdata.state->lastchar = mutt_mb_mbstowcs(&wdata.state->wbuf,
                                             &wdata.state->wbuflen, 0, buf);
    wdata.redraw = ENTER_REDRAW_INIT;
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
      if (self_insert(&wdata, event.ch))
        goto bye;
      continue;
    }

    wdata.first = false;
    if ((event.op != OP_EDITOR_COMPLETE) && (event.op != OP_EDITOR_COMPLETE_QUERY))
      wdata.state->tabs = 0;
    wdata.redraw = ENTER_REDRAW_LINE;
    int rc_disp = enter_function_dispatcher(&wdata, event.op);
    switch (rc_disp)
    {
      case FR_NO_ACTION:
      {
        if (self_insert(&wdata, event.ch))
          goto bye;
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
  return rc;
}
