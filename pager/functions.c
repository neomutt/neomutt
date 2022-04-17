/**
 * @file
 * Pager functions
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
 * @page pager_functions Pager functions
 *
 * Pager functions
 */

#include "config.h"
#include <stddef.h>
#include <inttypes.h> // IWYU pragma: keep
#include <stdbool.h>
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
#include "color/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "display.h"
#include "opcodes.h"
#include "private_data.h"
#include "protos.h"
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

static const char *Not_available_in_this_menu = N_("Not available in this menu");

static int op_pager_search_next(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op);

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
  mutt_error(_(Not_available_in_this_menu));
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
    if (!hiding || (info[cur].cid != MT_COLOR_QUOTED))
      nlines--;
  }

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
 * op_pager_bottom - Jump to the bottom of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_bottom(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  if (!jump_to_bottom(priv, priv->pview))
    mutt_message(_("Bottom of message is shown"));

  return FR_SUCCESS;
}

/**
 * op_pager_half_down - Scroll down 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_half_down(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  const bool c_pager_stop = cs_subset_bool(NeoMutt->sub, "pager_stop");
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    priv->top_line = up_n_lines(priv->pview->win_pager->state.rows / 2,
                                priv->lines, priv->cur_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (c_pager_stop)
  {
    /* emulate "less -q" and don't go on to the next message. */
    mutt_message(_("Bottom of message is shown"));
  }
  else
  {
    /* end of the current message, so display the next message. */
    index_next_undeleted(priv->pview->win_index);
  }
  return FR_SUCCESS;
}

/**
 * op_pager_half_up - Scroll up 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_half_up(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  if (priv->top_line)
  {
    priv->top_line = up_n_lines(priv->pview->win_pager->state.rows / 2 +
                                    (priv->pview->win_pager->state.rows % 2),
                                priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else
    mutt_message(_("Top of message is shown"));
  return FR_SUCCESS;
}

/**
 * op_pager_hide_quoted - Toggle display of quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_hide_quoted(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  if (!priv->has_types)
    return FR_NO_ACTION;

  priv->hide_quoted ^= MUTT_HIDE;
  if (priv->hide_quoted && (priv->lines[priv->top_line].cid == MT_COLOR_QUOTED))
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
 * op_pager_next_line - Scroll down one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_next_line(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    priv->top_line++;
    if (priv->hide_quoted)
    {
      while ((priv->lines[priv->top_line].cid == MT_COLOR_QUOTED) &&
             (priv->top_line < priv->lines_used))
      {
        priv->top_line++;
      }
    }
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else
  {
    mutt_message(_("Bottom of message is shown"));
  }
  return FR_SUCCESS;
}

/**
 * op_pager_next_page - Move to the next page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_next_page(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  const bool c_pager_stop = cs_subset_bool(NeoMutt->sub, "pager_stop");
  if (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1))
  {
    const short c_pager_context = cs_subset_number(NeoMutt->sub, "pager_context");
    priv->top_line = up_n_lines(c_pager_context, priv->lines, priv->cur_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else if (c_pager_stop)
  {
    /* emulate "less -q" and don't go on to the next message. */
    mutt_message(_("Bottom of message is shown"));
  }
  else
  {
    /* end of the current message, so display the next message. */
    index_next_undeleted(priv->pview->win_index);
  }
  return FR_SUCCESS;
}

/**
 * op_pager_prev_line - Scroll up one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_prev_line(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  if (priv->top_line)
  {
    priv->top_line = up_n_lines(1, priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  else
  {
    mutt_message(_("Top of message is shown"));
  }
  return FR_SUCCESS;
}

/**
 * op_pager_prev_page - Move to the previous page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_prev_page(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  if (priv->top_line == 0)
  {
    mutt_message(_("Top of message is shown"));
  }
  else
  {
    const short c_pager_context = cs_subset_number(NeoMutt->sub, "pager_context");
    priv->top_line = up_n_lines(priv->pview->win_pager->state.rows - c_pager_context,
                                priv->lines, priv->top_line, priv->hide_quoted);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }
  return FR_SUCCESS;
}

/**
 * op_pager_search - Search for a regular expression - Implements ::pager_function_t - @ingroup pager_function_api
 *
 * This function handles:
 * - OP_SEARCH
 * - OP_SEARCH_REVERSE
 */
