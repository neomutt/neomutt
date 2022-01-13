/**
 * @file
 * All user-callable functions
 *
 * @authors
 * Copyright (C) 2017 Damien Riegel <damien.riegel@gmail.com>
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

// clang-format off
#ifdef USE_AUTOCRYPT
#define OPS_AUTOCRYPT(_fmt) \
  _fmt(OP_AUTOCRYPT_ACCT_MENU,            N_("manage autocrypt accounts")) \
  _fmt(OP_AUTOCRYPT_CREATE_ACCT,          N_("create a new autocrypt account")) \
  _fmt(OP_AUTOCRYPT_DELETE_ACCT,          N_("delete the current account")) \
  _fmt(OP_AUTOCRYPT_TOGGLE_ACTIVE,        N_("toggle the current account active/inactive")) \
  _fmt(OP_AUTOCRYPT_TOGGLE_PREFER,        N_("toggle the current account prefer-encrypt flag")) \
  _fmt(OP_COMPOSE_AUTOCRYPT_MENU,         N_("show autocrypt compose menu options"))
#else
#define OPS_AUTOCRYPT(_)
#endif

#define OPS_CORE(_fmt) \
  _fmt(OP_ATTACH_COLLAPSE,                N_("toggle display of subparts")) \
  _fmt(OP_ATTACH_VIEW_MAILCAP,            N_("force viewing of attachment using mailcap")) \
  _fmt(OP_ATTACH_VIEW_PAGER,              N_("view attachment in pager using copiousoutput mailcap")) \
  _fmt(OP_ATTACH_VIEW_TEXT,               N_("view attachment as text")) \
  _fmt(OP_BOTTOM_PAGE,                    N_("move to the bottom of the page")) \
  _fmt(OP_BOUNCE_MESSAGE,                 N_("remail a message to another user")) \
  _fmt(OP_BROWSER_GOTO_FOLDER,            N_("swap the current folder position with $folder if it exists")) \
  _fmt(OP_BROWSER_NEW_FILE,               N_("select a new file in this directory")) \
  _fmt(OP_BROWSER_SUBSCRIBE,              N_("subscribe to current mbox (IMAP/NNTP only)")) \
  _fmt(OP_BROWSER_TELL,                   N_("display the currently selected file's name")) \
  _fmt(OP_BROWSER_TOGGLE_LSUB,            N_("toggle view all/subscribed mailboxes (IMAP only)")) \
  _fmt(OP_BROWSER_UNSUBSCRIBE,            N_("unsubscribe from current mbox (IMAP/NNTP only)")) \
  _fmt(OP_BROWSER_VIEW_FILE,              N_("view file")) \
  _fmt(OP_CATCHUP,                        N_("mark all articles in newsgroup as read")) \
  _fmt(OP_CHANGE_DIRECTORY,               N_("change directories")) \
  _fmt(OP_CHECK_NEW,                      N_("check mailboxes for new mail")) \
  _fmt(OP_CHECK_STATS,                    N_("calculate message statistics for all mailboxes")) \
  _fmt(OP_COMPOSE_ATTACH_FILE,            N_("attach files to this message")) \
  _fmt(OP_COMPOSE_ATTACH_MESSAGE,         N_("attach messages to this message")) \
  _fmt(OP_COMPOSE_ATTACH_NEWS_MESSAGE,    N_("attach news articles to this message")) \
  _fmt(OP_COMPOSE_EDIT_BCC,               N_("edit the BCC list")) \
  _fmt(OP_COMPOSE_EDIT_CC,                N_("edit the CC list")) \
  _fmt(OP_COMPOSE_EDIT_DESCRIPTION,       N_("edit attachment description")) \
  _fmt(OP_COMPOSE_EDIT_ENCODING,          N_("edit attachment transfer-encoding")) \
  _fmt(OP_COMPOSE_EDIT_FCC,               N_("enter a file to save a copy of this message in")) \
  _fmt(OP_COMPOSE_EDIT_FILE,              N_("edit the file to be attached")) \
  _fmt(OP_COMPOSE_EDIT_FOLLOWUP_TO,       N_("edit the Followup-To field")) \
  _fmt(OP_COMPOSE_EDIT_FROM,              N_("edit the from field")) \
  _fmt(OP_COMPOSE_EDIT_HEADERS,           N_("edit the message with headers")) \
  _fmt(OP_COMPOSE_EDIT_LANGUAGE,          N_("edit the 'Content-Language' of the attachment")) \
  _fmt(OP_COMPOSE_EDIT_MESSAGE,           N_("edit the message")) \
  _fmt(OP_COMPOSE_EDIT_MIME,              N_("edit attachment using mailcap entry")) \
  _fmt(OP_COMPOSE_EDIT_NEWSGROUPS,        N_("edit the newsgroups list")) \
  _fmt(OP_COMPOSE_EDIT_REPLY_TO,          N_("edit the Reply-To field")) \
  _fmt(OP_COMPOSE_EDIT_SUBJECT,           N_("edit the subject of this message")) \
  _fmt(OP_COMPOSE_EDIT_TO,                N_("edit the TO list")) \
  _fmt(OP_COMPOSE_EDIT_X_COMMENT_TO,      N_("edit the X-Comment-To field")) \
  _fmt(OP_COMPOSE_GET_ATTACHMENT,         N_("get a temporary copy of an attachment")) \
  _fmt(OP_COMPOSE_GROUP_ALTS,             N_("group tagged attachments as 'multipart/alternative'")) \
  _fmt(OP_COMPOSE_GROUP_LINGUAL,          N_("group tagged attachments as 'multipart/multilingual'")) \
  _fmt(OP_COMPOSE_UNGROUP_ATTACHMENT,     N_("ungroup 'multipart' attachment")) \
  _fmt(OP_COMPOSE_ISPELL,                 N_("run ispell on the message")) \
  _fmt(OP_COMPOSE_MOVE_DOWN,              N_("move an attachment down in the attachment list")) \
  _fmt(OP_COMPOSE_MOVE_UP,                N_("move an attachment up in the attachment list")) \
  _fmt(OP_COMPOSE_NEW_MIME,               N_("compose new attachment using mailcap entry")) \
  _fmt(OP_COMPOSE_POSTPONE_MESSAGE,       N_("save this message to send later")) \
  _fmt(OP_COMPOSE_RENAME_ATTACHMENT,      N_("send attachment with a different name")) \
  _fmt(OP_COMPOSE_RENAME_FILE,            N_("rename/move an attached file")) \
  _fmt(OP_COMPOSE_SEND_MESSAGE,           N_("send the message")) \
  _fmt(OP_COMPOSE_TOGGLE_DISPOSITION,     N_("toggle disposition between inline/attachment")) \
  _fmt(OP_COMPOSE_TOGGLE_RECODE,          N_("toggle recoding of this attachment")) \
  _fmt(OP_COMPOSE_TOGGLE_UNLINK,          N_("toggle whether to delete file after sending it")) \
  _fmt(OP_COMPOSE_TO_SENDER,              N_("compose new message to the current message sender")) \
  _fmt(OP_COMPOSE_UPDATE_ENCODING,        N_("update an attachment's encoding info")) \
  _fmt(OP_COMPOSE_WRITE_MESSAGE,          N_("write the message to a folder")) \
  _fmt(OP_COPY_MESSAGE,                   N_("copy a message to a file/mailbox")) \
  _fmt(OP_CREATE_ALIAS,                   N_("create an alias from a message sender")) \
  _fmt(OP_CREATE_MAILBOX,                 N_("create a new mailbox (IMAP only)")) \
  _fmt(OP_CURRENT_BOTTOM,                 N_("move entry to bottom of screen")) \
  _fmt(OP_CURRENT_MIDDLE,                 N_("move entry to middle of screen")) \
  _fmt(OP_CURRENT_TOP,                    N_("move entry to top of screen")) \
  _fmt(OP_DECODE_COPY,                    N_("make decoded (text/plain) copy")) \
  _fmt(OP_DECODE_SAVE,                    N_("make decoded copy (text/plain) and delete")) \
  _fmt(OP_DELETE,                         N_("delete the current entry")) \
  _fmt(OP_DELETE_MAILBOX,                 N_("delete the current mailbox (IMAP only)")) \
  _fmt(OP_DELETE_SUBTHREAD,               N_("delete all messages in subthread")) \
  _fmt(OP_DELETE_THREAD,                  N_("delete all messages in thread")) \
  _fmt(OP_DESCEND_DIRECTORY,              N_("descend into a directory")) \
  _fmt(OP_DISPLAY_ADDRESS,                N_("display full address of sender")) \
  _fmt(OP_DISPLAY_HEADERS,                N_("display message and toggle header weeding")) \
  _fmt(OP_DISPLAY_MESSAGE,                N_("display a message")) \
  _fmt(OP_EDITOR_BACKSPACE,               N_("delete the char in front of the cursor")) \
  _fmt(OP_EDITOR_BACKWARD_CHAR,           N_("move the cursor one character to the left")) \
  _fmt(OP_EDITOR_BACKWARD_WORD,           N_("move the cursor to the beginning of the word")) \
  _fmt(OP_EDITOR_BOL,                     N_("jump to the beginning of the line")) \
  _fmt(OP_EDITOR_CAPITALIZE_WORD,         N_("capitalize the word")) \
  _fmt(OP_EDITOR_COMPLETE,                N_("complete filename or alias")) \
  _fmt(OP_EDITOR_COMPLETE_QUERY,          N_("complete address with query")) \
  _fmt(OP_EDITOR_DELETE_CHAR,             N_("delete the char under the cursor")) \
  _fmt(OP_EDITOR_DOWNCASE_WORD,           N_("convert the word to lower case")) \
  _fmt(OP_EDITOR_EOL,                     N_("jump to the end of the line")) \
  _fmt(OP_EDITOR_FORWARD_CHAR,            N_("move the cursor one character to the right")) \
  _fmt(OP_EDITOR_FORWARD_WORD,            N_("move the cursor to the end of the word")) \
  _fmt(OP_EDITOR_HISTORY_DOWN,            N_("scroll down through the history list")) \
  _fmt(OP_EDITOR_HISTORY_SEARCH,          N_("search through the history list")) \
  _fmt(OP_EDITOR_HISTORY_UP,              N_("scroll up through the history list")) \
  _fmt(OP_EDITOR_KILL_EOL,                N_("delete chars from cursor to end of line")) \
  _fmt(OP_EDITOR_KILL_EOW,                N_("delete chars from the cursor to the end of the word")) \
  _fmt(OP_EDITOR_KILL_LINE,               N_("delete all chars on the line")) \
  _fmt(OP_EDITOR_KILL_WORD,               N_("delete the word in front of the cursor")) \
  _fmt(OP_EDITOR_MAILBOX_CYCLE,           N_("cycle among incoming mailboxes")) \
  _fmt(OP_EDITOR_QUOTE_CHAR,              N_("quote the next typed key")) \
  _fmt(OP_EDITOR_TRANSPOSE_CHARS,         N_("transpose character under cursor with previous")) \
  _fmt(OP_EDITOR_UPCASE_WORD,             N_("convert the word to upper case")) \
  _fmt(OP_EDIT_LABEL,                     N_("add, change, or delete a message's label")) \
  _fmt(OP_EDIT_OR_VIEW_RAW_MESSAGE,       N_("edit the raw message if the mailbox is not read-only, otherwise view it")) \
  _fmt(OP_EDIT_RAW_MESSAGE,               N_("edit the raw message (edit and edit-raw-message are synonyms)")) \
  _fmt(OP_EDIT_TYPE,                      N_("edit attachment content type")) \
  _fmt(OP_END_COND,                       N_("end of conditional execution (noop)")) \
  _fmt(OP_ENTER_COMMAND,                  N_("enter a neomuttrc command")) \
  _fmt(OP_ENTER_MASK,                     N_("enter a file mask")) \
  _fmt(OP_EXIT,                           N_("exit this menu")) \
  _fmt(OP_FILTER,                         N_("filter attachment through a shell command")) \
  _fmt(OP_FIRST_ENTRY,                    N_("move to the first entry")) \
  _fmt(OP_FLAG_MESSAGE,                   N_("toggle a message's 'important' flag")) \
  _fmt(OP_FOLLOWUP,                       N_("followup to newsgroup")) \
  _fmt(OP_FORWARD_MESSAGE,                N_("forward a message with comments")) \
  _fmt(OP_FORWARD_TO_GROUP,               N_("forward to newsgroup")) \
  _fmt(OP_GENERIC_SELECT_ENTRY,           N_("select the current entry")) \
  _fmt(OP_GET_CHILDREN,                   N_("get all children of the current message")) \
  _fmt(OP_GET_MESSAGE,                    N_("get message with Message-Id")) \
  _fmt(OP_GET_PARENT,                     N_("get parent of the current message")) \
  _fmt(OP_GOTO_PARENT,                    N_("go to parent directory")) \
  _fmt(OP_GROUP_CHAT_REPLY,               N_("reply to all recipients preserving To/Cc")) \
  _fmt(OP_GROUP_REPLY,                    N_("reply to all recipients")) \
  _fmt(OP_HALF_DOWN,                      N_("scroll down 1/2 page")) \
  _fmt(OP_HALF_UP,                        N_("scroll up 1/2 page")) \
  _fmt(OP_HELP,                           N_("this screen")) \
  _fmt(OP_JUMP,                           N_("jump to an index number")) \
  _fmt(OP_LAST_ENTRY,                     N_("move to the last entry")) \
  _fmt(OP_LIMIT_CURRENT_THREAD,           N_("limit view to current thread")) \
  _fmt(OP_LIST_REPLY,                     N_("reply to specified mailing list")) \
  _fmt(OP_LIST_SUBSCRIBE,                 N_("subscribe to a mailing list")) \
  _fmt(OP_LIST_UNSUBSCRIBE,               N_("unsubscribe from a mailing list")) \
  _fmt(OP_LOAD_ACTIVE,                    N_("load list of all newsgroups from NNTP server")) \
  _fmt(OP_MACRO,                          N_("execute a macro")) \
  _fmt(OP_MAIL,                           N_("compose a new mail message")) \
  _fmt(OP_MAILBOX_LIST,                   N_("list mailboxes with new mail")) \
  _fmt(OP_MAIN_BREAK_THREAD,              N_("break the thread in two")) \
  _fmt(OP_MAIN_CHANGE_FOLDER,             N_("open a different folder")) \
  _fmt(OP_MAIN_CHANGE_FOLDER_READONLY,    N_("open a different folder in read only mode")) \
  _fmt(OP_MAIN_CHANGE_GROUP,              N_("open a different newsgroup")) \
  _fmt(OP_MAIN_CHANGE_GROUP_READONLY,     N_("open a different newsgroup in read only mode")) \
  _fmt(OP_MAIN_CLEAR_FLAG,                N_("clear a status flag from a message")) \
  _fmt(OP_MAIN_COLLAPSE_ALL,              N_("collapse/uncollapse all threads")) \
  _fmt(OP_MAIN_COLLAPSE_THREAD,           N_("collapse/uncollapse current thread")) \
  _fmt(OP_MAIN_DELETE_PATTERN,            N_("delete messages matching a pattern")) \
  _fmt(OP_MAIN_FETCH_MAIL,                N_("retrieve mail from POP server")) \
  _fmt(OP_MAIN_IMAP_FETCH,                N_("force retrieval of mail from IMAP server")) \
  _fmt(OP_MAIN_IMAP_LOGOUT_ALL,           N_("logout from all IMAP servers")) \
  _fmt(OP_MAIN_LIMIT,                     N_("show only messages matching a pattern")) \
  _fmt(OP_MAIN_LINK_THREADS,              N_("link tagged message to the current one")) \
  _fmt(OP_MAIN_MODIFY_TAGS,               N_("modify (notmuch/imap) tags")) \
  _fmt(OP_MAIN_MODIFY_TAGS_THEN_HIDE,     N_("modify (notmuch/imap) tags and then hide message")) \
  _fmt(OP_MAIN_NEXT_NEW,                  N_("jump to the next new message")) \
  _fmt(OP_MAIN_NEXT_NEW_THEN_UNREAD,      N_("jump to the next new or unread message")) \
  _fmt(OP_MAIN_NEXT_SUBTHREAD,            N_("jump to the next subthread")) \
  _fmt(OP_MAIN_NEXT_THREAD,               N_("jump to the next thread")) \
  _fmt(OP_MAIN_NEXT_UNDELETED,            N_("move to the next undeleted message")) \
  _fmt(OP_MAIN_NEXT_UNREAD,               N_("jump to the next unread message")) \
  _fmt(OP_MAIN_NEXT_UNREAD_MAILBOX,       N_("open next mailbox with new mail")) \
  _fmt(OP_MAIN_PARENT_MESSAGE,            N_("jump to parent message in thread")) \
  _fmt(OP_MAIN_PREV_NEW,                  N_("jump to the previous new message")) \
  _fmt(OP_MAIN_PREV_NEW_THEN_UNREAD,      N_("jump to the previous new or unread message")) \
  _fmt(OP_MAIN_PREV_SUBTHREAD,            N_("jump to previous subthread")) \
  _fmt(OP_MAIN_PREV_THREAD,               N_("jump to previous thread")) \
  _fmt(OP_MAIN_PREV_UNDELETED,            N_("move to the previous undeleted message")) \
  _fmt(OP_MAIN_PREV_UNREAD,               N_("jump to the previous unread message")) \
  _fmt(OP_MAIN_QUASI_DELETE,              N_("delete from NeoMutt, don't touch on disk")) \
  _fmt(OP_MAIN_READ_SUBTHREAD,            N_("mark the current subthread as read")) \
  _fmt(OP_MAIN_READ_THREAD,               N_("mark the current thread as read")) \
  _fmt(OP_MAIN_ROOT_MESSAGE,              N_("jump to root message in thread")) \
  _fmt(OP_MAIN_SET_FLAG,                  N_("set a status flag on a message")) \
  _fmt(OP_MAIN_SHOW_LIMIT,                N_("show currently active limit pattern")) \
  _fmt(OP_MAIN_SYNC_FOLDER,               N_("save changes to mailbox")) \
  _fmt(OP_MAIN_TAG_PATTERN,               N_("tag messages matching a pattern")) \
  _fmt(OP_MAIN_UNDELETE_PATTERN,          N_("undelete messages matching a pattern")) \
  _fmt(OP_MAIN_UNTAG_PATTERN,             N_("untag messages matching a pattern")) \
  _fmt(OP_MARK_MSG,                       N_("create a hotkey macro for the current message")) \
  _fmt(OP_MIDDLE_PAGE,                    N_("move to the middle of the page")) \
  _fmt(OP_NEXT_ENTRY,                     N_("move to the next entry")) \
  _fmt(OP_NEXT_LINE,                      N_("scroll down one line")) \
  _fmt(OP_NEXT_PAGE,                      N_("move to the next page")) \
  _fmt(OP_PAGER_BOTTOM,                   N_("jump to the bottom of the message")) \
  _fmt(OP_PAGER_HIDE_QUOTED,              N_("toggle display of quoted text")) \
  _fmt(OP_PAGER_SKIP_QUOTED,              N_("skip beyond quoted text")) \
  _fmt(OP_PAGER_SKIP_HEADERS,             N_("jump to first line after headers")) \
  _fmt(OP_PAGER_TOP,                      N_("jump to the top of the message")) \
  _fmt(OP_PIPE,                           N_("pipe message/attachment to a shell command")) \
  _fmt(OP_POST,                           N_("post message to newsgroup")) \
  _fmt(OP_PREV_ENTRY,                     N_("move to the previous entry")) \
  _fmt(OP_PREV_LINE,                      N_("scroll up one line")) \
  _fmt(OP_PREV_PAGE,                      N_("move to the previous page")) \
  _fmt(OP_PRINT,                          N_("print the current entry")) \
  _fmt(OP_PURGE_MESSAGE,                  N_("delete the current entry, bypassing the trash folder")) \
  _fmt(OP_PURGE_THREAD,                   N_("delete the current thread, bypassing the trash folder")) \
  _fmt(OP_QUERY,                          N_("query external program for addresses")) \
  _fmt(OP_QUERY_APPEND,                   N_("append new query results to current results")) \
  _fmt(OP_QUIT,                           N_("save changes to mailbox and quit")) \
  _fmt(OP_RECALL_MESSAGE,                 N_("recall a postponed message")) \
  _fmt(OP_RECONSTRUCT_THREAD,             N_("reconstruct thread containing current message")) \
  _fmt(OP_REDRAW,                         N_("clear and redraw the screen")) \
  _fmt(OP_REFORMAT_WINCH,                 N_("{internal}")) \
  _fmt(OP_RENAME_MAILBOX,                 N_("rename the current mailbox (IMAP only)")) \
  _fmt(OP_REPLY,                          N_("reply to a message")) \
  _fmt(OP_RESEND,                         N_("use the current message as a template for a new one")) \
  _fmt(OP_SAVE,                           N_("save message/attachment to a mailbox/file")) \
  _fmt(OP_SEARCH,                         N_("search for a regular expression")) \
  _fmt(OP_SEARCH_NEXT,                    N_("search for next match")) \
  _fmt(OP_SEARCH_OPPOSITE,                N_("search for next match in opposite direction")) \
  _fmt(OP_SEARCH_REVERSE,                 N_("search backwards for a regular expression")) \
  _fmt(OP_SEARCH_TOGGLE,                  N_("toggle search pattern coloring")) \
  _fmt(OP_SHELL_ESCAPE,                   N_("invoke a command in a subshell")) \
  _fmt(OP_SHOW_LOG_MESSAGES,              N_("show log (and debug) messages")) \
  _fmt(OP_SORT,                           N_("sort messages")) \
  _fmt(OP_SORT_REVERSE,                   N_("sort messages in reverse order")) \
  _fmt(OP_SUBSCRIBE_PATTERN,              N_("subscribe to newsgroups matching a pattern")) \
  _fmt(OP_TAG,                            N_("tag the current entry")) \
  _fmt(OP_TAG_PREFIX,                     N_("apply next function to tagged messages")) \
  _fmt(OP_TAG_PREFIX_COND,                N_("apply next function ONLY to tagged messages")) \
  _fmt(OP_TAG_SUBTHREAD,                  N_("tag the current subthread")) \
  _fmt(OP_TAG_THREAD,                     N_("tag the current thread")) \
  _fmt(OP_TOGGLE_MAILBOXES,               N_("toggle whether to browse mailboxes or all files")) \
  _fmt(OP_TOGGLE_NEW,                     N_("toggle a message's 'new' flag")) \
  _fmt(OP_TOGGLE_READ,                    N_("toggle view of read messages")) \
  _fmt(OP_TOGGLE_WRITE,                   N_("toggle whether the mailbox will be rewritten")) \
  _fmt(OP_TOP_PAGE,                       N_("move to the top of the page")) \
  _fmt(OP_UNCATCHUP,                      N_("mark all articles in newsgroup as unread")) \
  _fmt(OP_UNDELETE,                       N_("undelete the current entry")) \
  _fmt(OP_UNDELETE_SUBTHREAD,             N_("undelete all messages in subthread")) \
  _fmt(OP_UNDELETE_THREAD,                N_("undelete all messages in thread")) \
  _fmt(OP_UNSUBSCRIBE_PATTERN,            N_("unsubscribe from newsgroups matching a pattern")) \
  _fmt(OP_VERSION,                        N_("show the NeoMutt version number and date")) \
  _fmt(OP_VIEW_ATTACH,                    N_("view attachment using mailcap entry if necessary")) \
  _fmt(OP_VIEW_ATTACHMENTS,               N_("show MIME attachments")) \
  _fmt(OP_VIEW_RAW_MESSAGE,               N_("show the raw message")) \
  _fmt(OP_WHAT_KEY,                       N_("display the keycode for a key press")) \

#define OPS_CRYPT(_fmt) \
  _fmt(OP_DECRYPT_COPY,                   N_("make decrypted copy")) \
  _fmt(OP_DECRYPT_SAVE,                   N_("make decrypted copy and delete")) \
  _fmt(OP_EXTRACT_KEYS,                   N_("extract supported public keys")) \
  _fmt(OP_FORGET_PASSPHRASE,              N_("wipe passphrases from memory")) \

#ifdef MIXMASTER
#define OPS_MIX(_fmt) \
  _fmt(OP_COMPOSE_MIX,                    N_("send the message through a mixmaster remailer chain")) \
  _fmt(OP_MIX_APPEND,                     N_("append a remailer to the chain")) \
  _fmt(OP_MIX_CHAIN_NEXT,                 N_("select the next element of the chain")) \
  _fmt(OP_MIX_CHAIN_PREV,                 N_("select the previous element of the chain")) \
  _fmt(OP_MIX_DELETE,                     N_("delete a remailer from the chain")) \
  _fmt(OP_MIX_INSERT,                     N_("insert a remailer into the chain")) \
  _fmt(OP_MIX_USE,                        N_("accept the chain constructed"))
#else
#define OPS_MIX(_)
#endif

#ifdef USE_NOTMUCH
#define OPS_NOTMUCH(_fmt) \
  _fmt(OP_MAIN_CHANGE_VFOLDER,            N_("open a different virtual folder")) \
  _fmt(OP_MAIN_ENTIRE_THREAD,             N_("read entire thread of the current message")) \
  _fmt(OP_MAIN_VFOLDER_FROM_QUERY,        N_("generate virtual folder from query")) \
  _fmt(OP_MAIN_VFOLDER_FROM_QUERY_READONLY, N_("generate a read-only virtual folder from query")) \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_BACKWARD, N_("shifts virtual folder time window backwards")) \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_FORWARD,  N_("shifts virtual folder time window forwards")) \
  _fmt(OP_MAIN_WINDOWED_VFOLDER_RESET,    N_("resets virtual folder time window to the present"))
#else
#define OPS_NOTMUCH(_)
#endif

#define OPS_PGP(_fmt) \
  _fmt(OP_CHECK_TRADITIONAL,              N_("check for classic PGP")) \
  _fmt(OP_COMPOSE_ATTACH_KEY,             N_("attach a PGP public key")) \
  _fmt(OP_COMPOSE_PGP_MENU,               N_("show PGP options")) \
  _fmt(OP_MAIL_KEY,                       N_("mail a PGP public key")) \
  _fmt(OP_VERIFY_KEY,                     N_("verify a PGP public key")) \
  _fmt(OP_VIEW_ID,                        N_("view the key's user id")) \

#define OPS_SIDEBAR(_fmt) \
  _fmt(OP_SIDEBAR_FIRST,                  N_("move the highlight to the first mailbox")) \
  _fmt(OP_SIDEBAR_LAST,                   N_("move the highlight to the last mailbox")) \
  _fmt(OP_SIDEBAR_NEXT,                   N_("move the highlight to next mailbox")) \
  _fmt(OP_SIDEBAR_NEXT_NEW,               N_("move the highlight to next mailbox with new mail")) \
  _fmt(OP_SIDEBAR_OPEN,                   N_("open highlighted mailbox")) \
  _fmt(OP_SIDEBAR_PAGE_DOWN,              N_("scroll the sidebar down 1 page")) \
  _fmt(OP_SIDEBAR_PAGE_UP,                N_("scroll the sidebar up 1 page")) \
  _fmt(OP_SIDEBAR_PREV,                   N_("move the highlight to previous mailbox")) \
  _fmt(OP_SIDEBAR_PREV_NEW,               N_("move the highlight to previous mailbox with new mail")) \
  _fmt(OP_SIDEBAR_TOGGLE_VIRTUAL,         N_("toggle between mailboxes and virtual mailboxes")) \
  _fmt(OP_SIDEBAR_TOGGLE_VISIBLE,         N_("make the sidebar (in)visible")) \

#define OPS_SMIME(_fmt) \
  _fmt(OP_COMPOSE_SMIME_MENU,             N_("show S/MIME options")) \

#define OPS(_fmt) \
  _fmt(OP_NULL,                           N_("null operation")) \
  OPS_AUTOCRYPT(_fmt) \
  OPS_CORE(_fmt) \
  OPS_SIDEBAR(_fmt) \
  OPS_MIX(_fmt) \
  OPS_NOTMUCH(_fmt) \
  OPS_PGP(_fmt) \
  OPS_SMIME(_fmt) \
  OPS_CRYPT(_fmt) \

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

extern const char *OpStrings[][2];

#endif /* MUTT_OPCODES_H */
