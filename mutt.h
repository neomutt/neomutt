/**
 * @file
 * Many unsorted constants and some structs
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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

#ifndef MUTT_MUTT_H
#define MUTT_MUTT_H

#include <stddef.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>

struct Buffer;
struct ListHead;
struct Mapping;

/* On OS X 10.5.x, wide char functions are inlined by default breaking
 * --without-wc-funcs compilation
 */
#ifdef __APPLE_CC__
#define _DONT_USE_CTYPE_INLINE_
#endif

/* PATH_MAX is undefined on the hurd */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef HAVE_FGETS_UNLOCKED
#define fgets fgets_unlocked
#endif

#ifdef HAVE_FGETC_UNLOCKED
#define fgetc fgetc_unlocked
#endif

/* flags for mutt_enter_string_full() */
#define MUTT_ALIAS    (1 << 0)  /**< do alias "completion" by calling up the alias-menu */
#define MUTT_FILE     (1 << 1)  /**< do file completion */
#define MUTT_EFILE    (1 << 2)  /**< do file completion, plus incoming folders */
#define MUTT_CMD      (1 << 3)  /**< do completion on previous word */
#define MUTT_PASS     (1 << 4)  /**< password mode (no echo) */
#define MUTT_CLEAR    (1 << 5)  /**< clear input if printable character is pressed */
#define MUTT_COMMAND  (1 << 6)  /**< do command completion */
#define MUTT_PATTERN  (1 << 7)  /**< pattern mode - only used for history classes */
#define MUTT_LABEL    (1 << 8)  /**< do label completion */
#define MUTT_NM_QUERY (1 << 9)  /**< Notmuch query mode. */
#define MUTT_NM_TAG   (1 << 10) /**< Notmuch tag +/- mode. */

/* flags for mutt_extract_token() */
#define MUTT_TOKEN_EQUAL         (1<<0)  /**< treat '=' as a special */
#define MUTT_TOKEN_CONDENSE      (1<<1)  /**< ^(char) to control chars (macros) */
#define MUTT_TOKEN_SPACE         (1<<2)  /**< don't treat whitespace as a term */
#define MUTT_TOKEN_QUOTE         (1<<3)  /**< don't interpret quotes */
#define MUTT_TOKEN_PATTERN       (1<<4)  /**< !)|~ are terms (for patterns) */
#define MUTT_TOKEN_COMMENT       (1<<5)  /**< don't reap comments */
#define MUTT_TOKEN_SEMICOLON     (1<<6)  /**< don't treat ; as special */
#define MUTT_TOKEN_BACKTICK_VARS (1<<7)  /**< expand variables within backticks */
#define MUTT_TOKEN_NOSHELL       (1<<8)  /**< don't expand environment variables */
#define MUTT_TOKEN_QUESTION      (1<<9)  /**< treat '?' as a special */

/* tree characters for linearize_tree and print_enriched_string */
#define MUTT_TREE_LLCORNER 1
#define MUTT_TREE_ULCORNER 2
#define MUTT_TREE_LTEE     3
#define MUTT_TREE_HLINE    4
#define MUTT_TREE_VLINE    5
#define MUTT_TREE_SPACE    6
#define MUTT_TREE_RARROW   7
#define MUTT_TREE_STAR     8
#define MUTT_TREE_HIDDEN   9
#define MUTT_TREE_EQUALS   10
#define MUTT_TREE_TTEE     11
#define MUTT_TREE_BTEE     12
#define MUTT_TREE_MISSING  13
#define MUTT_TREE_MAX      14

#define MUTT_SPECIAL_INDEX MUTT_TREE_MAX

/**
 * enum MuttMisc - Unsorted flags
 */
enum MuttMisc
{
  /* mutt_view_attachment() */
  MUTT_REGULAR = 1, ///< View using default method
  MUTT_MAILCAP,     ///< Force viewing using mailcap entry
  MUTT_AS_TEXT,     ///< Force viewing as text

