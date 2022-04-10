/**
 * @file
 * Definitions of user functions
 *
 * @authors
 * Copyright (C) 1996-2000,2002 Michael R. Elkins <me@mutt.org>
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
 * @page neo_functions Definitions of user functions
 *
 * Definitions of user functions
 *
 * This file contains the structures needed to parse "bind" commands, as well
 * as the default bindings for each menu.
 *
 * Notes:
 *
 * - If you need to bind a control char, use the octal value because the `\cX`
 *   construct does not work at this level.
 *
 * - The magic "map:" comments define how the map will be called in the manual.
 *   Lines starting with "**" will be included in the manual.
 *
 * - For "enter" bindings, add entries for "\n" and "\r" and "<keypadenter>".
 */

#include "config.h"
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#include <stddef.h>
#include "keymap.h"
#include "opcodes.h"
#endif

// clang-format off
/**
 * OpAlias - Functions for the Alias Menu
 */
const struct MenuFuncOp OpAlias[] = { /* map: alias */
  { "delete-entry",                  OP_DELETE },
  { "limit",                         OP_MAIN_LIMIT },
  { "mail",                          OP_MAIL },
  { "sort-alias",                    OP_SORT },
  { "sort-alias-reverse",            OP_SORT_REVERSE },
  { "undelete-entry",                OP_UNDELETE },
  { NULL, 0 },
};

/**
 * OpAttach - Functions for the Attachment Menu
 */
const struct MenuFuncOp OpAttach[] = { /* map: attachment */
  { "bounce-message",                OP_BOUNCE_MESSAGE },
  { "check-traditional-pgp",         OP_CHECK_TRADITIONAL },
  { "collapse-parts",                OP_ATTACHMENT_COLLAPSE },
  { "compose-to-sender",             OP_COMPOSE_TO_SENDER },
  { "delete-entry",                  OP_ATTACHMENT_DELETE },
  { "display-toggle-weed",           OP_DISPLAY_HEADERS },
  { "edit-type",                     OP_ATTACHMENT_EDIT_TYPE },
  { "extract-keys",                  OP_EXTRACT_KEYS },
#ifdef USE_NNTP
  { "followup-message",              OP_FOLLOWUP },
#endif
  { "forget-passphrase",             OP_FORGET_PASSPHRASE },
  { "forward-message",               OP_FORWARD_MESSAGE },
#ifdef USE_NNTP
  { "forward-to-group",              OP_FORWARD_TO_GROUP },
#endif
  { "group-chat-reply",              OP_GROUP_CHAT_REPLY },
  { "group-reply",                   OP_GROUP_REPLY },
  { "list-reply",                    OP_LIST_REPLY },
  { "list-subscribe",                OP_LIST_SUBSCRIBE },
  { "list-unsubscribe",              OP_LIST_UNSUBSCRIBE },
  { "pipe-entry",                    OP_ATTACHMENT_PIPE },
  { "print-entry",                   OP_ATTACHMENT_PRINT },
  { "reply",                         OP_REPLY },
  { "resend-message",                OP_RESEND },
  { "save-entry",                    OP_ATTACHMENT_SAVE },
  { "undelete-entry",                OP_ATTACHMENT_UNDELETE },
  { "view-attach",                   OP_ATTACHMENT_VIEW },
  { "view-mailcap",                  OP_ATTACHMENT_VIEW_MAILCAP },
  { "view-pager",                    OP_ATTACHMENT_VIEW_PAGER },
  { "view-text",                     OP_ATTACHMENT_VIEW_TEXT },
  { NULL, 0 },
};

#ifdef USE_AUTOCRYPT
/**
 * OpAutocrypt - Functions for the Autocrypt Account
 */
const struct MenuFuncOp OpAutocrypt[] = { /* map: autocrypt account */
  { "create-account",                OP_AUTOCRYPT_CREATE_ACCT },
  { "delete-account",                OP_AUTOCRYPT_DELETE_ACCT },
  { "toggle-active",                 OP_AUTOCRYPT_TOGGLE_ACTIVE },
  { "toggle-prefer-encrypt",         OP_AUTOCRYPT_TOGGLE_PREFER },
  { NULL, 0 }
};
#endif

/**
 * OpBrowser - Functions for the file Browser Menu
 */
const struct MenuFuncOp OpBrowser[] = { /* map: browser */
#ifdef USE_NNTP
  { "catchup",                       OP_CATCHUP },
#endif
  { "change-dir",                    OP_CHANGE_DIRECTORY },
  { "check-new",                     OP_CHECK_NEW },
#ifdef USE_IMAP
  { "create-mailbox",                OP_CREATE_MAILBOX },
  { "delete-mailbox",                OP_DELETE_MAILBOX },
#endif
  { "descend-directory",             OP_DESCEND_DIRECTORY },
  { "display-filename",              OP_BROWSER_TELL },
  { "enter-mask",                    OP_ENTER_MASK },
  { "goto-folder",                   OP_BROWSER_GOTO_FOLDER },
  { "goto-parent",                   OP_GOTO_PARENT },
  { "mailbox-list",                  OP_MAILBOX_LIST },
#ifdef USE_NNTP
  { "reload-active",                 OP_LOAD_ACTIVE },
#endif
#ifdef USE_IMAP
  { "rename-mailbox",                OP_RENAME_MAILBOX },
#endif
  { "select-new",                    OP_BROWSER_NEW_FILE },
  { "sort",                          OP_SORT },
  { "sort-reverse",                  OP_SORT_REVERSE },
#if defined(USE_IMAP) || defined(USE_NNTP)
  { "subscribe",                     OP_BROWSER_SUBSCRIBE },
#endif
#ifdef USE_NNTP
  { "subscribe-pattern",             OP_SUBSCRIBE_PATTERN },
#endif
  { "toggle-mailboxes",              OP_TOGGLE_MAILBOXES },
#ifdef USE_IMAP
  { "toggle-subscribed",             OP_BROWSER_TOGGLE_LSUB },
#endif
#ifdef USE_NNTP
  { "uncatchup",                     OP_UNCATCHUP },
#endif
#if defined(USE_IMAP) || defined(USE_NNTP)
  { "unsubscribe",                   OP_BROWSER_UNSUBSCRIBE },
#endif
#ifdef USE_NNTP
  { "unsubscribe-pattern",           OP_UNSUBSCRIBE_PATTERN },
#endif
  { "view-file",                     OP_BROWSER_VIEW_FILE },
  // Deprecated
  { "buffy-list",                    OP_MAILBOX_LIST },
  { NULL, 0 },
};