static int op_pager_search(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  int rc = FR_NO_ACTION;
  struct Buffer *buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(buf, priv->search_str);
  if (mutt_buffer_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                                _("Search for: ") :
                                _("Reverse search for: "),
                            buf, MUTT_COMP_CLEAR | MUTT_COMP_PATTERN, false,
                            NULL, NULL, NULL) != 0)
  {
    goto done;
  }

  if (mutt_str_equal(mutt_buffer_string(buf), priv->search_str))
  {
    if (priv->search_compiled)
    {
      /* do an implicit search-next */
      if (op == OP_SEARCH)
        op = OP_SEARCH_NEXT;
      else
        op = OP_SEARCH_OPPOSITE;

      priv->wrapped = false;
      op_pager_search_next(shared, priv, op);
    }
  }

  if (mutt_buffer_is_empty(buf))
    goto done;

  mutt_str_copy(priv->search_str, mutt_buffer_string(buf), sizeof(priv->search_str));

  /* leave search_back alone if op == OP_SEARCH_NEXT */
  if (op == OP_SEARCH)
    priv->search_back = false;
  else if (op == OP_SEARCH_REVERSE)
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
    mutt_error("%s", mutt_buffer_string(buf));
    for (size_t i = 0; i < priv->lines_max; i++)
    {
      /* cleanup */
      FREE(&(priv->lines[i].search));
      priv->lines[i].search_arr_size = -1;
    }
    priv->search_flag = 0;
    priv->search_compiled = false;
  }
  else
  {
    priv->search_compiled = true;
    /* update the search pointers */
    int line_num = 0;
    while (display_line(priv->fp, &priv->bytes_read, &priv->lines, line_num,
                        &priv->lines_used, &priv->lines_max,
                        MUTT_SEARCH | (pview->flags & MUTT_PAGER_NSKIP) |
                            (pview->flags & MUTT_PAGER_NOWRAP) | priv->has_types,
                        &priv->quote_list, &priv->q_level, &priv->force_redraw,
                        &priv->search_re, priv->pview->win_pager, &priv->ansi_list) == 0)
    {
      line_num++;
    }

    if (!priv->search_back)
    {
      /* searching forward */
      int i;
      for (i = priv->top_line; i < priv->lines_used; i++)
      {
        if ((!priv->hide_quoted || (priv->lines[i].cid != MT_COLOR_QUOTED)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      if (i < priv->lines_used)
        priv->top_line = i;
    }
    else
    {
      /* searching backward */
      int i;
      for (i = priv->top_line; i >= 0; i--)
      {
        if ((!priv->hide_quoted || (priv->lines[i].cid != MT_COLOR_QUOTED)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      if (i >= 0)
        priv->top_line = i;
    }

    if (priv->lines[priv->top_line].search_arr_size == 0)
    {
      priv->search_flag = 0;
      mutt_error(_("Not found"));
    }
    else
    {
      const short c_search_context = cs_subset_number(NeoMutt->sub, "search_context");
      priv->search_flag = MUTT_SEARCH;
      /* give some context for search results */
      if (c_search_context < priv->pview->win_pager->state.rows)
        priv->searchctx = c_search_context;
      else
        priv->searchctx = 0;
      if (priv->top_line - priv->searchctx > 0)
        priv->top_line -= priv->searchctx;
    }
  }
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  rc = FR_SUCCESS;

done:
  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * op_pager_search_next - Search for next match - Implements ::pager_function_t - @ingroup pager_function_api
 *
 * This function handles:
 * - OP_SEARCH_NEXT
 * - OP_SEARCH_OPPOSITE
 */
static int op_pager_search_next(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  if (priv->search_compiled)
  {
    const short c_search_context = cs_subset_number(NeoMutt->sub, "search_context");
    priv->wrapped = false;

    if (c_search_context < priv->pview->win_pager->state.rows)
      priv->searchctx = c_search_context;
    else
      priv->searchctx = 0;

  search_next:
    if ((!priv->search_back && (op == OP_SEARCH_NEXT)) ||
        (priv->search_back && (op == OP_SEARCH_OPPOSITE)))
    {
      /* searching forward */
      int i;
      for (i = priv->wrapped ? 0 : priv->top_line + priv->searchctx + 1;
           i < priv->lines_used; i++)
      {
        if ((!priv->hide_quoted || (priv->lines[i].cid != MT_COLOR_QUOTED)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
      if (i < priv->lines_used)
        priv->top_line = i;
      else if (priv->wrapped || !c_wrap_search)
        mutt_error(_("Not found"));
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
        if ((!priv->hide_quoted || (priv->has_types && (priv->lines[i].cid != MT_COLOR_QUOTED))) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
      if (i >= 0)
        priv->top_line = i;
      else if (priv->wrapped || !c_wrap_search)
        mutt_error(_("Not found"));
      else
      {
        mutt_message(_("Search wrapped to bottom"));
        priv->wrapped = true;
        goto search_next;
      }
    }

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
  return op_pager_search(shared, priv, op);
}

/**
 * op_pager_skip_headers - Jump to first line after headers - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_skip_headers(struct IndexSharedData *shared,
                                 struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!priv->has_types)
    return FR_NO_ACTION;

  int dretval = 0;
  int new_topline = 0;

  while (((new_topline < priv->lines_used) ||
          (0 == (dretval = display_line(priv->fp, &priv->bytes_read, &priv->lines,
                                        new_topline, &priv->lines_used, &priv->lines_max,
                                        MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                                        &priv->quote_list, &priv->q_level,
                                        &priv->force_redraw, &priv->search_re,
                                        priv->pview->win_pager, &priv->ansi_list)))) &&
         simple_color_is_header(priv->lines[new_topline].cid))
  {
    new_topline++;
  }

  if (dretval < 0)
  {
    /* L10N: Displayed if <skip-headers> is invoked in the pager, but
       there is no text past the headers.
       (I don't think this is actually possible in Mutt's code, but
       display some kind of message in case it somehow occurs.) */
    mutt_warning(_("No text past headers"));
    return FR_NO_ACTION;
  }
  priv->top_line = new_topline;
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return FR_SUCCESS;
}

/**
 * op_pager_skip_quoted - Skip beyond quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_skip_quoted(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  if (!priv->has_types)
    return FR_NO_ACTION;

  const short c_skip_quoted_context = cs_subset_number(NeoMutt->sub, "pager_skip_quoted_context");
  int dretval = 0;
  int new_topline = priv->top_line;
  int num_quoted = 0;

  /* In a header? Skip all the email headers, and done */
  if (simple_color_is_header(priv->lines[new_topline].cid))
  {
    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager, &priv->ansi_list)))) &&
           simple_color_is_header(priv->lines[new_topline].cid))
    {
      new_topline++;
    }
    priv->top_line = new_topline;
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
    return FR_SUCCESS;
  }

  /* Already in the body? Skip past previous "context" quoted lines */
  if (c_skip_quoted_context > 0)
  {
    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager, &priv->ansi_list)))) &&
           (priv->lines[new_topline].cid == MT_COLOR_QUOTED))
    {
      new_topline++;
      num_quoted++;
    }

    if (dretval < 0)
    {
      mutt_error(_("No more unquoted text after quoted text"));
      return FR_NO_ACTION;
    }
  }

  if (num_quoted <= c_skip_quoted_context)
  {
    num_quoted = 0;

    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager, &priv->ansi_list)))) &&
           (priv->lines[new_topline].cid != MT_COLOR_QUOTED))
    {
      new_topline++;
    }

    if (dretval < 0)
    {
      mutt_error(_("No more quoted text"));
      return FR_NO_ACTION;
    }

    while (((new_topline < priv->lines_used) ||
            (0 == (dretval = display_line(
                       priv->fp, &priv->bytes_read, &priv->lines, new_topline, &priv->lines_used,
                       &priv->lines_max, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                       &priv->quote_list, &priv->q_level, &priv->force_redraw,
                       &priv->search_re, priv->pview->win_pager, &priv->ansi_list)))) &&
           (priv->lines[new_topline].cid == MT_COLOR_QUOTED))
    {
      new_topline++;
      num_quoted++;
    }

    if (dretval < 0)
    {
      mutt_error(_("No more unquoted text after quoted text"));
      return FR_NO_ACTION;
    }
  }
  priv->top_line = new_topline - MIN(c_skip_quoted_context, num_quoted);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return FR_SUCCESS;
}

