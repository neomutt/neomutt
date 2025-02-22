/**
 * @file
 * Test code for mutt_window_swap()
 *
 * @authors
 * Copyright (C) 2024 Dennis Schön <mail@dennis-schoen.de>
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
#include <stdbool.h>
#include <stddef.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "test_common.h"

// clang-format off
static const char * initial_order[] = {
  "apple",
  "banana",
  "cherry",
  "damson",
  "endive",
  "fig",
  "guava",
  "hawthorn"
};
// clang-format on

static const int num_children = mutt_array_size(initial_order);

static void wdata_free(struct MuttWindow *win, void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

static struct MuttWindow *new_window(const char *name)
{
  struct MuttWindow *win = mutt_window_new(WT_CUSTOM, MUTT_WIN_ORIENT_VERTICAL,
                                           MUTT_WIN_SIZE_MAXIMISE, MUTT_WIN_SIZE_UNLIMITED,
                                           MUTT_WIN_SIZE_UNLIMITED);
  win->wdata = mutt_str_dup(name);
  win->wdata_free = wdata_free;

  return win;
}

static struct MuttWindow *new_parent(void)
{
  struct MuttWindow *parent = new_window("parent");

  for (int i = 0; i < num_children; i++)
    mutt_window_add_child(parent, new_window(initial_order[i]));

  return parent;
}

static struct MuttWindow *find_child(struct MuttWindow *parent, const char *name)
{
  struct MuttWindow *win = NULL;
  TAILQ_FOREACH(win, &parent->children, entries)
  {
    if (mutt_str_equal(win->wdata, name))
      return win;
  }

  ASSERT(false);
}

static void check_order(struct MuttWindow *parent, const char **expected)
{
  int i = 0;
  struct MuttWindow *win = NULL;
  TAILQ_FOREACH(win, &parent->children, entries)
  {
    TEST_CHECK_STR_EQ(win->wdata, expected[i]);
    i++;
  }

  TEST_CHECK(i == num_children);
}

void test_window_swap(void)
{
  // bool mutt_window_swap(struct MuttWindow *parent, struct MuttWindow *win1, struct MuttWindow *win2);

  // degenerate

  {
    TEST_CHECK(!mutt_window_swap(NULL, NULL, NULL));
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *apple = find_child(parent, "apple");

    TEST_CHECK(!mutt_window_swap(NULL, apple, NULL));
    check_order(parent, initial_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *banana = find_child(parent, "banana");

    TEST_CHECK(!mutt_window_swap(NULL, NULL, banana));
    check_order(parent, initial_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *apple = find_child(parent, "apple");
    struct MuttWindow *banana = find_child(parent, "banana");

    TEST_CHECK(!mutt_window_swap(NULL, apple, banana));
    check_order(parent, initial_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();

    TEST_CHECK(!mutt_window_swap(parent, NULL, NULL));
    check_order(parent, initial_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *apple = find_child(parent, "apple");

    TEST_CHECK(!mutt_window_swap(parent, apple, NULL));
    check_order(parent, initial_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *banana = find_child(parent, "banana");

    TEST_CHECK(!mutt_window_swap(parent, NULL, banana));
    check_order(parent, initial_order);

    mutt_window_free(&parent);
  }

  // spread out

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *cherry = find_child(parent, "cherry");
    struct MuttWindow *fig = find_child(parent, "fig");

    TEST_CHECK(mutt_window_swap(parent, cherry, fig));

    static const char *expected_order[] = { "apple",  "banana",  "fig",
                                            "damson", "endive",  "cherry",
                                            "guava",  "hawthorn" };
    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *cherry = find_child(parent, "cherry");
    struct MuttWindow *fig = find_child(parent, "fig");

    TEST_CHECK(mutt_window_swap(parent, fig, cherry));

    static const char *expected_order[] = { "apple",  "banana",  "fig",
                                            "damson", "endive",  "cherry",
                                            "guava",  "hawthorn" };
    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  // neighbouring
  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *damson = find_child(parent, "damson");
    struct MuttWindow *endive = find_child(parent, "endive");

    TEST_CHECK(mutt_window_swap(parent, damson, endive));

    static const char *expected_order[] = { "apple",  "banana",  "cherry",
                                            "endive", "damson",  "fig",
                                            "guava",  "hawthorn" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *damson = find_child(parent, "damson");
    struct MuttWindow *endive = find_child(parent, "endive");

    TEST_CHECK(mutt_window_swap(parent, endive, damson));

    static const char *expected_order[] = { "apple",  "banana",  "cherry",
                                            "endive", "damson",  "fig",
                                            "guava",  "hawthorn" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  // edge tests, spread out
  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *apple = find_child(parent, "apple");
    struct MuttWindow *damson = find_child(parent, "damson");

    TEST_CHECK(mutt_window_swap(parent, apple, damson));

    static const char *expected_order[] = { "damson", "banana",  "cherry",
                                            "apple",  "endive",  "fig",
                                            "guava",  "hawthorn" };
    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *apple = find_child(parent, "apple");
    struct MuttWindow *damson = find_child(parent, "damson");

    TEST_CHECK(mutt_window_swap(parent, damson, apple));

    static const char *expected_order[] = { "damson", "banana",  "cherry",
                                            "apple",  "endive",  "fig",
                                            "guava",  "hawthorn" };
    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *endive = find_child(parent, "endive");
    struct MuttWindow *hawthorn = find_child(parent, "hawthorn");

    TEST_CHECK(mutt_window_swap(parent, endive, hawthorn));

    static const char *expected_order[] = {
      "apple",    "banana", "cherry", "damson",
      "hawthorn", "fig",    "guava",  "endive",
    };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *endive = find_child(parent, "endive");
    struct MuttWindow *hawthorn = find_child(parent, "hawthorn");

    TEST_CHECK(mutt_window_swap(parent, hawthorn, endive));

    static const char *expected_order[] = {
      "apple",    "banana", "cherry", "damson",
      "hawthorn", "fig",    "guava",  "endive",
    };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  // edge tests, neighbouring

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *apple = find_child(parent, "apple");
    struct MuttWindow *banana = find_child(parent, "banana");

    TEST_CHECK(mutt_window_swap(parent, banana, apple));

    static const char *expected_order[] = { "banana", "apple",   "cherry",
                                            "damson", "endive",  "fig",
                                            "guava",  "hawthorn" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *apple = find_child(parent, "apple");
    struct MuttWindow *banana = find_child(parent, "banana");

    TEST_CHECK(mutt_window_swap(parent, apple, banana));

    static const char *expected_order[] = { "banana", "apple",   "cherry",
                                            "damson", "endive",  "fig",
                                            "guava",  "hawthorn" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *guava = find_child(parent, "guava");
    struct MuttWindow *hawthorn = find_child(parent, "hawthorn");

    TEST_CHECK(mutt_window_swap(parent, guava, hawthorn));

    static const char *expected_order[] = { "apple",    "banana", "cherry",
                                            "damson",   "endive", "fig",
                                            "hawthorn", "guava" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *guava = find_child(parent, "guava");
    struct MuttWindow *hawthorn = find_child(parent, "hawthorn");

    TEST_CHECK(mutt_window_swap(parent, hawthorn, guava));

    static const char *expected_order[] = { "apple",    "banana", "cherry",
                                            "damson",   "endive", "fig",
                                            "hawthorn", "guava" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  // spread out tests, sharing a neighbour

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *banana = find_child(parent, "banana");
    struct MuttWindow *damson = find_child(parent, "damson");

    TEST_CHECK(mutt_window_swap(parent, banana, damson));

    static const char *expected_order[] = { "apple",  "damson",  "cherry",
                                            "banana", "endive",  "fig",
                                            "guava",  "hawthorn" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  {
    struct MuttWindow *parent = new_parent();
    struct MuttWindow *banana = find_child(parent, "banana");
    struct MuttWindow *damson = find_child(parent, "damson");

    TEST_CHECK(mutt_window_swap(parent, damson, banana));

    static const char *expected_order[] = { "apple",  "damson",  "cherry",
                                            "banana", "endive",  "fig",
                                            "guava",  "hawthorn" };

    check_order(parent, expected_order);

    mutt_window_free(&parent);
  }

  // different parent tests

  {
    struct MuttWindow *parent1 = new_parent();
    struct MuttWindow *parent2 = new_parent();
    struct MuttWindow *apple = find_child(parent1, "apple");
    struct MuttWindow *endive = find_child(parent2, "endive");

    TEST_CHECK(!mutt_window_swap(parent1, apple, endive));
    check_order(parent1, initial_order);

    TEST_CHECK(!mutt_window_swap(parent2, apple, endive));
    check_order(parent2, initial_order);

    mutt_window_free(&parent1);
    mutt_window_free(&parent2);
  }

  {
    struct MuttWindow *parent1 = new_parent();
    struct MuttWindow *parent2 = new_parent();
    struct MuttWindow *apple = find_child(parent1, "apple");
    struct MuttWindow *banana = find_child(parent1, "banana");

    TEST_CHECK(!mutt_window_swap(parent2, apple, banana));
    check_order(parent1, initial_order);
    check_order(parent2, initial_order);

    mutt_window_free(&parent1);
    mutt_window_free(&parent2);
  }
}
