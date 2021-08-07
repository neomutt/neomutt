/**
 * @file
 * Pager Dialog
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page pager_dialog Pager Dialog
 *
 * ## Overview
 *
 * The Pager Dialog displays some text to the user that can be paged through.
 * The actual contents depend on the caller, but are usually an email, file or help.
 *
 * This dialog doesn't exist on its own.  The @ref pager_ppanel is packaged up
 * as part of the @ref index_dialog or the @ref pager_dopager.
 */

#include "config.h"
#include <assert.h>
#include <inttypes.h> // IWYU pragma: keep
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "attach/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "commands.h"
#include "context.h"
#include "display.h"
#include "format_flags.h"
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "private_data.h"
#include "protos.h"
#include "recvcmd.h"
#include "status.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/mdata.h" // IWYU pragma: keep
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/**
 * struct Resize - Keep track of screen resizing
 */
struct Resize
{
  int line;
  bool search_compiled;
  bool search_back;
};

/* hack to return to position when returning from index to same message */
static int TopLine = 0;
static struct Email *OldEmail = NULL;

int braille_row = -1;
int braille_col = -1;

static struct Resize *Resize = NULL;

static const char *Not_available_in_this_menu =
    N_("Not available in this menu");
static const char *Mailbox_is_read_only = N_("Mailbox is read-only");
static const char *Function_not_permitted_in_attach_message_mode =
    N_("Function not permitted in attach-message mode");

/// Help Bar for the Pager's Help Page
static const struct Mapping PagerHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { N_("Help"),          OP_HELP },
  { NULL, 0 },
  // clang-format on
};

/// Help Bar for the Help Page itself
static const struct Mapping PagerHelpHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { NULL, 0 },
  // clang-format on
};

/// Help Bar for the Pager of a normal Mailbox
static const struct Mapping PagerNormalHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { N_("View Attachm."), OP_VIEW_ATTACHMENTS },
  { N_("Del"),           OP_DELETE },
  { N_("Reply"),         OP_REPLY },
  { N_("Next"),          OP_MAIN_NEXT_UNDELETED },
  { N_("Help"),          OP_HELP },
  { NULL, 0 },
  // clang-format on
};

#ifdef USE_NNTP
/// Help Bar for the Pager of an NNTP Mailbox
static const struct Mapping PagerNewsHelp[] = {
  // clang-format off
  { N_("Exit"),          OP_EXIT },
  { N_("PrevPg"),        OP_PREV_PAGE },
  { N_("NextPg"),        OP_NEXT_PAGE },
  { N_("Post"),          OP_POST },
  { N_("Followup"),      OP_FOLLOWUP },
  { N_("Del"),           OP_DELETE },
  { N_("Next"),          OP_MAIN_NEXT_UNDELETED },
  { N_("Help"),          OP_HELP },
  { NULL, 0 },
  // clang-format on
};
#endif

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
 * assert_mailbox_writable - checks that mailbox is writable
 * @param mailbox mailbox to check
 * @retval true  Mailbox is writable
 * @retval false Mailbox is not writable
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_mailbox_writable(struct Mailbox *mailbox)
{
  assert(mailbox);
  if (mailbox->readonly)
  {
    mutt_flushinp();
    mutt_error(_(Mailbox_is_read_only));
    return false;
  }
  return true;
}

/**
 * assert_attach_msg_mode - Check that attach message mode is on
 * @param attach_msg Globally-named boolean pseudo-option
 * @retval true  Attach message mode in on
 * @retval false Attach message mode is off
 *
 * @note On true, the input will be flushed and an error message displayed
 */
static inline bool assert_attach_msg_mode(bool attach_msg)
{
  if (attach_msg)
  {
    mutt_flushinp();
    mutt_error(_(Function_not_permitted_in_attach_message_mode));
    return true;
  }
  return false;
}

/**
 * assert_mailbox_permissions - checks that mailbox is has requested acl flags set
 * @param m      Mailbox to check
 * @param acl    AclFlags required to be set on a given mailbox
 * @param action String to augment error message
 * @retval true  Mailbox has necessary flags set
 * @retval false Mailbox does not have necessary flags set
 *
 * @note On failure, the input will be flushed and an error message displayed
 */
static inline bool assert_mailbox_permissions(struct Mailbox *m, AclFlags acl, char *action)
{
  assert(m);
  assert(action);
  if (m->rights & acl)
  {
    return true;
  }
  mutt_flushinp();
  /* L10N: %s is one of the CHECK_ACL entries below. */
  mutt_error(_("%s: Operation not permitted by ACL"), action);
  return false;
}

/**
 * cleanup_quote - Free a quote list
 * @param[out] quote_list Quote list to free
 */
static void cleanup_quote(struct QClass **quote_list)
{
  struct QClass *ptr = NULL;

  while (*quote_list)
  {
    if ((*quote_list)->down)
      cleanup_quote(&((*quote_list)->down));
    ptr = (*quote_list)->next;
    FREE(&(*quote_list)->prefix);
    FREE(quote_list);
    *quote_list = ptr;
  }
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
    if (!hiding || (info[cur].type != MT_COLOR_QUOTED))
      nlines--;
  }

  return cur;
}

/**
 * mutt_clear_pager_position - Reset the pager's viewing position
 */
void mutt_clear_pager_position(void)
{
  TopLine = 0;
  OldEmail = NULL;
}

/**
 * pager_queue_redraw - Queue a request for a redraw
 * @param priv   Private Pager data
 * @param redraw Item to redraw, e.g. #MENU_REDRAW_FULL
 */
void pager_queue_redraw(struct PagerPrivateData *priv, MenuRedrawFlags redraw)
{
  priv->redraw |= redraw;
  // win_pager->actions |= WA_RECALC;
}

/**
 * pager_custom_redraw - Redraw the pager window
 * @param priv Private Pager data
 */
