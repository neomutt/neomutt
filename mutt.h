/**
 * @file
 * Many unsorted constants and some structs
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2018-2024 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
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

extern bool StartupComplete;

typedef uint8_t CompletionFlags;       ///< Flags for mw_get_field(), e.g. #MUTT_COMP_NO_FLAGS
#define MUTT_COMP_NO_FLAGS          0  ///< No flags are set
#define MUTT_COMP_CLEAR       (1 << 0) ///< Clear input if printable character is pressed
#define MUTT_COMP_PASS        (1 << 1) ///< Password mode (no echo)
#define MUTT_COMP_UNBUFFERED  (1 << 2) ///< Ignore macro buffer

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
