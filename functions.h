/*
 * Copyright (C) 1996-2000,2002 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/*
 * This file contains the structures needed to parse ``bind'' commands, as
 * well as the default bindings for each menu.
 *
 * Notes:
 *
 * - If you want to bind \n or \r, use M_ENTER_S so that it will work
 * correctly under both ncurses and S-Lang
 *
 * - If you need to bind a control char, use the octal value because the \cX
 * construct does not work at this level.
 *
 * - The magic "map:" comments define how the map will be called in the
 * manual. Lines starting with "**" will be included in the manual.
 *
 */

#ifdef _MAKEDOC
# include "config.h"
# include "doc/makedoc-defs.h"
#endif

struct binding_t OpGeneric[] = { /* map: generic */
  /*
  ** <para>
  ** The <emphasis>generic</emphasis> menu is not a real menu, but specifies common functions
  ** (such as movement) available in all menus except for <emphasis>pager</emphasis> and
  ** <emphasis>editor</emphasis>.  Changing settings for this menu will affect the default
  ** bindings for all menus (except as noted).
  ** </para>
  */
  { "top-page",		OP_TOP_PAGE,		"H" },
  { "next-entry",	OP_NEXT_ENTRY,		"j" },
  { "previous-entry",	OP_PREV_ENTRY,		"k" },
  { "bottom-page",	OP_BOTTOM_PAGE,		"L" },
  { "refresh",		OP_REDRAW,		"\014" },
  { "middle-page",	OP_MIDDLE_PAGE,		"M" },
  { "search-next",	OP_SEARCH_NEXT,		"n" },
  { "exit",		OP_EXIT,		"q" },
  { "tag-entry",	OP_TAG,			"t" },
  { "next-page",	OP_NEXT_PAGE,		"z" },
  { "previous-page",	OP_PREV_PAGE,		"Z" },
  { "last-entry",	OP_LAST_ENTRY,		"*" },
  { "first-entry",	OP_FIRST_ENTRY,		"=" },
  { "enter-command",	OP_ENTER_COMMAND,	":" },
  { "next-line",	OP_NEXT_LINE,		">" },
  { "previous-line",	OP_PREV_LINE,		"<" },
  { "half-up",		OP_HALF_UP,		"[" },
  { "half-down", 	OP_HALF_DOWN,		"]" },
  { "help",		OP_HELP,		"?" },
  { "tag-prefix",	OP_TAG_PREFIX,		";" },
  { "tag-prefix-cond",	OP_TAG_PREFIX_COND,	NULL },
  { "end-cond",		OP_END_COND,		NULL },
  { "shell-escape",	OP_SHELL_ESCAPE,	"!" },
  { "select-entry",	OP_GENERIC_SELECT_ENTRY,M_ENTER_S },
  { "search",		OP_SEARCH,		"/" },
  { "search-reverse",	OP_SEARCH_REVERSE,	"\033/" },
  { "search-opposite",	OP_SEARCH_OPPOSITE,	NULL },
  { "jump",		OP_JUMP,		NULL },
  { "current-top",      OP_CURRENT_TOP,		NULL },
  { "current-middle",   OP_CURRENT_MIDDLE,	NULL },
  { "current-bottom",   OP_CURRENT_BOTTOM,	NULL },
  { "what-key",		OP_WHAT_KEY,		NULL },
  { NULL,		0,			NULL }
};

