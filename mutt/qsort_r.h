/**
 * @file
 * Context-free sorting function
 *
 * @authors
 * Copyright (C) 2021 Eric Blake <eblake@redhat.com>
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MUTT_QSORT_R_H
#define MUTT_MUTT_QSORT_R_H

#include <stddef.h>

/**
 * @defgroup sort_api Sorting API
 *
 * Prototype for generic comparison function, compatible with qsort_r()
 *
 * @param a     First item
 * @param b     Second item
 * @param sdata Private data
 * @retval <0 a precedes b
 * @retval  0 a and b are identical
 * @retval >0 b precedes a
 */
typedef int (*sort_t)(const void *a, const void *b, void *sdata);

void mutt_qsort_r(void *base, size_t nmemb, size_t size, sort_t compar, void *sdata);

#endif /* MUTT_MUTT_QSORT_R_H */
