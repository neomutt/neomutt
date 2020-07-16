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
 * @page functions Definitions of user functions
 *
 * Definitions of user functions
 */

#include "config.h"
#ifdef _MAKEDOC
#include "docs/makedoc_defs.h"
#else
#include <stddef.h>
#include "keymap.h"
#include "opcodes.h"
#endif

/*
 * This file contains the structures needed to parse "bind" commands, as
 * well as the default bindings for each menu.
 *
 * Notes:
 *
 * - If you need to bind a control char, use the octal value because the \cX
 * construct does not work at this level.
 *
 * - The magic "map:" comments define how the map will be called in the
 * manual. Lines starting with "**" will be included in the manual.
 */

// clang-format off
/**
 * OpGeneric - Key bindings for the generic menu
 */
const struct Binding OpGeneric[] = { /* map: generic */
  /*
  ** <para>
  ** The <emphasis>generic</emphasis> menu is not a real menu, but specifies common functions
  ** (such as movement) available in all menus except for <emphasis>pager</emphasis> and
  ** <emphasis>editor</emphasis>.  Changing settings for this menu will affect the default
  ** bindings for all menus (except as noted).
  ** </para>
  */
  { "bottom-page",     OP_BOTTOM_PAGE,          "L" },
  { "check-stats",     OP_CHECK_STATS,          NULL },
  { "current-bottom",  OP_CURRENT_BOTTOM,       NULL },
  { "current-middle",  OP_CURRENT_MIDDLE,       NULL },
  { "current-top",     OP_CURRENT_TOP,          NULL },
  { "end-cond",        OP_END_COND,             NULL },
  { "enter-command",   OP_ENTER_COMMAND,        ":" },
  { "exit",            OP_EXIT,                 "q" },
  { "first-entry",     OP_FIRST_ENTRY,          "=" },
  { "half-down",       OP_HALF_DOWN,            "]" },
  { "half-up",         OP_HALF_UP,              "[" },
  { "help",            OP_HELP,                 "?" },
  { "jump",            OP_JUMP,                 NULL },
  { "last-entry",      OP_LAST_ENTRY,           "*" },
  { "middle-page",     OP_MIDDLE_PAGE,          "M" },
  { "next-entry",      OP_NEXT_ENTRY,           "j" },
  { "next-line",       OP_NEXT_LINE,            ">" },
  { "next-page",       OP_NEXT_PAGE,            "z" },
  { "previous-entry",  OP_PREV_ENTRY,           "k" },
  { "previous-line",   OP_PREV_LINE,            "<" },
  { "previous-page",   OP_PREV_PAGE,            "Z" },
  { "refresh",         OP_REDRAW,               "\014" },                       // <Ctrl-L>
  { "search",          OP_SEARCH,               "/" },
  { "search-next",     OP_SEARCH_NEXT,          "n" },
  { "search-opposite", OP_SEARCH_OPPOSITE,      NULL },
  { "search-reverse",  OP_SEARCH_REVERSE,       "\033/" },                      // <Alt-/>
  { "select-entry",    OP_GENERIC_SELECT_ENTRY, "\n" },
  { "select-entry",    OP_GENERIC_SELECT_ENTRY, "\r" },
  { "shell-escape",    OP_SHELL_ESCAPE,         "!" },
  { "tag-entry",       OP_TAG,                  "t" },
  { "tag-prefix",      OP_TAG_PREFIX,           ";" },
  { "tag-prefix-cond", OP_TAG_PREFIX_COND,      NULL },
  { "top-page",        OP_TOP_PAGE,             "H" },
  { "what-key",        OP_WHAT_KEY,             NULL },
  { NULL,              0,                       NULL },
};

/**
 * OpMain - Key bindings for the index menu
 */