struct binding_t OpMain[] = { /* map: index */
  { "create-alias",		OP_CREATE_ALIAS,		"a" },
  { "bounce-message",		OP_BOUNCE_MESSAGE,		"b" },
  { "break-thread",		OP_MAIN_BREAK_THREAD,		"#" },
  { "change-folder",		OP_MAIN_CHANGE_FOLDER,		"c" },
  { "change-folder-readonly",	OP_MAIN_CHANGE_FOLDER_READONLY,	"\033c" },
  { "next-unread-mailbox",	OP_MAIN_NEXT_UNREAD_MAILBOX,    NULL },
  { "collapse-thread",		OP_MAIN_COLLAPSE_THREAD,	"\033v" },
  { "collapse-all",		OP_MAIN_COLLAPSE_ALL,		"\033V" },
  { "copy-message",		OP_COPY_MESSAGE,		"C" },
  { "decode-copy",		OP_DECODE_COPY,			"\033C" },
  { "decode-save",		OP_DECODE_SAVE,			"\033s" },
  { "delete-message",		OP_DELETE,			"d" },
  { "delete-pattern",		OP_MAIN_DELETE_PATTERN,		"D" },
  { "delete-thread",		OP_DELETE_THREAD,		"\004" },
  { "delete-subthread",		OP_DELETE_SUBTHREAD,		"\033d" },
  { "edit",			OP_EDIT_MESSAGE,		"e" },
  { "edit-type",		OP_EDIT_TYPE,			"\005" },
  { "forward-message",		OP_FORWARD_MESSAGE,		"f" },
  { "flag-message",		OP_FLAG_MESSAGE,		"F" },
  { "group-reply",		OP_GROUP_REPLY,			"g" },
#ifdef USE_POP
  { "fetch-mail",		OP_MAIN_FETCH_MAIL,		"G" },
#endif
#ifdef USE_IMAP
  { "imap-fetch-mail",		OP_MAIN_IMAP_FETCH,		NULL },
#endif
  { "display-toggle-weed",		OP_DISPLAY_HEADERS,		"h" },
  { "next-undeleted",		OP_MAIN_NEXT_UNDELETED,		"j" },
  { "previous-undeleted",	OP_MAIN_PREV_UNDELETED,		"k" },
  { "limit",			OP_MAIN_LIMIT,			"l" },
  { "link-threads",		OP_MAIN_LINK_THREADS,		"&" },
  { "list-reply",		OP_LIST_REPLY,			"L" },
  { "mail",			OP_MAIL,			"m" },
  { "toggle-new",		OP_TOGGLE_NEW,			"N" },
  { "toggle-write",		OP_TOGGLE_WRITE,		"%" },
  { "next-thread",		OP_MAIN_NEXT_THREAD,		"\016" },
  { "next-subthread",		OP_MAIN_NEXT_SUBTHREAD,		"\033n" },
  { "query",			OP_QUERY,			"Q" },
  { "quit",			OP_QUIT,			"q" },
  { "reply",			OP_REPLY,			"r" },
  { "show-limit",		OP_MAIN_SHOW_LIMIT,		"\033l" },
  { "sort-mailbox",		OP_SORT,			"o" },
  { "sort-reverse",		OP_SORT_REVERSE,		"O" },
  { "print-message",		OP_PRINT,			"p" },
  { "previous-thread",		OP_MAIN_PREV_THREAD,		"\020" },
  { "previous-subthread",	OP_MAIN_PREV_SUBTHREAD,		"\033p" },
  { "recall-message",		OP_RECALL_MESSAGE,		"R" },
  { "read-thread",		OP_MAIN_READ_THREAD,		"\022" },
  { "read-subthread",		OP_MAIN_READ_SUBTHREAD,		"\033r" },
  { "resend-message",		OP_RESEND,			"\033e" },
  { "save-message",		OP_SAVE,			"s" },
  { "tag-pattern",		OP_MAIN_TAG_PATTERN,		"T" },
  { "tag-subthread",		OP_TAG_SUBTHREAD,		NULL },
  { "tag-thread",		OP_TAG_THREAD,			"\033t" },
  { "untag-pattern",		OP_MAIN_UNTAG_PATTERN,		"\024" },
  { "undelete-message",		OP_UNDELETE,			"u" },
  { "undelete-pattern",		OP_MAIN_UNDELETE_PATTERN,	"U"},
  { "undelete-subthread",	OP_UNDELETE_SUBTHREAD,		"\033u" },
  { "undelete-thread",		OP_UNDELETE_THREAD,		"\025" },
  { "view-attachments",		OP_VIEW_ATTACHMENTS,		"v" },
  { "show-version",		OP_VERSION,			"V" },
  { "set-flag",			OP_MAIN_SET_FLAG,		"w" },
  { "clear-flag",		OP_MAIN_CLEAR_FLAG,		"W" },
  { "display-message",		OP_DISPLAY_MESSAGE,		M_ENTER_S },
  { "buffy-list",		OP_BUFFY_LIST,			"." },
  { "sync-mailbox",		OP_MAIN_SYNC_FOLDER,		"$" },
  { "display-address",		OP_DISPLAY_ADDRESS,		"@" },
  { "pipe-message",		OP_PIPE,			"|" },
  { "next-new",			OP_MAIN_NEXT_NEW,		NULL },
  { "next-new-then-unread",	OP_MAIN_NEXT_NEW_THEN_UNREAD,	"\t" },
  { "previous-new",		OP_MAIN_PREV_NEW,		NULL },
  { "previous-new-then-unread",	OP_MAIN_PREV_NEW_THEN_UNREAD,	"\033\t" },
  { "next-unread",		OP_MAIN_NEXT_UNREAD,		NULL },
  { "previous-unread",		OP_MAIN_PREV_UNREAD,		NULL },
  { "parent-message",		OP_MAIN_PARENT_MESSAGE,		"P" },


