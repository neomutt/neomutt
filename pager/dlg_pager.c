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
 * @page pager_dlg_pager Pager Dialog
 *
 * The Pager Dialog displays some text to the user that can be paged through.
 * The actual contents depend on the caller, but are usually an email, file or help.
 *
 * This dialog doesn't exist on its own.  The @ref pager_ppanel is packaged up
 * as part of the @ref index_dlg_index or the @ref pager_dopager.
 */

#include "config.h"
#include <assert.h>
#include <inttypes.h> // IWYU pragma: keep
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "lib.h"
#include "color/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "context.h"
#include "display.h"
#include "functions.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "private_data.h"
#include "protos.h"
#include "status.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
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

#ifdef USE_DEBUG_COLOR
  dump_pager(priv);
#endif
  char buf[1024] = { 0 };
  struct IndexSharedData *shared = dialog_find(priv->pview->win_pager)->wdata;

  const bool c_tilde = cs_subset_bool(NeoMutt->sub, "tilde");
  const short c_pager_index_lines =
      cs_subset_number(NeoMutt->sub, "pager_index_lines");

  if (priv->redraw & MENU_REDRAW_FULL)
  {
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
    mutt_window_clear(priv->pview->win_pager);

    if ((priv->pview->mode == PAGER_MODE_EMAIL) &&
        ((shared->mailbox->vcount + 1) < c_pager_index_lines))
    {
      priv->index_size = shared->mailbox->vcount + 1;
    }
    else
    {
      priv->index_size = c_pager_index_lines;
    }

    priv->indicator = priv->index_size / 3;

    if (Resize)
    {
      priv->search_compiled = Resize->search_compiled;
      if (priv->search_compiled)
      {
        uint16_t flags = mutt_mb_is_lower(priv->search_str) ? REG_ICASE : 0;
        const int err = REG_COMP(&priv->search_re, priv->search_str, REG_NEWLINE | flags);
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
      priv->win_height = Resize->line;
      pager_queue_redraw(priv, MENU_REDRAW_FLOW);

      FREE(&Resize);
    }

    if ((priv->pview->mode == PAGER_MODE_EMAIL) && (c_pager_index_lines != 0) && priv->menu)
    {
      mutt_curses_set_color_by_id(MT_COLOR_NORMAL);

      /* some fudge to work out whereabouts the indicator should go */
      const int index = menu_get_index(priv->menu);
      if ((index - priv->indicator) < 0)
        priv->menu->top = 0;
      else if ((priv->menu->max - index) < (priv->menu->page_len - priv->indicator))
        priv->menu->top = priv->menu->max - priv->menu->page_len;
      else
        priv->menu->top = index - priv->indicator;

      menu_redraw_index(priv->menu);
    }

    pager_queue_redraw(priv, MENU_REDRAW_BODY | MENU_REDRAW_INDEX);
  }

  // We need to populate more lines, but not change position
  const bool repopulate = (priv->cur_line > priv->lines_used);
  if ((priv->redraw & MENU_REDRAW_FLOW) || repopulate)
  {
    if (!(priv->pview->flags & MUTT_PAGER_RETWINCH))
    {
      priv->win_height = -1;
      for (int i = 0; i <= priv->top_line; i++)
        if (!priv->lines[i].cont_line)
          priv->win_height++;
      for (int i = 0; i < priv->lines_max; i++)
      {
        priv->lines[i].offset = 0;
        priv->lines[i].cid = -1;
        priv->lines[i].cont_line = 0;
        priv->lines[i].syntax_arr_size = 0;
        priv->lines[i].search_arr_size = -1;
        priv->lines[i].quote = NULL;

        mutt_mem_realloc(&(priv->lines[i].syntax), sizeof(struct TextSyntax));
        if (priv->search_compiled && priv->lines[i].search)
          FREE(&(priv->lines[i].search));
      }

      if (!repopulate)
      {
        priv->lines_used = 0;
        priv->top_line = 0;
      }
    }
    int i = -1;
    int j = -1;
    while (display_line(priv->fp, &priv->bytes_read, &priv->lines, ++i,
                        &priv->lines_used, &priv->lines_max,
                        priv->has_types | priv->search_flag | (priv->pview->flags & MUTT_PAGER_NOWRAP),
                        &priv->quote_list, &priv->q_level, &priv->force_redraw,
                        &priv->search_re, priv->pview->win_pager, &priv->ansi_list) == 0)
    {
      if (!priv->lines[i].cont_line && (++j == priv->win_height))
      {
        if (!repopulate)
          priv->top_line = i;
        if (!priv->search_flag)
          break;
      }
    }
  }

  if ((priv->redraw & MENU_REDRAW_BODY) || (priv->top_line != priv->old_top_line))
  {
    do
    {
      mutt_window_move(priv->pview->win_pager, 0, 0);
      priv->cur_line = priv->top_line;
      priv->old_top_line = priv->top_line;
      priv->win_height = 0;
      priv->force_redraw = false;

      while ((priv->win_height < priv->pview->win_pager->state.rows) &&
             (priv->lines[priv->cur_line].offset <= priv->st.st_size - 1))
      {
        if (display_line(priv->fp, &priv->bytes_read, &priv->lines,
                         priv->cur_line, &priv->lines_used, &priv->lines_max,
                         (priv->pview->flags & MUTT_DISPLAYFLAGS) | priv->hide_quoted |
                             priv->search_flag | (priv->pview->flags & MUTT_PAGER_NOWRAP),
                         &priv->quote_list, &priv->q_level, &priv->force_redraw,
                         &priv->search_re, priv->pview->win_pager, &priv->ansi_list) > 0)
        {
          priv->win_height++;
        }
        priv->cur_line++;
        mutt_window_move(priv->pview->win_pager, 0, priv->win_height);
      }
    } while (priv->force_redraw);
    // curses_colors_dump();
    // attr_color_list_dump(&priv->ansi_list, "All AnsiColors");

    mutt_curses_set_color_by_id(MT_COLOR_TILDE);
    while (priv->win_height < priv->pview->win_pager->state.rows)
    {
      mutt_window_clrtoeol(priv->pview->win_pager);
      if (c_tilde)
        mutt_window_addch(priv->pview->win_pager, '~');
      priv->win_height++;
      mutt_window_move(priv->pview->win_pager, 0, priv->win_height);
    }
    mutt_curses_set_color_by_id(MT_COLOR_NORMAL);
  }

  priv->redraw = MENU_REDRAW_NO_FLAGS;
  mutt_debug(LL_DEBUG5, "repaint done\n");
}

