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
#include "index/lib.h"

struct PagerPrivateData;

/**
 * op_bounce_message - remail a message to another user - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_bounce_message(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_check_stats - calculate message statistics for all mailboxes - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_check_stats(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_check_traditional - check for classic PGP - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_check_traditional(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_compose_to_sender - compose new message to the current message sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_compose_to_sender(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_copy_message - copy a message to a file/mailbox - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_copy_message(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_create_alias - create an alias from a message sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_create_alias(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_delete - delete the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_delete(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_delete_thread - delete all messages in thread - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_delete_thread(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_display_address - display full address of sender - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_display_address(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_edit_label - add, change, or delete a message's label - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_edit_label(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_enter_command - enter a neomuttrc command - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_enter_command(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_exit - exit this menu - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_exit(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_extract_keys - extract supported public keys - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_extract_keys(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_flag_message - toggle a message's 'important' flag - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_flag_message(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_forget_passphrase - wipe passphrases from memory - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_forget_passphrase(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_forward_message - forward a message with comments - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_forward_message(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_half_down - scroll down 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_half_down(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_half_up - scroll up 1/2 page - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_half_up(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_help - this screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_help(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_mail - compose a new mail message - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_mail(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_mailbox_list - list mailboxes with new mail - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_mailbox_list(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_mail_key - mail a PGP public key - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_mail_key(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_main_set_flag - set a status flag on a message - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_main_set_flag(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_next_line - scroll down one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_next_line(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_next_page - move to the next page - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_next_page(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_bottom - jump to the bottom of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_pager_bottom(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_hide_quoted - toggle display of quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_pager_hide_quoted(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_skip_headers - jump to first line after headers - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_pager_skip_headers(struct IndexSharedData *shared,
                          struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_skip_quoted - skip beyond quoted text - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_pager_skip_quoted(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pager_top - jump to the top of the message - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_pager_top(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_pipe - pipe message/attachment to a shell command - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_pipe(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_prev_line - scroll up one line - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_prev_line(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_prev_page - move to the previous page - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_prev_page(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_print - print the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_print(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_quit - save changes to mailbox and quit - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_quit(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_recall_message - recall a postponed message - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_recall_message(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_redraw - clear and redraw the screen - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_redraw(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_reply - reply to a message - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_reply(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_resend - use the current message as a template for a new one - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_resend(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_save - save message/attachment to a mailbox/file - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_save(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_search - search for a regular expression - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_search(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_search_next - search for next match - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_search_next(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_search_toggle - toggle search pattern coloring - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_search_toggle(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_shell_escape - invoke a command in a subshell - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_shell_escape(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_sort - sort messages - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_sort(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_tag - tag the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_tag(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_toggle_new - toggle a message's 'new' flag - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_toggle_new(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_undelete - undelete the current entry - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_undelete(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_undelete_thread - undelete all messages in thread - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_undelete_thread(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_version - show the NeoMutt version number and date - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_version(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_view_attachments - show MIME attachments - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_view_attachments(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_what_key - display the keycode for a key press - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_what_key(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

// -----------------------------------------------------------------------------

#ifdef USE_NNTP
/**
 * op_followup - followup to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_followup(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_forward_to_group - forward to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_forward_to_group(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_post - post message to newsgroup - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_post(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}
#endif

#ifdef USE_SIDEBAR
/**
 * op_sidebar_move - move the sidebar highlight - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_sidebar_move(struct IndexSharedData *shared, struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}

/**
 * op_sidebar_toggle_visible - make the sidebar (in)visible - Implements ::pager_function_t - @ingroup pager_function_api
 */
int op_sidebar_toggle_visible(struct IndexSharedData *shared,
                              struct PagerPrivateData *priv, int op)
{
  return IR_SUCCESS;
}
#endif
