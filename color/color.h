/**
 * @file
 * Color and attribute parsing
 *
 * @authors
 * Copyright (C) 1996-2002,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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

#ifndef MUTT_COLOR_COLOR_H
#define MUTT_COLOR_COLOR_H

/**
 * enum ColorId - List of all colored objects
 *
 * This enumeration starts at 50 to avoid any of the values being 37 (ASCII %).
 * Inserting colour codes into expando strings, when one of the colour codes
 * was '%', was causing formatting problems.
 */
enum ColorId
{
  MT_COLOR_NONE = 50,
  MT_COLOR_ATTACHMENT,               ///< MIME attachments text (entire line)
  MT_COLOR_ATTACH_HEADERS,           ///< MIME attachment test (takes a pattern)
  MT_COLOR_BODY,                     ///< Pager: highlight body of message (takes a pattern)
  MT_COLOR_BOLD,                     ///< Bold text
  MT_COLOR_COMPOSE_HEADER,           ///< Header labels, e.g. From:
  MT_COLOR_COMPOSE_SECURITY_BOTH,    ///< Mail will be encrypted and signed
  MT_COLOR_COMPOSE_SECURITY_ENCRYPT, ///< Mail will be encrypted
  MT_COLOR_COMPOSE_SECURITY_NONE,    ///< Mail will not be encrypted or signed
  MT_COLOR_COMPOSE_SECURITY_SIGN,    ///< Mail will be signed
  MT_COLOR_ERROR,                    ///< Error message
  MT_COLOR_HDRDEFAULT,               ///< Header default colour
  MT_COLOR_HEADER,                   ///< Message headers (takes a pattern)
  MT_COLOR_INDICATOR,                ///< Selected item in list
  MT_COLOR_MARKERS,                  ///< Pager: markers, line continuation
  MT_COLOR_MESSAGE,                  ///< Informational message
  MT_COLOR_MESSAGE_LOG,              ///< Menu showing log messages
  MT_COLOR_NORMAL,                   ///< Plain text
  MT_COLOR_OPTIONS,                  ///< Options in prompt
  MT_COLOR_PROGRESS,                 ///< Progress bar
  MT_COLOR_PROMPT,                   ///< Question/user input
  MT_COLOR_QUOTED,                   ///< Pager: quoted text
  MT_COLOR_SEARCH,                   ///< Pager: search matches
#ifdef USE_SIDEBAR
  MT_COLOR_SIDEBAR_DIVIDER,          ///< Line dividing sidebar from the index/pager
  MT_COLOR_SIDEBAR_FLAGGED,          ///< Mailbox with flagged messages
  MT_COLOR_SIDEBAR_HIGHLIGHT,        ///< Select cursor
  MT_COLOR_SIDEBAR_INDICATOR,        ///< Current open mailbox
  MT_COLOR_SIDEBAR_NEW,              ///< Mailbox with new mail
  MT_COLOR_SIDEBAR_ORDINARY,         ///< Mailbox with no new or flagged messages
  MT_COLOR_SIDEBAR_SPOOLFILE,        ///< $spool_file (Spool mailbox)
  MT_COLOR_SIDEBAR_UNREAD,           ///< Mailbox with unread mail
#endif
  MT_COLOR_SIGNATURE,                ///< Pager: signature lines
  MT_COLOR_STATUS,                   ///< Status bar (takes a pattern)
  MT_COLOR_TILDE,                    ///< Pager: empty lines after message
  MT_COLOR_TREE,                     ///< Index: tree-drawing characters
  MT_COLOR_UNDERLINE,                ///< Underlined text
  MT_COLOR_WARNING,                  ///< Warning messages
  /* please no non-MT_COLOR_INDEX objects after this point */
  MT_COLOR_INDEX,                    ///< Index: default colour (takes a pattern)
  MT_COLOR_INDEX_AUTHOR,             ///< Index: author field (takes a pattern)
  MT_COLOR_INDEX_FLAGS,              ///< Index: flags field (takes a pattern)
  MT_COLOR_INDEX_SUBJECT,            ///< Index: subject field (takes a pattern)
  MT_COLOR_INDEX_TAG,                ///< Index: tag field (%g, takes a pattern)
  /* below here - only index coloring stuff that doesn't have a pattern */
  MT_COLOR_INDEX_COLLAPSED,          ///< Index: number of messages in collapsed thread
  MT_COLOR_INDEX_DATE,               ///< Index: date field
  MT_COLOR_INDEX_LABEL,              ///< Index: label field
  MT_COLOR_INDEX_NUMBER,             ///< Index: index number
  MT_COLOR_INDEX_SIZE,               ///< Index: size field
  MT_COLOR_INDEX_TAGS,               ///< Index: tags field (%g, %J)
  MT_COLOR_MAX,
};

#include "config.h"
#include <stdint.h>
#include "mutt/lib.h"

extern const struct Mapping ColorNames[];
extern const struct Mapping ColorFields[];
extern const struct Mapping ComposeColorFields[];

#define COLOR_DEFAULT (-2)
#define COLOR_UNSET   UINT32_MAX

void mutt_colors_init(void);
void mutt_colors_cleanup(void);

void colors_clear(void);

#endif /* MUTT_COLOR_COLOR_H */