const struct Binding OpMain[] = { /* map: index */
#ifdef USE_AUTOCRYPT
  { "autocrypt-acct-menu",       OP_AUTOCRYPT_ACCT_MENU,            "A" },
#endif
  { "bounce-message",            OP_BOUNCE_MESSAGE,                 "b" },
  { "break-thread",              OP_MAIN_BREAK_THREAD,              "#" },
#ifdef USE_NNTP
  { "catchup",                   OP_CATCHUP,                        NULL },
#endif
  { "change-folder",             OP_MAIN_CHANGE_FOLDER,             "c" },
  { "change-folder-readonly",    OP_MAIN_CHANGE_FOLDER_READONLY,    "\033c" },  // <Alt-c>
#ifdef USE_NNTP
  { "change-newsgroup",          OP_MAIN_CHANGE_GROUP,              "i" },
  { "change-newsgroup-readonly", OP_MAIN_CHANGE_GROUP_READONLY,     "\033i" },  // <Alt-i>
#endif
#ifdef USE_NOTMUCH
  { "change-vfolder",            OP_MAIN_CHANGE_VFOLDER,            NULL },
#endif
  { "check-traditional-pgp",     OP_CHECK_TRADITIONAL,              "\033P" },  // <Alt-P>
  { "clear-flag",                OP_MAIN_CLEAR_FLAG,                "W" },
  { "collapse-all",              OP_MAIN_COLLAPSE_ALL,              "\033V" },  // <Alt-V>
  { "collapse-thread",           OP_MAIN_COLLAPSE_THREAD,           "\033v" },  // <Alt-v>
  { "compose-to-sender",         OP_COMPOSE_TO_SENDER,              NULL },
  { "copy-message",              OP_COPY_MESSAGE,                   "C" },
  { "create-alias",              OP_CREATE_ALIAS,                   "a" },
  { "decode-copy",               OP_DECODE_COPY,                    "\033C" },  // <Alt-C>
  { "decode-save",               OP_DECODE_SAVE,                    "\033s" },  // <Alt-s>
  { "decrypt-copy",              OP_DECRYPT_COPY,                   NULL },
  { "decrypt-save",              OP_DECRYPT_SAVE,                   NULL },
  { "delete-message",            OP_DELETE,                         "d" },
  { "delete-pattern",            OP_MAIN_DELETE_PATTERN,            "D" },
  { "delete-subthread",          OP_DELETE_SUBTHREAD,               "\033d" },  // <Alt-d>
  { "delete-thread",             OP_DELETE_THREAD,                  "\004" },   // <Ctrl-D>
  { "display-address",           OP_DISPLAY_ADDRESS,                "@" },
  { "display-message",           OP_DISPLAY_MESSAGE,                "\n" },     // <Enter>
  { "display-message",           OP_DISPLAY_MESSAGE,                "\r" },
  { "display-toggle-weed",       OP_DISPLAY_HEADERS,                "h" },
  { "edit",                      OP_EDIT_RAW_MESSAGE,               NULL },
  { "edit-label",                OP_EDIT_LABEL,                     "Y" },
  { "edit-or-view-raw-message",  OP_EDIT_OR_VIEW_RAW_MESSAGE,       "e" },
  { "edit-raw-message",          OP_EDIT_RAW_MESSAGE,               NULL },
  { "edit-type",                 OP_EDIT_TYPE,                      "\005" },   // <Ctrl-E>
#ifdef USE_NOTMUCH
  { "entire-thread",             OP_MAIN_ENTIRE_THREAD,             NULL },
#endif
  { "extract-keys",              OP_EXTRACT_KEYS,                   "\013" },   // <Ctrl-K>
#ifdef USE_POP
  { "fetch-mail",                OP_MAIN_FETCH_MAIL,                "G" },
#endif
  { "flag-message",              OP_FLAG_MESSAGE,                   "F" },
#ifdef USE_NNTP
  { "followup-message",          OP_FOLLOWUP,                       NULL },
#endif
  { "forget-passphrase",         OP_FORGET_PASSPHRASE,              "\006" },   // <Ctrl-F>
  { "forward-message",           OP_FORWARD_MESSAGE,                "f" },
#ifdef USE_NNTP
  { "forward-to-group",          OP_FORWARD_TO_GROUP,               NULL },
  { "get-children",              OP_GET_CHILDREN,                   NULL },
  { "get-message",               OP_GET_MESSAGE,                    NULL },
  { "get-parent",                OP_GET_PARENT,                     NULL },
#endif
  { "group-chat-reply",          OP_GROUP_CHAT_REPLY,               NULL },
  { "group-reply",               OP_GROUP_REPLY,                    "g" },
#ifdef USE_IMAP
  { "imap-fetch-mail",           OP_MAIN_IMAP_FETCH,                NULL },
  { "imap-logout-all",           OP_MAIN_IMAP_LOGOUT_ALL,           NULL },
#endif
  { "limit",                     OP_MAIN_LIMIT,                     "l" },
  { "limit-current-thread",      OP_LIMIT_CURRENT_THREAD,           NULL },
  { "link-threads",              OP_MAIN_LINK_THREADS,              "&" },
  { "list-reply",                OP_LIST_REPLY,                     "L" },
  { "mail",                      OP_MAIL,                           "m" },
  { "mail-key",                  OP_MAIL_KEY,                       "\033k" },  // <Alt-k>
  { "mailbox-list",              OP_MAILBOX_LIST,                   "." },
  { "mark-message",              OP_MARK_MSG,                       "~" },
  { "modify-labels",             OP_MAIN_MODIFY_TAGS,               NULL }, // NOTE(sileht): kept for backward compatibility
  { "modify-labels-then-hide",   OP_MAIN_MODIFY_TAGS_THEN_HIDE,     NULL }, // NOTE(sileht): kept for backward compatibility
  { "modify-tags",               OP_MAIN_MODIFY_TAGS,               NULL },
  { "modify-tags-then-hide",     OP_MAIN_MODIFY_TAGS_THEN_HIDE,     NULL },
  { "next-new",                  OP_MAIN_NEXT_NEW,                  NULL },
  { "next-new-then-unread",      OP_MAIN_NEXT_NEW_THEN_UNREAD,      "\t" },
  { "next-subthread",            OP_MAIN_NEXT_SUBTHREAD,            "\033n" },  // <Alt-n>
  { "next-thread",               OP_MAIN_NEXT_THREAD,               "\016" },   // <Ctrl-N>
  { "next-undeleted",            OP_MAIN_NEXT_UNDELETED,            "j" },
  { "next-unread",               OP_MAIN_NEXT_UNREAD,               NULL },
  { "next-unread-mailbox",       OP_MAIN_NEXT_UNREAD_MAILBOX,       NULL },
  { "parent-message",            OP_MAIN_PARENT_MESSAGE,            "P" },
  { "pipe-message",              OP_PIPE,                           "|" },
#ifdef USE_NNTP
  { "post-message",              OP_POST,                           NULL },
#endif
  { "previous-new",              OP_MAIN_PREV_NEW,                  NULL },
  { "previous-new-then-unread",  OP_MAIN_PREV_NEW_THEN_UNREAD,      "\033\t" }, // <Alt-\>
  { "previous-subthread",        OP_MAIN_PREV_SUBTHREAD,            "\033p" },  // <Alt-p>
  { "previous-thread",           OP_MAIN_PREV_THREAD,               "\020" },   // <Ctrl-P>
  { "previous-undeleted",        OP_MAIN_PREV_UNDELETED,            "k" },
  { "previous-unread",           OP_MAIN_PREV_UNREAD,               NULL },
  { "print-message",             OP_PRINT,                          "p" },
  { "purge-message",             OP_PURGE_MESSAGE,                  NULL },
  { "purge-thread",              OP_PURGE_THREAD,                   NULL },
  { "quasi-delete",              OP_MAIN_QUASI_DELETE,              NULL },
  { "query",                     OP_QUERY,                          "Q" },
  { "quit",                      OP_QUIT,                           "q" },
  { "read-subthread",            OP_MAIN_READ_SUBTHREAD,            "\033r" },  // <Alt-r>
  { "read-thread",               OP_MAIN_READ_THREAD,               "\022" },   // <Ctrl-R>
  { "recall-message",            OP_RECALL_MESSAGE,                 "R" },
#ifdef USE_NNTP
  { "reconstruct-thread",        OP_RECONSTRUCT_THREAD,             NULL },
#endif
  { "reply",                     OP_REPLY,                          "r" },
  { "resend-message",            OP_RESEND,                         "\033e" },  // <Alt-e>
  { "root-message",              OP_MAIN_ROOT_MESSAGE,              NULL },
  { "save-message",              OP_SAVE,                           "s" },
  { "set-flag",                  OP_MAIN_SET_FLAG,                  "w" },
  { "show-limit",                OP_MAIN_SHOW_LIMIT,                "\033l" },  // <Alt-l>
  { "show-log-messages",         OP_SHOW_LOG_MESSAGES,              "M" },
  { "show-version",              OP_VERSION,                        "V" },
#ifdef USE_SIDEBAR
  { "sidebar-first",             OP_SIDEBAR_FIRST,                  NULL },
  { "sidebar-last",              OP_SIDEBAR_LAST,                   NULL },
  { "sidebar-next",              OP_SIDEBAR_NEXT,                   NULL },
  { "sidebar-next-new",          OP_SIDEBAR_NEXT_NEW,               NULL },
  { "sidebar-open",              OP_SIDEBAR_OPEN,                   NULL },
  { "sidebar-page-down",         OP_SIDEBAR_PAGE_DOWN,              NULL },
  { "sidebar-page-up",           OP_SIDEBAR_PAGE_UP,                NULL },
  { "sidebar-prev",              OP_SIDEBAR_PREV,                   NULL },
  { "sidebar-prev-new",          OP_SIDEBAR_PREV_NEW,               NULL },
  { "sidebar-toggle-virtual",    OP_SIDEBAR_TOGGLE_VIRTUAL,         NULL },
  { "sidebar-toggle-visible",    OP_SIDEBAR_TOGGLE_VISIBLE,         NULL },
#endif
  { "sort-mailbox",              OP_SORT,                           "o" },
  { "sort-reverse",              OP_SORT_REVERSE,                   "O" },
  { "sync-mailbox",              OP_MAIN_SYNC_FOLDER,               "$" },
  { "tag-pattern",               OP_MAIN_TAG_PATTERN,               "T" },
  { "tag-subthread",             OP_TAG_SUBTHREAD,                  NULL },
  { "tag-thread",                OP_TAG_THREAD,                     "\033t" },  // <Alt-t>
  { "toggle-new",                OP_TOGGLE_NEW,                     "N" },
  { "toggle-read",               OP_TOGGLE_READ,                    NULL },
  { "toggle-write",              OP_TOGGLE_WRITE,                   "%" },
  { "undelete-message",          OP_UNDELETE,                       "u" },
  { "undelete-pattern",          OP_MAIN_UNDELETE_PATTERN,          "U" },
  { "undelete-subthread",        OP_UNDELETE_SUBTHREAD,             "\033u" },  // <Alt-u>
  { "undelete-thread",           OP_UNDELETE_THREAD,                "\025" },   // <Ctrl-U>
  { "untag-pattern",             OP_MAIN_UNTAG_PATTERN,             "\024" },   // <Ctrl-T>
#ifdef USE_NOTMUCH
  { "vfolder-from-query",        OP_MAIN_VFOLDER_FROM_QUERY,        NULL },
  { "vfolder-from-query-readonly", OP_MAIN_VFOLDER_FROM_QUERY_READONLY, NULL },
  { "vfolder-window-backward",   OP_MAIN_WINDOWED_VFOLDER_BACKWARD, NULL },
  { "vfolder-window-forward",    OP_MAIN_WINDOWED_VFOLDER_FORWARD,  NULL },
#endif
  { "view-attachments",          OP_VIEW_ATTACHMENTS,               "v" },
  { "view-raw-message",          OP_VIEW_RAW_MESSAGE,               NULL },
  /* This is deprecated.  Leave it last so it doesn't show up in the help. */
  { "buffy-list",                OP_MAILBOX_LIST,                   NULL },
  { NULL,                        0,                                 NULL },
};