static void pager_custom_redraw(struct PagerPrivateData *priv)
{
  //---------------------------------------------------------------------------
  // ASSUMPTIONS & SANITY CHECKS
  //---------------------------------------------------------------------------
  // Since pager_custom_redraw() is a static function and it is always called
  // after mutt_pager() we can rely on a series of sanity checks in
  // mutt_pager(), namely:
  // - PAGER_MODE_EMAIL  guarantees ( data->email) and (!data->body)
  // - PAGER_MODE_ATTACH guarantees ( data->email) and ( data->body)
  // - PAGER_MODE_OTHER  guarantees (!data->email) and (!data->body)
  //
  // Additionally, while refactoring is still in progress the following checks
  // are still here to ensure data model consistency.
  assert(priv);        // Redraw function can't be called without its data.
  assert(priv->pview); // Redraw data can't exist separately without the view.
  assert(priv->pview->pdata); // View can't exist without its data
  //---------------------------------------------------------------------------

  char buf[1024] = { 0 };
  struct IndexSharedData *shared = dialog_find(priv->pview->win_pager)->wdata;

  const bool c_tilde = cs_subset_bool(NeoMutt->sub, "tilde");
  const short c_pager_index_lines =
      cs_subset_number(NeoMutt->sub, "pager_index_lines");

  if (priv->redraw & MENU_REDRAW_FULL)
  {
    mutt_curses_set_color(MT_COLOR_NORMAL);
    mutt_window_clear(priv->pview->win_pager);

    if ((priv->pview->mode == PAGER_MODE_EMAIL) &&
        ((shared->mailbox->vcount + 1) < c_pager_index_lines))
    {
      priv->indexlen = shared->mailbox->vcount + 1;
    }
    else
    {
      priv->indexlen = c_pager_index_lines;
    }

    priv->indicator = priv->indexlen / 3;

    if (Resize)
    {
      priv->search_compiled = Resize->search_compiled;
      if (priv->search_compiled)
      {
        uint16_t flags = mutt_mb_is_lower(priv->searchbuf) ? REG_ICASE : 0;
        const int err = REG_COMP(&priv->search_re, priv->searchbuf, REG_NEWLINE | flags);
        if (err == 0)
        {
          priv->search_flag = MUTT_SEARCH;
          priv->search_back = Resize->search_back;
        }
        else
        {
          regerror(err, &priv->search_re, buf, sizeof(buf));
          mutt_error("%s", buf);
          priv->search_compiled = false;
        }
      }
      priv->lines = Resize->line;
      pager_queue_redraw(priv, MENU_REDRAW_FLOW);

      FREE(&Resize);
    }

    if ((priv->pview->mode == PAGER_MODE_EMAIL) && (c_pager_index_lines != 0) && priv->menu)
    {
      mutt_curses_set_color(MT_COLOR_NORMAL);

      /* some fudge to work out whereabouts the indicator should go */
      const int index = menu_get_index(priv->menu);
      if ((index - priv->indicator) < 0)
        priv->menu->top = 0;
      else if ((priv->menu->max - index) < (priv->menu->pagelen - priv->indicator))
        priv->menu->top = priv->menu->max - priv->menu->pagelen;
      else
        priv->menu->top = index - priv->indicator;

      menu_redraw_index(priv->menu);
    }

    pager_queue_redraw(priv, MENU_REDRAW_BODY | MENU_REDRAW_INDEX | MENU_REDRAW_STATUS);
  }

  if (priv->redraw & MENU_REDRAW_FLOW)
  {
    if (!(priv->pview->flags & MUTT_PAGER_RETWINCH))
    {
      priv->lines = -1;
      for (int i = 0; i <= priv->topline; i++)
        if (!priv->line_info[i].continuation)
          priv->lines++;
      for (int i = 0; i < priv->max_line; i++)
      {
        priv->line_info[i].offset = 0;
        priv->line_info[i].type = -1;
        priv->line_info[i].continuation = 0;
        priv->line_info[i].chunks = 0;
        priv->line_info[i].search_cnt = -1;
        priv->line_info[i].quote = NULL;

        mutt_mem_realloc(&(priv->line_info[i].syntax), sizeof(struct TextSyntax));
        if (priv->search_compiled && priv->line_info[i].search)
          FREE(&(priv->line_info[i].search));
      }

      priv->last_line = 0;
      priv->topline = 0;
    }
    int i = -1;
    int j = -1;
    while (display_line(priv->fp, &priv->last_pos, &priv->line_info, ++i,
                        &priv->last_line, &priv->max_line,
                        priv->has_types | priv->search_flag | (priv->pview->flags & MUTT_PAGER_NOWRAP),
                        &priv->quote_list, &priv->q_level, &priv->force_redraw,
                        &priv->search_re, priv->pview->win_pager) == 0)
    {
      if (!priv->line_info[i].continuation && (++j == priv->lines))
      {
        priv->topline = i;
        if (!priv->search_flag)
          break;
      }
    }
  }

  if ((priv->redraw & MENU_REDRAW_BODY) || (priv->topline != priv->oldtopline))
  {
    do
    {
      mutt_window_move(priv->pview->win_pager, 0, 0);
      priv->curline = priv->topline;
      priv->oldtopline = priv->topline;
      priv->lines = 0;
      priv->force_redraw = false;

      while ((priv->lines < priv->pview->win_pager->state.rows) &&
             (priv->line_info[priv->curline].offset <= priv->sb.st_size - 1))
      {
        if (display_line(priv->fp, &priv->last_pos, &priv->line_info,
                         priv->curline, &priv->last_line, &priv->max_line,
                         (priv->pview->flags & MUTT_DISPLAYFLAGS) | priv->hide_quoted |
                             priv->search_flag | (priv->pview->flags & MUTT_PAGER_NOWRAP),
                         &priv->quote_list, &priv->q_level, &priv->force_redraw,
                         &priv->search_re, priv->pview->win_pager) > 0)
        {
          priv->lines++;
        }
        priv->curline++;
        mutt_window_move(priv->pview->win_pager, 0, priv->lines);
      }
      priv->last_offset = priv->line_info[priv->curline].offset;
    } while (priv->force_redraw);

    mutt_curses_set_color(MT_COLOR_TILDE);
    while (priv->lines < priv->pview->win_pager->state.rows)
    {
      mutt_window_clrtoeol(priv->pview->win_pager);
      if (c_tilde)
        mutt_window_addch(priv->pview->win_pager, '~');
      priv->lines++;
      mutt_window_move(priv->pview->win_pager, 0, priv->lines);
    }
    mutt_curses_set_color(MT_COLOR_NORMAL);

    /* We are going to update the pager status bar, so it isn't
     * necessary to reset to normal color now. */

    pager_queue_redraw(priv, MENU_REDRAW_STATUS); /* need to update the % seen */
  }

  if (priv->redraw & MENU_REDRAW_STATUS)
  {
    char pager_progress_str[65]; /* Lots of space for translations */

    if (priv->last_pos < priv->sb.st_size - 1)
    {
      snprintf(pager_progress_str, sizeof(pager_progress_str), OFF_T_FMT "%%",
               (100 * priv->last_offset / priv->sb.st_size));
    }
    else
    {
      const char *msg = (priv->topline == 0) ?
                            /* L10N: Status bar message: the entire email is visible in the pager */
                            _("all") :
                            /* L10N: Status bar message: the end of the email is visible in the pager */
                            _("end");
      mutt_str_copy(pager_progress_str, msg, sizeof(pager_progress_str));
    }

    /* print out the pager status bar */
    mutt_window_move(priv->pview->win_pbar, 0, 0);
    mutt_curses_set_color(MT_COLOR_STATUS);

    if (priv->pview->mode == PAGER_MODE_EMAIL || priv->pview->mode == PAGER_MODE_ATTACH_E)
    {
      const size_t l1 = priv->pview->win_pbar->state.cols * MB_LEN_MAX;
      const size_t l2 = sizeof(buf);
      const size_t buflen = (l1 < l2) ? l1 : l2;
      struct Email *e = (priv->pview->mode == PAGER_MODE_EMAIL) ? shared->email : // PAGER_MODE_EMAIL
                            priv->pview->pdata->body->email; // PAGER_MODE_ATTACH_E

      const char *const c_pager_format =
          cs_subset_string(NeoMutt->sub, "pager_format");
      mutt_make_string(buf, buflen, priv->pview->win_pbar->state.cols,
                       NONULL(c_pager_format), shared->mailbox, shared->ctx->msg_in_pager,
                       e, MUTT_FORMAT_NO_FLAGS, pager_progress_str);
      mutt_draw_statusline(priv->pview->win_pbar,
                           priv->pview->win_pbar->state.cols, buf, l2);
    }
    else
    {
      char bn[256];
      snprintf(bn, sizeof(bn), "%s (%s)", priv->pview->banner, pager_progress_str);
      mutt_draw_statusline(priv->pview->win_pbar,
                           priv->pview->win_pbar->state.cols, bn, sizeof(bn));
    }
    mutt_curses_set_color(MT_COLOR_NORMAL);
    const bool c_ts_enabled = cs_subset_bool(NeoMutt->sub, "ts_enabled");
    if (c_ts_enabled && TsSupported && priv->menu)
    {
      const char *const c_ts_status_format =
          cs_subset_string(NeoMutt->sub, "ts_status_format");
      menu_status_line(buf, sizeof(buf), shared, priv->menu, sizeof(buf),
                       NONULL(c_ts_status_format));
      mutt_ts_status(buf);
      const char *const c_ts_icon_format =
          cs_subset_string(NeoMutt->sub, "ts_icon_format");
      menu_status_line(buf, sizeof(buf), shared, priv->menu, sizeof(buf),
                       NONULL(c_ts_icon_format));
      mutt_ts_icon(buf);
    }
  }

  priv->redraw = MENU_REDRAW_NO_FLAGS;
  mutt_debug(LL_DEBUG5, "repaint done\n");
}

/**
 * pager_resolve_help_mapping - determine help mapping based on pager mode and
 * mailbox type
 * @param mode pager mode
 * @param type mailbox type
 */
static const struct Mapping *pager_resolve_help_mapping(enum PagerMode mode, enum MailboxType type)
{
  const struct Mapping *result;
  switch (mode)
  {
    case PAGER_MODE_EMAIL:
    case PAGER_MODE_ATTACH:
    case PAGER_MODE_ATTACH_E:
      if (type == MUTT_NNTP)
        result = PagerNewsHelp;
      else
        result = PagerNormalHelp;
      break;

    case PAGER_MODE_HELP:
      result = PagerHelpHelp;
      break;

    case PAGER_MODE_OTHER:
      result = PagerHelp;
      break;

    case PAGER_MODE_UNKNOWN:
    case PAGER_MODE_MAX:
    default:
      assert(false); // something went really wrong
  }
  assert(result);
  return result;
}

/**
 * jump_to_bottom - make sure the bottom line is displayed
 * @param priv   Private Pager data
 * @param pview PagerView
 * @retval true Something changed
 * @retval false Bottom was already displayed
 */