/**
 * op_pager_top - Jump to the top of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_top(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->top_line)
    priv->top_line = 0;
  else
    mutt_message(_("Top of message is shown"));
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * op_exit - Exit this menu - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_exit(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  priv->rc = -1;
  priv->loop = PAGER_LOOP_QUIT;
  return FR_DONE;
}

/**
 * op_help - This screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_help(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->pview->mode == PAGER_MODE_HELP)
  {
    /* don't let the user enter the help-menu from the help screen! */
    mutt_error(_("Help is currently being shown"));
    return FR_ERROR;
  }
  mutt_help(MENU_PAGER);
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  return FR_SUCCESS;
}

/**
 * op_search_toggle - Toggle search pattern coloring - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search_toggle(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
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
static int op_view_attachments(struct IndexSharedData *shared,
                               struct PagerPrivateData *priv, int op)
{
  struct PagerView *pview = priv->pview;

  // This needs to be delegated
  if (pview->flags & MUTT_PAGER_ATTACHMENT)
    return FR_UNKNOWN;

  if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
    return FR_NOT_IMPL;
  dlg_select_attachment(NeoMutt->sub, shared->mailbox, shared->email,
                        pview->pdata->fp);
  if (shared->email->attach_del)
    shared->mailbox->changed = true;
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * PagerFunctions - All the NeoMutt functions that the Pager supports
 */
