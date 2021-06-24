/**
 * @file
 * Context-free sorting function
 *
 * @authors
 * Copyright (C) 2021 Eric Blake <eblake@redhat.com>
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

#ifndef MUTT_LIB_QSORT_R_H
#define MUTT_LIB_QSORT_R_H

#include <stddef.h>

typedef int (*qsort_r_compar_t)(const void *a, const void *b, void *arg);

void mutt_qsort_r(void *base, size_t nmemb, size_t size, qsort_r_compar_t compar, void *arg);

#endif /* MUTT_LIB_QSORT_R_H */