/**
 * OpCompose - Functions for the Compose Menu
 */
const struct MenuFuncOp OpCompose[] = { /* map: compose */
  { "attach-file",                   OP_ATTACHMENT_ATTACH_FILE },
  { "attach-key",                    OP_ATTACHMENT_ATTACH_KEY },
  { "attach-message",                OP_ATTACHMENT_ATTACH_MESSAGE },
#ifdef USE_NNTP
  { "attach-news-message",           OP_ATTACHMENT_ATTACH_NEWS_MESSAGE },
#endif
#ifdef USE_AUTOCRYPT
  { "autocrypt-menu",                OP_COMPOSE_AUTOCRYPT_MENU },
#endif
  { "copy-file",                     OP_ATTACHMENT_SAVE },
  { "detach-file",                   OP_ATTACHMENT_DETACH },
  { "display-toggle-weed",           OP_DISPLAY_HEADERS },
  { "edit-bcc",                      OP_ENVELOPE_EDIT_BCC },
  { "edit-cc",                       OP_ENVELOPE_EDIT_CC },
  { "edit-content-id",               OP_ATTACHMENT_EDIT_CONTENT_ID },
  { "edit-description",              OP_ATTACHMENT_EDIT_DESCRIPTION },
  { "edit-encoding",                 OP_ATTACHMENT_EDIT_ENCODING },
  { "edit-fcc",                      OP_ENVELOPE_EDIT_FCC },
  { "edit-file",                     OP_COMPOSE_EDIT_FILE },
#ifdef USE_NNTP
  { "edit-followup-to",              OP_ENVELOPE_EDIT_FOLLOWUP_TO },
#endif
  { "edit-from",                     OP_ENVELOPE_EDIT_FROM },
  { "edit-headers",                  OP_ENVELOPE_EDIT_HEADERS },
  { "edit-language",                 OP_ATTACHMENT_EDIT_LANGUAGE },
  { "edit-message",                  OP_COMPOSE_EDIT_MESSAGE },
  { "edit-mime",                     OP_ATTACHMENT_EDIT_MIME },
#ifdef USE_NNTP
  { "edit-newsgroups",               OP_ENVELOPE_EDIT_NEWSGROUPS },
#endif
  { "edit-reply-to",                 OP_ENVELOPE_EDIT_REPLY_TO },
  { "edit-subject",                  OP_ENVELOPE_EDIT_SUBJECT },
  { "edit-to",                       OP_ENVELOPE_EDIT_TO },
  { "edit-type",                     OP_ATTACHMENT_EDIT_TYPE },
#ifdef USE_NNTP
  { "edit-x-comment-to",             OP_ENVELOPE_EDIT_X_COMMENT_TO },
#endif
  { "filter-entry",                  OP_ATTACHMENT_FILTER },
  { "forget-passphrase",             OP_FORGET_PASSPHRASE },
  { "get-attachment",                OP_ATTACHMENT_GET_ATTACHMENT },
  { "group-alternatives",            OP_ATTACHMENT_GROUP_ALTS },
  { "group-multilingual",            OP_ATTACHMENT_GROUP_LINGUAL },
  { "group-related",                 OP_ATTACHMENT_GROUP_RELATED },
  { "ispell",                        OP_COMPOSE_ISPELL },
#ifdef MIXMASTER
  { "mix",                           OP_COMPOSE_MIX },
#endif
  { "move-down",                     OP_ATTACHMENT_MOVE_DOWN },
  { "move-up",                       OP_ATTACHMENT_MOVE_UP },
  { "new-mime",                      OP_ATTACHMENT_NEW_MIME },
  { "pgp-menu",                      OP_COMPOSE_PGP_MENU },
  { "pipe-entry",                    OP_ATTACHMENT_PIPE },
  { "postpone-message",              OP_COMPOSE_POSTPONE_MESSAGE },
  { "print-entry",                   OP_ATTACHMENT_PRINT },
  { "rename-attachment",             OP_ATTACHMENT_RENAME_ATTACHMENT },
  { "rename-file",                   OP_COMPOSE_RENAME_FILE },
  { "send-message",                  OP_COMPOSE_SEND_MESSAGE },
  { "smime-menu",                    OP_COMPOSE_SMIME_MENU },
  { "toggle-disposition",            OP_ATTACHMENT_TOGGLE_DISPOSITION },
  { "toggle-recode",                 OP_ATTACHMENT_TOGGLE_RECODE },
  { "toggle-unlink",                 OP_ATTACHMENT_TOGGLE_UNLINK },
  { "ungroup-attachment",            OP_ATTACHMENT_UNGROUP },
  { "update-encoding",               OP_ATTACHMENT_UPDATE_ENCODING },
  { "view-attach",                   OP_ATTACHMENT_VIEW },
  { "view-mailcap",                  OP_ATTACHMENT_VIEW_MAILCAP },
  { "view-pager",                    OP_ATTACHMENT_VIEW_PAGER },
  { "view-text",                     OP_ATTACHMENT_VIEW_TEXT },
  { "write-fcc",                     OP_COMPOSE_WRITE_MESSAGE },
  { NULL, 0 },
};

/**
 * OpEditor - Functions for the Editor Menu
 */
const struct MenuFuncOp OpEditor[] = { /* map: editor */
  { "backspace",                     OP_EDITOR_BACKSPACE },
  { "backward-char",                 OP_EDITOR_BACKWARD_CHAR },
  { "backward-word",                 OP_EDITOR_BACKWARD_WORD },
  { "bol",                           OP_EDITOR_BOL },
  { "capitalize-word",               OP_EDITOR_CAPITALIZE_WORD },
  { "complete",                      OP_EDITOR_COMPLETE },
  { "complete-query",                OP_EDITOR_COMPLETE_QUERY },
  { "delete-char",                   OP_EDITOR_DELETE_CHAR },
  { "downcase-word",                 OP_EDITOR_DOWNCASE_WORD },
  { "eol",                           OP_EDITOR_EOL },
  { "forward-char",                  OP_EDITOR_FORWARD_CHAR },
  { "forward-word",                  OP_EDITOR_FORWARD_WORD },
  { "history-down",                  OP_EDITOR_HISTORY_DOWN },
  { "history-search",                OP_EDITOR_HISTORY_SEARCH },
  { "history-up",                    OP_EDITOR_HISTORY_UP },
  { "kill-eol",                      OP_EDITOR_KILL_EOL },
  { "kill-eow",                      OP_EDITOR_KILL_EOW },
  { "kill-line",                     OP_EDITOR_KILL_LINE },
  { "kill-word",                     OP_EDITOR_KILL_WORD },
  { "mailbox-cycle",                 OP_EDITOR_MAILBOX_CYCLE },
  { "quote-char",                    OP_EDITOR_QUOTE_CHAR },
  { "transpose-chars",               OP_EDITOR_TRANSPOSE_CHARS },
  { "upcase-word",                   OP_EDITOR_UPCASE_WORD },
  // Deprecated
  { "buffy-cycle",                   OP_EDITOR_MAILBOX_CYCLE },
  { NULL, 0 },
};

