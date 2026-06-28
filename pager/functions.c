/**
 * @file
 * Pager functions
 *
 * @authors
 * Copyright (C) 2021-2026 Richard Russon <rich@flatcap.org>
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
 * @page pager_functions Pager functions
 *
 * Pager functions
 */

#include "config.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "functions.h"
#include "lib.h"
#include "attach/lib.h"
#include "browser/lib.h"
#include "color/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "sidebar/lib.h"
#include "display.h"
#include "module_data.h"
#include "muttlib.h"
#include "private_data.h"

/// Error message for unavailable functions
static const char *Not_available_in_this_menu = N_("Not available in this menu");

static int op_pager_search_next(struct PagerFunctionData *fdata, const struct KeyEvent *event);

// clang-format off
/**
 * OpPager - Functions for the Pager Menu
 */
static const struct MenuFuncOp OpPager[] = { /* map: pager */
  { "search-toggle",                 OP_SEARCH_TOGGLE },
  { "skip-headers",                  OP_PAGER_SKIP_HEADERS },
  { "skip-quoted",                   OP_PAGER_SKIP_QUOTED },
  { "toggle-quoted",                 OP_PAGER_HIDE_QUOTED },

  // Deprecated
  { "bottom",                        OP_SELECT_LAST_ENTRY,   MFF_DEPRECATED },
  { "buffy-list",                    OP_MAILBOX_LIST,        MFF_DEPRECATED },
  { "error-history",                 OP_DISPLAY_LOG,         MFF_DEPRECATED },
  { "mark-as-new",                   OP_TOGGLE_NEW,          MFF_DEPRECATED },
  { "tag-message",                   OP_TOGGLE_TAG,          MFF_DEPRECATED },
  { "top",                           OP_SELECT_FIRST_ENTRY,  MFF_DEPRECATED },
  { NULL, 0 },
};

/**
 * PagerDefaultBindings - Key bindings for the Pager Menu
 */
static const struct MenuOpSeq PagerDefaultBindings[] = { /* map: pager */
  { OP_EXIT,                               "q" },
  { OP_MAIN_NEXT_UNDELETED,                "<right>" },
  { OP_MAIN_PREV_UNDELETED,                "<left>" },
  { OP_PAGER_HIDE_QUOTED,                  "T" },
  { OP_PAGER_SKIP_HEADERS,                 "H" },
  { OP_PAGER_SKIP_QUOTED,                  "S" },
  { OP_QUIT,                               "Q" },
  { OP_SCROLL_LINE_DOWN,                   "<keypadenter>" },
  { OP_SCROLL_LINE_DOWN,                   "\n" },             // <Enter>
  { OP_SCROLL_LINE_DOWN,                   "\r" },             // <Return>
  { OP_SCROLL_LINE_UP,                     "<backspace>" },
  { OP_SCROLL_PAGE_DOWN,                   " " },              // <Space>
  { OP_SCROLL_PAGE_UP,                     "-" },
  { OP_SEARCH_TOGGLE,                      "\\" },             // <Backslash>
  { OP_SELECT_FIRST_ENTRY,                 "^" },
  { 0, NULL },
};
// clang-format on

/**
 * pager_init_keys - Initialise the Pager Keybindings - Implements ::init_keys_api
 */
void pager_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic)
{
  struct MenuDefinition *md = NULL;
  struct SubMenu *sm_pager = NULL;
  struct SubMenu *sm_index = index_get_submenu();
  struct SubMenu *sm_sidebar = sidebar_get_submenu();

  sm_pager = km_register_submenu(OpPager);
  md = km_register_menu(MENU_PAGER, "pager");
  km_menu_add_submenu(md, sm_pager);
  km_menu_add_submenu(md, sm_index);
  km_menu_add_submenu(md, sm_sidebar);
  km_menu_add_submenu(md, sm_generic);
  km_menu_add_bindings(md, PagerDefaultBindings);

  struct PagerModuleData *mod_data = neomutt_get_module_data(n, MODULE_ID_PAGER);
  ASSERT(mod_data);
  mod_data->md_pager = md;
}