  { "extract-keys",		OP_EXTRACT_KEYS,		"\013" },
  { "forget-passphrase",	OP_FORGET_PASSPHRASE,		"\006" },
  { "check-traditional-pgp",	OP_CHECK_TRADITIONAL,		"\033P" },
  { "mail-key",			OP_MAIL_KEY,			"\033k" },
  { "decrypt-copy",		OP_DECRYPT_COPY,		NULL },
  { "decrypt-save",		OP_DECRYPT_SAVE,		NULL },


  { NULL,			0,				NULL }
};

struct binding_t OpPager[] = { /* map: pager */
  { "break-thread",	OP_MAIN_BREAK_THREAD,		"#" },
  { "create-alias",	OP_CREATE_ALIAS,		"a" },
  { "bounce-message",	OP_BOUNCE_MESSAGE,		"b" },
  { "change-folder",	OP_MAIN_CHANGE_FOLDER,		"c" },
  { "change-folder-readonly",	OP_MAIN_CHANGE_FOLDER_READONLY,	"\033c" },
  { "next-unread-mailbox",	OP_MAIN_NEXT_UNREAD_MAILBOX, NULL },
  { "copy-message",	OP_COPY_MESSAGE,		"C" },
  { "decode-copy",	OP_DECODE_COPY,			"\033C" },
  { "delete-message",	OP_DELETE,			"d" },
  { "delete-thread",	OP_DELETE_THREAD,		"\004" },
  { "delete-subthread",	OP_DELETE_SUBTHREAD,		"\033d" },
  { "set-flag",  	OP_MAIN_SET_FLAG,		"w" },
  { "clear-flag",       OP_MAIN_CLEAR_FLAG,		"W" },
  { "edit",		OP_EDIT_MESSAGE,		"e" },
  { "edit-type",	OP_EDIT_TYPE,			"\005" },
  { "forward-message",	OP_FORWARD_MESSAGE,		"f" },
  { "flag-message",	OP_FLAG_MESSAGE,		"F" },
  { "group-reply",	OP_GROUP_REPLY,			"g" },
#ifdef USE_IMAP
  { "imap-fetch-mail",  OP_MAIN_IMAP_FETCH,		NULL },
#endif
  { "display-toggle-weed",	OP_DISPLAY_HEADERS,		"h" },
  { "next-undeleted",	OP_MAIN_NEXT_UNDELETED,		"j" },
  { "next-entry",	OP_NEXT_ENTRY,			"J" },
  { "previous-undeleted",OP_MAIN_PREV_UNDELETED,	"k" },
  { "previous-entry",	OP_PREV_ENTRY,			"K" },
  { "link-threads",	OP_MAIN_LINK_THREADS,		"&" },
  { "list-reply",	OP_LIST_REPLY,			"L" },
  { "redraw-screen",	OP_REDRAW,			"\014" },
  { "mail",		OP_MAIL,			"m" },
  { "mark-as-new",	OP_TOGGLE_NEW,			"N" },
  { "search-next",	OP_SEARCH_NEXT,			"n" },
  { "next-thread",	OP_MAIN_NEXT_THREAD,		"\016" },
  { "next-subthread",	OP_MAIN_NEXT_SUBTHREAD,		"\033n" },
  { "print-message",	OP_PRINT,			"p" },
  { "previous-thread",	OP_MAIN_PREV_THREAD,		"\020" },
  { "previous-subthread",OP_MAIN_PREV_SUBTHREAD,	"\033p" },
  { "quit",		OP_QUIT,			"Q" },
  { "exit",		OP_EXIT,			"q" },
  { "reply",		OP_REPLY,			"r" },
  { "recall-message",	OP_RECALL_MESSAGE,		"R" },
  { "read-thread",	OP_MAIN_READ_THREAD,		"\022" },
  { "read-subthread",	OP_MAIN_READ_SUBTHREAD,		"\033r" },
  { "resend-message",	OP_RESEND,			"\033e" },
  { "save-message",	OP_SAVE,			"s" },
  { "skip-quoted",	OP_PAGER_SKIP_QUOTED,		"S" },
  { "decode-save",	OP_DECODE_SAVE,			"\033s" },
  { "tag-message",	OP_TAG,				"t" },
  { "toggle-quoted",	OP_PAGER_HIDE_QUOTED,		"T" },
  { "undelete-message",	OP_UNDELETE,			"u" },
  { "undelete-subthread",OP_UNDELETE_SUBTHREAD,		"\033u" },
  { "undelete-thread",	OP_UNDELETE_THREAD,		"\025" },
  { "view-attachments",	OP_VIEW_ATTACHMENTS,		"v" },
  { "show-version",	OP_VERSION,			"V" },
  { "search-toggle",	OP_SEARCH_TOGGLE,		"\\" },
  { "display-address",	OP_DISPLAY_ADDRESS,		"@" },
  { "next-new",		OP_MAIN_NEXT_NEW,		NULL },
  { "next-new-then-unread", 
                        OP_MAIN_NEXT_NEW_THEN_UNREAD,   "\t" },
  { "pipe-message",	OP_PIPE,			"|" },
  { "help",		OP_HELP,			"?" },
  { "next-page",	OP_NEXT_PAGE,			" " },
  { "previous-page",	OP_PREV_PAGE,			"-" },
  { "top",		OP_PAGER_TOP,			"^" },
  { "sync-mailbox",	OP_MAIN_SYNC_FOLDER,            "$" },
  { "shell-escape",	OP_SHELL_ESCAPE,		"!" },
  { "enter-command",	OP_ENTER_COMMAND,		":" },
  { "buffy-list",	OP_BUFFY_LIST,			"." },
  { "search",		OP_SEARCH,			"/" },
  { "search-reverse",	OP_SEARCH_REVERSE,		"\033/" },
  { "search-opposite",	OP_SEARCH_OPPOSITE,		NULL },
  { "next-line",	OP_NEXT_LINE,			M_ENTER_S },
  { "jump",		OP_JUMP,			NULL },
  { "next-unread",	OP_MAIN_NEXT_UNREAD,		NULL },
  { "previous-new",	OP_MAIN_PREV_NEW,		NULL },
  { "previous-new-then-unread",
      			OP_MAIN_PREV_NEW_THEN_UNREAD,   NULL },
  { "previous-unread",	OP_MAIN_PREV_UNREAD,		NULL },
  { "half-up",		OP_HALF_UP,			NULL },
  { "half-down",	OP_HALF_DOWN,			NULL },
  { "previous-line",	OP_PREV_LINE,			NULL },
  { "bottom",		OP_PAGER_BOTTOM,		NULL },
  { "parent-message",	OP_MAIN_PARENT_MESSAGE,		"P" },




