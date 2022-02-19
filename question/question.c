/**
 * @file
 * Ask the user a question
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
 * @page question_question Ask the user a question
 *
 * Ask the user a question
 */

#include "config.h"
#include <ctype.h>
#include <langinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"

/**
 * mutt_multi_choice - Offer the user a multiple choice question
 * @param prompt  Message prompt
 * @param letters Allowable selection keys
 * @retval >=0 0-based user selection
 * @retval  -1 Selection aborted
 */
int mutt_multi_choice(const char *prompt, const char *letters)
{
  struct MuttWindow *win = msgwin_get_window();
  if (!win)
    return -1;

  struct KeyEvent ch;
  int choice;
  bool redraw = true;
  int prompt_lines = 1;

  const bool opt_cols = simple_color_is_set(MT_COLOR_OPTIONS);

  struct MuttWindow *old_focus = window_set_focus(win);
  window_redraw(NULL);
  while (true)
  {
    if (redraw || SigWinch)
    {
      redraw = false;
      if (SigWinch)
      {
        SigWinch = false;
        mutt_resize_screen();
        clearok(stdscr, true);
        window_redraw(NULL);
      }
      if (win->state.cols)
      {
        int width = mutt_strwidth(prompt) + 2; // + '?' + space
        /* If we're going to colour the options,
         * make an assumption about the modified prompt size. */
        if (opt_cols)
          width -= 2 * mutt_str_len(letters);

        prompt_lines = (width + win->state.cols - 1) / win->state.cols;
        prompt_lines = MAX(1, MIN(3, prompt_lines));
      }
      if (prompt_lines != win->state.rows)
      {
        msgwin_set_height(prompt_lines);
        window_redraw(NULL);
      }

      mutt_window_move(win, 0, 0);

      if (opt_cols)
      {
        char *cur = NULL;

        while ((cur = strchr(prompt, '(')))
        {
          // write the part between prompt and cur using MT_COLOR_PROMPT
          mutt_curses_set_color_by_id(MT_COLOR_PROMPT);
          mutt_window_addnstr(win, prompt, cur - prompt);

          if (isalnum(cur[1]) && (cur[2] == ')'))
          {
            // we have a single letter within parentheses
            mutt_curses_set_color_by_id(MT_COLOR_OPTIONS);
            mutt_window_addch(win, cur[1]);
            prompt = cur + 3;
          }
          else
          {
            // we have a parenthesis followed by something else
            mutt_window_addch(win, cur[0]);
            prompt = cur + 1;
          }
        }
      }

      mutt_curses_set_color_by_id(MT_COLOR_PROMPT);
      mutt_window_addstr(win, prompt);
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

      mutt_window_addch(win, ' ');
      mutt_window_clrtoeol(win);
    }

    mutt_refresh();
    /* SigWinch is not processed unless timeout is set */
    mutt_getch_timeout(30 * 1000);
    ch = mutt_getch();
    mutt_getch_timeout(-1);
    if (ch.ch == -2) // Timeout
      continue;
    /* (ch.ch == 0) is technically possible.  Treat the same as < 0 (abort) */
    if ((ch.ch <= 0) || CI_is_return(ch.ch))
    {
      choice = -1;
      break;
    }
    else
    {
      char *p = strchr(letters, ch.ch);
      if (p)
      {
        choice = p - letters + 1;
        break;
      }
      else if ((ch.ch <= '9') && (ch.ch > '0'))
      {
        choice = ch.ch - '0';
        if (choice <= mutt_str_len(letters))
          break;
      }
    }
    mutt_beep(false);
  }
  if (win->state.rows == 1)
  {
    mutt_window_clearline(win, 0);
  }
  else
  {
    msgwin_set_height(1);
    window_redraw(NULL);
  }
  mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  window_set_focus(old_focus);
  mutt_refresh();
  return choice;
}

/**
 * mutt_yesorno - Ask the user a Yes/No question
 * @param msg Prompt
 * @param def Default answer, #MUTT_YES or #MUTT_NO (see #QuadOption)
 * @retval num Selection made, see #QuadOption
 */
enum QuadOption mutt_yesorno(const char *msg, enum QuadOption def)
{
  struct MuttWindow *win = msgwin_get_window();
  if (!win)
    return MUTT_ABORT;

  struct KeyEvent ch;
  char *answer_string = NULL;
  int answer_string_wid, msg_wid;
  size_t trunc_msg_len;
  bool redraw = true;
  int prompt_lines = 1;
  char answer[2] = { 0 };

  char *yes = N_("yes");
  char *no = N_("no");
  char *trans_yes = _(yes);
  char *trans_no = _(no);

  char *expr = NULL;
  regex_t reyes = { 0 };
  regex_t reno = { 0 };