/**
 * assert_pager_mode - Check that pager is in correct mode
 * @param test   Test condition
 * @retval true  Expected mode is set
 * @retval false Pager is is some other mode
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_pager_mode(bool test)
{
  if (test)
    return true;

  mutt_flushinp();
  mutt_error("%s", _(Not_available_in_this_menu));
  return false;
}

/**
 * up_n_lines - Reposition the pager's view up by n lines
 * @param nlines Number of lines to move
 * @param info   Line info array
 * @param cur    Current line number
 * @param hiding true if lines have been hidden
 * @retval num New current line number
 */
static int up_n_lines(int nlines, struct Line *info, int cur, bool hiding)
{
  while ((cur > 0) && (nlines > 0))
  {
    cur--;
    if (!hiding || !COLOR_QUOTED(info[cur].cid))
      nlines--;
  }

  return cur;
}

/**
 * down_n_lines - Reposition the pager's view down by n lines
 * @param nlines Number of lines to move
 * @param info   Line info array
 * @param cur    Current line number
 * @param max    Number of lines used
 * @param hiding true if lines have been hidden
 * @retval num New current line number
 */
static int down_n_lines(int nlines, struct Line *info, int cur, int max, bool hiding)
{
  while ((cur < max) && (nlines > 0))
  {
    cur++;
    if ((cur < max) && (!hiding || !COLOR_QUOTED(info[cur].cid)))
      nlines--;
  }

  if (cur > max)
    cur = max;

  return cur;
}

/**
 * jump_to_bottom - Make sure the bottom line is displayed
 * @param priv   Private Pager data
 * @param pview PagerView
 * @retval true Something changed
 * @retval false Bottom was already displayed
 */
bool jump_to_bottom(struct PagerPrivateData *priv, struct PagerView *pview)
{
  if (!(priv->lines[priv->cur_line].offset < (priv->st.st_size - 1)))
  {
    return false;
  }

  int line_num = priv->cur_line;
  /* make sure the types are defined to the end of file */
  while (display_line(priv->fp, &priv->bytes_read, &priv->lines, line_num,
                      &priv->lines_used, &priv->lines_max,
                      priv->has_types | (pview->flags & MUTT_PAGER_NOWRAP),
                      &priv->quote_list, &priv->q_level, &priv->force_redraw,
                      &priv->search_re, priv->pview->win_pager, &priv->ansi_list) == 0)
  {
    line_num++;
  }
  priv->top_line = up_n_lines(priv->pview->win_pager->state.rows, priv->lines,
                              priv->lines_used, priv->hide_quoted);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return true;
}

// -----------------------------------------------------------------------------

/**
 * op_select_first_entry - Move to the first entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_select_first_entry(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->top_line == 0)
  {
    mutt_message(_("Top of message is shown"));
    return FR_ERROR;
  }
  else
  {
    priv->top_line = 0;
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }

  return FR_SUCCESS;
}

/**
 * op_select_last_entry - Move to the last entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_select_last_entry(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (!jump_to_bottom(priv, priv->pview))
  {
    mutt_message(_("Bottom of message is shown"));
    return FR_ERROR;
  }

  return FR_SUCCESS;
}

/**
 * op_scroll_half_down - Scroll down 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_scroll_half_down(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  const bool c_pager_stop = cs_subset_bool(fdata->n->sub, "pager_stop");
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    int rows = priv->pview->win_pager->state.rows;
    if (event->count > 0)
    {
      int advance = event->count * (rows / 2);
      priv->top_line = down_n_lines(advance, priv->lines, priv->top_line,
                                    priv->lines_used, priv->hide_quoted);
    }
    else
    {
      priv->top_line = up_n_lines(rows / 2, priv->lines, priv->cur_line, priv->hide_quoted);
    }
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (c_pager_stop)
  {
    /* emulate "less -q" and don't go on to the next message. */
    if (event->count == 0)
    {
      mutt_message(_("Bottom of message is shown"));
      return FR_ERROR;
    }
  }
  else
  {
    /* end of the current message, so display the next message. */
    index_next_undeleted(priv->pview->win_index);
  }
  return FR_SUCCESS;
}