/**
 * pager_resolve_help_mapping - Determine help mapping based on pager mode and mailbox type
 * @param mode pager mode
 * @param type mailbox type
 * @retval ptr Help Mapping
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
  struct MuttWindow *win_sidebar = window_find_child(dlg, WT_SIDEBAR);

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
      //  - fp and body->email in special case of viewing an attached email.
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
  enum MailboxType mailbox_type = shared->mailbox ? shared->mailbox->type : MUTT_UNKNOWN;
  struct PagerPrivateData *priv = pview->win_pager->parent->wdata;
  priv->rc = -1;
  priv->searchctx = 0;
  priv->first = true;
  priv->wrapped = false;
  priv->delay_read_timestamp = 0;
  bool pager_redraw = false;

  {
    // Wipe any previous state info
    struct Menu *menu = priv->menu;
    struct Notify *notify = priv->notify;
    int rc = priv->rc;
    memset(priv, 0, sizeof(*priv));
    priv->rc = rc;
    priv->menu = menu;
    priv->notify = notify;
    priv->win_pbar = pview->win_pbar;
    TAILQ_INIT(&priv->ansi_list);
  }

  //---------- setup flags ----------------------------------------------------
  if (!(pview->flags & MUTT_SHOWCOLOR))
    pview->flags |= MUTT_SHOWFLAT;

  if ((pview->mode == PAGER_MODE_EMAIL) && !shared->email->read)
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
      priv->delay_read_timestamp = mutt_date_epoch_ms() + (1000 * c_pager_read_delay);
    }
  }
  //---------- setup help menu ------------------------------------------------
  pview->win_pager->help_data = pager_resolve_help_mapping(pview->mode, mailbox_type);
  pview->win_pager->help_menu = MENU_PAGER;

  //---------- initialize redraw pdata  -----------------------------------------
  pview->win_pager->size = MUTT_WIN_SIZE_MAXIMISE;
  priv->pview = pview;
  priv->index_size = c_pager_index_lines;
  priv->indicator = priv->index_size / 3;
  priv->lines_max = LINES; // number of lines on screen, from curses
  priv->lines = mutt_mem_calloc(priv->lines_max, sizeof(struct Line));
  priv->fp = fopen(pview->pdata->fname, "r");
  priv->has_types =
      ((pview->mode == PAGER_MODE_EMAIL) || (pview->flags & MUTT_SHOWCOLOR)) ?
          MUTT_TYPES :
          0; // main message or rfc822 attachment

  for (size_t i = 0; i < priv->lines_max; i++)
  {
    priv->lines[i].cid = -1;
    priv->lines[i].search_arr_size = -1;
    priv->lines[i].syntax = mutt_mem_malloc(sizeof(struct TextSyntax));
    (priv->lines[i].syntax)[0].first = -1;
    (priv->lines[i].syntax)[0].last = -1;
  }

  // ---------- try to open the pdata file -------------------------------------
  if (!priv->fp)
  {
    mutt_perror(pview->pdata->fname);
    return -1;
  }

  if (stat(pview->pdata->fname, &priv->st) != 0)
  {
    mutt_perror(pview->pdata->fname);
    mutt_file_fclose(&priv->fp);
    return -1;
  }
  unlink(pview->pdata->fname);

  //---------- restore global state if needed ---------------------------------
  while ((pview->mode == PAGER_MODE_EMAIL) && (OldEmail == shared->email) && // are we "resuming" to the same Email?
         (TopLine != priv->top_line) && // is saved offset different?
         (priv->lines[priv->cur_line].offset < (priv->st.st_size - 1)))
  {
    pager_queue_redraw(priv, MENU_REDRAW_FULL);
    pager_custom_redraw(priv);
    // trick user, as if nothing happened
    // scroll down to previously saved offset
    priv->top_line = ((TopLine - priv->top_line) > priv->win_height) ?
                         priv->top_line + priv->win_height :
                         TopLine;
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
  while (op != OP_ABORT)
  {
    if (check_read_delay(&priv->delay_read_timestamp))
    {
      mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true);
    }

    pager_queue_redraw(priv, MENU_REDRAW_FULL);
    pager_custom_redraw(priv);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
    window_redraw(NULL);

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

    if (pager_redraw)
    {
      pager_redraw = false;
      mutt_resize_screen();
      clearok(stdscr, true); /* force complete redraw */
      msgwin_clear_text();

      pager_queue_redraw(priv, MENU_REDRAW_FLOW);
      if (pview->flags & MUTT_PAGER_RETWINCH)
      {
        /* Store current position. */
        priv->win_height = -1;
        for (size_t i = 0; i <= priv->top_line; i++)
          if (!priv->lines[i].cont_line)
            priv->win_height++;

        Resize = mutt_mem_malloc(sizeof(struct Resize));

        Resize->line = priv->win_height;
        Resize->search_compiled = priv->search_compiled;
        Resize->search_back = priv->search_back;

        op = OP_ABORT;
        priv->rc = OP_REFORMAT_WINCH;
      }
      else
      {
        /* note: mutt_resize_screen() -> mutt_window_reflow() sets
         * MENU_REDRAW_FULL and MENU_REDRAW_FLOW */
        op = OP_NULL;
      }
      continue;
    }

