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
#include <inttypes.h> // IWYU pragma: keep
#include <string.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "functions.h"
#include "index/lib.h"
#include "opcodes.h"

struct PagerPrivateData;

static const char *Not_available_in_this_menu =
    N_("Not available in this menu");

/**
 * op_bounce_message - remail a message to another user - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_bounce_message(struct IndexSharedData *shared,
                             struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_check_stats - calculate message statistics for all mailboxes - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_check_stats(struct IndexSharedData *shared,
                          struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_check_traditional - check for classic PGP - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_check_traditional(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_compose_to_sender - compose new message to the current message sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_compose_to_sender(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_copy_message - copy a message to a file/mailbox - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_copy_message(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_create_alias - create an alias from a message sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_create_alias(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_delete - delete the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_delete(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_delete_thread - delete all messages in thread - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_delete_thread(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_display_address - display full address of sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_display_address(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_edit_label - add, change, or delete a message's label - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_edit_label(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_enter_command - enter a neomuttrc command - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_enter_command(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_exit - exit this menu - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_exit(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_extract_keys - extract supported public keys - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_extract_keys(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_flag_message - toggle a message's 'important' flag - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_flag_message(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_forget_passphrase - wipe passphrases from memory - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_forget_passphrase(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_forward_message - forward a message with comments - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_forward_message(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_half_down - scroll down 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_half_down(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_half_up - scroll up 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_half_up(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_help - this screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_help(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_mail - compose a new mail message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_mail(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_mailbox_list - list mailboxes with new mail - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_mailbox_list(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_mail_key - mail a PGP public key - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_mail_key(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_main_set_flag - set a status flag on a message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_main_set_flag(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_next_line - scroll down one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_next_line(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_next_page - move to the next page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_next_page(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_bottom - jump to the bottom of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_bottom(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_hide_quoted - toggle display of quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_hide_quoted(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_skip_headers - jump to first line after headers - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_skip_headers(struct IndexSharedData *shared,
                                 struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_skip_quoted - skip beyond quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_skip_quoted(struct IndexSharedData *shared,
                                struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_top - jump to the top of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pager_top(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pipe - pipe message/attachment to a shell command - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_pipe(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_prev_line - scroll up one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_prev_line(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_prev_page - move to the previous page - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_prev_page(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_print - print the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_print(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_quit - save changes to mailbox and quit - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_quit(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_recall_message - recall a postponed message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_recall_message(struct IndexSharedData *shared,
                             struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_redraw - clear and redraw the screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_redraw(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_reply - reply to a message - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_reply(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_resend - use the current message as a template for a new one - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_resend(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_save - save message/attachment to a mailbox/file - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_save(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_search - search for a regular expression - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_search_next - search for next match - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search_next(struct IndexSharedData *shared,
                          struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_search_toggle - toggle search pattern coloring - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_search_toggle(struct IndexSharedData *shared,
                            struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_shell_escape - invoke a command in a subshell - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_shell_escape(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_sort - sort messages - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_sort(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_tag - tag the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_tag(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_toggle_new - toggle a message's 'new' flag - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_toggle_new(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_undelete - undelete the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_undelete(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_undelete_thread - undelete all messages in thread - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_undelete_thread(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_version - show the NeoMutt version number and date - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_version(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_view_attachments - show MIME attachments - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_view_attachments(struct IndexSharedData *shared,
                               struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_what_key - display the keycode for a key press - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_what_key(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

// -----------------------------------------------------------------------------

#ifdef USE_NNTP
/**
 * op_followup - followup to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_followup(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_forward_to_group - forward to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_forward_to_group(struct IndexSharedData *shared,
                               struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_post - post message to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_post(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}
#endif

#ifdef USE_SIDEBAR
/**
 * op_sidebar_move - move the sidebar highlight - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_sidebar_move(struct IndexSharedData *shared,
                           struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_sidebar_toggle_visible - make the sidebar (in)visible - Implements ::pager_function_t - @ingroup pager_function_api
 */
static int op_sidebar_toggle_visible(struct IndexSharedData *shared,
                                     struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}
#endif

// -----------------------------------------------------------------------------

/**
 * PagerFunctions - All the NeoMutt functions that the Pager supports
 */
