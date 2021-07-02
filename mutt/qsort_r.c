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

/**
 * @page mutt_qsort_r Context-free sorting function
 *
 * Context-free sorting function
 */

#include "config.h"
#include <stddef.h>
#include <stdlib.h>
#include "qsort_r.h"

typedef int (*qsort_compar_t)(const void *a, const void *b);

#if !defined(HAVE_QSORT_S) && !defined(HAVE_QSORT_R)
#include <assert.h>
/// Original comparator in fallback implementation
static qsort_r_compar_t global_compar = NULL;
/// Original opaque data in fallback implementation
static void *global_data = NULL;

/**
 * relay_compar - Shim to pass context through to real comparator
 * @param a First item to be compared
 * @param b Second item to be compared
 * @return <0 a sorts before b
 * @return 0  a and b sort equally (sort stability not guaranteed)
 * @return >0 a sorts after b
 */
static int relay_compar(const void *a, const void *b)
{
  return global_compar(a, b, global_data);
}
#endif

/**
 * mutt_qsort_r - Sort an array, where the comparator has access to opaque data rather than requiring global variables
 * @param base   Start of the array to be sorted
 * @param nmemb  Number of elements in the array
 * @param size   Size of each array element
 * @param compar Comparison function, return <0/0/>0 to compare two elements
 * @param arg    Opaque argument to pass to @a compar
 *
 * @note This implementation might not be re-entrant: signal handlers and
 *       @a compar must not call mutt_qsort_r().
 */
void mutt_qsort_r(void *base, size_t nmemb, size_t size, qsort_r_compar_t compar, void *arg)
{
#ifdef HAVE_QSORT_S
  /* FreeBSD 13, where qsort_r had incompatible signature but qsort_s works */
  qsort_s(base, nmemb, size, compar, arg);
#elif defined(HAVE_QSORT_R)
  /* glibc, POSIX (https://www.austingroupbugs.net/view.php?id=900) */
  qsort_r(base, nmemb, size, compar, arg);
#else
  /* This fallback is not re-entrant. */
  assert((global_compar == NULL) && (global_data == NULL));
  global_compar = compar;
  global_data = arg;
  qsort(base, nmemb, size, relay_compar);
  global_compar = NULL;
  global_data = NULL;
#endif
}
