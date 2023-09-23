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
#include <assert.h>
#include <ctype.h>
#include <langinfo.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "gui/lib.h"
#include "color/lib.h"
#include "key/lib.h"

/**
 * mw_multi_choice - Offer the user a multiple choice question - @ingroup gui_mw
 * @param prompt  Message prompt
 * @param letters Allowable selection keys
 * @retval >=1 1-based user selection
 * @retval  -1 Selection aborted
 *
 * This function uses a message window.
 *
 * Ask the user a multiple-choice question, using shortcut letters, e.g.
 * `PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, s/(m)ime or (c)lear?`
 *
 * Colours:
 * - Question:  `color prompt`
 * - Shortcuts: `color options`
 */
int mw_multi_choice(const char *prompt, const char *letters)
{
  struct MuttWindow *win = msgwin_new(true);
  if (!win)
    return -1;

  int choice = 0;

  const struct AttrColor *ac_opts = NULL;
  if (simple_color_is_set(MT_COLOR_OPTIONS))
  {
    const struct AttrColor *ac_base = simple_color_get(MT_COLOR_NORMAL);
    ac_base = merged_color_overlay(ac_base, simple_color_get(MT_COLOR_PROMPT));

    ac_opts = simple_color_get(MT_COLOR_OPTIONS);
    ac_opts = merged_color_overlay(ac_base, ac_opts);
  }

  const struct AttrColor *ac_normal = simple_color_get(MT_COLOR_NORMAL);
  const struct AttrColor *ac_prompt = simple_color_get(MT_COLOR_PROMPT);

  if (ac_opts)
  {
    char *cur = NULL;

    while ((cur = strchr(prompt, '(')))
    {
      // write the part between prompt and cur using MT_COLOR_PROMPT
      msgwin_add_text_n(win, prompt, cur - prompt, ac_prompt);

      if (isalnum(cur[1]) && (cur[2] == ')'))
      {
        // we have a single letter within parentheses - MT_COLOR_OPTIONS
        msgwin_add_text_n(win, cur + 1, 1, ac_opts);
        prompt = cur + 3;
      }
      else
      {
        // we have a parenthesis followed by something else
        msgwin_add_text_n(win, cur, 1, ac_prompt);
        prompt = cur + 1;
      }
    }
  }

  msgwin_add_text(win, prompt, ac_prompt);
  msgwin_add_text(win, " ", ac_normal);

  msgcont_push_window(win);
  struct MuttWindow *old_focus = window_set_focus(win);
  window_redraw(win);

  // ---------------------------------------------------------------------------
  // Event Loop
  struct KeyEvent event = { 0, OP_NULL };
  while (true)
  {
    event = mutt_getch(GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "mw_multi_choice: EVENT(%d,%d)\n", event.ch, event.op);

    if (event.op == OP_REPAINT)
      window_redraw(NULL);

    if ((event.op == OP_TIMEOUT) || (event.op == OP_REPAINT))
      continue;

    if ((event.op == OP_ABORT) || key_is_return(event.ch))
    {
      choice = -1;
      break;
    }

    char *p = strchr(letters, event.ch);
    if (p)
    {
      choice = p - letters + 1;
      break;
    }

    if ((event.ch > '0') && (event.ch <= '9'))
    {
      choice = event.ch - '0';
      if (choice <= mutt_str_len(letters))
        break;
    }
  }
  // ---------------------------------------------------------------------------

  win = msgcont_pop_window();
  window_set_focus(old_focus);
  mutt_window_free(&win);

  return choice;
}

/**
 * mw_yesorno - Ask the user a Yes/No question offering help - @ingroup gui_mw
 * @param prompt Prompt
 * @param def    Default answer, e.g. #MUTT_YES
 * @param cdef   Config definition for help
 * @retval enum #QuadOption, Selection made
 *
 * This function uses a message window.
 *
 * Ask the user a yes/no question, using shortcut letters, e.g.
 * `Quit NeoMutt? ([yes]/no):`
 *
 * This question can be answered using locale-dependent letters, e.g.
 * - English, `[+1yY]` or `[-0nN]`
 * - Serbian, `[+1yYdDДд]` or `[-0nNНн]`
 *
 * If a config variable (cdef) is given, then help is offered.
 * The options change to: `([yes]/no/?)`
 *
 * Pressing '?' will show the name and one-line description of the config variable.
 * Additionally, if `$help` is set, a link to the config's documentation is shown.
 */
static enum QuadOption mw_yesorno(const char *prompt, enum QuadOption def,
                                  struct ConfigDef *cdef)
{
  struct MuttWindow *win = msgwin_new(true);
  if (!win)
    return MUTT_ABORT;

  char *yes = N_("yes");
  char *no = N_("no");
  char *trans_yes = _(yes);
  char *trans_no = _(no);

  regex_t reyes = { 0 };
  regex_t reno = { 0 };

  bool reyes_ok = false;
  bool reno_ok = false;

#ifdef OpenBSD
  /* OpenBSD only supports locale C and UTF-8
   * so there is no suitable base system's locale identification
   * Remove this code immediately if this situation changes! */
  char rexyes[16] = "^[+1YyYy]";
  rexyes[6] = toupper(trans_yes[0]);
  rexyes[7] = tolower(trans_yes[0]);

  char rexno[16] = "^[-0NnNn]";
  rexno[6] = toupper(trans_no[0]);
  rexno[7] = tolower(trans_no[0]);