struct PagerFunction PagerFunctions[] = {
  // clang-format off
  { OP_BOUNCE_MESSAGE,         op_bounce_message },
  { OP_CHECK_STATS,            op_check_stats },
  { OP_CHECK_TRADITIONAL,      op_check_traditional },
  { OP_COMPOSE_TO_SENDER,      op_compose_to_sender },
  { OP_COPY_MESSAGE,           op_copy_message },
  { OP_DECODE_COPY,            op_copy_message },
  { OP_DECODE_SAVE,            op_copy_message },
  { OP_DECRYPT_COPY,           op_copy_message },
  { OP_CREATE_ALIAS,           op_create_alias },
  { OP_DECRYPT_SAVE,           op_save },
  { OP_DELETE,                 op_delete },
  { OP_PURGE_MESSAGE,          op_delete },
  { OP_DELETE_SUBTHREAD,       op_delete_thread },
  { OP_DELETE_THREAD,          op_delete_thread },
  { OP_PURGE_THREAD,           op_delete_thread },
  { OP_DISPLAY_ADDRESS,        op_display_address },
  { OP_EDIT_LABEL,             op_edit_label },
  { OP_ENTER_COMMAND,          op_enter_command },
  { OP_EXIT,                   op_exit },
  { OP_EXTRACT_KEYS,           op_extract_keys },
  { OP_FLAG_MESSAGE,           op_flag_message },
#ifdef USE_NNTP
  { OP_FOLLOWUP,               op_followup },
#endif
  { OP_FORGET_PASSPHRASE,      op_forget_passphrase },
  { OP_FORWARD_MESSAGE,        op_forward_message },
#ifdef USE_NNTP
  { OP_FORWARD_TO_GROUP,       op_forward_to_group },
#endif
  { OP_HALF_DOWN,              op_half_down },
  { OP_HALF_UP,                op_half_up },
  { OP_HELP,                   op_help },
  { OP_MAIL,                   op_mail },
  { OP_MAILBOX_LIST,           op_mailbox_list },
  { OP_MAIL_KEY,               op_mail_key },
  { OP_MAIN_CLEAR_FLAG,        op_main_set_flag },
  { OP_MAIN_SET_FLAG,          op_main_set_flag },
  { OP_NEXT_LINE,              op_next_line },
  { OP_NEXT_PAGE,              op_next_page },
  { OP_PAGER_BOTTOM,           op_pager_bottom },
  { OP_PAGER_HIDE_QUOTED,      op_pager_hide_quoted },
  { OP_PAGER_SKIP_HEADERS,     op_pager_skip_headers },
  { OP_PAGER_SKIP_QUOTED,      op_pager_skip_quoted },
  { OP_PAGER_TOP,              op_pager_top },
  { OP_PIPE,                   op_pipe },
#ifdef USE_NNTP
  { OP_POST,                   op_post },
#endif
  { OP_PREV_LINE,              op_prev_line },
  { OP_PREV_PAGE,              op_prev_page },
  { OP_PRINT,                  op_print },
  { OP_QUIT,                   op_quit },
  { OP_RECALL_MESSAGE,         op_recall_message },
  { OP_REDRAW,                 op_redraw },
  { OP_GROUP_CHAT_REPLY,       op_reply },
  { OP_GROUP_REPLY,            op_reply },
  { OP_LIST_REPLY,             op_reply },
  { OP_REPLY,                  op_reply },
  { OP_RESEND,                 op_resend },
  { OP_SAVE,                   op_save },
  { OP_SEARCH,                 op_search },
  { OP_SEARCH_REVERSE,         op_search },
  { OP_SEARCH_NEXT,            op_search_next },
  { OP_SEARCH_OPPOSITE,        op_search_next },
  { OP_SEARCH_TOGGLE,          op_search_toggle },
  { OP_SHELL_ESCAPE,           op_shell_escape },
  { OP_SIDEBAR_FIRST,          op_sidebar_move },
  { OP_SIDEBAR_LAST,           op_sidebar_move },
  { OP_SIDEBAR_NEXT,           op_sidebar_move },
  { OP_SIDEBAR_NEXT_NEW,       op_sidebar_move },
  { OP_SIDEBAR_PAGE_DOWN,      op_sidebar_move },
  { OP_SIDEBAR_PAGE_UP,        op_sidebar_move },
  { OP_SIDEBAR_PREV,           op_sidebar_move },
  { OP_SIDEBAR_PREV_NEW,       op_sidebar_move },
  { OP_SIDEBAR_TOGGLE_VISIBLE, op_sidebar_toggle_visible },
  { OP_SORT,                   op_sort },
  { OP_SORT_REVERSE,           op_sort },
  { OP_TAG,                    op_tag },
  { OP_TOGGLE_NEW,             op_toggle_new },
  { OP_UNDELETE,               op_undelete },
  { OP_UNDELETE_SUBTHREAD,     op_undelete_thread },
  { OP_UNDELETE_THREAD,        op_undelete_thread },
  { OP_VERSION,                op_version },
  { OP_VIEW_ATTACHMENTS,       op_view_attachments },
  { OP_WHAT_KEY,               op_what_key },
  { 0, NULL },
  // clang-format on
};

/**
 * pager_function_dispatcher - Perform a Pager function
 * @param win_pager Window for the Index
 * @param op        Operation to perform, e.g. OP_MAIN_LIMIT
 * @retval num #IndexRetval, e.g. #IR_SUCCESS
 */
int pager_function_dispatcher(struct MuttWindow *win_pager, int op)
{
  if (!win_pager)
  {
    mutt_error(_(Not_available_in_this_menu));
    return IR_ERROR;
  }

  struct PagerPrivateData *priv = win_pager->parent->wdata;
  if (!priv)
    return IR_ERROR;

  struct MuttWindow *dlg = dialog_find(win_pager);
  if (!dlg || !dlg->wdata)
    return IR_ERROR;

  int rc = IR_UNKNOWN;
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

  return rc;
}