#ifdef USE_DEBUG_COLOR
    dump_pager(priv);
#endif

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
    if (SigWinch)
      pager_redraw = true;

    if (op >= OP_NULL)
      mutt_clear_error();

    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);

    if (op < OP_NULL)
    {
      op = OP_NULL;
      mutt_timeout_hook();
      continue;
    }

    priv->rc = op;

    if (op == OP_NULL)
    {
      km_error_key(MENU_PAGER);
      continue;
    }

    int rc = pager_function_dispatcher(priv->pview->win_pager, op);

#ifdef USE_SIDEBAR
    if (rc == FR_UNKNOWN)
      rc = sb_function_dispatcher(win_sidebar, op);
#endif
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(dlg, op);

    if (rc == FR_DONE)
      break;
    if (rc == FR_UNKNOWN)
      break;
  }
  //-------------------------------------------------------------------------
  // END OF ACT 3: Read user input loop - while (op != OP_ABORT)
  //-------------------------------------------------------------------------

  if (check_read_delay(&priv->delay_read_timestamp))
  {
    mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true);
  }
  mutt_file_fclose(&priv->fp);
  if (pview->mode == PAGER_MODE_EMAIL)
  {
    shared->ctx->msg_in_pager = -1;
    priv->win_pbar->actions |= WA_RECALC;
    switch (priv->rc)
    {
      case -1:
      case OP_DISPLAY_HEADERS:
        mutt_clear_pager_position();
        break;
      default:
        TopLine = priv->top_line;
        OldEmail = shared->email;
        break;
    }
  }

  qstyle_free_tree(&priv->quote_list);

  for (size_t i = 0; i < priv->lines_max; i++)
  {
    FREE(&(priv->lines[i].syntax));
    if (priv->search_compiled && priv->lines[i].search)
      FREE(&(priv->lines[i].search));
  }
  if (priv->search_compiled)
  {
    regfree(&priv->search_re);
    priv->search_compiled = false;
  }
  FREE(&priv->lines);
  attr_color_list_clear(&priv->ansi_list);
  {
    struct AttrColor *ac = NULL;
    int count = 0;
    TAILQ_FOREACH(ac, &priv->ansi_list, entries)
    {
      count++;
    }
    color_debug(LL_DEBUG5, "AnsiColors %d\n", count);
  }

  expand_index_panel(dlg, priv);

  return (priv->rc != -1) ? priv->rc : 0;
}
