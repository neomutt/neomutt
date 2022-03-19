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

/* combining mark / non-spacing character */
#define COMB_CHAR(wc) (IsWPrint(wc) && !wcwidth(wc))

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
 * replace_part - Search and replace on a buffer
 * @param state Current state of the input buffer
 * @param from  Starting point for the replacement
 * @param buf   Replacement string
 */
static void replace_part(struct EnterState *state, size_t from, const char *buf)
{
  /* Save the suffix */
  size_t savelen = state->lastchar - state->curpos;
  wchar_t *savebuf = NULL;

  if (savelen)
  {
    savebuf = mutt_mem_calloc(savelen, sizeof(wchar_t));
    memcpy(savebuf, state->wbuf + state->curpos, savelen * sizeof(wchar_t));
  }

  /* Convert to wide characters */
  state->curpos = mutt_mb_mbstowcs(&state->wbuf, &state->wbuflen, from, buf);

  if (savelen)
  {
    /* Make space for suffix */
    if (state->curpos + savelen > state->wbuflen)
    {
      state->wbuflen = state->curpos + savelen;
      mutt_mem_realloc(&state->wbuf, state->wbuflen * sizeof(wchar_t));
    }

    /* Restore suffix */
    memcpy(state->wbuf + state->curpos, savebuf, savelen * sizeof(wchar_t));
    FREE(&savebuf);
  }

  state->lastchar = state->curpos + savelen;
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

  while (true)
  {
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
    mutt_refresh();

    struct KeyEvent event = km_dokey_event(MENU_EDITOR);
    if (event.op < 0)
    {
      rc = (SigWinch && (event.op == -2)) ? 1 : -1;
      goto bye;
    }

    if (event.op != OP_NULL)
    {
      wdata.first = false;
      if ((event.op != OP_EDITOR_COMPLETE) && (event.op != OP_EDITOR_COMPLETE_QUERY))
        wdata.state->tabs = 0;
      wdata.redraw = ENTER_REDRAW_LINE;
      switch (event.op)
      {
        case OP_EDITOR_HISTORY_UP:
          wdata.state->curpos = wdata.state->lastchar;
          if (mutt_hist_at_scratch(wdata.hclass))
          {
            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf, wdata.state->curpos);
            mutt_hist_save_scratch(wdata.hclass, buf);
          }
          replace_part(wdata.state, 0, mutt_hist_prev(wdata.hclass));
          wdata.redraw = ENTER_REDRAW_INIT;
          break;

        case OP_EDITOR_HISTORY_DOWN:
          wdata.state->curpos = wdata.state->lastchar;
          if (mutt_hist_at_scratch(wdata.hclass))
          {
            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf, wdata.state->curpos);
            mutt_hist_save_scratch(wdata.hclass, buf);
          }
          replace_part(wdata.state, 0, mutt_hist_next(wdata.hclass));
          wdata.redraw = ENTER_REDRAW_INIT;
          break;

        case OP_EDITOR_HISTORY_SEARCH:
          wdata.state->curpos = wdata.state->lastchar;
          mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf, wdata.state->curpos);
          mutt_hist_complete(buf, buflen, wdata.hclass);
          replace_part(wdata.state, 0, buf);
          rc = 1;
          goto bye;
          break;

        case OP_EDITOR_BACKSPACE:
          if (wdata.state->curpos == 0)
          {
            // Pressing backspace when no text is in the command prompt should exit the prompt
            const bool c_abort_backspace = cs_subset_bool(NeoMutt->sub, "abort_backspace");
            if (c_abort_backspace && (wdata.state->lastchar == 0))
              goto bye;
            // Pressing backspace with text in the command prompt should just beep
            mutt_beep(false);
          }
          else
          {
            size_t i = wdata.state->curpos;
            while ((i > 0) && COMB_CHAR(wdata.state->wbuf[i - 1]))
              i--;
            if (i > 0)
              i--;
            memmove(wdata.state->wbuf + i, wdata.state->wbuf + wdata.state->curpos,
                    (wdata.state->lastchar - wdata.state->curpos) * sizeof(wchar_t));
            wdata.state->lastchar -= wdata.state->curpos - i;
            wdata.state->curpos = i;
          }
          break;

        case OP_EDITOR_BOL:
          wdata.state->curpos = 0;
          break;

        case OP_EDITOR_EOL:
          wdata.redraw = ENTER_REDRAW_INIT;
          break;

        case OP_EDITOR_KILL_LINE:
          wdata.state->curpos = 0;
          wdata.state->lastchar = 0;
          break;

        case OP_EDITOR_KILL_EOL:
          wdata.state->lastchar = wdata.state->curpos;
          break;

        case OP_EDITOR_BACKWARD_CHAR:
          if (wdata.state->curpos == 0)
            mutt_beep(false);
          else
          {
            while (wdata.state->curpos &&
                   COMB_CHAR(wdata.state->wbuf[wdata.state->curpos - 1]))
              wdata.state->curpos--;
            if (wdata.state->curpos)
              wdata.state->curpos--;
          }
          break;

        case OP_EDITOR_FORWARD_CHAR:
          if (wdata.state->curpos == wdata.state->lastchar)
            mutt_beep(false);
          else
          {
            wdata.state->curpos++;
            while ((wdata.state->curpos < wdata.state->lastchar) &&
                   COMB_CHAR(wdata.state->wbuf[wdata.state->curpos]))
            {
              wdata.state->curpos++;
            }
          }
          break;

        case OP_EDITOR_BACKWARD_WORD:
          if (wdata.state->curpos == 0)
            mutt_beep(false);
          else
          {
            while (wdata.state->curpos &&
                   iswspace(wdata.state->wbuf[wdata.state->curpos - 1]))
              wdata.state->curpos--;
            while (wdata.state->curpos &&
                   !iswspace(wdata.state->wbuf[wdata.state->curpos - 1]))
              wdata.state->curpos--;
          }
          break;

        case OP_EDITOR_FORWARD_WORD:
          if (wdata.state->curpos == wdata.state->lastchar)
            mutt_beep(false);
          else
          {
            while ((wdata.state->curpos < wdata.state->lastchar) &&
                   iswspace(wdata.state->wbuf[wdata.state->curpos]))
            {
              wdata.state->curpos++;
            }
            while ((wdata.state->curpos < wdata.state->lastchar) &&
                   !iswspace(wdata.state->wbuf[wdata.state->curpos]))
            {
              wdata.state->curpos++;
            }
          }
          break;

        case OP_EDITOR_CAPITALIZE_WORD:
        case OP_EDITOR_UPCASE_WORD:
        case OP_EDITOR_DOWNCASE_WORD:
          if (wdata.state->curpos == wdata.state->lastchar)
          {
            mutt_beep(false);
            break;
          }
          while (wdata.state->curpos && !iswspace(wdata.state->wbuf[wdata.state->curpos]))
            wdata.state->curpos--;
          while ((wdata.state->curpos < wdata.state->lastchar) &&
                 iswspace(wdata.state->wbuf[wdata.state->curpos]))
            wdata.state->curpos++;
          while ((wdata.state->curpos < wdata.state->lastchar) &&
                 !iswspace(wdata.state->wbuf[wdata.state->curpos]))
          {
            if (event.op == OP_EDITOR_DOWNCASE_WORD)
              wdata.state->wbuf[wdata.state->curpos] = towlower(
                  wdata.state->wbuf[wdata.state->curpos]);
            else
            {
              wdata.state->wbuf[wdata.state->curpos] = towupper(
                  wdata.state->wbuf[wdata.state->curpos]);
              if (event.op == OP_EDITOR_CAPITALIZE_WORD)
                event.op = OP_EDITOR_DOWNCASE_WORD;
            }
            wdata.state->curpos++;
          }
          break;

        case OP_EDITOR_DELETE_CHAR:
          if (wdata.state->curpos == wdata.state->lastchar)
            mutt_beep(false);
          else
          {
            size_t i = wdata.state->curpos;
            while ((i < wdata.state->lastchar) && COMB_CHAR(wdata.state->wbuf[i]))
              i++;
            if (i < wdata.state->lastchar)
              i++;
            while ((i < wdata.state->lastchar) && COMB_CHAR(wdata.state->wbuf[i]))
              i++;
            memmove(wdata.state->wbuf + wdata.state->curpos, wdata.state->wbuf + i,
                    (wdata.state->lastchar - i) * sizeof(wchar_t));
            wdata.state->lastchar -= i - wdata.state->curpos;
          }
          break;

        case OP_EDITOR_KILL_WORD:
          /* delete to beginning of word */
          if (wdata.state->curpos != 0)
          {
            size_t i = wdata.state->curpos;
            while (i && iswspace(wdata.state->wbuf[i - 1]))
              i--;
            if (i > 0)
            {
              if (iswalnum(wdata.state->wbuf[i - 1]))
              {
                for (--i; (i > 0) && iswalnum(wdata.state->wbuf[i - 1]); i--)
                  ; // do nothing
              }
              else
                i--;
            }
            memmove(wdata.state->wbuf + i, wdata.state->wbuf + wdata.state->curpos,
                    (wdata.state->lastchar - wdata.state->curpos) * sizeof(wchar_t));
            wdata.state->lastchar += i - wdata.state->curpos;
            wdata.state->curpos = i;
          }
          break;

        case OP_EDITOR_KILL_EOW:
        {
          /* delete to end of word */

          /* wdata.first skip over whitespace */
          size_t i;
          for (i = wdata.state->curpos;
               (i < wdata.state->lastchar) && iswspace(wdata.state->wbuf[i]); i++)
            ; // do nothing

          /* if there are any characters left.. */
          if (i < wdata.state->lastchar)
          {
            /* if the current character is alphanumeric.. */
            if (iswalnum(wdata.state->wbuf[i]))
            {
              /* skip over the rest of the word consistent of only alphanumerics */
              for (; (i < wdata.state->lastchar) && iswalnum(wdata.state->wbuf[i]); i++)
                ; // do nothing
            }
            else
            {
              /* skip over one non-alphanumeric character */
              i++;
            }
          }

          memmove(wdata.state->wbuf + wdata.state->curpos, wdata.state->wbuf + i,
                  (wdata.state->lastchar - i) * sizeof(wchar_t));
          wdata.state->lastchar += wdata.state->curpos - i;
          break;
        }

        case OP_EDITOR_MAILBOX_CYCLE:
          if (wdata.flags & MUTT_COMP_FILE_MBOX)
          {
            wdata.first = true; /* clear input if user types a real key later */
            mutt_mb_wcstombs(wdata.buf, wdata.buflen, wdata.state->wbuf,
                             wdata.state->curpos);

            struct Buffer *pool = mutt_buffer_pool_get();
            mutt_buffer_addstr(pool, wdata.buf);
            mutt_mailbox_next(wdata.m, pool);
            mutt_str_copy(wdata.buf, mutt_buffer_string(pool), wdata.buflen);
            mutt_buffer_pool_release(&pool);

            wdata.state->curpos = wdata.state->lastchar = mutt_mb_mbstowcs(
                &wdata.state->wbuf, &wdata.state->wbuflen, 0, wdata.buf);
            break;
          }
          else if (!(wdata.flags & MUTT_COMP_FILE))
          {
            if (self_insert(&wdata, event.ch))
              goto bye;
            break;
          }
          /* fallthrough */

        case OP_EDITOR_COMPLETE:
        case OP_EDITOR_COMPLETE_QUERY:
          wdata.state->tabs++;
          if (wdata.flags & MUTT_COMP_FILE_SIMPLE)
          {
            size_t i;
            for (i = wdata.state->curpos;
                 (i > 0) && !mutt_mb_is_shell_char(wdata.state->wbuf[i - 1]); i--)
            {
            }
            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf + i, wdata.state->curpos - i);
            if (wdata.tempbuf && (wdata.templen == (wdata.state->lastchar - i)) &&
                (memcmp(wdata.tempbuf, wdata.state->wbuf + i,
                        (wdata.state->lastchar - i) * sizeof(wchar_t)) == 0))
            {
              mutt_select_file(buf, buflen, MUTT_SEL_NO_FLAGS, m, NULL, NULL);
              if (buf[0] != '\0')
                replace_part(wdata.state, i, buf);
              rc = 1;
              goto bye;
            }
            if (mutt_complete(buf, buflen) == 0)
            {
              wdata.templen = wdata.state->lastchar - i;
              mutt_mem_realloc(&wdata.tempbuf, wdata.templen * sizeof(wchar_t));
              memcpy(wdata.tempbuf, wdata.state->wbuf + i, wdata.templen * sizeof(wchar_t));
            }
            else
              mutt_beep(false);

            replace_part(wdata.state, i, buf);
          }
          else if ((wdata.flags & MUTT_COMP_ALIAS) && (event.op == OP_EDITOR_COMPLETE))
          {
            /* invoke the alias-menu to get more addresses */
            size_t i;
            for (i = wdata.state->curpos; (i > 0) && (wdata.state->wbuf[i - 1] != ',') &&
                                          (wdata.state->wbuf[i - 1] != ':');
                 i--)
            {
            }
            for (; (i < wdata.state->lastchar) && (wdata.state->wbuf[i] == ' '); i++)
              ; // do nothing

            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf + i, wdata.state->curpos - i);
            int rc2 = alias_complete(buf, buflen, NeoMutt->sub);
            replace_part(wdata.state, i, buf);
            if (rc2 != 1)
            {
              rc = 1;
              goto bye;
            }
            break;
          }
          else if ((wdata.flags & MUTT_COMP_LABEL) && (event.op == OP_EDITOR_COMPLETE))
          {
            size_t i;
            for (i = wdata.state->curpos; (i > 0) && (wdata.state->wbuf[i - 1] != ',') &&
                                          (wdata.state->wbuf[i - 1] != ':');
                 i--)
            {
            }
            for (; (i < wdata.state->lastchar) && (wdata.state->wbuf[i] == ' '); i++)
              ; // do nothing

            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf + i, wdata.state->curpos - i);
            int rc2 = mutt_label_complete(buf, buflen, wdata.state->tabs);
            replace_part(wdata.state, i, buf);
            if (rc2 != 1)
            {
              rc = 1;
              goto bye;
            }
            break;
          }
          else if ((wdata.flags & MUTT_COMP_PATTERN) && (event.op == OP_EDITOR_COMPLETE))
          {
            size_t i = wdata.state->curpos;
            if (i && (wdata.state->wbuf[i - 1] == '~'))
            {
              if (dlg_select_pattern(buf, buflen))
                replace_part(wdata.state, i - 1, buf);
              rc = 1;
              goto bye;
            }
            for (; (i > 0) && (wdata.state->wbuf[i - 1] != '~'); i--)
              ; // do nothing

            if ((i > 0) && (i < wdata.state->curpos) &&
                (wdata.state->wbuf[i - 1] == '~') && (wdata.state->wbuf[i] == 'y'))
            {
              i++;
              mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf + i, wdata.state->curpos - i);
              int rc2 = mutt_label_complete(buf, buflen, wdata.state->tabs);
              replace_part(wdata.state, i, buf);
              if (rc2 != 1)
              {
                rc = 1;
                goto bye;
              }
            }
            else
            {
              if (self_insert(&wdata, event.ch))
                goto bye;
            }
            break;
          }
          else if ((wdata.flags & MUTT_COMP_ALIAS) && (event.op == OP_EDITOR_COMPLETE_QUERY))
          {
            size_t i = wdata.state->curpos;
            if (i != 0)
            {
              for (; (i > 0) && (wdata.state->wbuf[i - 1] != ','); i--)
                ; // do nothing

              for (; (i < wdata.state->curpos) && (wdata.state->wbuf[i] == ' '); i++)
                ; // do nothing
            }

            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf + i, wdata.state->curpos - i);
            struct Buffer *tmp = mutt_buffer_pool_get();
            mutt_buffer_strcpy(tmp, buf);
            query_complete(tmp, NeoMutt->sub);
            mutt_str_copy(buf, mutt_buffer_string(tmp), buflen);
            mutt_buffer_pool_release(&tmp);
            replace_part(wdata.state, i, buf);

            rc = 1;
            goto bye;
          }
          else if (wdata.flags & MUTT_COMP_COMMAND)
          {
            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf, wdata.state->curpos);
            size_t i = strlen(buf);
            if ((i != 0) && (buf[i - 1] == '=') &&
                (mutt_var_value_complete(buf, buflen, i) != 0))
            {
              wdata.state->tabs = 0;
            }
            else if (mutt_command_complete(buf, buflen, i, wdata.state->tabs) == 0)
              mutt_beep(false);
            replace_part(wdata.state, 0, buf);
          }
          else if (wdata.flags & (MUTT_COMP_FILE | MUTT_COMP_FILE_MBOX))
          {
            mutt_mb_wcstombs(buf, buflen, wdata.state->wbuf, wdata.state->curpos);

            /* see if the path has changed from the last time */
            if ((!wdata.tempbuf && !wdata.state->lastchar) ||
                (wdata.tempbuf && (wdata.templen == wdata.state->lastchar) &&
                 (memcmp(wdata.tempbuf, wdata.state->wbuf,
                         wdata.state->lastchar * sizeof(wchar_t)) == 0)))
            {
              mutt_select_file(buf, buflen,
                               ((wdata.flags & MUTT_COMP_FILE_MBOX) ? MUTT_SEL_FOLDER : MUTT_SEL_NO_FLAGS) |
                                   (multiple ? MUTT_SEL_MULTI : MUTT_SEL_NO_FLAGS),
                               m, files, numfiles);
              if (buf[0] != '\0')
              {
                mutt_pretty_mailbox(buf, buflen);
                if (!wdata.pass)
                  mutt_hist_add(wdata.hclass, buf, true);
                rc = 0;
                goto bye;
              }

              /* file selection cancelled */
              rc = 1;
              goto bye;
            }

            if (mutt_complete(wdata.buf, wdata.buflen) == 0)
            {
              wdata.templen = wdata.state->lastchar;
              mutt_mem_realloc(&wdata.tempbuf, wdata.templen * sizeof(wchar_t));
              memcpy(wdata.tempbuf, wdata.state->wbuf, wdata.templen * sizeof(wchar_t));
            }
            else
              mutt_beep(false); /* let the user know that nothing matched */
            replace_part(wdata.state, 0, wdata.buf);
          }
