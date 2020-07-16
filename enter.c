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
 * @page enter GUI ask the user to enter a string
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
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "browser.h"
#include "context.h"
#include "enter_state.h"
#include "init.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_history.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "opcodes.h"
#include "protos.h"
#include "history/lib.h"
#include "pattern/lib.h"

/**
 * enum EnterRedrawFlags - redraw flags for mutt_enter_string_full()
 */
enum EnterRedrawFlags
{
  ENTER_REDRAW_NONE = 0, ///< Nothing to redraw
  ENTER_REDRAW_INIT,     ///< Go to end of line and redraw
  ENTER_REDRAW_LINE,     ///< Redraw entire line
};

/* combining mark / non-spacing character */
#define COMB_CHAR(wc) (IsWPrint(wc) && !wcwidth(wc))

/**
 * my_addwch - Display one wide character on screen
 * @param wc Character to display
 * @retval OK  Success
 * @retval ERR Failure
 */
static int my_addwch(wchar_t wc)
{
  int n = wcwidth(wc);
  if (IsWPrint(wc) && (n > 0))
    return mutt_addwch(wc);
  if (!(wc & ~0x7f))
    return mutt_window_printf("^%c", ((int) wc + 0x40) & 0x7f);
  if (!(wc & ~0xffff))
    return mutt_window_printf("\\u%04x", (int) wc);
  return mutt_window_printf("\\u%08x", (int) wc);
}

/**
 * replace_part - Search and replace on a buffer
 * @param state Current state of the input buffer
 * @param from  Starting point for the replacement
 * @param buf   Replacement string
 */
static void replace_part(struct EnterState *state, size_t from, char *buf)
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
 * mutt_enter_state_new - Create a new EnterState
 * @retval ptr New EnterState
 */
struct EnterState *mutt_enter_state_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct EnterState));
}

/**
 * mutt_enter_string - Ask the user for a string
 * @param buf    Buffer to store the string
 * @param buflen Buffer length
 * @param col    Initial cursor position
 * @param flags  Flags, see #CompletionFlags
 * @retval 0 if input was given
 * @retval -1 if abort
 *
 * This function is for very basic input, currently used only by the
 * built-in editor.  It does not handle screen redrawing on resizes
 * well, because there is no active menu for the built-in editor.
 * Most callers should prefer mutt_get_field() instead.
 */
int mutt_enter_string(char *buf, size_t buflen, int col, CompletionFlags flags)
{
  int rc;
  struct EnterState *es = mutt_enter_state_new();
  do
  {
    if (SigWinch)
    {
      SigWinch = 0;
      mutt_resize_screen();
      clearok(stdscr, true);
    }
    rc = mutt_enter_string_full(buf, buflen, col, flags, false, NULL, NULL, es);
  } while (rc == 1);
  mutt_enter_state_free(&es);
  return rc;
}

/**
 * mutt_enter_string_full - Ask the user for a string
 * @param[in]  buf      Buffer to store the string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Initial cursor position
 * @param[in]  flags    Flags, see #CompletionFlags
 * @param[in]  multiple Allow multiple matches
 * @param[out] files    List of files selected
 * @param[out] numfiles Number of files selected
 * @param[out] state    Current state (if function is called repeatedly)
 * @retval 1  Redraw the screen and call the function again
 * @retval 0  Selection made
 * @retval -1 Aborted
 */
