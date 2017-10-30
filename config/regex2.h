/**
 * @file
 * Type representing a regular expression
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

#ifndef _CONFIG_REGEX_H
#define _CONFIG_REGEX_H

#include <features.h>
#include <regex.h>
#include <stdbool.h>

struct Buffer;
struct ConfigSet;

void regex_init(struct ConfigSet *cs);
struct Regex *regex_create(const char *str, int flags, struct Buffer *err);
void regex_free(struct Regex **regex);

#endif /* _CONFIG_REGEX_H */
