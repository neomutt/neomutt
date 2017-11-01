/**
 * @file
 * Handling of global boolean variables
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_OPTIONS_H_
#define _MUTT_OPTIONS_H_

/**
 * enum GlobalBool - boolean vars
 */
enum GlobalBool
{
  OPT_ALLOW_8BIT,
  OPT_ALLOW_ANSI,
  OPT_ARROW_CURSOR,
  OPT_ASCII_CHARS,
  OPT_ASKBCC,
  OPT_ASKCC,
  OPT_ASK_FOLLOW_UP,
  OPT_ASK_X_COMMENT_TO,
  OPT_ATTACH_SPLIT,
  OPT_AUTOEDIT,
  OPT_AUTO_TAG,
  OPT_BEEP,
  OPT_BEEP_NEW,
  OPT_BOUNCE_DELIVERED,
  OPT_BRAILLE_FRIENDLY,
  OPT_CHECK_MBOX_SIZE,
  OPT_CHECK_NEW,
  OPT_COLLAPSE_ALL,
  OPT_COLLAPSE_UNREAD,
  OPT_COLLAPSE_FLAGGED,
  OPT_CONFIRMAPPEND,
  OPT_CONFIRMCREATE,
  OPT_DELETE_UNTAG,
  OPT_DIGEST_COLLAPSE,
  OPT_DUPLICATE_THREADS,
  OPT_EDIT_HEADERS,
  OPT_ENCODE_FROM,
  OPT_USE_ENVELOPE_FROM,
  OPT_FAST_REPLY,
  OPT_FCC_CLEAR,
  OPT_FLAG_SAFE,
  OPT_FOLLOWUP_TO,
  OPT_FORCE_NAME,
  OPT_FORWARD_DECODE,
  OPT_FORWARD_QUOTE,
  OPT_FORWARD_REFERENCES,
#ifdef USE_HCACHE
  OPT_MAILDIR_HEADER_CACHE_VERIFY,
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
  OPT_HEADER_CACHE_COMPRESS,
#endif /* HAVE_QDBM */
#endif
  OPT_HDRS,
  OPT_HEADER,
  OPT_HEADER_COLOR_PARTIAL,
  OPT_HELP,
  OPT_HIDDEN_HOST,
  OPT_HIDE_LIMITED,
  OPT_HIDE_MISSING,
  OPT_HIDE_THREAD_SUBJECT,
  OPT_HIDE_TOP_LIMITED,
  OPT_HIDE_TOP_MISSING,
  OPT_HISTORY_REMOVE_DUPS,
  OPT_HONOR_DISPOSITION,
  OPT_IGNORE_LINEAR_WHITE_SPACE,
  OPT_IGNORE_LIST_REPLY_TO,
#ifdef USE_IMAP
  OPT_IMAP_CHECK_SUBSCRIBED,
  OPT_IMAP_IDLE,
  OPT_IMAP_LIST_SUBSCRIBED,
  OPT_IMAP_PASSIVE,
  OPT_IMAP_PEEK,
  OPT_IMAP_SERVERNOISE,
#endif
#ifdef USE_SSL
#ifndef USE_SSL_GNUTLS
  OPT_SSL_USESYSTEMCERTS,
  OPT_SSL_USE_SSLV2,
#endif /* USE_SSL_GNUTLS */
  OPT_SSL_USE_SSLV3,
  OPT_SSL_USE_TLSV1,
  OPT_SSL_USE_TLSV1_1,
  OPT_SSL_USE_TLSV1_2,
  OPT_SSL_FORCE_TLS,
  OPT_SSL_VERIFY_DATES,
  OPT_SSL_VERIFY_HOST,
#if defined(USE_SSL_OPENSSL) && defined(HAVE_SSL_PARTIAL_CHAIN)
  OPT_SSL_VERIFY_PARTIAL_CHAINS,
#endif /* USE_SSL_OPENSSL */
#endif /* defined(USE_SSL) */
  OPT_IMPLICIT_AUTOVIEW,
  OPT_INCLUDE_ONLYFIRST,
  OPT_KEEP_FLAGGED,
  OPT_MAILCAP_SANITIZE,
  OPT_MAIL_CHECK_RECENT,
  OPT_MAIL_CHECK_STATS,
  OPT_MAILDIR_TRASH,
  OPT_MAILDIR_CHECK_CUR,
  OPT_MARKERS,
  OPT_MARK_OLD,
  OPT_MENU_SCROLL,  /**< scroll menu instead of implicit next-page */
  OPT_MENU_MOVE_OFF, /**< allow menu to scroll past last entry */
#if defined(USE_IMAP) || defined(USE_POP)
  OPT_MESSAGE_CACHE_CLEAN,
#endif
  OPT_META_KEY, /**< interpret ALT-x as ESC-x */
  OPT_METOO,
  OPT_MH_PURGE,
  OPT_MIME_FORWARD_DECODE,
  OPT_MIME_TYPE_QUERY_FIRST,
#ifdef USE_NNTP
  OPT_MIME_SUBJECT, /**< encode subject line with RFC2047 */
#endif
  OPT_NARROW_TREE,
  OPT_PAGER_STOP,
  OPT_PIPE_DECODE,
  OPT_PIPE_SPLIT,
#ifdef USE_POP
  OPT_POP_AUTH_TRY_ALL,
  OPT_POP_LAST,
#endif
  OPT_POSTPONE_ENCRYPT,
  OPT_PRINT_DECODE,
  OPT_PRINT_SPLIT,
  OPT_PROMPT_AFTER,
  OPT_READ_ONLY,
  OPT_REFLOW_SPACE_QUOTES,
  OPT_REFLOW_TEXT,
  OPT_REPLY_SELF,
  OPT_REPLY_WITH_XORIG,
  OPT_RESOLVE,
  OPT_RESUME_DRAFT_FILES,
  OPT_RESUME_EDITED_DRAFT_FILES,
  OPT_REVERSE_ALIAS,
  OPT_REVERSE_NAME,
  OPT_REVERSE_REALNAME,
  OPT_RFC2047_PARAMETERS,
  OPT_SAVE_ADDRESS,
  OPT_SAVE_EMPTY,
  OPT_SAVE_NAME,
  OPT_SCORE,
#ifdef USE_SIDEBAR
  OPT_SIDEBAR_VISIBLE,
  OPT_SIDEBAR_FOLDER_INDENT,
  OPT_SIDEBAR_NEW_MAIL_ONLY,
  OPT_SIDEBAR_NEXT_NEW_WRAP,
  OPT_SIDEBAR_SHORT_PATH,
  OPT_SIDEBAR_ON_RIGHT,
#endif
  OPT_SIG_DASHES,
  OPT_SIG_ON_TOP,
  OPT_SORT_RE,
  OPT_STATUS_ON_TOP,
  OPT_STRICT_THREADS,
  OPT_SUSPEND,
  OPT_TEXT_FLOWED,
  OPT_THOROUGH_SEARCH,
  OPT_THREAD_RECEIVED,
  OPT_TILDE,
  OPT_TS_ENABLED,
  OPT_UNCOLLAPSE_JUMP,
  OPT_UNCOLLAPSE_NEW,
  OPT_USE_8BITMIME,
  OPT_USE_DOMAIN,
  OPT_USE_FROM,
  OPT_PGP_USE_GPG_AGENT,
#ifdef HAVE_LIBIDN
  OPT_IDN_DECODE,
  OPT_IDN_ENCODE,
#endif
#ifdef HAVE_GETADDRINFO
  OPT_USE_IPV6,
#endif
  OPT_WAIT_KEY,
  OPT_WEED,
  OPT_SMART_WRAP,
  OPT_WRAP_SEARCH,
  OPT_WRITE_BCC, /**< write out a bcc header? */
  OPT_USER_AGENT,

