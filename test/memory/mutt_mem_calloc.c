/**
 * @file
 * Test code for mutt_mem_calloc()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"

void test_mutt_mem_calloc(void)
{
  // void *mutt_mem_calloc(size_t nmemb, size_t size);

  {
    void *ptr = mutt_mem_calloc(0, 0);
    TEST_CHECK(ptr == NULL);
  }

  {
    void *ptr = mutt_mem_calloc(0, 1024);
    TEST_CHECK(ptr == NULL);
  }

  {
    void *ptr = mutt_mem_calloc(1024, 0);
    TEST_CHECK(ptr == NULL);
  }

  {
    size_t num = 64;
    size_t size = 128;

    void *ptr = mutt_mem_calloc(num, size);
    TEST_CHECK(ptr != NULL);

    unsigned char *cp = ptr;
    for (size_t i = 0; i < (num * size); i++)
    {
      if (cp[i] != '\0')
      {
        TEST_CHECK_(false, "mem[%zu] = 0x%02x\n", cp[i]);
        break;
      }
    }

    FREE(&ptr);
  }
}