static bool jump_to_bottom(struct PagerPrivateData *priv, struct PagerView *pview)
{
  if (!(priv->line_info[priv->curline].offset < (priv->sb.st_size - 1)))
  {
    return false;
  }

  int line_num = priv->curline;
  /* make sure the types are defined to the end of file */
  while (display_line(priv->fp, &priv->last_pos, &priv->line_info, line_num,
                      &priv->last_line, &priv->max_line,
                      priv->has_types | (pview->flags & MUTT_PAGER_NOWRAP),
                      &priv->quote_list, &priv->q_level, &priv->force_redraw,
                      &priv->search_re, priv->pview->win_pager) == 0)
  {
    line_num++;
  }
  priv->topline = up_n_lines(priv->pview->win_pager->state.rows, priv->line_info,
                             priv->last_line, priv->hide_quoted);
  return true;
}

/**
 * check_read_delay - Is it time to mark the message read?
 * @param timestamp Time when message should be marked read, or 0
 * @retval  true Message should be marked read
 * @retval false No action necessary
 */
static bool check_read_delay(uint64_t *timestamp)
{
  if ((*timestamp != 0) && (mutt_date_epoch_ms() > *timestamp))
  {
    *timestamp = 0;
    return true;
  }
  return false;
}

/**
 * squash_index_panel - Shrink or hide the Index Panel
 * @param dlg    Dialog
 * @param shared Shared Index data
 * @param priv   Private Pager data
 * @param pil    Pager Index Lines
 */
static void squash_index_panel(struct MuttWindow *dlg, struct IndexSharedData *shared,
                               struct PagerPrivateData *priv, int pil)
{
  struct MuttWindow *win_pager = priv->pview->win_pager;
  struct MuttWindow *win_index = priv->pview->win_index;

  if (win_index)
  {
    const int index_space = shared->mailbox ? MIN(pil, shared->mailbox->vcount) : pil;
    priv->menu = win_index->wdata;
    win_index->size = MUTT_WIN_SIZE_FIXED;
    win_index->req_rows = index_space;
    win_index->parent->size = MUTT_WIN_SIZE_MINIMISE;
    window_set_visible(win_index->parent, (index_space > 0));
  }

  window_set_visible(win_pager->parent, true);
  mutt_window_reflow(dlg);
}

/**
 * expand_index_panel - Restore the Index Panel
 * @param dlg  Dialog
 * @param priv Private Pager data
 */
static void expand_index_panel(struct MuttWindow *dlg, struct PagerPrivateData *priv)
{
  struct MuttWindow *win_pager = priv->pview->win_pager;
  struct MuttWindow *win_index = priv->pview->win_index;

  if (win_index)
  {
    win_index->size = MUTT_WIN_SIZE_MAXIMISE;
    win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    win_index->parent->size = MUTT_WIN_SIZE_MAXIMISE;
    win_index->parent->req_rows = MUTT_WIN_SIZE_UNLIMITED;
    window_set_visible(win_index->parent, true);
  }

  window_set_visible(win_pager->parent, false);
  mutt_window_reflow(dlg);
}

/**
 * mutt_pager - Display an email, attachment, or help, in a window
 * @param pview Pager view settings
 * @retval  0 Success
 * @retval -1 Error
 *
 * This pager is actually not so simple as it once was. But it will be again.
 * Currently it operates in 3 modes:
 * - viewing messages.                (PAGER_MODE_EMAIL)
 * - viewing attachments.             (PAGER_MODE_ATTACH)
 * - viewing other stuff (e.g. help). (PAGER_MODE_OTHER)
 * These can be distinguished by PagerMode in PagerView.
 * Data is not yet polymorphic and is fused into a single struct (PagerData).
 * Different elements of PagerData are expected to be present depending on the
 * mode:
 * - PAGER_MODE_EMAIL expects data->email and not expects data->body
 * - PAGER_MODE_ATTACH expects data->email and data->body
 *   special sub-case of this mode is viewing attached email message
 *   it is recognized by presence of data->fp and data->body->email
 * - PAGER_MODE_OTHER does not expect data->email or data->body
 */