/**
 * OpPager - Key bindings for the pager menu
 */
const struct Binding OpPager[] = { /* map: pager */
  { "bottom",                    OP_PAGER_BOTTOM,                 NULL },
  { "bounce-message",            OP_BOUNCE_MESSAGE,               "b" },
  { "break-thread",              OP_MAIN_BREAK_THREAD,            "#" },
  { "change-folder",             OP_MAIN_CHANGE_FOLDER,           "c" },
  { "change-folder-readonly",    OP_MAIN_CHANGE_FOLDER_READONLY,  "\033c" },    // <Alt-c>
#ifdef USE_NNTP
  { "change-newsgroup",          OP_MAIN_CHANGE_GROUP,            "i" },
  { "change-newsgroup-readonly", OP_MAIN_CHANGE_GROUP_READONLY,   "\033i" },    // <Alt-i>
#endif
#ifdef USE_NOTMUCH
  { "change-vfolder",            OP_MAIN_CHANGE_VFOLDER,          NULL },
#endif
  { "check-stats",               OP_CHECK_STATS,                  NULL },
  { "check-traditional-pgp",     OP_CHECK_TRADITIONAL,            "\033P" },    // <Alt-P>
  { "clear-flag",                OP_MAIN_CLEAR_FLAG,              "W" },
  { "compose-to-sender",         OP_COMPOSE_TO_SENDER,            NULL },
  { "copy-message",              OP_COPY_MESSAGE,                 "C" },
  { "create-alias",              OP_CREATE_ALIAS,                 "a" },
  { "decode-copy",               OP_DECODE_COPY,                  "\033C" },    // <Alt-C>
  { "decode-save",               OP_DECODE_SAVE,                  "\033s" },    // <Alt-s>
  { "decrypt-copy",              OP_DECRYPT_COPY,                 NULL },
  { "decrypt-save",              OP_DECRYPT_SAVE,                 NULL },
  { "delete-message",            OP_DELETE,                       "d" },
  { "delete-subthread",          OP_DELETE_SUBTHREAD,             "\033d" },    // <Alt-d>
  { "delete-thread",             OP_DELETE_THREAD,                "\004" },     // <Ctrl-D>
  { "display-address",           OP_DISPLAY_ADDRESS,              "@" },
  { "display-toggle-weed",       OP_DISPLAY_HEADERS,              "h" },
  { "edit",                      OP_EDIT_RAW_MESSAGE,             NULL },
  { "edit-label",                OP_EDIT_LABEL,                   "Y" },
  { "edit-or-view-raw-message",  OP_EDIT_OR_VIEW_RAW_MESSAGE,     "e" },
  { "edit-raw-message",          OP_EDIT_RAW_MESSAGE,             NULL },
  { "edit-type",                 OP_EDIT_TYPE,                    "\005" },     // <Ctrl-E>
  { "enter-command",             OP_ENTER_COMMAND,                ":" },
#ifdef USE_NOTMUCH
  { "entire-thread",             OP_MAIN_ENTIRE_THREAD,           NULL },
#endif
  { "exit",                      OP_EXIT,                         "q" },
  { "extract-keys",              OP_EXTRACT_KEYS,                 "\013" },     // <Ctrl-K>
  { "flag-message",              OP_FLAG_MESSAGE,                 "F" },
#ifdef USE_NNTP
  { "followup-message",          OP_FOLLOWUP,                     NULL },
#endif
  { "forget-passphrase",         OP_FORGET_PASSPHRASE,            "\006" },     // <Ctrl-F>
  { "forward-message",           OP_FORWARD_MESSAGE,              "f" },
#ifdef USE_NNTP
  { "forward-to-group",          OP_FORWARD_TO_GROUP,             NULL },
#endif
  { "group-chat-reply",          OP_GROUP_CHAT_REPLY,             NULL },
  { "group-reply",               OP_GROUP_REPLY,                  "g" },
  { "half-down",                 OP_HALF_DOWN,                    NULL },
  { "half-up",                   OP_HALF_UP,                      NULL },
  { "help",                      OP_HELP,                         "?" },
#ifdef USE_IMAP
  { "imap-fetch-mail",           OP_MAIN_IMAP_FETCH,              NULL },
  { "imap-logout-all",           OP_MAIN_IMAP_LOGOUT_ALL,         NULL },
#endif
  { "jump",                      OP_JUMP,                         NULL },
  { "link-threads",              OP_MAIN_LINK_THREADS,            "&" },
  { "list-reply",                OP_LIST_REPLY,                   "L" },
  { "mail",                      OP_MAIL,                         "m" },
  { "mail-key",                  OP_MAIL_KEY,                     "\033k" },    // <Alt-k>
  { "mailbox-list",              OP_MAILBOX_LIST,                 "." },
  { "mark-as-new",               OP_TOGGLE_NEW,                   "N" },
  { "modify-labels",             OP_MAIN_MODIFY_TAGS,             NULL }, // NOTE(sileht): kept for backward compatibility
  { "modify-labels-then-hide",   OP_MAIN_MODIFY_TAGS_THEN_HIDE,   NULL }, // NOTE(sileht): kept for backward compatibility
  { "modify-tags",               OP_MAIN_MODIFY_TAGS,             NULL },
  { "modify-tags-then-hide",     OP_MAIN_MODIFY_TAGS_THEN_HIDE,   NULL },
  { "next-entry",                OP_NEXT_ENTRY,                   "J" },
  { "next-line",                 OP_NEXT_LINE,                    "\n" },       // <Enter>
  { "next-line",                 OP_NEXT_LINE,                    "\r" },
  { "next-new",                  OP_MAIN_NEXT_NEW,                NULL },
  { "next-new-then-unread",      OP_MAIN_NEXT_NEW_THEN_UNREAD,    "\t" },
  { "next-page",                 OP_NEXT_PAGE,                    " " },
  { "next-subthread",            OP_MAIN_NEXT_SUBTHREAD,          "\033n" },    // <Alt-n>
  { "next-thread",               OP_MAIN_NEXT_THREAD,             "\016" },     // <Ctrl-N>
  { "next-undeleted",            OP_MAIN_NEXT_UNDELETED,          "j" },
  { "next-unread",               OP_MAIN_NEXT_UNREAD,             NULL },
  { "next-unread-mailbox",       OP_MAIN_NEXT_UNREAD_MAILBOX,     NULL },
  { "parent-message",            OP_MAIN_PARENT_MESSAGE,          "P" },
  { "pipe-message",              OP_PIPE,                         "|" },
#ifdef USE_NNTP
  { "post-message",              OP_POST,                         NULL },
#endif
  { "previous-entry",            OP_PREV_ENTRY,                   "K" },
  { "previous-line",             OP_PREV_LINE,                    NULL },
  { "previous-new",              OP_MAIN_PREV_NEW,                NULL },
  { "previous-new-then-unread",  OP_MAIN_PREV_NEW_THEN_UNREAD,    NULL },
  { "previous-page",             OP_PREV_PAGE,                    "-" },
  { "previous-subthread",        OP_MAIN_PREV_SUBTHREAD,          "\033p" },    // <Alt-p>
  { "previous-thread",           OP_MAIN_PREV_THREAD,             "\020" },     // <Ctrl-P>
  { "previous-undeleted",        OP_MAIN_PREV_UNDELETED,          "k" },
  { "previous-unread",           OP_MAIN_PREV_UNREAD,             NULL },
  { "print-message",             OP_PRINT,                        "p" },
  { "purge-message",             OP_PURGE_MESSAGE,                NULL },
  { "purge-thread",              OP_PURGE_THREAD,                 NULL },
  { "quasi-delete",              OP_MAIN_QUASI_DELETE,            NULL },
  { "quit",                      OP_QUIT,                         "Q" },
  { "read-subthread",            OP_MAIN_READ_SUBTHREAD,          "\033r" },    // <Alt-r>
  { "read-thread",               OP_MAIN_READ_THREAD,             "\022" },     // <Ctrl-R>
  { "recall-message",            OP_RECALL_MESSAGE,               "R" },
#ifdef USE_NNTP
  { "reconstruct-thread",        OP_RECONSTRUCT_THREAD,           NULL },
#endif
  { "redraw-screen",             OP_REDRAW,                       "\014" },     // <Ctrl-L>
  { "reply",                     OP_REPLY,                        "r" },
  { "resend-message",            OP_RESEND,                       "\033e" },    // <Alt-e>
  { "root-message",              OP_MAIN_ROOT_MESSAGE,            NULL },
  { "save-message",              OP_SAVE,                         "s" },
  { "search",                    OP_SEARCH,                       "/" },
  { "search-next",               OP_SEARCH_NEXT,                  "n" },
  { "search-opposite",           OP_SEARCH_OPPOSITE,              NULL },
  { "search-reverse",            OP_SEARCH_REVERSE,               "\033/" },    // <Alt-/>
  { "search-toggle",             OP_SEARCH_TOGGLE,                "\\" },       // <Backslash>
  { "set-flag",                  OP_MAIN_SET_FLAG,                "w" },
  { "shell-escape",              OP_SHELL_ESCAPE,                 "!" },
  { "show-version",              OP_VERSION,                      "V" },
#ifdef USE_SIDEBAR
  { "sidebar-first",             OP_SIDEBAR_FIRST,                NULL },
  { "sidebar-last",              OP_SIDEBAR_LAST,                 NULL },
  { "sidebar-next",              OP_SIDEBAR_NEXT,                 NULL },
  { "sidebar-next-new",          OP_SIDEBAR_NEXT_NEW,             NULL },
  { "sidebar-open",              OP_SIDEBAR_OPEN,                 NULL },
  { "sidebar-page-down",         OP_SIDEBAR_PAGE_DOWN,            NULL },
  { "sidebar-page-up",           OP_SIDEBAR_PAGE_UP,              NULL },
  { "sidebar-prev",              OP_SIDEBAR_PREV,                 NULL },
  { "sidebar-prev-new",          OP_SIDEBAR_PREV_NEW,             NULL },
  { "sidebar-toggle-virtual",    OP_SIDEBAR_TOGGLE_VIRTUAL,       NULL },
  { "sidebar-toggle-visible",    OP_SIDEBAR_TOGGLE_VISIBLE,       NULL },
#endif
  { "skip-quoted",               OP_PAGER_SKIP_QUOTED,            "S" },
  { "sort-mailbox",              OP_SORT,                         "o" },
  { "sort-reverse",              OP_SORT_REVERSE,                 "O" },
  { "sync-mailbox",              OP_MAIN_SYNC_FOLDER,             "$" },
  { "tag-message",               OP_TAG,                          "t" },
  { "toggle-quoted",             OP_PAGER_HIDE_QUOTED,            "T" },
  { "toggle-write",              OP_TOGGLE_WRITE,                 "%" },
  { "top",                       OP_PAGER_TOP,                    "^" },
  { "undelete-message",          OP_UNDELETE,                     "u" },
  { "undelete-subthread",        OP_UNDELETE_SUBTHREAD,           "\033u" },    // <Alt-u>
  { "undelete-thread",           OP_UNDELETE_THREAD,              "\025" },     // <Ctrl-U>
#ifdef USE_NOTMUCH
  { "vfolder-from-query",        OP_MAIN_VFOLDER_FROM_QUERY,      NULL },
  { "vfolder-from-query-readonly", OP_MAIN_VFOLDER_FROM_QUERY_READONLY, NULL },
#endif
  { "view-attachments",          OP_VIEW_ATTACHMENTS,             "v" },
  { "view-raw-message",          OP_VIEW_RAW_MESSAGE,             NULL },
  { "what-key",                  OP_WHAT_KEY,                     NULL },
  /* This is deprecated.  Leave it last so it doesn't show up in the help. */
  { "buffy-list",                OP_MAILBOX_LIST,                 NULL },
  { NULL,                        0,                               NULL },
};

