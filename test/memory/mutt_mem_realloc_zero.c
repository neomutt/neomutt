/**
 * @file
 * Test code for mutt_mem_realloc_zero()
 *
 * @authors
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include <string.h>
#include "mutt/lib.h"

static void init_mem(unsigned char *ptr, size_t bytes)
{
  for (size_t i = 0; i < bytes; i++)
  {
    ptr[i] = i % 256;
  }
}

static size_t check_mem_numbers(unsigned char *ptr, size_t bytes)
{
  for (size_t i = 0; i < bytes; i++)
  {
    if (ptr[i] != (i % 256))
    {
      return i;
    }
  }

  return bytes;
}

static size_t check_mem_zero(unsigned char *ptr, size_t bytes)
{
  for (size_t i = 0; i < bytes; i++)
  {
    if (ptr[i] != '\0')
    {
      return i;
    }
  }

  return bytes;
}

void test_mutt_mem_realloc_zero(void)
{
  // void mutt_mem_realloc_zero(void *ptr, size_t cur_size, size_t new_size);

  {
    const size_t cur_size = 1024;
    const size_t new_size = 2048;

    mutt_mem_realloc_zero(NULL, cur_size, new_size);

    TEST_CHECK_(1, "mutt_mem_realloc_zero(NULL, 1024, 2048)");
  }

  {
    const size_t cur_size = 0;
    const size_t new_size = 0;

    unsigned char *ptr = NULL;
    mutt_mem_realloc_zero(&ptr, cur_size, new_size);

    TEST_CHECK_(1, "mutt_mem_realloc_zero(&ptr, 0, 0)");
  }

  {
    const size_t cur_size = 0;
    const size_t new_size = 1024;

    unsigned char *ptr = NULL;
    mutt_mem_realloc_zero(&ptr, cur_size, new_size);

    TEST_CHECK_(1, "mutt_mem_realloc_zero(&ptr, 0, 1024)");
    TEST_CHECK(ptr != NULL);

    TEST_CHECK(check_mem_zero(ptr, new_size) == new_size);

    FREE(&ptr);
  }

  {
    const size_t cur_size = 1024;
    const size_t new_size = 1024;

    unsigned char *ptr = mutt_mem_malloc(cur_size);
    init_mem(ptr, cur_size);

    mutt_mem_realloc_zero(&ptr, cur_size, new_size);

    TEST_CHECK_(1, "mutt_mem_realloc_zero(&ptr, 1024, 1024)");
    TEST_CHECK(ptr != NULL);

    TEST_CHECK(check_mem_numbers(ptr, new_size) == new_size);

    FREE(&ptr);
  }

  {
    const size_t cur_size = 1024;
    const size_t new_size = 2048;

    unsigned char *ptr = mutt_mem_malloc(cur_size);
    init_mem(ptr, cur_size);

    mutt_mem_realloc_zero(&ptr, cur_size, new_size);
    TEST_CHECK_(1, "mutt_mem_realloc_zero(&ptr, 1024, 2048)");
    TEST_CHECK(ptr != NULL);

    TEST_CHECK(check_mem_numbers(ptr, cur_size) == cur_size);
    TEST_CHECK(check_mem_zero(ptr + cur_size, new_size - cur_size) == (new_size - cur_size));

    FREE(&ptr);
  }

  {
    const size_t cur_size = 2048;
    const size_t new_size = 1024;

    unsigned char *ptr = mutt_mem_malloc(cur_size);
    init_mem(ptr, cur_size);

    mutt_mem_realloc_zero(&ptr, cur_size, new_size);
    TEST_CHECK_(1, "mutt_mem_realloc_zero(&ptr, 2048, 1024)");
    TEST_CHECK(ptr != NULL);

    TEST_CHECK(check_mem_numbers(ptr, new_size) == new_size);

    FREE(&ptr);
  }

  {
    const size_t cur_size = 1024;
    const size_t new_size = 0;

    unsigned char *ptr = mutt_mem_malloc(cur_size);
    init_mem(ptr, cur_size);

    mutt_mem_realloc_zero(&ptr, cur_size, new_size);
    TEST_CHECK_(1, "mutt_mem_realloc_zero(&ptr, 1024, 0)");
    TEST_CHECK(ptr == NULL);

    FREE(&ptr);
  }
}