struct PagerFunction PagerFunctions[] = {
  // clang-format off
  { OP_EXIT,                   op_exit },
  { OP_HALF_DOWN,              op_pager_half_down },
  { OP_HALF_UP,                op_pager_half_up },
  { OP_HELP,                   op_help },
  { OP_NEXT_LINE,              op_pager_next_line },
  { OP_NEXT_PAGE,              op_pager_next_page },
  { OP_PAGER_BOTTOM,           op_pager_bottom },
  { OP_PAGER_HIDE_QUOTED,      op_pager_hide_quoted },
  { OP_PAGER_SKIP_HEADERS,     op_pager_skip_headers },
  { OP_PAGER_SKIP_QUOTED,      op_pager_skip_quoted },
  { OP_PAGER_TOP,              op_pager_top },
  { OP_PREV_LINE,              op_pager_prev_line },
  { OP_PREV_PAGE,              op_pager_prev_page },
  { OP_SEARCH,                 op_pager_search },
  { OP_SEARCH_REVERSE,         op_pager_search },
  { OP_SEARCH_NEXT,            op_pager_search_next },
  { OP_SEARCH_OPPOSITE,        op_pager_search_next },
  { OP_SEARCH_TOGGLE,          op_search_toggle },
  { OP_VIEW_ATTACHMENTS,       op_view_attachments },
  { 0, NULL },
  // clang-format on
};

/**
 * pager_function_dispatcher - Perform a Pager function - Implements ::function_dispatcher_t - @ingroup dispatcher_api
 */
int pager_function_dispatcher(struct MuttWindow *win, int op)
{
  if (!win)
  {
    mutt_error(_(Not_available_in_this_menu));
    return FR_ERROR;
  }

  struct PagerPrivateData *priv = win->parent->wdata;
  if (!priv)
    return FR_ERROR;

  struct MuttWindow *dlg = dialog_find(win);
  if (!dlg || !dlg->wdata)
    return FR_ERROR;

  int rc = FR_UNKNOWN;
  for (size_t i = 0; PagerFunctions[i].op != OP_NULL; i++)
  {
    const struct PagerFunction *fn = &PagerFunctions[i];
    if (fn->op == op)
    {
      struct IndexSharedData *shared = dlg->wdata;
      rc = fn->function(shared, priv, op);
      break;
    }
  }

  if (rc == FR_UNKNOWN) // Not our function
    return rc;

  const char *result = dispacher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
