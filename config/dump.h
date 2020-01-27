/**
 * @file
 * Dump all the config
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

#ifndef MUTT_CONFIG_DUMP_H
#define MUTT_CONFIG_DUMP_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct Buffer;
struct ConfigSet;
struct HashElem;

typedef uint16_t ConfigDumpFlags;         ///< Flags for dump_config(), e.g. #CS_DUMP_ONLY_CHANGED
#define CS_DUMP_NO_FLAGS              0   ///< No flags are set
#define CS_DUMP_ONLY_CHANGED    (1 << 0)  ///< Only show config that the user has changed
#define CS_DUMP_HIDE_SENSITIVE  (1 << 1)  ///< Obscure sensitive information like passwords
#define CS_DUMP_NO_ESCAPING     (1 << 2)  ///< Do not escape special chars, or quote the string
#define CS_DUMP_HIDE_NAME       (1 << 3)  ///< Do not print the name of the config item
#define CS_DUMP_HIDE_VALUE      (1 << 4)  ///< Do not print the value of the config item
#define CS_DUMP_SHOW_DEFAULTS   (1 << 5)  ///< Show the default value for the config item
#define CS_DUMP_SHOW_DISABLED   (1 << 6)  ///< Show disabled config items, too
#define CS_DUMP_SHOW_SYNONYMS   (1 << 7)  ///< Show synonyms and the config items they're linked to
#define CS_DUMP_SHOW_DEPRECATED (1 << 8)  ///< Show config items that aren't used any more

void              dump_config_neo(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value, struct Buffer *initial, ConfigDumpFlags flags, FILE *fp);
bool              dump_config(struct ConfigSet *cs, ConfigDumpFlags flags, FILE *fp);
size_t            escape_string(struct Buffer *buf, const char *src);
size_t            pretty_var(const char *str, struct Buffer *buf);

#endif /* MUTT_CONFIG_DUMP_H */