int mutt_pager(struct PagerView *pview)
{
  //===========================================================================
  // ACT 1 - Ensure sanity of the caller and determine the mode
  //===========================================================================
  assert(pview);
  assert((pview->mode > PAGER_MODE_UNKNOWN) && (pview->mode < PAGER_MODE_MAX));
  assert(pview->pdata); // view can't exist in a vacuum
  assert(pview->win_pager);
  assert(pview->win_pbar);

  struct MuttWindow *dlg = dialog_find(pview->win_pager);
  struct IndexSharedData *shared = dlg->wdata;

  switch (pview->mode)
  {
    case PAGER_MODE_EMAIL:
      // This case was previously identified by IsEmail macro
      // we expect data to contain email and not contain body
      // We also expect email to always belong to some mailbox
      assert(shared->ctx);
      assert(shared->mailbox);
      assert(shared->email);
      assert(!pview->pdata->body);
      break;

    case PAGER_MODE_ATTACH:
      // this case was previously identified by IsAttach and IsMsgAttach
      // macros, we expect data to contain:
      //  - body (viewing regular attachment)
      //  - email
      //  - fp and body->email in special case of viewing an attached email.
      assert(shared->email); // This should point to the top level email
      assert(pview->pdata->body);
      if (pview->pdata->fp && pview->pdata->body->email)
      {
        // Special case: attachment is a full-blown email message.
        // Yes, emails can contain other emails.
        pview->mode = PAGER_MODE_ATTACH_E;
      }
      break;

    case PAGER_MODE_HELP:
    case PAGER_MODE_OTHER:
      assert(!shared->ctx);
      assert(!shared->email);
      assert(!pview->pdata->body);
      break;

    case PAGER_MODE_UNKNOWN:
    case PAGER_MODE_MAX:
    default:
      // Unexpected mode. Catch fire and explode.
      // This *should* happen if mode is PAGER_MODE_ATTACH_E, since
      // we do not expect any caller to pass it to us.
      assert(false);
      break;
  }

  //===========================================================================
  // ACT 2 - Declare, initialize local variables, read config, etc.
  //===========================================================================

  //---------- reading config values needed now--------------------------------
  const short c_pager_index_lines =
      cs_subset_number(NeoMutt->sub, "pager_index_lines");

  //---------- local variables ------------------------------------------------
  int op = 0;
  int rc = -1;
  int searchctx = 0;
  bool first = true;
  bool wrapped = false;
  enum MailboxType mailbox_type = shared->mailbox ? shared->mailbox->type : MUTT_UNKNOWN;
  uint64_t delay_read_timestamp = 0;

  struct PagerPrivateData *priv = pview->win_pager->parent->wdata;
  {
    // Wipe any previous state info
    struct Menu *menu = priv->menu;
    memset(priv, 0, sizeof(*priv));
    priv->menu = menu;
    priv->win_pbar = pview->win_pbar;
  }

  //---------- setup flags ----------------------------------------------------
  if (!(pview->flags & MUTT_SHOWCOLOR))
    pview->flags |= MUTT_SHOWFLAT;

  if (pview->mode == PAGER_MODE_EMAIL && !shared->email->read)
  {
    shared->ctx->msg_in_pager = shared->email->msgno;
    priv->win_pbar->actions |= WA_RECALC;
    const short c_pager_read_delay =
        cs_subset_number(NeoMutt->sub, "pager_read_delay");
    if (c_pager_read_delay == 0)
    {
      mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true);
    }
    else
    {
      delay_read_timestamp = mutt_date_epoch_ms() + (1000 * c_pager_read_delay);
    }
  }
  //---------- setup help menu ------------------------------------------------
  pview->win_pager->help_data = pager_resolve_help_mapping(pview->mode, mailbox_type);
  pview->win_pager->help_menu = MENU_PAGER;

  //---------- initialize redraw pdata  -----------------------------------------
  pview->win_pager->size = MUTT_WIN_SIZE_MAXIMISE;
  priv->pview = pview;
  priv->indexlen = c_pager_index_lines;
  priv->indicator = priv->indexlen / 3;
  priv->max_line = LINES; // number of lines on screen, from curses
  priv->line_info = mutt_mem_calloc(priv->max_line, sizeof(struct Line));
  priv->fp = fopen(pview->pdata->fname, "r");
  priv->has_types =
      ((pview->mode == PAGER_MODE_EMAIL) || (pview->flags & MUTT_SHOWCOLOR)) ?
          MUTT_TYPES :
          0; // main message or rfc822 attachment

  for (size_t i = 0; i < priv->max_line; i++)
  {
    priv->line_info[i].type = -1;
    priv->line_info[i].search_cnt = -1;
    priv->line_info[i].syntax = mutt_mem_malloc(sizeof(struct TextSyntax));
    (priv->line_info[i].syntax)[0].first = -1;
    (priv->line_info[i].syntax)[0].last = -1;
  }

  // ---------- try to open the pdata file -------------------------------------
  if (!priv->fp)
  {
    mutt_perror(pview->pdata->fname);
    return -1;
  }

  if (stat(pview->pdata->fname, &priv->sb) != 0)
  {
    mutt_perror(pview->pdata->fname);
    mutt_file_fclose(&priv->fp);
    return -1;
  }
  unlink(pview->pdata->fname);

  //---------- restore global state if needed ---------------------------------
  while ((pview->mode == PAGER_MODE_EMAIL) && (OldEmail == shared->email) && // are we "resuming" to the same Email?
         (TopLine != priv->topline) && // is saved offset different?
         (priv->line_info[priv->curline].offset < (priv->sb.st_size - 1)))
  {
    pager_queue_redraw(priv, MENU_REDRAW_FULL);
    pager_custom_redraw(priv);
    // trick user, as if nothing happened
    // scroll down to previosly saved offset
    priv->topline =
        ((TopLine - priv->topline) > priv->lines) ? priv->topline + priv->lines : TopLine;
  }

  TopLine = 0;
  OldEmail = NULL;

  //---------- show windows, set focus and visibility --------------------------
  squash_index_panel(dlg, shared, priv, c_pager_index_lines);
  window_set_focus(pview->win_pager);

  //---------- jump to the bottom if requested ------------------------------
  if (pview->flags & MUTT_PAGER_BOTTOM)
  {
    jump_to_bottom(priv, pview);
  }

  //-------------------------------------------------------------------------
  // ACT 3: Read user input and decide what to do with it
  //        ...but also do a whole lot of other things.
  //-------------------------------------------------------------------------
  while (op != -1)
  {
    if (check_read_delay(&delay_read_timestamp))
    {
      mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true);
    }
    mutt_curses_set_cursor(MUTT_CURSOR_INVISIBLE);

    pager_queue_redraw(priv, MENU_REDRAW_FULL);
    window_redraw(NULL);
    pager_custom_redraw(priv);

    const bool c_braille_friendly =
        cs_subset_bool(NeoMutt->sub, "braille_friendly");
    if (c_braille_friendly)
    {
      if (braille_row != -1)
      {
        mutt_window_move(priv->pview->win_pager, braille_col, braille_row + 1);
        braille_row = -1;
      }
    }
    else
      mutt_window_move(priv->pview->win_pbar, priv->pview->win_pager->state.cols - 1, 0);

    // force redraw of the screen at every iteration of the event loop
    mutt_refresh();

    //-------------------------------------------------------------------------
    // Check if information in the status bar needs an update
    // This is done because pager is a single-threaded application, which
    // tries to emulate concurrency.
    //-------------------------------------------------------------------------
    bool do_new_mail = false;
    if (shared->mailbox && !OptAttachMsg)
    {
      int oldcount = shared->mailbox->msg_count;
      /* check for new mail */
      enum MxStatus check = mx_mbox_check(shared->mailbox);
      if (check == MX_STATUS_ERROR)
      {
        if (!shared->mailbox || mutt_buffer_is_empty(&shared->mailbox->pathbuf))
        {
          /* fatal error occurred */
          pager_queue_redraw(priv, MENU_REDRAW_FULL);
          break;
        }
      }
      else if ((check == MX_STATUS_NEW_MAIL) || (check == MX_STATUS_REOPENED) ||
               (check == MX_STATUS_FLAGS))
      {
        /* notify user of newly arrived mail */
        if (check == MX_STATUS_NEW_MAIL)
        {
          for (size_t i = oldcount; i < shared->mailbox->msg_count; i++)
          {
            struct Email *e = shared->mailbox->emails[i];

            if (e && !e->read)
            {
              mutt_message(_("New mail in this mailbox"));
              do_new_mail = true;
              break;
            }
          }
        }

        if ((check == MX_STATUS_NEW_MAIL) || (check == MX_STATUS_REOPENED))
        {
          if (priv->menu && shared->mailbox)
          {
            /* After the mailbox has been updated, selection might be invalid */
            int index = menu_get_index(priv->menu);
            menu_set_index(priv->menu, MIN(index, MAX(shared->mailbox->msg_count - 1, 0)));
            index = menu_get_index(priv->menu);
            struct Email *e = mutt_get_virt_email(shared->mailbox, index);
            if (!e)
              continue;

            bool verbose = shared->mailbox->verbose;
            shared->mailbox->verbose = false;
            mutt_update_index(priv->menu, shared->ctx, check, oldcount, shared);
            shared->mailbox->verbose = verbose;

            priv->menu->max = shared->mailbox->vcount;

            /* If these header pointers don't match, then our email may have
             * been deleted.  Make the pointer safe, then leave the pager.
             * This have a unpleasant behaviour to close the pager even the
             * deleted message is not the opened one, but at least it's safe. */
            e = mutt_get_virt_email(shared->mailbox, index);
            if (shared->email != e)
            {
              shared->email = e;
              break;
            }
          }

          pager_queue_redraw(priv, MENU_REDRAW_FULL);
          OptSearchInvalid = true;
        }
      }

      if (mutt_mailbox_notify(shared->mailbox) || do_new_mail)
      {
        const bool c_beep_new = cs_subset_bool(NeoMutt->sub, "beep_new");
        if (c_beep_new)
          mutt_beep(true);
        const char *const c_new_mail_command =
            cs_subset_string(NeoMutt->sub, "new_mail_command");
        if (c_new_mail_command)
        {
          char cmd[1024];
          menu_status_line(cmd, sizeof(cmd), shared, priv->menu, sizeof(cmd),
                           NONULL(c_new_mail_command));
          if (mutt_system(cmd) != 0)
            mutt_error(_("Error running \"%s\""), cmd);
        }
      }
    }
    //-------------------------------------------------------------------------

    if (SigWinch)
    {
      SigWinch = false;
      mutt_resize_screen();
      clearok(stdscr, true); /* force complete redraw */
      msgwin_clear_text();

      pager_queue_redraw(priv, MENU_REDRAW_FLOW);
      if (pview->flags & MUTT_PAGER_RETWINCH)
      {
        /* Store current position. */
        priv->lines = -1;
        for (size_t i = 0; i <= priv->topline; i++)
          if (!priv->line_info[i].continuation)
            priv->lines++;

        Resize = mutt_mem_malloc(sizeof(struct Resize));

        Resize->line = priv->lines;
        Resize->search_compiled = priv->search_compiled;
        Resize->search_back = priv->search_back;

        op = -1;
        rc = OP_REFORMAT_WINCH;
      }
      else
      {
        /* note: mutt_resize_screen() -> mutt_window_reflow() sets
         * MENU_REDRAW_FULL and MENU_REDRAW_FLOW */
        op = 0;
      }
      continue;
    }
    //-------------------------------------------------------------------------
    // Finally, read user's key press
    //-------------------------------------------------------------------------
    // km_dokey() reads not only user's key strokes, but also a MacroBuffer
    // MacroBuffer may contain OP codes of the operations.
    // MacroBuffer is global
    // OP codes inserted into the MacroBuffer by various functions.
    // One of such functions is `mutt_enter_command()`
    // Some OP codes are not handled by pager, they cause pager to quit returning
    // OP code to index. Index handles the operation and then restarts pager
    op = km_dokey(MENU_PAGER);

    if (op >= 0)
    {
      mutt_clear_error();
      mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", OpStrings[op][0], op);
    }
    mutt_curses_set_cursor(MUTT_CURSOR_VISIBLE);

    if (op < 0)
    {
      op = 0;
      mutt_timeout_hook();
      continue;
    }

    rc = op;

    switch (op)
    {
        //=======================================================================

      case OP_EXIT:
        rc = -1;
        op = -1;
        break;

        //=======================================================================

      case OP_QUIT:
      {
        const enum QuadOption c_quit = cs_subset_quad(NeoMutt->sub, "quit");
        if (query_quadoption(c_quit, _("Quit NeoMutt?")) == MUTT_YES)
        {
          /* avoid prompting again in the index menu */
          cs_subset_str_native_set(NeoMutt->sub, "quit", MUTT_YES, NULL);
          op = -1;
        }
        break;
      }

        //=======================================================================

      case OP_NEXT_PAGE:
      {
        const bool c_pager_stop = cs_subset_bool(NeoMutt->sub, "pager_stop");
        if (priv->line_info[priv->curline].offset < (priv->sb.st_size - 1))
        {
          const short c_pager_context =
              cs_subset_number(NeoMutt->sub, "pager_context");
          priv->topline = up_n_lines(c_pager_context, priv->line_info,
                                     priv->curline, priv->hide_quoted);
        }
        else if (c_pager_stop)
        {
          /* emulate "less -q" and don't go on to the next message. */
          mutt_message(_("Bottom of message is shown"));
        }
        else
        {
          /* end of the current message, so display the next message. */
          rc = OP_MAIN_NEXT_UNDELETED;
          op = -1;
        }
        break;
      }

        //=======================================================================

      case OP_PREV_PAGE:
        if (priv->topline == 0)
        {
          mutt_message(_("Top of message is shown"));
        }
        else
        {
          const short c_pager_context =
              cs_subset_number(NeoMutt->sub, "pager_context");
          priv->topline =
              up_n_lines(priv->pview->win_pager->state.rows - c_pager_context,
                         priv->line_info, priv->topline, priv->hide_quoted);
        }
        break;

        //=======================================================================

      case OP_NEXT_LINE:
        if (priv->line_info[priv->curline].offset < (priv->sb.st_size - 1))
        {
          priv->topline++;
          if (priv->hide_quoted)
          {
            while ((priv->line_info[priv->topline].type == MT_COLOR_QUOTED) &&
                   (priv->topline < priv->last_line))
            {
              priv->topline++;
            }
          }
        }
        else
          mutt_message(_("Bottom of message is shown"));
        break;

        //=======================================================================

      case OP_PREV_LINE:
        if (priv->topline)
          priv->topline = up_n_lines(1, priv->line_info, priv->topline, priv->hide_quoted);
        else
          mutt_message(_("Top of message is shown"));
        break;

        //=======================================================================

      case OP_PAGER_TOP:
        if (priv->topline)
          priv->topline = 0;
        else
          mutt_message(_("Top of message is shown"));
        break;

        //=======================================================================

      case OP_HALF_UP:
        if (priv->topline)
        {
          priv->topline = up_n_lines(priv->pview->win_pager->state.rows / 2 +
                                         (priv->pview->win_pager->state.rows % 2),
                                     priv->line_info, priv->topline, priv->hide_quoted);
        }
        else
          mutt_message(_("Top of message is shown"));
        break;

        //=======================================================================

      case OP_HALF_DOWN:
      {
        const bool c_pager_stop = cs_subset_bool(NeoMutt->sub, "pager_stop");
        if (priv->line_info[priv->curline].offset < (priv->sb.st_size - 1))
        {
          priv->topline = up_n_lines(priv->pview->win_pager->state.rows / 2,
                                     priv->line_info, priv->curline, priv->hide_quoted);
        }
        else if (c_pager_stop)
        {
          /* emulate "less -q" and don't go on to the next message. */
          mutt_message(_("Bottom of message is shown"));
        }
        else
        {
          /* end of the current message, so display the next message. */
          rc = OP_MAIN_NEXT_UNDELETED;
          op = -1;
        }
        break;
      }

        //=======================================================================

      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
        if (priv->search_compiled)
        {
          const short c_search_context =
              cs_subset_number(NeoMutt->sub, "search_context");
          wrapped = false;

          if (c_search_context < priv->pview->win_pager->state.rows)
            searchctx = c_search_context;
          else
            searchctx = 0;

        search_next:
          if ((!priv->search_back && (op == OP_SEARCH_NEXT)) ||
              (priv->search_back && (op == OP_SEARCH_OPPOSITE)))
          {
            /* searching forward */
            int i;
            for (i = wrapped ? 0 : priv->topline + searchctx + 1; i < priv->last_line; i++)
            {
              if ((!priv->hide_quoted || (priv->line_info[i].type != MT_COLOR_QUOTED)) &&
                  !priv->line_info[i].continuation && (priv->line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            const bool c_wrap_search =
                cs_subset_bool(NeoMutt->sub, "wrap_search");
            if (i < priv->last_line)
              priv->topline = i;
            else if (wrapped || !c_wrap_search)
              mutt_error(_("Not found"));
            else
            {
              mutt_message(_("Search wrapped to top"));
              wrapped = true;
              goto search_next;
            }
          }
          else
          {
            /* searching backward */
            int i;
            for (i = wrapped ? priv->last_line : priv->topline + searchctx - 1; i >= 0; i--)
            {
              if ((!priv->hide_quoted ||
                   (priv->has_types && (priv->line_info[i].type != MT_COLOR_QUOTED))) &&
                  !priv->line_info[i].continuation && (priv->line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            const bool c_wrap_search =
                cs_subset_bool(NeoMutt->sub, "wrap_search");
            if (i >= 0)
              priv->topline = i;
            else if (wrapped || !c_wrap_search)
              mutt_error(_("Not found"));
            else
            {
              mutt_message(_("Search wrapped to bottom"));
              wrapped = true;
              goto search_next;
            }
          }

          if (priv->line_info[priv->topline].search_cnt > 0)
          {
            priv->search_flag = MUTT_SEARCH;
            /* give some context for search results */
            if (priv->topline - searchctx > 0)
              priv->topline -= searchctx;
          }

          break;
        }
        /* no previous search pattern */
        /* fallthrough */
        //=======================================================================

      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
      {
        char buf[1024] = { 0 };
        mutt_str_copy(buf, priv->searchbuf, sizeof(buf));
        if (mutt_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ?
                               _("Search for: ") :
                               _("Reverse search for: "),
                           buf, sizeof(buf), MUTT_CLEAR | MUTT_PATTERN, false,
                           NULL, NULL) != 0)
        {
          break;
        }

        if (strcmp(buf, priv->searchbuf) == 0)
        {
          if (priv->search_compiled)
          {
            /* do an implicit search-next */
            if (op == OP_SEARCH)
              op = OP_SEARCH_NEXT;
            else
              op = OP_SEARCH_OPPOSITE;

            wrapped = false;
            goto search_next;
          }
        }

        if (buf[0] == '\0')
          break;

        mutt_str_copy(priv->searchbuf, buf, sizeof(priv->searchbuf));

        /* leave search_back alone if op == OP_SEARCH_NEXT */
        if (op == OP_SEARCH)
          priv->search_back = false;
        else if (op == OP_SEARCH_REVERSE)
          priv->search_back = true;

        if (priv->search_compiled)
        {
          regfree(&priv->search_re);
          for (size_t i = 0; i < priv->last_line; i++)
          {
            FREE(&(priv->line_info[i].search));
            priv->line_info[i].search_cnt = -1;
          }
        }

        uint16_t rflags = mutt_mb_is_lower(priv->searchbuf) ? REG_ICASE : 0;
        int err = REG_COMP(&priv->search_re, priv->searchbuf, REG_NEWLINE | rflags);
        if (err != 0)
        {
          regerror(err, &priv->search_re, buf, sizeof(buf));
          mutt_error("%s", buf);
          for (size_t i = 0; i < priv->max_line; i++)
          {
            /* cleanup */
            FREE(&(priv->line_info[i].search));
            priv->line_info[i].search_cnt = -1;
          }
          priv->search_flag = 0;
          priv->search_compiled = false;
        }
        else
        {
          priv->search_compiled = true;
          /* update the search pointers */
          int line_num = 0;
          while (display_line(priv->fp, &priv->last_pos, &priv->line_info,
                              line_num, &priv->last_line, &priv->max_line,
                              MUTT_SEARCH | (pview->flags & MUTT_PAGER_NSKIP) |
                                  (pview->flags & MUTT_PAGER_NOWRAP),
                              &priv->quote_list, &priv->q_level, &priv->force_redraw,
                              &priv->search_re, priv->pview->win_pager) == 0)
          {
            line_num++;
          }

          if (!priv->search_back)
          {
            /* searching forward */
            int i;
            for (i = priv->topline; i < priv->last_line; i++)
            {
              if ((!priv->hide_quoted || (priv->line_info[i].type != MT_COLOR_QUOTED)) &&
                  !priv->line_info[i].continuation && (priv->line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            if (i < priv->last_line)
              priv->topline = i;
          }
          else
          {
            /* searching backward */
            int i;
            for (i = priv->topline; i >= 0; i--)
            {
              if ((!priv->hide_quoted || (priv->line_info[i].type != MT_COLOR_QUOTED)) &&
                  !priv->line_info[i].continuation && (priv->line_info[i].search_cnt > 0))
              {
                break;
              }
            }

            if (i >= 0)
              priv->topline = i;
          }

          if (priv->line_info[priv->topline].search_cnt == 0)
          {
            priv->search_flag = 0;
            mutt_error(_("Not found"));
          }
          else
          {
            const short c_search_context =
                cs_subset_number(NeoMutt->sub, "search_context");
            priv->search_flag = MUTT_SEARCH;
            /* give some context for search results */
            if (c_search_context < priv->pview->win_pager->state.rows)
              searchctx = c_search_context;
            else
              searchctx = 0;
            if (priv->topline - searchctx > 0)
              priv->topline -= searchctx;
          }
        }
        pager_queue_redraw(priv, MENU_REDRAW_BODY);
        break;
      }

        //=======================================================================

      case OP_SEARCH_TOGGLE:
        if (priv->search_compiled)
        {
          priv->search_flag ^= MUTT_SEARCH;
          pager_queue_redraw(priv, MENU_REDRAW_BODY);
        }
        break;

        //=======================================================================

      case OP_SORT:
      case OP_SORT_REVERSE:
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (mutt_select_sort(op == OP_SORT_REVERSE))
        {
          OptNeedResort = true;
          op = -1;
          rc = OP_DISPLAY_MESSAGE;
        }
        break;

        //=======================================================================

      case OP_HELP:
        if (pview->mode == PAGER_MODE_HELP)
        {
          /* don't let the user enter the help-menu from the help screen! */
          mutt_error(_("Help is currently being shown"));
          break;
        }
        mutt_help(MENU_PAGER);
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_PAGER_HIDE_QUOTED:
        if (!priv->has_types)
          break;

        priv->hide_quoted ^= MUTT_HIDE;
        if (priv->hide_quoted && (priv->line_info[priv->topline].type == MT_COLOR_QUOTED))
          priv->topline = up_n_lines(1, priv->line_info, priv->topline, priv->hide_quoted);
        else
          pager_queue_redraw(priv, MENU_REDRAW_BODY);
        break;

        //=======================================================================

      case OP_PAGER_SKIP_QUOTED:
      {
        if (!priv->has_types)
          break;

        const short c_skip_quoted_context =
            cs_subset_number(NeoMutt->sub, "pager_skip_quoted_context");
        int dretval = 0;
        int new_topline = priv->topline;
        int num_quoted = 0;

        /* In a header? Skip all the email headers, and done */
        if (mutt_color_is_header(priv->line_info[new_topline].type))
        {
          while (((new_topline < priv->last_line) ||
                  (0 == (dretval = display_line(
                             priv->fp, &priv->last_pos, &priv->line_info,
                             new_topline, &priv->last_line, &priv->max_line,
                             MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                             &priv->quote_list, &priv->q_level, &priv->force_redraw,
                             &priv->search_re, priv->pview->win_pager)))) &&
                 mutt_color_is_header(priv->line_info[new_topline].type))
          {
            new_topline++;
          }
          priv->topline = new_topline;
          break;
        }

        /* Already in the body? Skip past previous "context" quoted lines */
        if (c_skip_quoted_context > 0)
        {
          while (((new_topline < priv->last_line) ||
                  (0 == (dretval = display_line(
                             priv->fp, &priv->last_pos, &priv->line_info,
                             new_topline, &priv->last_line, &priv->max_line,
                             MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                             &priv->quote_list, &priv->q_level, &priv->force_redraw,
                             &priv->search_re, priv->pview->win_pager)))) &&
                 (priv->line_info[new_topline].type == MT_COLOR_QUOTED))
          {
            new_topline++;
            num_quoted++;
          }

          if (dretval < 0)
          {
            mutt_error(_("No more unquoted text after quoted text"));
            break;
          }
        }

        if (num_quoted <= c_skip_quoted_context)
        {
          num_quoted = 0;

          while (((new_topline < priv->last_line) ||
                  (0 == (dretval = display_line(
                             priv->fp, &priv->last_pos, &priv->line_info,
                             new_topline, &priv->last_line, &priv->max_line,
                             MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                             &priv->quote_list, &priv->q_level, &priv->force_redraw,
                             &priv->search_re, priv->pview->win_pager)))) &&
                 (priv->line_info[new_topline].type != MT_COLOR_QUOTED))
          {
            new_topline++;
          }

          if (dretval < 0)
          {
            mutt_error(_("No more quoted text"));
            break;
          }

          while (((new_topline < priv->last_line) ||
                  (0 == (dretval = display_line(
                             priv->fp, &priv->last_pos, &priv->line_info,
                             new_topline, &priv->last_line, &priv->max_line,
                             MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                             &priv->quote_list, &priv->q_level, &priv->force_redraw,
                             &priv->search_re, priv->pview->win_pager)))) &&
                 (priv->line_info[new_topline].type == MT_COLOR_QUOTED))
          {
            new_topline++;
            num_quoted++;
          }

          if (dretval < 0)
          {
            mutt_error(_("No more unquoted text after quoted text"));
            break;
          }
        }
        priv->topline = new_topline - MIN(c_skip_quoted_context, num_quoted);
        break;
      }

        //=======================================================================

      case OP_PAGER_SKIP_HEADERS:
      {
        if (!priv->has_types)
          break;

        int dretval = 0;
        int new_topline = 0;

        while (((new_topline < priv->last_line) ||
                (0 == (dretval = display_line(
                           priv->fp, &priv->last_pos, &priv->line_info, new_topline, &priv->last_line,
                           &priv->max_line, MUTT_TYPES | (pview->flags & MUTT_PAGER_NOWRAP),
                           &priv->quote_list, &priv->q_level, &priv->force_redraw,
                           &priv->search_re, priv->pview->win_pager)))) &&
               mutt_color_is_header(priv->line_info[new_topline].type))
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
          break;
        }
        priv->topline = new_topline;
        break;
      }

        //=======================================================================

      case OP_PAGER_BOTTOM: /* move to the end of the file */
        if (!jump_to_bottom(priv, pview))
        {
          mutt_message(_("Bottom of message is shown"));
        }
        break;

        //=======================================================================

      case OP_REDRAW:
        mutt_window_reflow(NULL);
        clearok(stdscr, true);
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_NULL:
        km_error_key(MENU_PAGER);
        break;

      //=======================================================================
      // The following are operations on the current message rather than
      // adjusting the view of the message.
      //=======================================================================
      case OP_BOUNCE_MESSAGE:
      {
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_bounce(shared->mailbox, pview->pdata->fp,
                             pview->pdata->actx, pview->pdata->body);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, shared->email);
          ci_bounce_message(shared->mailbox, &el);
          emaillist_clear(&el);
        }
        break;
      }

        //=======================================================================

      case OP_RESEND:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_resend(pview->pdata->fp, ctx_mailbox(shared->ctx),
                             pview->pdata->actx, pview->pdata->body);
        }
        else
        {
          mutt_resend_message(NULL, ctx_mailbox(shared->ctx), shared->email,
                              NeoMutt->sub);
        }
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_COMPOSE_TO_SENDER:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_mail_sender(pview->pdata->fp, shared->email,
                                  pview->pdata->actx, pview->pdata->body);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, shared->email);

          mutt_send_message(SEND_TO_SENDER, NULL, NULL,
                            ctx_mailbox(shared->ctx), &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_CHECK_TRADITIONAL:
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        if (!(shared->email->security & PGP_TRADITIONAL_CHECKED))
        {
          op = -1;
          rc = OP_CHECK_TRADITIONAL;
        }
        break;

        //=======================================================================

      case OP_CREATE_ALIAS:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        struct AddressList *al = NULL;
        if (pview->mode == PAGER_MODE_ATTACH_E)
          al = mutt_get_address(pview->pdata->body->email->env, NULL);
        else
          al = mutt_get_address(shared->email->env, NULL);
        alias_create(al, NeoMutt->sub);
        break;

        //=======================================================================

      case OP_PURGE_MESSAGE:
      case OP_DELETE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(shared->mailbox))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE,
                                        _("Can't delete message")))
        {
          break;
        }

        mutt_set_flag(shared->mailbox, shared->email, MUTT_DELETE, true);
        mutt_set_flag(shared->mailbox, shared->email, MUTT_PURGE, (op == OP_PURGE_MESSAGE));
        const bool c_delete_untag =
            cs_subset_bool(NeoMutt->sub, "delete_untag");
        if (c_delete_untag)
          mutt_set_flag(shared->mailbox, shared->email, MUTT_TAG, false);
        pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;
      }

        //=======================================================================

      case OP_MAIN_SET_FLAG:
      case OP_MAIN_CLEAR_FLAG:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(shared->mailbox))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, shared->email);

        if (mutt_change_flag(shared->mailbox, &el, (op == OP_MAIN_SET_FLAG)) == 0)
          pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (shared->email->deleted && c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        emaillist_clear(&el);
        break;
      }

        //=======================================================================

      case OP_DELETE_THREAD:
      case OP_DELETE_SUBTHREAD:
      case OP_PURGE_THREAD:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(shared->mailbox))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
           delete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this.  */
        if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE,
                                        _("Can't delete messages")))
        {
          break;
        }

        int subthread = (op == OP_DELETE_SUBTHREAD);
        int r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_DELETE, 1, subthread);
        if (r == -1)
          break;
        if (op == OP_PURGE_THREAD)
        {
          r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_PURGE, true, subthread);
          if (r == -1)
            break;
        }

        const bool c_delete_untag =
            cs_subset_bool(NeoMutt->sub, "delete_untag");
        if (c_delete_untag)
          mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_TAG, 0, subthread);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          rc = OP_MAIN_NEXT_UNDELETED;
          op = -1;
        }

        if (!c_resolve &&
            (cs_subset_number(NeoMutt->sub, "pager_index_lines") != 0))
          pager_queue_redraw(priv, MENU_REDRAW_FULL);
        else
          pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);

        break;
      }

        //=======================================================================

      case OP_DISPLAY_ADDRESS:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH_E)
          mutt_display_address(pview->pdata->body->email->env);
        else
          mutt_display_address(shared->email->env);
        break;

        //=======================================================================

      case OP_ENTER_COMMAND:
        mutt_enter_command();
        pager_queue_redraw(priv, MENU_REDRAW_FULL);

        if (OptNeedResort)
        {
          OptNeedResort = false;
          if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
            break;
          OptNeedResort = true;
        }

        if ((priv->redraw & MENU_REDRAW_FLOW) && (pview->flags & MUTT_PAGER_RETWINCH))
        {
          op = -1;
          rc = OP_REFORMAT_WINCH;
          continue;
        }

        op = 0;
        break;

        //=======================================================================

      case OP_FLAG_MESSAGE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(shared->mailbox))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_WRITE, "Can't flag message"))
          break;

        mutt_set_flag(shared->mailbox, shared->email, MUTT_FLAG, !shared->email->flagged);
        pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;
      }

        //=======================================================================

      case OP_PIPE:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH)))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH)
        {
          mutt_pipe_attachment_list(pview->pdata->actx, pview->pdata->fp, false,
                                    pview->pdata->body, false);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, shared->ctx, shared->email, false);
          mutt_pipe_message(shared->mailbox, &el);
          emaillist_clear(&el);
        }
        break;

        //=======================================================================

      case OP_PRINT:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH)))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH)
        {
          mutt_print_attachment_list(pview->pdata->actx, pview->pdata->fp,
                                     false, pview->pdata->body);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          el_add_tagged(&el, shared->ctx, shared->email, false);
          mutt_print_message(shared->mailbox, &el);
          emaillist_clear(&el);
        }
        break;

        //=======================================================================

      case OP_MAIL:
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;

        mutt_send_message(SEND_NO_FLAGS, NULL, NULL, ctx_mailbox(shared->ctx),
                          NULL, NeoMutt->sub);
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;

        //=======================================================================