  OPT_CRYPT_USE_GPGME,
  OPT_CRYPT_USE_PKA,

  /* PGP options */

  OPT_CRYPT_AUTOSIGN,
  OPT_CRYPT_AUTOENCRYPT,
  OPT_CRYPT_AUTOPGP,
  OPT_CRYPT_AUTOSMIME,
  OPT_CRYPT_CONFIRMHOOK,
  OPT_CRYPT_OPPORTUNISTIC_ENCRYPT,
  OPT_CRYPT_REPLYENCRYPT,
  OPT_CRYPT_REPLYSIGN,
  OPT_CRYPT_REPLYSIGNENCRYPTED,
  OPT_CRYPT_TIMESTAMP,
  OPT_SMIME_IS_DEFAULT,
  OPT_SMIME_SELF_ENCRYPT,
  OPT_SMIME_ASK_CERT_LABEL,
  OPT_SMIME_DECRYPT_USE_DEFAULT_KEY,
  OPT_PGP_IGNORE_SUBKEYS,
  OPT_PGP_CHECK_EXIT,
  OPT_PGP_LONG_IDS,
  OPT_PGP_AUTO_DECODE,
  OPT_PGP_RETAINABLE_SIGS,
  OPT_PGP_SELF_ENCRYPT,
  OPT_PGP_STRICT_ENC,
  OPT_FORWARD_DECRYPT,
  OPT_PGP_SHOW_UNUSABLE,
  OPT_PGP_AUTOINLINE,
  OPT_PGP_REPLYINLINE,

/* news options */

#ifdef USE_NNTP
  OPT_SHOW_NEW_NEWS,
  OPT_SHOW_ONLY_UNREAD,
  OPT_SAVE_UNSUBSCRIBED,
  OPT_NNTP_LISTGROUP,
  OPT_NNTP_LOAD_DESCRIPTION,
  OPT_X_COMMENT_TO,
#endif

