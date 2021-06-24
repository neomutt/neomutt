/**
 * @file
 * Test code for mutt_qsort_r()
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"

static void *exparg;

/* Compare two integers, ascending if arg is NULL, else descending */
static int compare_ints(const void *a, const void *b, void *arg)
{
  TEST_CHECK(arg == exparg);
  if (!arg)
    return *(int *) a - *(int *) b;
  return *(int *) b - *(int *) a;
}

void test_mutt_qsort_r(void)
{
  // void mutt_qsort_r(void *base, size_t nmemb, size_t size, qsort_r_compar_t compar, void *arg);
  int array[3];

  array[0] = 2;
  array[1] = 1;
  array[2] = 3;
  exparg = NULL;
  mutt_qsort_r(array, 3, sizeof(int), compare_ints, exparg);
  TEST_CHECK(array[0] == 1);
  TEST_CHECK(array[1] == 2);
  TEST_CHECK(array[2] == 3);

  array[0] = 2;
  array[1] = 1;
  array[2] = 3;
  exparg = array;
  mutt_qsort_r(array, 3, sizeof(int), compare_ints, exparg);
  TEST_CHECK(array[0] == 3);
  TEST_CHECK(array[1] == 2);
  TEST_CHECK(array[2] == 1);
}
