/**
 * @file
 * Dump all the config
 *
 * @authors
 * Copyright (C) 2017-2015 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CONFIG_DUMP_H
#define MUTT_CONFIG_DUMP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct Buffer;
struct ConfigSet;
struct HashElem;
struct HashElemArray;

/**
 * enum ConfigDumpFlag - Flags for dump_config(), e.g. #CS_DUMP_ONLY_CHANGED
 */
enum ConfigDumpFlag
{
  CS_DUMP_NONE            =        0,  ///< No flags are set
  CS_DUMP_ONLY_CHANGED    = 1U <<  0,  ///< Only show config that the user has changed
  CS_DUMP_HIDE_SENSITIVE  = 1U <<  1,  ///< Obscure sensitive information like passwords
  CS_DUMP_NO_ESCAPING     = 1U <<  2,  ///< Do not escape special chars, or quote the string
  CS_DUMP_HIDE_NAME       = 1U <<  3,  ///< Do not print the name of the config item
  CS_DUMP_HIDE_VALUE      = 1U <<  4,  ///< Do not print the value of the config item
  CS_DUMP_SHOW_DEFAULTS   = 1U <<  5,  ///< Show the default value for the config item
  CS_DUMP_SHOW_DISABLED   = 1U <<  6,  ///< Show disabled config items, too
  CS_DUMP_SHOW_SYNONYMS   = 1U <<  7,  ///< Show synonyms and the config items they're linked to
  CS_DUMP_SHOW_DEPRECATED = 1U <<  8,  ///< Show config items that aren't used any more
  CS_DUMP_SHOW_DOCS       = 1U <<  9,  ///< Show one-liner documentation for the config item
  CS_DUMP_LINK_DOCS       = 1U << 10,  ///< Link to the online docs
};
typedef uint16_t ConfigDumpFlags;

void              dump_config_neo(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value, struct Buffer *initial, ConfigDumpFlags flags, FILE *fp);
bool              dump_config(struct ConfigSet *cs, struct HashElemArray *hea, ConfigDumpFlags flags, FILE *fp);
size_t            escape_string(struct Buffer *buf, const char *src);
size_t            pretty_var(const char *str, struct Buffer *buf);

#endif /* MUTT_CONFIG_DUMP_H */