/**
 * OpGeneric - Functions for the Generic Menu
 */
const struct MenuFuncOp OpGeneric[] = { /* map: generic */
  /*
  ** <para>
  ** The <emphasis>generic</emphasis> menu is not a real menu, but specifies common functions
  ** (such as movement) available in all menus except for <emphasis>pager</emphasis> and
  ** <emphasis>editor</emphasis>.  Changing settings for this menu will affect the default
  ** bindings for all menus (except as noted).
  ** </para>
  */
  { "bottom-page",                   OP_BOTTOM_PAGE },
  { "check-stats",                   OP_CHECK_STATS },
  { "current-bottom",                OP_CURRENT_BOTTOM },
  { "current-middle",                OP_CURRENT_MIDDLE },
  { "current-top",                   OP_CURRENT_TOP },
  { "end-cond",                      OP_END_COND },
  { "enter-command",                 OP_ENTER_COMMAND },
  { "exit",                          OP_EXIT },
  { "first-entry",                   OP_FIRST_ENTRY },
  { "half-down",                     OP_HALF_DOWN },
  { "half-up",                       OP_HALF_UP },
  { "help",                          OP_HELP },
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
  { "last-entry",                    OP_LAST_ENTRY },
  { "middle-page",                   OP_MIDDLE_PAGE },
  { "next-entry",                    OP_NEXT_ENTRY },
  { "next-line",                     OP_NEXT_LINE },
  { "next-page",                     OP_NEXT_PAGE },
  { "previous-entry",                OP_PREV_ENTRY },
  { "previous-line",                 OP_PREV_LINE },
  { "previous-page",                 OP_PREV_PAGE },
  { "refresh",                       OP_REDRAW },
  { "search",                        OP_SEARCH },
  { "search-next",                   OP_SEARCH_NEXT },
  { "search-opposite",               OP_SEARCH_OPPOSITE },
  { "search-reverse",                OP_SEARCH_REVERSE },
  { "select-entry",                  OP_GENERIC_SELECT_ENTRY },
  { "shell-escape",                  OP_SHELL_ESCAPE },
  { "show-log-messages",             OP_SHOW_LOG_MESSAGES },
  { "tag-entry",                     OP_TAG },
  { "tag-prefix",                    OP_TAG_PREFIX },
  { "tag-prefix-cond",               OP_TAG_PREFIX_COND },
  { "top-page",                      OP_TOP_PAGE },
  { "what-key",                      OP_WHAT_KEY },
  // Deprecated
  { "error-history",                 OP_SHOW_LOG_MESSAGES },
  { NULL, 0 },
};

/**
 * OpIndex - Functions for the Index Menu
 */