/**
 * op_scroll_half_up - Scroll up 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_scroll_half_up(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  const int old_top_line = priv->top_line;
  if (priv->top_line)
  {
    int rows = priv->pview->win_pager->state.rows;
    int n = MAX(event->count, 1) * (rows / 2 + rows % 2);
    priv->top_line = up_n_lines(n, priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (event->count == 0)
  {
    mutt_message(_("Top of message is shown"));
  }
  return (old_top_line == 0) ? FR_ERROR : FR_SUCCESS;
}

/**
 * op_pager_hide_quoted - Toggle display of quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_hide_quoted(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (!priv->has_types)
    return FR_NO_ACTION;

  priv->hide_quoted ^= MUTT_HIDE;
  if (priv->hide_quoted && COLOR_QUOTED(priv->lines[priv->top_line].cid))
  {
    priv->top_line = up_n_lines(1, priv->lines, priv->top_line, priv->hide_quoted);
  }
  else
  {
    pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  }
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return FR_SUCCESS;
}

/**
 * op_scroll_line_down - Scroll down one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_scroll_line_down(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    int n = MAX(event->count, 1);
    priv->top_line = down_n_lines(n, priv->lines, priv->top_line,
                                  priv->lines_used, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (event->count == 0)
  {
    mutt_message(_("Bottom of message is shown"));
    return FR_ERROR;
  }
  return FR_SUCCESS;
}

/**
 * op_scroll_page_down - Move to the next page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_scroll_page_down(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  const bool c_pager_stop = cs_subset_bool(fdata->n->sub, "pager_stop");
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    const short c_pager_context = cs_subset_number(fdata->n->sub, "pager_context");
    if (event->count > 0)
    {
      int advance = event->count * (priv->pview->win_pager->state.rows - c_pager_context);
      priv->top_line = down_n_lines(advance, priv->lines, priv->top_line,
                                    priv->lines_used, priv->hide_quoted);
    }
    else
    {
      priv->top_line = up_n_lines(c_pager_context, priv->lines, priv->cur_line,
                                  priv->hide_quoted);
    }
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (c_pager_stop)
  {
    /* emulate "less -q" and don't go on to the next message. */
    if (event->count == 0)
    {
      mutt_message(_("Bottom of message is shown"));
      return FR_ERROR;
    }
  }
  else
  {
    /* end of the current message, so display the next message. */
    index_next_undeleted(priv->pview->win_index);
  }
  return FR_SUCCESS;
}

/**
 * op_scroll_line_up - Scroll up one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_scroll_line_up(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->top_line)
  {
    int n = MAX(event->count, 1);
    priv->top_line = up_n_lines(n, priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (event->count == 0)
  {
    mutt_message(_("Top of message is shown"));
    return FR_ERROR;
  }
  return FR_SUCCESS;
}

/**
 * op_scroll_page_up - Move to the previous page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_scroll_page_up(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->top_line == 0)
  {
    if (event->count == 0)
    {
      mutt_message(_("Top of message is shown"));
      return FR_ERROR;
    }
  }
  else
  {
    const short c_pager_context = cs_subset_number(fdata->n->sub, "pager_context");
    int n = MAX(event->count, 1) * (priv->pview->win_pager->state.rows - c_pager_context);
    priv->top_line = up_n_lines(n, priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  return FR_SUCCESS;
}

/**
 * op_pager_search - Search for a regular expression - Implements ::pager_function_t - @ingroup pager_function_api
 *
 * This function handles:
 * - OP_SEARCH_BACKWARD
 * - OP_SEARCH_FORWARD
 */
