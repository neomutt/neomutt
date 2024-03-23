/**
 * @file
 * Constants for all the config types
 *
 * @authors
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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

#ifndef MUTT_CONFIG_TYPES_H
#define MUTT_CONFIG_TYPES_H

#include <stdint.h>

enum ConfigType
{
  DT_ADDRESS,     ///< e-mail address
  DT_BOOL,        ///< boolean option
  DT_ENUM,        ///< an enumeration
  DT_EXPANDO,     ///< an expando
  DT_HCACHE,      ///< header cache backend
  DT_LONG,        ///< a number (long)
  DT_MBTABLE,     ///< multibyte char table
  DT_MYVAR,       ///< a user-defined variable (my_foo)
  DT_NUMBER,      ///< a number
  DT_PATH,        ///< a path to a file/directory
  DT_QUAD,        ///< quad-option (no/yes/ask-no/ask-yes)
  DT_REGEX,       ///< regular expressions
  DT_SLIST,       ///< a list of strings
  DT_SORT,        ///< sorting methods
  DT_STRING,      ///< a string
  DT_SYNONYM,     ///< synonym for another variable
  DT_END,
};

#define DTYPE(t) ((enum ConfigType)((t) & 0x1F))

enum ConfigTypeField
{
  D_B_ON_STARTUP = 5,              ///< May only be set at startup
  D_B_NOT_EMPTY,                   ///< Empty strings are not allowed
  D_B_SENSITIVE,                   ///< Contains sensitive value, e.g. password

  D_B_CHARSET_SINGLE,              ///< Flag for charset_validator to allow only one charset
  D_B_CHARSET_STRICT,              ///< Flag for charset_validator to use strict char check

  D_B_INTERNAL_FREE_CONFIGDEF,     ///< Config item must have its ConfigDef freed
  D_B_INTERNAL_DEPRECATED,         ///< Config item shouldn't be used any more
  D_B_INTERNAL_INHERITED,          ///< Config item is inherited
  D_B_INTERNAL_INITIAL_SET,        ///< Config item must have its initial value freed

  D_B_CUSTOM_BIT_0,                ///< 1st flag available for customising config types
  D_B_CUSTOM_BIT_1,                ///< 2nd flag available for customising config types
  D_B_CUSTOM_BIT_2,                ///< 3rd flag available for customising config types
  D_B_CUSTOM_BIT_3,                ///< 4th flag available for customising config types
  D_B_CUSTOM_BIT_4,                ///< 5th flag available for customising config types
  D_B_CUSTOM_BIT_5,                ///< 6th flag available for customising config types
  D_B_CUSTOM_BIT_6,                ///< 7th flag available for customising config types
  D_B_CUSTOM_BIT_7,                ///< 8th flag available for customising config types

  D_B_END,
};

#define D_ON_STARTUP               (1 << D_B_ON_STARTUP)               ///< May only be set at startup
#define D_NOT_EMPTY                (1 << D_B_NOT_EMPTY)                ///< Empty strings are not allowed
#define D_SENSITIVE                (1 << D_B_SENSITIVE)                ///< Contains sensitive value, e.g. password

#define D_CHARSET_SINGLE           (1 << D_B_CHARSET_SINGLE)           ///< Flag for charset_validator to allow only one charset
#define D_CHARSET_STRICT           (1 << D_B_CHARSET_STRICT)           ///< Flag for charset_validator to use strict char check

#define D_INTERNAL_FREE_CONFIGDEF  (1 << D_B_INTERNAL_FREE_CONFIGDEF)  ///< Config item must have its ConfigDef freed
#define D_INTERNAL_DEPRECATED      (1 << D_B_INTERNAL_DEPRECATED)      ///< Config item shouldn't be used any more
#define D_INTERNAL_INHERITED       (1 << D_B_INTERNAL_INHERITED)       ///< Config item is inherited
#define D_INTERNAL_INITIAL_SET     (1 << D_B_INTERNAL_INITIAL_SET)     ///< Config item must have its initial value freed

#define D_CUSTOM_BIT_0             (1 << D_B_CUSTOM_BIT_0)
#define D_CUSTOM_BIT_1             (1 << D_B_CUSTOM_BIT_1)
#define D_CUSTOM_BIT_2             (1 << D_B_CUSTOM_BIT_2)
#define D_CUSTOM_BIT_3             (1 << D_B_CUSTOM_BIT_3)
#define D_CUSTOM_BIT_4             (1 << D_B_CUSTOM_BIT_4)

#define D_STRING_MAILBOX           D_CUSTOM_BIT_0                      ///< Don't perform path expansions
#define D_STRING_COMMAND           D_CUSTOM_BIT_1                      ///< A command

#define D_INTEGER_NOT_NEGATIVE     D_CUSTOM_BIT_0                      ///< Negative numbers are not allowed

#define D_PATH_DIR                 D_CUSTOM_BIT_0                      ///< Path is a directory
#define D_PATH_FILE                D_CUSTOM_BIT_1                      ///< Path is a file

#define D_REGEX_MATCH_CASE         D_CUSTOM_BIT_0                      ///< Case-sensitive matching
#define D_REGEX_ALLOW_NOT          D_CUSTOM_BIT_1                      ///< Regex can begin with '!'
#define D_REGEX_NOSUB              D_CUSTOM_BIT_2                      ///< Do not report what was matched (REG_NOSUB)

#define D_SLIST_SEP_SPACE          (0 << D_B_CUSTOM_BIT_0)             ///< Slist items are space-separated
#define D_SLIST_SEP_COMMA          (1 << D_B_CUSTOM_BIT_0)             ///< Slist items are comma-separated
#define D_SLIST_SEP_COLON          (2 << D_B_CUSTOM_BIT_0)             ///< Slist items are colon-separated
#define D_SLIST_SEP_MASK           (D_CUSTOM_BIT_0 | D_CUSTOM_BIT_1)

#define D_SLIST_ALLOW_DUPES        D_CUSTOM_BIT_2                      ///< Slist may contain duplicates
#define D_SLIST_ALLOW_EMPTY        D_CUSTOM_BIT_3                      ///< Slist may be empty
#define D_SLIST_CASE_SENSITIVE     D_CUSTOM_BIT_4                      ///< Slist is case-sensitive

#define D_SORT_LAST                D_CUSTOM_BIT_0                      ///< Sort flag for -last prefix
#define D_SORT_REVERSE             D_CUSTOM_BIT_1                      ///< Sort flag for -reverse prefix

#define IS_MAILBOX(flags)   ((DTYPE(flags) == DT_STRING) && (flags & D_STRING_MAILBOX))
#define IS_COMMAND(flags)   ((DTYPE(flags) == DT_STRING) && (flags & D_STRING_COMMAND))

#endif /* MUTT_CONFIG_TYPES_H */
