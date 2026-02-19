/**
 * @file
 * Memory management wrappers
 *
 * @authors
 * Copyright (C) 2017-2023 Richard Russon <rich@flatcap.org>
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
 * @page mutt_memory Memory management wrappers
 *
 * "Safe" memory management routines.
 *
 * @note If any of the allocators fail, the user is notified and the program is
 *       stopped immediately.
 */

#include "config.h"
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "exit.h"
#include "logging2.h"

/**
 * reallocarray - reallocarray(3) implementation
 * @param p     Address of memory block to resize
 * @param n     Number of elements
 * @param size  Size of element
 *
 * See reallocarray(3) for the documentation of this function.
 * We need to implement it because MacOS is cruft from another century
 * and doesn't have this fundamental API.  We'll remove it once all the
 * systems we support have it.
 */
#if !defined(HAVE_REALLOCARRAY)
static void *reallocarray(void *p, size_t n, size_t size)
{
  if ((n != 0) && (size > (SIZE_MAX / n)))
  {
    errno = ENOMEM;
    return NULL;
  }

  return realloc(p, n * size);
}
#endif

/**
 * mutt_mem_calloc - Allocate zeroed memory on the heap
 * @param nmemb Number of blocks
 * @param size  Size of blocks
 * @retval ptr Memory on the heap
 *
 * @note On error, this function will never return NULL.
 *       It will print an error and exit the program.
 *
 * The caller should call mutt_mem_free() to release the memory
 */
void *mutt_mem_calloc(size_t nmemb, size_t size)
{
  if ((nmemb == 0) || (size == 0))
    return NULL;

  void *p = calloc(nmemb, size);
  if (!p)
  {
    mutt_error("%s", strerror(errno)); // LCOV_EXCL_LINE
    mutt_exit(1);                      // LCOV_EXCL_LINE
  }
  return p;
}

/**
 * mutt_mem_free - Release memory allocated on the heap
 * @param ptr Memory to release
 */
void mutt_mem_free(void *ptr)
{
  if (!ptr)
    return;
  void **p = (void **) ptr;
  free(*p);
  *p = NULL;
}

/**
 * mutt_mem_malloc - Allocate memory on the heap
 * @param size Size of block to allocate
 * @retval ptr Memory on the heap
 *
 * @note On error, this function will never return NULL.
 *       It will print an error and exit the program.
 *
 * The caller should call mutt_mem_free() to release the memory
 */
void *mutt_mem_malloc(size_t size)
{
  return mutt_mem_mallocarray(size, 1);
}

/**
 * mutt_mem_mallocarray - Allocate memory on the heap (array version)
 * @param nmemb Number of blocks
 * @param size  Size of blocks
 * @retval ptr  Memory on the heap
 *
 * @note On error, this function will never return NULL.
 *       It will print an error and exit the program.
 *
 * The caller should call mutt_mem_free() to release the memory
 */
void *mutt_mem_mallocarray(size_t nmemb, size_t size)
{
  void *p = NULL;
  mutt_mem_reallocarray(&p, nmemb, size);
  return p;
}

/**
 * mutt_mem_realloc - Resize a block of memory on the heap
 * @param pptr Address of pointer to memory block to resize
 * @param size New size
 * @retval ptr  Memory
 * @retval NULL Size was 0
 *
 * @note On error, this function will never return NULL.
 *       It will print an error and exit the program.
 *
 * If the new size is zero, the block will be freed.
 */
void *mutt_mem_realloc(void *pptr, size_t size)
{
  mutt_mem_reallocarray(pptr, size, 1);
}

/**
 * mutt_mem_reallocarray - Resize a block of memory on the heap (array version)
 * @param pptr  Address of pointer to memory block to resize
 * @param nmemb Number of blocks
 * @param size  Size of blocks
 *
 * @note On error, this function will never return NULL.
 *       It will print an error and exit the program.
 *
 * If the new size is zero, the block will be freed.
 */
void mutt_mem_reallocarray(void *pptr, size_t nmemb, size_t size)
{
  if (!pptr)
    return;

  void **pp = (void **) pptr;

  if ((nmemb == 0) || (size == 0))
  {
    free(*pp);
    *pp = NULL;
    return;
  }

  void *r = reallocarray(*pp, nmemb, size);
  if (!r)
  {
    mutt_error("%s", strerror(errno)); // LCOV_EXCL_LINE
    mutt_exit(1);                      // LCOV_EXCL_LINE
  }

  *pp = r;
}

/**
 * mutt_mem_recallocarray - resize clear allocation array
 * @param p Memory block to resize
 * @param old_n Old number of elements
 * @param n Requested number of elements
 * @param size Element size
 *
 * @note On error, this function will never return NULL.
 *       It will print an error and exit the program.
 *
 * If the new size is zero, the block will be freed.
 *
 * Caveats:
 *	Unlike recallocarray(3) from OpenBSD,
 *	we don't use explicit_bzero(3).
 *	Sensitive data is not securely erased by this API.
 */
void *mutt_mem_recallocarray(void *p, size_t old_n, size_t n, size_t size)
{
  mutt_mem_reallocarray(&p, n, size);

  if (n > old_n)
  {
    unsigned char *cp = p;
    memset(cp + old_n, '\0', n - old_n);
  }

  return p;
}

/**
 * mutt_mem_recalloc - resize clear allocation
 * @param p Memory block to resize
 * @param old_size Old size
 * @param size Requested size
 *
 * @note On error, this function will never return NULL.
 *       It will print an error and exit the program.
 *
 * If the new size is zero, the block will be freed.
 *
 * Caveats:
 *	Unlike recallocarray(3) from OpenBSD,
 *	we don't use explicit_bzero(3).
 *	Sensitive data is not securely erased by this API.
 */
void *mutt_mem_recalloc(void *p, size_t old_size, size_t size)
{
  return mutt_mem_recallocarray(p, old_size, size, 1);
}
