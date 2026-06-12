/**
 * @file
 * All user-callable functions
 *
 * @authors
 * Copyright (C) 2017 Damien Riegel <damien.riegel@gmail.com>
 * Copyright (C) 2018-2026 Richard Russon <rich@flatcap.org>
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
  /* L10N: Help for Compose function: <attach-file> */ \
  _fmt(OP_ATTACH_ATTACH_FILE,                 N_("Attach files to this message")) \
  /* L10N: Help for Compose function: <attach-message> */ \
  _fmt(OP_ATTACH_ATTACH_MESSAGE,              N_("Attach messages to this message")) \
  /* L10N: Help for Compose function: <attach-news-message> */ \
  _fmt(OP_ATTACH_ATTACH_NEWS_MESSAGE,         N_("Attach news articles to this message")) \
  /* L10N: Help for Attach function: <collapse-parts> */ \
  _fmt(OP_ATTACH_COLLAPSE,                    N_("Toggle display of subparts")) \
  /* L10N: Help for Attach function: <delete-entry> */ \
  _fmt(OP_ATTACH_DELETE,                      N_("Delete the current entry")) \
  /* L10N: Help for Compose function: <detach-file> */ \
  _fmt(OP_ATTACH_DETACH,                      N_("Delete the current entry")) \
  /* L10N: Help for Compose function: <edit-content-id> */ \
  _fmt(OP_ATTACH_EDIT_CONTENT_ID,             N_("Edit the 'Content-ID' of the attachment")) \
  /* L10N: Help for Compose function: <edit-description> */ \
  _fmt(OP_ATTACH_EDIT_DESCRIPTION,            N_("Edit attachment description")) \
  /* L10N: Help for Compose function: <edit-encoding> */ \
  _fmt(OP_ATTACH_EDIT_ENCODING,               N_("Edit attachment transfer-encoding")) \
  /* L10N: Help for Compose function: <edit-language> */ \
  _fmt(OP_ATTACH_EDIT_LANGUAGE,               N_("Edit the 'Content-Language' of the attachment")) \
  /* L10N: Help for Compose function: <edit-mime> */ \
  _fmt(OP_ATTACH_EDIT_MIME,                   N_("Edit attachment using mailcap entry")) \
  /* L10N: Help for Attach, Compose, Index function: <edit-type> */ \
  _fmt(OP_ATTACH_EDIT_TYPE,                   N_("Edit attachment content type")) \
  /* L10N: Help for Compose function: <filter-entry> */ \
  _fmt(OP_ATTACH_FILTER,                      N_("Filter attachment through a shell command")) \
  /* L10N: Help for Compose function: <get-attachment> */ \
  _fmt(OP_ATTACH_GET_ATTACHMENT,              N_("Get a temporary copy of an attachment")) \
  /* L10N: Help for Compose function: <group-alternatives> */ \
  _fmt(OP_ATTACH_GROUP_ALTS,                  N_("Group tagged attachments as 'multipart/alternative'")) \
  /* L10N: Help for Compose function: <group-multilingual> */ \
  _fmt(OP_ATTACH_GROUP_LINGUAL,               N_("Group tagged attachments as 'multipart/multilingual'")) \
  /* L10N: Help for Compose function: <group-related> */ \
  _fmt(OP_ATTACH_GROUP_RELATED,               N_("Group tagged attachments as 'multipart/related'")) \
  /* L10N: Help for Compose function: <move-down> */ \
  _fmt(OP_ATTACH_MOVE_DOWN,                   N_("Move an attachment down in the attachment list")) \
  /* L10N: Help for Compose function: <move-up> */ \
  _fmt(OP_ATTACH_MOVE_UP,                     N_("Move an attachment up in the attachment list")) \
  /* L10N: Help for Compose function: <new-mime> */ \
  _fmt(OP_ATTACH_NEW_MIME,                    N_("Compose new attachment using mailcap entry")) \
  /* L10N: Help for Attach, Compose function: <print-entry> */ \
  _fmt(OP_ATTACH_PRINT,                       N_("Print the current entry")) \
  /* L10N: Help for Compose function: <rename-attachment> */ \
  _fmt(OP_ATTACH_RENAME_ATTACHMENT,           N_("Send attachment with a different name")) \
  /* L10N: Help for Compose, Attach function: <copy-file> */ \
  _fmt(OP_ATTACH_SAVE,                        N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help for Compose function: <toggle-disposition> */ \
  _fmt(OP_ATTACH_TOGGLE_DISPOSITION,          N_("Toggle disposition between inline/attachment")) \
  /* L10N: Help for Compose function: <toggle-recode> */ \
  _fmt(OP_ATTACH_TOGGLE_RECODE,               N_("Toggle recoding of this attachment")) \
  /* L10N: Help for Compose function: <toggle-unlink> */ \
  _fmt(OP_ATTACH_TOGGLE_UNLINK,               N_("Toggle whether to delete file after sending it")) \
  /* L10N: Help for Attach function: <undelete-entry> */ \
  _fmt(OP_ATTACH_UNDELETE,                    N_("Undelete the current entry")) \
  /* L10N: Help for Compose function: <ungroup-attachment> */ \
  _fmt(OP_ATTACH_UNGROUP,                     N_("Ungroup 'multipart' attachment")) \
  /* L10N: Help for Compose function: <update-encoding> */ \
  _fmt(OP_ATTACH_UPDATE_ENCODING,             N_("Update an attachment's encoding info")) \
  /* L10N: Help for Attach, Compose function: <view-attach> */ \
  _fmt(OP_ATTACH_VIEW,                        N_("View attachment using mailcap entry if necessary")) \
  /* L10N: Help for Attach, Compose function: <view-mailcap> */ \
  _fmt(OP_ATTACH_VIEW_MAILCAP,                N_("Force viewing of attachment using mailcap")) \
  /* L10N: Help for Attach, Compose function: <view-pager> */ \
  _fmt(OP_ATTACH_VIEW_PAGER,                  N_("View attachment in pager using copiousoutput mailcap")) \
  /* L10N: Help for Attach, Compose function: <view-text> */ \
  _fmt(OP_ATTACH_VIEW_TEXT,                   N_("View attachment as text")) \
  /* L10N: Help for Compose function: <preview-page-down> */ \
  _fmt(OP_PREVIEW_PAGE_DOWN,                  N_("Show the next page of the message")) \
  /* L10N: Help for Compose function: <preview-page-up> */ \
  _fmt(OP_PREVIEW_PAGE_UP,                    N_("Show the previous page of the message")) \

#ifdef USE_AUTOCRYPT
#define OPS_AUTOCRYPT(_fmt) \
  /* L10N: Help for Index function: <autocrypt-acct-menu> */ \
  _fmt(OP_AUTOCRYPT_ACCT_MENU,                N_("Manage autocrypt accounts")) \
  /* L10N: Help for Autocrypt function: <create-account> */ \
  _fmt(OP_AUTOCRYPT_CREATE_ACCT,              N_("Create a new autocrypt account")) \
  /* L10N: Help for Autocrypt function: <delete-account> */ \
  _fmt(OP_AUTOCRYPT_DELETE_ACCT,              N_("Delete the current account")) \
  /* L10N: Help for Autocrypt function: <toggle-active> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_ACTIVE,            N_("Toggle the current account active/inactive")) \
  /* L10N: Help for Autocrypt function: <toggle-prefer-encrypt> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_PREFER,            N_("Toggle the current account prefer-encrypt flag")) \
  /* L10N: Help for Compose function: <autocrypt-menu> */ \
  _fmt(OP_COMPOSE_AUTOCRYPT_MENU,             N_("Show autocrypt compose menu options"))
#else
#define OPS_AUTOCRYPT(_)
#endif

#define OPS_CORE(_fmt) \
  /* L10N: Help for Index function: <alias-dialog> */ \
  _fmt(OP_ALIAS_DIALOG,                       N_("Open the aliases dialog")) \
  /* L10N: Help for Pager: <bottom-page> */ \
  _fmt(OP_BOTTOM_PAGE,                        N_("Move to the bottom of the page")) \
  /* L10N: Help for Attach, Index function: <bounce-message> */ \
  _fmt(OP_BOUNCE_MESSAGE,                     N_("Remail a message to another user")) \
  /* L10N: Help for Browser function: <goto-folder> */ \
  _fmt(OP_BROWSER_GOTO_FOLDER,                N_("Swap the current folder position with $folder if it exists")) \
  /* L10N: Help for Browser function: <select-new> */ \
  _fmt(OP_BROWSER_NEW_FILE,                   N_("Select a new file in this directory")) \
  /* L10N: Help for Browser function: <subscribe> */ \
  _fmt(OP_BROWSER_SUBSCRIBE,                  N_("Subscribe to current mbox (IMAP/NNTP only)")) \
  /* L10N: Help for Browser function: <display-filename> */ \
  _fmt(OP_BROWSER_TELL,                       N_("Display the currently selected file's name")) \
  /* L10N: Help for Browser function: <toggle-subscribed> */ \
  _fmt(OP_BROWSER_TOGGLE_LSUB,                N_("Toggle view all/subscribed mailboxes (IMAP only)")) \
  /* L10N: Help for Browser function: <unsubscribe> */ \
  _fmt(OP_BROWSER_UNSUBSCRIBE,                N_("Unsubscribe from current mbox (IMAP/NNTP only)")) \
  /* L10N: Help for Browser function: <view-file> */ \
  _fmt(OP_BROWSER_VIEW_FILE,                  N_("View file")) \
  /* L10N: Help for Browser, Index function: <catchup> */ \
  _fmt(OP_CATCHUP,                            N_("Mark all articles in newsgroup as read")) \
  /* L10N: Help for Browser function: <change-dir> */ \
  _fmt(OP_CHANGE_DIRECTORY,                   N_("Change directories")) \
  /* L10N: Help for Browser function: <check-new> */ \
  _fmt(OP_CHECK_NEW,                          N_("Check mailboxes for new mail")) \
  /* L10N: Help for Generic function: <check-stats> */ \
  _fmt(OP_CHECK_STATS,                        N_("Calculate message statistics for all mailboxes")) \
  /* L10N: Help for Compose function: <edit-file> */ \
  _fmt(OP_COMPOSE_EDIT_FILE,                  N_("Edit the file to be attached")) \
  /* L10N: Help for Compose function: <edit-message> */ \
  _fmt(OP_COMPOSE_EDIT_MESSAGE,               N_("Edit the message")) \
  /* L10N: Help for Compose function: <ispell> */ \
  _fmt(OP_COMPOSE_ISPELL,                     N_("Run ispell on the message")) \
  /* L10N: Help for Compose function: <postpone-message> */ \
  _fmt(OP_COMPOSE_POSTPONE_MESSAGE,           N_("Save this message to send later")) \
  /* L10N: Help for Compose function: <rename-file> */ \
  _fmt(OP_COMPOSE_RENAME_FILE,                N_("Rename/move an attached file")) \
  /* L10N: Help for Compose function: <send-message> */ \
  _fmt(OP_COMPOSE_SEND_MESSAGE,               N_("Send the message")) \
  /* L10N: Help for Attach, Index function: <compose-to-sender> */ \
  _fmt(OP_COMPOSE_TO_SENDER,                  N_("Compose new message to the current message sender")) \
  /* L10N: Help for Compose function: <write-fcc> */ \
  _fmt(OP_COMPOSE_WRITE_MESSAGE,              N_("Write the message to a folder")) \
  /* L10N: Help for Index function: <copy-message> */ \
  _fmt(OP_COPY_MESSAGE,                       N_("Copy a message to a file/mailbox")) \
  /* L10N: Help for Index, Query function: <create-alias> */ \
  _fmt(OP_CREATE_ALIAS,                       N_("Create an alias from a message sender")) \
  /* L10N: Help for Browser function: <create-mailbox> */ \
  _fmt(OP_CREATE_MAILBOX,                     N_("Create a new mailbox (IMAP only)")) \
  /* L10N: Help for Generic function: <current-bottom> */ \
  _fmt(OP_CURRENT_BOTTOM,                     N_("Move entry to bottom of screen")) \
  /* L10N: Help for Generic function: <current-middle> */ \
  _fmt(OP_CURRENT_MIDDLE,                     N_("Move entry to middle of screen")) \
  /* L10N: Help for Generic function: <current-top> */ \
  _fmt(OP_CURRENT_TOP,                        N_("Move entry to top of screen")) \
  /* L10N: Help for Index function: <decode-copy> */ \
  _fmt(OP_DECODE_COPY,                        N_("Make decoded (text/plain) copy")) \
  /* L10N: Help for Index function: <decode-save> */ \
  _fmt(OP_DECODE_SAVE,                        N_("Make decoded copy (text/plain) and delete")) \
  /* L10N: Help for Alias, Postpone, Index function: <delete-entry> */ \
  _fmt(OP_DELETE,                             N_("Delete the current entry")) \
  /* L10N: Help for Browser function: <delete-mailbox> */ \
  _fmt(OP_DELETE_MAILBOX,                     N_("Delete the current mailbox (IMAP only)")) \
  /* L10N: Help for Index function: <delete-subthread> */ \
  _fmt(OP_DELETE_SUBTHREAD,                   N_("Delete all messages in subthread")) \
  /* L10N: Help for Index function: <delete-thread> */ \
  _fmt(OP_DELETE_THREAD,                      N_("Delete all messages in thread")) \
  /* L10N: Help for Browser function: <descend-directory> */ \
  _fmt(OP_DESCEND_DIRECTORY,                  N_("Descend into a directory")) \
  /* L10N: Help for Index function: <display-address> */ \
  _fmt(OP_DISPLAY_ADDRESS,                    N_("Display full address of sender")) \
  /* L10N: Help for Attach, Compose, Index function: <display-toggle-weed> */ \
  _fmt(OP_DISPLAY_HEADERS,                    N_("Display message and toggle header weeding")) \
  /* L10N: Help for Index function: <display-message> */ \
  _fmt(OP_DISPLAY_MESSAGE,                    N_("Display a message")) \
  /* L10N: Help for Editor function: <backspace> */ \
  _fmt(OP_EDITOR_BACKSPACE,                   N_("Delete the char in front of the cursor")) \
  /* L10N: Help for Editor function: <backward-char> */ \
  _fmt(OP_EDITOR_BACKWARD_CHAR,               N_("Move the cursor one character to the left")) \
  /* L10N: Help for Editor function: <backward-word> */ \
  _fmt(OP_EDITOR_BACKWARD_WORD,               N_("Move the cursor to the beginning of the word")) \
  /* L10N: Help for Editor function: <bol> */ \
  _fmt(OP_EDITOR_BOL,                         N_("Jump to the beginning of the line")) \
  /* L10N: Help for Editor function: <capitalize-word> */ \
  _fmt(OP_EDITOR_CAPITALIZE_WORD,             N_("Capitalize the word")) \
  /* L10N: Help for Editor function: <complete> */ \
  _fmt(OP_EDITOR_COMPLETE,                    N_("Complete filename or alias")) \
  /* L10N: Help for Editor function: <complete-query> */ \
  _fmt(OP_EDITOR_COMPLETE_QUERY,              N_("Complete address with query")) \
  /* L10N: Help for Editor function: <delete-char> */ \
  _fmt(OP_EDITOR_DELETE_CHAR,                 N_("Delete the char under the cursor")) \
  /* L10N: Help for Editor function: <downcase-word> */ \
  _fmt(OP_EDITOR_DOWNCASE_WORD,               N_("Convert the word to lower case")) \
  /* L10N: Help for Editor function: <eol> */ \
  _fmt(OP_EDITOR_EOL,                         N_("Jump to the end of the line")) \
  /* L10N: Help for Editor function: <forward-char> */ \
  _fmt(OP_EDITOR_FORWARD_CHAR,                N_("Move the cursor one character to the right")) \
  /* L10N: Help for Editor function: <forward-word> */ \
  _fmt(OP_EDITOR_FORWARD_WORD,                N_("Move the cursor to the end of the word")) \
  /* L10N: Help for Editor function: <history-down> */ \
  _fmt(OP_EDITOR_HISTORY_DOWN,                N_("Scroll down through the history list")) \
  /* L10N: Help for Editor function: <history-search> */ \
  _fmt(OP_EDITOR_HISTORY_SEARCH,              N_("Search through the history list")) \
  /* L10N: Help for Editor function: <history-up> */ \
  _fmt(OP_EDITOR_HISTORY_UP,                  N_("Scroll up through the history list")) \
  /* L10N: Help for Editor function: <kill-eol> */ \
  _fmt(OP_EDITOR_KILL_EOL,                    N_("Delete chars from cursor to end of line")) \
  /* L10N: Help for Editor function: <kill-eow> */ \
  _fmt(OP_EDITOR_KILL_EOW,                    N_("Delete chars from the cursor to the end of the word")) \
  /* L10N: Help for Editor function: <kill-line> */ \
  _fmt(OP_EDITOR_KILL_LINE,                   N_("Delete chars from cursor to beginning the line")) \
  /* L10N: Help for Editor function: <kill-whole-line> */ \
  _fmt(OP_EDITOR_KILL_WHOLE_LINE,             N_("Delete all chars on the line")) \
  /* L10N: Help for Editor function: <kill-word> */ \
  _fmt(OP_EDITOR_KILL_WORD,                   N_("Delete the word in front of the cursor")) \
  /* L10N: Help for Editor function: <mailbox-cycle> */ \
  _fmt(OP_EDITOR_MAILBOX_CYCLE,               N_("Cycle among incoming mailboxes")) \
  /* L10N: Help for Editor function: <quote-char> */ \
  _fmt(OP_EDITOR_QUOTE_CHAR,                  N_("Quote the next typed key")) \
  /* L10N: Help for Editor function: <transpose-chars> */ \
  _fmt(OP_EDITOR_TRANSPOSE_CHARS,             N_("Transpose character under cursor with previous")) \
  /* L10N: Help for Editor function: <upcase-word> */ \
  _fmt(OP_EDITOR_UPCASE_WORD,                 N_("Convert the word to upper case")) \
  /* L10N: Help for Index function: <edit-label> */ \
  _fmt(OP_EDIT_LABEL,                         N_("Add, change, or delete a message's label")) \
  /* L10N: Help for Index function: <edit-or-view-raw-message> */ \
  _fmt(OP_EDIT_OR_VIEW_RAW_MESSAGE,           N_("Edit the raw message if the mailbox is not read-only, otherwise view it")) \
  /* L10N: Help for Index, Index function: <edit> */ \
  _fmt(OP_EDIT_RAW_MESSAGE,                   N_("Edit the raw message (edit and edit-raw-message are synonyms)")) \
  /* L10N: Help for Generic function: <end-cond> */ \
  _fmt(OP_END_COND,                           N_("End of conditional execution (noop)")) \
  /* L10N: Help for Generic function: <enter-command> */ \
  _fmt(OP_ENTER_COMMAND,                      N_("Enter a neomuttrc command")) \
  /* L10N: Help for Browser function: <enter-mask> */ \
  _fmt(OP_ENTER_MASK,                         N_("Enter a file mask")) \
  /* L10N: Help for Generic function: <exit> */ \
  _fmt(OP_EXIT,                               N_("Exit this menu")) \
  /* L10N: Help for Generic function: <first-entry> */ \
  _fmt(OP_FIRST_ENTRY,                        N_("Move to the first entry")) \
  /* L10N: Help for Index function: <flag-message> */ \
  _fmt(OP_FLAG_MESSAGE,                       N_("Toggle a message's 'important' flag")) \
  /* L10N: Help for Attach, Index function: <followup-message> */ \
  _fmt(OP_FOLLOWUP,                           N_("Followup to newsgroup")) \
  /* L10N: Help for Attach, Index function: <forward-message> */ \
  _fmt(OP_FORWARD_MESSAGE,                    N_("Forward a message with comments")) \
  /* L10N: Help for Attach, Index function: <forward-to-group> */ \
  _fmt(OP_FORWARD_TO_GROUP,                   N_("Forward to newsgroup")) \
  /* L10N: Help for Generic function: <select-entry> */ \
  _fmt(OP_GENERIC_SELECT_ENTRY,               N_("Select the current entry")) \
  /* L10N: Help for Index function: <get-children> */ \
  _fmt(OP_GET_CHILDREN,                       N_("Get all children of the current message")) \
  /* L10N: Help for Index function: <get-message> */ \
  _fmt(OP_GET_MESSAGE,                        N_("Get message with Message-ID")) \
  /* L10N: Help for Index function: <get-parent> */ \
  _fmt(OP_GET_PARENT,                         N_("Get parent of the current message")) \
  /* L10N: Help for Browser function: <goto-parent> */ \
  _fmt(OP_GOTO_PARENT,                        N_("Go to parent directory")) \
  /* L10N: Help for Attach, Index function: <group-chat-reply> */ \
  _fmt(OP_GROUP_CHAT_REPLY,                   N_("Reply to all recipients preserving To/Cc")) \
  /* L10N: Help for Attach, Index function: <group-reply> */ \
  _fmt(OP_GROUP_REPLY,                        N_("Reply to all recipients")) \
  /* L10N: Help for Generic function: <half-down> */ \
  _fmt(OP_HALF_DOWN,                          N_("Scroll down 1/2 page")) \
  /* L10N: Help for Generic function: <half-up> */ \
  _fmt(OP_HALF_UP,                            N_("Scroll up 1/2 page")) \
  /* L10N: Help for Editor, Generic function: <help> */ \
  _fmt(OP_HELP,                               N_("This screen")) \
  /* L10N: Help for Generic function: <jump> */ \
  _fmt(OP_JUMP,                               N_("Jump to an index number")) \
  /* L10N: Help for Generic function: <last-entry> */ \
  _fmt(OP_LAST_ENTRY,                         N_("Move to the last entry")) \
  /* L10N: Help for Index function: <limit-current-thread> */ \
  _fmt(OP_LIMIT_CURRENT_THREAD,               N_("Limit view to current thread")) \
  /* L10N: Help for Index function: <list-action> */ \
  _fmt(OP_LIST_ACTION,                        N_("Perform mailing list action")) \
  /* L10N: Help for List function: <list-archive> */ \
  _fmt(OP_LIST_ARCHIVE,                       N_("Retrieve list archive information")) \
  /* L10N: Help for List function: <list-help> */ \
  _fmt(OP_LIST_HELP,                          N_("Retrieve list help")) \
  /* L10N: Help for List function: <list-owner> */ \
  _fmt(OP_LIST_OWNER,                         N_("Contact list owner")) \
  /* L10N: Help for List function: <list-post> */ \
  _fmt(OP_LIST_POST,                          N_("Post to mailing list")) \
  /* L10N: Help for Attach, Index function: <list-reply> */ \
  _fmt(OP_LIST_REPLY,                         N_("Reply to specified mailing list")) \
  /* L10N: Help for Attach, Index, List function: <list-subscribe> */ \
  _fmt(OP_LIST_SUBSCRIBE,                     N_("Subscribe to a mailing list")) \
  /* L10N: Help for Attach, Index, List function: <list-unsubscribe> */ \
  _fmt(OP_LIST_UNSUBSCRIBE,                   N_("Unsubscribe from a mailing list")) \
  /* L10N: Help for Browser function: <reload-active> */ \
  _fmt(OP_LOAD_ACTIVE,                        N_("Load list of all newsgroups from NNTP server")) \
  /* L10N: Help for executing a macro */ \
  _fmt(OP_MACRO,                              N_("Execute a macro")) \
  /* L10N: Help for Alias, Index, Query function: <mail> */ \
  _fmt(OP_MAIL,                               N_("Compose a new mail message")) \
  /* L10N: Help for Browser, Index function: <mailbox-list> */ \
  _fmt(OP_MAILBOX_LIST,                       N_("List mailboxes with new mail")) \
  /* L10N: Help for Index function: <break-thread> */ \
  _fmt(OP_MAIN_BREAK_THREAD,                  N_("Break the thread in two")) \
  /* L10N: Help for Index function: <change-folder> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER,                 N_("Open a different folder")) \
  /* L10N: Help for Index function: <change-folder-readonly> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER_READONLY,        N_("Open a different folder in read only mode")) \
  /* L10N: Help for Index function: <change-newsgroup> */ \
  _fmt(OP_MAIN_CHANGE_GROUP,                  N_("Open a different newsgroup")) \
  /* L10N: Help for Index function: <change-newsgroup-readonly> */ \
  _fmt(OP_MAIN_CHANGE_GROUP_READONLY,         N_("Open a different newsgroup in read only mode")) \
  /* L10N: Help for Index function: <clear-flag> */ \
  _fmt(OP_MAIN_CLEAR_FLAG,                    N_("Clear a status flag from a message")) \
  /* L10N: Help for Index function: <close-all-threads> */ \
  _fmt(OP_MAIN_CLOSE_ALL_THREADS,             N_("Collapse all threads")) \
  /* L10N: Help for Index function: <close-thread> */ \
  _fmt(OP_MAIN_CLOSE_THREAD,                  N_("Collapse current thread")) \
  /* L10N: Help for Index function: <collapse-all> */ \
  _fmt(OP_MAIN_COLLAPSE_ALL,                  N_("Collapse/uncollapse all threads")) \
  /* L10N: Help for Index function: <collapse-thread> */ \
  _fmt(OP_MAIN_COLLAPSE_THREAD,               N_("Collapse/uncollapse current thread")) \
  /* L10N: Help for Index function: <delete-pattern> */ \
  _fmt(OP_MAIN_DELETE_PATTERN,                N_("Delete non-hidden messages matching a pattern")) \
  /* L10N: Help for Index function: <fetch-mail> */ \
  _fmt(OP_MAIN_FETCH_MAIL,                    N_("Retrieve mail from POP server")) \
  /* L10N: Help for Index function: <imap-fetch-mail> */ \
  _fmt(OP_MAIN_IMAP_FETCH,                    N_("Force retrieval of mail from IMAP server")) \
  /* L10N: Help for Index function: <imap-logout-all> */ \
  _fmt(OP_MAIN_IMAP_LOGOUT_ALL,               N_("Logout from all IMAP servers")) \
  /* L10N: Help for Alias, Index, Query function: <limit> */ \
  _fmt(OP_MAIN_LIMIT,                         N_("Show only messages matching a pattern")) \
  /* L10N: Help for Index function: <link-threads> */ \
  _fmt(OP_MAIN_LINK_THREADS,                  N_("Link tagged message to the current one")) \
  /* L10N: Help for Index, Index function: <modify-labels> */ \
  _fmt(OP_MAIN_MODIFY_TAGS,                   N_("Modify (notmuch/imap) tags")) \
  /* L10N: Help for Index, Index function: <modify-labels-then-hide> */ \
  _fmt(OP_MAIN_MODIFY_TAGS_THEN_HIDE,         N_("Modify (notmuch/imap) tags and then hide message")) \
  /* L10N: Help for Index function: <next-new> */ \
  _fmt(OP_MAIN_NEXT_NEW,                      N_("Jump to the next new message")) \
  /* L10N: Help for Index function: <next-new-then-unread> */ \
  _fmt(OP_MAIN_NEXT_NEW_THEN_UNREAD,          N_("Jump to the next new or unread message")) \
  /* L10N: Help for Index function: <next-subthread> */ \
  _fmt(OP_MAIN_NEXT_SUBTHREAD,                N_("Jump to the next subthread")) \
  /* L10N: Help for Index function: <next-thread> */ \
  _fmt(OP_MAIN_NEXT_THREAD,                   N_("Jump to the next thread")) \
  /* L10N: Help for Index function: <next-undeleted> */ \
  _fmt(OP_MAIN_NEXT_UNDELETED,                N_("Move to the next undeleted message")) \
  /* L10N: Help for Index function: <next-unread> */ \
  _fmt(OP_MAIN_NEXT_UNREAD,                   N_("Jump to the next unread message")) \
  /* L10N: Help for Index function: <next-unread-mailbox> */ \
  _fmt(OP_MAIN_NEXT_UNREAD_MAILBOX,           N_("Open next mailbox with new mail")) \
  /* L10N: Help for Index function: <previous-unread-mailbox> */ \
  _fmt(OP_MAIN_PREV_UNREAD_MAILBOX,           N_("Open previous mailbox with new mail")) \
  /* L10N: Help for Index function: <open-all-threads> */ \
  _fmt(OP_MAIN_OPEN_ALL_THREADS,              N_("Uncollapse all threads")) \
  /* L10N: Help for Index function: <open-thread> */ \
  _fmt(OP_MAIN_OPEN_THREAD,                   N_("Uncollapse current thread")) \
  /* L10N: Help for Index function: <parent-message> */ \
  _fmt(OP_MAIN_PARENT_MESSAGE,                N_("Jump to parent message in thread")) \
  /* L10N: Help for Index function: <previous-new> */ \
  _fmt(OP_MAIN_PREV_NEW,                      N_("Jump to the previous new message")) \
  /* L10N: Help for Index function: <previous-new-then-unread> */ \
  _fmt(OP_MAIN_PREV_NEW_THEN_UNREAD,          N_("Jump to the previous new or unread message")) \
  /* L10N: Help for Index function: <previous-subthread> */ \
  _fmt(OP_MAIN_PREV_SUBTHREAD,                N_("Jump to previous subthread")) \
  /* L10N: Help for Index function: <previous-thread> */ \
  _fmt(OP_MAIN_PREV_THREAD,                   N_("Jump to previous thread")) \
  /* L10N: Help for Index function: <previous-undeleted> */ \
  _fmt(OP_MAIN_PREV_UNDELETED,                N_("Move to the previous undeleted message")) \
  /* L10N: Help for Index function: <previous-unread> */ \
  _fmt(OP_MAIN_PREV_UNREAD,                   N_("Jump to the previous unread message")) \
  /* L10N: Help for Index function: <quasi-delete> */ \
  _fmt(OP_MAIN_QUASI_DELETE,                  N_("Delete from NeoMutt, don't touch on disk")) \
  /* L10N: Help for Index function: <read-subthread> */ \
  _fmt(OP_MAIN_READ_SUBTHREAD,                N_("Mark the current subthread as read")) \
  /* L10N: Help for Index function: <read-thread> */ \
  _fmt(OP_MAIN_READ_THREAD,                   N_("Mark the current thread as read")) \
  /* L10N: Help for Index function: <root-message> */ \
  _fmt(OP_MAIN_ROOT_MESSAGE,                  N_("Jump to root message in thread")) \
  /* L10N: Help for Index function: <set-flag> */ \
  _fmt(OP_MAIN_SET_FLAG,                      N_("Set a status flag on a message")) \
  /* L10N: Help for Index function: <show-limit> */ \
  _fmt(OP_MAIN_SHOW_LIMIT,                    N_("Show currently active limit pattern")) \
  /* L10N: Help for Index function: <sync-mailbox> */ \
  _fmt(OP_MAIN_SYNC_FOLDER,                   N_("Save changes to mailbox")) \
  /* L10N: Help for Alias, Index, Query function: <tag-pattern> */ \
  _fmt(OP_MAIN_TAG_PATTERN,                   N_("Tag non-hidden messages matching a pattern")) \
  /* L10N: Help for Index function: <undelete-pattern> */ \
  _fmt(OP_MAIN_UNDELETE_PATTERN,              N_("Undelete non-hidden messages matching a pattern")) \
  /* L10N: Help for Alias, Index, Query function: <untag-pattern> */ \
  _fmt(OP_MAIN_UNTAG_PATTERN,                 N_("Untag non-hidden messages matching a pattern")) \
  /* L10N: Help for Index function: <mark-message> */ \
  _fmt(OP_MARK_MSG,                           N_("Create a hotkey macro for the current message")) \
  /* L10N: Help for Generic function: <middle-page> */ \
  _fmt(OP_MIDDLE_PAGE,                        N_("Move to the middle of the page")) \
  /* L10N: Help for Generic function: <next-entry> */ \
  _fmt(OP_NEXT_ENTRY,                         N_("Move to the next entry")) \
  /* L10N: Help for Generic function: <next-line> */ \
  _fmt(OP_NEXT_LINE,                          N_("Scroll down one line")) \
  /* L10N: Help for Generic function: <next-page> */ \
  _fmt(OP_NEXT_PAGE,                          N_("Move to the next page")) \
  /* L10N: Help for Generic function: <pager-bottom> */ \
  _fmt(OP_PAGER_BOTTOM,                       N_("Jump to the bottom of the message")) \
  /* L10N: Help for Pager function: <toggle-quoted> */ \
  _fmt(OP_PAGER_HIDE_QUOTED,                  N_("Toggle display of quoted text")) \
  /* L10N: Help for Pager function: <skip-headers> */ \
  _fmt(OP_PAGER_SKIP_HEADERS,                 N_("Jump to first line after headers")) \
  /* L10N: Help for Pager function: <skip-quoted> */ \
  _fmt(OP_PAGER_SKIP_QUOTED,                  N_("Skip beyond quoted text")) \
  /* L10N: Help for Generic function: <pager-top> */ \
  _fmt(OP_PAGER_TOP,                          N_("Jump to the top of the message")) \
  /* L10N: Help for Attach, Compose, Index, Attach, Compose, Index function: <pipe-entry> */ \
  _fmt(OP_PIPE,                               N_("Pipe message/attachment to a shell command")) \
  /* L10N: Help for Index function: <post-message> */ \
  _fmt(OP_POST,                               N_("Post message to newsgroup")) \
  /* L10N: Help for Generic function: <previous-entry> */ \
  _fmt(OP_PREV_ENTRY,                         N_("Move to the previous entry")) \
  /* L10N: Help for Generic function: <previous-line> */ \
  _fmt(OP_PREV_LINE,                          N_("Scroll up one line")) \
  /* L10N: Help for Generic function: <previous-page> */ \
  _fmt(OP_PREV_PAGE,                          N_("Move to the previous page")) \
  /* L10N: Help for Index function: <print-message> */ \
  _fmt(OP_PRINT,                              N_("Print the current entry")) \
  /* L10N: Help for Index function: <purge-message> */ \
  _fmt(OP_PURGE_MESSAGE,                      N_("Delete the current entry, bypassing the trash folder")) \
  /* L10N: Help for Index function: <purge-thread> */ \
  _fmt(OP_PURGE_THREAD,                       N_("Delete the current thread, bypassing the trash folder")) \
  /* L10N: Help for Index, Query function: <query> */ \
  _fmt(OP_QUERY,                              N_("Query external program for addresses")) \
  /* L10N: Help for Query function: <query-append> */ \
  _fmt(OP_QUERY_APPEND,                       N_("Append new query results to current results")) \
  /* L10N: Help for Generic function: <quit> */ \
  _fmt(OP_QUIT,                               N_("Save changes to mailbox and quit")) \
  /* L10N: Help for Index function: <recall-message> */ \
  _fmt(OP_RECALL_MESSAGE,                     N_("Recall a postponed message")) \
  /* L10N: Help for Index function: <reconstruct-thread> */ \
  _fmt(OP_RECONSTRUCT_THREAD,                 N_("Reconstruct thread containing current message")) \
  /* L10N: Help for Editor, Generic function: <redraw-screen> */ \
  _fmt(OP_REDRAW,                             N_("Clear and redraw the screen")) \
  /* L10N: Help for Browser function: <rename-mailbox> */ \
  _fmt(OP_RENAME_MAILBOX,                     N_("Rename the current mailbox (IMAP only)")) \
  /* L10N: Help for Attach, Index function: <reply> */ \
  _fmt(OP_REPLY,                              N_("Reply to a message")) \
  /* L10N: Help for Attach, Index function: <resend-message> */ \
  _fmt(OP_RESEND,                             N_("Use the current message as a template for a new one")) \
  /* L10N: Help for Index function: <save-message> */ \
  _fmt(OP_SAVE,                               N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help for Generic function: <search> */ \
  _fmt(OP_SEARCH,                             N_("Search for a regular expression")) \
  /* L10N: Help for Generic function: <search-next> */ \
  _fmt(OP_SEARCH_NEXT,                        N_("Search for next match")) \
  /* L10N: Help for Generic function: <search-opposite> */ \
  _fmt(OP_SEARCH_OPPOSITE,                    N_("Search for next match in opposite direction")) \
  /* L10N: Help for Generic function: <search-reverse> */ \
  _fmt(OP_SEARCH_REVERSE,                     N_("Search backwards for a regular expression")) \
  /* L10N: Help for Pager function: <search-toggle> */ \
  _fmt(OP_SEARCH_TOGGLE,                      N_("Toggle search pattern coloring")) \
  /* L10N: Help for Generic function: <shell-escape> */ \
  _fmt(OP_SHELL_ESCAPE,                       N_("Invoke a command in a subshell")) \
  /* L10N: Help for Generic function: <show-log-messages> */ \
  _fmt(OP_SHOW_LOG_MESSAGES,                  N_("Show log (and debug) messages")) \
  /* L10N: Help for Browser, Query, Alias, Index function: <sort> */ \
  _fmt(OP_SORT,                               N_("Sort messages")) \
  /* L10N: Help for Alias, Browser, Index, Query function: <sort-alias-reverse> */ \
  _fmt(OP_SORT_REVERSE,                       N_("Sort messages in reverse order")) \
  /* L10N: Help for Browser function: <subscribe-pattern> */ \
  _fmt(OP_SUBSCRIBE_PATTERN,                  N_("Subscribe to newsgroups matching a pattern")) \
  /* L10N: Help for Generic, Postpone function: <tag-entry> */ \
  _fmt(OP_TAG,                                N_("Tag the current entry")) \
  /* L10N: Help for Generic function: <tag-prefix> */ \
  _fmt(OP_TAG_PREFIX,                         N_("Apply next function to tagged messages")) \
  /* L10N: Help for Generic function: <tag-prefix-cond> */ \
  _fmt(OP_TAG_PREFIX_COND,                    N_("Apply next function ONLY to tagged messages")) \
  /* L10N: Help for Index function: <tag-subthread> */ \
  _fmt(OP_TAG_SUBTHREAD,                      N_("Tag the current subthread")) \
  /* L10N: Help for Index function: <tag-thread> */ \
  _fmt(OP_TAG_THREAD,                         N_("Tag the current thread")) \
  /* L10N: Help for Browser function: <toggle-mailboxes> */ \
  _fmt(OP_TOGGLE_MAILBOXES,                   N_("Toggle whether to browse mailboxes or all files")) \
  /* L10N: Help for Index function: <toggle-new> */ \
  _fmt(OP_TOGGLE_NEW,                         N_("Toggle a message's 'new' flag")) \
  /* L10N: Help for Index function: <toggle-read> */ \
  _fmt(OP_TOGGLE_READ,                        N_("Toggle view of read messages")) \
  /* L10N: Help for Index function: <toggle-write> */ \
  _fmt(OP_TOGGLE_WRITE,                       N_("Toggle whether the mailbox will be rewritten")) \
  /* L10N: Help for Generic function: <top-page> */ \
  _fmt(OP_TOP_PAGE,                           N_("Move to the top of the page")) \
  /* L10N: Help for Browser function: <uncatchup> */ \
  _fmt(OP_UNCATCHUP,                          N_("Mark all articles in newsgroup as unread")) \
  /* L10N: Help for Alias, Postpone, Index function: <undelete-entry> */ \
  _fmt(OP_UNDELETE,                           N_("Undelete the current entry")) \
  /* L10N: Help for Index function: <undelete-subthread> */ \
  _fmt(OP_UNDELETE_SUBTHREAD,                 N_("Undelete all messages in subthread")) \
  /* L10N: Help for Index function: <undelete-thread> */ \
  _fmt(OP_UNDELETE_THREAD,                    N_("Undelete all messages in thread")) \
  /* L10N: Help for Browser function: <unsubscribe-pattern> */ \
  _fmt(OP_UNSUBSCRIBE_PATTERN,                N_("Unsubscribe from newsgroups matching a pattern")) \
  /* L10N: Help for Generic function: <show-version> */ \
  _fmt(OP_VERSION,                            N_("Show the NeoMutt version number and date")) \
  /* L10N: Help for Index function: <view-attachments> */ \
  _fmt(OP_VIEW_ATTACHMENTS,                   N_("Show MIME attachments")) \
  /* L10N: Help for Index function: <view-raw-message> */ \
  _fmt(OP_VIEW_RAW_MESSAGE,                   N_("Show the raw message")) \
  /* L10N: Help for Generic function: <what-key> */ \
  _fmt(OP_WHAT_KEY,                           N_("Display the keycode for a key press")) \

#define OPS_CRYPT(_fmt) \
  /* L10N: Help for Index function: <decrypt-copy> */ \
  _fmt(OP_DECRYPT_COPY,                       N_("Make decrypted copy")) \
  /* L10N: Help for Index function: <decrypt-save> */ \
  _fmt(OP_DECRYPT_SAVE,                       N_("Make decrypted copy and delete")) \
  /* L10N: Help for Attach, Index function: <extract-keys> */ \
  _fmt(OP_EXTRACT_KEYS,                       N_("Extract supported public keys")) \
  /* L10N: Help for Attach, Compose, Index function: <forget-passphrase> */ \
  _fmt(OP_FORGET_PASSPHRASE,                  N_("Wipe passphrases from memory")) \

#define OPS_ENVELOPE(_fmt) \
  /* L10N: Help for Compose function: <edit-bcc> */ \
  _fmt(OP_ENVELOPE_EDIT_BCC,                  N_("Edit the BCC list")) \
  /* L10N: Help for Compose function: <edit-cc> */ \
  _fmt(OP_ENVELOPE_EDIT_CC,                   N_("Edit the CC list")) \
  /* L10N: Help for Compose function: <edit-fcc> */ \
  _fmt(OP_ENVELOPE_EDIT_FCC,                  N_("Enter a file to save a copy of this message in")) \
  /* L10N: Help for Compose function: <edit-followup-to> */ \
  _fmt(OP_ENVELOPE_EDIT_FOLLOWUP_TO,          N_("Edit the Followup-To field")) \
  /* L10N: Help for Compose function: <edit-from> */ \
  _fmt(OP_ENVELOPE_EDIT_FROM,                 N_("Edit the from field")) \
  /* L10N: Help for Compose function: <edit-headers> */ \
  _fmt(OP_ENVELOPE_EDIT_HEADERS,              N_("Edit the message with headers")) \
  /* L10N: Help for Compose function: <edit-newsgroups> */ \
  _fmt(OP_ENVELOPE_EDIT_NEWSGROUPS,           N_("Edit the newsgroups list")) \
  /* L10N: Help for Compose function: <edit-reply-to> */ \
  _fmt(OP_ENVELOPE_EDIT_REPLY_TO,             N_("Edit the Reply-To field")) \
  /* L10N: Help for Compose function: <edit-subject> */ \
  _fmt(OP_ENVELOPE_EDIT_SUBJECT,              N_("Edit the subject of this message")) \
  /* L10N: Help for Compose function: <edit-to> */ \
  _fmt(OP_ENVELOPE_EDIT_TO,                   N_("Edit the TO list")) \
  /* L10N: Help for Compose function: <edit-x-comment-to> */ \
  _fmt(OP_ENVELOPE_EDIT_X_COMMENT_TO,         N_("Edit the X-Comment-To field")) \

#ifdef USE_NOTMUCH
#define OPS_NOTMUCH(_fmt) \
  /* L10N: Help for Index function: <change-vfolder> */ \
  _fmt(OP_MAIN_CHANGE_VFOLDER,                N_("Open a different virtual folder")) \
  /* L10N: Help for Index function: <entire-thread> */ \
  _fmt(OP_MAIN_ENTIRE_THREAD,                 N_("Read entire thread of the current message")) \
  /* L10N: Help for Index function: <vfolder-from-query> */ \
  _fmt(OP_MAIN_VFOLDER_FROM_QUERY,            N_("Generate virtual folder from query")) \
  /* L10N: Help for Index function: <vfolder-from-query-readonly> */ \
  _fmt(OP_MAIN_VFOLDER_FROM_QUERY_READONLY,   N_("Generate a read-only virtual folder from query")) \
  /* L10N: Help for Index function: <vfolder-window-backward> */ \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_BACKWARD,     N_("Shifts virtual folder time window backwards")) \
  /* L10N: Help for Index function: <vfolder-window-forward> */ \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_FORWARD,      N_("Shifts virtual folder time window forwards")) \
  /* L10N: Help for Index function: <vfolder-window-reset> */ \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_RESET,        N_("Resets virtual folder time window to the present"))
