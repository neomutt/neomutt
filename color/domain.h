/**
 * @file
 * Colour Domains
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COLOR_DOMAIN_H
#define MUTT_COLOR_DOMAIN_H

/**
 * enum ColorDomain - Colour Domains
 *
 * Each domain has CID associated with it.
 * Their prefixes are shown.
 */
enum ColorDomain
{
  CD_CORE = 1,     ///< Core       CD_COR_ #ColorCore
  CD_COMPOSE,      ///< Compose    CD_COM_ #ColorCompose
  CD_INDEX,        ///< Index      CD_IND_ #ColorIndex
  CD_PAGER,        ///< Pager      CD_PAG_ #ColorPager
  CD_QUOTED,       ///< Quoted     CD_QUO_ #ColorQuoted
  CD_SIDEBAR,      ///< Sidebar    CD_SID_ #ColorSidebar
};

/**
 * enum ColorCore - Core NeoMutt Colours
 *
 * @sa CD_CORE, ColorDomain
 */
enum ColorCore
{
  CD_COR_BOLD = 1,                 ///< Bold text
  CD_COR_ERROR,                    ///< Error message
  CD_COR_INDICATOR,                ///< Selected item in list
  CD_COR_ITALIC,                   ///< Italic text
  CD_COR_MESSAGE,                  ///< Informational message
  CD_COR_NORMAL,                   ///< Plain text
  CD_COR_OPTIONS,                  ///< Options in prompt
  CD_COR_PROGRESS,                 ///< Progress bar
  CD_COR_PROMPT,                   ///< Question/user input
  CD_COR_STATUS,                   ///< Status bar (takes a pattern)
  CD_COR_STRIPE_EVEN,              ///< Stripes: even lines of the Help Page
  CD_COR_STRIPE_ODD,               ///< Stripes: odd lines of the Help Page
  CD_COR_TREE,                     ///< Index: tree-drawing characters
  CD_COR_UNDERLINE,                ///< Underlined text
  CD_COR_WARNING,                  ///< Warning messages
};

/**
 * enum ColorCompose - Compose Dialog Colours
 *
 * @sa CD_COMPOSE, ColorDomain
 */
enum ColorCompose
{
  CD_COM_COMPOSE_HEADER = 1,       ///< Header labels, e.g. From:
  CD_COM_COMPOSE_SECURITY_BOTH,    ///< Mail will be encrypted and signed
  CD_COM_COMPOSE_SECURITY_ENCRYPT, ///< Mail will be encrypted
  CD_COM_COMPOSE_SECURITY_NONE,    ///< Mail will not be encrypted or signed
  CD_COM_COMPOSE_SECURITY_SIGN,    ///< Mail will be signed
};

/**
 * enum ColorIndex - Index Dialog Colours
 *
 * @sa CD_INDX, ColorDomain
 */
enum ColorIndex
{
  CD_IND_INDEX = 1,                ///< Default colour
  CD_IND_INDEX_AUTHOR,             ///< Author field
  CD_IND_INDEX_COLLAPSED,          ///< Number of messages in collapsed thread
  CD_IND_INDEX_DATE,               ///< Date field
  CD_IND_INDEX_FLAGS,              ///< Flags field
  CD_IND_INDEX_LABEL,              ///< Label field
  CD_IND_INDEX_NUMBER,             ///< Index number
  CD_IND_INDEX_SIZE,               ///< Size field
  CD_IND_INDEX_SUBJECT,            ///< Subject field
  CD_IND_INDEX_TAG,                ///< Tag field (%G)
  CD_IND_INDEX_TAGS,               ///< Tags field (%g, %J)
};

/**
 * enum ColorPager - Pager Dialog Colours
 *
 * @sa CD_PAGER, ColorDomain
 */
enum ColorPager
{
  CD_PAG_ATTACHMENT = 1,           ///< MIME attachments text (entire line)
  CD_PAG_ATTACH_HEADERS,           ///< MIME attachment test (takes a pattern)
  CD_PAG_BODY,                     ///< Highlight body of message (takes a pattern)
  CD_PAG_HDRDEFAULT,               ///< Header default colour
  CD_PAG_HEADER,                   ///< Message headers (takes a pattern)
  CD_PAG_MARKERS,                  ///< Markers, line continuation
  CD_PAG_SEARCH,                   ///< Search matches
  CD_PAG_SIGNATURE,                ///< Signature lines
  CD_PAG_TILDE,                    ///< Empty lines after message
};

/**
 * enum ColorQuoted - Quoted Text Colours
 *
 * @sa CD_QUOTED, ColorDomain
 */
enum ColorQuoted
{
  CD_QUO_QUOTED0 = 1,              ///< Quoted text, level 0
  CD_QUO_QUOTED1,                  ///< Quoted text, level 1
  CD_QUO_QUOTED2,                  ///< Quoted text, level 2
  CD_QUO_QUOTED3,                  ///< Quoted text, level 3
  CD_QUO_QUOTED4,                  ///< Quoted text, level 4
  CD_QUO_QUOTED5,                  ///< Quoted text, level 5
  CD_QUO_QUOTED6,                  ///< Quoted text, level 6
  CD_QUO_QUOTED7,                  ///< Quoted text, level 7
  CD_QUO_QUOTED8,                  ///< Quoted text, level 8
  CD_QUO_QUOTED9,                  ///< Quoted text, level 9
};

/**
 * enum ColorSidebar - Sidebar Colours
 *
 * @sa CD_SIDEBAR, ColorDomain
 */
enum ColorSidebar
{
  CD_SID_SIDEBAR_BACKGROUND = 1,   ///< Background colour for the Sidebar
  CD_SID_SIDEBAR_DIVIDER,          ///< Line dividing sidebar from the index/pager
  CD_SID_SIDEBAR_FLAGGED,          ///< Mailbox with flagged messages
  CD_SID_SIDEBAR_HIGHLIGHT,        ///< Select cursor
  CD_SID_SIDEBAR_INDICATOR,        ///< Current open mailbox
  CD_SID_SIDEBAR_NEW,              ///< Mailbox with new mail
  CD_SID_SIDEBAR_ORDINARY,         ///< Mailbox with no new or flagged messages
  CD_SID_SIDEBAR_SPOOLFILE,        ///< $spool_file (Spool mailbox)
  CD_SID_SIDEBAR_UNREAD,           ///< Mailbox with unread mail
};

#endif /* MUTT_COLOR_DOMAIN_H */
