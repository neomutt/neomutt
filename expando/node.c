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
  return mutt_mem_calloc(1, sizeof(struct ExpandoNode));
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

  struct ExpandoNode **enp = NULL;
  ARRAY_FOREACH(enp, &node->children)
  {
    node_free(enp);
  }
  ARRAY_FREE(&node->children);

  if (node->ndata_free)
    node->ndata_free(&node->ndata);

  FREE(&node->format);

  FREE(ptr);
}

/**
 * node_add_child - Add a child to an ExpandoNode
 * @param node  Parent node
 * @param child Child node
 */
void node_add_child(struct ExpandoNode *node, struct ExpandoNode *child)
{
  if (!node)
    return;

  ARRAY_ADD(&node->children, child);
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
 * node_last - Find the last Node in a tree
 * @param node Root Node
 * @retval ptr Last Node
 */
struct ExpandoNode *node_last(struct ExpandoNode *node)
{
  if (!node)
    return NULL;

  struct ExpandoNode **np = NULL;
  while ((np = ARRAY_LAST(&node->children)) && *np)
  {
    node = *np;
  }

  return node;
}
