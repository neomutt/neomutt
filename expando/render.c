/**
 * @file
 * Render Expandos using Data
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
 * @page expando_render Render Expandos using Data
 *
 * The caller uses ExpandoRenderData to define a set of callback functions.
 * The formatter uses these functions to get data, then format it.
 */

#include "config.h"
#include "render.h"
#include "node.h"

/**
 * node_render - Render a tree of ExpandoNodes into a string
 * @param node     Root of tree
 * @param rdata    Expando Render data
 * @param buf      Buffer for the result
 * @param max_cols Maximum number of screen columns to use
 * @param data     Private data
 * @param flags    Flags to control behaviour
 * @retval num Number of screen columns used
 */
int node_render(const struct ExpandoNode *node, const struct ExpandoRenderData *rdata,
                struct Buffer *buf, int max_cols, void *data, MuttFormatFlags flags)
{
  if (!node || !node->render)
    return 0;

  return node->render(node, rdata, buf, max_cols, data, flags);
}