const struct MenuFuncOp OpIndex[] = { /* map: index */
  { "alias-dialog",                  OP_ALIAS_DIALOG },
#ifdef USE_AUTOCRYPT
  { "autocrypt-acct-menu",           OP_AUTOCRYPT_ACCT_MENU },
#endif
  { "bounce-message",                OP_BOUNCE_MESSAGE },
  { "break-thread",                  OP_MAIN_BREAK_THREAD },
#ifdef USE_NNTP
  { "catchup",                       OP_CATCHUP },
#endif
  { "change-folder",                 OP_MAIN_CHANGE_FOLDER },
  { "change-folder-readonly",        OP_MAIN_CHANGE_FOLDER_READONLY },
#ifdef USE_NNTP
  { "change-newsgroup",              OP_MAIN_CHANGE_GROUP },
  { "change-newsgroup-readonly",     OP_MAIN_CHANGE_GROUP_READONLY },
#endif
#ifdef USE_NOTMUCH
  { "change-vfolder",                OP_MAIN_CHANGE_VFOLDER },
#endif
  { "check-traditional-pgp",         OP_CHECK_TRADITIONAL },
  { "clear-flag",                    OP_MAIN_CLEAR_FLAG },
  { "collapse-all",                  OP_MAIN_COLLAPSE_ALL },
  { "collapse-thread",               OP_MAIN_COLLAPSE_THREAD },
  { "compose-to-sender",             OP_COMPOSE_TO_SENDER },
  { "copy-message",                  OP_COPY_MESSAGE },
  { "create-alias",                  OP_CREATE_ALIAS },
  { "decode-copy",                   OP_DECODE_COPY },
  { "decode-save",                   OP_DECODE_SAVE },
  { "decrypt-copy",                  OP_DECRYPT_COPY },
  { "decrypt-save",                  OP_DECRYPT_SAVE },
  { "delete-message",                OP_DELETE },
  { "delete-pattern",                OP_MAIN_DELETE_PATTERN },
  { "delete-subthread",              OP_DELETE_SUBTHREAD },
  { "delete-thread",                 OP_DELETE_THREAD },
  { "display-address",               OP_DISPLAY_ADDRESS },
  { "display-message",               OP_DISPLAY_MESSAGE },
  { "display-toggle-weed",           OP_DISPLAY_HEADERS },
  { "edit",                          OP_EDIT_RAW_MESSAGE },
  { "edit-label",                    OP_EDIT_LABEL },
  { "edit-or-view-raw-message",      OP_EDIT_OR_VIEW_RAW_MESSAGE },
  { "edit-raw-message",              OP_EDIT_RAW_MESSAGE },
  { "edit-type",                     OP_ATTACHMENT_EDIT_TYPE },
#ifdef USE_NOTMUCH
  { "entire-thread",                 OP_MAIN_ENTIRE_THREAD },
#endif
  { "extract-keys",                  OP_EXTRACT_KEYS },
#ifdef USE_POP
  { "fetch-mail",                    OP_MAIN_FETCH_MAIL },
#endif
  { "flag-message",                  OP_FLAG_MESSAGE },
#ifdef USE_NNTP
  { "followup-message",              OP_FOLLOWUP },
#endif
  { "forget-passphrase",             OP_FORGET_PASSPHRASE },
  { "forward-message",               OP_FORWARD_MESSAGE },
#ifdef USE_NNTP
  { "forward-to-group",              OP_FORWARD_TO_GROUP },
  { "get-children",                  OP_GET_CHILDREN },
  { "get-message",                   OP_GET_MESSAGE },
  { "get-parent",                    OP_GET_PARENT },
#endif
  { "group-chat-reply",              OP_GROUP_CHAT_REPLY },
  { "group-reply",                   OP_GROUP_REPLY },
#ifdef USE_IMAP
  { "imap-fetch-mail",               OP_MAIN_IMAP_FETCH },
  { "imap-logout-all",               OP_MAIN_IMAP_LOGOUT_ALL },
#endif
  { "limit",                         OP_MAIN_LIMIT },
  { "limit-current-thread",          OP_LIMIT_CURRENT_THREAD },
  { "link-threads",                  OP_MAIN_LINK_THREADS },
  { "list-reply",                    OP_LIST_REPLY },
  { "list-subscribe",                OP_LIST_SUBSCRIBE },
  { "list-unsubscribe",              OP_LIST_UNSUBSCRIBE },
  { "mail",                          OP_MAIL },
  { "mail-key",                      OP_MAIL_KEY },
  { "mailbox-list",                  OP_MAILBOX_LIST },
  { "mark-message",                  OP_MARK_MSG },
  { "modify-labels",                 OP_MAIN_MODIFY_TAGS },
  { "modify-labels-then-hide",       OP_MAIN_MODIFY_TAGS_THEN_HIDE },
  { "modify-tags",                   OP_MAIN_MODIFY_TAGS },
  { "modify-tags-then-hide",         OP_MAIN_MODIFY_TAGS_THEN_HIDE },
  { "next-new",                      OP_MAIN_NEXT_NEW },
  { "next-new-then-unread",          OP_MAIN_NEXT_NEW_THEN_UNREAD },
  { "next-subthread",                OP_MAIN_NEXT_SUBTHREAD },
  { "next-thread",                   OP_MAIN_NEXT_THREAD },
  { "next-undeleted",                OP_MAIN_NEXT_UNDELETED },
  { "next-unread",                   OP_MAIN_NEXT_UNREAD },
  { "next-unread-mailbox",           OP_MAIN_NEXT_UNREAD_MAILBOX },
  { "parent-message",                OP_MAIN_PARENT_MESSAGE },
  { "pipe-message",                  OP_PIPE },
#ifdef USE_NNTP
  { "post-message",                  OP_POST },
#endif
  { "previous-new",                  OP_MAIN_PREV_NEW },
  { "previous-new-then-unread",      OP_MAIN_PREV_NEW_THEN_UNREAD },
  { "previous-subthread",            OP_MAIN_PREV_SUBTHREAD },
  { "previous-thread",               OP_MAIN_PREV_THREAD },
  { "previous-undeleted",            OP_MAIN_PREV_UNDELETED },
  { "previous-unread",               OP_MAIN_PREV_UNREAD },
  { "print-message",                 OP_PRINT },
  { "purge-message",                 OP_PURGE_MESSAGE },
  { "purge-thread",                  OP_PURGE_THREAD },
  { "quasi-delete",                  OP_MAIN_QUASI_DELETE },
  { "query",                         OP_QUERY },
  { "quit",                          OP_QUIT },
  { "read-subthread",                OP_MAIN_READ_SUBTHREAD },
  { "read-thread",                   OP_MAIN_READ_THREAD },
  { "recall-message",                OP_RECALL_MESSAGE },
#ifdef USE_NNTP
  { "reconstruct-thread",            OP_RECONSTRUCT_THREAD },
#endif
  { "reply",                         OP_REPLY },
  { "resend-message",                OP_RESEND },
  { "root-message",                  OP_MAIN_ROOT_MESSAGE },
  { "save-message",                  OP_SAVE },
  { "set-flag",                      OP_MAIN_SET_FLAG },
  { "show-limit",                    OP_MAIN_SHOW_LIMIT },
  { "show-version",                  OP_VERSION },
#ifdef USE_SIDEBAR
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
#endif
  { "sort-mailbox",                  OP_SORT },
  { "sort-reverse",                  OP_SORT_REVERSE },
  { "sync-mailbox",                  OP_MAIN_SYNC_FOLDER },
  { "tag-pattern",                   OP_MAIN_TAG_PATTERN },
  { "tag-subthread",                 OP_TAG_SUBTHREAD },
  { "tag-thread",                    OP_TAG_THREAD },
  { "toggle-new",                    OP_TOGGLE_NEW },
  { "toggle-read",                   OP_TOGGLE_READ },
  { "toggle-write",                  OP_TOGGLE_WRITE },
  { "undelete-message",              OP_UNDELETE },
  { "undelete-pattern",              OP_MAIN_UNDELETE_PATTERN },
  { "undelete-subthread",            OP_UNDELETE_SUBTHREAD },
  { "undelete-thread",               OP_UNDELETE_THREAD },
  { "untag-pattern",                 OP_MAIN_UNTAG_PATTERN },
#ifdef USE_NOTMUCH
  { "vfolder-from-query",            OP_MAIN_VFOLDER_FROM_QUERY },
  { "vfolder-from-query-readonly",   OP_MAIN_VFOLDER_FROM_QUERY_READONLY },
  { "vfolder-window-backward",       OP_MAIN_WINDOWED_VFOLDER_BACKWARD },
  { "vfolder-window-forward",        OP_MAIN_WINDOWED_VFOLDER_FORWARD },
  { "vfolder-window-reset",          OP_MAIN_WINDOWED_VFOLDER_RESET },
#endif
  { "view-attachments",              OP_VIEW_ATTACHMENTS },
  { "view-raw-message",              OP_VIEW_RAW_MESSAGE },
  // Deprecated
  { "buffy-list",                    OP_MAILBOX_LIST },
  { NULL, 0 },
};

#ifdef MIXMASTER
/**
 * OpMix - Functions for the Mixmaster Menu
 */
