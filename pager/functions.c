/**
 * @file
 * Pager functions
 *
 * @authors
 * Copyright (C) 2021-2023 Richard Russon <rich@flatcap.org>
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
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
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
#include "display.h"
#include "functions.h"
#include "muttlib.h"
#include "private_data.h"
#include "protos.h"
#endif

/// Error message for unavailable functions
static const char *Not_available_in_this_menu = N_("Not available in this menu");

static int op_pager_search_next(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op);

// clang-format off
/**
 * OpPager - Functions for the Pager Menu
 */
const struct MenuFuncOp OpPager[] = { /* map: pager */
  { "bottom",                        OP_PAGER_BOTTOM },
  { "bounce-message",                OP_BOUNCE_MESSAGE },
  { "break-thread",                  OP_MAIN_BREAK_THREAD },
  { "change-folder",                 OP_MAIN_CHANGE_FOLDER },
  { "change-folder-readonly",        OP_MAIN_CHANGE_FOLDER_READONLY },
  { "change-newsgroup",              OP_MAIN_CHANGE_GROUP },
  { "change-newsgroup-readonly",     OP_MAIN_CHANGE_GROUP_READONLY },
#ifdef USE_NOTMUCH
  { "change-vfolder",                OP_MAIN_CHANGE_VFOLDER },
#endif
  { "check-stats",                   OP_CHECK_STATS },
  { "check-traditional-pgp",         OP_CHECK_TRADITIONAL },
  { "clear-flag",                    OP_MAIN_CLEAR_FLAG },
  { "compose-to-sender",             OP_COMPOSE_TO_SENDER },
  { "copy-message",                  OP_COPY_MESSAGE },
  { "create-alias",                  OP_CREATE_ALIAS },
  { "decode-copy",                   OP_DECODE_COPY },
  { "decode-save",                   OP_DECODE_SAVE },
  { "decrypt-copy",                  OP_DECRYPT_COPY },
  { "decrypt-save",                  OP_DECRYPT_SAVE },
  { "delete-message",                OP_DELETE },
  { "delete-subthread",              OP_DELETE_SUBTHREAD },
  { "delete-thread",                 OP_DELETE_THREAD },
  { "display-address",               OP_DISPLAY_ADDRESS },
  { "display-toggle-weed",           OP_DISPLAY_HEADERS },
  { "edit",                          OP_EDIT_RAW_MESSAGE },
  { "edit-label",                    OP_EDIT_LABEL },
  { "edit-or-view-raw-message",      OP_EDIT_OR_VIEW_RAW_MESSAGE },
  { "edit-raw-message",              OP_EDIT_RAW_MESSAGE },
  { "edit-type",                     OP_ATTACHMENT_EDIT_TYPE },
  { "enter-command",                 OP_ENTER_COMMAND },
#ifdef USE_NOTMUCH
  { "entire-thread",                 OP_MAIN_ENTIRE_THREAD },
#endif
  { "exit",                          OP_EXIT },
  { "extract-keys",                  OP_EXTRACT_KEYS },
  { "flag-message",                  OP_FLAG_MESSAGE },
  { "followup-message",              OP_FOLLOWUP },
  { "forget-passphrase",             OP_FORGET_PASSPHRASE },
  { "forward-message",               OP_FORWARD_MESSAGE },
  { "forward-to-group",              OP_FORWARD_TO_GROUP },
  { "group-chat-reply",              OP_GROUP_CHAT_REPLY },
  { "group-reply",                   OP_GROUP_REPLY },
  { "half-down",                     OP_HALF_DOWN },
  { "half-up",                       OP_HALF_UP },
  { "help",                          OP_HELP },
  { "imap-fetch-mail",               OP_MAIN_IMAP_FETCH },
  { "imap-logout-all",               OP_MAIN_IMAP_LOGOUT_ALL },
  { "jump",                          OP_JUMP },
  { "jump",                          OP_JUMP_1 },
  { "jump",                          OP_JUMP_2 },
  { "jump",                          OP_JUMP_3 },
  { "jump",                          OP_JUMP_4 },
  { "jump",                          OP_JUMP_5 },
  { "jump",                          OP_JUMP_6 },
  { "jump",                          OP_JUMP_7 },
  { "jump",                          OP_JUMP_8 },
  { "jump",                          OP_JUMP_9 },
  { "link-threads",                  OP_MAIN_LINK_THREADS },
  { "list-reply",                    OP_LIST_REPLY },
  { "list-subscribe",                OP_LIST_SUBSCRIBE },
  { "list-unsubscribe",              OP_LIST_UNSUBSCRIBE },
  { "mail",                          OP_MAIL },
  { "mail-key",                      OP_MAIL_KEY },
  { "mailbox-list",                  OP_MAILBOX_LIST },
  { "mark-as-new",                   OP_TOGGLE_NEW },
  { "modify-labels",                 OP_MAIN_MODIFY_TAGS },
  { "modify-labels-then-hide",       OP_MAIN_MODIFY_TAGS_THEN_HIDE },
  { "modify-tags",                   OP_MAIN_MODIFY_TAGS },
  { "modify-tags-then-hide",         OP_MAIN_MODIFY_TAGS_THEN_HIDE },
  { "next-entry",                    OP_NEXT_ENTRY },
  { "next-line",                     OP_NEXT_LINE },
  { "next-new",                      OP_MAIN_NEXT_NEW },
  { "next-new-then-unread",          OP_MAIN_NEXT_NEW_THEN_UNREAD },
  { "next-page",                     OP_NEXT_PAGE },
  { "next-subthread",                OP_MAIN_NEXT_SUBTHREAD },
  { "next-thread",                   OP_MAIN_NEXT_THREAD },
  { "next-undeleted",                OP_MAIN_NEXT_UNDELETED },
  { "next-unread",                   OP_MAIN_NEXT_UNREAD },
  { "next-unread-mailbox",           OP_MAIN_NEXT_UNREAD_MAILBOX },
  { "parent-message",                OP_MAIN_PARENT_MESSAGE },
  { "pipe-entry",                    OP_PIPE },
  { "pipe-message",                  OP_PIPE },
  { "post-message",                  OP_POST },
  { "previous-entry",                OP_PREV_ENTRY },
  { "previous-line",                 OP_PREV_LINE },
  { "previous-new",                  OP_MAIN_PREV_NEW },
  { "previous-new-then-unread",      OP_MAIN_PREV_NEW_THEN_UNREAD },
  { "previous-page",                 OP_PREV_PAGE },
  { "previous-subthread",            OP_MAIN_PREV_SUBTHREAD },
  { "previous-thread",               OP_MAIN_PREV_THREAD },
  { "previous-undeleted",            OP_MAIN_PREV_UNDELETED },
  { "previous-unread",               OP_MAIN_PREV_UNREAD },
  { "print-entry",                   OP_ATTACHMENT_PRINT },
  { "print-message",                 OP_PRINT },
  { "purge-message",                 OP_PURGE_MESSAGE },
  { "purge-thread",                  OP_PURGE_THREAD },
  { "quasi-delete",                  OP_MAIN_QUASI_DELETE },
  { "quit",                          OP_QUIT },
  { "read-subthread",                OP_MAIN_READ_SUBTHREAD },
  { "read-thread",                   OP_MAIN_READ_THREAD },
  { "recall-message",                OP_RECALL_MESSAGE },
  { "reconstruct-thread",            OP_RECONSTRUCT_THREAD },
  { "redraw-screen",                 OP_REDRAW },
  { "reply",                         OP_REPLY },
  { "resend-message",                OP_RESEND },
  { "root-message",                  OP_MAIN_ROOT_MESSAGE },
  { "save-entry",                    OP_ATTACHMENT_SAVE },
  { "save-message",                  OP_SAVE },
  { "search",                        OP_SEARCH },
  { "search-next",                   OP_SEARCH_NEXT },
  { "search-opposite",               OP_SEARCH_OPPOSITE },
  { "search-reverse",                OP_SEARCH_REVERSE },
  { "search-toggle",                 OP_SEARCH_TOGGLE },
  { "set-flag",                      OP_MAIN_SET_FLAG },
  { "shell-escape",                  OP_SHELL_ESCAPE },
  { "show-log-messages",             OP_SHOW_LOG_MESSAGES },
  { "show-version",                  OP_VERSION },
  { "sidebar-first",                 OP_SIDEBAR_FIRST },
  { "sidebar-last",                  OP_SIDEBAR_LAST },
  { "sidebar-next",                  OP_SIDEBAR_NEXT },
  { "sidebar-next-new",              OP_SIDEBAR_NEXT_NEW },
  { "sidebar-open",                  OP_SIDEBAR_OPEN },
  { "sidebar-page-down",             OP_SIDEBAR_PAGE_DOWN },
  { "sidebar-page-up",               OP_SIDEBAR_PAGE_UP },
  { "sidebar-prev",                  OP_SIDEBAR_PREV },
  { "sidebar-prev-new",              OP_SIDEBAR_PREV_NEW },
  { "sidebar-toggle-virtual",        OP_SIDEBAR_TOGGLE_VIRTUAL },
  { "sidebar-toggle-visible",        OP_SIDEBAR_TOGGLE_VISIBLE },
  { "skip-headers",                  OP_PAGER_SKIP_HEADERS },
  { "skip-quoted",                   OP_PAGER_SKIP_QUOTED },
  { "sort-mailbox",                  OP_SORT },
  { "sort-reverse",                  OP_SORT_REVERSE },
  { "sync-mailbox",                  OP_MAIN_SYNC_FOLDER },
  { "tag-message",                   OP_TAG },
  { "toggle-quoted",                 OP_PAGER_HIDE_QUOTED },
  { "toggle-write",                  OP_TOGGLE_WRITE },
  { "top",                           OP_PAGER_TOP },
  { "undelete-message",              OP_UNDELETE },
  { "undelete-subthread",            OP_UNDELETE_SUBTHREAD },
  { "undelete-thread",               OP_UNDELETE_THREAD },
#ifdef USE_NOTMUCH
  { "vfolder-from-query",            OP_MAIN_VFOLDER_FROM_QUERY },
  { "vfolder-from-query-readonly",   OP_MAIN_VFOLDER_FROM_QUERY_READONLY },
#endif
  { "view-attachments",              OP_VIEW_ATTACHMENTS },
  { "view-raw-message",              OP_VIEW_RAW_MESSAGE },
  { "what-key",                      OP_WHAT_KEY },
  // Deprecated
  { "buffy-list",                    OP_MAILBOX_LIST },
  { "error-history",                 OP_SHOW_LOG_MESSAGES },
  { NULL, 0 },
};

