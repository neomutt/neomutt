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

#include "config.h"
#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include "config/lib.h"
#include "hook.h"
#include "keymap.h"
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
#define MUTT_TOKEN_PATTERN       (1 << 4)  ///< ~%=!| are terms (for patterns)
#define MUTT_TOKEN_COMMENT       (1 << 5)  ///< Don't reap comments
#define MUTT_TOKEN_SEMICOLON     (1 << 6)  ///< Don't treat ; as special
#define MUTT_TOKEN_BACKTICK_VARS (1 << 7)  ///< Expand variables within backticks
#define MUTT_TOKEN_NOSHELL       (1 << 8)  ///< Don't expand environment variables
#define MUTT_TOKEN_QUESTION      (1 << 9)  ///< Treat '?' as a special

/**
 * enum MessageType - To set flags or match patterns
 *
 * @sa mutt_set_flag(), mutt_pattern_func()
 */
enum MessageType
{
  MUTT_ALL = 1,    ///< All messages
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

  MUTT_MT_MAX,
};

/* flags for parse_spam_list */
#define MUTT_SPAM   1
#define MUTT_NOSPAM 2

int mutt_init(struct ConfigSet *cs, bool skip_sys_rc, struct ListHead *commands);
struct ConfigSet *init_config(size_t size);

#define EXEC_SHELL "/bin/sh"

char *mutt_compile_help(char *buf, size_t buflen, enum MenuType menu, const struct Mapping *items);

int mutt_extract_token(struct Buffer *dest, struct Buffer *tok, TokenFlags flags);
void mutt_opts_free(void);
enum QuadOption query_quadoption(enum QuadOption opt, const char *prompt);
int mutt_label_complete(char *buf, size_t buflen, int numtabs);
int mutt_command_complete(char *buf, size_t buflen, int pos, int numtabs);
int mutt_var_value_complete(char *buf, size_t buflen, int pos);
bool mutt_nm_query_complete(char *buf, size_t buflen, int pos, int numtabs);
bool mutt_nm_tag_complete(char *buf, size_t buflen, int numtabs);
HookFlags mutt_get_hook_type(const char *name);
enum CommandResult mutt_parse_rc_line(/* const */ char *line, struct Buffer *token, struct Buffer *err);
int mutt_query_variables(struct ListHead *queries);
void reset_value(const char *name);

#ifdef HAVE_LIBUNWIND
void show_backtrace(void);
#endif

#endif /* MUTT_MUTT_H */
