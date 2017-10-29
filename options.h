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

#include <stdbool.h>
#include "where.h"

WHERE bool OPT_ALLOW_8BIT;
WHERE bool OPT_ALLOW_ANSI;
WHERE bool OPT_ARROW_CURSOR;
WHERE bool OPT_ASCII_CHARS;
WHERE bool OPT_ASKBCC;
WHERE bool OPT_ASKCC;
WHERE bool OPT_ASK_FOLLOW_UP;
WHERE bool OPT_ASK_X_COMMENT_TO;
WHERE bool OPT_ATTACH_SPLIT;
WHERE bool OPT_AUTOEDIT;
WHERE bool OPT_AUTO_TAG;
WHERE bool OPT_BEEP;
WHERE bool OPT_BEEP_NEW;
WHERE bool OPT_BOUNCE_DELIVERED;
WHERE bool OPT_BRAILLE_FRIENDLY;
WHERE bool OPT_CHANGE_FOLDER_NEXT;
WHERE bool OPT_CHECK_MBOX_SIZE;
WHERE bool OPT_CHECK_NEW;
WHERE bool OPT_COLLAPSE_ALL;
WHERE bool OPT_COLLAPSE_UNREAD;
WHERE bool OPT_COLLAPSE_FLAGGED;
WHERE bool OPT_CONFIRMAPPEND;
WHERE bool OPT_CONFIRMCREATE;
WHERE bool OPT_DELETE_UNTAG;
WHERE bool OPT_DIGEST_COLLAPSE;
WHERE bool OPT_DUPLICATE_THREADS;
WHERE bool OPT_EDIT_HEADERS;
WHERE bool OPT_ENCODE_FROM;
WHERE bool OPT_USE_ENVELOPE_FROM;
WHERE bool OPT_FAST_REPLY;
WHERE bool OPT_FCC_CLEAR;
WHERE bool OPT_FLAG_SAFE;
WHERE bool OPT_FOLLOWUP_TO;
WHERE bool OPT_FORCE_NAME;
WHERE bool OPT_FORWARD_DECODE;
WHERE bool OPT_FORWARD_QUOTE;
WHERE bool OPT_FORWARD_REFERENCES;
#ifdef USE_HCACHE
WHERE bool OPT_MAILDIR_HEADER_CACHE_VERIFY;
#if defined(HAVE_QDBM) || defined(HAVE_TC) || defined(HAVE_KC)
WHERE bool OPT_HEADER_CACHE_COMPRESS;
#endif /* HAVE_QDBM */
#endif
WHERE bool OPT_HDRS;
WHERE bool OPT_HEADER;
WHERE bool OPT_HEADER_COLOR_PARTIAL;
WHERE bool OPT_HELP;
WHERE bool OPT_HIDDEN_HOST;
WHERE bool OPT_HIDE_LIMITED;
WHERE bool OPT_HIDE_MISSING;
WHERE bool OPT_HIDE_THREAD_SUBJECT;
WHERE bool OPT_HIDE_TOP_LIMITED;
WHERE bool OPT_HIDE_TOP_MISSING;
WHERE bool OPT_HISTORY_REMOVE_DUPS;
WHERE bool OPT_HONOR_DISPOSITION;
WHERE bool OPT_IGNORE_LINEAR_WHITE_SPACE;
WHERE bool OPT_IGNORE_LIST_REPLY_TO;
#ifdef USE_IMAP
WHERE bool OPT_IMAP_CHECK_SUBSCRIBED;
WHERE bool OPT_IMAP_IDLE;
WHERE bool OPT_IMAP_LIST_SUBSCRIBED;
WHERE bool OPT_IMAP_PASSIVE;
WHERE bool OPT_IMAP_PEEK;
WHERE bool OPT_IMAP_SERVERNOISE;
#endif
#ifdef USE_SSL
#ifndef USE_SSL_GNUTLS
WHERE bool OPT_SSL_USESYSTEMCERTS;
WHERE bool OPT_SSL_USE_SSLV2;
#endif /* USE_SSL_GNUTLS */
WHERE bool OPT_SSL_USE_SSLV3;
WHERE bool OPT_SSL_USE_TLSV1;
WHERE bool OPT_SSL_USE_TLSV1_1;
WHERE bool OPT_SSL_USE_TLSV1_2;
WHERE bool OPT_SSL_FORCE_TLS;
WHERE bool OPT_SSL_VERIFY_DATES;
WHERE bool OPT_SSL_VERIFY_HOST;
#if defined(USE_SSL_OPENSSL) && defined(HAVE_SSL_PARTIAL_CHAIN)
WHERE bool OPT_SSL_VERIFY_PARTIAL_CHAINS;
#endif /* USE_SSL_OPENSSL */
#endif /* defined(USE_SSL) */
WHERE bool OPT_IMPLICIT_AUTOVIEW;
WHERE bool OPT_INCLUDE_ONLYFIRST;
WHERE bool OPT_KEEP_FLAGGED;
WHERE bool OPT_MAILCAP_SANITIZE;
WHERE bool OPT_MAIL_CHECK_RECENT;
WHERE bool OPT_MAIL_CHECK_STATS;
WHERE bool OPT_MAILDIR_TRASH;
WHERE bool OPT_MAILDIR_CHECK_CUR;
WHERE bool OPT_MARKERS;
WHERE bool OPT_MARK_OLD;
WHERE bool OPT_MENU_SCROLL;  /**< scroll menu instead of implicit next-page */
WHERE bool OPT_MENU_MOVE_OFF; /**< allow menu to scroll past last entry */
#if defined(USE_IMAP) || defined(USE_POP)
WHERE bool OPT_MESSAGE_CACHE_CLEAN;
#endif
WHERE bool OPT_META_KEY; /**< interpret ALT-x as ESC-x */
WHERE bool OPT_METOO;
WHERE bool OPT_MH_PURGE;
WHERE bool OPT_MIME_FORWARD_DECODE;
WHERE bool OPT_MIME_TYPE_QUERY_FIRST;
#ifdef USE_NNTP
WHERE bool OPT_MIME_SUBJECT; /**< encode subject line with RFC2047 */
#endif
WHERE bool OPT_NARROW_TREE;
WHERE bool OPT_PAGER_STOP;
WHERE bool OPT_PIPE_DECODE;
WHERE bool OPT_PIPE_SPLIT;
#ifdef USE_POP
WHERE bool OPT_POP_AUTH_TRY_ALL;
WHERE bool OPT_POP_LAST;
#endif
WHERE bool OPT_POSTPONE_ENCRYPT;
WHERE bool OPT_PRINT_DECODE;
WHERE bool OPT_PRINT_SPLIT;
WHERE bool OPT_PROMPT_AFTER;
WHERE bool OPT_READ_ONLY;
WHERE bool OPT_REFLOW_SPACE_QUOTES;
WHERE bool OPT_REFLOW_TEXT;
WHERE bool OPT_REPLY_SELF;
WHERE bool OPT_REPLY_WITH_XORIG;
WHERE bool OPT_RESOLVE;
WHERE bool OPT_RESUME_DRAFT_FILES;
WHERE bool OPT_RESUME_EDITED_DRAFT_FILES;
WHERE bool OPT_REVERSE_ALIAS;
WHERE bool OPT_REVERSE_NAME;
WHERE bool OPT_REVERSE_REALNAME;
WHERE bool OPT_RFC2047_PARAMETERS;
WHERE bool OPT_SAVE_ADDRESS;
WHERE bool OPT_SAVE_EMPTY;
WHERE bool OPT_SAVE_NAME;
WHERE bool OPT_SCORE;
#ifdef USE_SIDEBAR
WHERE bool OPT_SIDEBAR_VISIBLE;
WHERE bool OPT_SIDEBAR_FOLDER_INDENT;
WHERE bool OPT_SIDEBAR_NEW_MAIL_ONLY;
WHERE bool OPT_SIDEBAR_NEXT_NEW_WRAP;
WHERE bool OPT_SIDEBAR_SHORT_PATH;
WHERE bool OPT_SIDEBAR_ON_RIGHT;
#endif
WHERE bool OPT_SIG_DASHES;
WHERE bool OPT_SIG_ON_TOP;
WHERE bool OPT_SORT_RE;
WHERE bool OPT_STATUS_ON_TOP;
WHERE bool OPT_STRICT_THREADS;
WHERE bool OPT_SUSPEND;
WHERE bool OPT_TEXT_FLOWED;
WHERE bool OPT_THOROUGH_SEARCH;
WHERE bool OPT_THREAD_RECEIVED;
WHERE bool OPT_TILDE;
WHERE bool OPT_TS_ENABLED;
WHERE bool OPT_UNCOLLAPSE_JUMP;
WHERE bool OPT_UNCOLLAPSE_NEW;
WHERE bool OPT_USE_8BITMIME;
WHERE bool OPT_USE_DOMAIN;
WHERE bool OPT_USE_FROM;
WHERE bool OPT_PGP_USE_GPG_AGENT;
#ifdef HAVE_LIBIDN
WHERE bool OPT_IDN_DECODE;
WHERE bool OPT_IDN_ENCODE;
#endif
#ifdef HAVE_GETADDRINFO
WHERE bool OPT_USE_IPV6;
#endif
WHERE bool OPT_WAIT_KEY;
WHERE bool OPT_WEED;
WHERE bool OPT_SMART_WRAP;
WHERE bool OPT_WRAP_SEARCH;
WHERE bool OPT_WRITE_BCC; /**< write out a bcc header? */
WHERE bool OPT_USER_AGENT;