/**
 * PagerDefaultBindings - Key bindings for the Pager Menu
 */
const struct MenuOpSeq PagerDefaultBindings[] = { /* map: pager */
  { OP_ATTACHMENT_EDIT_TYPE,               "\005" },           // <Ctrl-E>
  { OP_BOUNCE_MESSAGE,                     "b" },
  { OP_CHECK_TRADITIONAL,                  "\033P" },          // <Alt-P>
  { OP_COPY_MESSAGE,                       "C" },
  { OP_CREATE_ALIAS,                       "a" },
  { OP_DECODE_COPY,                        "\033C" },          // <Alt-C>
  { OP_DECODE_SAVE,                        "\033s" },          // <Alt-s>
  { OP_DELETE,                             "d" },
  { OP_DELETE_SUBTHREAD,                   "\033d" },          // <Alt-d>
  { OP_DELETE_THREAD,                      "\004" },           // <Ctrl-D>
  { OP_DISPLAY_ADDRESS,                    "@" },
  { OP_DISPLAY_HEADERS,                    "h" },
  { OP_EDIT_LABEL,                         "Y" },
  { OP_EDIT_OR_VIEW_RAW_MESSAGE,           "e" },
  { OP_ENTER_COMMAND,                      ":" },
  { OP_EXIT,                               "i" },
  { OP_EXIT,                               "q" },
  { OP_EXIT,                               "x" },
  { OP_EXTRACT_KEYS,                       "\013" },           // <Ctrl-K>
  { OP_FLAG_MESSAGE,                       "F" },
  { OP_FORGET_PASSPHRASE,                  "\006" },           // <Ctrl-F>
  { OP_FORWARD_MESSAGE,                    "f" },
  { OP_GROUP_REPLY,                        "g" },
  { OP_HELP,                               "?" },
  { OP_JUMP_1,                             "1" },
  { OP_JUMP_2,                             "2" },
  { OP_JUMP_3,                             "3" },
  { OP_JUMP_4,                             "4" },
  { OP_JUMP_5,                             "5" },
  { OP_JUMP_6,                             "6" },
  { OP_JUMP_7,                             "7" },
  { OP_JUMP_8,                             "8" },
  { OP_JUMP_9,                             "9" },
  { OP_LIST_REPLY,                         "L" },
  { OP_MAIL,                               "m" },
  { OP_MAILBOX_LIST,                       "." },
  { OP_MAIL_KEY,                           "\033k" },          // <Alt-k>
  { OP_MAIN_BREAK_THREAD,                  "#" },
  { OP_MAIN_CHANGE_FOLDER,                 "c" },
  { OP_MAIN_CHANGE_FOLDER_READONLY,        "\033c" },          // <Alt-c>
  { OP_MAIN_CLEAR_FLAG,                    "W" },
  { OP_MAIN_LINK_THREADS,                  "&" },
  { OP_MAIN_NEXT_NEW_THEN_UNREAD,          "\t" },             // <Tab>
  { OP_MAIN_NEXT_SUBTHREAD,                "\033n" },          // <Alt-n>
  { OP_MAIN_NEXT_THREAD,                   "\016" },           // <Ctrl-N>
  { OP_MAIN_NEXT_UNDELETED,                "<down>" },
  { OP_MAIN_NEXT_UNDELETED,                "<right>" },
  { OP_MAIN_NEXT_UNDELETED,                "j" },
  { OP_MAIN_PARENT_MESSAGE,                "P" },
  { OP_MAIN_PREV_SUBTHREAD,                "\033p" },          // <Alt-p>
  { OP_MAIN_PREV_THREAD,                   "\020" },           // <Ctrl-P>
  { OP_MAIN_PREV_UNDELETED,                "<left>" },
  { OP_MAIN_PREV_UNDELETED,                "<up>" },
  { OP_MAIN_PREV_UNDELETED,                "k" },
  { OP_MAIN_READ_SUBTHREAD,                "\033r" },          // <Alt-r>
  { OP_MAIN_READ_THREAD,                   "\022" },           // <Ctrl-R>
  { OP_MAIN_SET_FLAG,                      "w" },
  { OP_MAIN_SYNC_FOLDER,                   "$" },
  { OP_NEXT_ENTRY,                         "J" },
  { OP_NEXT_LINE,                          "<keypadenter>" },
  { OP_NEXT_LINE,                          "\n" },             // <Enter>
  { OP_NEXT_LINE,                          "\r" },             // <Return>
  { OP_NEXT_PAGE,                          " " },              // <Space>
  { OP_NEXT_PAGE,                          "<pagedown>" },
  { OP_PAGER_BOTTOM,                       "<end>" },
  { OP_PAGER_HIDE_QUOTED,                  "T" },
  { OP_PAGER_SKIP_HEADERS,                 "H" },
  { OP_PAGER_SKIP_QUOTED,                  "S" },
  { OP_PAGER_TOP,                          "<home>" },
  { OP_PAGER_TOP,                          "^" },
  { OP_PIPE,                               "|" },
  { OP_PREV_ENTRY,                         "K" },
  { OP_PREV_LINE,                          "<backspace>" },
  { OP_PREV_PAGE,                          "-" },
  { OP_PREV_PAGE,                          "<pageup>" },
  { OP_PRINT,                              "p" },
  { OP_QUIT,                               "Q" },
  { OP_RECALL_MESSAGE,                     "R" },
  { OP_REDRAW,                             "\014" },           // <Ctrl-L>
  { OP_REPLY,                              "r" },
  { OP_RESEND,                             "\033e" },          // <Alt-e>
  { OP_SAVE,                               "s" },
  { OP_SEARCH,                             "/" },
  { OP_SEARCH_NEXT,                        "n" },
  { OP_SEARCH_REVERSE,                     "\033/" },          // <Alt-/>
  { OP_SEARCH_TOGGLE,                      "\\" },             // <Backslash>
  { OP_SHELL_ESCAPE,                       "!" },
  { OP_SORT,                               "o" },
  { OP_SORT_REVERSE,                       "O" },
  { OP_TAG,                                "t" },
  { OP_TOGGLE_NEW,                         "N" },
  { OP_TOGGLE_WRITE,                       "%" },
  { OP_UNDELETE,                           "u" },
  { OP_UNDELETE_SUBTHREAD,                 "\033u" },          // <Alt-u>
  { OP_UNDELETE_THREAD,                    "\025" },           // <Ctrl-U>
  { OP_VERSION,                            "V" },
  { OP_VIEW_ATTACHMENTS,                   "v" },
  { 0, NULL },
};
// clang-format on

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
  {
    mutt_message(_("Top of message is shown"));
  }
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
      while ((priv->top_line < priv->lines_used) &&
             COLOR_QUOTED(priv->lines[priv->top_line].cid))
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
  struct Buffer *buf = buf_pool_get();

  buf_strcpy(buf, priv->search_str);
  if (mw_get_field(((op == OP_SEARCH) || (op == OP_SEARCH_NEXT)) ? _("Search for: ") : _("Reverse search for: "),
                   buf, MUTT_COMP_CLEAR, HC_PATTERN, &CompletePatternOps, NULL) != 0)
  {
    goto done;
  }

  if (mutt_str_equal(buf_string(buf), priv->search_str))
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

  if (buf_is_empty(buf))
    goto done;

  mutt_str_copy(priv->search_str, buf_string(buf), sizeof(priv->search_str));

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
    mutt_error("%s", buf_string(buf));
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
  buf_pool_release(&buf);
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
        if ((!priv->hide_quoted || !COLOR_QUOTED(priv->lines[i].cid)) &&
            !priv->lines[i].cont_line && (priv->lines[i].search_arr_size > 0))
        {
          break;
        }
      }

      const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
      if (i < priv->lines_used)
      {
        priv->top_line = i;
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

      const bool c_wrap_search = cs_subset_bool(NeoMutt->sub, "wrap_search");
      if (i >= 0)
      {
        priv->top_line = i;
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

  const short c_pager_skip_quoted_context = cs_subset_number(NeoMutt->sub, "pager_skip_quoted_context");
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
    return FR_SUCCESS;
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
    {
      mutt_error(_("No more unquoted text after quoted text"));
      return FR_NO_ACTION;
    }
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
    {
      mutt_error(_("No more quoted text"));
      return FR_NO_ACTION;
    }

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
    {
      mutt_error(_("No more unquoted text after quoted text"));
      return FR_NO_ACTION;
    }
  }
  priv->top_line = new_topline - MIN(c_pager_skip_quoted_context, num_quoted);
  notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  return FR_SUCCESS;
}

