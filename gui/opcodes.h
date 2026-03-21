/**
 * @file
 * All user-callable functions
 *
 * @authors
 * Copyright (C) 2017 Damien Riegel <damien.riegel@gmail.com>
 * Copyright (C) 2018-2024 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_OPCODES_H
#define MUTT_OPCODES_H

#include "config.h"
#include "mutt/lib.h"

const char *opcodes_get_description(int op);
const char *opcodes_get_name       (int op);

#define OP_REPAINT     -3 ///< Repaint is needed
#define OP_TIMEOUT     -2 ///< 1 second with no events
#define OP_ABORT       -1 ///< $abort_key pressed (Ctrl-G)

// clang-format off
#define OPS_ATTACH(_fmt) \
  /* L10N: Help screen description for OP_ATTACH_ATTACH_FILE */ \
  /*       Compose Dialog: <op_attach_attach_file> */ \
  _fmt(OP_ATTACH_ATTACH_FILE,                 N_("Attach files to this message")) \
  /* L10N: Help screen description for OP_ATTACH_ATTACH_MESSAGE */ \
  /*       Compose Dialog: <op_attach_attach_message> */ \
  _fmt(OP_ATTACH_ATTACH_MESSAGE,              N_("Attach messages to this message")) \
  /* L10N: Help screen description for OP_ATTACH_ATTACH_NEWS_MESSAGE */ \
  /*       Compose Dialog: <op_attach_attach_message> */ \
  _fmt(OP_ATTACH_ATTACH_NEWS_MESSAGE,         N_("Attach news articles to this message")) \
  /* L10N: Help screen description for OP_ATTACH_COLLAPSE */ \
  /*       Attach Dialog: <op_attach_collapse> */ \
  _fmt(OP_ATTACH_COLLAPSE,                    N_("Toggle display of subparts")) \
  /* L10N: Help screen description for OP_ATTACH_DELETE */ \
  /*       Attach Dialog: <op_attach_delete> */ \
  _fmt(OP_ATTACH_DELETE,                      N_("Delete the current entry")) \
  /* L10N: Help screen description for OP_ATTACH_DETACH */ \
  /*       Compose Dialog: <op_attach_detach> */ \
  _fmt(OP_ATTACH_DETACH,                      N_("Delete the current entry")) \
  /* L10N: Help screen description for OP_ATTACH_EDIT_CONTENT_ID */ \
  /*       Compose Dialog: <op_attach_edit_content_id> */ \
  _fmt(OP_ATTACH_EDIT_CONTENT_ID,             N_("Edit the 'Content-ID' of the attachment")) \
  /* L10N: Help screen description for OP_ATTACH_EDIT_DESCRIPTION */ \
  /*       Compose Dialog: <op_attach_edit_description> */ \
  _fmt(OP_ATTACH_EDIT_DESCRIPTION,            N_("Edit attachment description")) \
  /* L10N: Help screen description for OP_ATTACH_EDIT_ENCODING */ \
  /*       Compose Dialog: <op_attach_edit_encoding> */ \
  _fmt(OP_ATTACH_EDIT_ENCODING,               N_("Edit attachment transfer-encoding")) \
  /* L10N: Help screen description for OP_ATTACH_EDIT_LANGUAGE */ \
  /*       Compose Dialog: <op_attach_edit_language> */ \
  _fmt(OP_ATTACH_EDIT_LANGUAGE,               N_("Edit the 'Content-Language' of the attachment")) \
  /* L10N: Help screen description for OP_ATTACH_EDIT_MIME */ \
  /*       Compose Dialog: <op_attach_edit_mime> */ \
  _fmt(OP_ATTACH_EDIT_MIME,                   N_("Edit attachment using mailcap entry")) \
  /* L10N: Help screen description for OP_ATTACH_EDIT_TYPE */ \
  /*       Attach Dialog: <op_attach_edit_type> */ \
  /*       Compose Dialog: <op_attach_edit_type> */ \
  /*       Index: <op_attach_edit_type> */ \
  _fmt(OP_ATTACH_EDIT_TYPE,                   N_("Edit attachment content type")) \
  /* L10N: Help screen description for OP_ATTACH_FILTER */ \
  /*       Compose Dialog: <op_attach_filter> */ \
  _fmt(OP_ATTACH_FILTER,                      N_("Filter attachment through a shell command")) \
  /* L10N: Help screen description for OP_ATTACH_GET_ATTACHMENT */ \
  /*       Compose Dialog: <op_attach_get_attachment> */ \
  _fmt(OP_ATTACH_GET_ATTACHMENT,              N_("Get a temporary copy of an attachment")) \
  /* L10N: Help screen description for OP_ATTACH_GROUP_ALTS */ \
  /*       Compose Dialog: <op_attach_group_alts> */ \
  _fmt(OP_ATTACH_GROUP_ALTS,                  N_("Group tagged attachments as 'multipart/alternative'")) \
  /* L10N: Help screen description for OP_ATTACH_GROUP_LINGUAL */ \
  /*       Compose Dialog: <op_attach_group_lingual> */ \
  _fmt(OP_ATTACH_GROUP_LINGUAL,               N_("Group tagged attachments as 'multipart/multilingual'")) \
  /* L10N: Help screen description for OP_ATTACH_GROUP_RELATED */ \
  /*       Compose Dialog: <op_attach_group_related> */ \
  _fmt(OP_ATTACH_GROUP_RELATED,               N_("Group tagged attachments as 'multipart/related'")) \
  /* L10N: Help screen description for OP_ATTACH_MOVE_DOWN */ \
  /*       Compose Dialog: <op_attach_move_down> */ \
  _fmt(OP_ATTACH_MOVE_DOWN,                   N_("Move an attachment down in the attachment list")) \
  /* L10N: Help screen description for OP_ATTACH_MOVE_UP */ \
  /*       Compose Dialog: <op_attach_move_up> */ \
  _fmt(OP_ATTACH_MOVE_UP,                     N_("Move an attachment up in the attachment list")) \
  /* L10N: Help screen description for OP_ATTACH_NEW_MIME */ \
  /*       Compose Dialog: <op_attach_new_mime> */ \
  _fmt(OP_ATTACH_NEW_MIME,                    N_("Compose new attachment using mailcap entry")) \
  /* L10N: Help screen description for OP_ATTACH_PIPE */ \
  _fmt(OP_ATTACH_PIPE,                        N_("Pipe message/attachment to a shell command")) \
  /* L10N: Help screen description for OP_ATTACH_PRINT */ \
  /*       Attach Dialog: <op_attach_print> */ \
  /*       Compose Dialog: <op_attach_print> */ \
  _fmt(OP_ATTACH_PRINT,                       N_("Print the current entry")) \
  /* L10N: Help screen description for OP_ATTACH_RENAME_ATTACHMENT */ \
  /*       Compose Dialog: <op_attach_rename_attachment> */ \
  _fmt(OP_ATTACH_RENAME_ATTACHMENT,           N_("Send attachment with a different name")) \
  /* L10N: Help screen description for OP_ATTACH_SAVE */ \
  /*       Attach Dialog: <op_attach_save> */ \
  /*       Compose Dialog: <op_attach_save> */ \
  _fmt(OP_ATTACH_SAVE,                        N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help screen description for OP_ATTACH_TOGGLE_DISPOSITION */ \
  /*       Compose Dialog: <op_attach_toggle_disposition> */ \
  _fmt(OP_ATTACH_TOGGLE_DISPOSITION,          N_("Toggle disposition between inline/attachment")) \
  /* L10N: Help screen description for OP_ATTACH_TOGGLE_RECODE */ \
  /*       Compose Dialog: <op_attach_toggle_recode> */ \
  _fmt(OP_ATTACH_TOGGLE_RECODE,               N_("Toggle recoding of this attachment")) \
  /* L10N: Help screen description for OP_ATTACH_TOGGLE_UNLINK */ \
  /*       Compose Dialog: <op_attach_toggle_unlink> */ \
  _fmt(OP_ATTACH_TOGGLE_UNLINK,               N_("Toggle whether to delete file after sending it")) \
  /* L10N: Help screen description for OP_ATTACH_UNDELETE */ \
  /*       Attach Dialog: <op_attach_undelete> */ \
  _fmt(OP_ATTACH_UNDELETE,                    N_("Undelete the current entry")) \
  /* L10N: Help screen description for OP_ATTACH_UNGROUP */ \
  /*       Compose Dialog: <op_attach_ungroup> */ \
  _fmt(OP_ATTACH_UNGROUP,                     N_("Ungroup 'multipart' attachment")) \
  /* L10N: Help screen description for OP_ATTACH_UPDATE_ENCODING */ \
  /*       Compose Dialog: <op_attach_update_encoding> */ \
  _fmt(OP_ATTACH_UPDATE_ENCODING,             N_("Update an attachment's encoding info")) \
  /* L10N: Help screen description for OP_ATTACH_VIEW */ \
  /*       Attach Dialog: <op_attach_view> */ \
  /*       Compose Dialog: <op_display_headers> */ \
  _fmt(OP_ATTACH_VIEW,                        N_("View attachment using mailcap entry if necessary")) \
  /* L10N: Help screen description for OP_ATTACH_VIEW_MAILCAP */ \
  /*       Attach Dialog: <op_attach_view_mailcap> */ \
  /*       Compose Dialog: <op_display_headers> */ \
  _fmt(OP_ATTACH_VIEW_MAILCAP,                N_("Force viewing of attachment using mailcap")) \
  /* L10N: Help screen description for OP_ATTACH_VIEW_PAGER */ \
  /*       Attach Dialog: <op_attach_view_pager> */ \
  /*       Compose Dialog: <op_display_headers> */ \
  _fmt(OP_ATTACH_VIEW_PAGER,                  N_("View attachment in pager using copiousoutput mailcap")) \
  /* L10N: Help screen description for OP_ATTACH_VIEW_TEXT */ \
  /*       Attach Dialog: <op_attach_view_text> */ \
  /*       Compose Dialog: <op_display_headers> */ \
  _fmt(OP_ATTACH_VIEW_TEXT,                   N_("View attachment as text")) \
  /* L10N: Help screen description for OP_PREVIEW_PAGE_DOWN */ \
  /*       Compose Dialog: <op_preview_page_down> */ \
  _fmt(OP_PREVIEW_PAGE_DOWN,                  N_("Show the next page of the message")) \
  /* L10N: Help screen description for OP_PREVIEW_PAGE_UP */ \
  /*       Compose Dialog: <op_preview_page_up> */ \
  _fmt(OP_PREVIEW_PAGE_UP,                    N_("Show the previous page of the message")) \

#ifdef USE_AUTOCRYPT
#define OPS_AUTOCRYPT(_fmt) \
  /* L10N: Help screen description for OP_AUTOCRYPT_ACCT_MENU */ \
  /*       Index: <op_autocrypt_acct_menu> */ \
  _fmt(OP_AUTOCRYPT_ACCT_MENU,                N_("Manage autocrypt accounts")) \
  /* L10N: Help screen description for OP_AUTOCRYPT_CREATE_ACCT */ \
  /*       Autocrypt Dialog: <op_autocrypt_create_acct> */ \
  _fmt(OP_AUTOCRYPT_CREATE_ACCT,              N_("Create a new autocrypt account")) \
  /* L10N: Help screen description for OP_AUTOCRYPT_DELETE_ACCT */ \
  /*       Autocrypt Dialog: <op_autocrypt_delete_acct> */ \
  _fmt(OP_AUTOCRYPT_DELETE_ACCT,              N_("Delete the current account")) \
  /* L10N: Help screen description for OP_AUTOCRYPT_TOGGLE_ACTIVE */ \
  /*       Autocrypt Dialog: <op_autocrypt_toggle_active> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_ACTIVE,            N_("Toggle the current account active/inactive")) \
  /* L10N: Help screen description for OP_AUTOCRYPT_TOGGLE_PREFER */ \
  /*       Autocrypt Dialog: <op_autocrypt_toggle_prefer> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_PREFER,            N_("Toggle the current account prefer-encrypt flag")) \
  /* L10N: Help screen description for OP_COMPOSE_AUTOCRYPT_MENU */ \
  /*       Envelope Window: <op_compose_autocrypt_menu> */ \
  _fmt(OP_COMPOSE_AUTOCRYPT_MENU,             N_("Show autocrypt compose menu options"))
#else
#define OPS_AUTOCRYPT(_)
#endif

#define OPS_CORE(_fmt) \
  /* L10N: Help screen description for OP_ALIAS_DIALOG */ \
  /*       Index: <op_alias_dialog> */ \
  _fmt(OP_ALIAS_DIALOG,                       N_("Open the aliases dialog")) \
  /* L10N: Help screen description for OP_BOTTOM_PAGE */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_BOTTOM_PAGE,                        N_("Move to the bottom of the page")) \
  /* L10N: Help screen description for OP_BOUNCE_MESSAGE */ \
  /*       Attach Dialog: <op_bounce_message> */ \
  /*       Index: <op_bounce_message> */ \
  _fmt(OP_BOUNCE_MESSAGE,                     N_("Remail a message to another user")) \
  /* L10N: Help screen description for OP_BROWSER_GOTO_FOLDER */ \
  /*       Browser: <op_toggle_mailboxes> */ \
  _fmt(OP_BROWSER_GOTO_FOLDER,                N_("Swap the current folder position with $folder if it exists")) \
  /* L10N: Help screen description for OP_BROWSER_NEW_FILE */ \
  /*       Browser: <op_browser_new_file> */ \
  _fmt(OP_BROWSER_NEW_FILE,                   N_("Select a new file in this directory")) \
  /* L10N: Help screen description for OP_BROWSER_SUBSCRIBE */ \
  /*       Browser: <op_browser_subscribe> */ \
  _fmt(OP_BROWSER_SUBSCRIBE,                  N_("Subscribe to current mbox (IMAP/NNTP only)")) \
  /* L10N: Help screen description for OP_BROWSER_TELL */ \
  /*       Browser: <op_browser_tell> */ \
  _fmt(OP_BROWSER_TELL,                       N_("Display the currently selected file's name")) \
  /* L10N: Help screen description for OP_BROWSER_TOGGLE_LSUB */ \
  /*       Browser: <op_browser_toggle_lsub> */ \
  _fmt(OP_BROWSER_TOGGLE_LSUB,                N_("Toggle view all/subscribed mailboxes (IMAP only)")) \
  /* L10N: Help screen description for OP_BROWSER_UNSUBSCRIBE */ \
  /*       Browser: <op_browser_subscribe> */ \
  _fmt(OP_BROWSER_UNSUBSCRIBE,                N_("Unsubscribe from current mbox (IMAP/NNTP only)")) \
  /* L10N: Help screen description for OP_BROWSER_VIEW_FILE */ \
  /*       Browser: <op_browser_view_file> */ \
  _fmt(OP_BROWSER_VIEW_FILE,                  N_("View file")) \
  /* L10N: Help screen description for OP_CATCHUP */ \
  /*       Browser: <op_catchup> */ \
  /*       Index: <op_catchup> */ \
  _fmt(OP_CATCHUP,                            N_("Mark all articles in newsgroup as read")) \
  /* L10N: Help screen description for OP_CHANGE_DIRECTORY */ \
  /*       Browser: <op_change_directory> */ \
  _fmt(OP_CHANGE_DIRECTORY,                   N_("Change directories")) \
  /* L10N: Help screen description for OP_CHECK_NEW */ \
  /*       Browser: <op_toggle_mailboxes> */ \
  _fmt(OP_CHECK_NEW,                          N_("Check mailboxes for new mail")) \
  /* L10N: Help screen description for OP_CHECK_STATS */ \
  /*       Global: <op_check_stats> */ \
  _fmt(OP_CHECK_STATS,                        N_("Calculate message statistics for all mailboxes")) \
  /* L10N: Help screen description for OP_COMPOSE_EDIT_FILE */ \
  /*       Compose Dialog: <op_compose_edit_file> */ \
  _fmt(OP_COMPOSE_EDIT_FILE,                  N_("Edit the file to be attached")) \
  /* L10N: Help screen description for OP_COMPOSE_EDIT_MESSAGE */ \
  /*       Compose Dialog: <op_compose_edit_message> */ \
  _fmt(OP_COMPOSE_EDIT_MESSAGE,               N_("Edit the message")) \
  /* L10N: Help screen description for OP_COMPOSE_ISPELL */ \
  /*       Compose Dialog: <op_compose_ispell> */ \
  _fmt(OP_COMPOSE_ISPELL,                     N_("Run ispell on the message")) \
  /* L10N: Help screen description for OP_COMPOSE_POSTPONE_MESSAGE */ \
  /*       Compose Dialog: <op_compose_postpone_message> */ \
  _fmt(OP_COMPOSE_POSTPONE_MESSAGE,           N_("Save this message to send later")) \
  /* L10N: Help screen description for OP_COMPOSE_RENAME_FILE */ \
  /*       Compose Dialog: <op_compose_rename_file> */ \
  _fmt(OP_COMPOSE_RENAME_FILE,                N_("Rename/move an attached file")) \
  /* L10N: Help screen description for OP_COMPOSE_SEND_MESSAGE */ \
  /*       Compose Dialog: <op_compose_send_message> */ \
  _fmt(OP_COMPOSE_SEND_MESSAGE,               N_("Send the message")) \
  /* L10N: Help screen description for OP_COMPOSE_TO_SENDER */ \
  /*       Attach Dialog: <op_compose_to_sender> */ \
  /*       Index: <op_compose_to_sender> */ \
  _fmt(OP_COMPOSE_TO_SENDER,                  N_("Compose new message to the current message sender")) \
  /* L10N: Help screen description for OP_COMPOSE_WRITE_MESSAGE */ \
  /*       Compose Dialog: <op_compose_write_message> */ \
  _fmt(OP_COMPOSE_WRITE_MESSAGE,              N_("Write the message to a folder")) \
  /* L10N: Help screen description for OP_COPY_MESSAGE */ \
  /*       Index: <op_save> */ \
  _fmt(OP_COPY_MESSAGE,                       N_("Copy a message to a file/mailbox")) \
  /* L10N: Help screen description for OP_CREATE_ALIAS */ \
  /*       Alias Dialog: <op_create_alias> */ \
  /*       Index: <op_create_alias> */ \
  _fmt(OP_CREATE_ALIAS,                       N_("Create an alias from a message sender")) \
  /* L10N: Help screen description for OP_CREATE_MAILBOX */ \
  /*       Browser: <op_create_mailbox> */ \
  _fmt(OP_CREATE_MAILBOX,                     N_("Create a new mailbox (IMAP only)")) \
  /* L10N: Help screen description for OP_CURRENT_BOTTOM */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_CURRENT_BOTTOM,                     N_("Move entry to bottom of screen")) \
  /* L10N: Help screen description for OP_CURRENT_MIDDLE */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_CURRENT_MIDDLE,                     N_("Move entry to middle of screen")) \
  /* L10N: Help screen description for OP_CURRENT_TOP */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_CURRENT_TOP,                        N_("Move entry to top of screen")) \
  /* L10N: Help screen description for OP_DECODE_COPY */ \
  /*       Index: <op_save> */ \
  _fmt(OP_DECODE_COPY,                        N_("Make decoded (text/plain) copy")) \
  /* L10N: Help screen description for OP_DECODE_SAVE */ \
  /*       Index: <op_save> */ \
  _fmt(OP_DECODE_SAVE,                        N_("Make decoded copy (text/plain) and delete")) \
  /* L10N: Help screen description for OP_DELETE */ \
  /*       Alias Dialog: <op_delete> */ \
  /*       Index: <op_delete> */ \
  /*       Postponed Dialog: <op_delete> */ \
  _fmt(OP_DELETE,                             N_("Delete the current entry")) \
  /* L10N: Help screen description for OP_DELETE_MAILBOX */ \
  /*       Browser: <op_delete_mailbox> */ \
  _fmt(OP_DELETE_MAILBOX,                     N_("Delete the current mailbox (IMAP only)")) \
  /* L10N: Help screen description for OP_DELETE_SUBTHREAD */ \
  /*       Index: <op_delete_thread> */ \
  _fmt(OP_DELETE_SUBTHREAD,                   N_("Delete all messages in subthread")) \
  /* L10N: Help screen description for OP_DELETE_THREAD */ \
  /*       Index: <op_delete_thread> */ \
  _fmt(OP_DELETE_THREAD,                      N_("Delete all messages in thread")) \
  /* L10N: Help screen description for OP_DESCEND_DIRECTORY */ \
  /*       Browser: <op_generic_select_entry> */ \
  _fmt(OP_DESCEND_DIRECTORY,                  N_("Descend into a directory")) \
  /* L10N: Help screen description for OP_DISPLAY_ADDRESS */ \
  /*       Index: <op_display_address> */ \
  _fmt(OP_DISPLAY_ADDRESS,                    N_("Display full address of sender")) \
  /* L10N: Help screen description for OP_DISPLAY_HEADERS */ \
  /*       Attach Dialog: <op_attach_view> */ \
  /*       Compose Dialog: <op_display_headers> */ \
  /*       Index: <op_display_message> */ \
  _fmt(OP_DISPLAY_HEADERS,                    N_("Display message and toggle header weeding")) \
  /* L10N: Help screen description for OP_DISPLAY_MESSAGE */ \
  /*       Index: <op_display_message> */ \
  _fmt(OP_DISPLAY_MESSAGE,                    N_("Display a message")) \
  /* L10N: Help screen description for OP_EDITOR_BACKSPACE */ \
  /*       Text Entry: <op_editor_backspace> */ \
  _fmt(OP_EDITOR_BACKSPACE,                   N_("Delete the char in front of the cursor")) \
  /* L10N: Help screen description for OP_EDITOR_BACKWARD_CHAR */ \
  /*       Text Entry: <op_editor_backward_char> */ \
  _fmt(OP_EDITOR_BACKWARD_CHAR,               N_("Move the cursor one character to the left")) \
  /* L10N: Help screen description for OP_EDITOR_BACKWARD_WORD */ \
  /*       Text Entry: <op_editor_backward_word> */ \
  _fmt(OP_EDITOR_BACKWARD_WORD,               N_("Move the cursor to the beginning of the word")) \
  /* L10N: Help screen description for OP_EDITOR_BOL */ \
  /*       Text Entry: <op_editor_bol> */ \
  _fmt(OP_EDITOR_BOL,                         N_("Jump to the beginning of the line")) \
  /* L10N: Help screen description for OP_EDITOR_CAPITALIZE_WORD */ \
  /*       Text Entry: <op_editor_capitalize_word> */ \
  _fmt(OP_EDITOR_CAPITALIZE_WORD,             N_("Capitalize the word")) \
  /* L10N: Help screen description for OP_EDITOR_COMPLETE */ \
  /*       Text Entry: <op_editor_complete> */ \
  _fmt(OP_EDITOR_COMPLETE,                    N_("Complete filename or alias")) \
  /* L10N: Help screen description for OP_EDITOR_COMPLETE_QUERY */ \
  /*       Text Entry: <op_editor_complete> */ \
  _fmt(OP_EDITOR_COMPLETE_QUERY,              N_("Complete address with query")) \
  /* L10N: Help screen description for OP_EDITOR_DELETE_CHAR */ \
  /*       Text Entry: <op_editor_delete_char> */ \
  _fmt(OP_EDITOR_DELETE_CHAR,                 N_("Delete the char under the cursor")) \
  /* L10N: Help screen description for OP_EDITOR_DOWNCASE_WORD */ \
  /*       Text Entry: <op_editor_capitalize_word> */ \
  _fmt(OP_EDITOR_DOWNCASE_WORD,               N_("Convert the word to lower case")) \
  /* L10N: Help screen description for OP_EDITOR_EOL */ \
  /*       Text Entry: <op_editor_eol> */ \
  _fmt(OP_EDITOR_EOL,                         N_("Jump to the end of the line")) \
  /* L10N: Help screen description for OP_EDITOR_FORWARD_CHAR */ \
  /*       Text Entry: <op_editor_forward_char> */ \
  _fmt(OP_EDITOR_FORWARD_CHAR,                N_("Move the cursor one character to the right")) \
  /* L10N: Help screen description for OP_EDITOR_FORWARD_WORD */ \
  /*       Text Entry: <op_editor_forward_word> */ \
  _fmt(OP_EDITOR_FORWARD_WORD,                N_("Move the cursor to the end of the word")) \
  /* L10N: Help screen description for OP_EDITOR_HISTORY_DOWN */ \
  /*       Text Entry: <op_editor_history_down> */ \
  _fmt(OP_EDITOR_HISTORY_DOWN,                N_("Scroll down through the history list")) \
  /* L10N: Help screen description for OP_EDITOR_HISTORY_SEARCH */ \
  /*       Text Entry: <op_editor_history_search> */ \
  _fmt(OP_EDITOR_HISTORY_SEARCH,              N_("Search through the history list")) \
  /* L10N: Help screen description for OP_EDITOR_HISTORY_UP */ \
  /*       Text Entry: <op_editor_history_up> */ \
  _fmt(OP_EDITOR_HISTORY_UP,                  N_("Scroll up through the history list")) \
  /* L10N: Help screen description for OP_EDITOR_KILL_EOL */ \
  /*       Text Entry: <op_editor_kill_eol> */ \
  _fmt(OP_EDITOR_KILL_EOL,                    N_("Delete chars from cursor to end of line")) \
  /* L10N: Help screen description for OP_EDITOR_KILL_EOW */ \
  /*       Text Entry: <op_editor_kill_eow> */ \
  _fmt(OP_EDITOR_KILL_EOW,                    N_("Delete chars from the cursor to the end of the word")) \
  /* L10N: Help screen description for OP_EDITOR_KILL_LINE */ \
  /*       Text Entry: <op_editor_kill_line> */ \
  _fmt(OP_EDITOR_KILL_LINE,                   N_("Delete chars from cursor to beginning the line")) \
  /* L10N: Help screen description for OP_EDITOR_KILL_WHOLE_LINE */ \
  /*       Text Entry: <op_editor_kill_whole_line> */ \
  _fmt(OP_EDITOR_KILL_WHOLE_LINE,             N_("Delete all chars on the line")) \
  /* L10N: Help screen description for OP_EDITOR_KILL_WORD */ \
  /*       Text Entry: <op_editor_kill_word> */ \
  _fmt(OP_EDITOR_KILL_WORD,                   N_("Delete the word in front of the cursor")) \
  /* L10N: Help screen description for OP_EDITOR_MAILBOX_CYCLE */ \
  /*       Text Entry: <op_editor_complete> */ \
  _fmt(OP_EDITOR_MAILBOX_CYCLE,               N_("Cycle among incoming mailboxes")) \
  /* L10N: Help screen description for OP_EDITOR_QUOTE_CHAR */ \
  /*       Text Entry: <op_editor_quote_char> */ \
  _fmt(OP_EDITOR_QUOTE_CHAR,                  N_("Quote the next typed key")) \
  /* L10N: Help screen description for OP_EDITOR_TRANSPOSE_CHARS */ \
  /*       Text Entry: <op_editor_transpose_chars> */ \
  _fmt(OP_EDITOR_TRANSPOSE_CHARS,             N_("Transpose character under cursor with previous")) \
  /* L10N: Help screen description for OP_EDITOR_UPCASE_WORD */ \
  /*       Text Entry: <op_editor_capitalize_word> */ \
  _fmt(OP_EDITOR_UPCASE_WORD,                 N_("Convert the word to upper case")) \
  /* L10N: Help screen description for OP_EDIT_LABEL */ \
  /*       Index: <op_edit_label> */ \
  _fmt(OP_EDIT_LABEL,                         N_("Add, change, or delete a message's label")) \
  /* L10N: Help screen description for OP_EDIT_OR_VIEW_RAW_MESSAGE */ \
  /*       Index: <op_edit_raw_message> */ \
  _fmt(OP_EDIT_OR_VIEW_RAW_MESSAGE,           N_("Edit the raw message if the mailbox is not read-only, otherwise view it")) \
  /* L10N: Help screen description for OP_EDIT_RAW_MESSAGE */ \
  /*       Index: <op_edit_raw_message> */ \
  _fmt(OP_EDIT_RAW_MESSAGE,                   N_("Edit the raw message (edit and edit-raw-message are synonyms)")) \
  /* L10N: Help screen description for OP_END_COND */ \
  /*       Index: <op_end_cond> */ \
  _fmt(OP_END_COND,                           N_("End of conditional execution (noop)")) \
  /* L10N: Help screen description for OP_ENTER_COMMAND */ \
  /*       Global: <op_enter_command> */ \
  _fmt(OP_ENTER_COMMAND,                      N_("Enter a neomuttrc command")) \
  /* L10N: Help screen description for OP_ENTER_MASK */ \
  /*       Browser: <op_enter_mask> */ \
  _fmt(OP_ENTER_MASK,                         N_("Enter a file mask")) \
  /* L10N: Help screen description for OP_EXIT */ \
  /*       Alias Dialog: <op_exit> */ \
  /*       Attach Dialog: <op_exit> */ \
  /*       Autocrypt Dialog: <op_exit> */ \
  /*       Browser: <op_exit> */ \
  /*       Compose Dialog: <op_exit> */ \
  /*       GPGME Key Selection Dialog: <op_exit> */ \
  /*       Index: <op_exit> */ \
  /*       Pager: <op_exit> */ \
  /*       PGP Key Selection Dialog: <op_exit> */ \
  /*       Postponed Dialog: <op_exit> */ \
  /*       Smime Key Selection Dialog: <op_exit> */ \
  _fmt(OP_EXIT,                               N_("Exit this menu")) \
  /* L10N: Help screen description for OP_FIRST_ENTRY */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_FIRST_ENTRY,                        N_("Move to the first entry")) \
  /* L10N: Help screen description for OP_FLAG_MESSAGE */ \
  /*       Index: <op_flag_message> */ \
  _fmt(OP_FLAG_MESSAGE,                       N_("Toggle a message's 'important' flag")) \
  /* L10N: Help screen description for OP_FOLLOWUP */ \
  /*       Attach Dialog: <op_followup> */ \
  /*       Index: <op_post> */ \
  _fmt(OP_FOLLOWUP,                           N_("Followup to newsgroup")) \
  /* L10N: Help screen description for OP_FORWARD_MESSAGE */ \
  /*       Attach Dialog: <op_forward_message> */ \
  /*       Index: <op_forward_message> */ \
  _fmt(OP_FORWARD_MESSAGE,                    N_("Forward a message with comments")) \
  /* L10N: Help screen description for OP_FORWARD_TO_GROUP */ \
  /*       Attach Dialog: <op_forward_to_group> */ \
  /*       Index: <op_post> */ \
  _fmt(OP_FORWARD_TO_GROUP,                   N_("Forward to newsgroup")) \
  /* L10N: Help screen description for OP_GENERIC_SELECT_ENTRY */ \
  /*       Alias Dialog: <op_generic_select_entry> */ \
  /*       Browser: <op_generic_select_entry> */ \
  /*       GPGME Key Selection Dialog: <op_generic_select_entry> */ \
  /*       History Dialog: <op_generic_select_entry> */ \
  /*       Pattern Dialog: <op_generic_select_entry> */ \
  /*       PGP Key Selection Dialog: <op_generic_select_entry> */ \
  /*       Postponed Dialog: <op_generic_select_entry> */ \
  /*       Smime Key Selection Dialog: <op_generic_select_entry> */ \
  _fmt(OP_GENERIC_SELECT_ENTRY,               N_("Select the current entry")) \
  /* L10N: Help screen description for OP_GET_CHILDREN */ \
  /*       Index: <op_get_children> */ \
  _fmt(OP_GET_CHILDREN,                       N_("Get all children of the current message")) \
  /* L10N: Help screen description for OP_GET_MESSAGE */ \
  /*       Index: <op_get_message> */ \
  _fmt(OP_GET_MESSAGE,                        N_("Get message with Message-Id")) \
  /* L10N: Help screen description for OP_GET_PARENT */ \
  /*       Index: <op_get_message> */ \
  _fmt(OP_GET_PARENT,                         N_("Get parent of the current message")) \
  /* L10N: Help screen description for OP_GOTO_PARENT */ \
  /*       Browser: <op_change_directory> */ \
  _fmt(OP_GOTO_PARENT,                        N_("Go to parent directory")) \
  /* L10N: Help screen description for OP_GROUP_CHAT_REPLY */ \
  /*       Index: <op_group_reply> */ \
  /*       Attach Dialog: <op_reply> */ \
  _fmt(OP_GROUP_CHAT_REPLY,                   N_("Reply to all recipients preserving To/Cc")) \
  /* L10N: Help screen description for OP_GROUP_REPLY */ \
  /*       Index: <op_group_reply> */ \
  /*       Attach Dialog: <op_reply> */ \
  _fmt(OP_GROUP_REPLY,                        N_("Reply to all recipients")) \
  /* L10N: Help screen description for OP_HALF_DOWN */ \
  /*       Menu: <menu_movement> */ \
  /*       Pager: <op_pager_half_down> */ \
  _fmt(OP_HALF_DOWN,                          N_("Scroll down 1/2 page")) \
  /* L10N: Help screen description for OP_HALF_UP */ \
  /*       Menu: <menu_movement> */ \
  /*       Pager: <op_pager_half_up> */ \
  _fmt(OP_HALF_UP,                            N_("Scroll up 1/2 page")) \
  /* L10N: Help screen description for OP_HELP */ \
  /*       Text Entry: <op_help> */ \
  /*       Menu: <op_help> */ \
  /*       Pager: <op_help> */ \
  _fmt(OP_HELP,                               N_("This screen")) \
  /* L10N: Help screen description for OP_JUMP */ \
  /*       Index: <op_jump> */ \
  /*       Menu: <op_jump> */ \
  _fmt(OP_JUMP,                               N_("Jump to an index number")) \
  _fmt(OP_JUMP_1,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_2,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_3,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_4,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_5,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_6,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_7,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_8,                             N_("Jump to an index number")) \
  _fmt(OP_JUMP_9,                             N_("Jump to an index number")) \
  /* L10N: Help screen description for OP_LAST_ENTRY */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_LAST_ENTRY,                         N_("Move to the last entry")) \
  /* L10N: Help screen description for OP_LIMIT_CURRENT_THREAD */ \
  /*       Index: <op_main_limit> */ \
  _fmt(OP_LIMIT_CURRENT_THREAD,               N_("Limit view to current thread")) \
  /* L10N: Help screen description for OP_LIST_REPLY */ \
  /*       Index: <op_list_reply> */ \
  /*       Attach Dialog: <op_reply> */ \
  _fmt(OP_LIST_REPLY,                         N_("Reply to specified mailing list")) \
  /* L10N: Help screen description for OP_LIST_SUBSCRIBE */ \
  /*       Attach Dialog: <op_list_subscribe> */ \
  /*       Index: <op_list_subscribe> */ \
  _fmt(OP_LIST_SUBSCRIBE,                     N_("Subscribe to a mailing list")) \
  /* L10N: Help screen description for OP_LIST_UNSUBSCRIBE */ \
  /*       Attach Dialog: <op_list_unsubscribe> */ \
  /*       Index: <op_list_unsubscribe> */ \
  _fmt(OP_LIST_UNSUBSCRIBE,                   N_("Unsubscribe from a mailing list")) \
  /* L10N: Help screen description for OP_LOAD_ACTIVE */ \
  /*       Browser: <op_load_active> */ \
  _fmt(OP_LOAD_ACTIVE,                        N_("Load list of all newsgroups from NNTP server")) \
  /* L10N: Help screen description for OP_MACRO */ \
  _fmt(OP_MACRO,                              N_("Execute a macro")) \
  /* L10N: Help screen description for OP_MAIL */ \
  /*       Alias Dialog: <op_generic_select_entry> */ \
  /*       Index: <op_mail> */ \
  _fmt(OP_MAIL,                               N_("Compose a new mail message")) \
  /* L10N: Help screen description for OP_MAILBOX_LIST */ \
  /*       Browser: <op_mailbox_list> */ \
  /*       Index: <op_mailbox_list> */ \
  _fmt(OP_MAILBOX_LIST,                       N_("List mailboxes with new mail")) \
  /* L10N: Help screen description for OP_MAIN_BREAK_THREAD */ \
  /*       Index: <op_main_break_thread> */ \
  _fmt(OP_MAIN_BREAK_THREAD,                  N_("Break the thread in two")) \
  /* L10N: Help screen description for OP_MAIN_CHANGE_FOLDER */ \
  /*       Index: <op_main_change_folder> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER,                 N_("Open a different folder")) \
  /* L10N: Help screen description for OP_MAIN_CHANGE_FOLDER_READONLY */ \
  /*       Index: <op_main_change_folder> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER_READONLY,        N_("Open a different folder in read only mode")) \
  /* L10N: Help screen description for OP_MAIN_CHANGE_GROUP */ \
  /*       Index: <op_main_change_group> */ \
  _fmt(OP_MAIN_CHANGE_GROUP,                  N_("Open a different newsgroup")) \
  /* L10N: Help screen description for OP_MAIN_CHANGE_GROUP_READONLY */ \
  /*       Index: <op_main_change_group> */ \
  _fmt(OP_MAIN_CHANGE_GROUP_READONLY,         N_("Open a different newsgroup in read only mode")) \
  /* L10N: Help screen description for OP_MAIN_CLEAR_FLAG */ \
  /*       Index: <op_main_set_flag> */ \
  _fmt(OP_MAIN_CLEAR_FLAG,                    N_("Clear a status flag from a message")) \
  /* L10N: Help screen description for OP_MAIN_CLOSE_ALL_THREADS */ \
  /*       Index: <op_main_close_all_threads> */ \
  _fmt(OP_MAIN_CLOSE_ALL_THREADS,             N_("Collapse all threads")) \
  /* L10N: Help screen description for OP_MAIN_CLOSE_THREAD */ \
  /*       Index: <op_main_close_thread> */ \
  _fmt(OP_MAIN_CLOSE_THREAD,                  N_("Collapse current thread")) \
  /* L10N: Help screen description for OP_MAIN_COLLAPSE_ALL */ \
  /*       Index: <op_main_collapse_all> */ \
  _fmt(OP_MAIN_COLLAPSE_ALL,                  N_("Collapse/uncollapse all threads")) \
  /* L10N: Help screen description for OP_MAIN_COLLAPSE_THREAD */ \
  /*       Index: <op_main_collapse_thread> */ \
  _fmt(OP_MAIN_COLLAPSE_THREAD,               N_("Collapse/uncollapse current thread")) \
  /* L10N: Help screen description for OP_MAIN_DELETE_PATTERN */ \
  /*       Index: <op_main_delete_pattern> */ \
  _fmt(OP_MAIN_DELETE_PATTERN,                N_("Delete non-hidden messages matching a pattern")) \
  /* L10N: Help screen description for OP_MAIN_FETCH_MAIL */ \
  /*       Index: <op_main_fetch_mail> */ \
  _fmt(OP_MAIN_FETCH_MAIL,                    N_("Retrieve mail from POP server")) \
  /* L10N: Help screen description for OP_MAIN_IMAP_FETCH */ \
  /*       Index: <op_main_imap_fetch> */ \
  _fmt(OP_MAIN_IMAP_FETCH,                    N_("Force retrieval of mail from IMAP server")) \
  /* L10N: Help screen description for OP_MAIN_IMAP_LOGOUT_ALL */ \
  /*       Index: <op_main_imap_logout_all> */ \
  _fmt(OP_MAIN_IMAP_LOGOUT_ALL,               N_("Logout from all IMAP servers")) \
  /* L10N: Help screen description for OP_MAIN_LIMIT */ \
  /*       Alias Dialog: <op_main_limit> */ \
  /*       Index: <op_main_limit> */ \
  _fmt(OP_MAIN_LIMIT,                         N_("Show only messages matching a pattern")) \
  /* L10N: Help screen description for OP_MAIN_LINK_THREADS */ \
  /*       Index: <op_main_link_threads> */ \
  _fmt(OP_MAIN_LINK_THREADS,                  N_("Link tagged message to the current one")) \
  /* L10N: Help screen description for OP_MAIN_MODIFY_TAGS */ \
  /*       Index: <op_main_modify_tags> */ \
  _fmt(OP_MAIN_MODIFY_TAGS,                   N_("Modify (notmuch/imap) tags")) \
  /* L10N: Help screen description for OP_MAIN_MODIFY_TAGS_THEN_HIDE */ \
  /*       Index: <op_main_modify_tags> */ \
  _fmt(OP_MAIN_MODIFY_TAGS_THEN_HIDE,         N_("Modify (notmuch/imap) tags and then hide message")) \
  /* L10N: Help screen description for OP_MAIN_NEXT_NEW */ \
  /*       Index: <op_main_next_new> */ \
  _fmt(OP_MAIN_NEXT_NEW,                      N_("Jump to the next new message")) \
  /* L10N: Help screen description for OP_MAIN_NEXT_NEW_THEN_UNREAD */ \
  /*       Index: <op_main_next_new> */ \
  _fmt(OP_MAIN_NEXT_NEW_THEN_UNREAD,          N_("Jump to the next new or unread message")) \
  /* L10N: Help screen description for OP_MAIN_NEXT_SUBTHREAD */ \
  /*       Index: <op_main_next_thread> */ \
  _fmt(OP_MAIN_NEXT_SUBTHREAD,                N_("Jump to the next subthread")) \
  /* L10N: Help screen description for OP_MAIN_NEXT_THREAD */ \
  /*       Index: <op_main_next_thread> */ \
  _fmt(OP_MAIN_NEXT_THREAD,                   N_("Jump to the next thread")) \
  /* L10N: Help screen description for OP_MAIN_NEXT_UNDELETED */ \
  /*       Index: <op_main_next_undeleted> */ \
  _fmt(OP_MAIN_NEXT_UNDELETED,                N_("Move to the next undeleted message")) \
  /* L10N: Help screen description for OP_MAIN_NEXT_UNREAD */ \
  /*       Index: <op_main_next_new> */ \
  _fmt(OP_MAIN_NEXT_UNREAD,                   N_("Jump to the next unread message")) \
  /* L10N: Help screen description for OP_MAIN_NEXT_UNREAD_MAILBOX */ \
  /*       Index: <op_main_next_unread_mailbox> */ \
  _fmt(OP_MAIN_NEXT_UNREAD_MAILBOX,           N_("Open next mailbox with new mail")) \
  /* L10N: Help screen description for OP_MAIN_OPEN_ALL_THREADS */ \
  /*       Index: <op_main_open_all_threads> */ \
  _fmt(OP_MAIN_OPEN_ALL_THREADS,              N_("Uncollapse all threads")) \
  /* L10N: Help screen description for OP_MAIN_OPEN_THREAD */ \
  /*       Index: <op_main_open_thread> */ \
  _fmt(OP_MAIN_OPEN_THREAD,                   N_("Uncollapse current thread")) \
  /* L10N: Help screen description for OP_MAIN_PARENT_MESSAGE */ \
  /*       Index: <op_main_root_message> */ \
  _fmt(OP_MAIN_PARENT_MESSAGE,                N_("Jump to parent message in thread")) \
  /* L10N: Help screen description for OP_MAIN_PREV_NEW */ \
  /*       Index: <op_main_next_new> */ \
  _fmt(OP_MAIN_PREV_NEW,                      N_("Jump to the previous new message")) \
  /* L10N: Help screen description for OP_MAIN_PREV_NEW_THEN_UNREAD */ \
  /*       Index: <op_main_next_new> */ \
  _fmt(OP_MAIN_PREV_NEW_THEN_UNREAD,          N_("Jump to the previous new or unread message")) \
  /* L10N: Help screen description for OP_MAIN_PREV_SUBTHREAD */ \
  /*       Index: <op_main_next_thread> */ \
  _fmt(OP_MAIN_PREV_SUBTHREAD,                N_("Jump to previous subthread")) \
  /* L10N: Help screen description for OP_MAIN_PREV_THREAD */ \
  /*       Index: <op_main_next_thread> */ \
  _fmt(OP_MAIN_PREV_THREAD,                   N_("Jump to previous thread")) \
  /* L10N: Help screen description for OP_MAIN_PREV_UNDELETED */ \
  /*       Index: <op_main_prev_undeleted> */ \
  _fmt(OP_MAIN_PREV_UNDELETED,                N_("Move to the previous undeleted message")) \
  /* L10N: Help screen description for OP_MAIN_PREV_UNREAD */ \
  /*       Index: <op_main_next_new> */ \
  _fmt(OP_MAIN_PREV_UNREAD,                   N_("Jump to the previous unread message")) \
  /* L10N: Help screen description for OP_MAIN_QUASI_DELETE */ \
  /*       Index: <op_main_quasi_delete> */ \
  _fmt(OP_MAIN_QUASI_DELETE,                  N_("Delete from NeoMutt, don't touch on disk")) \
  /* L10N: Help screen description for OP_MAIN_READ_SUBTHREAD */ \
  /*       Index: <op_main_read_thread> */ \
  _fmt(OP_MAIN_READ_SUBTHREAD,                N_("Mark the current subthread as read")) \
  /* L10N: Help screen description for OP_MAIN_READ_THREAD */ \
  /*       Index: <op_main_read_thread> */ \
  _fmt(OP_MAIN_READ_THREAD,                   N_("Mark the current thread as read")) \
  /* L10N: Help screen description for OP_MAIN_ROOT_MESSAGE */ \
  /*       Index: <op_main_root_message> */ \
  _fmt(OP_MAIN_ROOT_MESSAGE,                  N_("Jump to root message in thread")) \
  /* L10N: Help screen description for OP_MAIN_SET_FLAG */ \
  /*       Index: <op_main_set_flag> */ \
  _fmt(OP_MAIN_SET_FLAG,                      N_("Set a status flag on a message")) \
  /* L10N: Help screen description for OP_MAIN_SHOW_LIMIT */ \
  /*       Index: <op_main_show_limit> */ \
  _fmt(OP_MAIN_SHOW_LIMIT,                    N_("Show currently active limit pattern")) \
  /* L10N: Help screen description for OP_MAIN_SYNC_FOLDER */ \
  /*       Index: <op_main_sync_folder> */ \
  _fmt(OP_MAIN_SYNC_FOLDER,                   N_("Save changes to mailbox")) \
  /* L10N: Help screen description for OP_MAIN_TAG_PATTERN */ \
  /*       Index: <op_main_tag_pattern> */ \
  _fmt(OP_MAIN_TAG_PATTERN,                   N_("Tag non-hidden messages matching a pattern")) \
  /* L10N: Help screen description for OP_MAIN_UNDELETE_PATTERN */ \
  /*       Index: <op_main_undelete_pattern> */ \
  _fmt(OP_MAIN_UNDELETE_PATTERN,              N_("Undelete non-hidden messages matching a pattern")) \
  /* L10N: Help screen description for OP_MAIN_UNTAG_PATTERN */ \
  /*       Index: <op_main_untag_pattern> */ \
  _fmt(OP_MAIN_UNTAG_PATTERN,                 N_("Untag non-hidden messages matching a pattern")) \
  /* L10N: Help screen description for OP_MARK_MSG */ \
  /*       Index: <op_mark_msg> */ \
  _fmt(OP_MARK_MSG,                           N_("Create a hotkey macro for the current message")) \
  /* L10N: Help screen description for OP_MIDDLE_PAGE */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_MIDDLE_PAGE,                        N_("Move to the middle of the page")) \
  /* L10N: Help screen description for OP_NEXT_ENTRY */ \
  /*       Menu: <menu_movement> */ \
  /*       Index: <op_next_entry> */ \
  _fmt(OP_NEXT_ENTRY,                         N_("Move to the next entry")) \
  /* L10N: Help screen description for OP_NEXT_LINE */ \
  /*       Menu: <menu_movement> */ \
  /*       Pager: <op_pager_next_line> */ \
  _fmt(OP_NEXT_LINE,                          N_("Scroll down one line")) \
  /* L10N: Help screen description for OP_NEXT_PAGE */ \
  /*       Menu: <menu_movement> */ \
  /*       Pager: <op_pager_next_page> */ \
  _fmt(OP_NEXT_PAGE,                          N_("Move to the next page")) \
  /* L10N: Help screen description for OP_PAGER_BOTTOM */ \
  /*       Pager: <op_pager_bottom> */ \
  _fmt(OP_PAGER_BOTTOM,                       N_("Jump to the bottom of the message")) \
  /* L10N: Help screen description for OP_PAGER_HIDE_QUOTED */ \
  /*       Pager: <op_pager_hide_quoted> */ \
  _fmt(OP_PAGER_HIDE_QUOTED,                  N_("Toggle display of quoted text")) \
  /* L10N: Help screen description for OP_PAGER_SKIP_HEADERS */ \
  /*       Pager: <op_pager_skip_headers> */ \
  _fmt(OP_PAGER_SKIP_HEADERS,                 N_("Jump to first line after headers")) \
  /* L10N: Help screen description for OP_PAGER_SKIP_QUOTED */ \
  /*       Pager: <op_pager_skip_quoted> */ \
  _fmt(OP_PAGER_SKIP_QUOTED,                  N_("Skip beyond quoted text")) \
  /* L10N: Help screen description for OP_PAGER_TOP */ \
  /*       Pager: <op_pager_top> */ \
  _fmt(OP_PAGER_TOP,                          N_("Jump to the top of the message")) \
  /* L10N: Help screen description for OP_PIPE */ \
  /*       Compose Dialog: <op_attach_filter> */ \
  /*       Attach Dialog: <op_attach_pipe> */ \
  /*       Index: <op_pipe> */ \
  _fmt(OP_PIPE,                               N_("Pipe message/attachment to a shell command")) \
  /* L10N: Help screen description for OP_POST */ \
  /*       Index: <op_post> */ \
  _fmt(OP_POST,                               N_("Post message to newsgroup")) \
  /* L10N: Help screen description for OP_PREV_ENTRY */ \
  /*       Menu: <menu_movement> */ \
  /*       Index: <op_prev_entry> */ \
  _fmt(OP_PREV_ENTRY,                         N_("Move to the previous entry")) \
  /* L10N: Help screen description for OP_PREV_LINE */ \
  /*       Menu: <menu_movement> */ \
  /*       Pager: <op_pager_prev_line> */ \
  _fmt(OP_PREV_LINE,                          N_("Scroll up one line")) \
  /* L10N: Help screen description for OP_PREV_PAGE */ \
  /*       Menu: <menu_movement> */ \
  /*       Pager: <op_pager_prev_page> */ \
  _fmt(OP_PREV_PAGE,                          N_("Move to the previous page")) \
  /* L10N: Help screen description for OP_PRINT */ \
  /*       Index: <op_print> */ \
  _fmt(OP_PRINT,                              N_("Print the current entry")) \
  /* L10N: Help screen description for OP_PURGE_MESSAGE */ \
  /*       Index: <op_delete> */ \
  _fmt(OP_PURGE_MESSAGE,                      N_("Delete the current entry, bypassing the trash folder")) \
  /* L10N: Help screen description for OP_PURGE_THREAD */ \
  /*       Index: <op_delete_thread> */ \
  _fmt(OP_PURGE_THREAD,                       N_("Delete the current thread, bypassing the trash folder")) \
  /* L10N: Help screen description for OP_QUERY */ \
  /*       Alias Dialog: <op_query> */ \
  /*       Index: <op_query> */ \
  _fmt(OP_QUERY,                              N_("Query external program for addresses")) \
  /* L10N: Help screen description for OP_QUERY_APPEND */ \
  /*       Alias Dialog: <op_query> */ \
  _fmt(OP_QUERY_APPEND,                       N_("Append new query results to current results")) \
  /* L10N: Help screen description for OP_QUIT */ \
  /*       History Dialog: <op_quit> */ \
  /*       Index: <op_quit> */ \
  /*       Pattern Dialog: <op_quit> */ \
  _fmt(OP_QUIT,                               N_("Save changes to mailbox and quit")) \
  /* L10N: Help screen description for OP_RECALL_MESSAGE */ \
  /*       Index: <op_recall_message> */ \
  _fmt(OP_RECALL_MESSAGE,                     N_("Recall a postponed message")) \
  /* L10N: Help screen description for OP_RECONSTRUCT_THREAD */ \
  /*       Index: <op_get_children> */ \
  _fmt(OP_RECONSTRUCT_THREAD,                 N_("Reconstruct thread containing current message")) \
  /* L10N: Help screen description for OP_REDRAW */ \
  /*       Text Entry: <op_redraw> */ \
  /*       Global: <op_redraw> */ \
  _fmt(OP_REDRAW,                             N_("Clear and redraw the screen")) \
  /* L10N: Help screen description for OP_RENAME_MAILBOX */ \
  /*       Browser: <op_rename_mailbox> */ \
  _fmt(OP_RENAME_MAILBOX,                     N_("Rename the current mailbox (IMAP only)")) \
  /* L10N: Help screen description for OP_REPLY */ \
  /*       Attach Dialog: <op_reply> */ \
  /*       Index: <op_reply> */ \
  _fmt(OP_REPLY,                              N_("Reply to a message")) \
  /* L10N: Help screen description for OP_RESEND */ \
  /*       Attach Dialog: <op_resend> */ \
  /*       Index: <op_resend> */ \
  _fmt(OP_RESEND,                             N_("Use the current message as a template for a new one")) \
  /* L10N: Help screen description for OP_SAVE */ \
  /*       Index: <op_save> */ \
  /*       Pager: <op_save> */ \
  _fmt(OP_SAVE,                               N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help screen description for OP_SEARCH */ \
  /*       Menu: <menu_search> */ \
  /*       Pager: <op_pager_search> */ \
  /*       Alias Dialog: <op_search> */ \
  /*       Index: <op_search> */ \
  /*       Postponed Dialog: <op_search> */ \
  _fmt(OP_SEARCH,                             N_("Search for a regular expression")) \
  /* L10N: Help screen description for OP_SEARCH_NEXT */ \
  /*       Menu: <menu_search> */ \
  /*       Pager: <op_pager_search_next> */ \
  /*       Alias Dialog: <op_search> */ \
  /*       Index: <op_search> */ \
  /*       Postponed Dialog: <op_search> */ \
  _fmt(OP_SEARCH_NEXT,                        N_("Search for next match")) \
  /* L10N: Help screen description for OP_SEARCH_OPPOSITE */ \
  /*       Menu: <menu_search> */ \
  /*       Pager: <op_pager_search_next> */ \
  /*       Alias Dialog: <op_search> */ \
  /*       Index: <op_search> */ \
  /*       Postponed Dialog: <op_search> */ \
  _fmt(OP_SEARCH_OPPOSITE,                    N_("Search for next match in opposite direction")) \
  /* L10N: Help screen description for OP_SEARCH_REVERSE */ \
  /*       Menu: <menu_search> */ \
  /*       Pager: <op_pager_search> */ \
  /*       Alias Dialog: <op_search> */ \
  /*       Index: <op_search> */ \
  /*       Postponed Dialog: <op_search> */ \
  _fmt(OP_SEARCH_REVERSE,                     N_("Search backwards for a regular expression")) \
  /* L10N: Help screen description for OP_SEARCH_TOGGLE */ \
  /*       Pager: <op_search_toggle> */ \
  _fmt(OP_SEARCH_TOGGLE,                      N_("Toggle search pattern coloring")) \
  /* L10N: Help screen description for OP_SHELL_ESCAPE */ \
  /*       Global: <op_shell_escape> */ \
  _fmt(OP_SHELL_ESCAPE,                       N_("Invoke a command in a subshell")) \
  /* L10N: Help screen description for OP_SHOW_LOG_MESSAGES */ \
  /*       Global: <op_show_log_messages> */ \
  _fmt(OP_SHOW_LOG_MESSAGES,                  N_("Show log (and debug) messages")) \
  /* L10N: Help screen description for OP_SORT */ \
  /*       Alias Dialog: <op_sort> */ \
  /*       Browser: <op_sort> */ \
  /*       Index: <op_sort> */ \
  _fmt(OP_SORT,                               N_("Sort messages")) \
  /* L10N: Help screen description for OP_SORT_REVERSE */ \
  /*       Alias Dialog: <op_sort> */ \
  /*       Browser: <op_sort> */ \
  /*       Index: <op_sort> */ \
  _fmt(OP_SORT_REVERSE,                       N_("Sort messages in reverse order")) \
  /* L10N: Help screen description for OP_SUBSCRIBE_PATTERN */ \
  /*       Browser: <op_subscribe_pattern> */ \
  _fmt(OP_SUBSCRIBE_PATTERN,                  N_("Subscribe to newsgroups matching a pattern")) \
  /* L10N: Help screen description for OP_TAG */ \
  /*       Index: <op_tag> */ \
  _fmt(OP_TAG,                                N_("Tag the current entry")) \
  /* L10N: Help screen description for OP_TAG_PREFIX */ \
  _fmt(OP_TAG_PREFIX,                         N_("Apply next function to tagged messages")) \
  /* L10N: Help screen description for OP_TAG_PREFIX_COND */ \
  _fmt(OP_TAG_PREFIX_COND,                    N_("Apply next function ONLY to tagged messages")) \
  /* L10N: Help screen description for OP_TAG_SUBTHREAD */ \
  /*       Index: <op_tag_thread> */ \
  _fmt(OP_TAG_SUBTHREAD,                      N_("Tag the current subthread")) \
  /* L10N: Help screen description for OP_TAG_THREAD */ \
  /*       Index: <op_tag_thread> */ \
  _fmt(OP_TAG_THREAD,                         N_("Tag the current thread")) \
  /* L10N: Help screen description for OP_TOGGLE_MAILBOXES */ \
  /*       Browser: <op_toggle_mailboxes> */ \
  _fmt(OP_TOGGLE_MAILBOXES,                   N_("Toggle whether to browse mailboxes or all files")) \
  /* L10N: Help screen description for OP_TOGGLE_NEW */ \
  /*       Index: <op_toggle_new> */ \
  _fmt(OP_TOGGLE_NEW,                         N_("Toggle a message's 'new' flag")) \
  /* L10N: Help screen description for OP_TOGGLE_READ */ \
  /*       Index: <op_main_limit> */ \
  _fmt(OP_TOGGLE_READ,                        N_("Toggle view of read messages")) \
  /* L10N: Help screen description for OP_TOGGLE_WRITE */ \
  /*       Index: <op_toggle_write> */ \
  _fmt(OP_TOGGLE_WRITE,                       N_("Toggle whether the mailbox will be rewritten")) \
  /* L10N: Help screen description for OP_TOP_PAGE */ \
  /*       Menu: <menu_movement> */ \
  _fmt(OP_TOP_PAGE,                           N_("Move to the top of the page")) \
  /* L10N: Help screen description for OP_UNCATCHUP */ \
  /*       Browser: <op_catchup> */ \
  _fmt(OP_UNCATCHUP,                          N_("Mark all articles in newsgroup as unread")) \
  /* L10N: Help screen description for OP_UNDELETE */ \
  /*       Alias Dialog: <op_delete> */ \
  /*       Postponed Dialog: <op_delete> */ \
  /*       Index: <op_undelete> */ \
  _fmt(OP_UNDELETE,                           N_("Undelete the current entry")) \
  /* L10N: Help screen description for OP_UNDELETE_SUBTHREAD */ \
  /*       Index: <op_undelete_thread> */ \
  _fmt(OP_UNDELETE_SUBTHREAD,                 N_("Undelete all messages in subthread")) \
  /* L10N: Help screen description for OP_UNDELETE_THREAD */ \
  /*       Index: <op_undelete_thread> */ \
  _fmt(OP_UNDELETE_THREAD,                    N_("Undelete all messages in thread")) \
  /* L10N: Help screen description for OP_UNSUBSCRIBE_PATTERN */ \
  /*       Browser: <op_subscribe_pattern> */ \
  _fmt(OP_UNSUBSCRIBE_PATTERN,                N_("Unsubscribe from newsgroups matching a pattern")) \
  /* L10N: Help screen description for OP_VERSION */ \
  /*       Global: <op_version> */ \
  _fmt(OP_VERSION,                            N_("Show the NeoMutt version number and date")) \
  /* L10N: Help screen description for OP_VIEW_ATTACHMENTS */ \
  /*       Index: <op_view_attachments> */ \
  /*       Pager: <op_view_attachments> */ \
  _fmt(OP_VIEW_ATTACHMENTS,                   N_("Show MIME attachments")) \
  /* L10N: Help screen description for OP_VIEW_RAW_MESSAGE */ \
  /*       Index: <op_edit_raw_message> */ \
  _fmt(OP_VIEW_RAW_MESSAGE,                   N_("Show the raw message")) \
  /* L10N: Help screen description for OP_WHAT_KEY */ \
  /*       Global: <op_what_key> */ \
  _fmt(OP_WHAT_KEY,                           N_("Display the keycode for a key press")) \

#define OPS_CRYPT(_fmt) \
  /* L10N: Help screen description for OP_DECRYPT_COPY */ \
  /*       Index: <op_save> */ \
  _fmt(OP_DECRYPT_COPY,                       N_("Make decrypted copy")) \
  /* L10N: Help screen description for OP_DECRYPT_SAVE */ \
  /*       Index: <op_save> */ \
  _fmt(OP_DECRYPT_SAVE,                       N_("Make decrypted copy and delete")) \
  /* L10N: Help screen description for OP_EXTRACT_KEYS */ \
  /*       Attach Dialog: <op_extract_keys> */ \
  /*       Index: <op_extract_keys> */ \
  _fmt(OP_EXTRACT_KEYS,                       N_("Extract supported public keys")) \
  /* L10N: Help screen description for OP_FORGET_PASSPHRASE */ \
  /*       Attach Dialog: <op_forget_passphrase> */ \
  /*       Compose Dialog: <op_forget_passphrase> */ \
  /*       Index: <op_forget_passphrase> */ \
  _fmt(OP_FORGET_PASSPHRASE,                  N_("Wipe passphrases from memory")) \

#define OPS_ENVELOPE(_fmt) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_BCC */ \
  /*       Envelope Window: <op_envelope_edit_bcc> */ \
  _fmt(OP_ENVELOPE_EDIT_BCC,                  N_("Edit the BCC list")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_CC */ \
  /*       Envelope Window: <op_envelope_edit_cc> */ \
  _fmt(OP_ENVELOPE_EDIT_CC,                   N_("Edit the CC list")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_FCC */ \
  /*       Envelope Window: <op_envelope_edit_fcc> */ \
  _fmt(OP_ENVELOPE_EDIT_FCC,                  N_("Enter a file to save a copy of this message in")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_FOLLOWUP_TO */ \
  /*       Envelope Window: <op_envelope_edit_followup_to> */ \
  _fmt(OP_ENVELOPE_EDIT_FOLLOWUP_TO,          N_("Edit the Followup-To field")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_FROM */ \
  /*       Envelope Window: <op_envelope_edit_from> */ \
  _fmt(OP_ENVELOPE_EDIT_FROM,                 N_("Edit the from field")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_HEADERS */ \
  /*       Compose Dialog: <op_envelope_edit_headers> */ \
  _fmt(OP_ENVELOPE_EDIT_HEADERS,              N_("Edit the message with headers")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_NEWSGROUPS */ \
  /*       Envelope Window: <op_envelope_edit_newsgroups> */ \
  _fmt(OP_ENVELOPE_EDIT_NEWSGROUPS,           N_("Edit the newsgroups list")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_REPLY_TO */ \
  /*       Envelope Window: <op_envelope_edit_reply_to> */ \
  _fmt(OP_ENVELOPE_EDIT_REPLY_TO,             N_("Edit the Reply-To field")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_SUBJECT */ \
  /*       Envelope Window: <op_envelope_edit_subject> */ \
  _fmt(OP_ENVELOPE_EDIT_SUBJECT,              N_("Edit the subject of this message")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_TO */ \
  /*       Envelope Window: <op_envelope_edit_to> */ \
  _fmt(OP_ENVELOPE_EDIT_TO,                   N_("Edit the TO list")) \
  /* L10N: Help screen description for OP_ENVELOPE_EDIT_X_COMMENT_TO */ \
  /*       Envelope Window: <op_envelope_edit_x_comment_to> */ \
  _fmt(OP_ENVELOPE_EDIT_X_COMMENT_TO,         N_("Edit the X-Comment-To field")) \

#ifdef USE_NOTMUCH
#define OPS_NOTMUCH(_fmt) \
  /* L10N: Help screen description for OP_MAIN_CHANGE_VFOLDER */ \
  /*       Index: <op_main_change_folder> */ \
  _fmt(OP_MAIN_CHANGE_VFOLDER,                N_("Open a different virtual folder")) \
  /* L10N: Help screen description for OP_MAIN_ENTIRE_THREAD */ \
  /*       Index: <op_main_entire_thread> */ \
  _fmt(OP_MAIN_ENTIRE_THREAD,                 N_("Read entire thread of the current message")) \
  /* L10N: Help screen description for OP_MAIN_VFOLDER_FROM_QUERY */ \
  /*       Index: <op_main_vfolder_from_query> */ \
  _fmt(OP_MAIN_VFOLDER_FROM_QUERY,            N_("Generate virtual folder from query")) \
  /* L10N: Help screen description for OP_MAIN_VFOLDER_FROM_QUERY_READONLY */ \
  /*       Index: <op_main_vfolder_from_query> */ \
  _fmt(OP_MAIN_VFOLDER_FROM_QUERY_READONLY,   N_("Generate a read-only virtual folder from query")) \
  /* L10N: Help screen description for OP_MAIN_WINDOWED_VFOLDER_BACKWARD */ \
  /*       Index: <op_main_windowed_vfolder> */ \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_BACKWARD,     N_("Shifts virtual folder time window backwards")) \
  /* L10N: Help screen description for OP_MAIN_WINDOWED_VFOLDER_FORWARD */ \
  /*       Index: <op_main_windowed_vfolder> */ \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_FORWARD,      N_("Shifts virtual folder time window forwards")) \
  /* L10N: Help screen description for OP_MAIN_WINDOWED_VFOLDER_RESET */ \
  /*       Index: <op_main_windowed_vfolder> */ \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_RESET,        N_("Resets virtual folder time window to the present"))
#else
#define OPS_NOTMUCH(_)
#endif

#define OPS_PGP(_fmt) \
  /* L10N: Help screen description for OP_ATTACH_ATTACH_KEY */ \
  /*       Compose Dialog: <op_attach_attach_key> */ \
  _fmt(OP_ATTACH_ATTACH_KEY,                  N_("Attach a PGP public key")) \
  /* L10N: Help screen description for OP_CHECK_TRADITIONAL */ \
  /*       Attach Dialog: <op_check_traditional> */ \
  /*       Index: <op_check_traditional> */ \
  _fmt(OP_CHECK_TRADITIONAL,                  N_("Check for classic PGP")) \
  /* L10N: Help screen description for OP_COMPOSE_PGP_MENU */ \
  /*       Envelope Window: <op_compose_pgp_menu> */ \
  _fmt(OP_COMPOSE_PGP_MENU,                   N_("Show PGP options")) \
  /* L10N: Help screen description for OP_MAIL_KEY */ \
  /*       Index: <op_mail_key> */ \
  _fmt(OP_MAIL_KEY,                           N_("Mail a PGP public key")) \
  /* L10N: Help screen description for OP_VERIFY_KEY */ \
  /*       GPGME Key Selection Dialog: <op_verify_key> */ \
  /*       PGP Key Selection Dialog: <op_verify_key> */ \
  _fmt(OP_VERIFY_KEY,                         N_("Verify a public key")) \
  /* L10N: Help screen description for OP_VIEW_ID */ \
  /*       GPGME Key Selection Dialog: <op_view_id> */ \
  /*       PGP Key Selection Dialog: <op_view_id> */ \
  _fmt(OP_VIEW_ID,                            N_("View the key's user id")) \

#define OPS_SIDEBAR(_fmt) \
  /* L10N: Help screen description for OP_SIDEBAR_FIRST */ \
  /*       Sidebar: <op_sidebar_first> */ \
  _fmt(OP_SIDEBAR_FIRST,                      N_("Move the highlight to the first mailbox")) \
  /* L10N: Help screen description for OP_SIDEBAR_LAST */ \
  /*       Sidebar: <op_sidebar_last> */ \
  _fmt(OP_SIDEBAR_LAST,                       N_("Move the highlight to the last mailbox")) \
  /* L10N: Help screen description for OP_SIDEBAR_NEXT */ \
  /*       Sidebar: <op_sidebar_next> */ \
  _fmt(OP_SIDEBAR_NEXT,                       N_("Move the highlight to next mailbox")) \
  /* L10N: Help screen description for OP_SIDEBAR_NEXT_NEW */ \
  /*       Sidebar: <op_sidebar_next_new> */ \
  _fmt(OP_SIDEBAR_NEXT_NEW,                   N_("Move the highlight to next mailbox with new mail")) \
  /* L10N: Help screen description for OP_SIDEBAR_OPEN */ \
  /*       Sidebar: <op_sidebar_open> */ \
  _fmt(OP_SIDEBAR_OPEN,                       N_("Open highlighted mailbox")) \
  /* L10N: Help screen description for OP_SIDEBAR_PAGE_DOWN */ \
  /*       Sidebar: <op_sidebar_page_down> */ \
  _fmt(OP_SIDEBAR_PAGE_DOWN,                  N_("Scroll the sidebar down 1 page")) \
  /* L10N: Help screen description for OP_SIDEBAR_PAGE_UP */ \
  /*       Sidebar: <op_sidebar_page_up> */ \
  _fmt(OP_SIDEBAR_PAGE_UP,                    N_("Scroll the sidebar up 1 page")) \
  /* L10N: Help screen description for OP_SIDEBAR_PREV */ \
  /*       Sidebar: <op_sidebar_prev> */ \
  _fmt(OP_SIDEBAR_PREV,                       N_("Move the highlight to previous mailbox")) \
  /* L10N: Help screen description for OP_SIDEBAR_PREV_NEW */ \
  /*       Sidebar: <op_sidebar_prev_new> */ \
  _fmt(OP_SIDEBAR_PREV_NEW,                   N_("Move the highlight to previous mailbox with new mail")) \
  /* L10N: Help screen description for OP_SIDEBAR_TOGGLE_VIRTUAL */ \
  /*       Sidebar: <op_sidebar_toggle_virtual> */ \
  _fmt(OP_SIDEBAR_TOGGLE_VIRTUAL,             N_("Toggle between mailboxes and virtual mailboxes")) \
  /* L10N: Help screen description for OP_SIDEBAR_TOGGLE_VISIBLE */ \
  /*       Sidebar: <op_sidebar_toggle_visible> */ \
  _fmt(OP_SIDEBAR_TOGGLE_VISIBLE,             N_("Make the sidebar (in)visible")) \
  /* L10N: Help screen description for OP_SIDEBAR_ABORT_SEARCH */ \
  /*       Sidebar: <op_sidebar_abort_search> */ \
  _fmt(OP_SIDEBAR_ABORT_SEARCH,               N_("Close the sidebar search")) \
  /* L10N: Help screen description for OP_SIDEBAR_START_SEARCH */ \
  /*       Sidebar: <op_sidebar_start_search> */ \
  _fmt(OP_SIDEBAR_START_SEARCH,               N_("Fuzzy search the sidebar")) \

#define OPS_SMIME(_fmt) \
  /* L10N: Help screen description for OP_COMPOSE_SMIME_MENU */ \
  /*       Envelope Window: <op_compose_smime_menu> */ \
  _fmt(OP_COMPOSE_SMIME_MENU,                 N_("Show S/MIME options")) \

#define OPS(_fmt) \
  _fmt(OP_NULL,                               N_("Null operation")) \
  OPS_ATTACH(_fmt) \
  OPS_AUTOCRYPT(_fmt) \
  OPS_CORE(_fmt) \
  OPS_CRYPT(_fmt) \
  OPS_ENVELOPE(_fmt) \
  OPS_NOTMUCH(_fmt) \
  OPS_PGP(_fmt) \
  OPS_SIDEBAR(_fmt) \
  OPS_SMIME(_fmt) \

/**
 * enum MuttOps - All NeoMutt Opcodes
 *
 * Opcodes, e.g. OP_TOGGLE_NEW
 */
enum MuttOps {
#define DEFINE_OPS(opcode, help_string) opcode,
  OPS(DEFINE_OPS)
#undef DEFINE_OPS
  OP_MAX,
};
// clang-format on

#endif /* MUTT_OPCODES_H */
