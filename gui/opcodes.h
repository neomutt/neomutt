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
#define OPS_DIALOG(_fmt) \
  /* L10N: Help for Generic function: <exit> */ \
  _fmt(OP_EXIT,                               N_("Exit a menu")) \
  /* L10N: Help for Generic function: <quit> */ \
  _fmt(OP_QUIT,                               N_("Save changes and quit")) \

#define OPS_ENTRY(_fmt) \
  /* L10N: Help for Generic function: <pipe-entry> */ \
  _fmt(OP_PIPE_ENTRY,                         N_("Pipe the current entry to a shell command")) \
  /* L10N: Help for Generic function: <print-entry> */ \
  _fmt(OP_PRINT_ENTRY,                        N_("Print the current entry")) \

#define OPS_GLOBAL(_fmt) \
  /* L10N: Help for Generic function: <check-stats> */ \
  _fmt(OP_CHECK_STATS,                        N_("Calculate message statistics for all mailboxes")) \
  /* L10N: Help for Generic function: <display-help> */ \
  _fmt(OP_DISPLAY_HELP,                       N_("Display the help screen")) \
  /* L10N: Help for Generic function: <display-log> */ \
  _fmt(OP_DISPLAY_LOG,                        N_("Display log and debug messages")) \
  /* L10N: Help for Generic function: <forget-passphrase> */ \
  _fmt(OP_FORGET_PASSPHRASE,                  N_("Wipe passphrases from memory")) \
  /* L10N: Help for Generic function: <redraw-screen> */ \
  _fmt(OP_REDRAW_SCREEN,                      N_("Clear and redraw the screen")) \
  /* L10N: Help for Generic function: <run-command> */ \
  _fmt(OP_RUN_COMMAND,                        N_("Execute a NeoMutt command")) \
  /* L10N: Help for Generic function: <run-shell-command> */ \
  _fmt(OP_RUN_SHELL_COMMAND,                  N_("Execute external command in a subshell")) \
  /* L10N: Help for Generic function: <show-version> */ \
  _fmt(OP_SHOW_VERSION,                       N_("Show the NeoMutt version number and date")) \
  /* L10N: Help for Generic function: <view-keycodes> */ \
  _fmt(OP_VIEW_KEYCODES,                      N_("Show the keycodes for key presses")) \

#define OPS_SCROLL(_fmt) \
  /* L10N: Help for Generic function: <scroll-end> */ \
  _fmt(OP_SCROLL_END,                         N_("Scroll to the bottom")) \
  /* L10N: Help for Generic function: <scroll-half-down> */ \
  _fmt(OP_SCROLL_HALF_DOWN,                   N_("Scroll down half a page")) \
  /* L10N: Help for Generic function: <scroll-half-up> */ \
  _fmt(OP_SCROLL_HALF_UP,                     N_("Scroll up half a page")) \
  /* L10N: Help for Generic function: <scroll-home> */ \
  _fmt(OP_SCROLL_HOME,                        N_("Scroll to the top")) \
  /* L10N: Help for Generic function: <scroll-line-down> */ \
  _fmt(OP_SCROLL_LINE_DOWN,                   N_("Scroll down one line")) \
  /* L10N: Help for Generic function: <scroll-line-up> */ \
  _fmt(OP_SCROLL_LINE_UP,                     N_("Scroll up one line")) \
  /* L10N: Help for Generic function: <scroll-page-down> */ \
  _fmt(OP_SCROLL_PAGE_DOWN,                   N_("Scroll down one page")) \
  /* L10N: Help for Generic function: <scroll-page-up> */ \
  _fmt(OP_SCROLL_PAGE_UP,                     N_("Scroll up one page")) \
  /* L10N: Help for Generic function: <scroll-selection-to-bottom> */ \
  _fmt(OP_SCROLL_SELECTION_TO_BOTTOM,         N_("Scroll the selection to the bottom of the page")) \
  /* L10N: Help for Generic function: <scroll-selection-to-middle> */ \
  _fmt(OP_SCROLL_SELECTION_TO_MIDDLE,         N_("Scroll the selection to the middle of the page")) \
  /* L10N: Help for Generic function: <scroll-selection-to-top> */ \
  _fmt(OP_SCROLL_SELECTION_TO_TOP,            N_("Scroll the selection to the top of the page")) \

#define OPS_SEARCH(_fmt) \
  /* L10N: Help for Generic function: <search-backward> */ \
  _fmt(OP_SEARCH_BACKWARD,                    N_("Search backward for a regular expression")) \
  /* L10N: Help for Generic function: <search-forward> */ \
  _fmt(OP_SEARCH_FORWARD,                     N_("Search forward for a regular expression")) \
  /* L10N: Help for Generic function: <search-next> */ \
  _fmt(OP_SEARCH_NEXT,                        N_("Search for next match")) \
  /* L10N: Help for Generic function: <search-previous> */ \
  _fmt(OP_SEARCH_PREVIOUS,                    N_("Search for next match in opposite direction")) \

#define OPS_SELECT(_fmt) \
  /* L10N: Help for Generic function: <activate-entry> */ \
  _fmt(OP_ACTIVATE_ENTRY,                     N_("Activate the current entry")) \
  /* L10N: Help for Generic function: <select-entry-by-number> */ \
  _fmt(OP_SELECT_ENTRY_BY_NUMBER,             N_("Select an entry by its index number")) \
  /* L10N: Help for Generic function: <select-first-entry> */ \
  _fmt(OP_SELECT_FIRST_ENTRY,                 N_("Select the first entry")) \
  /* L10N: Help for Generic function: <select-last-entry> */ \
  _fmt(OP_SELECT_LAST_ENTRY,                  N_("Select the last entry")) \
  /* L10N: Help for Generic function: <select-next-entry> */ \
  _fmt(OP_SELECT_NEXT_ENTRY,                  N_("Select the next entry")) \
  /* L10N: Help for Generic function: <select-page-bottom> */ \
  _fmt(OP_SELECT_PAGE_BOTTOM,                 N_("Select the entry at the bottom of the page")) \
  /* L10N: Help for Generic function: <select-page-middle> */ \
  _fmt(OP_SELECT_PAGE_MIDDLE,                 N_("Select the entry in the middle of the page")) \
  /* L10N: Help for Generic function: <select-page-top> */ \
  _fmt(OP_SELECT_PAGE_TOP,                    N_("Select the entry at the top of the page")) \
  /* L10N: Help for Generic function: <select-previous-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_ENTRY,              N_("Select the previous entry")) \

#define OPS_TAG(_fmt) \
  /* L10N: Help for Generic function: <apply-to-tagged> */ \
  _fmt(OP_APPLY_TO_TAGGED,                    N_("Apply the next function to tagged entries")) \
  /* L10N: Help for Generic function: <apply-to-tagged-begin> */ \
  _fmt(OP_APPLY_TO_TAGGED_BEGIN,              N_("Skip to <apply-to-tagged-end> if nothing is tagged")) \
  /* L10N: Help for Generic function: <apply-to-tagged-end> */ \
  _fmt(OP_APPLY_TO_TAGGED_END,                N_("End marker for <apply-to-tagged-begin>")) \
  /* L10N: Help for Generic function: <tag-pattern> */ \
  _fmt(OP_TAG_PATTERN,                   N_("Tag entries matching a pattern")) \
  /* L10N: Help for Generic function: <toggle-tag> */ \
  _fmt(OP_TOGGLE_TAG,                         N_("Tag/untag the current entry")) \
  /* L10N: Help for Generic function: <untag-pattern> */ \
  _fmt(OP_UNTAG_PATTERN,                 N_("Untag entries matching a pattern")) \

