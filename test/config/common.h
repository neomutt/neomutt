/**
 * @file
 * Shared Testing Code
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _TEST_COMMON_H
#define _TEST_COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include "config/set.h"

struct Buffer;
struct Hash;
struct HashElem;

extern const char *line;
extern bool dont_fail;

int validator_succeed(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                      intptr_t value, struct Buffer *result);
int validator_warn(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                   intptr_t value, struct Buffer *result);
int validator_fail(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                   intptr_t value, struct Buffer *result);

void log_line(const char *fn);
bool log_listener(const struct ConfigSet *cs, struct HashElem *he,
                  const char *name, enum ConfigEvent ev);
void set_list(const struct ConfigSet *cs);

void hash_dump(struct Hash *table);
void cs_dump_set(const struct ConfigSet *cs);

#endif /* _TEST_COMMON_H */