  bool reyes_ok = (expr = nl_langinfo(YESEXPR)) && (expr[0] == '^') &&
                  (REG_COMP(&reyes, expr, REG_NOSUB) == 0);
  bool reno_ok = (expr = nl_langinfo(NOEXPR)) && (expr[0] == '^') &&
                 (REG_COMP(&reno, expr, REG_NOSUB) == 0);

  if ((yes != trans_yes) && (no != trans_no) && reyes_ok && reno_ok)
  {
    // If all parts of the translation succeeded...
    yes = trans_yes;
    no = trans_no;
  }
  else
  {
    // otherwise, fallback to English
    if (reyes_ok)
    {
      regfree(&reyes);
      reyes_ok = false;
    }
    if (reno_ok)
    {
      regfree(&reno);
      reno_ok = false;
    }
  }

  /* In order to prevent the default answer to the question to wrapped
   * around the screen in the event the question is wider than the screen,
   * ensure there is enough room for the answer and truncate the question
   * to fit.  */
  mutt_str_asprintf(&answer_string, " ([%s]/%s): ", (def == MUTT_YES) ? yes : no,
                    (def == MUTT_YES) ? no : yes);
  answer_string_wid = mutt_strwidth(answer_string);
  msg_wid = mutt_strwidth(msg);

  struct MuttWindow *old_focus = window_set_focus(win);
  window_redraw(NULL);
  while (true)
  {
    if (redraw || SigWinch)
    {
      redraw = false;
      if (SigWinch)
      {
        SigWinch = false;
        mutt_resize_screen();
        clearok(stdscr, true);
        window_redraw(NULL);
      }
      if (win->state.cols)
      {
        prompt_lines =
            (msg_wid + answer_string_wid + win->state.cols - 1) / win->state.cols;
        prompt_lines = MAX(1, MIN(3, prompt_lines));
      }
      if (prompt_lines != win->state.rows)
      {
        msgwin_set_height(prompt_lines);
        window_redraw(NULL);
      }

      /* maxlen here is sort of arbitrary, so pick a reasonable upper bound */
      trunc_msg_len = mutt_wstr_trunc(
          msg, (size_t) 4 * prompt_lines * win->state.cols,
          ((size_t) prompt_lines * win->state.cols) - answer_string_wid, NULL);

      mutt_window_move(win, 0, 0);
      mutt_curses_set_color_by_id(MT_COLOR_PROMPT);
      mutt_window_addnstr(win, msg, trunc_msg_len);
      mutt_window_addstr(win, answer_string);
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
      mutt_window_clrtoeol(win);
    }

    mutt_refresh();
    /* SigWinch is not processed unless timeout is set */
    mutt_getch_timeout(30 * 1000);
    ch = mutt_getch();
    mutt_getch_timeout(-1);
    if (ch.ch == -2) // Timeout
      continue;
    if (CI_is_return(ch.ch))
      break;
    if (ch.ch < 0)
    {
      def = MUTT_ABORT;
      break;
    }

    answer[0] = ch.ch;
    if (reyes_ok ? (regexec(&reyes, answer, 0, 0, 0) == 0) : (tolower(ch.ch) == 'y'))
    {
      def = MUTT_YES;
      break;
    }
    else if (reno_ok ? (regexec(&reno, answer, 0, 0, 0) == 0) : (tolower(ch.ch) == 'n'))
    {
      def = MUTT_NO;
      break;
    }
    else
    {
      mutt_beep(false);
    }
  }
  window_set_focus(old_focus);

  FREE(&answer_string);

  if (reyes_ok)
    regfree(&reyes);
  if (reno_ok)
    regfree(&reno);

  if (win->state.rows == 1)
  {
    mutt_window_clearline(win, 0);
  }
  else
  {
    msgwin_set_height(1);
    window_redraw(NULL);
  }

  if (def == MUTT_ABORT)
  {
    /* when the users cancels with ^G, clear the message stored with
     * mutt_message() so it isn't displayed when the screen is refreshed. */
    mutt_clear_error();
  }
  else
  {
    mutt_window_addstr(win, (char *) ((def == MUTT_YES) ? yes : no));
    mutt_refresh();
  }
  return def;
}

/**
 * query_quadoption - Ask the user a quad-question
 * @param opt    Option to use
 * @param prompt Message to show to the user
 * @retval #QuadOption Result, e.g. #MUTT_NO
 */
enum QuadOption query_quadoption(enum QuadOption opt, const char *prompt)
{
  switch (opt)
  {
    case MUTT_YES:
    case MUTT_NO:
      return opt;

    default:
      opt = mutt_yesorno(prompt, (opt == MUTT_ASKYES) ? MUTT_YES : MUTT_NO);
      msgwin_clear_text();
      return opt;
  }

  /* not reached */
}
