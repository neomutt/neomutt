/**
 * @file
 * Expando Node for a Condition
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
 * @page expando_node_condition Condition Node
 *
 * Expando Node for a Condition
 */

#include "config.h"
#include <assert.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "node_condition.h"
#include "node.h"
#include "render.h"

/**
 * node_condition_render - Render a Conditional Node - Implements ExpandoNode::render - @ingroup expando_render
 */
static int node_condition_render(const struct ExpandoNode *node,
                                 const struct ExpandoRenderData *rdata, struct Buffer *buf,
                                 int max_cols, void *data, MuttFormatFlags flags)
{
  assert(node->type == ENT_CONDITION);

  const struct ExpandoNode *node_cond = node_get_child(node, ENC_CONDITION);

  // Discard any text returned, just use the return value as a bool
  struct Buffer *buf_cond = buf_pool_get();
  int rc = node_cond->render(node_cond, rdata, buf_cond, max_cols, data, flags);
  buf_pool_release(&buf_cond);

  if (rc == true)
  {
    const struct ExpandoNode *node_true = node_get_child(node, ENC_TRUE);
    return node_tree_render(node_true, rdata, buf, max_cols, data, flags);
  }
  else
  {
    const struct ExpandoNode *node_false = node_get_child(node, ENC_FALSE);
    return node_tree_render(node_false, rdata, buf, max_cols, data, flags);
  }
}

/**
 * node_condition_new - Create a new Condition Expando Node
 * @param condition     Expando Node that will be tested
 * @param if_true_tree  Node tree for the 'true' case
 * @param if_false_tree Node tree for the 'false' case
 * @retval ptr New Condition Expando Node
 */
struct ExpandoNode *node_condition_new(struct ExpandoNode *condition,
                                       struct ExpandoNode *if_true_tree,
                                       struct ExpandoNode *if_false_tree)
{
  assert(condition);
  assert(if_true_tree);

  struct ExpandoNode *node = node_new();

  node->type = ENT_CONDITION;
  node->render = node_condition_render;

  ARRAY_SET(&node->children, ENC_CONDITION, condition);
  ARRAY_SET(&node->children, ENC_TRUE, if_true_tree);
  ARRAY_SET(&node->children, ENC_FALSE, if_false_tree);

  return node;
}
