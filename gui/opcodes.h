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

#ifndef MUTT_GUI_OPCODES_H
#define MUTT_GUI_OPCODES_H

#include "config.h"
#include "mutt/lib.h"

const char *opcodes_get_description(int op);
const char *opcodes_get_name       (int op);

#define OP_REPAINT     (-3) ///< Repaint is needed
#define OP_TIMEOUT     (-2) ///< 1 second with no events
#define OP_ABORT       (-1) ///< $abort_key pressed (Ctrl-G)

// Alias Functions - Handled by alias_function_dispatcher()
#define OPS_ALIAS(_fmt) \
  /* L10N: Help for Alias Function: <compose-to> */ \
  _fmt(OP_ALI_COMPOSE_MESSAGE,                    N_("Compose a new mail message to the current alias")) \
  /* L10N: Help for Alias Function: <delete-alias> */ \
  _fmt(OP_DELETE_ALIAS,                           N_("Delete the current alias")) \
  /* L10N: Help for Alias Function: <undelete-alias> */ \
  _fmt(OP_UNDELETE_ALIAS,                         N_("Undelete the current alias")) \

// Attach Functions - Handled by attach_function_dispatcher()
#define OPS_ATTACH(_fmt) \
  /* L10N: Help for Attach Function: <bounce-message> */ \
  _fmt(OP_ATT_BOUNCE_MESSAGE,                     N_("Remail a message to another user")) \
  /* L10N: Help for Attach Function: <check-traditional-pgp> */ \
  _fmt(OP_ATT_CHECK_TRADITIONAL,                  N_("Check for classic PGP")) \
  /* L10N: Help for Attach Function: <compose-to-sender> */ \
  _fmt(OP_ATT_COMPOSE_TO_SENDER,                  N_("Compose new message to the current message sender")) \
  /* L10N: Help for Attach Function: <delete-attachment> */ \
  _fmt(OP_ATTACH_DELETE,                          N_("Delete the current entry")) \
  /* L10N: Help for Attach Function: <display-attachment-default> */ \
  _fmt(OP_ATT_DISPLAY_ATTACHMENT_DEFAULT,         N_("View attachment using mailcap entry if necessary")) \
  /* L10N: Help for Attach Function: <display-attachment-mailcap> */ \
  _fmt(OP_ATT_DISPLAY_ATTACHMENT_MAILCAP,         N_("Force viewing of attachment using mailcap")) \
  /* L10N: Help for Attach Function: <display-attachment-pager> */ \
  _fmt(OP_ATT_DISPLAY_ATTACHMENT_PAGER,           N_("View attachment in pager using copiousoutput mailcap")) \
  /* L10N: Help for Attach Function: <display-attachment-text> */ \
  _fmt(OP_ATT_DISPLAY_ATTACHMENT_TEXT,            N_("View attachment as text")) \
  /* L10N: Help for Attach Function: <display-message-headers> */ \
  _fmt(OP_ATT_DISPLAY_MESSAGE_HEADERS,            N_("Display the current message and toggle ignored headers")) \
  /* L10N: Help for Attach Function: <edit-content-type> */ \
  _fmt(OP_ATT_ATTACH_EDIT_CONTENT_TYPE,           N_("Edit attachment content type")) \
  /* L10N: Help for Attach Function: <extract-keys> */ \
  _fmt(OP_ATT_EXTRACT_KEYS,                       N_("Extract supported public keys")) \
  /* L10N: Help for Attach Function: <forward-message> */ \
  _fmt(OP_ATT_FORWARD_MESSAGE,                    N_("Forward a message with comments")) \
  /* L10N: Help for Attach Function: <list-reply> */ \
  _fmt(OP_ATT_LIST_REPLY,                         N_("Reply to specified mailing list")) \
  /* L10N: Help for Attach Function: <list-subscribe> */ \
  _fmt(OP_ATT_LIST_SUBSCRIBE,                     N_("Subscribe to a mailing list")) \
  /* L10N: Help for Attach Function: <list-unsubscribe> */ \
  _fmt(OP_ATT_LIST_UNSUBSCRIBE,                   N_("Unsubscribe from a mailing list")) \
  /* L10N: Help for Attach Function: <nntp-followup-message> */ \
  _fmt(OP_ATT_NNTP_FOLLOWUP_MESSAGE,              N_("Followup to newsgroup")) \
  /* L10N: Help for Attach Function: <nntp-forward-to-group> */ \
  _fmt(OP_ATT_NNTP_FORWARD_TO_GROUP,              N_("Forward to newsgroup")) \
  /* L10N: Help for Attach Function: <reply-all> */ \
  _fmt(OP_ATT_REPLY_ALL,                          N_("Reply to all recipients")) \
  /* L10N: Help for Attach Function: <reply-group-chat> */ \
  _fmt(OP_ATT_REPLY_GROUP_CHAT,                   N_("Reply to all recipients preserving To/Cc")) \
  /* L10N: Help for Attach Function: <reply-sender> */ \
  _fmt(OP_ATT_REPLY_SENDER,                       N_("Reply to a message")) \
  /* L10N: Help for Attach Function: <resend-message> */ \
  _fmt(OP_ATT_RESEND,                             N_("Use the current message as a template for a new one")) \
  /* L10N: Help for Attach Function: <save-attachment> */ \
  _fmt(OP_ATT_ATTACH_SAVE_ATTACHMENT,             N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help for Attach Function: <undelete-attachment> */ \
  _fmt(OP_ATTACH_UNDELETE,                        N_("Undelete the current entry")) \

// Autocrypt Functions - Handled by autocrypt_function_dispatcher()
#ifdef USE_AUTOCRYPT
#define OPS_AUTOCRYPT(_fmt) \
  /* L10N: Help for Autocrypt Function: <create-account> */ \
  _fmt(OP_AUTOCRYPT_CREATE_ACCT,                  N_("Create a new autocrypt account")) \
  /* L10N: Help for Autocrypt Function: <delete-account> */ \
  _fmt(OP_AUTOCRYPT_DELETE_ACCT,                  N_("Delete the current account")) \
  /* L10N: Help for Autocrypt Function: <toggle-enabled> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_ENABLED,                N_("Enable/disable the current account")) \
  /* L10N: Help for Autocrypt Function: <toggle-prefer-encrypt> */ \
  _fmt(OP_AUTOCRYPT_TOGGLE_PREFER,                N_("Toggle the prefer-encrypt attribute of the current account"))
#else
#define OPS_AUTOCRYPT(_)
#endif

// Browser functions
#define OPS_BROWSER(_fmt) \
  /* L10N: Help for Browser Function: <choose-entry> */ \
  _fmt(OP_BRO_CHOOSE_ENTRY,                       N_("Choose the currently selected entry")) \
  /* L10N: Help for Browser Function: <create-mailbox> */ \
  _fmt(OP_CREATE_MAILBOX,                         N_("Create a new mailbox (IMAP only)")) \
  /* L10N: Help for Browser Function: <delete-mailbox> */ \
  _fmt(OP_DELETE_MAILBOX,                         N_("Delete the current mailbox (IMAP only)")) \
  /* L10N: Help for Browser Function: <display-file> */ \
  _fmt(OP_BROWSER_VIEW_FILE,                      N_("Display the current file")) \
  /* L10N: Help for Browser Function: <goto-directory> */ \
  _fmt(OP_CHANGE_DIRECTORY,                       N_("Change the directory to a different one")) \
  /* L10N: Help for Browser Function: <goto-folder> */ \
  _fmt(OP_BROWSER_GOTO_FOLDER,                    N_("Change the directory to '$folder'")) \
  /* L10N: Help for Browser Function: <goto-home> */ \
  _fmt(OP_GOTO_HOME,                              N_("Change the directory to the home directory")) \
  /* L10N: Help for Browser Function: <goto-parent> */ \
  _fmt(OP_GOTO_PARENT,                            N_("Change the directory to the parent directory")) \
  /* L10N: Help for Browser Function: <goto-root> */ \
  _fmt(OP_GOTO_ROOT,                              N_("Change the directory to the root directory")) \
  /* L10N: Help for Browser Function: <goto-selected-directory> */ \
  _fmt(OP_DESCEND_DIRECTORY,                      N_("Change the directory to the selected one")) \
  /* L10N: Help for Browser Function: <limit> */ \
  _fmt(OP_BROWSER_LIMIT,                          N_("Limit the browser to matching files")) \
  /* L10N: Help for Browser Function: <nntp-mark-newsgroup-read> */ \
  _fmt(OP_BRO_NNTP_MARK_NEWSGROUP_READ,           N_("Mark all articles in newsgroup as read")) \
  /* L10N: Help for Browser Function: <nntp-mark-newsgroup-unread> */ \
  _fmt(OP_NNTP_MARK_NEWSGROUP_UNREAD,             N_("Mark all articles in newsgroup as unread")) \
  /* L10N: Help for Browser Function: <reload-active> */ \
  _fmt(OP_LOAD_ACTIVE,                            N_("Load list of all newsgroups from NNTP server")) \
  /* L10N: Help for Browser Function: <rename-mailbox> */ \
  _fmt(OP_RENAME_MAILBOX,                         N_("Rename the current mailbox (IMAP only)")) \
  /* L10N: Help for Browser Function: <select-new> */ \
  _fmt(OP_BROWSER_NEW_FILE,                       N_("Select a new file in this directory")) \
  /* L10N: Help for Browser Function: <show-full-path> */ \
  _fmt(OP_BROWSER_TELL,                           N_("Show the absolute path of the current entry")) \
  /* L10N: Help for Browser Function: <show-mailboxes> */ \
  _fmt(OP_BRO_SHOW_MAILBOXES,                     N_("Show list of mailboxes with new mail")) \
  /* L10N: Help for Browser Function: <subscribe> */ \
  _fmt(OP_BROWSER_SUBSCRIBE,                      N_("Subscribe to current mbox (IMAP/NNTP only)")) \
  /* L10N: Help for Browser Function: <subscribe-pattern> */ \
  _fmt(OP_SUBSCRIBE_PATTERN,                      N_("Subscribe to newsgroups matching a pattern")) \
  /* L10N: Help for Browser Function: <toggle-mailboxes> */ \
  _fmt(OP_TOGGLE_MAILBOXES,                       N_("Toggle whether to browse mailboxes or all files")) \
  /* L10N: Help for Browser Function: <toggle-subscribed> */ \
  _fmt(OP_BROWSER_TOGGLE_LSUB,                    N_("Toggle view all/subscribed mailboxes (IMAP only)")) \
  /* L10N: Help for Browser Function: <unsubscribe> */ \
  _fmt(OP_BROWSER_UNSUBSCRIBE,                    N_("Unsubscribe from current mbox (IMAP/NNTP only)")) \
  /* L10N: Help for Browser Function: <unsubscribe-pattern> */ \
  _fmt(OP_UNSUBSCRIBE_PATTERN,                    N_("Unsubscribe from newsgroups matching a pattern")) \

// Compose Functions - Handled by compose_function_dispatcher()
#define OPS_COMPOSE(_fmt) \
  /* L10N: Help for Compose Function: <attach-file> */ \
  _fmt(OP_ATTACH_ATTACH_FILE,                     N_("Attach files to this message")) \
  /* L10N: Help for Compose Function: <attach-message> */ \
  _fmt(OP_ATTACH_ATTACH_MESSAGE,                  N_("Attach messages to this message")) \
  /* L10N: Help for Compose Function: <attach-new-mime> */ \
  _fmt(OP_ATTACH_ATTACH_NEW_MIME,                 N_("Compose new attachment using mailcap entry")) \
  /* L10N: Help for Compose Function: <attach-news-message> */ \
  _fmt(OP_ATTACH_ATTACH_NEWS_MESSAGE,             N_("Attach news articles to this message")) \
  /* L10N: Help for Compose Function: <attach-pgp-key> */ \
  _fmt(OP_COMPOSE_ATTACH_PGP_KEY,                 N_("Attach a PGP public key")) \
  /* L10N: Help for Compose Function: <check-spelling> */ \
  _fmt(OP_COMPOSE_CHECK_SPELLING,                 N_("Check the spelling of the message")) \
  /* L10N: Help for Compose Function: <detach-file> */ \
  _fmt(OP_ATTACH_DETACH,                          N_("Delete the current entry")) \
  /* L10N: Help for Compose Function: <display-attachment-default> */ \
  _fmt(OP_COM_DISPLAY_ATTACHMENT_DEFAULT,         N_("View attachment using mailcap entry if necessary")) \
  /* L10N: Help for Compose Function: <display-attachment-mailcap> */ \
  _fmt(OP_COM_DISPLAY_ATTACHMENT_MAILCAP,         N_("Force viewing of attachment using mailcap")) \
  /* L10N: Help for Compose Function: <display-attachment-pager> */ \
  _fmt(OP_COM_DISPLAY_ATTACHMENT_PAGER,           N_("View attachment in pager using copiousoutput mailcap")) \
  /* L10N: Help for Compose Function: <display-attachment-text> */ \
  _fmt(OP_COM_DISPLAY_ATTACHMENT_TEXT,            N_("View attachment as text")) \
  /* L10N: Help for Compose Function: <display-message-headers> */ \
  _fmt(OP_COM_DISPLAY_MESSAGE_HEADERS,            N_("Display the current message and toggle ignored headers")) \
  /* L10N: Help for Compose Function: <edit-attachment-name> */ \
  _fmt(OP_ATTACH_EDIT_ATTACHMENT_NAME,            N_("Rename the current attachment; does not rename the file on disk")) \
  /* L10N: Help for Compose Function: <edit-content-id> */ \
  _fmt(OP_ATTACH_EDIT_CONTENT_ID,                 N_("Edit the 'Content-ID:' of the current attachment")) \
  /* L10N: Help for Compose Function: <edit-content-type> */ \
  _fmt(OP_COM_ATTACH_EDIT_CONTENT_TYPE,           N_("Edit the 'Content-Type:' of the current attachment")) \
  /* L10N: Help for Compose Function: <edit-content-description> */ \
  _fmt(OP_ATTACH_EDIT_DESCRIPTION,                N_("Edit the 'Content-Description:' of the current attachment")) \
  /* L10N: Help for Compose Function: <edit-content-transfer-encoding> */ \
  _fmt(OP_ATTACH_EDIT_ENCODING,                   N_("Edit the 'Content-Transfer-Encoding:' of the current attachment")) \
  /* L10N: Help for Compose Function: <edit-file> */ \
  _fmt(OP_COMPOSE_EDIT_FILE,                      N_("Edit the file to be attached")) \
  /* L10N: Help for Compose Function: <edit-headers> */ \
  _fmt(OP_ENVELOPE_EDIT_HEADERS,                  N_("Edit this message with headers")) \
  /* L10N: Help for Compose Function: <edit-content-language> */ \
  _fmt(OP_ATTACH_EDIT_LANGUAGE,                   N_("Edit the 'Content-Language:' of the current attachment")) \
  /* L10N: Help for Compose Function: <edit-message> */ \
  _fmt(OP_COMPOSE_EDIT_MESSAGE,                   N_("Edit this message")) \
  /* L10N: Help for Compose Function: <edit-mime> */ \
  _fmt(OP_ATTACH_EDIT_MIME,                       N_("Edit attachment using mailcap entry")) \
  /* L10N: Help for Compose Function: <filter-attachment> */ \
  _fmt(OP_ATTACH_FILTER_ATTACHMENT,               N_("Filter attachment through a shell command")) \
  /* L10N: Help for Compose Function: <get-attachment> */ \
  _fmt(OP_ATTACH_GET_ATTACHMENT,                  N_("Get a temporary copy of an attachment")) \
  /* L10N: Help for Compose Function: <group-alternative> */ \
  _fmt(OP_ATTACH_GROUP_ALTS,                      N_("Group tagged attachments as 'multipart/alternative'")) \
  /* L10N: Help for Compose Function: <group-multilingual> */ \
  _fmt(OP_ATTACH_GROUP_LINGUAL,                   N_("Group tagged attachments as 'multipart/multilingual'")) \
  /* L10N: Help for Compose Function: <group-related> */ \
  _fmt(OP_ATTACH_GROUP_RELATED,                   N_("Group tagged attachments as 'multipart/related'")) \
  /* L10N: Help for Compose Function: <move-attachment-down> */ \
  _fmt(OP_ATTACH_MOVE_ATTACHMENT_DOWN,            N_("Move an attachment down in the attachment list")) \
  /* L10N: Help for Compose Function: <move-attachment-up> */ \
  _fmt(OP_ATTACH_MOVE_ATTACHMENT_UP,              N_("Move an attachment up in the attachment list")) \
  /* L10N: Help for Compose Function: <postpone-message> */ \
  _fmt(OP_COMPOSE_POSTPONE_MESSAGE,               N_("Save this message as draft and leave the Compose Dialog")) \
  /* L10N: Help for Compose Function: <rename-file-on-disk> */ \
  _fmt(OP_COMPOSE_RENAME_FILE_ON_DISK,            N_("Rename/move an attached file")) \
  /* L10N: Help for Compose Function: <save-attachment> */ \
  _fmt(OP_COM_ATTACH_SAVE_ATTACHMENT,             N_("Save message/attachment to a mailbox/file")) \
  /* L10N: Help for Compose Function: <save-message-copy> */ \
  _fmt(OP_COMPOSE_SAVE_MESSAGE_COPY,              N_("Save a copy of this message to a mailbox")) \
  /* L10N: Help for Compose Function: <send-message> */ \
  _fmt(OP_COMPOSE_SEND_MESSAGE,                   N_("Send this message")) \
  /* L10N: Help for Compose Function: <toggle-disposition> */ \
  _fmt(OP_ATTACH_TOGGLE_DISPOSITION,              N_("Toggle the 'Content-Disposition:' of the current attachment between 'inline' and 'attachment'")) \
  /* L10N: Help for Compose Function: <toggle-recode> */ \
  _fmt(OP_ATTACH_TOGGLE_RECODE,                   N_("Toggle recoding of this attachment")) \
  /* L10N: Help for Compose Function: <toggle-unlink> */ \
  _fmt(OP_ATTACH_TOGGLE_UNLINK,                   N_("Toggle whether to delete file after sending it")) \
  /* L10N: Help for Compose Function: <ungroup-attachments> */ \
  _fmt(OP_ATTACH_UNGROUP_ATTACHMENTS,             N_("Ungroup 'multipart' attachment")) \
  /* L10N: Help for Compose Function: <update-encoding> */ \
  _fmt(OP_ATTACH_UPDATE_ENCODING,                 N_("Update an attachment's encoding info")) \

// Compose Envelope Functions - Handled by env_function_dispatcher()
#define OPS_COMPOSE_ENVELOPE(_fmt) \
  /* L10N: Help for Compose Envelope Function: <edit-bcc> */ \
  _fmt(OP_ENVELOPE_EDIT_BCC,                      N_("Edit the 'Bcc:' address list")) \
  /* L10N: Help for Compose Envelope Function: <edit-cc> */ \
  _fmt(OP_ENVELOPE_EDIT_CC,                       N_("Edit the 'Cc:' address list")) \
  /* L10N: Help for Compose Envelope Function: <edit-sent-mailbox> */ \
  _fmt(OP_ENVELOPE_EDIT_FCC,                      N_("Choose the mailbox where the sent message will be saved")) \
  /* L10N: Help for Compose Envelope Function: <edit-followup-to> */ \
  _fmt(OP_ENVELOPE_EDIT_FOLLOWUP_TO,              N_("Edit the 'Followup-To:' newsgroups list (NNTP only)")) \
  /* L10N: Help for Compose Envelope Function: <edit-from> */ \
  _fmt(OP_ENVELOPE_EDIT_FROM,                     N_("Edit the 'From:' address")) \
  /* L10N: Help for Compose Envelope Function: <edit-newsgroups> */ \
  _fmt(OP_ENVELOPE_EDIT_NEWSGROUPS,               N_("Edit the newsgroups list (NNTP only)")) \
  /* L10N: Help for Compose Envelope Function: <edit-reply-to> */ \
  _fmt(OP_ENVELOPE_EDIT_REPLY_TO,                 N_("Edit the 'Reply-to:' address")) \
  /* L10N: Help for Compose Envelope Function: <edit-subject> */ \
  _fmt(OP_ENVELOPE_EDIT_SUBJECT,                  N_("Edit the 'Subject:' of this message")) \
  /* L10N: Help for Compose Envelope Function: <edit-to> */ \
  _fmt(OP_ENVELOPE_EDIT_TO,                       N_("Edit the 'To:' address list")) \
  /* L10N: Help for Compose Envelope Function: <edit-x-comment-to> */ \
  _fmt(OP_ENVELOPE_EDIT_X_COMMENT_TO,             N_("Edit the 'X-Comment-To:' field (NNTP only)")) \
  /* L10N: Help for Compose Envelope Function: <view-pgp-options> */ \
  _fmt(OP_COMPOSE_VIEW_PGP_OPTIONS,               N_("Show PGP options")) \
  /* L10N: Help for Compose Envelope Function: <view-smime-options> */ \
  _fmt(OP_COMPOSE_VIEW_SMIME_OPTIONS,             N_("Show S/MIME options")) \
  /* L10N: Help for Compose Envelope Function: <view-autocrypt-options> */ \
  _fmt(OP_COMPOSE_VIEW_AUTOCRYPT_OPTIONS,         N_("Show Autocrypt options")) \

// Compose Preview Functions - Handled by preview_function_dispatcher()
#define OPS_COMPOSE_PREVIEW(_fmt) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-end> */ \
  _fmt(OP_PREVIEW_SCROLL_END,                     N_("Scroll to the bottom in the preview")) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-half-down> */ \
  _fmt(OP_PREVIEW_SCROLL_HALF_DOWN,               N_("Scroll down half a page in the preview")) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-half-up> */ \
  _fmt(OP_PREVIEW_SCROLL_HALF_UP,                 N_("Scroll up half a page in the preview")) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-home> */ \
  _fmt(OP_PREVIEW_SCROLL_HOME,                    N_("Scroll to the top in the preview")) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-line-down> */ \
  _fmt(OP_PREVIEW_SCROLL_LINE_DOWN,               N_("Scroll down one line in the preview")) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-line-up> */ \
  _fmt(OP_PREVIEW_SCROLL_LINE_UP,                 N_("Scroll up one line in the preview")) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-page-down> */ \
  _fmt(OP_PREVIEW_SCROLL_PAGE_DOWN,               N_("Scroll down one page in the preview")) \
  /* L10N: Help for Compose Preview Function: <preview-scroll-page-up> */ \
  _fmt(OP_PREVIEW_SCROLL_PAGE_UP,                 N_("Scroll up one page in the preview")) \

// Editor Functions - Handled by enter_function_dispatcher()
#define OPS_EDITOR(_fmt) \
  /* L10N: Help for Editor Function: <backspace> */ \
  _fmt(OP_EDITOR_BACKSPACE,                       N_("Delete the char in front of the cursor")) \
  /* L10N: Help for Editor Function: <backward-char> */ \
  _fmt(OP_EDITOR_BACKWARD_CHAR,                   N_("Move the cursor one character to the left")) \
  /* L10N: Help for Editor Function: <backward-word> */ \
  _fmt(OP_EDITOR_BACKWARD_WORD,                   N_("Move the cursor to the beginning of the word")) \
  /* L10N: Help for Editor Function: <bol> */ \
  _fmt(OP_EDITOR_BOL,                             N_("Move the cursor to the beginning of the line")) \
  /* L10N: Help for Editor Function: <capitalize-word> */ \
  _fmt(OP_EDITOR_CAPITALIZE_WORD,                 N_("Capitalize the word in front of the cursor")) \
  /* L10N: Help for Editor Function: <complete> */ \
  _fmt(OP_EDITOR_COMPLETE,                        N_("Auto-complete (general): aliases, mailboxes, files/dir")) \
  /* L10N: Help for Editor Function: <complete-mailbox> */ \
  _fmt(OP_EDITOR_COMPLETE_MAILBOX,                N_("Auto-complete from mailboxes with new mail")) \
  /* L10N: Help for Editor Function: <complete-query> */ \
  _fmt(OP_EDITOR_COMPLETE_QUERY,                  N_("Auto-complete from alias query (external command)")) \
  /* L10N: Help for Editor Function: <delete-char> */ \
  _fmt(OP_EDITOR_DELETE_CHAR,                     N_("Delete the char under the cursor")) \
  /* L10N: Help for Editor Function: <display-help> */ \
  _fmt(OP_EDITOR_DISPLAY_HELP,                    N_("Display the help screen")) \
  /* L10N: Help for Editor Function: <downcase-word> */ \
  _fmt(OP_EDITOR_DOWNCASE_WORD,                   N_("Convert the word in front of the cursor to lower case")) \
  /* L10N: Help for Editor Function: <eol> */ \
  _fmt(OP_EDITOR_EOL,                             N_("Move the cursor to the end of the line")) \
  /* L10N: Help for Editor Function: <forward-char> */ \
  _fmt(OP_EDITOR_FORWARD_CHAR,                    N_("Move the cursor one character to the right")) \
  /* L10N: Help for Editor Function: <forward-word> */ \
  _fmt(OP_EDITOR_FORWARD_WORD,                    N_("Move the cursor to the end of the word")) \
  /* L10N: Help for Editor Function: <kill-eol> */ \
  _fmt(OP_EDITOR_KILL_EOL,                        N_("Delete chars from the cursor to end of line")) \
  /* L10N: Help for Editor Function: <kill-eow> */ \
  _fmt(OP_EDITOR_KILL_EOW,                        N_("Delete chars from the cursor to the end of the word")) \
  /* L10N: Help for Editor Function: <kill-line> */ \
  _fmt(OP_EDITOR_KILL_LINE,                       N_("Delete chars from the cursor to beginning the line")) \
  /* L10N: Help for Editor Function: <kill-whole-line> */ \
  _fmt(OP_EDITOR_KILL_WHOLE_LINE,                 N_("Delete all chars on the line")) \
  /* L10N: Help for Editor Function: <kill-word> */ \
  _fmt(OP_EDITOR_KILL_WORD,                       N_("Delete the word in front of the cursor")) \
  /* L10N: Help for Editor Function: <quote-char> */ \
  _fmt(OP_EDITOR_QUOTE_CHAR,                      N_("Quote the next typed key")) \
  /* L10N: Help for Editor Function: <redraw-screen> */ \
  _fmt(OP_EDITOR_REDRAW_SCREEN,                   N_("Clear and redraw the screen")) \
  /* L10N: Help for Editor Function: <transpose-chars> */ \
  _fmt(OP_EDITOR_TRANSPOSE_CHARS,                 N_("Transpose character under cursor with the previous one")) \
  /* L10N: Help for Editor Function: <upcase-word> */ \
  _fmt(OP_EDITOR_UPCASE_WORD,                     N_("Convert the word in front of the cursor to upper case")) \
  /* L10N: Help for Editor Function: <history-search> */ \
  _fmt(OP_HISTORY_SEARCH,                         N_("View the history list")) \
  /* L10N: Help for Editor Function: <history-select-next-entry> */ \
  _fmt(OP_HISTORY_SELECT_NEXT_ENTRY,              N_("Select next newer entry in history list")) \
  /* L10N: Help for Editor Function: <history-select-previous-entry> */ \
  _fmt(OP_HISTORY_SELECT_PREVIOUS_ENTRY,          N_("Select next older entry in history list")) \

// Fuzzy Functions - Handled by sb_fuzzy_function_dispatcher()
#define OPS_FUZZY(_fmt) \
  /* L10N: Help for Fuzzy Function: <scroll-page-down> */ \
  _fmt(OP_FUZ_SCROLL_PAGE_DOWN,                   N_("Scroll down one page")) \
  /* L10N: Help for Fuzzy Function: <scroll-page-up> */ \
  _fmt(OP_FUZ_SCROLL_PAGE_UP,                     N_("Scroll up one page")) \
  /* L10N: Help for Fuzzy Function: <select-first-entry> */ \
  _fmt(OP_FUZ_SELECT_FIRST_ENTRY,                 N_("Select the first entry")) \
  /* L10N: Help for Fuzzy Function: <select-last-entry> */ \
  _fmt(OP_FUZ_SELECT_LAST_ENTRY,                  N_("Select the last entry")) \
  /* L10N: Help for Fuzzy Function: <select-next-entry> */ \
  _fmt(OP_FUZ_SELECT_NEXT_ENTRY,                  N_("Select the next entry")) \
  /* L10N: Help for Fuzzy Function: <select-previous-entry> */ \
  _fmt(OP_FUZ_SELECT_PREVIOUS_ENTRY,              N_("Select the previous entry")) \

// Generic Functions
// Generic Dialog Functions - Handled by menu_function_dispatcher()
#define OPS_GENERIC_DIALOG(_fmt) \
  /* L10N: Help for Generic Dialog Function: <exit> */ \
  _fmt(OP_EXIT,                                   N_("Exit a menu")) \
  /* L10N: Help for Generic Dialog Function: <quit> */ \
  _fmt(OP_QUIT,                                   N_("Save changes and quit")) \

// Generic Entry Functions - Handled by menu_function_dispatcher()
#define OPS_GENERIC_ENTRY(_fmt) \
  /* L10N: Help for Generic Entry Function: <pipe-entry> */ \
  _fmt(OP_PIPE_ENTRY,                             N_("Pipe the current entry to a shell command")) \
  /* L10N: Help for Generic Entry Function: <print-entry> */ \
  _fmt(OP_PRINT_ENTRY,                            N_("Print the current entry")) \

// Generic Global Functions - Handled by global_function_dispatcher()
#define OPS_GENERIC_GLOBAL(_fmt) \
  /* L10N: Help for Generic Global Function: <check-stats> */ \
  _fmt(OP_CHECK_STATS,                            N_("Calculate message statistics for all mailboxes")) \
  /* L10N: Help for Generic Global Function: <display-help> */ \
  _fmt(OP_DISPLAY_HELP,                           N_("Display the help screen")) \
  /* L10N: Help for Generic Global Function: <display-log> */ \
  _fmt(OP_DISPLAY_LOG,                            N_("Display log and debug messages")) \
  /* L10N: Help for Generic Global Function: <forget-passphrase> */ \
  _fmt(OP_FORGET_PASSPHRASE,                      N_("Wipe passphrases from memory")) \
  /* L10N: Help for Generic Global Function: <redraw-screen> */ \
  _fmt(OP_REDRAW_SCREEN,                          N_("Clear and redraw the screen")) \
  /* L10N: Help for Generic Global Function: <run-command> */ \
  _fmt(OP_RUN_COMMAND,                            N_("Execute a NeoMutt command")) \
  /* L10N: Help for Generic Global Function: <run-shell-command> */ \
  _fmt(OP_RUN_SHELL_COMMAND,                      N_("Execute external command in a subshell")) \
  /* L10N: Help for Generic Global Function: <show-version> */ \
  _fmt(OP_SHOW_VERSION,                           N_("Show the NeoMutt version number and date")) \
  /* L10N: Help for Generic Global Function: <view-keycodes> */ \
  _fmt(OP_VIEW_KEYCODES,                          N_("Show the keycodes for key presses")) \

// Generic Scrolling Functions - Handled by menu_function_dispatcher()
#define OPS_GENERIC_SCROLL(_fmt) \
  /* L10N: Help for Generic Scrolling Function: <scroll-end> */ \
  _fmt(OP_SCROLL_END,                             N_("Scroll to the bottom")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-half-down> */ \
  _fmt(OP_SCROLL_HALF_DOWN,                       N_("Scroll down half a page")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-half-up> */ \
  _fmt(OP_SCROLL_HALF_UP,                         N_("Scroll up half a page")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-home> */ \
  _fmt(OP_SCROLL_HOME,                            N_("Scroll to the top")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-line-down> */ \
  _fmt(OP_SCROLL_LINE_DOWN,                       N_("Scroll down one line")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-line-up> */ \
  _fmt(OP_SCROLL_LINE_UP,                         N_("Scroll up one line")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-page-down> */ \
  _fmt(OP_SCROLL_PAGE_DOWN,                       N_("Scroll down one page")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-page-up> */ \
  _fmt(OP_SCROLL_PAGE_UP,                         N_("Scroll up one page")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-selection-to-bottom> */ \
  _fmt(OP_SCROLL_SELECTION_TO_BOTTOM,             N_("Scroll the selection to the bottom of the page")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-selection-to-middle> */ \
  _fmt(OP_SCROLL_SELECTION_TO_MIDDLE,             N_("Scroll the selection to the middle of the page")) \
  /* L10N: Help for Generic Scrolling Function: <scroll-selection-to-top> */ \
  _fmt(OP_SCROLL_SELECTION_TO_TOP,                N_("Scroll the selection to the top of the page")) \

// Generic Searching Functions - Handled by menu_function_dispatcher()
#define OPS_GENERIC_SEARCH(_fmt) \
  /* L10N: Help for Generic Searching Function: <search-backward> */ \
  _fmt(OP_SEARCH_BACKWARD,                        N_("Search backward for a regular expression")) \
  /* L10N: Help for Generic Searching Function: <search-forward> */ \
  _fmt(OP_SEARCH_FORWARD,                         N_("Search forward for a regular expression")) \
  /* L10N: Help for Generic Searching Function: <search-next> */ \
  _fmt(OP_SEARCH_NEXT,                            N_("Search for next match")) \
  /* L10N: Help for Generic Searching Function: <search-previous> */ \
  _fmt(OP_SEARCH_PREVIOUS,                        N_("Search for next match in opposite direction")) \

// Generic Selection Functions - Handled by menu_function_dispatcher()
#define OPS_GENERIC_SELECT(_fmt) \
  /* L10N: Help for Generic Selection Function: <activate-entry> */ \
  _fmt(OP_ACTIVATE_ENTRY,                         N_("Activate the current entry")) \
  /* L10N: Help for Generic Selection Function: <select-entry-by-number> */ \
  _fmt(OP_SELECT_ENTRY_BY_NUMBER,                 N_("Select an entry by its index number")) \
  /* L10N: Help for Generic Selection Function: <select-first-entry> */ \
  _fmt(OP_SELECT_FIRST_ENTRY,                     N_("Select the first entry")) \
  /* L10N: Help for Generic Selection Function: <select-last-entry> */ \
  _fmt(OP_SELECT_LAST_ENTRY,                      N_("Select the last entry")) \
  /* L10N: Help for Generic Selection Function: <select-next-entry> */ \
  _fmt(OP_SELECT_NEXT_ENTRY,                      N_("Select the next entry")) \
  /* L10N: Help for Generic Selection Function: <select-page-bottom> */ \
  _fmt(OP_SELECT_PAGE_BOTTOM,                     N_("Select the entry at the bottom of the page")) \
  /* L10N: Help for Generic Selection Function: <select-page-middle> */ \
  _fmt(OP_SELECT_PAGE_MIDDLE,                     N_("Select the entry in the middle of the page")) \
  /* L10N: Help for Generic Selection Function: <select-page-top> */ \
  _fmt(OP_SELECT_PAGE_TOP,                        N_("Select the entry at the top of the page")) \
  /* L10N: Help for Generic Selection Function: <select-previous-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_ENTRY,                  N_("Select the previous entry")) \

// Generic Tagging Functions - Handled by dialog's function dispatcher
#define OPS_GENERIC_TAG(_fmt) \
  /* L10N: Help for Generic Tagging Function: <apply-to-tagged> */ \
  _fmt(OP_APPLY_TO_TAGGED,                        N_("Apply the next function to the tagged entries")) \
  /* L10N: Help for Generic Tagging Function: <apply-to-tagged-begin> */ \
  _fmt(OP_APPLY_TO_TAGGED_BEGIN,                  N_("Apply the next function to the tagged entries or skip to `<apply-to-tagged-end>` if nothing is tagged")) \
  /* L10N: Help for Generic Tagging Function: <apply-to-tagged-end> */ \
  _fmt(OP_APPLY_TO_TAGGED_END,                    N_("End marker for `<apply-to-tagged-begin>`")) \
  /* L10N: Help for Generic Tagging Function: <tag-pattern> */ \
  _fmt(OP_TAG_PATTERN,                            N_("Tag entries matching a pattern")) \
  /* L10N: Help for Generic Tagging Function: <toggle-tag> */ \
  _fmt(OP_TOGGLE_TAG,                             N_("Tag/untag the current entry")) \
  /* L10N: Help for Generic Tagging Function: <untag-pattern> */ \
  _fmt(OP_UNTAG_PATTERN,                          N_("Untag entries matching a pattern")) \

// Generic Tree Functions - Handled by dialog's function dispatcher
#define OPS_GENERIC_TREE(_fmt) \
  /* L10N: Help for Generic Tree Function: <fold-all-trees> */ \
  _fmt(OP_FOLD_ALL_TREES,                         N_("Collapse all trees")) \
  /* L10N: Help for Generic Tree Function: <fold-tree> */ \
  _fmt(OP_FOLD_TREE,                              N_("Collapse the current tree")) \
  /* L10N: Help for Generic Tree Function: <select-next-subtree> */ \
  _fmt(OP_SELECT_NEXT_SUBTREE,                    N_("Select the next subtree")) \
  /* L10N: Help for Generic Tree Function: <select-next-tree> */ \
  _fmt(OP_SELECT_NEXT_TREE,                       N_("Select the next tree")) \
  /* L10N: Help for Generic Tree Function: <select-previous-subtree> */ \
  _fmt(OP_SELECT_PREVIOUS_SUBTREE,                N_("Select the previous subtree")) \
  /* L10N: Help for Generic Tree Function: <select-previous-tree> */ \
  _fmt(OP_SELECT_PREVIOUS_TREE,                   N_("Select the previous tree")) \
  /* L10N: Help for Generic Tree Function: <select-tree-parent-entry> */ \
  _fmt(OP_SELECT_TREE_PARENT_ENTRY,               N_("Select the parent entry in the tree")) \
  /* L10N: Help for Generic Tree Function: <select-tree-root-entry> */ \
  _fmt(OP_SELECT_TREE_ROOT_ENTRY,                 N_("Select the root entry of the tree")) \
  /* L10N: Help for Generic Tree Function: <toggle-all-trees> */ \
  _fmt(OP_TOGGLE_ALL_TREES,                       N_("Collapse/expand all trees")) \
  /* L10N: Help for Generic Tree Function: <toggle-tag-subtree> */ \
  _fmt(OP_TOGGLE_TAG_SUBTREE,                     N_("Tag/untag the current subtree")) \
  /* L10N: Help for Generic Tree Function: <toggle-tag-tree> */ \
  _fmt(OP_TOGGLE_TAG_TREE,                        N_("Tag/untag the current tree")) \
  /* L10N: Help for Generic Tree Function: <toggle-tree> */ \
  _fmt(OP_TOGGLE_TREE,                            N_("Collapse/expand the current tree")) \
  /* L10N: Help for Generic Tree Function: <unfold-all-trees> */ \
  _fmt(OP_UNFOLD_ALL_TREES,                       N_("Expand all trees")) \
  /* L10N: Help for Generic Tree Function: <unfold-tree> */ \
  _fmt(OP_UNFOLD_TREE,                            N_("Expand the current tree")) \

// Generic View Functions - Handled by dialog's function dispatcher
#define OPS_GENERIC_VIEW(_fmt) \
  /* L10N: Help for Generic View Function: <limit-entries> */ \
  _fmt(OP_LIMIT_ENTRIES,                          N_("Show only entries matching a pattern")) \
  /* L10N: Help for Generic View Function: <show-limit> */ \
  _fmt(OP_SHOW_LIMIT,                             N_("Show the currently active limit pattern")) \
  /* L10N: Help for Generic View Function: <sort-entries> */ \
  _fmt(OP_SORT_ENTRIES,                           N_("Sort entries")) \
  /* L10N: Help for Generic View Function: <sort-entries-reverse> */ \
  _fmt(OP_SORT_ENTRIES_REVERSE,                   N_("Sort entries in reverse order")) \

// Index Functions - Handled by index_function_dispatcher()
#define OPS_INDEX(_fmt) \
  /* L10N: Help for Index Function: <bounce-message> */ \
  _fmt(OP_IND_BOUNCE_MESSAGE,                     N_("Remail a message to another user")) \
  /* L10N: Help for Index Function: <break-thread> */ \
  _fmt(OP_MAIN_BREAK_THREAD,                      N_("Break the thread in two, at the current message")) \
  /* L10N: Help for Index Function: <browse-mailboxes> */ \
  _fmt(OP_MAIN_BROWSE_MAILBOXES,                  N_("Browse mailboxes")) \
  /* L10N: Help for Index Function: <browse-mailboxes-readonly> */ \
  _fmt(OP_MAIN_BROWSE_MAILBOXES_READONLY,         N_("Browse mailboxes in read only mode")) \
  /* L10N: Help for Index Function: <change-folder> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER,                     N_("Open a different folder")) \
  /* L10N: Help for Index Function: <change-folder-readonly> */ \
  _fmt(OP_MAIN_CHANGE_FOLDER_READONLY,            N_("Open a different folder in read only mode")) \
  /* L10N: Help for Index Function: <change-newsgroup> */ \
  _fmt(OP_MAIN_CHANGE_GROUP,                      N_("Open a different newsgroup")) \
  /* L10N: Help for Index Function: <change-newsgroup-readonly> */ \
  _fmt(OP_MAIN_CHANGE_GROUP_READONLY,             N_("Open a different newsgroup in read only mode")) \
  /* L10N: Help for Index Function: <check-traditional-pgp> */ \
  _fmt(OP_IND_CHECK_TRADITIONAL,                  N_("Check for classic PGP")) \
  /* L10N: Help for Index Function: <compose-message> */ \
  _fmt(OP_IND_COMPOSE_MESSAGE,                    N_("Compose a new mail message")) \
  /* L10N: Help for Index Function: <compose-to-sender> */ \
  _fmt(OP_IND_COMPOSE_TO_SENDER,                  N_("Compose new message to the current message sender")) \
  /* L10N: Help for Index Function: <copy-message> */ \
  _fmt(OP_COPY_MESSAGE,                           N_("Copy the message to a mailbox/file")) \
  /* L10N: Help for Index Function: <copy-message-decoded> */ \
  _fmt(OP_COPY_MESSAGE_DECODED,                   N_("Copy the decoded (text/plain) message to a mailbox/filei")) \
  /* L10N: Help for Index Function: <copy-message-decrypted> */ \
  _fmt(OP_COPY_MESSAGE_DECRYPTED,                 N_("Copy the decrypted message to a mailbox/file")) \
  /* L10N: Help for Index Function: <create-alias> */ \
  _fmt(OP_IND_CREATE_ALIAS,                       N_("Create an alias from the message sender")) \
  /* L10N: Help for Index Function: <create-message-hotkey> */ \
  _fmt(OP_CREATE_MESSAGE_HOTKEY,                  N_("Create a hotkey macro for the current message")) \
  /* L10N: Help for Index Function: <delete-message> */ \
  _fmt(OP_IND_DELETE_MESSAGE,                     N_("Delete the current message")) \
  /* L10N: Help for Index Function: <delete-pattern> */ \
  _fmt(OP_MAIN_DELETE_PATTERN,                    N_("Delete non-hidden messages matching a pattern")) \
  /* L10N: Help for Index Function: <delete-subthread> */ \
  _fmt(OP_DELETE_SUBTHREAD,                       N_("Delete all messages in subthread")) \
  /* L10N: Help for Index Function: <delete-thread> */ \
  _fmt(OP_DELETE_THREAD,                          N_("Delete all messages in thread")) \
  /* L10N: Help for Index Function: <display-message> */ \
  _fmt(OP_DISPLAY_MESSAGE,                        N_("Display the current message and toggle ignored headers")) \
  /* L10N: Help for Index Function: <display-message-headers> */ \
  _fmt(OP_IND_DISPLAY_MESSAGE_HEADERS,            N_("Unset a message flag, e.g. important, new, replied")) \
  /* L10N: Help for Index Function: <edit-content-type> */ \
  _fmt(OP_IND_ATTACH_EDIT_CONTENT_TYPE,           N_("Edit the 'Content-Type:' of the current message")) \
  /* L10N: Help for Index Function: <edit-raw-message> */ \
  _fmt(OP_EDIT_RAW_MESSAGE,                       N_("Edit the raw message")) \
  /* L10N: Help for Index Function: <edit-raw-message-readonly> */ \
  _fmt(OP_EDIT_RAW_MESSAGE_READONLY,              N_("Open the raw message read-only in an editor")) \
  /* L10N: Help for Index Function: <edit-tags> */ \
  _fmt(OP_EDIT_TAGS,                              N_("Edit tags (IMAP and Notmuch only)")) \
  /* L10N: Help for Index Function: <edit-tags-then-hide> */ \
  _fmt(OP_EDIT_TAGS_THEN_HIDE,                    N_("Edit tags and then hide the message (IMAP and Notmuch only)")) \
  /* L10N: Help for Index Function: <edit-x-label> */ \
  _fmt(OP_EDIT_X_LABEL,                           N_("Edit labels (the 'X-Label:' header)")) \
  /* L10N: Help for Index Function: <extract-keys> */ \
  _fmt(OP_IND_EXTRACT_KEYS,                       N_("Extract keys/certificates from the message and add them to the store")) \
  /* L10N: Help for Index Function: <forward-message> */ \
  _fmt(OP_IND_FORWARD_MESSAGE,                    N_("Forward a message with comments")) \
  /* L10N: Help for Index Function: <imap-fetch-mail> */ \
  _fmt(OP_MAIN_IMAP_FETCH,                        N_("Force retrieval of mail from IMAP server")) \
  /* L10N: Help for Index Function: <imap-logout-all> */ \
  _fmt(OP_MAIN_IMAP_LOGOUT_ALL,                   N_("Logout from all IMAP servers")) \
  /* L10N: Help for Index Function: <limit-thread> */ \
  _fmt(OP_LIMIT_THREAD,                           N_("Show only messages belonging to the current thread")) \
  /* L10N: Help for Index Function: <link-threads> */ \
  _fmt(OP_MAIN_LINK_THREADS,                      N_("Make the tagged messages children of the current message")) \
  /* L10N: Help for Index Function: <list-reply> */ \
  _fmt(OP_IND_LIST_REPLY,                         N_("Reply to specified mailing list")) \
  /* L10N: Help for Index Function: <list-subscribe> */ \
  _fmt(OP_IND_LIST_SUBSCRIBE,                     N_("Subscribe to a mailing list")) \
  /* L10N: Help for Index Function: <list-unsubscribe> */ \
  _fmt(OP_IND_LIST_UNSUBSCRIBE,                   N_("Unsubscribe from a mailing list")) \
  /* L10N: Help for Index Function: <mark-subthread-read> */ \
  _fmt(OP_MARK_SUBTHREAD_READ,                    N_("Mark the current subthread as read")) \
  /* L10N: Help for Index Function: <mark-thread-read> */ \
  _fmt(OP_MARK_THREAD_READ,                       N_("Mark the current thread as read")) \
  /* L10N: Help for Index Function: <move-message> */ \
  _fmt(OP_MOVE_MESSAGE,                           N_("Move the message to a mailbox/file")) \
  /* L10N: Help for Index Function: <move-message-decoded> */ \
  _fmt(OP_MOVE_MESSAGE_DECODED,                   N_("Move the decoded (text/plain) message to a mailbox/file")) \
  /* L10N: Help for Index Function: <move-message-decrypted> */ \
  _fmt(OP_MOVE_MESSAGE_DECRYPTED,                 N_("Move the decrypted message to a mailbox/file")) \
  /* L10N: Help for Index Function: <nntp-followup-message> */ \
  _fmt(OP_IND_NNTP_FOLLOWUP_MESSAGE,              N_("Followup to newsgroup")) \
  /* L10N: Help for Index Function: <nntp-forward-to-group> */ \
  _fmt(OP_IND_NNTP_FORWARD_TO_GROUP,              N_("Forward to newsgroup")) \
  /* L10N: Help for Index Function: <nntp-get-children> */ \
  _fmt(OP_NNTP_GET_CHILDREN,                      N_("Get all children of the current message")) \
  /* L10N: Help for Index Function: <nntp-get-message> */ \
  _fmt(OP_NNTP_GET_MESSAGE,                       N_("Get message with 'Message-ID:'")) \
  /* L10N: Help for Index Function: <nntp-get-parent> */ \
  _fmt(OP_NNTP_GET_PARENT,                        N_("Get parent of the current message")) \
  /* L10N: Help for Index Function: <nntp-mark-newsgroup-read> */ \
  _fmt(OP_IND_NNTP_MARK_NEWSGROUP_READ,           N_("Mark all articles in newsgroup as read")) \
  /* L10N: Help for Index Function: <nntp-post-message> */ \
  _fmt(OP_NNTP_POST_MESSAGE,                      N_("Post message to newsgroup")) \
  /* L10N: Help for Index Function: <nntp-reconstruct-thread> */ \
  _fmt(OP_NNTP_RECONSTRUCT_THREAD,                N_("Reconstruct thread containing current message")) \
  /* L10N: Help for Index Function: <pop-fetch-mail> */ \
  _fmt(OP_POP_FETCH_MAIL,                         N_("Retrieve mail from POP server")) \
  /* L10N: Help for Index Function: <purge-message> */ \
  _fmt(OP_PURGE_MESSAGE,                          N_("Delete the current entry, bypassing the trash folder")) \
  /* L10N: Help for Index function: <purge-pattern> */ \
  _fmt(OP_PURGE_PATTERN,                          N_("Delete non-hidden messages matching a pattern, bypassing the trash folder")) \
  /* L10N: Help for Index function: <purge-subthread> */ \
  _fmt(OP_PURGE_SUBTHREAD,                        N_("Delete all messages in subthread, bypassing the trash folder")) \
  /* L10N: Help for Index Function: <purge-thread> */ \
  _fmt(OP_PURGE_THREAD,                           N_("Delete the current thread, bypassing the trash folder")) \
  /* L10N: Help for Index Function: <quasi-delete-message> */ \
  _fmt(OP_QUASI_DELETE_MESSAGE,                   N_("Delete from NeoMutt, don't touch on disk")) \
  /* L10N: Help for Index Function: <recall-draft-message> */ \
  _fmt(OP_RECALL_DRAFT_MESSAGE,                   N_("Recall a postponed message")) \
  /* L10N: Help for Index Function: <reply-all> */ \
  _fmt(OP_IND_REPLY_ALL,                          N_("Reply to all recipients")) \
  /* L10N: Help for Index Function: <reply-group-chat> */ \
  _fmt(OP_IND_REPLY_GROUP_CHAT,                   N_("Reply to all recipients preserving To/Cc")) \
  /* L10N: Help for Index Function: <reply-sender> */ \
  _fmt(OP_IND_REPLY_SENDER,                       N_("Reply to a message")) \
  /* L10N: Help for Index Function: <resend-message> */ \
  _fmt(OP_IND_RESEND,                             N_("Use the current message as a template for a new one")) \
  /* L10N: Help for Index Function: <select-next-new-entry> */ \
  _fmt(OP_SELECT_NEXT_NEW_ENTRY,                  N_("Select the next new message")) \
  /* L10N: Help for Index Function: <select-next-new-or-unread-entry> */ \
  _fmt(OP_SELECT_NEXT_NEW_OR_UNREAD_ENTRY,        N_("Select the next new or unread message")) \
  /* L10N: Help for Index Function: <select-next-undeleted-entry> */ \
  _fmt(OP_SELECT_NEXT_UNDELETED_ENTRY,            N_("Select the next undeleted message")) \
  /* L10N: Help for Index Function: <select-next-unread-entry> */ \
  _fmt(OP_SELECT_NEXT_UNREAD_ENTRY,               N_("Select the next unread message")) \
  /* L10N: Help for Index Function: <select-next-unread-mailbox> */ \
  _fmt(OP_SELECT_NEXT_UNREAD_MAILBOX,             N_("Open next mailbox with new mail")) \
  /* L10N: Help for Index Function: <select-previous-new-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_NEW_ENTRY,              N_("Select the previous new message")) \
  /* L10N: Help for Index Function: <select-previous-new-or-unread-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_NEW_OR_UNREAD_ENTRY,    N_("Select the previous new or unread message")) \
  /* L10N: Help for Index Function: <select-previous-undeleted-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_UNDELETED_ENTRY,        N_("Select the previous undeleted message")) \
  /* L10N: Help for Index Function: <select-previous-unread-entry> */ \
  _fmt(OP_SELECT_PREVIOUS_UNREAD,                 N_("Select the previous unread message")) \
  /* L10N: Help for Index Function: <select-previous-unread-mailbox> */ \
  _fmt(OP_SELECT_PREVIOUS_UNREAD_MAILBOX,         N_("Open previous mailbox with new mail")) \
  /* L10N: Help for Index Function: <send-pgp-key> */ \
  _fmt(OP_SEND_PGP_KEY,                           N_("Mail a PGP public key")) \
  /* L10N: Help for Index Function: <set-flag> */ \
  _fmt(OP_MAIN_SET_FLAG,                          N_("Set a message flag, e.g. important, new, replied")) \
  /* L10N: Help for Index Function: <show-mailboxes> */ \
  _fmt(OP_IND_SHOW_MAILBOXES,                     N_("Show list of mailboxes with new mail")) \
  /* L10N: Help for Index Function: <show-sender-address> */ \
  _fmt(OP_SHOW_SENDER_ADDRESS,                    N_("Show the address of the sender")) \
  /* L10N: Help for Index Function: <sync-mailbox> */ \
  _fmt(OP_MAIN_SYNC_FOLDER,                       N_("Save changes to mailbox")) \
  /* L10N: Help for Index Function: <toggle-important-flag> */ \
  _fmt(OP_TOGGLE_IMPORTANT_FLAG,                  N_("Toggle the 'important' flag of the current message")) \
  /* L10N: Help for Index Function: <toggle-mailbox-readonly> */ \
  _fmt(OP_TOGGLE_MAILBOX_READONLY,                N_("Toggle whether the mailbox will be rewritten")) \
  /* L10N: Help for Index Function: <toggle-new-flag> */ \
  _fmt(OP_TOGGLE_NEW_FLAG,                        N_("Toggle the 'new' flag of the current message")) \
  /* L10N: Help for Index Function: <toggle-read-messages> */ \
  _fmt(OP_TOGGLE_READ_MESSAGES,                   N_("Show/hide read messages (adjusts the limit pattern)")) \
  /* L10N: Help for Index Function: <undelete-message> */ \
  _fmt(OP_IND_UNDELETE_MESSAGE,                   N_("Undelete the current message")) \
  /* L10N: Help for Index Function: <undelete-pattern> */ \
  _fmt(OP_MAIN_UNDELETE_PATTERN,                  N_("Undelete non-hidden messages matching a pattern")) \
  /* L10N: Help for Index Function: <undelete-subthread> */ \
  _fmt(OP_UNDELETE_SUBTHREAD,                     N_("Undelete all messages in subthread")) \
  /* L10N: Help for Index Function: <undelete-thread> */ \
  _fmt(OP_UNDELETE_THREAD,                        N_("Undelete all messages in thread")) \
  /* L10N: Help for Index Function: <unset-flag> */ \
  _fmt(OP_UNSET_FLAG,                             N_("Unset a message flag, e.g. important, new, replied")) \
  /* L10N: Help for Index Function: <view-address-query> */ \
  _fmt(OP_IND_VIEW_ADDRESS_QUERY,                 N_("Look up contacts in an external address book")) \
  /* L10N: Help for Index Function: <view-aliases> */ \
  _fmt(OP_VIEW_ALIASES,                           N_("Lookup aliases in the (built-in) address book")) \
  /* L10N: Help for Index Function: <view-attachments> */ \
  _fmt(OP_VIEW_ATTACHMENTS,                       N_("List the attachments of the current message")) \
  /* L10N: Help for Index Function: <view-autocrypt-accounts> */ \
  _fmt(OP_VIEW_AUTOCRYPT_ACCOUNTS,                N_("Manage Autocrypt accounts")) \
  /* L10N: Help for Index Function: <view-list-actions> */ \
  _fmt(OP_VIEW_LIST_ACTIONS,                      N_("Perform mailing list action")) \

// Index Notmuch Functions - Handled by index_function_dispatcher()
#ifdef USE_NOTMUCH
#define OPS_INDEX_NOTMUCH(_fmt) \
  /* L10N: Help for Index Notmuch Function: <fetch-entire-thread> */ \
  _fmt(OP_FETCH_ENTIRE_THREAD,                    N_("Read entire thread of the current message")) \
  /* L10N: Help for Index Notmuch Function: <vfolder-create-from-query> */ \
  _fmt(OP_VFOLDER_CREATE_FROM_QUERY,              N_("Generate a virtual folder from a query")) \
  /* L10N: Help for Index Notmuch Function: <vfolder-create-from-query-readonly> */ \
  _fmt(OP_VFOLDER_CREATE_FROM_QUERY_READONLY,     N_("Generate a read-only virtual folder from a query")) \
  /* L10N: Help for Index Notmuch Function: <vfolder-reset-window> */ \
  _fmt(OP_VFOLDER_RESET_WINDOW,                   N_("Reset the time window of the virtual folder to the present")) \
  /* L10N: Help for Index Notmuch Function: <vfolder-shift-window-back> */ \
  _fmt(OP_VFOLDER_SHIFT_WINDOW_BACK,              N_("Shift the time window of the virtual folder backwards")) \
  /* L10N: Help for Index Notmuch Function: <vfolder-shift-window-forward> */ \
  _fmt(OP_VFOLDER_SHIFT_WINDOW_FORWARD,           N_("Shift the time window of the virtual folder forwards"))
#else
#define OPS_INDEX_NOTMUCH(_)
#endif

// List Functions - Handled by mlist_function_dispatcher()
#define OPS_LIST(_fmt) \
  /* L10N: Help for List Function: <list-archive> */ \
  _fmt(OP_LIST_ARCHIVE,                           N_("Retrieve list archive information")) \
  /* L10N: Help for List Function: <list-help> */ \
  _fmt(OP_LIST_HELP,                              N_("Retrieve list help")) \
  /* L10N: Help for List Function: <list-owner> */ \
  _fmt(OP_LIST_OWNER,                             N_("Contact list owner")) \
  /* L10N: Help for List Function: <list-post> */ \
  _fmt(OP_LIST_POST,                              N_("Post to mailing list")) \
  /* L10N: Help for List Function: <list-subscribe> */ \
  _fmt(OP_LIS_LIST_SUBSCRIBE,                     N_("Subscribe to a mailing list")) \
  /* L10N: Help for List Function: <list-unsubscribe> */ \
  _fmt(OP_LIS_LIST_UNSUBSCRIBE,                   N_("Unsubscribe from a mailing list")) \

// Pager Functions - Handled by pager_function_dispatcher()
#define OPS_PAGER(_fmt) \
  /* L10N: Help for Pager Function: <skip-headers> */ \
  _fmt(OP_PAGER_SKIP_HEADERS,                     N_("Scroll to first line after the headers")) \
  /* L10N: Help for Pager Function: <skip-quoted-text> */ \
  _fmt(OP_PAGER_SKIP_QUOTED_TEXT,                 N_("Scroll past the next quoted block")) \
  /* L10N: Help for Pager Function: <toggle-quoted-text> */ \
  _fmt(OP_PAGER_TOGGLE_QUOTED_TEXT,               N_("Show/hide quoted text")) \
  /* L10N: Help for Pager Function: <toggle-search-highlighting> */ \
  _fmt(OP_PAGER_TOGGLE_SEARCH_HIGHLIGHTING,       N_("Show/hide highlighting of search matches")) \

// Pgp Functions - Handled by pgp_function_dispatcher()
#define OPS_PGP(_fmt) \
  /* L10N: Help for Pgp Function: <display-details> */ \
  _fmt(OP_PGP_DISPLAY_DETAILS,                    N_("Display key/certificate info")) \
  /* L10N: Help for Pgp Function: <show-identity> */ \
  _fmt(OP_PGP_SHOW_IDENTITY,                      N_("Show the identity of the key/certificate")) \

// Postpone Functions - Handled by postpone_function_dispatcher()
#define OPS_POSTPONE(_fmt) \
  /* L10N: Help for Postpone Function: <delete-message> */ \
  _fmt(OP_POS_DELETE_MESSAGE,                     N_("Delete the current message")) \
  /* L10N: Help for Postpone Function: <undelete-message> */ \
  _fmt(OP_POS_UNDELETE_MESSAGE,                   N_("Undelete the current message")) \

// Query Functions - Handled by alias_function_dispatcher()
#define OPS_QUERY(_fmt) \
  /* L10N: Help for Query Function: <compose-message> */ \
  _fmt(OP_QUE_COMPOSE_MESSAGE,                    N_("Compose a new mail message to the current contact")) \
  /* L10N: Help for Query Function: <create-alias> */ \
  _fmt(OP_QUE_CREATE_ALIAS,                       N_("Create an alias from the current contact")) \
  /* L10N: Help for Query Function: <query-append> */ \
  _fmt(OP_QUERY_APPEND,                           N_("Append new query results to current results")) \
  /* L10N: Help for Query Function: <view-address-query> */ \
  _fmt(OP_QUE_VIEW_ADDRESS_QUERY,                 N_("Query external program for addresses")) \

// Sidebar Functions - Handled by sb_function_dispatcher()
#define OPS_SIDEBAR(_fmt) \
  /* L10N: Help for Sidebar Function: <sidebar-activate-entry> */ \
  _fmt(OP_SIDEBAR_ACTIVATE_ENTRY,                 N_("Open the highlighted mailbox")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-end> */ \
  _fmt(OP_SIDEBAR_SCROLL_END,                     N_("Scroll to the bottom")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-half-down> */ \
  _fmt(OP_SIDEBAR_SCROLL_HALF_DOWN,               N_("Scroll down half a page")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-half-up> */ \
  _fmt(OP_SIDEBAR_SCROLL_HALF_UP,                 N_("Scroll up half a page")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-home> */ \
  _fmt(OP_SIDEBAR_SCROLL_HOME,                    N_("Scroll to the top")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-line-down> */ \
  _fmt(OP_SIDEBAR_SCROLL_LINE_DOWN,               N_("Scroll down one line")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-line-up> */ \
  _fmt(OP_SIDEBAR_SCROLL_LINE_UP,                 N_("Scroll up one line")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-page-down> */ \
  _fmt(OP_SIDEBAR_SCROLL_PAGE_DOWN,               N_("Scroll down one page")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-page-up> */ \
  _fmt(OP_SIDEBAR_SCROLL_PAGE_UP,                 N_("Scroll up one page")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-selection-to-bottom> */ \
  _fmt(OP_SIDEBAR_SCROLL_SELECTION_TO_BOTTOM,     N_("Scroll the highlight to the bottom of the page")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-selection-to-middle> */ \
  _fmt(OP_SIDEBAR_SCROLL_SELECTION_TO_MIDDLE,     N_("Scroll the highlight to the middle of the page")) \
  /* L10N: Help for Sidebar Function: <sidebar-scroll-selection-to-top> */ \
  _fmt(OP_SIDEBAR_SCROLL_SELECTION_TO_TOP,        N_("Scroll the highlight to the top of the page")) \
  /* L10N: Help for Sidebar Function: <sidebar-search> */ \
  _fmt(OP_SIDEBAR_SEARCH,                         N_("Fuzzy search the sidebar")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-entry-by-number> */ \
  _fmt(OP_SIDEBAR_SELECT_ENTRY_BY_NUMBER,         N_("Select a mailbox by its index number")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-first-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_FIRST_ENTRY,             N_("Highlight the first mailbox")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-last-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_LAST_ENTRY,              N_("Highlight the last mailbox")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-next-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_NEXT_ENTRY,              N_("Highlight the next mailbox")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-next-new-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_NEXT_NEW_ENTRY,          N_("Highlight the next mailbox with new mail")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-page-bottom> */ \
  _fmt(OP_SIDEBAR_SELECT_PAGE_BOTTOM,             N_("Highlight the mailbox at the bottom of the page")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-page-middle> */ \
  _fmt(OP_SIDEBAR_SELECT_PAGE_MIDDLE,             N_("Highlight the mailbox in the middle of the page")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-page-top> */ \
  _fmt(OP_SIDEBAR_SELECT_PAGE_TOP,                N_("Highlight the mailbox at the top of the page")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-previous-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_PREVIOUS_ENTRY,          N_("Highlight the previous mailbox")) \
  /* L10N: Help for Sidebar Function: <sidebar-select-previous-new-entry> */ \
  _fmt(OP_SIDEBAR_SELECT_PREVIOUS_NEW_ENTRY,      N_("Highlight the previous mailbox with new mail")) \
  /* L10N: Help for Sidebar Function: <sidebar-toggle-visible> */ \
  _fmt(OP_SIDEBAR_TOGGLE_VISIBLE,                 N_("Show/hide the sidebar")) \

// Smime Functions - Handled by smime_function_dispatcher()
#define OPS_SMIME(_fmt) \
  /* L10N: Help for Smime Function: <display-details> */ \
  _fmt(OP_SMI_DISPLAY_DETAILS,                    N_("Display key/certificate info")) \
  /* L10N: Help for Smime Function: <show-identity> */ \
  _fmt(OP_SMI_SHOW_IDENTITY,                      N_("Show the identity of the key/certificate")) \

#define OPS(_fmt) \
  _fmt(OP_NULL,                                   N_("Null operation")) \
  /* L10N: Help for executing a macro */ \
  _fmt(OP_MACRO,                                  N_("Execute a macro")) \
  OPS_ALIAS(_fmt) \
  OPS_ATTACH(_fmt) \
  OPS_AUTOCRYPT(_fmt) \
  OPS_BROWSER(_fmt) \
  OPS_COMPOSE(_fmt) \
  OPS_COMPOSE_ENVELOPE(_fmt) \
  OPS_COMPOSE_PREVIEW(_fmt) \
  OPS_EDITOR(_fmt) \
  OPS_FUZZY(_fmt) \
  OPS_GENERIC_DIALOG(_fmt) \
  OPS_GENERIC_ENTRY(_fmt) \
  OPS_GENERIC_GLOBAL(_fmt) \
  OPS_GENERIC_SCROLL(_fmt) \
  OPS_GENERIC_SEARCH(_fmt) \
  OPS_GENERIC_SELECT(_fmt) \
  OPS_GENERIC_TAG(_fmt) \
  OPS_GENERIC_TREE(_fmt) \
  OPS_GENERIC_VIEW(_fmt) \
  OPS_INDEX(_fmt) \
  OPS_INDEX_NOTMUCH(_fmt) \
  OPS_LIST(_fmt) \
  OPS_PAGER(_fmt) \
  OPS_PGP(_fmt) \
  OPS_POSTPONE(_fmt) \
  OPS_QUERY(_fmt) \
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

#endif /* MUTT_GUI_OPCODES_H */