#ifdef USE_NOTMUCH
          else if (wdata.flags & MUTT_COMP_NM_QUERY)
          {
            mutt_mb_wcstombs(wdata.buf, wdata.buflen, wdata.state->wbuf,
                             wdata.state->curpos);
            size_t len = strlen(wdata.buf);
            if (!mutt_nm_query_complete(wdata.buf, wdata.buflen, len,
                                        wdata.state->tabs))
              mutt_beep(false);

            replace_part(wdata.state, 0, wdata.buf);
          }
          else if (wdata.flags & MUTT_COMP_NM_TAG)
          {
            mutt_mb_wcstombs(wdata.buf, wdata.buflen, wdata.state->wbuf,
                             wdata.state->curpos);
            if (!mutt_nm_tag_complete(wdata.buf, wdata.buflen, wdata.state->tabs))
              mutt_beep(false);

            replace_part(wdata.state, 0, wdata.buf);
          }
#endif
          else
          {
            if (self_insert(&wdata, event.ch))
              goto bye;
          }
          break;

        case OP_EDITOR_QUOTE_CHAR:
        {
          struct KeyEvent quote_event = { OP_NULL, OP_NULL };
          do
          {
            quote_event = mutt_getch();
          } while (quote_event.ch == OP_TIMEOUT);
          if (quote_event.op != OP_ABORT)
          {
            event = quote_event;
            if (self_insert(&wdata, event.ch))
              goto bye;
          }
          break;
        }

        case OP_EDITOR_TRANSPOSE_CHARS:
          if (wdata.state->lastchar < 2)
            mutt_beep(false);
          else
          {
            if (wdata.state->curpos == 0)
              wdata.state->curpos = 2;
            else if (wdata.state->curpos < wdata.state->lastchar)
              wdata.state->curpos++;

            wchar_t wc = wdata.state->wbuf[wdata.state->curpos - 2];
            wdata.state->wbuf[wdata.state->curpos - 2] =
                wdata.state->wbuf[wdata.state->curpos - 1];
            wdata.state->wbuf[wdata.state->curpos - 1] = wc;
          }
          break;

        default:
          mutt_beep(false);
      }
    }
    else
    {
      if (self_insert(&wdata, event.ch))
        goto bye;
    }
  }

bye:
  mutt_hist_reset_state(wdata.hclass);
  FREE(&wdata.tempbuf);
  return rc;
}
