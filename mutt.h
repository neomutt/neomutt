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
#include <limits.h>
#include <stdint.h>

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

typedef uint16_t CompletionFlags;       ///< Flags for mutt_enter_string_full(), e.g. #MUTT_COMP_ALIAS
#define MUTT_COMP_NO_FLAGS          0   ///< No flags are set
#define MUTT_COMP_ALIAS       (1 << 0)  ///< Alias completion (in alias dialog)
#define MUTT_COMP_COMMAND     (1 << 1)  ///< Complete a NeoMutt command
#define MUTT_COMP_FILE        (1 << 2)  ///< File completion (in browser)
#define MUTT_COMP_FILE_MBOX   (1 << 3)  ///< File completion, plus incoming folders (in browser)
#define MUTT_COMP_FILE_SIMPLE (1 << 4)  ///< File completion (no browser)
#define MUTT_COMP_LABEL       (1 << 5)  ///< Label completion
#define MUTT_COMP_NM_QUERY    (1 << 6)  ///< Notmuch query mode
#define MUTT_COMP_NM_TAG      (1 << 7)  ///< Notmuch tag +/- mode
#define MUTT_COMP_PATTERN     (1 << 8)  ///< Pattern mode (in pattern dialog)
#define MUTT_COMP_CLEAR       (1 << 9)  ///< Clear input if printable character is pressed
#define MUTT_COMP_PASS        (1 << 10) ///< Password mode (no echo)
#define MUTT_COMP_UNBUFFERED  (1 << 11) ///< Ignore macro buffer

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
#define MUTT_TOKEN_PLUS          (1 << 10) ///< Treat '+' as a special
#define MUTT_TOKEN_MINUS         (1 << 11) ///< Treat '-' as a special

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

void reset_value(const char *name);

#endif /* MUTT_MUTT_H */