/**
 * op_pager_top - Jump to the top of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_top(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  if (priv->top_line == 0)
  {
    mutt_message(_("Top of message is shown"));
  }
  else
  {
    priv->top_line = 0;
    notify_send(priv->notify, NT_PAGER, NT_PAGER_VIEW, priv);
  }

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
 * op_help - Help screen - Implements ::pager_function_t - @ingroup pager_function_api
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
 * op_save - Save the Pager text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_save(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
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

  struct FileCompletionData cdata = { false, NULL, NULL, NULL };
  if ((mw_get_field(_("Save to file: "), buf, MUTT_COMP_CLEAR, HC_FILE,
                    &CompleteFileOps, &cdata) != 0) ||
      buf_is_empty(buf))
  {
    rc = FR_SUCCESS;
    goto done;
  }

  buf_expand_path(buf);
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

  // Restore the read position
  if (pos >= 0)
    mutt_file_seek(priv->fp, pos, SEEK_CUR);

  mutt_message(_("Saved to: %s"), buf_string(buf));
  rc = FR_SUCCESS;

done:
  mutt_file_fclose(&fp_save);
  buf_pool_release(&buf);

  return rc;
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
  dlg_attachment(NeoMutt->sub, shared->mailbox_view, shared->email,
                 pview->pdata->fp, shared->attach_msg);
  if (shared->email->attach_del)
    shared->mailbox->changed = true;
  pager_queue_redraw(priv, PAGER_REDRAW_PAGER);
  return FR_SUCCESS;
}

// -----------------------------------------------------------------------------

/**
 * PagerFunctions - All the NeoMutt functions that the Pager supports
 */
static const struct PagerFunction PagerFunctions[] = {
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
  { OP_SAVE,                   op_save },
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
    mutt_error("%s", _(Not_available_in_this_menu));
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

  const char *result = dispatcher_get_retval_name(rc);
  mutt_debug(LL_DEBUG1, "Handled %s (%d) -> %s\n", opcodes_get_name(op), op, NONULL(result));

  return rc;
}
