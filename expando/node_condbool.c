/**
 * @file
 * Expando Node for a Conditional Boolean
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
 * @page expando_node_condbool Condition Boolean Node
 *
 * Expando Node for a Conditional Boolean
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "node_condbool.h"
#include "helpers.h"
#include "node.h"
#include "render.h"

/**
 * node_condbool_render - Callback for every bool node - Implements ExpandoNode::render() - @ingroup expando_render
 */
int node_condbool_render(const struct ExpandoNode *node,
                         const struct ExpandoRenderData *rdata, struct Buffer *buf,
                         int max_cols, void *data, MuttFormatFlags flags)
{
  ASSERT(node->type == ENT_CONDBOOL);

  const struct ExpandoRenderData *rd_match = find_get_number(rdata, node->did, node->uid);
  if (rd_match)
  {
    const long num = rd_match->get_number(node, data, flags);
    return (num != 0); // bool-ify
  }

  rd_match = find_get_string(rdata, node->did, node->uid);
  if (rd_match)
  {
    struct Buffer *buf_str = buf_pool_get();
    rd_match->get_string(node, data, flags, buf_str);
    const size_t len = buf_len(buf_str);
    buf_pool_release(&buf_str);

    return (len > 0); // bool-ify
  }

  return 0;
}
