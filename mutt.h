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
#include "config/lib.h"
#include "mutt_commands.h"

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

typedef uint16_t CompletionFlags;    ///< Flags for mutt_enter_string_full(), e.g. #MUTT_ALIAS
#define MUTT_COMP_NO_FLAGS       0   ///< No flags are set
#define MUTT_ALIAS         (1 << 0)  ///< Do alias "completion" by calling up the alias-menu
#define MUTT_FILE          (1 << 1)  ///< Do file completion
#define MUTT_EFILE         (1 << 2)  ///< Do file completion, plus incoming folders
#define MUTT_CMD           (1 << 3)  ///< Do completion on previous word
#define MUTT_PASS          (1 << 4)  ///< Password mode (no echo)
#define MUTT_CLEAR         (1 << 5)  ///< Clear input if printable character is pressed
#define MUTT_COMMAND       (1 << 6)  ///< Do command completion
#define MUTT_PATTERN       (1 << 7)  ///< Pattern mode - only used for history classes
#define MUTT_LABEL         (1 << 8)  ///< Do label completion
#define MUTT_NM_QUERY      (1 << 9)  ///< Notmuch query mode.
#define MUTT_NM_TAG        (1 << 10) ///< Notmuch tag +/- mode.

typedef uint16_t TokenFlags;               ///< Flags for mutt_extract_token(), e.g. #MUTT_TOKEN_EQUAL
#define MUTT_TOKEN_NO_FLAGS            0   ///< No flags are set
#define MUTT_TOKEN_EQUAL         (1 << 0)  ///< Treat '=' as a special
#define MUTT_TOKEN_CONDENSE      (1 << 1)  ///< ^(char) to control chars (macros)
#define MUTT_TOKEN_SPACE         (1 << 2)  ///< Don't treat whitespace as a term
#define MUTT_TOKEN_QUOTE         (1 << 3)  ///< Don't interpret quotes
#define MUTT_TOKEN_PATTERN       (1 << 4)  ///< !)|~ are terms (for patterns)
#define MUTT_TOKEN_COMMENT       (1 << 5)  ///< Don't reap comments
#define MUTT_TOKEN_SEMICOLON     (1 << 6)  ///< Don't treat ; as special
#define MUTT_TOKEN_BACKTICK_VARS (1 << 7)  ///< Expand variables within backticks
#define MUTT_TOKEN_NOSHELL       (1 << 8)  ///< Don't expand environment variables
#define MUTT_TOKEN_QUESTION      (1 << 9)  ///< Treat '?' as a special

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
  MUTT_EXPIRED,    ///< Expired messages
  MUTT_SUPERSEDED, ///< Superseded messages
  MUTT_TRASH,      ///< Trashed messages

  /* actions for mutt_pattern_comp/mutt_pattern_exec */
  MUTT_AND,             ///< Both patterns must match
  MUTT_OR,              ///< Either pattern can match
  MUTT_THREAD,          ///< Pattern matches email thread
  MUTT_PARENT,          ///< Pattern matches parent
  MUTT_CHILDREN,        ///< Pattern matches a child email
  MUTT_TO,              ///< Pattern matches 'To:' field
  MUTT_CC,              ///< Pattern matches 'Cc:' field
  MUTT_COLLAPSED,       ///< Thread is collapsed
  MUTT_SUBJECT,         ///< Pattern matches 'Subject:' field
  MUTT_FROM,            ///< Pattern matches 'From:' field
  MUTT_DATE,            ///< Pattern matches 'Date:' field
  MUTT_DATE_RECEIVED,   ///< Pattern matches date received
  MUTT_DUPLICATED,      ///< Duplicate message
  MUTT_UNREFERENCED,    ///< Message is unreferenced in the thread
  MUTT_BROKEN,          ///< Message is part of a broken thread
  MUTT_ID,              ///< Pattern matches email's Message-Id
  MUTT_ID_EXTERNAL,     ///< Message-Id is among results from an external query
  MUTT_BODY,            ///< Pattern matches email's body
  MUTT_HEADER,          ///< Pattern matches email's header
  MUTT_HORMEL,          ///< Pattern matches email's spam score
  MUTT_WHOLE_MSG,       ///< Pattern matches raw email text
  MUTT_SENDER,          ///< Pattern matches sender
  MUTT_MESSAGE,         ///< Pattern matches message number
  MUTT_SCORE,           ///< Pattern matches email's score
  MUTT_SIZE,            ///< Pattern matches email's size
  MUTT_REFERENCE,       ///< Pattern matches 'References:' or 'In-Reply-To:' field
  MUTT_RECIPIENT,       ///< User is a recipient of the email
  MUTT_LIST,            ///< Email is on mailing list
  MUTT_SUBSCRIBED_LIST, ///< Email is on subscribed mailing list
  MUTT_PERSONAL_RECIP,  ///< Email is addressed to the user
  MUTT_PERSONAL_FROM,   ///< Email is from the user
  MUTT_ADDRESS,         ///< Pattern matches any address field
  MUTT_CRYPT_SIGN,      ///< Message is signed
  MUTT_CRYPT_VERIFIED,  ///< Message is crypographically verified
  MUTT_CRYPT_ENCRYPT,   ///< Message is encrypted
  MUTT_PGP_KEY,         ///< Message has PGP key
  MUTT_XLABEL,          ///< Pattern matches keyword/label
  MUTT_SERVERSEARCH,    ///< Server-side pattern matches
  MUTT_DRIVER_TAGS,     ///< Pattern matches message tags
  MUTT_MIMEATTACH,      ///< Pattern matches number of attachments
  MUTT_MIMETYPE,        ///< Pattern matches MIME type
#ifdef USE_NNTP
  MUTT_NEWSGROUPS,      ///< Pattern matches newsgroup
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

int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, TokenFlags flags);
void mutt_free_opts(void);
enum QuadOption query_quadoption(enum QuadOption opt, const char *prompt);
int mutt_label_complete(char *buf, size_t buflen, int numtabs);
int mutt_command_complete(char *buf, size_t buflen, int pos, int numtabs);
int mutt_var_value_complete(char *buf, size_t buflen, int pos);
void myvar_set(const char *var, const char *val);
bool mutt_nm_query_complete(char *buf, size_t buflen, int pos, int numtabs);
bool mutt_nm_tag_complete(char *buf, size_t buflen, int numtabs);
int mutt_dump_variables(bool hide_sensitive);
int mutt_get_hook_type(const char *name);
enum CommandResult mutt_parse_rc_line(/* const */ char *line, struct Buffer *token, struct Buffer *err);
int mutt_query_variables(struct ListHead *queries);
void reset_value(const char *name);

#endif /* MUTT_MUTT_H */