  { "check-traditional-pgp",	OP_CHECK_TRADITIONAL,	"\033P"   },
  { "mail-key",		OP_MAIL_KEY,			"\033k" },
  { "extract-keys",	OP_EXTRACT_KEYS,		"\013" },
  { "forget-passphrase",OP_FORGET_PASSPHRASE,		"\006" },
  { "decrypt-copy",	OP_DECRYPT_COPY,		NULL },
  { "decrypt-save",    	OP_DECRYPT_SAVE,		NULL },

  { "what-key",		OP_WHAT_KEY,		NULL },

  { NULL,		0,				NULL }
};

struct binding_t OpAttach[] = { /* map: attach */
  { "bounce-message",	OP_BOUNCE_MESSAGE,		"b" },
  { "display-toggle-weed",	OP_DISPLAY_HEADERS,	"h" },
  { "edit-type",	OP_EDIT_TYPE,			"\005" },
  { "print-entry",	OP_PRINT,			"p" },
  { "save-entry",	OP_SAVE,			"s" },
  { "pipe-entry",	OP_PIPE,			"|" },
  { "view-mailcap",	OP_ATTACH_VIEW_MAILCAP,		"m" },
  { "reply",		OP_REPLY,			"r" },
  { "resend-message",	OP_RESEND,			"\033e" },
  { "group-reply",	OP_GROUP_REPLY,			"g" },
  { "list-reply",	OP_LIST_REPLY,			"L" },
  { "forward-message",	OP_FORWARD_MESSAGE,		"f" },
  { "view-text",	OP_ATTACH_VIEW_TEXT,		"T" },
  { "view-attach",	OP_VIEW_ATTACH,			M_ENTER_S },
  { "delete-entry",	OP_DELETE,			"d" },
  { "undelete-entry",	OP_UNDELETE,			"u" },
  { "collapse-parts",	OP_ATTACH_COLLAPSE,		"v" },