#ifdef USE_NNTP
      case OP_POST:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        const enum QuadOption c_post_moderated =
            cs_subset_quad(NeoMutt->sub, "post_moderated");
        if ((shared->mailbox->type == MUTT_NNTP) &&
            !((struct NntpMboxData *) shared->mailbox->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
        {
          break;
        }

        mutt_send_message(SEND_NEWS, NULL, NULL, ctx_mailbox(shared->ctx), NULL,
                          NeoMutt->sub);
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_FORWARD_TO_GROUP:
      {
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        const enum QuadOption c_post_moderated =
            cs_subset_quad(NeoMutt->sub, "post_moderated");
        if ((shared->mailbox->type == MUTT_NNTP) &&
            !((struct NntpMboxData *) shared->mailbox->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
        {
          break;
        }
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_forward(pview->pdata->fp, shared->email,
                              pview->pdata->actx, pview->pdata->body, SEND_NEWS);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, shared->email);

          mutt_send_message(SEND_NEWS | SEND_FORWARD, NULL, NULL,
                            ctx_mailbox(shared->ctx), &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_FOLLOWUP:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;

        char *followup_to = NULL;
        if (pview->mode == PAGER_MODE_ATTACH_E)
          followup_to = pview->pdata->body->email->env->followup_to;
        else
          followup_to = shared->email->env->followup_to;

        const enum QuadOption c_followup_to_poster =
            cs_subset_quad(NeoMutt->sub, "followup_to_poster");
        if (!followup_to || !mutt_istr_equal(followup_to, "poster") ||
            (query_quadoption(c_followup_to_poster,
                              _("Reply by mail as poster prefers?")) != MUTT_YES))
        {
          const enum QuadOption c_post_moderated =
              cs_subset_quad(NeoMutt->sub, "post_moderated");
          if ((shared->mailbox->type == MUTT_NNTP) &&
              !((struct NntpMboxData *) shared->mailbox->mdata)->allowed && (query_quadoption(c_post_moderated, _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES))
          {
            break;
          }
          if (pview->mode == PAGER_MODE_ATTACH_E)
          {
            mutt_attach_reply(pview->pdata->fp, shared->mailbox, shared->email,
                              pview->pdata->actx, pview->pdata->body, SEND_NEWS | SEND_REPLY);
          }
          else
          {
            struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
            emaillist_add_email(&el, shared->email);
            mutt_send_message(SEND_NEWS | SEND_REPLY, NULL, NULL,
                              ctx_mailbox(shared->ctx), &el, NeoMutt->sub);
            emaillist_clear(&el);
          }
          pager_queue_redraw(priv, MENU_REDRAW_FULL);
          break;
        }

        //=======================================================================

#endif
      /* fallthrough */
      case OP_REPLY:
      case OP_GROUP_REPLY:
      case OP_GROUP_CHAT_REPLY:
      case OP_LIST_REPLY:
      {
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;

        SendFlags replyflags = SEND_REPLY;
        if (op == OP_GROUP_REPLY)
          replyflags |= SEND_GROUP_REPLY;
        else if (op == OP_GROUP_CHAT_REPLY)
          replyflags |= SEND_GROUP_CHAT_REPLY;
        else if (op == OP_LIST_REPLY)
          replyflags |= SEND_LIST_REPLY;

        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_reply(pview->pdata->fp, shared->mailbox, shared->email,
                            pview->pdata->actx, pview->pdata->body, replyflags);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, shared->email);
          mutt_send_message(replyflags, NULL, NULL, ctx_mailbox(shared->ctx),
                            &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_RECALL_MESSAGE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, shared->email);

        mutt_send_message(SEND_POSTPONED, NULL, NULL, ctx_mailbox(shared->ctx),
                          &el, NeoMutt->sub);
        emaillist_clear(&el);
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_FORWARD_MESSAGE:
        if (!assert_pager_mode((pview->mode == PAGER_MODE_EMAIL) ||
                               (pview->mode == PAGER_MODE_ATTACH_E)))
        {
          break;
        }
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        if (pview->mode == PAGER_MODE_ATTACH_E)
        {
          mutt_attach_forward(pview->pdata->fp, shared->email, pview->pdata->actx,
                              pview->pdata->body, SEND_NO_FLAGS);
        }
        else
        {
          struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
          emaillist_add_email(&el, shared->email);

          mutt_send_message(SEND_FORWARD, NULL, NULL, ctx_mailbox(shared->ctx),
                            &el, NeoMutt->sub);
          emaillist_clear(&el);
        }
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_DECRYPT_SAVE:
        if (!WithCrypto)
        {
          op = -1;
          break;
        }
        /* fallthrough */
        //=======================================================================

      case OP_SAVE:
        if (pview->mode == PAGER_MODE_ATTACH)
        {
          mutt_save_attachment_list(pview->pdata->actx, pview->pdata->fp, false,
                                    pview->pdata->body, shared->email, NULL);
          break;
        }
        /* fallthrough */
        //=======================================================================

      case OP_COPY_MESSAGE:
      case OP_DECODE_SAVE:
      case OP_DECODE_COPY:
      case OP_DECRYPT_COPY:
      {
        if (!(WithCrypto != 0) && (op == OP_DECRYPT_COPY))
        {
          op = -1;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, shared->email);

        const enum MessageSaveOpt save_opt =
            ((op == OP_SAVE) || (op == OP_DECODE_SAVE) || (op == OP_DECRYPT_SAVE)) ?
                SAVE_MOVE :
                SAVE_COPY;

        enum MessageTransformOpt transform_opt =
            ((op == OP_DECODE_SAVE) || (op == OP_DECODE_COPY)) ? TRANSFORM_DECODE :
            ((op == OP_DECRYPT_SAVE) || (op == OP_DECRYPT_COPY)) ? TRANSFORM_DECRYPT :
                                                                   TRANSFORM_NONE;

        const int rc2 = mutt_save_message(shared->mailbox, &el, save_opt, transform_opt);
        if ((rc2 == 0) && (save_opt == SAVE_MOVE))
        {
          const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
          if (c_resolve)
          {
            op = -1;
            rc = OP_MAIN_NEXT_UNDELETED;
          }
          else
            pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        }
        emaillist_clear(&el);
        break;
      }

        //=======================================================================

      case OP_SHELL_ESCAPE:
        if (mutt_shell_escape())
        {
          mutt_mailbox_check(shared->mailbox, MUTT_MAILBOX_CHECK_FORCE);
        }
        break;

        //=======================================================================

      case OP_TAG:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        mutt_set_flag(shared->mailbox, shared->email, MUTT_TAG, !shared->email->tagged);

        pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_NEXT_ENTRY;
        }
        break;
      }

        //=======================================================================

      case OP_TOGGLE_NEW:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(shared->mailbox))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_SEEN, _("Can't toggle new")))
          break;

        if (shared->email->read || shared->email->old)
          mutt_set_flag(shared->mailbox, shared->email, MUTT_NEW, true);
        else if (!first || (delay_read_timestamp != 0))
          mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true);
        delay_read_timestamp = 0;
        first = false;
        shared->ctx->msg_in_pager = -1;
        priv->win_pbar->actions |= WA_RECALC;
        pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_MAIN_NEXT_UNDELETED;
        }
        break;
      }

        //=======================================================================

      case OP_UNDELETE:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(shared->mailbox))
          break;
        /* L10N: CHECK_ACL */
        if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE,
                                        _("Can't undelete message")))
        {
          break;
        }

        mutt_set_flag(shared->mailbox, shared->email, MUTT_DELETE, false);
        mutt_set_flag(shared->mailbox, shared->email, MUTT_PURGE, false);
        pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
        if (c_resolve)
        {
          op = -1;
          rc = OP_NEXT_ENTRY;
        }
        break;
      }

        //=======================================================================

      case OP_UNDELETE_THREAD:
      case OP_UNDELETE_SUBTHREAD:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (!assert_mailbox_writable(shared->mailbox))
          break;
        /* L10N: CHECK_ACL */
        /* L10N: Due to the implementation details we do not know whether we
           undelete zero, 1, 12, ... messages. So in English we use
           "messages". Your language might have other means to express this. */
        if (!assert_mailbox_permissions(shared->mailbox, MUTT_ACL_DELETE,
                                        _("Can't undelete messages")))
        {
          break;
        }

        int r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_DELETE,
                                     false, (op != OP_UNDELETE_THREAD));
        if (r != -1)
        {
          r = mutt_thread_set_flag(shared->mailbox, shared->email, MUTT_PURGE,
                                   false, (op != OP_UNDELETE_THREAD));
        }
        if (r != -1)
        {
          const bool c_resolve = cs_subset_bool(NeoMutt->sub, "resolve");
          if (c_resolve)
          {
            rc = (op == OP_DELETE_THREAD) ? OP_MAIN_NEXT_THREAD : OP_MAIN_NEXT_SUBTHREAD;
            op = -1;
          }

          if (!c_resolve &&
              (cs_subset_number(NeoMutt->sub, "pager_index_lines") != 0))
            pager_queue_redraw(priv, MENU_REDRAW_FULL);
          else
            pager_queue_redraw(priv, MENU_REDRAW_STATUS | MENU_REDRAW_INDEX);
        }
        break;
      }

        //=======================================================================

      case OP_VERSION:
        mutt_message(mutt_make_version());
        break;

        //=======================================================================

      case OP_MAILBOX_LIST:
        mutt_mailbox_list();
        break;

        //=======================================================================

      case OP_VIEW_ATTACHMENTS:
        if (pview->flags & MUTT_PAGER_ATTACHMENT)
        {
          op = -1;
          rc = OP_ATTACH_COLLAPSE;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        dlg_select_attachment(NeoMutt->sub, ctx_mailbox(Context), shared->email,
                              pview->pdata->fp);
        if (shared->email->attach_del)
          shared->mailbox->changed = true;
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;

        //=======================================================================

      case OP_MAIL_KEY:
      {
        if (!(WithCrypto & APPLICATION_PGP))
        {
          op = -1;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        if (assert_attach_msg_mode(OptAttachMsg))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, shared->email);

        mutt_send_message(SEND_KEY, NULL, NULL, ctx_mailbox(shared->ctx), &el,
                          NeoMutt->sub);
        emaillist_clear(&el);
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_EDIT_LABEL:
      {
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;

        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, shared->email);
        rc = mutt_label_message(shared->mailbox, &el);
        emaillist_clear(&el);

        if (rc > 0)
        {
          shared->mailbox->changed = true;
          pager_queue_redraw(priv, MENU_REDRAW_FULL);
          mutt_message(ngettext("%d label changed", "%d labels changed", rc), rc);
        }
        else
        {
          mutt_message(_("No labels changed"));
        }
        break;
      }

        //=======================================================================

      case OP_FORGET_PASSPHRASE:
        crypt_forget_passphrase();
        break;

        //=======================================================================

      case OP_EXTRACT_KEYS:
      {
        if (!WithCrypto)
        {
          op = -1;
          break;
        }
        if (!assert_pager_mode(pview->mode == PAGER_MODE_EMAIL))
          break;
        struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
        emaillist_add_email(&el, shared->email);
        crypt_extract_keys_from_messages(shared->mailbox, &el);
        emaillist_clear(&el);
        pager_queue_redraw(priv, MENU_REDRAW_FULL);
        break;
      }

        //=======================================================================

      case OP_WHAT_KEY:
        mutt_what_key();
        break;

        //=======================================================================

      case OP_CHECK_STATS:
        mutt_check_stats(shared->mailbox);
        break;

        //=======================================================================

#ifdef USE_SIDEBAR
      case OP_SIDEBAR_FIRST:
      case OP_SIDEBAR_LAST:
      case OP_SIDEBAR_NEXT:
      case OP_SIDEBAR_NEXT_NEW:
      case OP_SIDEBAR_PAGE_DOWN:
      case OP_SIDEBAR_PAGE_UP:
      case OP_SIDEBAR_PREV:
      case OP_SIDEBAR_PREV_NEW:
      {
        struct MuttWindow *win_sidebar = window_find_child(dlg, WT_SIDEBAR);
        if (!win_sidebar)
          break;
        sb_change_mailbox(win_sidebar, op);
        break;
      }

        //=======================================================================

      case OP_SIDEBAR_TOGGLE_VISIBLE:
        bool_str_toggle(NeoMutt->sub, "sidebar_visible", NULL);
        mutt_window_reflow(dlg);
        break;

        //=======================================================================
#endif

      default:
        op = -1;
        break;
    }
  }
  //-------------------------------------------------------------------------
  // END OF ACT 3: Read user input loop - while (op != -1)
  //-------------------------------------------------------------------------

  if (check_read_delay(&delay_read_timestamp))
  {
    mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true);
  }
  mutt_file_fclose(&priv->fp);
  if (pview->mode == PAGER_MODE_EMAIL)
  {
    shared->ctx->msg_in_pager = -1;
    priv->win_pbar->actions |= WA_RECALC;
    switch (rc)
    {
      case -1:
      case OP_DISPLAY_HEADERS:
        mutt_clear_pager_position();
        break;
      default:
        TopLine = priv->topline;
        OldEmail = shared->email;
        break;
    }
  }

  cleanup_quote(&priv->quote_list);

  for (size_t i = 0; i < priv->max_line; i++)
  {
    FREE(&(priv->line_info[i].syntax));
    if (priv->search_compiled && priv->line_info[i].search)
      FREE(&(priv->line_info[i].search));
  }
  if (priv->search_compiled)
  {
    regfree(&priv->search_re);
    priv->search_compiled = false;
  }
  FREE(&priv->line_info);

  expand_index_panel(dlg, priv);

  return (rc != -1) ? rc : 0;
}
