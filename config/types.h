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
#define DT_COMMAND   3  ///< a command
#define DT_ENUM      4  ///< an enumeration
#define DT_HCACHE    5  ///< header cache backend
#define DT_LONG      6  ///< a number (long)
#define DT_MAGIC     7  ///< mailbox type
#define DT_MBTABLE   8  ///< multibyte char table
#define DT_NUMBER    9  ///< a number
#define DT_PATH     10  ///< a pathname
#define DT_QUAD     11  ///< quad-option (no/yes/ask-no/ask-yes)
#define DT_REGEX    12  ///< regular expressions
#define DT_SLIST    13  ///< a list of strings
#define DT_SORT     14  ///< sorting methods
#define DT_STRING   15  ///< a string
#define DT_SYNONYM  16  ///< synonym for another variable

#define DTYPE(x) ((x) & 0x1F)  ///< Mask for the Data Type

#define DT_NOT_EMPTY     (1 << 6)  ///< Empty strings are not allowed
#define DT_NOT_NEGATIVE  (1 << 7)  ///< Negative numbers are not allowed
#define DT_MAILBOX       (1 << 8)  ///< DT_PATH: Don't perform path expansions

/* subtypes for... */
#define DT_SUBTYPE_MASK  0x0FE0  ///< Mask for the Data Subtype

typedef uint16_t ConfigRedrawFlags; ///< Flags for redraw/resort, e.g. #R_INDEX
#define R_NONE               0      ///< No refresh/resort flags
#define R_INDEX        (1 << 0)     ///< Redraw the index menu (MENU_MAIN)
#define R_PAGER        (1 << 1)     ///< Redraw the pager menu
#define R_PAGER_FLOW   (1 << 2)     ///< Reflow line_info and redraw the pager menu
#define R_RESORT       (1 << 3)     ///< Resort the mailbox
#define R_RESORT_SUB   (1 << 4)     ///< Resort subthreads
#define R_RESORT_INIT  (1 << 5)     ///< Resort from scratch
#define R_TREE         (1 << 6)     ///< Redraw the thread tree
#define R_REFLOW       (1 << 7)     ///< Reflow window layout and full redraw
#define R_SIDEBAR      (1 << 8)     ///< Redraw the sidebar
#define R_MENU         (1 << 9)     ///< Redraw all menus
#define F_SENSITIVE    (1 << 10)    ///< Config item contains sensitive value (will be OR'd with R_ flags above)

#define IS_SENSITIVE(x) (((x).flags & F_SENSITIVE) == F_SENSITIVE)

/* Private config item flags */
#define DT_INHERITED    (1 << 28)  ///< Config item is inherited
#define DT_INITIAL_SET  (1 << 29)  ///< Config item must have its initial value freed
#define DT_DISABLED     (1 << 30)  ///< Config item is disabled
#define DT_MY_CONFIG    (1 << 31)  ///< Config item is a "my_" variable

#endif /* MUTT_CONFIG_TYPES_H */