/**
 * OpAttach - Key bindings for the attachment menu
 */
const struct Binding OpAttach[] = { /* map: attachment */
  { "bounce-message",        OP_BOUNCE_MESSAGE,              "b" },
  { "check-traditional-pgp", OP_CHECK_TRADITIONAL,           "\033P" },         // <Alt-P>
  { "collapse-parts",        OP_ATTACH_COLLAPSE,             "v" },
  { "compose-to-sender",     OP_COMPOSE_TO_SENDER,           NULL },
  { "delete-entry",          OP_DELETE,                      "d" },
  { "display-toggle-weed",   OP_DISPLAY_HEADERS,             "h" },
  { "edit-type",             OP_EDIT_TYPE,                   "\005" },          // <Ctrl-E>
  { "extract-keys",          OP_EXTRACT_KEYS,                "\013" },          // <Ctrl-K>
#ifdef USE_NNTP
  { "followup-message",      OP_FOLLOWUP,                    NULL },
#endif
  { "forget-passphrase",     OP_FORGET_PASSPHRASE,           "\006" },          // <Ctrl-F>
  { "forward-message",       OP_FORWARD_MESSAGE,             "f" },
#ifdef USE_NNTP
  { "forward-to-group",      OP_FORWARD_TO_GROUP,            NULL },
#endif
  { "group-chat-reply",      OP_GROUP_CHAT_REPLY,            NULL },
  { "group-reply",           OP_GROUP_REPLY,                 "g" },
  { "list-reply",            OP_LIST_REPLY,                  "L" },
  { "pipe-entry",            OP_PIPE,                        "|" },
  { "print-entry",           OP_PRINT,                       "p" },
  { "reply",                 OP_REPLY,                       "r" },
  { "resend-message",        OP_RESEND,                      "\033e" },         // <Alt-e>
  { "save-entry",            OP_SAVE,                        "s" },
  { "undelete-entry",        OP_UNDELETE,                    "u" },
  { "view-attach",           OP_VIEW_ATTACH,                 "\n" },            // <Enter>
  { "view-attach",           OP_VIEW_ATTACH,                 "\r" },
  { "view-mailcap",          OP_ATTACH_VIEW_MAILCAP,         "m" },
  { "view-text",             OP_ATTACH_VIEW_TEXT,            "T" },
  { NULL,                    0,                              NULL },
};