const struct MenuFuncOp OpMix[] = { /* map: mixmaster */
  { "accept",                        OP_MIX_USE },
  { "append",                        OP_MIX_APPEND },
  { "chain-next",                    OP_MIX_CHAIN_NEXT },
  { "chain-prev",                    OP_MIX_CHAIN_PREV },
  { "delete",                        OP_MIX_DELETE },
  { "insert",                        OP_MIX_INSERT },
  { NULL, 0 },
};
#endif /* MIXMASTER */

/**
 * OpPager - Functions for the Pager Menu
 */
const struct MenuFuncOp OpPager[] = { /* map: pager */
  { "bottom",                        OP_PAGER_BOTTOM },
  { "bounce-message",                OP_BOUNCE_MESSAGE },
  { "break-thread",                  OP_MAIN_BREAK_THREAD },
  { "change-folder",                 OP_MAIN_CHANGE_FOLDER },
  { "change-folder-readonly",        OP_MAIN_CHANGE_FOLDER_READONLY },
#ifdef USE_NNTP
  { "change-newsgroup",              OP_MAIN_CHANGE_GROUP },
  { "change-newsgroup-readonly",     OP_MAIN_CHANGE_GROUP_READONLY },
#endif
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
#ifdef USE_NNTP
  { "followup-message",              OP_FOLLOWUP },
#endif
  { "forget-passphrase",             OP_FORGET_PASSPHRASE },
  { "forward-message",               OP_FORWARD_MESSAGE },
#ifdef USE_NNTP
  { "forward-to-group",              OP_FORWARD_TO_GROUP },
#endif
  { "group-chat-reply",              OP_GROUP_CHAT_REPLY },
  { "group-reply",                   OP_GROUP_REPLY },
  { "half-down",                     OP_HALF_DOWN },
  { "half-up",                       OP_HALF_UP },
  { "help",                          OP_HELP },
#ifdef USE_IMAP
  { "imap-fetch-mail",               OP_MAIN_IMAP_FETCH },
  { "imap-logout-all",               OP_MAIN_IMAP_LOGOUT_ALL },
#endif
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
  { "pipe-entry",                    OP_ATTACHMENT_PIPE },
  { "pipe-message",                  OP_PIPE },
#ifdef USE_NNTP
  { "post-message",                  OP_POST },
#endif
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
#ifdef USE_NNTP
  { "reconstruct-thread",            OP_RECONSTRUCT_THREAD },
#endif
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
#ifdef USE_SIDEBAR
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
#endif
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
 * OpPgp - Functions for the Pgp Menu
 */
const struct MenuFuncOp OpPgp[] = { /* map: pgp */
  { "verify-key",                    OP_VERIFY_KEY },
  { "view-name",                     OP_VIEW_ID },
  { NULL, 0 },
};

/**
 * OpPostpone - Functions for the Postpone Menu
 */
const struct MenuFuncOp OpPostpone[] = { /* map: postpone */
  { "delete-entry",                  OP_DELETE },
  { "undelete-entry",                OP_UNDELETE },
  { NULL, 0 },
};

/**
 * OpQuery - Functions for the external Query Menu
 */
const struct MenuFuncOp OpQuery[] = { /* map: query */
  { "create-alias",                  OP_CREATE_ALIAS },
  { "limit",                         OP_MAIN_LIMIT },
  { "mail",                          OP_MAIL },
  { "query",                         OP_QUERY },
  { "query-append",                  OP_QUERY_APPEND },
  { "sort",                          OP_SORT },
  { "sort-reverse",                  OP_SORT_REVERSE },
  { NULL, 0 },
};

/**
 * OpSmime - Functions for the Smime Menu
 */
const struct MenuFuncOp OpSmime[] = { /* map: smime */
#ifdef CRYPT_BACKEND_GPGME
  { "verify-key",                    OP_VERIFY_KEY },
  { "view-name",                     OP_VIEW_ID },
#endif
  { NULL, 0 },
};

/**
 * AliasDefaultBindings - Key bindings for the Alias Menu
 */
const struct MenuOpSeq AliasDefaultBindings[] = { /* map: alias */
  { OP_DELETE,                             "d" },
  { OP_MAIL,                               "m" },
  { OP_MAIN_LIMIT,                         "l" },
  { OP_SORT,                               "o" },
  { OP_SORT_REVERSE,                       "O" },
  { OP_TAG,                                "<space>" },
  { OP_UNDELETE,                           "u" },
  { 0, NULL },
};

/**
 * AttachDefaultBindings - Key bindings for the Attachment Menu
 */
const struct MenuOpSeq AttachDefaultBindings[] = { /* map: attachment */
  { OP_ATTACHMENT_COLLAPSE,                "v" },
  { OP_ATTACHMENT_DELETE,                  "d" },
  { OP_ATTACHMENT_EDIT_TYPE,               "\005" },           // <Ctrl-E>
  { OP_ATTACHMENT_PIPE,                    "|" },
  { OP_ATTACHMENT_PRINT,                   "p" },
  { OP_ATTACHMENT_SAVE,                    "s" },
  { OP_ATTACHMENT_UNDELETE,                "u" },
  { OP_ATTACHMENT_VIEW,                    "<keypadenter>" },
  { OP_ATTACHMENT_VIEW,                    "\n" },             // <Enter>
  { OP_ATTACHMENT_VIEW,                    "\r" },             // <Return>
  { OP_ATTACHMENT_VIEW_MAILCAP,            "m" },
  { OP_ATTACHMENT_VIEW_TEXT,               "T" },
  { OP_BOUNCE_MESSAGE,                     "b" },
  { OP_CHECK_TRADITIONAL,                  "\033P" },          // <Alt-P>
  { OP_DISPLAY_HEADERS,                    "h" },
  { OP_EXTRACT_KEYS,                       "\013" },           // <Ctrl-K>
  { OP_FORGET_PASSPHRASE,                  "\006" },           // <Ctrl-F>
  { OP_FORWARD_MESSAGE,                    "f" },
  { OP_GROUP_REPLY,                        "g" },
  { OP_LIST_REPLY,                         "L" },
  { OP_REPLY,                              "r" },
  { OP_RESEND,                             "\033e" },          // <Alt-e>
  { 0, NULL },
};

#ifdef USE_AUTOCRYPT
/**
 * AutocryptAcctDefaultBindings - Key bindings for the Autocrypt Account
 */
const struct MenuOpSeq AutocryptAcctDefaultBindings[] = { /* map: autocrypt account */
  { OP_AUTOCRYPT_CREATE_ACCT,              "c" },
  { OP_AUTOCRYPT_DELETE_ACCT,              "D" },
  { OP_AUTOCRYPT_TOGGLE_ACTIVE,            "a" },
  { OP_AUTOCRYPT_TOGGLE_PREFER,            "p" },
  { 0, NULL }
};
#endif

/**
 * BrowserDefaultBindings - Key bindings for the file Browser Menu
 */
const struct MenuOpSeq BrowserDefaultBindings[] = { /* map: browser */
  { OP_BROWSER_GOTO_FOLDER,                "=" },
  { OP_BROWSER_NEW_FILE,                   "N" },
#if defined(USE_IMAP) || defined(USE_NNTP)
  { OP_BROWSER_SUBSCRIBE,                  "s" },
#endif
  { OP_BROWSER_TELL,                       "@" },
#ifdef USE_IMAP
  { OP_BROWSER_TOGGLE_LSUB,                "T" },
#endif
#if defined(USE_IMAP) || defined(USE_NNTP)
  { OP_BROWSER_UNSUBSCRIBE,                "u" },
#endif
  { OP_BROWSER_VIEW_FILE,                  " " },              // <Space>
  { OP_CHANGE_DIRECTORY,                   "c" },
#ifdef USE_IMAP
  { OP_CREATE_MAILBOX,                     "C" },
  { OP_DELETE_MAILBOX,                     "d" },
#endif
  { OP_ENTER_MASK,                         "m" },
  { OP_GOTO_PARENT,                        "p" },
#ifdef USE_NNTP
#endif
  { OP_MAILBOX_LIST,                       "." },
#ifdef USE_IMAP
  { OP_RENAME_MAILBOX,                     "r" },
#endif
  { OP_SORT,                               "o" },
  { OP_SORT_REVERSE,                       "O" },
  { OP_TOGGLE_MAILBOXES,                   "\t" },             // <Tab>
  { 0, NULL },
};

/**
 * ComposeDefaultBindings - Key bindings for the Compose Menu
 */
const struct MenuOpSeq ComposeDefaultBindings[] = { /* map: compose */
  { OP_ATTACHMENT_ATTACH_FILE,             "a" },
  { OP_ATTACHMENT_ATTACH_KEY,              "\033k" },          // <Alt-k>
  { OP_ATTACHMENT_ATTACH_MESSAGE,          "A" },
  { OP_ATTACHMENT_DETACH,                  "D" },
  { OP_ATTACHMENT_EDIT_CONTENT_ID,         "\033i" },          // <Alt-i>
  { OP_ATTACHMENT_EDIT_DESCRIPTION,        "d" },
  { OP_ATTACHMENT_EDIT_ENCODING,           "\005" },           // <Ctrl-E>
  { OP_ATTACHMENT_EDIT_LANGUAGE,           "\014" },           // <Ctrl-L>
  { OP_ATTACHMENT_EDIT_MIME,               "m" },
  { OP_ATTACHMENT_EDIT_TYPE,               "\024" },           // <Ctrl-T>
  { OP_ATTACHMENT_FILTER,                  "F" },
  { OP_ATTACHMENT_GET_ATTACHMENT,          "G" },
  { OP_ATTACHMENT_GROUP_ALTS,              "&" },
  { OP_ATTACHMENT_GROUP_LINGUAL,           "^" },
  { OP_ATTACHMENT_GROUP_RELATED,           "%" },
  { OP_ATTACHMENT_MOVE_DOWN,               "+" },
  { OP_ATTACHMENT_MOVE_UP,                 "-" },
  { OP_ATTACHMENT_NEW_MIME,                "n" },
  { OP_ATTACHMENT_PIPE,                    "|" },
  { OP_ATTACHMENT_PRINT,                   "l" },
  { OP_ATTACHMENT_RENAME_ATTACHMENT,       "\017" },           // <Ctrl-O>
  { OP_ATTACHMENT_SAVE,                    "C" },
  { OP_ATTACHMENT_TOGGLE_DISPOSITION,      "\004" },           // <Ctrl-D>
  { OP_ATTACHMENT_TOGGLE_UNLINK,           "u" },
  { OP_ATTACHMENT_UNGROUP,                 "#" },
  { OP_ATTACHMENT_UPDATE_ENCODING,         "U" },
  { OP_ATTACHMENT_VIEW,                    "<keypadenter>" },
  { OP_ATTACHMENT_VIEW,                    "\n" },             // <Enter>
  { OP_ATTACHMENT_VIEW,                    "\r" },             // <Return>
#ifdef USE_AUTOCRYPT
  { OP_COMPOSE_AUTOCRYPT_MENU,             "o" },
#endif
  { OP_COMPOSE_EDIT_FILE,                  "\033e" },          // <Alt-e>
  { OP_COMPOSE_EDIT_MESSAGE,               "e" },
  { OP_COMPOSE_ISPELL,                     "i" },
#ifdef MIXMASTER
  { OP_COMPOSE_MIX,                        "M" },
#endif
  { OP_COMPOSE_PGP_MENU,                   "p" },
  { OP_COMPOSE_POSTPONE_MESSAGE,           "P" },
  { OP_COMPOSE_RENAME_FILE,                "R" },
  { OP_COMPOSE_SEND_MESSAGE,               "y" },
  { OP_COMPOSE_SMIME_MENU,                 "S" },
  { OP_COMPOSE_WRITE_MESSAGE,              "w" },
  { OP_DISPLAY_HEADERS,                    "h" },
  { OP_ENVELOPE_EDIT_BCC,                  "b" },
  { OP_ENVELOPE_EDIT_CC,                   "c" },
  { OP_ENVELOPE_EDIT_FCC,                  "f" },
  { OP_ENVELOPE_EDIT_FROM,                 "\033f" },          // <Alt-f>
  { OP_ENVELOPE_EDIT_HEADERS,              "E" },
  { OP_ENVELOPE_EDIT_REPLY_TO,             "r" },
  { OP_ENVELOPE_EDIT_SUBJECT,              "s" },
  { OP_ENVELOPE_EDIT_TO,                   "t" },
  { OP_FORGET_PASSPHRASE,                  "\006" },           // <Ctrl-F>
  { OP_TAG,                                "T" },
  { 0, NULL },
};

/**
 * EditorDefaultBindings - Key bindings for the Editor Menu
 */
const struct MenuOpSeq EditorDefaultBindings[] = { /* map: editor */
  { OP_EDITOR_BACKSPACE,                   "<backspace>" },
  { OP_EDITOR_BACKSPACE,                   "\010" },           // <Ctrl-H>
  { OP_EDITOR_BACKSPACE,                   "\177" },           // <Backspace>
  { OP_EDITOR_BACKWARD_CHAR,               "<left>" },
  { OP_EDITOR_BACKWARD_CHAR,               "\002" },           // <Ctrl-B>
  { OP_EDITOR_BACKWARD_WORD,               "\033b" },          // <Alt-b>
  { OP_EDITOR_BOL,                         "<home>" },
  { OP_EDITOR_BOL,                         "\001" },           // <Ctrl-A>
  { OP_EDITOR_CAPITALIZE_WORD,             "\033c" },          // <Alt-c>
  { OP_EDITOR_COMPLETE,                    "\t" },             // <Tab>
  { OP_EDITOR_COMPLETE_QUERY,              "\024" },           // <Ctrl-T>
  { OP_EDITOR_DELETE_CHAR,                 "<delete>" },
  { OP_EDITOR_DELETE_CHAR,                 "\004" },           // <Ctrl-D>
  { OP_EDITOR_DOWNCASE_WORD,               "\033l" },          // <Alt-l>
  { OP_EDITOR_EOL,                         "<end>" },
  { OP_EDITOR_EOL,                         "\005" },           // <Ctrl-E>
  { OP_EDITOR_FORWARD_CHAR,                "<right>" },
  { OP_EDITOR_FORWARD_CHAR,                "\006" },           // <Ctrl-F>
  { OP_EDITOR_FORWARD_WORD,                "\033f" },          // <Alt-f>
  { OP_EDITOR_HISTORY_DOWN,                "<down>" },
  { OP_EDITOR_HISTORY_DOWN,                "\016" },           // <Ctrl-N>
  { OP_EDITOR_HISTORY_SEARCH,              "\022" },           // <Ctrl-R>
  { OP_EDITOR_HISTORY_UP,                  "<up>" },
  { OP_EDITOR_HISTORY_UP,                  "\020" },           // <Ctrl-P>
  { OP_EDITOR_KILL_EOL,                    "\013" },           // <Ctrl-K>
  { OP_EDITOR_KILL_EOW,                    "\033d" },          // <Alt-d>
  { OP_EDITOR_KILL_LINE,                   "\025" },           // <Ctrl-U>
  { OP_EDITOR_KILL_WORD,                   "\027" },           // <Ctrl-W>
  { OP_EDITOR_MAILBOX_CYCLE,               " " },              // <Space>
  { OP_EDITOR_QUOTE_CHAR,                  "\026" },           // <Ctrl-V>
  { OP_EDITOR_UPCASE_WORD,                 "\033u" },          // <Alt-u>
  { 0, NULL },
};

/**
 * GenericDefaultBindings - Key bindings for the Generic Menu
 */
const struct MenuOpSeq GenericDefaultBindings[] = { /* map: generic */
  { OP_BOTTOM_PAGE,                        "L" },
  { OP_ENTER_COMMAND,                      ":" },
  { OP_EXIT,                               "q" },
  { OP_FIRST_ENTRY,                        "<home>" },
  { OP_FIRST_ENTRY,                        "=" },
  { OP_GENERIC_SELECT_ENTRY,               "<keypadenter>" },
  { OP_GENERIC_SELECT_ENTRY,               "\n" },             // <Enter>
  { OP_GENERIC_SELECT_ENTRY,               "\r" },             // <Return>
  { OP_HALF_DOWN,                          "]" },
  { OP_HALF_UP,                            "[" },
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
  { OP_LAST_ENTRY,                         "*" },
  { OP_LAST_ENTRY,                         "<end>" },
  { OP_MIDDLE_PAGE,                        "M" },
  { OP_NEXT_ENTRY,                         "<down>" },
  { OP_NEXT_ENTRY,                         "j" },
  { OP_NEXT_LINE,                          ">" },
  { OP_NEXT_PAGE,                          "<pagedown>" },
  { OP_NEXT_PAGE,                          "<right>" },
  { OP_NEXT_PAGE,                          "z" },
  { OP_PREV_ENTRY,                         "<up>" },
  { OP_PREV_ENTRY,                         "k" },
  { OP_PREV_LINE,                          "<" },
  { OP_PREV_PAGE,                          "<left>" },
  { OP_PREV_PAGE,                          "<pageup>" },
  { OP_PREV_PAGE,                          "Z" },
  { OP_REDRAW,                             "\014" },           // <Ctrl-L>
  { OP_SEARCH,                             "/" },
  { OP_SEARCH_NEXT,                        "n" },
  { OP_SEARCH_REVERSE,                     "\033/" },          // <Alt-/>
  { OP_SHELL_ESCAPE,                       "!" },
  { OP_TAG,                                "t" },
  { OP_TAG_PREFIX,                         ";" },
  { OP_TOP_PAGE,                           "H" },
  { 0, NULL },
};

/**
 * IndexDefaultBindings - Key bindings for the Index Menu
 */
const struct MenuOpSeq IndexDefaultBindings[] = { /* map: index */
  { OP_ATTACHMENT_EDIT_TYPE,               "\005" },           // <Ctrl-E>
#ifdef USE_AUTOCRYPT
  { OP_AUTOCRYPT_ACCT_MENU,                "A" },
#endif
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
  { OP_DISPLAY_MESSAGE,                    " " },              // <Space>
  { OP_DISPLAY_MESSAGE,                    "<keypadenter>" },
  { OP_DISPLAY_MESSAGE,                    "\n" },             // <Enter>
  { OP_DISPLAY_MESSAGE,                    "\r" },             // <Return>
  { OP_EDIT_LABEL,                         "Y" },
  { OP_EDIT_OR_VIEW_RAW_MESSAGE,           "e" },
  { OP_EXIT,                               "x" },
  { OP_EXTRACT_KEYS,                       "\013" },           // <Ctrl-K>
  { OP_FLAG_MESSAGE,                       "F" },
  { OP_FORGET_PASSPHRASE,                  "\006" },           // <Ctrl-F>
  { OP_FORWARD_MESSAGE,                    "f" },
  { OP_GROUP_REPLY,                        "g" },
  { OP_LIST_REPLY,                         "L" },
  { OP_MAIL,                               "m" },
  { OP_MAILBOX_LIST,                       "." },
  { OP_MAIL_KEY,                           "\033k" },          // <Alt-k>
  { OP_MAIN_BREAK_THREAD,                  "#" },
  { OP_MAIN_CHANGE_FOLDER,                 "c" },
  { OP_MAIN_CHANGE_FOLDER_READONLY,        "\033c" },          // <Alt-c>
#ifdef USE_NNTP
  { OP_MAIN_CHANGE_GROUP,                  "i" },
  { OP_MAIN_CHANGE_GROUP_READONLY,         "\033i" },          // <Alt-i>
#endif
  { OP_MAIN_CLEAR_FLAG,                    "W" },
  { OP_MAIN_COLLAPSE_ALL,                  "\033V" },          // <Alt-V>
  { OP_MAIN_COLLAPSE_THREAD,               "\033v" },          // <Alt-v>
  { OP_MAIN_DELETE_PATTERN,                "D" },
#ifdef USE_POP
  { OP_MAIN_FETCH_MAIL,                    "G" },
#endif
  { OP_MAIN_LIMIT,                         "l" },
  { OP_MAIN_LINK_THREADS,                  "&" },
  { OP_MAIN_NEXT_NEW_THEN_UNREAD,          "\t" },             // <Tab>
  { OP_MAIN_NEXT_SUBTHREAD,                "\033n" },          // <Alt-n>
  { OP_MAIN_NEXT_THREAD,                   "\016" },           // <Ctrl-N>
  { OP_MAIN_NEXT_UNDELETED,                "<down>" },
  { OP_MAIN_NEXT_UNDELETED,                "j" },
  { OP_MAIN_PARENT_MESSAGE,                "P" },
  { OP_MAIN_PREV_NEW_THEN_UNREAD,          "\033\t" },         // <Alt-\>
  { OP_MAIN_PREV_SUBTHREAD,                "\033p" },          // <Alt-p>
  { OP_MAIN_PREV_THREAD,                   "\020" },           // <Ctrl-P>
  { OP_MAIN_PREV_UNDELETED,                "<up>" },
  { OP_MAIN_PREV_UNDELETED,                "k" },
  { OP_MAIN_READ_SUBTHREAD,                "\033r" },          // <Alt-r>
  { OP_MAIN_READ_THREAD,                   "\022" },           // <Ctrl-R>
  { OP_MAIN_SET_FLAG,                      "w" },
  { OP_MAIN_SHOW_LIMIT,                    "\033l" },          // <Alt-l>
  { OP_MAIN_SYNC_FOLDER,                   "$" },
  { OP_MAIN_TAG_PATTERN,                   "T" },
  { OP_MAIN_UNDELETE_PATTERN,              "U" },
  { OP_MAIN_UNTAG_PATTERN,                 "\024" },           // <Ctrl-T>
  { OP_MARK_MSG,                           "~" },
  { OP_NEXT_ENTRY,                         "J" },
  { OP_PIPE,                               "|" },
  { OP_PREV_ENTRY,                         "K" },
  { OP_PRINT,                              "p" },
  { OP_QUERY,                              "Q" },
  { OP_QUIT,                               "q" },
  { OP_RECALL_MESSAGE,                     "R" },
  { OP_REPLY,                              "r" },
  { OP_RESEND,                             "\033e" },          // <Alt-e>
  { OP_SAVE,                               "s" },
  { OP_SHOW_LOG_MESSAGES,                  "M" },
  { OP_SORT,                               "o" },
  { OP_SORT_REVERSE,                       "O" },
  { OP_TAG_THREAD,                         "\033t" },          // <Alt-t>
  { OP_TOGGLE_NEW,                         "N" },
  { OP_TOGGLE_WRITE,                       "%" },
  { OP_UNDELETE,                           "u" },
  { OP_UNDELETE_SUBTHREAD,                 "\033u" },          // <Alt-u>
  { OP_UNDELETE_THREAD,                    "\025" },           // <Ctrl-U>
  { OP_VERSION,                            "V" },
  { OP_VIEW_ATTACHMENTS,                   "v" },
  { 0, NULL },
};

#ifdef MIXMASTER
/**
 * MixDefaultBindings - Key bindings for the Mixmaster Menu
 */
const struct MenuOpSeq MixDefaultBindings[] = { /* map: mixmaster */
  { OP_GENERIC_SELECT_ENTRY,               "<space>" },
  { OP_MIX_APPEND,                         "a" },
  { OP_MIX_CHAIN_NEXT,                     "<right>" },
  { OP_MIX_CHAIN_NEXT,                     "l" },
  { OP_MIX_CHAIN_PREV,                     "<left>" },
  { OP_MIX_CHAIN_PREV,                     "h" },
  { OP_MIX_DELETE,                         "d" },
  { OP_MIX_INSERT,                         "i" },
  { OP_MIX_USE,                            "<keypadenter>" },
  { OP_MIX_USE,                            "\n" },             // <Enter>
  { OP_MIX_USE,                            "\r" },             // <Return>
  { 0, NULL },
};
#endif /* MIXMASTER */

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

/**
 * PgpDefaultBindings - Key bindings for the Pgp Menu
 */
const struct MenuOpSeq PgpDefaultBindings[] = { /* map: pgp */
  { OP_VERIFY_KEY,                         "c" },
  { OP_VIEW_ID,                            "%" },
  { 0, NULL },
};

/**
 * PostDefaultBindings - Key bindings for the Postpone Menu
 */
const struct MenuOpSeq PostDefaultBindings[] = { /* map: postpone */
  { OP_DELETE,                             "d" },
  { OP_UNDELETE,                           "u" },
  { 0, NULL },
};

/**
 * QueryDefaultBindings - Key bindings for the external Query Menu
 */
const struct MenuOpSeq QueryDefaultBindings[] = { /* map: query */
  { OP_CREATE_ALIAS,                       "a" },
  { OP_MAIL,                               "m" },
  { OP_MAIN_LIMIT,                         "l" },
  { OP_QUERY,                              "Q" },
  { OP_QUERY_APPEND,                       "A" },
  { OP_SORT,                               "o" },
  { OP_SORT_REVERSE,                       "O" },
  { 0, NULL },
};

/**
 * SmimeDefaultBindings - Key bindings for the Smime Menu
 */
const struct MenuOpSeq SmimeDefaultBindings[] = { /* map: smime */
#ifdef CRYPT_BACKEND_GPGME
  { OP_VERIFY_KEY,                         "c" },
  { OP_VIEW_ID,                            "%" },
#endif
  { 0, NULL },
};

// clang-format on
