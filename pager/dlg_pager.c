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
#include <inttypes.h>
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
#include "key/lib.h"
#include "menu/lib.h"
#include "pattern/lib.h"
#include "display.h"
#include "functions.h"
#include "globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mview.h"
#include "mx.h"
#include "private_data.h"
#include "protos.h"
#include "status.h"
#ifdef USE_SIDEBAR
#include "sidebar/lib.h"
#endif

/// Braille display: row to leave the cursor
int BrailleRow = -1;
/// Braille display: column to leave the cursor
int BrailleCol = -1;

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
 * pager_queue_redraw - Queue a request for a redraw
 * @param priv   Private Pager data
 * @param redraw Item to redraw, e.g. #PAGER_REDRAW_PAGER
 */
void pager_queue_redraw(struct PagerPrivateData *priv, PagerRedrawFlags redraw)
{
  priv->redraw |= redraw;
  priv->pview->win_pager->actions |= WA_RECALC;
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
  if ((*timestamp != 0) && (mutt_date_now_ms() > *timestamp))
  {
    *timestamp = 0;
    return true;
  }
  return false;
}

/**
 * dlg_pager - Display an email, attachment, or help, in a window - @ingroup gui_dlg
 * @param pview Pager view settings
 * @retval  0 Success
 * @retval -1 Error
 *
 * The Pager Dialog displays an Email to the user.
 *
 * They can navigate through the Email, search through it and user `color`
 * commands to highlight it.
 *
 * From the Pager, the user can also use some Index functions, such as
 * `<next-entry>` or `<delete>`.
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
int dlg_pager(struct PagerView *pview)
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
      assert(shared->mailbox_view);
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
      assert(!shared->mailbox_view);
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

  //---------- local variables ------------------------------------------------
  int op = 0;
  enum MailboxType mailbox_type = shared->mailbox ? shared->mailbox->type : MUTT_UNKNOWN;
  struct PagerPrivateData *priv = pview->win_pager->parent->wdata;
  priv->rc = -1;
  priv->searchctx = 0;
  priv->first = true;
  priv->wrapped = false;
  priv->delay_read_timestamp = 0;
  priv->pager_redraw = false;

  // Wipe any previous state info
  struct Notify *notify = priv->notify;
  int prc = priv->rc;
  memset(priv, 0, sizeof(*priv));
  priv->rc = prc;
  priv->notify = notify;
  TAILQ_INIT(&priv->ansi_list);

  //---------- setup flags ----------------------------------------------------
  if (!(pview->flags & MUTT_SHOWCOLOR))
    pview->flags |= MUTT_SHOWFLAT;

  if ((pview->mode == PAGER_MODE_EMAIL) && !shared->email->read)
  {
    if (shared->mailbox_view)
      shared->mailbox_view->msg_in_pager = shared->email->msgno;
    const short c_pager_read_delay = cs_subset_number(NeoMutt->sub, "pager_read_delay");
    if (c_pager_read_delay == 0)
    {
      mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true, true);
    }
    else
    {
      priv->delay_read_timestamp = mutt_date_now_ms() + (1000 * c_pager_read_delay);
    }
  }
  //---------- setup help menu ------------------------------------------------
  pview->win_pager->help_data = pager_resolve_help_mapping(pview->mode, mailbox_type);
  pview->win_pager->help_menu = MENU_PAGER;

  //---------- initialize redraw pdata  -----------------------------------------
  pview->win_pager->size = MUTT_WIN_SIZE_MAXIMISE;
  priv->lines_max = LINES; // number of lines on screen, from curses
  priv->lines = mutt_mem_calloc(priv->lines_max, sizeof(struct Line));
  priv->fp = fopen(pview->pdata->fname, "r");
  priv->has_types = ((pview->mode == PAGER_MODE_EMAIL) || (pview->flags & MUTT_SHOWCOLOR)) ?
                        MUTT_TYPES :
                        0; // main message or rfc822 attachment

  for (size_t i = 0; i < priv->lines_max; i++)
  {
    priv->lines[i].cid = -1;
    priv->lines[i].search_arr_size = -1;
    priv->lines[i].syntax = mutt_mem_calloc(1, sizeof(struct TextSyntax));
    (priv->lines[i].syntax)[0].first = -1;
    (priv->lines[i].syntax)[0].last = -1;
  }

  // ---------- try to open the pdata file -------------------------------------
  if (!priv->fp)
  {
    mutt_perror("%s", pview->pdata->fname);
    return -1;
  }

  if (stat(pview->pdata->fname, &priv->st) != 0)
  {
    mutt_perror("%s", pview->pdata->fname);
    mutt_file_fclose(&priv->fp);
    return -1;
  }
  unlink(pview->pdata->fname);
  priv->pview = pview;

  //---------- show windows, set focus and visibility --------------------------
  window_set_visible(pview->win_pager->parent, true);
  mutt_window_reflow(dlg);
  window_invalidate_all();

  struct MuttWindow *old_focus = window_set_focus(pview->win_pager);

  //---------- jump to the bottom if requested ------------------------------
  if (pview->flags & MUTT_PAGER_BOTTOM)
  {
    jump_to_bottom(priv, pview);
  }

  //-------------------------------------------------------------------------
  // ACT 3: Read user input and decide what to do with it
  //        ...but also do a whole lot of other things.
  //-------------------------------------------------------------------------

  // Force an initial paint, which will populate priv->lines
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  window_redraw(NULL);

  priv->loop = PAGER_LOOP_CONTINUE;
  do
  {
    pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
    window_redraw(NULL);

    const bool c_braille_friendly = cs_subset_bool(NeoMutt->sub, "braille_friendly");
    if (c_braille_friendly)
    {
      if (BrailleRow != -1)
      {
        mutt_window_move(priv->pview->win_pager, BrailleCol, BrailleRow + 1);
        BrailleRow = -1;
      }
    }
    else
    {
      mutt_window_move(priv->pview->win_pbar, priv->pview->win_pager->state.cols - 1, 0);
    }

    // force redraw of the screen at every iteration of the event loop
    mutt_refresh();

    //-------------------------------------------------------------------------
    // Check if information in the status bar needs an update
    // This is done because pager is a single-threaded application, which
    // tries to emulate concurrency.
    //-------------------------------------------------------------------------
    bool do_new_mail = false;
    if (shared->mailbox && !shared->attach_msg)
    {
      int oldcount = shared->mailbox->msg_count;
      /* check for new mail */
      enum MxStatus check = mx_mbox_check(shared->mailbox);
      if (check == MX_STATUS_ERROR)
      {
        if (!shared->mailbox || buf_is_empty(&shared->mailbox->pathbuf))
        {
          /* fatal error occurred */
          pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
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
          pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
          mutt_pattern_free(&shared->search_state->pattern);
        }
      }

      if (mutt_mailbox_notify(shared->mailbox) || do_new_mail)
      {
        const bool c_beep_new = cs_subset_bool(NeoMutt->sub, "beep_new");
        if (c_beep_new)
          mutt_beep(true);
        const char *const c_new_mail_command = cs_subset_string(NeoMutt->sub, "new_mail_command");
        if (c_new_mail_command)
        {
          char cmd[1024] = { 0 };
          menu_status_line(cmd, sizeof(cmd), shared, NULL, sizeof(cmd),
                           NONULL(c_new_mail_command));
          if (mutt_system(cmd) != 0)
            mutt_error(_("Error running \"%s\""), cmd);
        }
      }
    }
    //-------------------------------------------------------------------------

    if (priv->pager_redraw)
    {
      priv->pager_redraw = false;
      mutt_resize_screen();
      clearok(stdscr, true); /* force complete redraw */
      msgwin_clear_text(NULL);

      pager_queue_redraw(priv, PAGER_REDRAW_FLOW);
      if (pview->flags & MUTT_PAGER_RETWINCH)
      {
        /* Store current position. */
        priv->win_height = -1;
        for (size_t i = 0; i <= priv->top_line; i++)
          if (!priv->lines[i].cont_line)
            priv->win_height++;

        op = OP_ABORT;
        priv->rc = OP_REFORMAT_WINCH;
        break;
      }
      else
      {
        /* note: mutt_resize_screen() -> mutt_window_reflow() sets
         * PAGER_REDRAW_PAGER and PAGER_REDRAW_FLOW */
        op = OP_NULL;
      }
      continue;
    }

    dump_pager(priv);

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
    op = km_dokey(MENU_PAGER, GETCH_NO_FLAGS);

    // km_dokey() can block, so recheck the timer.
    // Note: This check must occur before handling the operations of the index
    // as those can change the currently selected message/entry yielding to
    // marking the wrong message as read.
    if (check_read_delay(&priv->delay_read_timestamp))
    {
      mutt_set_flag(shared->mailbox, shared->email, MUTT_READ, true, true);
    }

    if (SigWinch)
      priv->pager_redraw = true;

    if (op >= OP_NULL)
      mutt_clear_error();

    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);

    if (op < OP_NULL)
    {
      op = OP_NULL;
      continue;
    }

    if (op == OP_NULL)
    {
      km_error_key(MENU_PAGER);
      continue;
    }

    int rc = pager_function_dispatcher(priv->pview->win_pager, op);

    if (pview->mode == PAGER_MODE_EMAIL)
    {
      if ((rc == FR_UNKNOWN) && priv->pview->win_index)
        rc = index_function_dispatcher(priv->pview->win_index, op);
#ifdef USE_SIDEBAR
      if (rc == FR_UNKNOWN)
        rc = sb_function_dispatcher(win_sidebar, op);
#endif
    }
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);

    if ((rc == FR_UNKNOWN) &&
        ((pview->mode == PAGER_MODE_ATTACH) || (pview->mode == PAGER_MODE_ATTACH_E)))
    {
      // Some attachment functions still need to be delegated
      priv->rc = op;
      break;
    }

    if ((pview->mode != PAGER_MODE_EMAIL) && (rc == FR_UNKNOWN))
      mutt_flushinp();

  } while (priv->loop == PAGER_LOOP_CONTINUE);
  window_set_focus(old_focus);

  //-------------------------------------------------------------------------
  // END OF ACT 3: Read user input loop - while (op != OP_ABORT)
  //-------------------------------------------------------------------------

  mutt_file_fclose(&priv->fp);
  if (pview->mode == PAGER_MODE_EMAIL)
  {
    if (shared->mailbox_view)
      shared->mailbox_view->msg_in_pager = -1;
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

  priv->pview = NULL;

  if (priv->loop == PAGER_LOOP_RELOAD)
    return PAGER_LOOP_RELOAD;

  return (priv->rc != -1) ? priv->rc : 0;
}