/**
 * OpCompose - Key bindings for the compose menu
 */
const struct Binding OpCompose[] = { /* map: compose */
  { "attach-file",           OP_COMPOSE_ATTACH_FILE,         "a" },
  { "attach-key",            OP_COMPOSE_ATTACH_KEY,          "\033k" },         // <Alt-k>
  { "attach-message",        OP_COMPOSE_ATTACH_MESSAGE,      "A" },
#ifdef USE_NNTP
  { "attach-news-message",   OP_COMPOSE_ATTACH_NEWS_MESSAGE, NULL },
#endif
#ifdef USE_AUTOCRYPT
  { "autocrypt-menu",        OP_COMPOSE_AUTOCRYPT_MENU,      "o" },
#endif
  { "copy-file",             OP_SAVE,                        "C" },
  { "detach-file",           OP_DELETE,                      "D" },
  { "display-toggle-weed",   OP_DISPLAY_HEADERS,             "h" },
  { "edit-bcc",              OP_COMPOSE_EDIT_BCC,            "b" },
  { "edit-cc",               OP_COMPOSE_EDIT_CC,             "c" },
  { "edit-description",      OP_COMPOSE_EDIT_DESCRIPTION,    "d" },
  { "edit-encoding",         OP_COMPOSE_EDIT_ENCODING,       "\005" },          // <Ctrl-E>
  { "edit-fcc",              OP_COMPOSE_EDIT_FCC,            "f" },
  { "edit-file",             OP_COMPOSE_EDIT_FILE,           "\030e" },         // <Alt-e>
#ifdef USE_NNTP
  { "edit-followup-to",      OP_COMPOSE_EDIT_FOLLOWUP_TO,    NULL },
#endif
  { "edit-from",             OP_COMPOSE_EDIT_FROM,           "\033f" },         // <Alt-f>
  { "edit-headers",          OP_COMPOSE_EDIT_HEADERS,        "E" },
  { "edit-language",         OP_COMPOSE_EDIT_LANGUAGE,       "\014" },          // <Ctrl-L>
  { "edit-message",          OP_COMPOSE_EDIT_MESSAGE,        "e" },
  { "edit-mime",             OP_COMPOSE_EDIT_MIME,           "m" },
#ifdef USE_NNTP
  { "edit-newsgroups",       OP_COMPOSE_EDIT_NEWSGROUPS,     NULL },
#endif
  { "edit-reply-to",         OP_COMPOSE_EDIT_REPLY_TO,       "r" },
  { "edit-subject",          OP_COMPOSE_EDIT_SUBJECT,        "s" },
  { "edit-to",               OP_COMPOSE_EDIT_TO,             "t" },
  { "edit-type",             OP_EDIT_TYPE,                   "\024" },          // <Ctrl-T>
#ifdef USE_NNTP
  { "edit-x-comment-to",     OP_COMPOSE_EDIT_X_COMMENT_TO,   NULL },
#endif
  { "filter-entry",          OP_FILTER,                      "F" },
  { "forget-passphrase",     OP_FORGET_PASSPHRASE,           "\006" },          // <Ctrl-F>
  { "get-attachment",        OP_COMPOSE_GET_ATTACHMENT,      "G" },
  { "group-alternatives",    OP_COMPOSE_GROUP_ALTS,          "&" },
  { "group-multilingual",    OP_COMPOSE_GROUP_LINGUAL,       "^" },
  { "ispell",                OP_COMPOSE_ISPELL,              "i" },
#ifdef MIXMASTER
  { "mix",                   OP_COMPOSE_MIX,                 "M" },
#endif
  { "move-down",             OP_COMPOSE_MOVE_DOWN,           "+" },
  { "move-up",               OP_COMPOSE_MOVE_UP,             "-" },
  { "new-mime",              OP_COMPOSE_NEW_MIME,            "n" },
  { "pgp-menu",              OP_COMPOSE_PGP_MENU,            "p" },
  { "pipe-entry",            OP_PIPE,                        "|" },
  { "postpone-message",      OP_COMPOSE_POSTPONE_MESSAGE,    "P" },
  { "print-entry",           OP_PRINT,                       "l" },
  { "rename-attachment",     OP_COMPOSE_RENAME_ATTACHMENT,   "\017" },          // <Ctrl-O>
  { "rename-file",           OP_COMPOSE_RENAME_FILE,         "R" },
  { "send-message",          OP_COMPOSE_SEND_MESSAGE,        "y" },
  { "smime-menu",            OP_COMPOSE_SMIME_MENU,          "S" },
  { "toggle-disposition",    OP_COMPOSE_TOGGLE_DISPOSITION,  "\004" },          // <Ctrl-D>
  { "toggle-recode",         OP_COMPOSE_TOGGLE_RECODE,       NULL },
  { "toggle-unlink",         OP_COMPOSE_TOGGLE_UNLINK,       "u" },
  { "update-encoding",       OP_COMPOSE_UPDATE_ENCODING,     "U" },
  { "view-attach",           OP_VIEW_ATTACH,                 "\n" },            // <Enter>
  { "view-attach",           OP_VIEW_ATTACH,                 "\r" },
  { "write-fcc",             OP_COMPOSE_WRITE_MESSAGE,       "w" },
  { NULL,                    0,                              NULL },
};

