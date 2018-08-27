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

#ifndef _CONFIG_DUMP_H
#define _CONFIG_DUMP_H

#include <stddef.h>

struct ConfigSet;

#define CS_DUMP_STYLE_MUTT   0 /**< Display config in Mutt style */
#define CS_DUMP_STYLE_NEO    1 /**< Display config in NeoMutt style */

#define CS_DUMP_ONLY_CHANGED   (1 << 0) /**< Only show config that the user has changed */
#define CS_DUMP_HIDE_SENSITIVE (1 << 1) /**< Obscure sensitive information like passwords */
#define CS_DUMP_NO_ESCAPING    (1 << 2) /**< Do not escape special chars, or quote the string */
#define CS_DUMP_HIDE_NAME      (1 << 3) /**< Do not print the name of the config item */
#define CS_DUMP_HIDE_VALUE     (1 << 4) /**< Do not print the value of the config item */
#define CS_DUMP_SHOW_DEFAULTS  (1 << 5) /**< Show the default value for the config item */
#define CS_DUMP_SHOW_DISABLED  (1 << 6) /**< Show disabled config items, too */
#define CS_DUMP_SHOW_SYNONYMS  (1 << 7) /**< Show synonyms and the config items their linked to */

void              dump_config_mutt(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value, struct Buffer *initial, int flags);
void              dump_config_neo(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value, struct Buffer *initial, int flags);
bool              dump_config(struct ConfigSet *cs, int style, int flags);
int               elem_list_sort(const void *a, const void *b);
size_t            escape_string(struct Buffer *buf, const char *src);
struct HashElem **get_elem_list(struct ConfigSet *cs);
size_t            pretty_var(const char *str, struct Buffer *buf);

#endif /* _CONFIG_DUMP_H */