#else
#define OPS_NOTMUCH(_)
#endif

#define OPS_PGP(_fmt) \
  /* L10N: Help for Compose function: <attach-key> */ \
  _fmt(OP_ATTACH_ATTACH_KEY,                  N_("Attach a PGP public key")) \
  /* L10N: Help for Attach, Index function: <check-traditional-pgp> */ \
  _fmt(OP_CHECK_TRADITIONAL,                  N_("Check for classic PGP")) \
  /* L10N: Help for Compose function: <pgp-menu> */ \
  _fmt(OP_COMPOSE_PGP_MENU,                   N_("Show PGP options")) \
  /* L10N: Help for Index function: <mail-key> */ \
  _fmt(OP_MAIL_KEY,                           N_("Mail a PGP public key")) \
  /* L10N: Help for Pgp, Smime function: <verify-key> */ \
  _fmt(OP_VERIFY_KEY,                         N_("Verify a public key")) \
  /* L10N: Help for Pgp, Smime function: <view-name> */ \
  _fmt(OP_VIEW_ID,                            N_("View the key's user id")) \

#define OPS_SIDEBAR(_fmt) \
  /* L10N: Help for Sidebar function: <sidebar-first> */ \
  _fmt(OP_SIDEBAR_FIRST,                      N_("Move the highlight to the first mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-last> */ \
  _fmt(OP_SIDEBAR_LAST,                       N_("Move the highlight to the last mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-next> */ \
  _fmt(OP_SIDEBAR_NEXT,                       N_("Move the highlight to next mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-next-new> */ \
  _fmt(OP_SIDEBAR_NEXT_NEW,                   N_("Move the highlight to next mailbox with new mail")) \
  /* L10N: Help for Sidebar function: <sidebar-open> */ \
  _fmt(OP_SIDEBAR_OPEN,                       N_("Open highlighted mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-page-down> */ \
  _fmt(OP_SIDEBAR_PAGE_DOWN,                  N_("Scroll the sidebar down 1 page")) \
  /* L10N: Help for Sidebar function: <sidebar-page-up> */ \
  _fmt(OP_SIDEBAR_PAGE_UP,                    N_("Scroll the sidebar up 1 page")) \
  /* L10N: Help for Sidebar function: <sidebar-prev> */ \
  _fmt(OP_SIDEBAR_PREV,                       N_("Move the highlight to previous mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-prev-new> */ \
  _fmt(OP_SIDEBAR_PREV_NEW,                   N_("Move the highlight to previous mailbox with new mail")) \
  /* L10N: Help for Sidebar function: <sidebar-toggle-virtual> */ \
  _fmt(OP_SIDEBAR_TOGGLE_VIRTUAL,             N_("Toggle between mailboxes and virtual mailboxes")) \
  /* L10N: Help for Sidebar function: <sidebar-toggle-visible> */ \
  _fmt(OP_SIDEBAR_TOGGLE_VISIBLE,             N_("Make the sidebar (in)visible")) \
  /* L10N: Help for Sidebar function: <sidebar-abort-search> */ \
  _fmt(OP_SIDEBAR_ABORT_SEARCH,               N_("Close the sidebar search")) \
  /* L10N: Help for Sidebar function: <sidebar-start-search> */ \
  _fmt(OP_SIDEBAR_START_SEARCH,               N_("Fuzzy search the sidebar")) \

#define OPS_SMIME(_fmt) \
  /* L10N: Help for Compose function: <smime-menu> */ \
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