WHERE bool OPT_CRYPT_USE_GPGME;
WHERE bool OPT_CRYPT_USE_PKA;

/* PGP options */

WHERE bool OPT_CRYPT_AUTOSIGN;
WHERE bool OPT_CRYPT_AUTOENCRYPT;
WHERE bool OPT_CRYPT_AUTOPGP;
WHERE bool OPT_CRYPT_AUTOSMIME;
WHERE bool OPT_CRYPT_CONFIRMHOOK;
WHERE bool OPT_CRYPT_OPPORTUNISTIC_ENCRYPT;
WHERE bool OPT_CRYPT_REPLYENCRYPT;
WHERE bool OPT_CRYPT_REPLYSIGN;
WHERE bool OPT_CRYPT_REPLYSIGNENCRYPTED;
WHERE bool OPT_CRYPT_TIMESTAMP;
WHERE bool OPT_SMIME_IS_DEFAULT;
WHERE bool OPT_SMIME_SELF_ENCRYPT;
WHERE bool OPT_SMIME_ASK_CERT_LABEL;
WHERE bool OPT_SMIME_DECRYPT_USE_DEFAULT_KEY;
WHERE bool OPT_PGP_IGNORE_SUBKEYS;
WHERE bool OPT_PGP_CHECK_EXIT;
WHERE bool OPT_PGP_LONG_IDS;
WHERE bool OPT_PGP_AUTO_DECODE;
WHERE bool OPT_PGP_RETAINABLE_SIGS;
WHERE bool OPT_PGP_SELF_ENCRYPT;
WHERE bool OPT_PGP_STRICT_ENC;
WHERE bool OPT_FORWARD_DECRYPT;
WHERE bool OPT_PGP_SHOW_UNUSABLE;
WHERE bool OPT_PGP_AUTOINLINE;
WHERE bool OPT_PGP_REPLYINLINE;

