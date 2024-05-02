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
    TEST_CHECK(node_get_child(NULL, -1) == NULL);
    TEST_CHECK(node_get_child(NULL, 10) == NULL);

    node_free(&node);
  }

  // struct ExpandoNode *node_first(struct ExpandoNode *node);
  // struct ExpandoNode *node_last (struct ExpandoNode *node);
  {
    struct ExpandoNode *root = node_new();
    struct ExpandoNode *child0 = node_new();
    struct ExpandoNode *child1 = node_new();
    struct ExpandoNode *child2 = node_new();
    struct ExpandoNode *next0 = node_new();
    struct ExpandoNode *next1 = node_new();
    struct ExpandoNode *next2 = node_new();
    struct ExpandoNode *n2child0 = node_new();
    struct ExpandoNode *n2child1 = node_new();
    struct ExpandoNode *n2child2 = node_new();

    ARRAY_SET(&root->children, 0, child0);
    ARRAY_SET(&root->children, 1, child1);
    ARRAY_SET(&root->children, 2, child2);
    root->next = next0;
    next0->next = next1;
    next1->next = next2;

    ARRAY_SET(&next2->children, 0, n2child0);
    ARRAY_SET(&next2->children, 1, n2child1);
    ARRAY_SET(&next2->children, 2, n2child2);

    struct ExpandoNode *node = NULL;

    node = node_first(NULL);
    TEST_CHECK(node == NULL);

    node = node_first(root);
    TEST_CHECK(node == child0);

    node = node_last(NULL);
    TEST_CHECK(node == NULL);

    node = node_last(root);
    TEST_CHECK(node == n2child2);

    node_tree_free(&root);
  }
}
