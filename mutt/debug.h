/**
 * @file
 * Debug messages
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

#ifndef _MUTT_DEBUG_H
#define _MUTT_DEBUG_H

int mutt_debug_real(const char *function, const char *file, int line, int level, ...);
#define mutt_debug(LEVEL, ...) mutt_debug_real(__func__, __FILE__, __LINE__, LEVEL, __VA_ARGS__)

/* Mark unused parameters in API functions */
#define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))

#endif /* _MUTT_DEBUG_H */