/**
 * OpPost - Key bindings for the postpone menu
 */
const struct Binding OpPost[] = { /* map: postpone */
  { "delete-entry",          OP_DELETE,                      "d" },
  { "undelete-entry",        OP_UNDELETE,                    "u" },
  { NULL,                    0,                              NULL },
};

/**
 * OpAlias - Key bindings for the alias menu
 */
const struct Binding OpAlias[] = { /* map: alias */
  { "delete-entry",          OP_DELETE,                      "d" },
  { "undelete-entry",        OP_UNDELETE,                    "u" },
  { NULL,                    0,                              NULL },
};

/**
 * OpBrowser - Key bindings for the file browser menu
 */
const struct Binding OpBrowser[] = { /* map: browser */
#ifdef USE_NNTP
  { "catchup",               OP_CATCHUP,                     NULL },
#endif
  { "change-dir",            OP_CHANGE_DIRECTORY,            "c" },
  { "check-new",             OP_CHECK_NEW,                   NULL },
#ifdef USE_IMAP
  { "create-mailbox",        OP_CREATE_MAILBOX,              "C" },
  { "delete-mailbox",        OP_DELETE_MAILBOX,              "d" },
#endif
  { "descend-directory",     OP_DESCEND_DIRECTORY,           NULL },
  { "display-filename",      OP_BROWSER_TELL,                "@" },
  { "enter-mask",            OP_ENTER_MASK,                  "m" },
  { "goto-folder",           OP_BROWSER_GOTO_FOLDER,         "=" },
  { "goto-parent",           OP_GOTO_PARENT,                 "p" },
  { "mailbox-list",          OP_MAILBOX_LIST,                "." },
#ifdef USE_NNTP
  { "reload-active",         OP_LOAD_ACTIVE,                 NULL },
#endif
#ifdef USE_IMAP
  { "rename-mailbox",        OP_RENAME_MAILBOX,              "r" },
#endif
  { "select-new",            OP_BROWSER_NEW_FILE,            "N" },
  { "sort",                  OP_SORT,                        "o" },
  { "sort-reverse",          OP_SORT_REVERSE,                "O" },
#if defined(USE_IMAP) || defined(USE_NNTP)
  { "subscribe",             OP_BROWSER_SUBSCRIBE,           "s" },
#endif
#ifdef USE_NNTP
  { "subscribe-pattern",     OP_SUBSCRIBE_PATTERN,           NULL },
#endif
  { "toggle-mailboxes",      OP_TOGGLE_MAILBOXES,            "\t" },
#ifdef USE_IMAP
  { "toggle-subscribed",     OP_BROWSER_TOGGLE_LSUB,         "T" },
#endif
#ifdef USE_NNTP
  { "uncatchup",             OP_UNCATCHUP,                   NULL },
#endif
#if defined(USE_IMAP) || defined(USE_NNTP)
  { "unsubscribe",           OP_BROWSER_UNSUBSCRIBE,         "u" },
#endif
#ifdef USE_NNTP
  { "unsubscribe-pattern",   OP_UNSUBSCRIBE_PATTERN,         NULL },
#endif
  { "view-file",             OP_BROWSER_VIEW_FILE,           " " },
  /* This is deprecated.  Leave it last so it doesn't show up in the help. */
  { "buffy-list",            OP_MAILBOX_LIST,                NULL },
  { NULL,                    0,                              NULL },
};