#define OPS_TREE(_fmt) \
  /* L10N: Help for Generic function: <fold-all-trees> */ \
  _fmt(OP_FOLD_ALL_TREES,                     N_("Collapse all trees")) \
  /* L10N: Help for Generic function: <fold-tree> */ \
  _fmt(OP_FOLD_TREE,                          N_("Collapse current tree")) \
  /* L10N: Help for Generic function: <select-next-subtree> */ \
  _fmt(OP_SELECT_NEXT_SUBTREE,                N_("Select the next subtree")) \
  /* L10N: Help for Generic function: <select-next-tree> */ \
  _fmt(OP_SELECT_NEXT_TREE,                   N_("Select the next tree")) \
  /* L10N: Help for Generic function: <select-previous-subtree> */ \
  _fmt(OP_SELECT_PREVIOUS_SUBTREE,            N_("Select the previous subtree")) \
  /* L10N: Help for Generic function: <select-previous-tree> */ \
  _fmt(OP_SELECT_PREVIOUS_TREE,               N_("Select the previous tree")) \
  /* L10N: Help for Generic function: <select-tree-parent-entry> */ \
  _fmt(OP_SELECT_TREE_PARENT_ENTRY,           N_("Select the parent entry in the tree")) \
  /* L10N: Help for Generic function: <select-tree-root-entry> */ \
  _fmt(OP_SELECT_TREE_ROOT_ENTRY,             N_("Select the root entry of the tree")) \
  /* L10N: Help for Generic function: <toggle-all-trees> */ \
  _fmt(OP_TOGGLE_ALL_TREES,                   N_("Collapse/expand all trees")) \
  /* L10N: Help for Generic function: <toggle-tag-subtree> */ \
  _fmt(OP_TOGGLE_TAG_SUBTREE,                 N_("Tag/untag the current subtree")) \
  /* L10N: Help for Generic function: <toggle-tag-tree> */ \
  _fmt(OP_TOGGLE_TAG_TREE,                    N_("Tag/untag the current tree")) \
  /* L10N: Help for Generic function: <toggle-tree> */ \
  _fmt(OP_TOGGLE_TREE,                        N_("Collapse/expand current tree")) \
  /* L10N: Help for Generic function: <unfold-all-trees> */ \
  _fmt(OP_UNFOLD_ALL_TREES,                   N_("Expand all trees")) \
  /* L10N: Help for Generic function: <unfold-tree> */ \
  _fmt(OP_UNFOLD_TREE,                        N_("Expand current tree")) \

#define OPS_VIEW(_fmt) \
  /* L10N: Help for Generic function: <limit-entries> */ \
  _fmt(OP_LIMIT_ENTRIES,                      N_("Show only entries matching a pattern")) \
  /* L10N: Help for Generic function: <show-limit> */ \
  _fmt(OP_SHOW_LIMIT,                         N_("Show currently active limit pattern")) \
  /* L10N: Help for Generic function: <sort-entries> */ \
  _fmt(OP_SORT_ENTRIES,                       N_("Sort entries")) \
  /* L10N: Help for Generic function: <sort-entries-reverse> */ \
  _fmt(OP_SORT_ENTRIES_REVERSE,               N_("Sort entries in reverse order")) \

#define OPS_EDITOR(_fmt) \
  /* L10N: Help for Editor function: <backspace> */ \
  _fmt(OP_EDITOR_BACKSPACE,                   N_("Delete the char in front of the cursor")) \
  /* L10N: Help for Editor function: <backward-char> */ \
  _fmt(OP_EDITOR_BACKWARD_CHAR,               N_("Move the cursor one character to the left")) \
  /* L10N: Help for Editor function: <backward-word> */ \
  _fmt(OP_EDITOR_BACKWARD_WORD,               N_("Move the cursor to the beginning of the word")) \
  /* L10N: Help for Editor function: <bol> */ \
  _fmt(OP_EDITOR_BOL,                         N_("Move the cursor to the beginning of the line")) \
  /* L10N: Help for Editor function: <capitalize-word> */ \
  _fmt(OP_EDITOR_CAPITALIZE_WORD,             N_("Capitalize the word in front of the cursor")) \
  /* L10N: Help for Editor function: <complete> */ \
  _fmt(OP_EDITOR_COMPLETE,                    N_("Auto-complete (general): aliases, mailboxes, files/dir")) \
  /* L10N: Help for Editor function: <complete-mailbox> */ \
  _fmt(OP_EDITOR_COMPLETE_MAILBOX,            N_("Auto-complete from mailboxes with new mail")) \
  /* L10N: Help for Editor function: <complete-query> */ \
  _fmt(OP_EDITOR_COMPLETE_QUERY,              N_("Auto-complete from alias query (external command)")) \
  /* L10N: Help for Editor function: <delete-char> */ \
  _fmt(OP_EDITOR_DELETE_CHAR,                 N_("Delete the char under the cursor")) \
  /* L10N: Help for Editor function: <downcase-word> */ \
  _fmt(OP_EDITOR_DOWNCASE_WORD,               N_("Convert the word in front of the cursor to lower case")) \
  /* L10N: Help for Editor function: <eol> */ \
  _fmt(OP_EDITOR_EOL,                         N_("Move the cursor to the end of the line")) \
  /* L10N: Help for Editor function: <forward-char> */ \
  _fmt(OP_EDITOR_FORWARD_CHAR,                N_("Move the cursor one character to the right")) \
  /* L10N: Help for Editor function: <forward-word> */ \
  _fmt(OP_EDITOR_FORWARD_WORD,                N_("Move the cursor to the end of the word")) \
  /* L10N: Help for Editor function: <kill-eol> */ \
  _fmt(OP_EDITOR_KILL_EOL,                    N_("Delete chars from the cursor to end of line")) \
  /* L10N: Help for Editor function: <kill-eow> */ \
  _fmt(OP_EDITOR_KILL_EOW,                    N_("Delete chars from the cursor to the end of the word")) \
  /* L10N: Help for Editor function: <kill-line> */ \
  _fmt(OP_EDITOR_KILL_LINE,                   N_("Delete chars from the cursor to beginning the line")) \
  /* L10N: Help for Editor function: <kill-whole-line> */ \
  _fmt(OP_EDITOR_KILL_WHOLE_LINE,             N_("Delete all chars on the line")) \
  /* L10N: Help for Editor function: <kill-word> */ \
  _fmt(OP_EDITOR_KILL_WORD,                   N_("Delete the word in front of the cursor")) \
  /* L10N: Help for Editor function: <quote-char> */ \
  _fmt(OP_EDITOR_QUOTE_CHAR,                  N_("Quote the next typed key")) \
  /* L10N: Help for Editor function: <transpose-chars> */ \
  _fmt(OP_EDITOR_TRANSPOSE_CHARS,             N_("Transpose character under cursor with the previous one")) \
  /* L10N: Help for Editor function: <upcase-word> */ \
  _fmt(OP_EDITOR_UPCASE_WORD,                 N_("Convert the word in front of the cursor to upper case")) \
  /* L10N: Help for Editor function: <history-search> */ \
  _fmt(OP_HISTORY_SEARCH,                     N_("View the history list")) \
  /* L10N: Help for Editor function: <history-select-next-entry> */ \
  _fmt(OP_HISTORY_SELECT_NEXT_ENTRY,          N_("Select next newer entry in history list")) \
  /* L10N: Help for Editor function: <history-select-previous-entry> */ \
  _fmt(OP_HISTORY_SELECT_PREVIOUS_ENTRY,      N_("Select next older entry in history list")) \

