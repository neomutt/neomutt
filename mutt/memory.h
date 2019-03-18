/**
 * @file
 * Memory management wrappers
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

#ifndef MUTT_LIB_MEMORY_H
#define MUTT_LIB_MEMORY_H

#include <stddef.h>

#undef MAX
#undef MIN
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define mutt_array_size(x) (sizeof(x) / sizeof((x)[0]))

void *mutt_mem_calloc(size_t nmemb, size_t size);
void  mutt_mem_free(void *ptr);
void *mutt_mem_malloc(size_t size);
void  mutt_mem_realloc(void *ptr, size_t size);

#define FREE(x) mutt_mem_free(x)

#endif /* MUTT_LIB_MEMORY_H */
