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
#define DT_ADDRESS    1   /**< e-mail address */
#define DT_BOOL       2   /**< boolean option */
#define DT_COMMAND    3   /**< a command */
#define DT_HCACHE     4   /**< header cache backend */
#define DT_LONG       5   /**< a number (long) */
#define DT_MAGIC      6   /**< mailbox type */
#define DT_MBTABLE    7   /**< multibyte char table */
#define DT_NUMBER     8   /**< a number */
#define DT_PATH       9   /**< a pathname */
#define DT_QUAD      10   /**< quad-option (no/yes/ask-no/ask-yes) */
#define DT_REGEX     11   /**< regular expressions */
#define DT_SORT      12   /**< sorting methods */
#define DT_STRING    13   /**< a string */
#define DT_SYNONYM   14   /**< synonym for another variable */

#define DTYPE(x) ((x) & 0x1f) /**< Mask for the Data Type */

#define DT_NOT_EMPTY     0x40  /**< Empty strings are not allowed */
#define DT_NOT_NEGATIVE  0x80  /**< Negative numbers are not allowed */
#define DT_MAILBOX      0x100  /**< DT_PATH: Don't perform path expansions */

/* subtypes for... */
#define DT_SUBTYPE_MASK 0xfe0 /**< Mask for the Data Subtype */

/* Private config item flags */
#define DT_INHERITED    0x1000 /**< Config item is inherited */
#define DT_INITIAL_SET  0x2000 /**< Config item must have its initial value freed */
#define DT_DISABLED     0x4000 /**< Config item is disabled */
#define DT_MY_CONFIG    0x8000 /**< Config item is a "my_" variable */

typedef uint16_t ConfigRedrawFlags; ///< Flags for redraw/resort, e.g. #R_INDEX
#define R_NONE              0       ///< No refresh/resort flags
#define R_INDEX       (1 << 0)      ///< Redraw the index menu (MENU_MAIN)
#define R_PAGER       (1 << 1)      ///< Redraw the pager menu
#define R_PAGER_FLOW  (1 << 2)      ///< Reflow line_info and redraw the pager menu
#define R_RESORT      (1 << 3)      ///< Resort the mailbox
#define R_RESORT_SUB  (1 << 4)      ///< Resort subthreads
#define R_RESORT_INIT (1 << 5)      ///< Resort from scratch
#define R_TREE        (1 << 6)      ///< Redraw the thread tree
#define R_REFLOW      (1 << 7)      ///< Reflow window layout and full redraw
#define R_SIDEBAR     (1 << 8)      ///< Redraw the sidebar
#define R_MENU        (1 << 9)      ///< Redraw all menus
#define F_SENSITIVE   (1 << 10)     ///< Config item contains sensitive value (will be OR'd with R_ flags above)

#define IS_SENSITIVE(x) (((x).flags & F_SENSITIVE) == F_SENSITIVE)

#endif /* MUTT_CONFIG_TYPES_H */