static int op_pager_search(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  struct PagerView *pview = priv->pview;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = buf_pool_get();

  buf_strcpy(buf, priv->search_str);
  const int op = event->op;
  if (mw_get_field(((op == OP_SEARCH_FORWARD) || (op == OP_SEARCH_NEXT)) ?
                       _("Search for: ") :
                       _("Reverse search for: "),
                   buf, MUTT_COMP_CLEAR, HC_PATTERN, &CompletePatternOps, NULL) != 0)
  {
    goto done;
  }

  if (mutt_str_equal(buf_string(buf), priv->search_str))
  {
    if (priv->search_compiled)
    {
      struct KeyEvent event_s = { 0, OP_NULL };

      /* do an implicit search-next */
      if (op == OP_SEARCH_FORWARD)
        event_s.op = OP_SEARCH_NEXT;
      else
        event_s.op = OP_SEARCH_PREVIOUS;

      priv->wrapped = false;
      op_pager_search_next(fdata, &event_s);
    }
  }

  if (buf_is_empty(buf))
    goto done;

  mutt_str_copy(priv->search_str, buf_string(buf), sizeof(priv->search_str));

  /* leave search_back alone if op == OP_SEARCH_NEXT */
  if (op == OP_SEARCH_FORWARD)
    priv->search_back = false;
  else if (op == OP_SEARCH_BACKWARD)
    priv->search_back = true;

  if (priv->search_compiled)
  {
    regfree(&priv->search_re);
    for (size_t i = 0; i < priv->lines_used; i++)
    {
      FREE(&(priv->lines[i].search));
      priv->lines[i].search_arr_size = -1;
    }
  }

  uint16_t rflags = mutt_mb_is_lower(priv->search_str) ? REG_ICASE : 0;
  int err = REG_COMP(&priv->search_re, priv->search_str, REG_NEWLINE | rflags);
  if (err != 0)
  {
    regerror(err, &priv->search_re, buf->data, buf->dsize);
    mutt_error("%s", buf_string(buf));
    for (size_t i = 0; i < priv->lines_max; i++)
    {
      /* cleanup */
      FREE(&(priv->lines[i].search));
      priv->lines[i].search_arr_size = -1;
    }
    priv->search_flag = 0;
    priv->search_compiled = false;
    rc = FR_ERROR;
  }
  else
  {
    priv->search_compiled = true;
    /* update the search pointers */
    int line_num = 0;
    while (display_line(priv->fp, &priv->bytes_read, &priv->lines, line_num,
                        &priv->lines_used, &priv->lines_max,
                        MUTT_SEARCH | (pview->flags & MUTT_PAGER_NOWRAP) | priv->has_types,
                        &priv->quote_list, &priv->q_level, &priv->force_redraw,
                        &priv->search_re, priv->pview->win_pager, &priv->ansi_list) == 0)
    {
      line_num++;
    }

    if (priv->search_back)
    {
      /* searching backward */
      int i;
      for (i = priv->top_line; i >= 0; i--)
      {
        if ((!priv->hide_quoted || !COLOR_QUOTED(priv->lines[i].cid)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      if (i >= 0)
        priv->top_line = i;
    }
    else
    {
      /* searching forward */
      int i;
      for (i = priv->top_line; i < priv->lines_used; i++)
      {
        if ((!priv->hide_quoted || !COLOR_QUOTED(priv->lines[i].cid)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      if (i < priv->lines_used)
        priv->top_line = i;
    }

    if (priv->lines[priv->top_line].search_arr_size == 0)
    {
      priv->search_flag = 0;
      mutt_error(_("Not found"));
      rc = FR_ERROR;
    }
    else
    {
      const short c_search_context = cs_subset_number(fdata->n->sub, "search_context");
      priv->search_flag = MUTT_SEARCH;
      /* give some context for search results */
      if (c_search_context < priv->pview->win_pager->state.rows)
        priv->searchctx = c_search_context;
      else
        priv->searchctx = 0;
      if (priv->top_line - priv->searchctx > 0)
        priv->top_line -= priv->searchctx;
      rc = FR_SUCCESS;
    }
  }
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);

done:
  buf_pool_release(&buf);
  return rc;
}

/**
 * op_pager_search_next - Search for next match - Implements ::pager_function_t - @ingroup pager_function_api
 *
 * This function handles:
 * - OP_SEARCH_NEXT
 * - OP_SEARCH_PREVIOUS
 */
static int op_pager_search_next(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->search_compiled)
  {
    const short c_search_context = cs_subset_number(fdata->n->sub, "search_context");
    bool found = false;
    priv->wrapped = false;

    if (c_search_context < priv->pview->win_pager->state.rows)
      priv->searchctx = c_search_context;
    else
      priv->searchctx = 0;

    const int op = event->op;

  search_next:
    if ((!priv->search_back && (op == OP_SEARCH_NEXT)) ||
        (priv->search_back && (op == OP_SEARCH_PREVIOUS)))
    {
      /* searching forward */
      int i;
      for (i = priv->wrapped ? 0 : priv->top_line + priv->searchctx + 1;
           i < priv->lines_used; i++)
      {
        if ((!priv->hide_quoted || !COLOR_QUOTED(priv->lines[i].cid)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      const bool c_wrap_search = cs_subset_bool(fdata->n->sub, "wrap_search");
      if (i < priv->lines_used)
      {
        priv->top_line = i;
        found = true;
      }
      else if (priv->wrapped || !c_wrap_search)
      {
        mutt_error(_("Not found"));
      }
      else
      {
        mutt_message(_("Search wrapped to top"));
        priv->wrapped = true;
        goto search_next;
      }
    }
    else
    {
      /* searching backward */
      int i;
      for (i = priv->wrapped ? priv->lines_used : priv->top_line + priv->searchctx - 1;
           i >= 0; i--)
      {
        if ((!priv->hide_quoted ||
             (priv->has_types && !COLOR_QUOTED(priv->lines[i].cid))) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      const bool c_wrap_search = cs_subset_bool(fdata->n->sub, "wrap_search");
      if (i >= 0)
      {
        priv->top_line = i;
        found = true;
      }
      else if (priv->wrapped || !c_wrap_search)
      {
        mutt_error(_("Not found"));
      }
      else
      {
        mutt_message(_("Search wrapped to bottom"));
        priv->wrapped = true;
        goto search_next;
      }
    }

    if (!found)
      return FR_ERROR;

    if (priv->lines[priv->top_line].search_arr_size > 0)
    {
      priv->search_flag = MUTT_SEARCH;
      /* give some context for search results */
      if (priv->top_line - priv->searchctx > 0)
        priv->top_line -= priv->searchctx;
    }

    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
    return FR_SUCCESS;
  }

  /* no previous search pattern */
  return op_pager_search(fdata, event);
}

/**
 * op_pager_skip_headers - Jump to first line after headers - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_skip_headers(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  struct PagerView *pview = priv->pview;

  if (!priv->has_types)
    return FR_NO_ACTION;

  int rc = 0;
  int new_topline = 0;

  while (((new_topline < priv->lines_used) ||
          (0 == (rc = display_line(priv->fp, &priv->bytes_read, &priv->lines,
                                   new_topline, &priv->lines_used, &priv->lines_max,
                                   MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP), &priv->quote_list,
                                   &priv->q_level, &priv->force_redraw, &priv->search_re,
                                   priv->pview->win_pager, &priv->ansi_list)))) &&
         color_is_header(priv->lines[new_topline].cid))
  {
    new_topline++;
  }

  if (rc < 0)
  {
    /* L10N: Displayed if <skip-headers> is invoked in the pager, but
       there is no text past the headers.
       (I don't think this is actually possible in Mutt's code, but
       display some kind of message in case it somehow occurs.) */
    mutt_warning(_("No text past headers"));
    return FR_ERROR;
  }
  priv->top_line = new_topline;
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return FR_SUCCESS;
}

/**
 * pager_skip_quoted_once - Skip to next unquoted block
 * @param priv  Pager private data
 * @param fdata Pager function data
 * @retval 0 Success
 * @retval -1 No more unquoted text
 */
static int pager_skip_quoted_once(struct PagerPrivateData *priv, struct PagerFunctionData *fdata)
{
  struct PagerView *pview = priv->pview;
  const short c_pager_skip_quoted_context = cs_subset_number(fdata->n->sub, "pager_skip_quoted_context");
  int rc = 0;
  int new_topline = priv->top_line;
  int num_quoted = 0;

  /* In a header? Skip all the email headers, and done */
  if (color_is_header(priv->lines[new_topline].cid))
  {
    while (((new_topline < priv->lines_used) ||
            (0 == (rc = display_line(priv->fp, &priv->bytes_read, &priv->lines,
                                     new_topline, &priv->lines_used, &priv->lines_max,
                                     MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP), &priv->quote_list,
                                     &priv->q_level, &priv->force_redraw, &priv->search_re,
                                     priv->pview->win_pager, &priv->ansi_list)))) &&
           color_is_header(priv->lines[new_topline].cid))
    {
      new_topline++;
    }
    priv->top_line = new_topline;
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
    return 0;
  }

  /* Already in the body? Skip past previous "context" quoted lines */
  if (c_pager_skip_quoted_context > 0)
  {
    while (((new_topline < priv->lines_used) ||
            (0 == (rc = display_line(priv->fp, &priv->bytes_read, &priv->lines,
                                     new_topline, &priv->lines_used, &priv->lines_max,
                                     MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP), &priv->quote_list,
                                     &priv->q_level, &priv->force_redraw, &priv->search_re,
                                     priv->pview->win_pager, &priv->ansi_list)))) &&
           COLOR_QUOTED(priv->lines[new_topline].cid))
    {
      new_topline++;
      num_quoted++;
    }

    if (rc < 0)
      return -1;
  }

  if (num_quoted <= c_pager_skip_quoted_context)
  {
    num_quoted = 0;

    while (((new_topline < priv->lines_used) ||
            (0 == (rc = display_line(priv->fp, &priv->bytes_read, &priv->lines,
                                     new_topline, &priv->lines_used, &priv->lines_max,
                                     MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP), &priv->quote_list,
                                     &priv->q_level, &priv->force_redraw, &priv->search_re,
                                     priv->pview->win_pager, &priv->ansi_list)))) &&
           !COLOR_QUOTED(priv->lines[new_topline].cid))
    {
      new_topline++;
    }

    if (rc < 0)
      return -1;

    while (((new_topline < priv->lines_used) ||
            (0 == (rc = display_line(priv->fp, &priv->bytes_read, &priv->lines,
                                     new_topline, &priv->lines_used, &priv->lines_max,
                                     MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP), &priv->quote_list,
                                     &priv->q_level, &priv->force_redraw, &priv->search_re,
                                     priv->pview->win_pager, &priv->ansi_list)))) &&
           COLOR_QUOTED(priv->lines[new_topline].cid))
    {
      new_topline++;
      num_quoted++;
    }

    if (rc < 0)
      return -1;
  }
  priv->top_line = new_topline - MIN(c_pager_skip_quoted_context, num_quoted);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return 0;
}

/**
 * op_pager_skip_quoted - Skip beyond quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 *
 * With a count prefix, skip N quoted blocks instead of 1.
 */
static int op_pager_skip_quoted(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;

  if (!priv->has_types)
    return FR_NO_ACTION;

  const int count = MAX(event->count, 1);

  for (int i = 0; i < count; i++)
  {
    if (pager_skip_quoted_once(priv, fdata) < 0)
      return FR_NO_ACTION;
  }

  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * op_exit - Exit this menu - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_exit(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  priv->rc = -1;
  priv->loop = PAGER_LOOP_QUIT;
  return FR_DONE;
}

/**
 * op_quit - Save changes to mailbox and quit - Implements ::pager_function_t - @ingroup pager_function_api
 *
 * Close the Pager and ask the Index to quit NeoMutt.  The Index will save any
 * changes to the Mailbox and close it.
 */
static int op_quit(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->pview->mode != PAGER_MODE_EMAIL)
    return FR_UNKNOWN;

  priv->rc = -1;
  priv->loop = PAGER_LOOP_QUIT;

  // Close the Pager, then let the Index handle the quit
  mutt_push_macro_event(0, OP_QUIT);

  return FR_DONE;
}

/**
 * op_display_help - Help screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_display_help(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->pview->mode == PAGER_MODE_HELP)
  {
    /* don't let the user enter the help-menu from the help screen! */
    mutt_error(_("Help is currently being shown"));
    return FR_ERROR;
  }
  mutt_help(fdata->mod_data->md_pager);
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  return FR_SUCCESS;
}

/**
 * op_save - Save the Pager text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_save(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  struct PagerView *pview = priv->pview;
  if (pview->mode != PAGER_MODE_OTHER)
    return FR_UNKNOWN;

  if (!priv->fp)
    return FR_UNKNOWN;

  int rc = FR_ERROR;
  FILE *fp_save = NULL;
  struct Buffer *buf = buf_pool_get();

  // Save the current read position
  long pos = ftell(priv->fp);
  rewind(priv->fp);

  struct FileCompletionData cdata = { false, NULL, NULL, NULL, NULL };
  if ((mw_get_field(_("Save to file: "), buf, MUTT_COMP_CLEAR, HC_FILE,
                    &CompleteFileOps, &cdata) != 0) ||
      buf_is_empty(buf))
  {
    rc = FR_SUCCESS;
    goto done;
  }

  expand_path(buf, false);
  fp_save = mutt_file_fopen(buf_string(buf), "a+");
  if (!fp_save)
  {
    mutt_perror("%s", buf_string(buf));
    goto done;
  }

  int bytes = mutt_file_copy_stream(priv->fp, fp_save);
  if (bytes == -1)
  {
    mutt_perror("%s", buf_string(buf));
    goto done;
  }

  mutt_message(_("Saved to: %s"), buf_string(buf));
  rc = FR_SUCCESS;

done:
  // Restore the read position (rewound at start of function)
  if (pos >= 0)
    (void) mutt_file_seek(priv->fp, pos, SEEK_SET);

  mutt_file_fclose(&fp_save);
  buf_pool_release(&buf);

  return rc;
}

/**
 * op_search_toggle - Toggle search pattern coloring - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search_toggle(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct PagerPrivateData *priv = fdata->priv;
  if (priv->search_compiled)
  {
    priv->search_flag ^= MUTT_SEARCH;
    pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  }
  return FR_SUCCESS;
}

/**
 * op_view_attachments - Show MIME attachments - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_view_attachments(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  struct IndexSharedData *shared = fdata->shared;
  struct PagerPrivateData *priv = fdata->priv;
  struct PagerView *pview = priv->pview;

  // This needs to be delegated
  if (pview->flags & MUTT_PAGER_ATTACHMENT)
    return FR_UNKNOWN;

  if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
    return FR_NOT_IMPL;
  dlg_attach(fdata->n->sub, shared->mailbox_view, shared->email,
             pview->pdata->fp, shared->attach_msg);
  if (shared->email->attach_del)
    shared->mailbox->changed = true;
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  return FR_SUCCESS;
}

/**
 * op_ignore - Ignore this function - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_ignore(struct PagerFunctionData *fdata, const struct KeyEvent *event)
{
  return FR_NO_ACTION;
}

// -----------------------------------------------------------------------------

/**
 * PagerFunctions - All the NeoMutt functions that the Pager supports
 */
static const struct PagerFunction PagerFunctions[] = {
  // clang-format off
  { OP_DISPLAY_HELP,           op_display_help },
  { OP_EXIT,                   op_exit },
  { OP_PAGER_HIDE_QUOTED,      op_pager_hide_quoted },
  { OP_PAGER_SKIP_HEADERS,     op_pager_skip_headers },
  { OP_PAGER_SKIP_QUOTED,      op_pager_skip_quoted },
  { OP_QUIT,                   op_quit },
  { OP_SAVE,                   op_save },
  { OP_SCROLL_HALF_DOWN,       op_scroll_half_down },
  { OP_SCROLL_HALF_UP,         op_scroll_half_up },
  { OP_SCROLL_LINE_DOWN,       op_scroll_line_down },
  { OP_SCROLL_LINE_UP,         op_scroll_line_up },
  { OP_SCROLL_PAGE_DOWN,       op_scroll_page_down },
  { OP_SCROLL_PAGE_UP,         op_scroll_page_up },
  { OP_SEARCH_BACKWARD,        op_pager_search },
  { OP_SEARCH_FORWARD,         op_pager_search },
  { OP_SEARCH_NEXT,            op_pager_search_next },
  { OP_SEARCH_PREVIOUS,        op_pager_search_next },
  { OP_SEARCH_TOGGLE,          op_search_toggle },
  { OP_SELECT_FIRST_ENTRY,     op_select_first_entry },
  { OP_SELECT_LAST_ENTRY,      op_select_last_entry },
  { OP_VIEW_ATTACHMENTS,       op_view_attachments },

  // OpGeneric - Ignore
  { OP_ACTIVATE_ENTRY,                op_ignore },
  { OP_SCROLL_SELECTION_TO_BOTTOM,    op_ignore },
  { OP_SCROLL_SELECTION_TO_MIDDLE,    op_ignore },
  { OP_SCROLL_SELECTION_TO_TOP,       op_ignore },
  { OP_SELECT_PAGE_BOTTOM,            op_ignore },
  { OP_SELECT_PAGE_MIDDLE,            op_ignore },
  { OP_SELECT_PAGE_TOP,               op_ignore },
  { 0, NULL },
  // clang-format on
};

/**
 * pager_function_dispatcher - Perform a Pager function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int pager_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event)
{
  if (!win || !event)
  {
    mutt_error("%s", _(Not_available_in_this_menu));
    dispatcher_flush_on_error(FR_ERROR);
    return FR_ERROR;
  }

  if (!win->parent || !win->parent->wdata)
  {
    dispatcher_flush_on_error(FR_ERROR);
    return FR_ERROR;
  }

  struct PagerPrivateData *priv = win->parent->wdata;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg || !dlg->wdata)
  {
    dispatcher_flush_on_error(FR_ERROR);
    return FR_ERROR;
  }

  const int op = event->op;

  struct PagerModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_PAGER);

  struct PagerFunctionData fdata = {
    .n = NeoMutt,
    .mod_data = mod_data,
    .shared = dlg->wdata,
    .priv = priv,
  };

  int rc = FR_UNKNOWN;
  for (size_t i = 0; PagerFunctions[i].op != OP_NULL; i++)
  {
    const struct PagerFunction *fn = &PagerFunctions[i];
    if (fn->op == op)
    {
      rc = fn->function(&fdata, event);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  dispatcher_flush_on_error(rc);
  return rc;
}

/**
 * pager_get_menu_definition - Get the Pager Menu Definition
 * @retval ptr Pager Menu Definition
 */
struct MenuDefinition *pager_get_menu_definition(void)
{
  struct PagerModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_PAGER);
  ASSERT(mod_data);

  return mod_data->md_pager;
}