/* news options */

#ifdef USE_NNTP
WHERE bool OPT_SHOW_NEW_NEWS;
WHERE bool OPT_SHOW_ONLY_UNREAD;
WHERE bool OPT_SAVE_UNSUBSCRIBED;
WHERE bool OPT_NNTP_LISTGROUP;
WHERE bool OPT_NNTP_LOAD_DESCRIPTION;
WHERE bool OPT_X_COMMENT_TO;
#endif

/* pseudo options */

WHERE bool OPT_AUX_SORT;           /**< (pseudo) using auxiliary sort function */
WHERE bool OPT_FORCE_REFRESH;      /**< (pseudo) refresh even during macros */
WHERE bool OPT_NO_CURSES;          /**< (pseudo) when sending in batch mode */
WHERE bool OPT_SEARCH_REVERSE;     /**< (pseudo) used by ci_search_command */
WHERE bool OPT_MSG_ERR;            /**< (pseudo) used by mutt_error/mutt_message */
WHERE bool OPT_SEARCH_INVALID;     /**< (pseudo) used to invalidate the search pat */
WHERE bool OPT_NEED_RESORT;        /**< (pseudo) used to force a re-sort */
WHERE bool OPT_RESORT_INIT;        /**< (pseudo) used to force the next resort to be from scratch */
WHERE bool OPT_VIEW_ATTACH;        /**< (pseudo) signals that we are viewing attachments */
WHERE bool OPT_SORT_SUBTHREADS;    /**< (pseudo) used when $sort_aux changes */
WHERE bool OPT_NEED_RESCORE;       /**< (pseudo) set when the `score' command is used */
WHERE bool OPT_ATTACH_MSG;         /**< (pseudo) used by attach-message */
WHERE bool OPT_HIDE_READ;          /**< (pseudo) whether or not hide read messages */
WHERE bool OPT_KEEP_QUIET;         /**< (pseudo) shut up the message and refresh functions while we are executing an external program.  */
WHERE bool OPT_MENU_CALLER;        /**< (pseudo) tell menu to give caller a take */
WHERE bool OPT_REDRAW_TREE;        /**< (pseudo) redraw the thread tree */
WHERE bool OPT_PGP_CHECK_TRUST;     /**< (pseudo) used by pgp_select_key () */
WHERE bool OPT_DONT_HANDLE_PGP_KEYS; /**< (pseudo) used to extract PGP keys */
WHERE bool OPT_IGNORE_MACRO_EVENTS; /**< (pseudo) don't process macro/push/exec events while set */

#ifdef USE_NNTP
WHERE bool OPT_NEWS;              /**< (pseudo) used to change reader mode */
WHERE bool OPT_NEWS_SEND;          /**< (pseudo) used to change behavior when posting */
#endif
#ifdef USE_NOTMUCH
WHERE bool OPT_VIRTUAL_SPOOLFILE;
WHERE bool OPT_NM_RECORD;
#endif


#define mutt_bit_set(v, n)    v[n / 8] |= (1 << (n % 8))
#define mutt_bit_unset(v, n)  v[n / 8] &= ~(1 << (n % 8))
#define mutt_bit_toggle(v, n) v[n / 8] ^= (1 << (n % 8))
#define mutt_bit_isset(v, n)  (v[n / 8] & (1 << (n % 8)))

#endif /* _MUTT_OPTIONS_H_ */