int mutt_enter_string_full(char *buf, size_t buflen, int col,
                           CompletionFlags flags, bool multiple, char ***files,
                           int *numfiles, struct EnterState *state)
{
  int width = MuttMessageWindow->state.cols - col - 1;
  enum EnterRedrawFlags redraw = ENTER_REDRAW_NONE;
  bool pass = (flags & MUTT_PASS);
  bool first = true;
  int ch;
  wchar_t *tempbuf = NULL;
  size_t templen = 0;
  enum HistoryClass hclass;
  int rc = 0;
  mbstate_t mbstate;
  memset(&mbstate, 0, sizeof(mbstate));

  if (state->wbuf)
  {
    /* Coming back after return 1 */
    redraw = ENTER_REDRAW_LINE;
    first = false;
  }
  else
  {
    /* Initialise wbuf from buf */
    state->wbuflen = 0;
    state->lastchar = mutt_mb_mbstowcs(&state->wbuf, &state->wbuflen, 0, buf);
    redraw = ENTER_REDRAW_INIT;
  }

  if (flags & MUTT_FILE)
    hclass = HC_FILE;
  else if (flags & MUTT_EFILE)
    hclass = HC_MBOX;
  else if (flags & MUTT_CMD)
    hclass = HC_CMD;
  else if (flags & MUTT_ALIAS)
    hclass = HC_ALIAS;
  else if (flags & MUTT_COMMAND)
    hclass = HC_COMMAND;
  else if (flags & MUTT_PATTERN)
    hclass = HC_PATTERN;
  else
    hclass = HC_OTHER;

  while (true)
  {
    if (!pass)
    {
      if (redraw == ENTER_REDRAW_INIT)
      {
        /* Go to end of line */
        state->curpos = state->lastchar;
        state->begin = mutt_mb_width_ceiling(
            state->wbuf, state->lastchar,
            mutt_mb_wcswidth(state->wbuf, state->lastchar) - width + 1);
      }
      if ((state->curpos < state->begin) ||
          (mutt_mb_wcswidth(state->wbuf + state->begin, state->curpos - state->begin) >= width))
      {
        state->begin = mutt_mb_width_ceiling(
            state->wbuf, state->lastchar,
            mutt_mb_wcswidth(state->wbuf, state->curpos) - (width / 2));
      }
      mutt_window_move(MuttMessageWindow, col, 0);
      int w = 0;
      for (size_t i = state->begin; i < state->lastchar; i++)
      {
        w += mutt_mb_wcwidth(state->wbuf[i]);
        if (w > width)
          break;
        my_addwch(state->wbuf[i]);
      }
      mutt_window_clrtoeol(MuttMessageWindow);
      mutt_window_move(MuttMessageWindow,
                       col + mutt_mb_wcswidth(state->wbuf + state->begin,
                                              state->curpos - state->begin),
                       0);
    }
    mutt_refresh();

    ch = km_dokey(MENU_EDITOR);
    if (ch < 0)
    {
      rc = (SigWinch && (ch == -2)) ? 1 : -1;
      goto bye;
    }

    if (ch != OP_NULL)
    {
      first = false;
      if ((ch != OP_EDITOR_COMPLETE) && (ch != OP_EDITOR_COMPLETE_QUERY))
        state->tabs = 0;
      redraw = ENTER_REDRAW_LINE;
      switch (ch)
      {
        case OP_EDITOR_HISTORY_UP:
          state->curpos = state->lastchar;
          if (mutt_hist_at_scratch(hclass))
          {
            mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);
            mutt_hist_save_scratch(hclass, buf);
          }
          replace_part(state, 0, mutt_hist_prev(hclass));
          redraw = ENTER_REDRAW_INIT;
          break;

        case OP_EDITOR_HISTORY_DOWN:
          state->curpos = state->lastchar;
          if (mutt_hist_at_scratch(hclass))
          {
            mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);
            mutt_hist_save_scratch(hclass, buf);
          }
          replace_part(state, 0, mutt_hist_next(hclass));
          redraw = ENTER_REDRAW_INIT;
          break;

        case OP_EDITOR_HISTORY_SEARCH:
          state->curpos = state->lastchar;
          mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);
          mutt_hist_complete(buf, buflen, hclass);
          replace_part(state, 0, buf);
          rc = 1;
          goto bye;
          break;

        case OP_EDITOR_BACKSPACE:
          if (state->curpos == 0)
          {
            // Pressing backspace when no text is in the command prompt should exit the prompt
            if (C_AbortBackspace && (state->lastchar == 0))
              goto bye;
            // Pressing backspace with text in the command prompt should just beep
            mutt_beep(false);
          }
          else
          {
            size_t i = state->curpos;
            while ((i > 0) && COMB_CHAR(state->wbuf[i - 1]))
              i--;
            if (i > 0)
              i--;
            memmove(state->wbuf + i, state->wbuf + state->curpos,
                    (state->lastchar - state->curpos) * sizeof(wchar_t));
            state->lastchar -= state->curpos - i;
            state->curpos = i;
          }
          break;

        case OP_EDITOR_BOL:
          state->curpos = 0;
          break;

        case OP_EDITOR_EOL:
          redraw = ENTER_REDRAW_INIT;
          break;

        case OP_EDITOR_KILL_LINE:
          state->curpos = 0;
          state->lastchar = 0;
          break;

        case OP_EDITOR_KILL_EOL:
          state->lastchar = state->curpos;
          break;

        case OP_EDITOR_BACKWARD_CHAR:
          if (state->curpos == 0)
            mutt_beep(false);
          else
          {
            while (state->curpos && COMB_CHAR(state->wbuf[state->curpos - 1]))
              state->curpos--;
            if (state->curpos)
              state->curpos--;
          }
          break;

        case OP_EDITOR_FORWARD_CHAR:
          if (state->curpos == state->lastchar)
            mutt_beep(false);
          else
          {
            state->curpos++;
            while ((state->curpos < state->lastchar) &&
                   COMB_CHAR(state->wbuf[state->curpos]))
            {
              state->curpos++;
            }
          }
          break;

        case OP_EDITOR_BACKWARD_WORD:
          if (state->curpos == 0)
            mutt_beep(false);
          else
          {
            while (state->curpos && iswspace(state->wbuf[state->curpos - 1]))
              state->curpos--;
            while (state->curpos && !iswspace(state->wbuf[state->curpos - 1]))
              state->curpos--;
          }
          break;

        case OP_EDITOR_FORWARD_WORD:
          if (state->curpos == state->lastchar)
            mutt_beep(false);
          else
          {
            while ((state->curpos < state->lastchar) &&
                   iswspace(state->wbuf[state->curpos]))
            {
              state->curpos++;
            }
            while ((state->curpos < state->lastchar) &&
                   !iswspace(state->wbuf[state->curpos]))
            {
              state->curpos++;
            }
          }
          break;

        case OP_EDITOR_CAPITALIZE_WORD:
        case OP_EDITOR_UPCASE_WORD:
        case OP_EDITOR_DOWNCASE_WORD:
          if (state->curpos == state->lastchar)
          {
            mutt_beep(false);
            break;
          }
          while (state->curpos && !iswspace(state->wbuf[state->curpos]))
            state->curpos--;
          while ((state->curpos < state->lastchar) && iswspace(state->wbuf[state->curpos]))
            state->curpos++;
          while ((state->curpos < state->lastchar) &&
                 !iswspace(state->wbuf[state->curpos]))
          {
            if (ch == OP_EDITOR_DOWNCASE_WORD)
              state->wbuf[state->curpos] = towlower(state->wbuf[state->curpos]);
            else
            {
              state->wbuf[state->curpos] = towupper(state->wbuf[state->curpos]);
              if (ch == OP_EDITOR_CAPITALIZE_WORD)
                ch = OP_EDITOR_DOWNCASE_WORD;
            }
            state->curpos++;
          }
          break;

        case OP_EDITOR_DELETE_CHAR:
          if (state->curpos == state->lastchar)
            mutt_beep(false);
          else
          {
            size_t i = state->curpos;
            while ((i < state->lastchar) && COMB_CHAR(state->wbuf[i]))
              i++;
            if (i < state->lastchar)
              i++;
            while ((i < state->lastchar) && COMB_CHAR(state->wbuf[i]))
              i++;
            memmove(state->wbuf + state->curpos, state->wbuf + i,
                    (state->lastchar - i) * sizeof(wchar_t));
            state->lastchar -= i - state->curpos;
          }
          break;

        case OP_EDITOR_KILL_WORD:
          /* delete to beginning of word */
          if (state->curpos != 0)
          {
            size_t i = state->curpos;
            while (i && iswspace(state->wbuf[i - 1]))
              i--;
            if (i > 0)
            {
              if (iswalnum(state->wbuf[i - 1]))
              {
                for (--i; (i > 0) && iswalnum(state->wbuf[i - 1]); i--)
                  ; // do nothing
              }
              else
                i--;
            }
            memmove(state->wbuf + i, state->wbuf + state->curpos,
                    (state->lastchar - state->curpos) * sizeof(wchar_t));
            state->lastchar += i - state->curpos;
            state->curpos = i;
          }
          break;

        case OP_EDITOR_KILL_EOW:
        {
          /* delete to end of word */

          /* first skip over whitespace */
          size_t i;
          for (i = state->curpos; (i < state->lastchar) && iswspace(state->wbuf[i]); i++)
            ; // do nothing

          /* if there are any characters left.. */
          if (i < state->lastchar)
          {
            /* if the current character is alphanumeric.. */
            if (iswalnum(state->wbuf[i]))
            {
              /* skip over the rest of the word consistent of only alphanumerics */
              for (; (i < state->lastchar) && iswalnum(state->wbuf[i]); i++)
                ; // do nothing
            }
            else
            {
              /* skip over one non-alphanumeric character */
              i++;
            }
          }

          memmove(state->wbuf + state->curpos, state->wbuf + i,
                  (state->lastchar - i) * sizeof(wchar_t));
          state->lastchar += state->curpos - i;
          break;
        }

        case OP_EDITOR_MAILBOX_CYCLE:
          if (flags & MUTT_EFILE)
          {
            first = true; /* clear input if user types a real key later */
            mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);

            struct Buffer *pool = mutt_buffer_pool_get();
            mutt_buffer_addstr(pool, buf);
            mutt_mailbox_next(Context ? Context->mailbox : NULL, pool);
            mutt_str_copy(buf, mutt_b2s(pool), buflen);
            mutt_buffer_pool_release(&pool);

            state->curpos = state->lastchar =
                mutt_mb_mbstowcs(&state->wbuf, &state->wbuflen, 0, buf);
            break;
          }
          else if (!(flags & MUTT_FILE))
          {
            goto self_insert;
          }
          /* fallthrough */

        case OP_EDITOR_COMPLETE:
        case OP_EDITOR_COMPLETE_QUERY:
          state->tabs++;
          if (flags & MUTT_CMD)
          {
            size_t i;
            for (i = state->curpos;
                 (i > 0) && !mutt_mb_is_shell_char(state->wbuf[i - 1]); i--)
            {
            }
            mutt_mb_wcstombs(buf, buflen, state->wbuf + i, state->curpos - i);
            if (tempbuf && (templen == (state->lastchar - i)) &&
                (memcmp(tempbuf, state->wbuf + i, (state->lastchar - i) * sizeof(wchar_t)) == 0))
            {
              mutt_select_file(buf, buflen, (flags & MUTT_EFILE) ? MUTT_SEL_FOLDER : MUTT_SEL_NO_FLAGS,
                               NULL, NULL);
              if (buf[0] != '\0')
                replace_part(state, i, buf);
              rc = 1;
              goto bye;
            }
            if (mutt_complete(buf, buflen) == 0)
            {
              templen = state->lastchar - i;
              mutt_mem_realloc(&tempbuf, templen * sizeof(wchar_t));
            }
            else
              mutt_beep(false);

            replace_part(state, i, buf);
          }
          else if ((flags & MUTT_ALIAS) && (ch == OP_EDITOR_COMPLETE))
          {
            /* invoke the alias-menu to get more addresses */
            size_t i;
            for (i = state->curpos;
                 (i > 0) && (state->wbuf[i - 1] != ',') && (state->wbuf[i - 1] != ':'); i--)
            {
            }
            for (; (i < state->lastchar) && (state->wbuf[i] == ' '); i++)
              ; // do nothing

            mutt_mb_wcstombs(buf, buflen, state->wbuf + i, state->curpos - i);
            int rc2 = alias_complete(buf, buflen);
            replace_part(state, i, buf);
            if (rc2 != 1)
            {
              rc = 1;
              goto bye;
            }
            break;
          }
          else if ((flags & MUTT_LABEL) && (ch == OP_EDITOR_COMPLETE))
          {
            size_t i;
            for (i = state->curpos;
                 (i > 0) && (state->wbuf[i - 1] != ',') && (state->wbuf[i - 1] != ':'); i--)
            {
            }
            for (; (i < state->lastchar) && (state->wbuf[i] == ' '); i++)
              ; // do nothing

            mutt_mb_wcstombs(buf, buflen, state->wbuf + i, state->curpos - i);
            int rc2 = mutt_label_complete(buf, buflen, state->tabs);
            replace_part(state, i, buf);
            if (rc2 != 1)
            {
              rc = 1;
              goto bye;
            }
            break;
          }
          else if ((flags & MUTT_PATTERN) && (ch == OP_EDITOR_COMPLETE))
          {
            size_t i = state->curpos;
            if (i && (state->wbuf[i - 1] == '~'))
            {
              if (mutt_ask_pattern(buf, buflen))
                replace_part(state, i - 1, buf);
              rc = 1;
              goto bye;
            }
            for (; (i > 0) && (state->wbuf[i - 1] != '~'); i--)
              ; // do nothing

            if ((i > 0) && (i < state->curpos) && (state->wbuf[i - 1] == '~') &&
                (state->wbuf[i] == 'y'))
            {
              i++;
              mutt_mb_wcstombs(buf, buflen, state->wbuf + i, state->curpos - i);
              int rc2 = mutt_label_complete(buf, buflen, state->tabs);
              replace_part(state, i, buf);
              if (rc2 != 1)
              {
                rc = 1;
                goto bye;
              }
            }
            else
              goto self_insert;
            break;
          }
          else if ((flags & MUTT_ALIAS) && (ch == OP_EDITOR_COMPLETE_QUERY))
          {
            size_t i = state->curpos;
            if (i != 0)
            {
              for (; (i > 0) && (state->wbuf[i - 1] != ','); i--)
                ; // do nothing

              for (; (i < state->curpos) && (state->wbuf[i] == ' '); i++)
                ; // do nothing
            }

            mutt_mb_wcstombs(buf, buflen, state->wbuf + i, state->curpos - i);
            query_complete(buf, buflen);
            replace_part(state, i, buf);

            rc = 1;
            goto bye;
          }
          else if (flags & MUTT_COMMAND)
          {
            mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);
            size_t i = strlen(buf);
            if ((i != 0) && (buf[i - 1] == '=') &&
                (mutt_var_value_complete(buf, buflen, i) != 0))
            {
              state->tabs = 0;
            }
            else if (mutt_command_complete(buf, buflen, i, state->tabs) == 0)
              mutt_beep(false);
            replace_part(state, 0, buf);
          }
          else if (flags & (MUTT_FILE | MUTT_EFILE))
          {
            mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);

            /* see if the path has changed from the last time */
            if ((!tempbuf && !state->lastchar) ||
                (tempbuf && (templen == state->lastchar) &&
                 (memcmp(tempbuf, state->wbuf, state->lastchar * sizeof(wchar_t)) == 0)))
            {
              mutt_select_file(buf, buflen,
                               ((flags & MUTT_EFILE) ? MUTT_SEL_FOLDER : MUTT_SEL_NO_FLAGS) |
                                   (multiple ? MUTT_SEL_MULTI : MUTT_SEL_NO_FLAGS),
                               files, numfiles);
              if (buf[0] != '\0')
              {
                mutt_pretty_mailbox(buf, buflen);
                if (!pass)
                  mutt_hist_add(hclass, buf, true);
                rc = 0;
                goto bye;
              }

              /* file selection cancelled */
              rc = 1;
              goto bye;
            }

            if (mutt_complete(buf, buflen) == 0)
            {
              templen = state->lastchar;
              mutt_mem_realloc(&tempbuf, templen * sizeof(wchar_t));
              memcpy(tempbuf, state->wbuf, templen * sizeof(wchar_t));
            }
            else
              mutt_beep(false); /* let the user know that nothing matched */
            replace_part(state, 0, buf);
          }
