/**
 * @file
 * Test code for NeoMutt's array API
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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
#include "mutt/array.h"

struct Dummy
{
  int i;
  double d;
};

ARRAY_HEAD(Dummies, struct Dummy);

static void test_get_one(struct Dummies *d, size_t idx)
{
  struct Dummy *elem = ARRAY_GET(d, idx);
  if (!TEST_CHECK(elem != NULL))
  {
    TEST_MSG("Expected: { %d, %lf }", idx, idx);
    TEST_MSG("Actual  : NULL");
  }
  if (!TEST_CHECK(elem->i == idx))
  {
    TEST_MSG("Expected: %d", idx);
    TEST_MSG("Actual  : %d", elem->i);
  }

  if (!TEST_CHECK(elem->d == (double) idx))
  {
    TEST_MSG("Expected: %lf", idx);
    TEST_MSG("Actual  : %lf", elem->d);
  }
  if (!TEST_CHECK(ARRAY_IDX(d, elem) == idx))
  {
    TEST_MSG("Expected: %zu", idx);
    TEST_MSG("Actual  : %zu", ARRAY_IDX(d, elem));
  }
}

static void test_get(struct Dummies *d, size_t nof_elem)
{
  for (size_t i = 0; i < nof_elem; i++)
  {
    test_get_one(d, i);
  }

  /* Get past the end */
  {
    struct Dummy *elem = ARRAY_GET(d, nof_elem);
    if (!TEST_CHECK(elem == NULL))
    {
      TEST_MSG("Expected: NULL");
      TEST_MSG("Actual  : { %d, %lf }", elem->i, elem->d);
    }
  }
}

static struct Dummy make_elem(size_t idx)
{
  struct Dummy elem;
  elem.i = idx;
  elem.d = idx;
  return elem;
}

static void test_set_one(struct Dummies *d, size_t idx)
{
  TEST_CHECK(ARRAY_SET(d, idx, make_elem(idx)));
}

static void test_set(struct Dummies *d, size_t begin, size_t end)
{
  for (size_t i = begin; i < end; i++)
  {
    test_set_one(d, i);
  }
  const size_t new_size = ARRAY_SIZE(d);
  if (!TEST_CHECK(new_size == end))
  {
    TEST_MSG("Expected: %zu", end);
    TEST_MSG("Actual  : %zu", new_size);
  }
}

static int gt(const void *a, const void *b)
{
  const struct Dummy *da = a;
  const struct Dummy *db = b;
  return db->i - da->i;
}

