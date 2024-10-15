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
#include "mutt/lib.h"
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

    node_add_child(NULL, NULL);
  }

  // void node_free(struct ExpandoNode **ptr);
  {
    struct ExpandoNode *first = node_new();

    for (int i = 0; i < 5; i++)
    {
      struct ExpandoNode *node = node_new();
      node_add_child(first, node);
    }

    node_free(&first);
  }

  // struct ExpandoNode *node_get_child(const struct ExpandoNode *node, int index);
  {
    struct ExpandoNode *node = node_new();

    // Consecutive
    ARRAY_SET(&node->children, 0, node_new());
    ARRAY_SET(&node->children, 1, node_new());
    ARRAY_SET(&node->children, 2, node_new());

    // Skips
    ARRAY_SET(&node->children, 4, node_new());
    ARRAY_SET(&node->children, 6, node_new());
    ARRAY_SET(&node->children, 8, node_new());

    TEST_CHECK(node_get_child(node, 0) != NULL);
    TEST_CHECK(node_get_child(node, 1) != NULL);
    TEST_CHECK(node_get_child(node, 2) != NULL);

    TEST_CHECK(node_get_child(node, 3) == NULL);
    TEST_CHECK(node_get_child(node, 5) == NULL);

    TEST_CHECK(node_get_child(NULL, 0) == NULL);
    TEST_CHECK(node_get_child(node, -1) == NULL);
    TEST_CHECK(node_get_child(node, 10) == NULL);

    node_free(&node);
  }

  // struct ExpandoNode *node_last (struct ExpandoNode *node);
  {
    struct ExpandoNode *root = node_new();
    struct ExpandoNode *child0 = node_new();
    struct ExpandoNode *child1 = node_new();
    struct ExpandoNode *child2 = node_new();

    node_add_child(root, child0);
    node_add_child(root, child1);
    node_add_child(root, child2);

    struct ExpandoNode *child00 = node_new();
    struct ExpandoNode *child01 = node_new();
    struct ExpandoNode *child02 = node_new();

    node_add_child(child0, child00);
    node_add_child(child0, child01);
    node_add_child(child0, child02);

    struct ExpandoNode *child20 = node_new();
    struct ExpandoNode *child21 = node_new();
    struct ExpandoNode *child22 = node_new();

    node_add_child(child2, child20);
    node_add_child(child2, child21);
    node_add_child(child2, child22);

    struct ExpandoNode *node = NULL;

    node = node_last(NULL);
    TEST_CHECK(node == NULL);

    node = node_last(root);
    TEST_CHECK(node == child22);

    node_free(&root);
  }
}