  if (REG_COMP(&reyes, rexyes, REG_NOSUB) == 0)
    reyes_ok = true;

  if (REG_COMP(&reno, rexno, REG_NOSUB) == 0)
    reno_ok = true;

#else
  char *expr = NULL;
  reyes_ok = (expr = nl_langinfo(YESEXPR)) && (expr[0] == '^') &&
             (REG_COMP(&reyes, expr, REG_NOSUB) == 0);
  reno_ok = (expr = nl_langinfo(NOEXPR)) && (expr[0] == '^') &&
            (REG_COMP(&reno, expr, REG_NOSUB) == 0);
#endif

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

  bool show_help_prompt = cdef;

  struct Buffer *text = buf_pool_get();
  buf_printf(text, "%s ([%s]/%s%s): ", prompt, (def == MUTT_YES) ? yes : no,
             (def == MUTT_YES) ? no : yes, show_help_prompt ? "/?" : "");

  msgwin_set_text(win, buf_string(text), MT_COLOR_PROMPT);
  msgcont_push_window(win);
  struct MuttWindow *old_focus = window_set_focus(win);

  struct KeyEvent event = { 0, OP_NULL };
  window_redraw(NULL);
  while (true)
  {
    event = mutt_getch(GETCH_NO_FLAGS);
    if ((event.op == OP_TIMEOUT) || (event.op == OP_REPAINT))
    {
      window_redraw(NULL);
      mutt_refresh();
      continue;
    }

    if (key_is_return(event.ch))
      break; // Do nothing, use default

    if (event.op == OP_ABORT)
    {
      def = MUTT_ABORT;
      break;
    }

    char answer[4] = { 0 };
    answer[0] = event.ch;
    if (reyes_ok ? (regexec(&reyes, answer, 0, 0, 0) == 0) : (tolower(event.ch) == 'y'))
    {
      def = MUTT_YES;
      break;
    }
    if (reno_ok ? (regexec(&reno, answer, 0, 0, 0) == 0) : (tolower(event.ch) == 'n'))
    {
      def = MUTT_NO;
      break;
    }
    if (show_help_prompt && (event.ch == '?'))
    {
      show_help_prompt = false;
      msgwin_clear_text(win);
      buf_printf(text, "$%s - %s\n", cdef->name, cdef->docs);

      char hyphen[128] = { 0 };
      mutt_str_hyphenate(hyphen, sizeof(hyphen), cdef->name);
      buf_add_printf(text, "https://neomutt.org/guide/reference#%s\n", hyphen);

      msgwin_add_text(win, buf_string(text), simple_color_get(MT_COLOR_NORMAL));

      buf_printf(text, "%s ([%s]/%s): ", prompt, (def == MUTT_YES) ? yes : no,
                 (def == MUTT_YES) ? no : yes);
      msgwin_add_text(win, buf_string(text), simple_color_get(MT_COLOR_PROMPT));
      msgwin_add_text(win, NULL, NULL);

      window_redraw(NULL);
      mutt_refresh();
    }

    mutt_beep(false);
  }

  win = msgcont_pop_window();
  window_set_focus(old_focus);
  mutt_window_free(&win);

  if (reyes_ok)
    regfree(&reyes);
  if (reno_ok)
    regfree(&reno);

  buf_pool_release(&text);
  return def;
}

/**
 * query_yesorno - Ask the user a Yes/No question
 * @param prompt Prompt
 * @param def Default answer, e.g. #MUTT_YES
 * @retval enum #QuadOption, Selection made
 *
 * Wrapper for mw_yesorno().
 */
enum QuadOption query_yesorno(const char *prompt, enum QuadOption def)
{
  return mw_yesorno(prompt, def, NULL);
}

/**
 * query_yesorno_help - Ask the user a Yes/No question offering help
 * @param prompt Prompt
 * @param def    Default answer, e.g. #MUTT_YES
 * @param sub    Config Subset
 * @param name   Name of controlling config variable
 * @retval enum #QuadOption, Selection made
 *
 * Wrapper for mw_yesorno().
 */
enum QuadOption query_yesorno_help(const char *prompt, enum QuadOption def,
                                   struct ConfigSubset *sub, const char *name)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  struct HashElem *he_base = cs_get_base(he);
  assert(DTYPE(he_base->type) == DT_BOOL);

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  assert(value != INT_MIN);

  struct ConfigDef *cdef = he_base->data;
  return mw_yesorno(prompt, def, cdef);
}

/**
 * query_quadoption - Ask the user a quad-question
 * @param prompt Message to show to the user
 * @param sub    Config Subset
 * @param name   Name of controlling config variable
 * @retval #QuadOption Result, e.g. #MUTT_NO
 *
 * If the config variable is set to 'yes' or 'no', the function returns immediately.
 * Otherwise, the job is delegated to mw_yesorno().
 */
enum QuadOption query_quadoption(const char *prompt, struct ConfigSubset *sub, const char *name)
{
  struct HashElem *he = cs_subset_create_inheritance(sub, name);
  struct HashElem *he_base = cs_get_base(he);
  assert(DTYPE(he_base->type) == DT_QUAD);

  intptr_t value = cs_subset_he_native_get(sub, he, NULL);
  assert(value != INT_MIN);

  if ((value == MUTT_YES) || (value == MUTT_NO))
    return value;

  struct ConfigDef *cdef = he_base->data;
  enum QuadOption def = (value == MUTT_ASKYES) ? MUTT_YES : MUTT_NO;
  return mw_yesorno(prompt, def, cdef);
}
