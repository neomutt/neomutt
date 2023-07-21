/**
 * @file
 * Shared code
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_EXPANDO_HELPERS_H
#define MUTT_EXPANDO_HELPERS_H

struct ExpandoDefinition;
struct ExpandoRenderData;

const char *skip_classic_expando      (const char *str, const struct ExpandoDefinition *defs);
const char *skip_until_ch             (const char *start, char terminator);
const char *skip_until_classic_expando(const char *start);

const struct ExpandoRenderData *find_get_number(const struct ExpandoRenderData *rdata, int did, int uid);
const struct ExpandoRenderData *find_get_string(const struct ExpandoRenderData *rdata, int did, int uid);

#endif /* MUTT_EXPANDO_HELPERS_H */