/**
 * OpQuery - Key bindings for the external query menu
 */
const struct Binding OpQuery[] = { /* map: query */
  { "create-alias",          OP_CREATE_ALIAS,                "a" },
  { "mail",                  OP_MAIL,                        "m" },
  { "query",                 OP_QUERY,                       "Q" },
  { "query-append",          OP_QUERY_APPEND,                "A" },
  { NULL,                    0,                              NULL },
};

/**
 * OpEditor - Key bindings for the editor menu
 */
const struct Binding OpEditor[] = { /* map: editor */
  { "backspace",             OP_EDITOR_BACKSPACE,            "\010" },          // <Ctrl-H>
  { "backward-char",         OP_EDITOR_BACKWARD_CHAR,        "\002" },          // <Ctrl-B>
  { "backward-word",         OP_EDITOR_BACKWARD_WORD,        "\033b" },         // <Alt-b>
  { "bol",                   OP_EDITOR_BOL,                  "\001" },          // <Ctrl-A>
  { "capitalize-word",       OP_EDITOR_CAPITALIZE_WORD,      "\033c" },         // <Alt-c>
  { "complete",              OP_EDITOR_COMPLETE,             "\t" },            // <Tab>
  { "complete-query",        OP_EDITOR_COMPLETE_QUERY,       "\024" },          // <Ctrl-T>
  { "delete-char",           OP_EDITOR_DELETE_CHAR,          "\004" },          // <Ctrl-D>
  { "downcase-word",         OP_EDITOR_DOWNCASE_WORD,        "\033l" },         // <Alt-l>
  { "eol",                   OP_EDITOR_EOL,                  "\005" },          // <Ctrl-E>
  { "forward-char",          OP_EDITOR_FORWARD_CHAR,         "\006" },          // <Ctrl-F>
  { "forward-word",          OP_EDITOR_FORWARD_WORD,         "\033f" },         // <Alt-f>
  { "history-down",          OP_EDITOR_HISTORY_DOWN,         "\016" },          // <Ctrl-N>
  { "history-search",        OP_EDITOR_HISTORY_SEARCH,       "\022" },          // <Ctrl-R>
  { "history-up",            OP_EDITOR_HISTORY_UP,           "\020" },          // <Ctrl-P>
  { "kill-eol",              OP_EDITOR_KILL_EOL,             "\013" },          // <Ctrl-K>
  { "kill-eow",              OP_EDITOR_KILL_EOW,             "\033d" },         // <Alt-d>
  { "kill-line",             OP_EDITOR_KILL_LINE,            "\025" },          // <Ctrl-U>
  { "kill-word",             OP_EDITOR_KILL_WORD,            "\027" },          // <Ctrl-W>
  { "mailbox-cycle",         OP_EDITOR_MAILBOX_CYCLE,        " " },
  { "quote-char",            OP_EDITOR_QUOTE_CHAR,           "\026" },          // <Ctrl-V>
  { "transpose-chars",       OP_EDITOR_TRANSPOSE_CHARS,      NULL },
  { "upcase-word",           OP_EDITOR_UPCASE_WORD,          "\033u" },         // <Alt-u>
  /* This is deprecated.  Leave it last so it doesn't show up in the help. */
  { "buffy-cycle",           OP_EDITOR_MAILBOX_CYCLE,        NULL },
  { NULL,                    0,                              NULL },
};

