/**
 * @file
 * Memory management wrappers
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2025 Alejandro Colomar <alx@kernel.org>
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

#ifndef MUTT_MUTT_MEMORY_H
#define MUTT_MUTT_MEMORY_H

#if defined __has_include
# if __has_include(<stdcountof.h>)
#  include <stdcountof.h>
# endif
#endif
#include <stddef.h>

#undef MAX
#undef MIN
#undef CLAMP
/// Return the maximum of two values
#define MAX(a, b) (((a) < (b)) ? (b) : (a))
/// Return the minimum of two values
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
/// Clamp a value between a lower and upper bound
#define CLAMP(val, lo, hi) MIN(hi, MAX(lo, val))

#undef ROUND_UP
/// Round up NUM to the nearest multiple of STEP
#define ROUND_UP(NUM, STEP) ((((NUM) + (STEP) -1) / (STEP)) * (STEP))

#if !defined(countof)
# define countof(x)  (sizeof(x) / sizeof((x)[0]))
#endif

#define MUTT_MEM_CALLOC(n, type)  ((type *) mutt_mem_calloc(n, sizeof(type)))
#define MUTT_MEM_MALLOC(n, type)  ((type *) mutt_mem_mallocarray(n, sizeof(type)))

#define MUTT_MEM_REALLOC(pptr, n, type)                                       \
(                                                                             \
  _Generic(*(pptr), type *: mutt_mem_reallocarray(pptr, n, sizeof(type)))     \
)

void *mutt_mem_calloc(size_t nmemb, size_t size);
void  mutt_mem_free(void *ptr);
void *mutt_mem_malloc(size_t size);
void *mutt_mem_mallocarray(size_t nmemb, size_t size);
void  mutt_mem_realloc(void *pptr, size_t size);
void  mutt_mem_reallocarray(void *pptr, size_t nmemb, size_t size);

/// Free memory and set the pointer to NULL
#define FREE(x) mutt_mem_free(x)

#endif /* MUTT_MUTT_MEMORY_H */
