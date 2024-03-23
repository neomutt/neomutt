/**
 * @file
 * Test text justification
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_node(void)
{
  // struct ExpandoNode *node_new(void);
  // void node_free(struct ExpandoNode **ptr);
  {
    struct ExpandoNode *node = node_new();
    node_free(&node);
    node_free(NULL);
  }

  // void node_tree_free(struct ExpandoNode **ptr);
  {
    struct ExpandoNode *first = node_new();
    struct ExpandoNode *last = first;

    for (int i = 0; i < 5; i++)
    {
      struct ExpandoNode *node = node_new();
      last->next = node;
      last = node;
    }

    node_tree_free(&first);
  }

  // void node_append(struct ExpandoNode **root, struct ExpandoNode *new_node);
  {
    struct ExpandoNode *first = NULL;

    for (int i = 0; i < 5; i++)
    {
      struct ExpandoNode *node = node_new();
      node_append(&first, node);
      TEST_CHECK(first != NULL);
    }

    node_tree_free(&first);
  }

  // struct ExpandoNode *node_get_child(const struct ExpandoNode *node, int index);
  // void node_set_child(struct ExpandoNode *node, int index, struct ExpandoNode *child);
  {
    struct ExpandoNode *node = node_new();

    // Consecutive
    node_set_child(node, 0, node_new());
    node_set_child(node, 1, node_new());
    node_set_child(node, 2, node_new());

    // Skips
    node_set_child(node, 4, node_new());
    node_set_child(node, 6, node_new());
    node_set_child(node, 8, node_new());

    // Overwrite
    node_set_child(node, 1, node_new());
    node_set_child(node, 2, node_new());
    node_set_child(node, 2, node_new());

    struct ExpandoNode *n2 = node_new();
    node_set_child(NULL, 2, n2);
    node_set_child(n2, 2, NULL);
    node_free(&n2);

    TEST_CHECK(node_get_child(node, 0) != NULL);
    TEST_CHECK(node_get_child(node, 1) != NULL);
    TEST_CHECK(node_get_child(node, 2) != NULL);

    TEST_CHECK(node_get_child(node, 3) == NULL);
    TEST_CHECK(node_get_child(node, 5) == NULL);

    TEST_CHECK(node_get_child(NULL, 0) == NULL);
    TEST_CHECK(node_get_child(NULL, -1) == NULL);
    TEST_CHECK(node_get_child(NULL, 10) == NULL);

    node_free(&node);
  }
}