void test_mutt_array_api(void)
{
  const size_t nof_elem = 12;
  struct Dummies d = ARRAY_HEAD_INITIALIZER;

  /* Initial state */
  {
    TEST_CHECK(ARRAY_EMPTY(&d));
    TEST_CHECK(ARRAY_SIZE(&d) == 0);
    TEST_CHECK(ARRAY_CAPACITY(&d) == 0);
    TEST_CHECK(ARRAY_ELEM_SIZE(&d) == sizeof(struct Dummy));
  }

  /* Initialization */
  {
    struct Dummies d2;
    ARRAY_INIT(&d2);
    TEST_CHECK(ARRAY_EMPTY(&d2));
    TEST_CHECK(ARRAY_SIZE(&d2) == 0);
    TEST_CHECK(ARRAY_CAPACITY(&d2) == 0);
    TEST_CHECK(ARRAY_ELEM_SIZE(&d2) == sizeof(struct Dummy));
  }

  /* Reserve */
  {
    ARRAY_RESERVE(&d, 12);
    TEST_CHECK(ARRAY_EMPTY(&d));
    TEST_CHECK(ARRAY_SIZE(&d) == 0);
    if (!TEST_CHECK(ARRAY_CAPACITY(&d) == nof_elem + ARRAY_HEADROOM))
    {
      TEST_MSG("Expected: %zu", nof_elem + ARRAY_HEADROOM);
      TEST_MSG("Actual  : %zu", ARRAY_CAPACITY(&d));
    }
  }

  /* Set */
  test_set(&d, 0, nof_elem);

  /* Get */
  test_get(&d, nof_elem);

  /* First and last */
  {
    struct Dummy *fst = ARRAY_FIRST(&d);
    if (!TEST_CHECK(fst != NULL))
    {
      TEST_MSG("Expected: not null");
      TEST_MSG("Actual  : null");
    }
    if (!TEST_CHECK(fst->i == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", fst->i);
    }

    struct Dummy *lst = ARRAY_LAST(&d);
    if (!TEST_CHECK(lst != NULL))
    {
      TEST_MSG("Expected: not NULL");
      TEST_MSG("Actual  : NULL");
    }
    if (!TEST_CHECK(lst->i == nof_elem - 1))
    {
      TEST_MSG("Expected: %zu", nof_elem - 1);
      TEST_MSG("Actual  : %zu", lst->i);
    }
  }

  /* First and last on empty arrays */
  {
    ARRAY_HEAD(, int) a = ARRAY_HEAD_INITIALIZER;
    if (!TEST_CHECK(ARRAY_FIRST(&a) == NULL))
    {
      TEST_MSG("Expected: NULL");
      TEST_MSG("Actual  : not NULL");
    }
    if (!TEST_CHECK(ARRAY_LAST(&a) == NULL))
    {
      TEST_MSG("Expected: NULL");
      TEST_MSG("Actual  : not NULL");
    }
  }

  /* Realloc within the current boundaries */
  {
    const size_t before = ARRAY_CAPACITY(&d);
    const size_t after = ARRAY_RESERVE(&d, nof_elem + ARRAY_HEADROOM / 2);
    if (!TEST_CHECK(after == before))
    {
      TEST_MSG("Expected: %zu", before);
      TEST_MSG("Actual  : %zu", after);
    }
  }

  /* Realloc beyond the current boundaries */
  {
    size_t after = ARRAY_RESERVE(&d, 2 * nof_elem + ARRAY_HEADROOM);
    if (!TEST_CHECK(after == 2 * nof_elem + ARRAY_HEADROOM + ARRAY_HEADROOM))
    {
      TEST_MSG("Expected: %zu", 2 * nof_elem + ARRAY_HEADROOM + ARRAY_HEADROOM);
      TEST_MSG("Actual  : %zu", after);
    }
  }

  /* Get again - previous elements are still available */
  test_get(&d, nof_elem);

  /* Shrink */
  const size_t shrinkage = nof_elem / 2;
  const size_t new_nof_elem = nof_elem - shrinkage;
  {
    ARRAY_SHRINK(&d, shrinkage);
    if (!TEST_CHECK(ARRAY_SIZE(&d) == new_nof_elem))
    {
      TEST_MSG("Expected: %zu", new_nof_elem);
      TEST_MSG("Actual  : %zu", ARRAY_SIZE(&d));
    }
  }

  /* Get again - only the remaining ones */
  test_get(&d, new_nof_elem);

  /* Add elements after a hole */
  const size_t start = new_nof_elem + (nof_elem - new_nof_elem) / 2;
  test_set(&d, start, nof_elem);

  /* Get them all - the old ones are still there */
  test_get(&d, nof_elem);

  /* Add one by one - we stop short of one element to leave space for test_get,
   * which checks the subsequent element for NULL. We don't want to end up on a
   * page boundary and have test_get crash. */
  {
    const size_t begin = ARRAY_SIZE(&d);
    const size_t end = ARRAY_CAPACITY(&d) - 1;
    for (size_t idx = begin; idx < end; idx++)
    {
      ARRAY_ADD(&d, make_elem(idx));
      test_get_one(&d, idx);
    }
    test_get(&d, end);
  }

  /* Iteration */
  {
    struct Dummy *elem;
    size_t i = 0;
    ARRAY_FOREACH(elem, &d)
    {
      if (!TEST_CHECK(elem == ARRAY_GET(&d, i)))
      {
        TEST_MSG("Expected: %lp", ARRAY_GET(&d, i));
        TEST_MSG("Actual  : %lp", elem);
      }
      i++;
    }
    if (!TEST_CHECK(i == ARRAY_SIZE(&d)))
    {
      TEST_MSG("Expected: %zu", ARRAY_SIZE(&d));
      TEST_MSG("Actual  : %zu", i);
    }
  }

  /* Partial iteration - from */
  {
    struct Dummy *elem;
    size_t from = 4;
    ARRAY_FOREACH_FROM(elem, &d, from)
    {
      if (!TEST_CHECK(elem == ARRAY_GET(&d, from)))
      {
        TEST_MSG("Expected: %lp", ARRAY_GET(&d, from));
        TEST_MSG("Actual  : %lp", elem);
      }
      from++;
    }
  }

  /* Partial iteration - to */
  {
    struct Dummy *elem;
    size_t i = 0;
    size_t to = 10;
    ARRAY_FOREACH_TO(elem, &d, to)
    {
      if (!TEST_CHECK(elem == ARRAY_GET(&d, i)))
      {
        TEST_MSG("Expected: %lp", ARRAY_GET(&d, i));
        TEST_MSG("Actual  : %lp", elem);
      }
      i++;
    }
    if (!TEST_CHECK(i == to))
    {
      TEST_MSG("Expected: %zu", to);
      TEST_MSG("Actual  : %zu", i);
    }
  }

  /* Partial iteration - from+to */
  {
    struct Dummy *elem;
    size_t from = 4;
    size_t to = 10;
    ARRAY_FOREACH_FROM_TO(elem, &d, from, to)
    {
      if (!TEST_CHECK(elem == ARRAY_GET(&d, from)))
      {
        TEST_MSG("Expected: %lp", ARRAY_GET(&d, from));
        TEST_MSG("Actual  : %lp", elem);
      }
      from++;
    }
    if (!TEST_CHECK(from == to))
    {
      TEST_MSG("Expected: %zu", to);
      TEST_MSG("Actual  : %zu", from);
    }
  }

  /* Sorting */
  {
    ARRAY_SORT(&d, gt);
    struct Dummy *elem = NULL;
    ARRAY_FOREACH_FROM(elem, &d, 1)
    {
      int prev = ARRAY_GET(&d, ARRAY_FOREACH_IDX - 1)->i;
      int curr = elem->i;
      if (!TEST_CHECK(curr < prev))
      {
        TEST_MSG("Expected: %d < %d", curr, prev);
        TEST_MSG("Actual  : %d < %d", prev, curr);
      }
    }
  }

  /* Free */
  {
    ARRAY_FREE(&d);
    if (!TEST_CHECK(d.size == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", d.size);
    }
    if (!TEST_CHECK(d.capacity == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", d.capacity);
    }
    if (!TEST_CHECK(d.entries == NULL))
    {
      TEST_MSG("Expected: %lp", NULL);
      TEST_MSG("Actual  : %lp", d.entries);
    }
  }

  /* Iteration over an empty array */
  {
    struct Dummy *elem;
    ARRAY_FOREACH(elem, &d)
    {
      TEST_CHECK(false);
    }
  }

  /* Automatic resizing */
  {
    ARRAY_HEAD(, size_t) head = ARRAY_HEAD_INITIALIZER;
    for (size_t i = 0; i < 10; i++)
    {
      ARRAY_ADD(&head, i);
    }
    if (!TEST_CHECK(ARRAY_SIZE(&head) == 10))
    {
      TEST_MSG("Expected: %zu", 10);
      TEST_MSG("Actual  : %zu", ARRAY_SIZE(&head));
    }
    for (size_t i = 0; i < 10; i++)
    {
      if (!TEST_CHECK(*ARRAY_GET(&head, i) == i))
      {
        TEST_MSG("Expected: %zu", i);
        TEST_MSG("Actual  : %zu", *ARRAY_GET(&head, i));
      }
    }
    ARRAY_FREE(&head);
  }

  /* Removal */
  {
    const size_t to_rem = 5;

    ARRAY_HEAD(, size_t) head = ARRAY_HEAD_INITIALIZER;
    for (size_t i = 0; i < 10; i++)
    {
      ARRAY_ADD(&head, i);
    }

    size_t *elem = ARRAY_GET(&head, to_rem);
    ARRAY_REMOVE(&head, elem);

    if (!TEST_CHECK(ARRAY_SIZE(&head) == 9))
    {
      TEST_MSG("Expected: %zu", 9);
      TEST_MSG("Actual  : %zu", ARRAY_SIZE(&head));
    }
    for (size_t i = 0; i < 9; i++)
    {
      size_t res = i < to_rem ? i : i + 1;
      if (!TEST_CHECK(*ARRAY_GET(&head, i) == res))
      {
        TEST_MSG("Expected: %zu", res);
        TEST_MSG("Actual  : %zu", *ARRAY_GET(&head, i));
      }
    }
    ARRAY_FREE(&head);
  }
}