  { "check-traditional-pgp",	OP_CHECK_TRADITIONAL,		"\033P"   },
  { "extract-keys",		OP_EXTRACT_KEYS,		"\013" },
  { "forget-passphrase",	OP_FORGET_PASSPHRASE,		"\006" },

  { NULL,		0,				NULL }
};

struct binding_t OpCompose[] = { /* map: compose */
  { "attach-file",	OP_COMPOSE_ATTACH_FILE,		"a" },
  { "attach-message",	OP_COMPOSE_ATTACH_MESSAGE,	"A" },
  { "edit-bcc",		OP_COMPOSE_EDIT_BCC,		"b" },
  { "edit-cc",		OP_COMPOSE_EDIT_CC,		"c" },
  { "copy-file",	OP_SAVE,			"C" },
  { "detach-file",	OP_DELETE,			"D" },
  { "toggle-disposition",OP_COMPOSE_TOGGLE_DISPOSITION,	"\004" },
  { "edit-description",	OP_COMPOSE_EDIT_DESCRIPTION,	"d" },
  { "edit-message",	OP_COMPOSE_EDIT_MESSAGE,	"e" },
  { "edit-headers",	OP_COMPOSE_EDIT_HEADERS,	"E" },
  { "edit-file",	OP_COMPOSE_EDIT_FILE,		"\030e" },
  { "edit-encoding",	OP_COMPOSE_EDIT_ENCODING,	"\005" },
  { "edit-from",	OP_COMPOSE_EDIT_FROM,		"\033f" },
  { "edit-fcc",		OP_COMPOSE_EDIT_FCC,		"f" },
  { "filter-entry",	OP_FILTER,			"F" },
  { "get-attachment",	OP_COMPOSE_GET_ATTACHMENT,	"G" },
  { "display-toggle-weed",	OP_DISPLAY_HEADERS,		"h" },
  { "ispell",		OP_COMPOSE_ISPELL,		"i" },
  { "print-entry",	OP_PRINT,			"l" },
  { "edit-mime",	OP_COMPOSE_EDIT_MIME,		"m" },
  { "new-mime",		OP_COMPOSE_NEW_MIME,		"n" },
  { "postpone-message",	OP_COMPOSE_POSTPONE_MESSAGE,	"P" },
  { "edit-reply-to",	OP_COMPOSE_EDIT_REPLY_TO,	"r" },
  { "rename-file",	OP_COMPOSE_RENAME_FILE,		"R" },
  { "edit-subject",	OP_COMPOSE_EDIT_SUBJECT,	"s" },
  { "edit-to",		OP_COMPOSE_EDIT_TO,		"t" },
  { "edit-type",	OP_EDIT_TYPE,			"\024" },
  { "write-fcc",	OP_COMPOSE_WRITE_MESSAGE,	"w" },
  { "toggle-unlink",	OP_COMPOSE_TOGGLE_UNLINK,	"u" },
  { "toggle-recode",    OP_COMPOSE_TOGGLE_RECODE,	NULL },
  { "update-encoding",	OP_COMPOSE_UPDATE_ENCODING,	"U" },
  { "view-attach",	OP_VIEW_ATTACH,			M_ENTER_S },
  { "send-message",	OP_COMPOSE_SEND_MESSAGE,	"y" },
  { "pipe-entry",	OP_PIPE,			"|" },

