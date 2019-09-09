/**
 * @file
 * Constants for all the config types
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONFIG_TYPES_H
#define MUTT_CONFIG_TYPES_H

#include <stdint.h>

/* Data Types */
#define DT_ADDRESS   1  ///< e-mail address
#define DT_BOOL      2  ///< boolean option
#define DT_ENUM      4  ///< an enumeration
#define DT_HCACHE    5  ///< header cache backend
#define DT_LONG      6  ///< a number (long)
#define DT_MBTABLE   8  ///< multibyte char table
#define DT_NUMBER    9  ///< a number
#define DT_QUAD     11  ///< quad-option (no/yes/ask-no/ask-yes)
#define DT_REGEX    12  ///< regular expressions
#define DT_SLIST    13  ///< a list of strings
#define DT_SORT     14  ///< sorting methods
#define DT_STRING   15  ///< a string
#define DT_SYNONYM  16  ///< synonym for another variable

#define DTYPE(x) ((x) & 0x1F)  ///< Mask for the Data Type

#define DT_NOT_EMPTY     (1 << 6)  ///< Empty strings are not allowed
#define DT_NOT_NEGATIVE  (1 << 7)  ///< Negative numbers are not allowed
#define DT_MAILBOX       (1 << 8)  ///< Don't perform path expansions
#define DT_SENSITIVE     (1 << 9)  ///< Contains sensitive value, e.g. password
#define DT_PATH          (1 << 10) ///< A pathname
#define DT_COMMAND       (1 << 11) ///< A command
#define DT_INHERIT_ACC   (1 << 12) ///< Config item can be Account-specific
#define DT_INHERIT_MBOX  (1 << 13) ///< Config item can be Mailbox-specific

#define IS_SENSITIVE(x) (((x).type & DT_SENSITIVE) == DT_SENSITIVE)
#define IS_PATH(x)      (((x)->type & (DT_STRING | DT_PATH)) == (DT_STRING | DT_PATH))
#define IS_COMMAND(x)   (((x)->type & (DT_STRING | DT_COMMAND)) == (DT_STRING | DT_COMMAND))

/* subtypes for... */
#define DT_SUBTYPE_MASK  0x0FE0  ///< Mask for the Data Subtype

typedef uint32_t ConfigRedrawFlags; ///< Flags for redraw/resort, e.g. #R_INDEX
#define R_REDRAW_NO_FLAGS        0  ///< No refresh/resort flags
#define R_INDEX           (1 << 17) ///< Redraw the index menu (MENU_MAIN)
#define R_PAGER           (1 << 18) ///< Redraw the pager menu
#define R_PAGER_FLOW      (1 << 19) ///< Reflow line_info and redraw the pager menu
#define R_RESORT          (1 << 20) ///< Resort the mailbox
#define R_RESORT_SUB      (1 << 21) ///< Resort subthreads
#define R_RESORT_INIT     (1 << 22) ///< Resort from scratch
#define R_TREE            (1 << 23) ///< Redraw the thread tree
#define R_REFLOW          (1 << 24) ///< Reflow window layout and full redraw
#define R_SIDEBAR         (1 << 25) ///< Redraw the sidebar
#define R_MENU            (1 << 26) ///< Redraw all menus

#define R_REDRAW_MASK  0x07FE0000   ///< Mask for the Redraw Flags

/* Private config item flags */
#define DT_DEPRECATED   (1 << 27)  ///< Config item shouldn't be used any more
#define DT_INHERITED    (1 << 28)  ///< Config item is inherited
#define DT_INITIAL_SET  (1 << 29)  ///< Config item must have its initial value freed
#define DT_DISABLED     (1 << 30)  ///< Config item is disabled
#define DT_MY_CONFIG    (1 << 31)  ///< Config item is a "my_" variable

#endif /* MUTT_CONFIG_TYPES_H */
