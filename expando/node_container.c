/**
 * @file
 * Expando Node for a Container
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
 * @page expando_node_container Container Node
 *
 * Expando Node for a Container
 */

#include "config.h"
#include <stddef.h>
#include <assert.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "node_container.h"
#include "format.h"
#include "node.h"
#include "render.h"

/**
 * node_container_render - Callback for a Container Node - Implements ExpandoNode::render - @ingroup expando_render
 */
int node_container_render(const struct ExpandoNode *node,
                          const struct ExpandoRenderData *rdata, struct Buffer *buf,
                          int max_cols, void *data, MuttFormatFlags flags)
{
  assert(node->type == ENT_CONTAINER);

  const struct ExpandoFormat *fmt = node->format;
  if (fmt)
    max_cols = MIN(max_cols, fmt->max_cols);

  int total_cols = 0;

  struct Buffer *tmp = buf_pool_get();
  struct ExpandoNode **enp = NULL;
  ARRAY_FOREACH(enp, &node->children)
  {
    total_cols += node_tree_render(*enp, rdata, tmp, max_cols - total_cols, data, flags);
  }

  if (fmt)
  {
    struct Buffer *tmp2 = buf_pool_get();
    total_cols = format_string(tmp2, fmt->min_cols, fmt->max_cols, fmt->justification,
                               fmt->leader, buf_string(tmp), buf_len(tmp), true);
    buf_addstr(buf, buf_string(tmp2));
    buf_pool_release(&tmp2);
  }
  else
  {
    buf_addstr(buf, buf_string(tmp));
  }

  buf_pool_release(&tmp);
  return total_cols;
}

/**
 * node_container_new - Create a new Container ExpandoNode
 * @retval ptr New Container ExpandoNode
 */
struct ExpandoNode *node_container_new(void)
{
  struct ExpandoNode *node = node_new();

  node->type = ENT_CONTAINER;
  node->render = node_container_render;

  return node;
}