#define OPS_ATTACH(_fmt) \
  /* L10N: Help for Compose function: <attach-file> */ \
  _fmt(OP_ATTACH_ATTACH_FILE,                 N_("Attach files to this message")) \
  /* L10N: Help for Compose function: <attach-message> */ \
  _fmt(OP_ATTACH_ATTACH_MESSAGE,              N_("Attach messages to this message")) \
  /* L10N: Help for Compose function: <attach-news-message> */ \
  _fmt(OP_ATTACH_ATTACH_NEWS_MESSAGE,         N_("Attach news articles to this message")) \
  /* L10N: Help for Compose function: <attach-new-mime> */ \
  _fmt(OP_ATTACH_ATTACH_NEW_MIME,             N_("Compose new attachment using mailcap entry")) \
  /* L10N: Help for Attach function: <collapse-parts> */ \
  _fmt(OP_ATTACH_COLLAPSE,                    N_("Toggle display of subparts")) \
  /* L10N: Help for Attach function: <delete-entry> */ \
  _fmt(OP_ATTACH_DELETE,                      N_("Delete the current entry")) \
  /* L10N: Help for Compose function: <detach-file> */ \
  _fmt(OP_ATTACH_DETACH,                      N_("Delete the current entry")) \
  /* L10N: Help for Attach, Compose function: <display-attachment-default> */ \
  _fmt(OP_ATTACH_DISPLAY_ATTACHMENT_DEFAULT,  N_("View attachment using mailcap entry if necessary")) \
  /* L10N: Help for Attach, Compose function: <display-attachment-mailcap> */ \
  _fmt(OP_ATTACH_DISPLAY_ATTACHMENT_MAILCAP,  N_("Force viewing of attachment using mailcap")) \
  /* L10N: Help for Attach, Compose function: <display-attachment-pager> */ \
  _fmt(OP_ATTACH_DISPLAY_ATTACHMENT_PAGER,    N_("View attachment in pager using copiousoutput mailcap")) \
  /* L10N: Help for Attach, Compose function: <display-attachment-text> */ \
  _fmt(OP_ATTACH_DISPLAY_ATTACHMENT_TEXT,     N_("View attachment as text")) \
  /* L10N: Help for Compose function: <edit-attachment-name> */ \
  _fmt(OP_ATTACH_EDIT_ATTACHMENT_NAME,        N_("Send attachment with a different name")) \
  /* L10N: Help for Compose function: <edit-content-id> */ \
  _fmt(OP_ATTACH_EDIT_CONTENT_ID,             N_("Edit the 'Content-ID' of the attachment")) \
  /* L10N: Help for Attach, Compose, Index function: <edit-content-type> */ \
  _fmt(OP_ATTACH_EDIT_CONTENT_TYPE,           N_("Edit attachment content type")) \
  /* L10N: Help for Compose function: <edit-description> */ \
  _fmt(OP_ATTACH_EDIT_DESCRIPTION,            N_("Edit attachment description")) \
  /* L10N: Help for Compose function: <edit-encoding> */ \
  _fmt(OP_ATTACH_EDIT_ENCODING,               N_("Edit attachment transfer-encoding")) \
  /* L10N: Help for Compose function: <edit-language> */ \
  _fmt(OP_ATTACH_EDIT_LANGUAGE,               N_("Edit the 'Content-Language' of the attachment")) \
  /* L10N: Help for Compose function: <edit-mime> */ \
  _fmt(OP_ATTACH_EDIT_MIME,                   N_("Edit attachment using mailcap entry")) \
  /* L10N: Help for Compose function: <filter-attachment> */ \
  _fmt(OP_ATTACH_FILTER_ATTACHMENT,           N_("Filter attachment through a shell command")) \
  /* L10N: Help for Compose function: <get-attachment> */ \
  _fmt(OP_ATTACH_GET_ATTACHMENT,              N_("Get a temporary copy of an attachment")) \
  /* L10N: Help for Compose function: <group-alternatives> */ \
  _fmt(OP_ATTACH_GROUP_ALTS,                  N_("Group tagged attachments as 'multipart/alternative'")) \
  /* L10N: Help for Compose function: <group-multilingual> */ \
  _fmt(OP_ATTACH_GROUP_LINGUAL,               N_("Group tagged attachments as 'multipart/multilingual'")) \
  /* L10N: Help for Compose function: <group-related> */ \
  _fmt(OP_ATTACH_GROUP_RELATED,               N_("Group tagged attachments as 'multipart/related'")) \
  /* L10N: Help for Compose function: <move-attachment-down> */ \
  _fmt(OP_ATTACH_MOVE_ATTACHMENT_DOWN,        N_("Move an attachment down in the attachment list")) \
  /* L10N: Help for Compose function: <move-attachment-up> */ \
  _fmt(OP_ATTACH_MOVE_ATTACHMENT_UP,          N_("Move an attachment up in the attachment list")) \
  /* L10N: Help for Compose, Attach function: <save-attachment> */ \
  _fmt(OP_ATTACH_SAVE_ATTACHMENT,             N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help for Compose function: <toggle-disposition> */ \
  _fmt(OP_ATTACH_TOGGLE_DISPOSITION,          N_("Toggle disposition between inline/attachment")) \
  /* L10N: Help for Compose function: <toggle-recode> */ \
  _fmt(OP_ATTACH_TOGGLE_RECODE,               N_("Toggle recoding of this attachment")) \
  /* L10N: Help for Compose function: <toggle-unlink> */ \
  _fmt(OP_ATTACH_TOGGLE_UNLINK,               N_("Toggle whether to delete file after sending it")) \
  /* L10N: Help for Attach function: <undelete-entry> */ \
  _fmt(OP_ATTACH_UNDELETE,                    N_("Undelete the current entry")) \
  /* L10N: Help for Compose function: <ungroup-attachments> */ \
  _fmt(OP_ATTACH_UNGROUP_ATTACHMENTS,         N_("Ungroup 'multipart' attachment")) \
  /* L10N: Help for Compose function: <update-encoding> */ \
  _fmt(OP_ATTACH_UPDATE_ENCODING,             N_("Update an attachment's encoding info")) \

#ifdef USE_AUTOCRYPT
#define OPS_AUTOCRYPT(_fmt) \
  /* L10N: Help for Autocrypt function: <create-account> */ \
  _fmt(OP_AUTOCRYPT_CREATE_ACCT,              N_("Create a new autocrypt account")) \
  /* L10N: Help for Autocrypt function: <delete-account> */ \
  _fmt(OP_AUTOCRYPT_DELETE_ACCT,              N_("Delete the current account")) \
  /* L10N: Help for Autocrypt function: <toggle-active> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_ACTIVE,            N_("Toggle the current account active/inactive")) \
  /* L10N: Help for Autocrypt function: <toggle-prefer-encrypt> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_PREFER,            N_("Toggle the current account prefer-encrypt flag")) \
  /* L10N: Help for Compose function: <view-autocrypt-options> */ \
  _fmt(OP_COMPOSE_VIEW_AUTOCRYPT_OPTIONS,     N_("Show Autocrypt options")) \
  /* L10N: Help for Index function: <view-autocrypt-accounts> */ \
  _fmt(OP_VIEW_AUTOCRYPT_ACCOUNTS,            N_("Manage autocrypt accounts"))
#else
#define OPS_AUTOCRYPT(_)
#endif

#define OPS_COMPOSE(_fmt) \
  /* L10N: Help for Compose function: <check-spelling> */ \
  _fmt(OP_COMPOSE_CHECK_SPELLING,             N_("Check the spelling of the message")) \
  /* L10N: Help for Compose function: <edit-file> */ \
  _fmt(OP_COMPOSE_EDIT_FILE,                  N_("Edit the file to be attached")) \
  /* L10N: Help for Compose function: <edit-message> */ \
  _fmt(OP_COMPOSE_EDIT_MESSAGE,               N_("Edit the message")) \
  /* L10N: Help for Compose function: <postpone-message> */ \
  _fmt(OP_COMPOSE_POSTPONE_MESSAGE,           N_("Save this message to send later")) \
  /* L10N: Help for Compose function: <rename-file-on-disk> */ \
  _fmt(OP_COMPOSE_RENAME_FILE_ON_DISK,        N_("Rename/move an attached file")) \
  /* L10N: Help for Compose function: <save-message-copy> */ \
  _fmt(OP_COMPOSE_SAVE_MESSAGE_COPY,          N_("Write the message to a folder")) \
  /* L10N: Help for Compose function: <send-message> */ \
  _fmt(OP_COMPOSE_SEND_MESSAGE,               N_("Send the message")) \
  /* L10N: Help for Attach, Index function: <compose-to-sender> */ \
  _fmt(OP_COMPOSE_TO_SENDER,                  N_("Compose new message to the current message sender")) \

#define OPS_CORE(_fmt) \
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
  /* L10N: Help for Browser function: <change-dir> */ \
  _fmt(OP_CHANGE_DIRECTORY,                   N_("Change directories")) \
  /* L10N: Help for Browser function: <check-new> */ \
  _fmt(OP_CHECK_NEW,                          N_("Check mailboxes for new mail")) \
  /* L10N: Help for Alias, Index, Query function: <compose-message> */ \
  _fmt(OP_COMPOSE_MESSAGE,                    N_("Compose a new mail message")) \
  /* L10N: Help for Index function: <copy-message> */ \
  _fmt(OP_COPY_MESSAGE,                       N_("Copy a message to a file/mailbox")) \
  /* L10N: Help for Index function: <copy-message-decoded> */ \
  _fmt(OP_COPY_MESSAGE_DECODED,               N_("Make decoded (text/plain) copy")) \
  /* L10N: Help for Index, Query function: <create-alias> */ \
  _fmt(OP_CREATE_ALIAS,                       N_("Create an alias from a message sender")) \
  /* L10N: Help for Browser function: <create-mailbox> */ \
  _fmt(OP_CREATE_MAILBOX,                     N_("Create a new mailbox (IMAP only)")) \
  /* L10N: Help for Index function: <create-message-hotkey> */ \
  _fmt(OP_CREATE_MESSAGE_HOTKEY,              N_("Create a hotkey macro for the current message")) \
  /* L10N: Help for Alias function: <delete-alias> */ \
  _fmt(OP_DELETE_ALIAS,                       N_("Delete the current alias")) \
  /* L10N: Help for Postpone, Index function: <delete-message> */ \
  _fmt(OP_DELETE_MESSAGE,                     N_("Delete the current message")) \
  /* L10N: Help for Browser function: <delete-mailbox> */ \
  _fmt(OP_DELETE_MAILBOX,                     N_("Delete the current mailbox (IMAP only)")) \
  /* L10N: Help for Index function: <delete-subthread> */ \
  _fmt(OP_DELETE_SUBTHREAD,                   N_("Delete all messages in subthread")) \
  /* L10N: Help for Index function: <delete-thread> */ \
  _fmt(OP_DELETE_THREAD,                      N_("Delete all messages in thread")) \
  /* L10N: Help for Browser function: <descend-directory> */ \
  _fmt(OP_DESCEND_DIRECTORY,                  N_("Descend into a directory")) \
  /* L10N: Help for Index function: <display-message> */ \
  _fmt(OP_DISPLAY_MESSAGE,                    N_("Display a message")) \
  /* L10N: Help for Attach, Compose, Index function: <display-message-headers> */ \
  _fmt(OP_DISPLAY_MESSAGE_HEADERS,            N_("Display message and toggle header weeding")) \
  /* L10N: Help for Index, Index function: <edit-raw-message> */ \
  _fmt(OP_EDIT_RAW_MESSAGE,                   N_("Edit the raw message if the mailbox is not read-only, otherwise view it")) \
  /* L10N: Help for Index, Index function: <edit-raw-message-readonly> */ \
  _fmt(OP_EDIT_RAW_MESSAGE_READONLY,          N_("Edit the raw message (edit and edit-raw-message are synonyms)")) \
  /* L10N: Help for Index, Index function: <edit-tags> */ \
  _fmt(OP_EDIT_TAGS,                          N_("Modify (notmuch/imap) tags")) \
  /* L10N: Help for Index, Index function: <edit-tags-then-hide> */ \
  _fmt(OP_EDIT_TAGS_THEN_HIDE,                N_("Modify (notmuch/imap) tags and then hide message")) \
  /* L10N: Help for Index function: <edit-x-label> */ \
  _fmt(OP_EDIT_X_LABEL,                       N_("Add, change, or delete a message's label")) \
  /* L10N: Help for Browser function: <limit> */ \
  _fmt(OP_BROWSER_LIMIT,                      N_("Limit the browser to matching files")) \
  /* L10N: Help for Attach, Index function: <forward-message> */ \
  _fmt(OP_FORWARD_MESSAGE,                    N_("Forward a message with comments")) \
  /* L10N: Help for Browser function: <goto-home> */ \
  _fmt(OP_GOTO_HOME,                          N_("Go to home directory")) \
  /* L10N: Help for Browser function: <goto-parent> */ \
  _fmt(OP_GOTO_PARENT,                        N_("Go to parent directory")) \
  /* L10N: Help for Index function: <limit-thread> */ \
  _fmt(OP_LIMIT_THREAD,                       N_("Limit view to current thread")) \
  /* L10N: Help for Browser function: <goto-root> */ \
  _fmt(OP_GOTO_ROOT,                          N_("Go to root directory")) \
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
  /* L10N: Help for Index function: <break-thread> */ \
  _fmt(OP_MAIN_BREAK_THREAD,                  N_("Break the thread in two")) \
  /* L10N: Help for Index function: <browse-mailboxes> */ \
  _fmt(OP_MAIN_BROWSE_MAILBOXES,              N_("Browse mailboxes")) \
  /* L10N: Help for Index function: <browse-mailboxes-readonly> */ \
  _fmt(OP_MAIN_BROWSE_MAILBOXES_READONLY,     N_("Browse mailboxes in read only mode")) \
  /* L10N: Help for Index function: <change-folder> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER,                 N_("Open a different folder")) \
  /* L10N: Help for Index function: <change-folder-readonly> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER_READONLY,        N_("Open a different folder in read only mode")) \
  /* L10N: Help for Index function: <change-newsgroup> */ \
  _fmt(OP_MAIN_CHANGE_GROUP,                  N_("Open a different newsgroup")) \
  /* L10N: Help for Index function: <change-newsgroup-readonly> */ \
  _fmt(OP_MAIN_CHANGE_GROUP_READONLY,         N_("Open a different newsgroup in read only mode")) \
  /* L10N: Help for Index function: <delete-pattern> */ \
  _fmt(OP_MAIN_DELETE_PATTERN,                N_("Delete non-hidden messages matching a pattern")) \
  /* L10N: Help for Index function: <imap-fetch-mail> */ \
  _fmt(OP_MAIN_IMAP_FETCH,                    N_("Force retrieval of mail from IMAP server")) \
  /* L10N: Help for Index function: <imap-logout-all> */ \
  _fmt(OP_MAIN_IMAP_LOGOUT_ALL,               N_("Logout from all IMAP servers")) \
  /* L10N: Help for Index function: <link-threads> */ \
  _fmt(OP_MAIN_LINK_THREADS,                  N_("Link tagged message to the current one")) \
  /* L10N: Help for Index function: <set-flag> */ \
  _fmt(OP_MAIN_SET_FLAG,                      N_("Set a status flag on a message")) \
  /* L10N: Help for Index function: <sync-mailbox> */ \
  _fmt(OP_MAIN_SYNC_FOLDER,                   N_("Save changes to mailbox")) \
  /* L10N: Help for Index function: <undelete-pattern> */ \
  _fmt(OP_MAIN_UNDELETE_PATTERN,              N_("Undelete non-hidden messages matching a pattern")) \
  /* L10N: Help for Index function: <mark-subthread-read> */ \
  _fmt(OP_MARK_SUBTHREAD_READ,                N_("Mark the current subthread as read")) \
  /* L10N: Help for Index function: <mark-thread-read> */ \
  _fmt(OP_MARK_THREAD_READ,                   N_("Mark the current thread as read")) \
  /* L10N: Help for Index function: <move-message> */ \
  _fmt(OP_MOVE_MESSAGE,                       N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help for Index function: <move-message-decoded> */ \
  _fmt(OP_MOVE_MESSAGE_DECODED,               N_("Make decoded copy (text/plain) and delete")) \
  /* L10N: Help for Attach, Index function: <nntp-followup-message> */ \
  _fmt(OP_NNTP_FOLLOWUP_MESSAGE,              N_("Followup to newsgroup")) \
  /* L10N: Help for Attach, Index function: <nntp-forward-to-group> */ \
  _fmt(OP_NNTP_FORWARD_TO_GROUP,              N_("Forward to newsgroup")) \
  /* L10N: Help for Index function: <nntp-get-children> */ \
  _fmt(OP_NNTP_GET_CHILDREN,                  N_("Get all children of the current message")) \
  /* L10N: Help for Index function: <nntp-get-message> */ \
  _fmt(OP_NNTP_GET_MESSAGE,                   N_("Get message with Message-ID")) \
  /* L10N: Help for Index function: <nntp-get-parent> */ \
  _fmt(OP_NNTP_GET_PARENT,                    N_("Get parent of the current message")) \
  /* L10N: Help for Browser, Index function: <nntp-mark-newsgroup-read> */ \
  _fmt(OP_NNTP_MARK_NEWSGROUP_READ,           N_("Mark all articles in newsgroup as read")) \
  /* L10N: Help for Index function: <nntp-post-message> */ \
  _fmt(OP_NNTP_POST_MESSAGE,                  N_("Post message to newsgroup")) \
  /* L10N: Help for Index function: <nntp-reconstruct-thread> */ \
  _fmt(OP_NNTP_RECONSTRUCT_THREAD,            N_("Reconstruct thread containing current message")) \
  /* L10N: Help for Index function: <pop-fetch-mail> */ \
  _fmt(OP_POP_FETCH_MAIL,                     N_("Retrieve mail from POP server")) \
  /* L10N: Help for Index function: <purge-message> */ \
  _fmt(OP_PURGE_MESSAGE,                      N_("Delete the current entry, bypassing the trash folder")) \
  /* L10N: Help for Index function: <purge-thread> */ \
  _fmt(OP_PURGE_THREAD,                       N_("Delete the current thread, bypassing the trash folder")) \
  /* L10N: Help for Index function: <quasi-delete-message> */ \
  _fmt(OP_QUASI_DELETE_MESSAGE,               N_("Delete from NeoMutt, don't touch on disk")) \
  /* L10N: Help for Query function: <query-append> */ \
  _fmt(OP_QUERY_APPEND,                       N_("Append new query results to current results")) \
  /* L10N: Help for Index function: <recall-draft-message> */ \
  _fmt(OP_RECALL_DRAFT_MESSAGE,               N_("Recall a postponed message")) \
  /* L10N: Help for Browser function: <rename-mailbox> */ \
  _fmt(OP_RENAME_MAILBOX,                     N_("Rename the current mailbox (IMAP only)")) \
  /* L10N: Help for Attach, Index function: <reply-all> */ \
  _fmt(OP_REPLY_ALL,                          N_("Reply to all recipients")) \
  /* L10N: Help for Attach, Index function: <reply-group-chat> */ \
  _fmt(OP_REPLY_GROUP_CHAT,                   N_("Reply to all recipients preserving To/Cc")) \
  /* L10N: Help for Attach, Index function: <reply-sender> */ \
  _fmt(OP_REPLY_SENDER,                       N_("Reply to a message")) \
  /* L10N: Help for Attach, Index function: <resend-message> */ \
  _fmt(OP_RESEND,                             N_("Use the current message as a template for a new one")) \
  /* L10N: Help for Index function: <select-next-new-entry> */ \
  _fmt(OP_SELECT_NEXT_NEW_ENTRY,              N_("Jump to the next new message")) \
  /* L10N: Help for Index function: <select-next-new-or-unread-entry> */ \
  _fmt(OP_SELECT_NEXT_NEW_OR_UNREAD_ENTRY,    N_("Jump to the next new or unread message")) \
  /* L10N: Help for Index function: <select-next-undeleted-entry> */ \
  _fmt(OP_SELECT_NEXT_UNDELETED_ENTRY,        N_("Move to the next undeleted message")) \
  /* L10N: Help for Index function: <select-next-unread-entry> */ \
  _fmt(OP_SELECT_NEXT_UNREAD_ENTRY,           N_("Jump to the next unread message")) \
  /* L10N: Help for Index function: <select-next-unread-mailbox> */ \
  _fmt(OP_SELECT_NEXT_UNREAD_MAILBOX,         N_("Open next mailbox with new mail")) \
  /* L10N: Help for Index function: <select-previous-new-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_NEW_ENTRY,          N_("Jump to the previous new message")) \
  /* L10N: Help for Index function: <select-previous-new-or-unread-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_NEW_OR_UNREAD_ENTRY, N_("Jump to the previous new or unread message")) \
  /* L10N: Help for Index function: <select-previous-undeleted-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_UNDELETED_ENTRY,    N_("Move to the previous undeleted message")) \
  /* L10N: Help for Index function: <select-previous-unread> */ \
  _fmt(OP_SELECT_PREVIOUS_UNREAD,             N_("Jump to the previous unread message")) \
  /* L10N: Help for Index function: <select-previous-unread-mailbox> */ \
  _fmt(OP_SELECT_PREVIOUS_UNREAD_MAILBOX,     N_("Open previous mailbox with new mail")) \
  /* L10N: Help for Browser, Index function: <show-mailboxes> */ \
  _fmt(OP_SHOW_MAILBOXES,                     N_("List mailboxes with new mail")) \
  /* L10N: Help for Index function: <show-sender-address> */ \
  _fmt(OP_SHOW_SENDER_ADDRESS,                N_("Display full address of sender")) \
  /* L10N: Help for Browser function: <subscribe-pattern> */ \
  _fmt(OP_SUBSCRIBE_PATTERN,                  N_("Subscribe to newsgroups matching a pattern")) \
  /* L10N: Help for Index function: <toggle-important-flag> */ \
  _fmt(OP_TOGGLE_IMPORTANT_FLAG,              N_("Toggle a message's 'important' flag")) \
  /* L10N: Help for Browser function: <toggle-mailboxes> */ \
  _fmt(OP_TOGGLE_MAILBOXES,                   N_("Toggle whether to browse mailboxes or all files")) \
  /* L10N: Help for Index function: <toggle-mailbox-readonly> */ \
  _fmt(OP_TOGGLE_MAILBOX_READONLY,            N_("Toggle whether the mailbox will be rewritten")) \
  /* L10N: Help for Index function: <toggle-new-flag> */ \
  _fmt(OP_TOGGLE_NEW_FLAG,                    N_("Toggle a message's 'new' flag")) \
  /* L10N: Help for Index function: <toggle-read-messages> */ \
  _fmt(OP_TOGGLE_READ_MESSAGES,               N_("Toggle view of read messages")) \
  /* L10N: Help for Browser function: <uncatchup> */ \
  _fmt(OP_UNCATCHUP,                          N_("Mark all articles in newsgroup as unread")) \
  /* L10N: Help for Alias function: <undelete-alias> */ \
  _fmt(OP_UNDELETE_ALIAS,                     N_("Undelete the current alias")) \
  /* L10N: Help for Alias, Postpone, Index function: <undelete-message> */ \
  _fmt(OP_UNDELETE_MESSAGE,                   N_("Undelete the current message")) \
  /* L10N: Help for Index function: <undelete-subthread> */ \
  _fmt(OP_UNDELETE_SUBTHREAD,                 N_("Undelete all messages in subthread")) \
  /* L10N: Help for Index function: <undelete-thread> */ \
  _fmt(OP_UNDELETE_THREAD,                    N_("Undelete all messages in thread")) \
  /* L10N: Help for Index function: <unset-flag> */ \
  _fmt(OP_UNSET_FLAG,                         N_("Clear a status flag from a message")) \
  /* L10N: Help for Browser function: <unsubscribe-pattern> */ \
  _fmt(OP_UNSUBSCRIBE_PATTERN,                N_("Unsubscribe from newsgroups matching a pattern")) \
  /* L10N: Help for Index, Query function: <view-address-query> */ \
  _fmt(OP_VIEW_ADDRESS_QUERY,                 N_("Query external program for addresses")) \
  /* L10N: Help for Index function: <view-aliases> */ \
  _fmt(OP_VIEW_ALIASES,                       N_("Open the aliases dialog")) \
  /* L10N: Help for Index function: <view-attachments> */ \
  _fmt(OP_VIEW_ATTACHMENTS,                   N_("Show MIME attachments")) \
  /* L10N: Help for Index function: <view-list-actions> */ \
  _fmt(OP_VIEW_LIST_ACTIONS,                  N_("Perform mailing list action")) \

#define OPS_CRYPT(_fmt) \
  /* L10N: Help for Index function: <copy-message-decrypted> */ \
  _fmt(OP_COPY_MESSAGE_DECRYPTED,             N_("Make decrypted copy")) \
  /* L10N: Help for Attach, Index function: <extract-keys> */ \
  _fmt(OP_EXTRACT_KEYS,                       N_("Extract supported public keys")) \
  /* L10N: Help for Index function: <move-message-decrypted> */ \
  _fmt(OP_MOVE_MESSAGE_DECRYPTED,             N_("Make decrypted copy and delete")) \

#define OPS_ENVELOPE(_fmt) \
  /* L10N: Help for Compose function: <edit-bcc> */ \
  _fmt(OP_ENVELOPE_EDIT_BCC,                  N_("Edit the 'Bcc:' list")) \
  /* L10N: Help for Compose function: <edit-cc> */ \
  _fmt(OP_ENVELOPE_EDIT_CC,                   N_("Edit the 'Cc:' list")) \
  /* L10N: Help for Compose function: <edit-fcc> */ \
  _fmt(OP_ENVELOPE_EDIT_FCC,                  N_("Enter a file to save a copy of this message in")) \
  /* L10N: Help for Compose function: <edit-followup-to> */ \
  _fmt(OP_ENVELOPE_EDIT_FOLLOWUP_TO,          N_("Edit the 'Followup-to:' field")) \
  /* L10N: Help for Compose function: <edit-from> */ \
  _fmt(OP_ENVELOPE_EDIT_FROM,                 N_("Edit the 'From:' field")) \
  /* L10N: Help for Compose function: <edit-headers> */ \
  _fmt(OP_ENVELOPE_EDIT_HEADERS,              N_("Edit the message with headers")) \
  /* L10N: Help for Compose function: <edit-newsgroups> */ \
  _fmt(OP_ENVELOPE_EDIT_NEWSGROUPS,           N_("Edit the newsgroups list")) \
  /* L10N: Help for Compose function: <edit-reply-to> */ \
  _fmt(OP_ENVELOPE_EDIT_REPLY_TO,             N_("Edit the 'Reply-to:' field")) \
  /* L10N: Help for Compose function: <edit-subject> */ \
  _fmt(OP_ENVELOPE_EDIT_SUBJECT,              N_("Edit the 'Subject:' of this message")) \
  /* L10N: Help for Compose function: <edit-to> */ \
  _fmt(OP_ENVELOPE_EDIT_TO,                   N_("Edit the 'To:' list")) \
  /* L10N: Help for Compose function: <edit-x-comment-to> */ \
  _fmt(OP_ENVELOPE_EDIT_X_COMMENT_TO,         N_("Edit the 'X-comment-to:' field")) \

#ifdef USE_NOTMUCH
#define OPS_NOTMUCH(_fmt) \
  /* L10N: Help for Index function: <fetch-entire-thread> */ \
  _fmt(OP_FETCH_ENTIRE_THREAD,                N_("Read entire thread of the current message")) \
  /* L10N: Help for Index function: <vfolder-create-from-query> */ \
  _fmt(OP_VFOLDER_CREATE_FROM_QUERY,          N_("Generate virtual folder from query")) \
  /* L10N: Help for Index function: <vfolder-create-from-query-readonly> */ \
  _fmt(OP_VFOLDER_CREATE_FROM_QUERY_READONLY, N_("Generate a read-only virtual folder from query")) \
  /* L10N: Help for Index function: <vfolder-reset-window> */ \
  _fmt(OP_VFOLDER_RESET_WINDOW,               N_("Resets virtual folder time window to the present")) \
  /* L10N: Help for Index function: <vfolder-shift-window-back> */ \
  _fmt(OP_VFOLDER_SHIFT_WINDOW_BACK,          N_("Shifts virtual folder time window backwards")) \
  /* L10N: Help for Index function: <vfolder-shift-window-forward> */ \
  _fmt(OP_VFOLDER_SHIFT_WINDOW_FORWARD,       N_("Shifts virtual folder time window forwards"))
#else
#define OPS_NOTMUCH(_)
#endif

#define OPS_PAGER(_fmt) \
  /* L10N: Help for Pager function: <skip-headers> */ \
  _fmt(OP_PAGER_SKIP_HEADERS,                 N_("Scroll to first line after the headers")) \
  /* L10N: Help for Pager function: <skip-quoted-text> */ \
  _fmt(OP_PAGER_SKIP_QUOTED_TEXT,             N_("Scroll past the next quoted block")) \
  /* L10N: Help for Pager function: <toggle-quoted-text> */ \
  _fmt(OP_PAGER_TOGGLE_QUOTED_TEXT,           N_("Show/hide quoted text")) \
  /* L10N: Help for Pager function: <toggle-search-highlighting> */ \
  _fmt(OP_PAGER_TOGGLE_SEARCH_HIGHLIGHTING,   N_("Show/hide highlighting of search matches")) \

#define OPS_PGP(_fmt) \
  /* L10N: Help for Attach, Index function: <check-traditional-pgp> */ \
  _fmt(OP_CHECK_TRADITIONAL,                  N_("Check for classic PGP")) \
  /* L10N: Help for Compose function: <attach-pgp-key> */ \
  _fmt(OP_COMPOSE_ATTACH_PGP_KEY,             N_("Attach a PGP public key")) \
  /* L10N: Help for Compose function: <view-pgp-options> */ \
  _fmt(OP_COMPOSE_VIEW_PGP_OPTIONS,           N_("Show PGP options")) \
  /* L10N: Help for Index function: <send-pgp-key> */ \
  _fmt(OP_SEND_PGP_KEY,                       N_("Mail a PGP public key")) \
  /* L10N: Help for Pgp, Smime function: <display-details> */ \
  _fmt(OP_DISPLAY_DETAILS,                    N_("Display full key/certificate info")) \
  /* L10N: Help for Pgp, Smime function: <show-identity> */ \
  _fmt(OP_SHOW_IDENTITY,                      N_("Show the full identity of the key/certificate")) \

#define OPS_PREVIEW(_fmt) \
  /* L10N: Help for Compose function <preview-scroll-end> */ \
  _fmt(OP_PREVIEW_SCROLL_END,                 N_("Scroll to the bottom")) \
  /* L10N: Help for Compose function: <preview-scroll-half-down> */ \
  _fmt(OP_PREVIEW_SCROLL_HALF_DOWN,           N_("Scroll down half a page")) \
  /* L10N: Help for Compose function: <preview-scroll-half-up> */ \
  _fmt(OP_PREVIEW_SCROLL_HALF_UP,             N_("Scroll up half a page")) \
  /* L10N: Help for Compose function <preview-scroll-home> */ \
  _fmt(OP_PREVIEW_SCROLL_HOME,                N_("Scroll to the top")) \
  /* L10N: Help for Compose function: <preview-scroll-line-down> */ \
  _fmt(OP_PREVIEW_SCROLL_LINE_DOWN,           N_("Scroll down one line")) \
  /* L10N: Help for Compose function: <preview-scroll-line-up> */ \
  _fmt(OP_PREVIEW_SCROLL_LINE_UP,             N_("Scroll up one line")) \
  /* L10N: Help for Compose function: <preview-scroll-page-down> */ \
  _fmt(OP_PREVIEW_SCROLL_PAGE_DOWN,           N_("Show the next page of the message")) \
  /* L10N: Help for Compose function: <preview-scroll-page-up> */ \
  _fmt(OP_PREVIEW_SCROLL_PAGE_UP,             N_("Show the previous page of the message")) \

#define OPS_SIDEBAR(_fmt) \
  /* L10N: Help for Sidebar function: <sidebar-activate-entry> */ \
  _fmt(OP_SIDEBAR_ACTIVATE_ENTRY,             N_("Open highlighted mailbox")) \
  /* L10N: Help for Generic function <sidebar-scroll-end> */ \
  _fmt(OP_SIDEBAR_SCROLL_END,                 N_("Scroll to the bottom")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-half-down> */ \
  _fmt(OP_SIDEBAR_SCROLL_HALF_DOWN,           N_("Scroll down half a page")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-half-up> */ \
  _fmt(OP_SIDEBAR_SCROLL_HALF_UP,             N_("Scroll up half a page")) \
  /* L10N: Help for Generic function <sidebar-scroll-home> */ \
  _fmt(OP_SIDEBAR_SCROLL_HOME,                N_("Scroll to the top")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-line-down> */ \
  _fmt(OP_SIDEBAR_SCROLL_LINE_DOWN,           N_("Scroll down one line")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-line-up> */ \
  _fmt(OP_SIDEBAR_SCROLL_LINE_UP,             N_("Scroll up one line")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-page-down> */ \
  _fmt(OP_SIDEBAR_SCROLL_PAGE_DOWN,           N_("Scroll down one page")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-page-up> */ \
  _fmt(OP_SIDEBAR_SCROLL_PAGE_UP,             N_("Scroll up one page")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-selection-to-bottom> */ \
  _fmt(OP_SIDEBAR_SCROLL_SELECTION_TO_BOTTOM, N_("Scroll the highlight to the bottom of the page")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-selection-to-middle> */ \
  _fmt(OP_SIDEBAR_SCROLL_SELECTION_TO_MIDDLE, N_("Scroll the highlight to the middle of the page")) \
  /* L10N: Help for Sidebar function: <sidebar-scroll-selection-to-top> */ \
  _fmt(OP_SIDEBAR_SCROLL_SELECTION_TO_TOP,    N_("Scroll the highlight to the top of the page")) \
  /* L10N: Help for Sidebar function: <sidebar-search> */ \
  _fmt(OP_SIDEBAR_SEARCH,                     N_("Fuzzy search the sidebar")) \
  /* L10N: Help for Sidebar function: <sidebar-select-entry-by-number> */ \
  _fmt(OP_SIDEBAR_SELECT_ENTRY_BY_NUMBER,     N_("Select a mailbox by its index number")) \
  /* L10N: Help for Sidebar function: <sidebar-select-first-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_FIRST_ENTRY,         N_("Highlight the first mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-select-last-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_LAST_ENTRY,          N_("Highlight the last mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-select-next-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_NEXT_ENTRY,          N_("Highlight the next mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-select-next-new-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_NEXT_NEW_ENTRY,      N_("Highlight the next mailbox with new mail")) \
  /* L10N: Help for Sidebar function: <sidebar-select-page-bottom> */ \
  _fmt(OP_SIDEBAR_SELECT_PAGE_BOTTOM,         N_("Highlight the mailbox at the bottom of the page")) \
  /* L10N: Help for Sidebar function: <sidebar-select-page-middle> */ \
  _fmt(OP_SIDEBAR_SELECT_PAGE_MIDDLE,         N_("Highlight the mailbox in the middle of the page")) \
  /* L10N: Help for Sidebar function: <sidebar-select-page-top> */ \
  _fmt(OP_SIDEBAR_SELECT_PAGE_TOP,            N_("Highlight the mailbox at the top of the page")) \
  /* L10N: Help for Sidebar function: <sidebar-select-previous-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_PREVIOUS_ENTRY,      N_("Highlight the previous mailbox")) \
  /* L10N: Help for Sidebar function: <sidebar-select-previous-new-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_PREVIOUS_NEW_ENTRY,  N_("Highlight the previous mailbox with new mail")) \
  /* L10N: Help for Sidebar function: <sidebar-toggle-virtual> */ \
  _fmt(OP_SIDEBAR_TOGGLE_VIRTUAL,             N_("Toggle between mailboxes and virtual mailboxes")) \
  /* L10N: Help for Sidebar function: <sidebar-toggle-visible> */ \
  _fmt(OP_SIDEBAR_TOGGLE_VISIBLE,             N_("Show/hide the sidebar")) \

#define OPS_SMIME(_fmt) \
  /* L10N: Help for Compose function: <view-smime-options> */ \
  _fmt(OP_COMPOSE_VIEW_SMIME_OPTIONS,         N_("Show S/MIME options")) \

#define OPS(_fmt) \
  _fmt(OP_NULL,                               N_("Null operation")) \
  OPS_DIALOG(_fmt) \
  OPS_ENTRY(_fmt) \
  OPS_GLOBAL(_fmt) \
  OPS_SCROLL(_fmt) \
  OPS_SEARCH(_fmt) \
  OPS_SELECT(_fmt) \
  OPS_TAG(_fmt) \
  OPS_TREE(_fmt) \
  OPS_VIEW(_fmt) \
  OPS_EDITOR(_fmt) \
  OPS_ATTACH(_fmt) \
  OPS_AUTOCRYPT(_fmt) \
  OPS_COMPOSE(_fmt) \
  OPS_CORE(_fmt) \
  OPS_CRYPT(_fmt) \
  OPS_ENVELOPE(_fmt) \
  OPS_NOTMUCH(_fmt) \
  OPS_PAGER(_fmt) \
  OPS_PGP(_fmt) \
  OPS_PREVIEW(_fmt) \
  OPS_SIDEBAR(_fmt) \
  OPS_SMIME(_fmt) \

/**
 * enum MuttOps - All NeoMutt Opcodes
 *
 * Opcodes, e.g. OP_TOGGLE_NEW_FLAG
 */
enum MuttOps {
#define DEFINE_OPS(opcode, help_string) opcode,
  OPS(DEFINE_OPS)
#undef DEFINE_OPS
  OP_MAX,
};
// clang-format on

#endif /* MUTT_OPCODES_H */
