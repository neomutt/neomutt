/**
 * @file
 * Basic Expando Node
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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
 * @page expando_node Basic Expando Node
 *
 * This Node is the "base class" of all other Node types.
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "node.h"

/**
 * node_new - Create a new empty ExpandoNode
 * @retval ptr New ExpandoNode
 */
struct ExpandoNode *node_new(void)
{
  return MUTT_MEM_CALLOC(1, struct ExpandoNode);
}

/**
 * node_free - Free an ExpandoNode and its private data
 * @param ptr Node to free
 */
void node_free(struct ExpandoNode **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ExpandoNode *node = *ptr;
  if (node->ndata_free)
  {
    node->ndata_free(&node->ndata);
  }

  struct ExpandoNode **enp = NULL;
  ARRAY_FOREACH(enp, &node->children)
  {
    node_tree_free(enp);
  }

  FREE(&node->format);

  ARRAY_FREE(&node->children);

  FREE(ptr);
}

/**
 * node_tree_free - Free a tree of ExpandoNodes
 * @param ptr Root of tree to free
 */
void node_tree_free(struct ExpandoNode **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct ExpandoNode *node = *ptr;
  while (node)
  {
    struct ExpandoNode *n = node;
    node = node->next;
    node_free(&n);
  }

  *ptr = NULL;
}

/**
 * node_get_child - Get a child of an ExpandoNode
 * @param node  Parent node
 * @param index Index of child to get
 * @retval ptr  Child node
 * @retval NULL No child, or index out of range
 */
struct ExpandoNode *node_get_child(const struct ExpandoNode *node, int index)
{
  if (!node)
    return NULL;

  struct ExpandoNode **ptr = ARRAY_GET(&node->children, index);
  if (!ptr)
    return NULL;

  return *ptr;
}

/**
 * node_append - Append an ExpandoNode to an existing node
 * @param[in,out] root     Existing node (may be NULL)
 * @param[in]     new_node Node to add
 */
void node_append(struct ExpandoNode **root, struct ExpandoNode *new_node)
{
  if (!*root)
  {
    *root = new_node;
    return;
  }

  struct ExpandoNode *node = *root;
  while (node->next)
  {
    node = node->next;
  }

  node->next = new_node;
}

/**
 * node_last - Find the last Node in a tree
 * @param node Root Node
 * @retval ptr Last Node
 */
struct ExpandoNode *node_last(struct ExpandoNode *node)
{
  if (!node)
    return NULL;

  while (true)
  {
    if (node->next)
    {
      node = node->next;
      continue;
    }

    size_t size = ARRAY_SIZE(&node->children);
    if (size == 0)
      break;

    struct ExpandoNode *last = node_get_child(node, size - 1);
    if (!last)
      break; // LCOV_EXCL_LINE

    node = last;
  }

  return node;
}
