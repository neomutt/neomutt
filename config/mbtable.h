/**
 * @file
 * Type representing a multibyte character table
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

#ifndef MUTT_CONFIG_MBTABLE_H
#define MUTT_CONFIG_MBTABLE_H

struct ConfigSet;

/**
 * struct MbTable - multibyte character table
 *
 * Allows for direct access to the individual multibyte characters in a string.
 * This is used for the #C_FlagChars, #C_FromChars, #C_StatusChars and #C_ToChars
 * option types.
 */
struct MbTable
{
  char *orig_str;      ///< Original string used to generate this object
  int len;             ///< Number of characters
  char **chars;        ///< The array of multibyte character strings
  char *segmented_str; ///< Each chars entry points inside this string
};

void            mbtable_free (struct MbTable **table);
void            mbtable_init (struct ConfigSet *cs);
struct MbTable *mbtable_parse(const char *str);

#endif /* MUTT_CONFIG_MBTABLE_H */