  /* action codes used by mutt_set_flag() and mutt_pattern_func() */
  MUTT_ALL,        ///< All messages
  MUTT_NONE,       ///< No messages
  MUTT_NEW,        ///< New messages
  MUTT_OLD,        ///< Old messages
  MUTT_REPLIED,    ///< Messages that have been replied to
  MUTT_READ,       ///< Messages that have been read
  MUTT_UNREAD,     ///< Unread messages
  MUTT_DELETE,     ///< Messages to be deleted
  MUTT_UNDELETE,   ///< Messages to be un-deleted
  MUTT_PURGE,      ///< Messages to be purged (bypass trash)
  MUTT_DELETED,    ///< Deleted messages
  MUTT_FLAG,       ///< Flagged messages
  MUTT_TAG,        ///< Tagged messages
  MUTT_UNTAG,      ///< Messages to be un-tagged
  MUTT_LIMIT,      ///< Messages in limited view
  MUTT_EXPIRED,    ///< Expired messsages
  MUTT_SUPERSEDED, ///< Superseded messages
  MUTT_TRASH,      ///< Trashed messages

  /* actions for mutt_pattern_comp/mutt_pattern_exec */
  MUTT_AND,
  MUTT_OR,
  MUTT_THREAD,
  MUTT_PARENT,
  MUTT_CHILDREN,
  MUTT_TO,
  MUTT_CC,
  MUTT_COLLAPSED,
  MUTT_SUBJECT,
  MUTT_FROM,
  MUTT_DATE,
  MUTT_DATE_RECEIVED,
  MUTT_DUPLICATED,
  MUTT_UNREFERENCED,
  MUTT_BROKEN,
  MUTT_ID,
  MUTT_BODY,
  MUTT_HEADER,
  MUTT_HORMEL,
  MUTT_WHOLE_MSG,
  MUTT_SENDER,
  MUTT_MESSAGE,
  MUTT_SCORE,
  MUTT_SIZE,
  MUTT_REFERENCE,
  MUTT_RECIPIENT,
  MUTT_LIST,
  MUTT_SUBSCRIBED_LIST,
  MUTT_PERSONAL_RECIP,
  MUTT_PERSONAL_FROM,
  MUTT_ADDRESS,
  MUTT_CRYPT_SIGN,
  MUTT_CRYPT_VERIFIED,
  MUTT_CRYPT_ENCRYPT,
  MUTT_PGP_KEY,
  MUTT_XLABEL,
  MUTT_SERVERSEARCH,
  MUTT_DRIVER_TAGS,
  MUTT_MIMEATTACH,
  MUTT_MIMETYPE,
#ifdef USE_NNTP
  MUTT_NEWSGROUPS,
#endif

  /* Options for Mailcap lookup */
  MUTT_EDIT,     ///< Mailcap edit field
  MUTT_COMPOSE,  ///< Mailcap compose field
  MUTT_PRINT,    ///< Mailcap print field
  MUTT_AUTOVIEW, ///< Mailcap autoview field

  MUTT_SAVE_APPEND,    ///< Append to existing file - mutt_save_attachment()
  MUTT_SAVE_OVERWRITE, ///< Overwrite existing file - mutt_save_attachment()
};

/* flags for parse_spam_list */
#define MUTT_SPAM   1
#define MUTT_NOSPAM 2

int mutt_init(bool skip_sys_rc, struct ListHead *commands);
struct ConfigSet *init_config(size_t size);

/**
 * struct AttachMatch - An attachment matching a regex
 *
 * for attachment counter
 */
struct AttachMatch
{
  const char *major;
  int major_int;
  const char *minor;
  regex_t minor_regex;
};

#define EXECSHELL "/bin/sh"

int safe_asprintf(char **, const char *, ...);

char *mutt_compile_help(char *buf, size_t buflen, int menu, const struct Mapping *items);

int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, int flags);
void mutt_free_opts(void);
int query_quadoption(int opt, const char *prompt);
int mutt_label_complete(char *buf, size_t buflen, int numtabs);
int mutt_command_complete(char *buf, size_t buflen, int pos, int numtabs);
int mutt_var_value_complete(char *buf, size_t buflen, int pos);
void myvar_set(const char *var, const char *val);
bool mutt_nm_query_complete(char *buf, size_t buflen, int pos, int numtabs);
bool mutt_nm_tag_complete(char *buf, size_t buflen, int numtabs);
int mutt_dump_variables(bool hide_sensitive);
int mutt_get_hook_type(const char *name);
int mutt_parse_rc_line(/* const */ char *line, struct Buffer *token, struct Buffer *err);
int mutt_query_variables(struct ListHead *queries);
void reset_value(const char *name);

#endif /* MUTT_MUTT_H */