#ifdef USE_NOTMUCH
          else if (flags & MUTT_NM_QUERY)
          {
            mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);
            size_t len = strlen(buf);
            if (!mutt_nm_query_complete(buf, buflen, len, state->tabs))
              mutt_beep(false);

            replace_part(state, 0, buf);
          }
          else if (flags & MUTT_NM_TAG)
          {
            mutt_mb_wcstombs(buf, buflen, state->wbuf, state->curpos);
            if (!mutt_nm_tag_complete(buf, buflen, state->tabs))
              mutt_beep(false);

            replace_part(state, 0, buf);
          }
#endif
          else
            goto self_insert;
          break;

        case OP_EDITOR_QUOTE_CHAR:
        {
          struct KeyEvent event;
          do
          {
            event = mutt_getch();
          } while (event.ch == -2);
          if (event.ch >= 0)
          {
            LastKey = event.ch;
            goto self_insert;
          }
          break;
        }

        case OP_EDITOR_TRANSPOSE_CHARS:
          if (state->lastchar < 2)
            mutt_beep(false);
          else
          {
            wchar_t t;

            if (state->curpos == 0)
              state->curpos = 2;
            else if (state->curpos < state->lastchar)
              state->curpos++;

            t = state->wbuf[state->curpos - 2];
            state->wbuf[state->curpos - 2] = state->wbuf[state->curpos - 1];
            state->wbuf[state->curpos - 1] = t;
          }
          break;

        default:
          mutt_beep(false);
      }
    }
    else
    {
    self_insert:
      state->tabs = 0;
      wchar_t wc;
      /* use the raw keypress */
      ch = LastKey;

      /* quietly ignore all other function keys */
      if (ch & ~0xff)
        continue;

      /* gather the octets into a wide character */
      {
        char c = ch;
        size_t k = mbrtowc(&wc, &c, 1, &mbstate);
        if (k == (size_t)(-2))
          continue;
        else if ((k != 0) && (k != 1))
        {
          memset(&mbstate, 0, sizeof(mbstate));
          continue;
        }
      }

      if (first && (flags & MUTT_CLEAR))
      {
        first = false;
        if (IsWPrint(wc)) /* why? */
        {
          state->curpos = 0;
          state->lastchar = 0;
        }
      }

      if ((wc == '\r') || (wc == '\n'))
      {
        /* Convert from wide characters */
        mutt_mb_wcstombs(buf, buflen, state->wbuf, state->lastchar);
        if (!pass)
          mutt_hist_add(hclass, buf, true);

        if (multiple)
        {
          char **tfiles = NULL;
          *numfiles = 1;
          tfiles = mutt_mem_calloc(*numfiles, sizeof(char *));
          mutt_expand_path(buf, buflen);
          tfiles[0] = mutt_str_dup(buf);
          *files = tfiles;
        }
        rc = 0;
        goto bye;
      }
      else if (wc && ((wc < ' ') || IsWPrint(wc))) /* why? */
      {
        if (state->lastchar >= state->wbuflen)
        {
          state->wbuflen = state->lastchar + 20;
          mutt_mem_realloc(&state->wbuf, state->wbuflen * sizeof(wchar_t));
        }
        memmove(state->wbuf + state->curpos + 1, state->wbuf + state->curpos,
                (state->lastchar - state->curpos) * sizeof(wchar_t));
        state->wbuf[state->curpos++] = wc;
        state->lastchar++;
      }
      else
      {
        mutt_flushinp();
        mutt_beep(false);
      }
    }
  }

bye:

  mutt_hist_reset_state(hclass);
  FREE(&tempbuf);
  return rc;
}

/**
 * mutt_enter_state_free - Free an EnterState
 * @param[out] ptr EnterState to free
 */
void mutt_enter_state_free(struct EnterState **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct EnterState *es = *ptr;

  FREE(&es->wbuf);
  FREE(ptr);
}