  /* pseudo options */

  OPT_AUX_SORT,           /**< (pseudo) using auxiliary sort function */
  OPT_FORCE_REFRESH,      /**< (pseudo) refresh even during macros */
  OPT_NO_CURSES,          /**< (pseudo) when sending in batch mode */
  OPT_SEARCH_REVERSE,     /**< (pseudo) used by ci_search_command */
  OPT_MSG_ERR,            /**< (pseudo) used by mutt_error/mutt_message */
  OPT_SEARCH_INVALID,     /**< (pseudo) used to invalidate the search pat */
  OPT_SIGNALS_BLOCKED,    /**< (pseudo) using by mutt_block_signals () */
  OPT_SYS_SIGNALS_BLOCKED, /**< (pseudo) using by mutt_block_signals_system () */
  OPT_NEED_RESORT,        /**< (pseudo) used to force a re-sort */
  OPT_RESORT_INIT,        /**< (pseudo) used to force the next resort to be from scratch */
  OPT_VIEW_ATTACH,        /**< (pseudo) signals that we are viewing attachments */
  OPT_SORT_SUBTHREADS,    /**< (pseudo) used when $sort_aux changes */
  OPT_NEED_RESCORE,       /**< (pseudo) set when the `score' command is used */
  OPT_ATTACH_MSG,         /**< (pseudo) used by attach-message */
  OPT_HIDE_READ,          /**< (pseudo) whether or not hide read messages */
  OPT_KEEP_QUIET,         /**< (pseudo) shut up the message and refresh
                         *            functions while we are executing an
                         *            external program.  */
  OPT_MENU_CALLER,        /**< (pseudo) tell menu to give caller a take */
  OPT_REDRAW_TREE,        /**< (pseudo) redraw the thread tree */
  OPT_PGP_CHECK_TRUST,     /**< (pseudo) used by pgp_select_key () */
  OPT_DONT_HANDLE_PGP_KEYS, /**< (pseudo) used to extract PGP keys */
  OPT_IGNORE_MACRO_EVENTS, /**< (pseudo) don't process macro/push/exec events while set */

#ifdef USE_NNTP
  OPT_NEWS,              /**< (pseudo) used to change reader mode */
  OPT_NEWS_SEND,          /**< (pseudo) used to change behavior when posting */
#endif
#ifdef USE_NOTMUCH
  OPT_VIRTUAL_SPOOLFILE,
  OPT_NM_RECORD,
#endif

  OPT_GLOBAL_MAX
};

#define mutt_bit_set(v, n)    v[n / 8] |= (1 << (n % 8))
#define mutt_bit_unset(v, n)  v[n / 8] &= ~(1 << (n % 8))
#define mutt_bit_toggle(v, n) v[n / 8] ^= (1 << (n % 8))
#define mutt_bit_isset(v, n)  (v[n / 8] & (1 << (n % 8)))

/* bit vector for boolean variables */
#ifdef MAIN_C
unsigned char Options[(OPT_GLOBAL_MAX + 7) / 8];
#else
extern unsigned char Options[];
#endif

#define set_option(x) mutt_bit_set(Options, x)
#define unset_option(x) mutt_bit_unset(Options, x)
#define toggle_option(x) mutt_bit_toggle(Options, x)
#define option(x) mutt_bit_isset(Options, x)

#endif /* _MUTT_OPTIONS_H_ */