  { "attach-key",	OP_COMPOSE_ATTACH_KEY,		"\033k" },
  { "pgp-menu",		OP_COMPOSE_PGP_MENU,		"p" 	},

  { "forget-passphrase",OP_FORGET_PASSPHRASE,		"\006"  },

  { "smime-menu",	OP_COMPOSE_SMIME_MENU,		"S" 	},

#ifdef MIXMASTER
  { "mix",		OP_COMPOSE_MIX,			"M" },
#endif
  
  { NULL,		0,				NULL }
};

struct binding_t OpPost[] = { /* map: postpone */
  { "delete-entry",	OP_DELETE,	"d" },
  { "undelete-entry",	OP_UNDELETE,	"u" },
  { NULL,		0,		NULL }
};

struct binding_t OpAlias[] = { /* map: alias */
  { "delete-entry",	OP_DELETE,	"d" },
  { "undelete-entry",	OP_UNDELETE,	"u" },
  { NULL,		0,		NULL }
};
  

/* The file browser */
struct binding_t OpBrowser[] = { /* map: browser */
  { "change-dir",	OP_CHANGE_DIRECTORY,	"c" },
  { "display-filename",	OP_BROWSER_TELL,	"@" },
  { "enter-mask",	OP_ENTER_MASK,		"m" },
  { "sort",		OP_SORT,		"o" },
  { "sort-reverse",	OP_SORT_REVERSE,	"O" },
  { "select-new",	OP_BROWSER_NEW_FILE,	"N" },
  { "check-new",	OP_CHECK_NEW,		NULL },
  { "toggle-mailboxes", OP_TOGGLE_MAILBOXES, 	"\t" },
  { "view-file",	OP_BROWSER_VIEW_FILE,	" " },
  { "buffy-list",	OP_BUFFY_LIST,		"." },
#ifdef USE_IMAP
  { "create-mailbox",   OP_CREATE_MAILBOX,      "C" },
  { "delete-mailbox",   OP_DELETE_MAILBOX,      "d" },
  { "rename-mailbox",   OP_RENAME_MAILBOX,      "r" },
  { "subscribe",	OP_BROWSER_SUBSCRIBE,	"s" },
  { "unsubscribe",	OP_BROWSER_UNSUBSCRIBE,	"u" },
  { "toggle-subscribed", OP_BROWSER_TOGGLE_LSUB, "T" },
#endif
  { NULL,		0,			NULL }
};