/**
 * OpPgp - Key bindings for the pgp menu
 */
const struct Binding OpPgp[] = { /* map: pgp */
  { "verify-key",            OP_VERIFY_KEY,                  "c" },
  { "view-name",             OP_VIEW_ID,                     "%" },
  { NULL,                    0,                              NULL },
};

/**
 * OpSmime - Key bindings for the smime menu
 */
const struct Binding OpSmime[] = { /* map: smime */
#ifdef CRYPT_BACKEND_GPGME
  { "verify-key",            OP_VERIFY_KEY,                  "c" },
  { "view-name",             OP_VIEW_ID,                     "%" },
#endif
  { NULL,                    0,                              NULL },
};

#ifdef MIXMASTER
/**
 * OpMix - Key bindings for the mixmaster menu
 */
const struct Binding OpMix[] = { /* map: mixmaster */
  { "accept",                OP_MIX_USE,                     "\n" },            // <Enter>
  { "accept",                OP_MIX_USE,                     "\r" },            // <Return>
  { "append",                OP_MIX_APPEND,                  "a" },
  { "chain-next",            OP_MIX_CHAIN_NEXT,              "<right>" },
  { "chain-prev",            OP_MIX_CHAIN_PREV,              "<left>" },
  { "delete",                OP_MIX_DELETE,                  "d" },
  { "insert",                OP_MIX_INSERT,                  "i" },
  { NULL,                    0,                              NULL },
};
#endif /* MIXMASTER */

#ifdef USE_AUTOCRYPT
/**
 * OpAutocryptAcct - Key bindings for the autocrypt account
 */
const struct Binding OpAutocryptAcct[] = { /* map: autocrypt account */
  { "create-account",        OP_AUTOCRYPT_CREATE_ACCT,       "c" },
  { "delete-account",        OP_AUTOCRYPT_DELETE_ACCT,       "D" },
  { "toggle-active",         OP_AUTOCRYPT_TOGGLE_ACTIVE,     "a" },
  { "toggle-prefer-encrypt", OP_AUTOCRYPT_TOGGLE_PREFER,     "p" },
  { NULL,                    0,                              NULL }
};
#endif

// clang-format on
