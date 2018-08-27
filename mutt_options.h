/**
 * @file
 * Definitions of NeoMutt Configuration
 *
 * @authors
 * Copyright (C) 2016 Bernard Pratz <z+mutt+pub@m0g.net>
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

#ifndef _MUTT_OPTIONS_H
#define _MUTT_OPTIONS_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

struct Buffer;
struct ConfigDef;

int  mutt_option_to_string(const struct ConfigDef *opt, char *val, size_t len);
bool mutt_option_get(const char *s, struct ConfigDef *opt);
int  mutt_option_set(const struct ConfigDef *val, struct Buffer *err);

#endif /* _MUTT_OPTIONS_H */