/* External Query Menu */
struct binding_t OpQuery[] = { /* map: query */
  { "create-alias",	OP_CREATE_ALIAS,	"a" },
  { "mail",		OP_MAIL,		"m" },
  { "query",		OP_QUERY,		"Q" },
  { "query-append",	OP_QUERY_APPEND,	"A" },
  { NULL,		0,			NULL }
};

struct binding_t OpEditor[] = { /* map: editor */
  { "bol",		OP_EDITOR_BOL,			"\001" },
  { "backward-char",	OP_EDITOR_BACKWARD_CHAR,	"\002" },
  { "backward-word",	OP_EDITOR_BACKWARD_WORD,	"\033b"},
  { "capitalize-word",	OP_EDITOR_CAPITALIZE_WORD,	"\033c"},
  { "downcase-word",	OP_EDITOR_DOWNCASE_WORD,	"\033l"},
  { "upcase-word",	OP_EDITOR_UPCASE_WORD,		"\033u"},
  { "delete-char",	OP_EDITOR_DELETE_CHAR,		"\004" },
  { "eol",		OP_EDITOR_EOL,			"\005" },
  { "forward-char",	OP_EDITOR_FORWARD_CHAR,		"\006" },
  { "forward-word",	OP_EDITOR_FORWARD_WORD,		"\033f"},
  { "backspace",	OP_EDITOR_BACKSPACE,		"\010" },
  { "kill-eol",		OP_EDITOR_KILL_EOL,		"\013" },
  { "kill-eow",		OP_EDITOR_KILL_EOW,		"\033d"},
  { "kill-line",	OP_EDITOR_KILL_LINE,		"\025" },
  { "quote-char",	OP_EDITOR_QUOTE_CHAR,		"\026" },
  { "kill-word",	OP_EDITOR_KILL_WORD,		"\027" },
  { "complete",		OP_EDITOR_COMPLETE,		"\t"   },
  { "complete-query",	OP_EDITOR_COMPLETE_QUERY,	"\024" },
  { "buffy-cycle",	OP_EDITOR_BUFFY_CYCLE,		" "    },
  { "history-up",	OP_EDITOR_HISTORY_UP,		NULL   },
  { "history-down",	OP_EDITOR_HISTORY_DOWN,		NULL   },
  { "transpose-chars",	OP_EDITOR_TRANSPOSE_CHARS,	NULL   },
  { NULL,		0,				NULL   }
};



struct binding_t OpPgp[] = { /* map: pgp */
  { "verify-key",	OP_VERIFY_KEY,		"c" },
  { "view-name",	OP_VIEW_ID,		"%" },
  { NULL,		0,				NULL }
};



/* When using the GPGME based backend we have some useful functions
   for the SMIME menu.  */
struct binding_t OpSmime[] = { /* map: smime */
#ifdef CRYPT_BACKEND_GPGME
  { "verify-key",    OP_VERIFY_KEY,             "c" },
  { "view-name",     OP_VIEW_ID,	        "%" },
#endif
  { NULL,	0,	NULL }
};



#ifdef MIXMASTER
struct binding_t OpMix[] = { /* map: mix */
  { "accept",		OP_MIX_USE,	M_ENTER_S },
  { "append",		OP_MIX_APPEND,	"a"       },
  { "insert",		OP_MIX_INSERT,	"i"       },
  { "delete",		OP_MIX_DELETE,  "d"	  },
  { "chain-prev",	OP_MIX_CHAIN_PREV, "<left>" },
  { "chain-next",	OP_MIX_CHAIN_NEXT, "<right>" },
  { NULL, 		0, 		NULL }
};
#endif /* MIXMASTER */
