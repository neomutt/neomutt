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
 * The caller uses ExpandoRenderCallback to define a set of callback functions.
 * The formatter uses these functions to get data, then format it.
 */

#include "config.h"
#include "render.h"
#include "node.h"

/// Width of the line being rendered by expando_render(), -1 if unlimited
int ExpandoRenderCols = -1;

/**
 * format_resolve - Turn percentage widths into screen columns
 * @param fmt Format, may be NULL
 * @param tmp Storage for the resolved copy
 * @retval ptr fmt itself, or tmp holding absolute widths
 *
 * With an unlimited width, a percentage minimum is dropped and a percentage
 * maximum means no limit.
 */
const struct ExpandoFormat *format_resolve(const struct ExpandoFormat *fmt,
                                           struct ExpandoFormat *tmp)
{
  if (!fmt || (!fmt->min_pct && !fmt->max_pct))
    return fmt;

  *tmp = *fmt;
  tmp->min_pct = false;
  tmp->max_pct = false;
  if (fmt->min_pct)
    tmp->min_cols = (ExpandoRenderCols < 0) ? 0 : ExpandoRenderCols * fmt->min_cols / 100;
  if (fmt->max_pct)
  {
    tmp->max_cols = (ExpandoRenderCols < 0) ? -1 : ExpandoRenderCols * fmt->max_cols / 100;
    // Don't round down to 0, which an Expando node reads as unlimited
    if ((tmp->max_cols == 0) && (fmt->max_cols > 0))
      tmp->max_cols = 1;
  }
  return tmp;
}

/**
 * node_render - Render a tree of ExpandoNodes into a string
 * @param node     Root of tree
 * @param erc      Expando Render Callback functions
 * @param buf      Buffer for the result
 * @param max_cols Maximum number of screen columns to use
 * @param data     Private data
 * @param flags    Flags to control behaviour
 * @retval num Number of screen columns used
 */
int node_render(const struct ExpandoNode *node, const struct ExpandoRenderCallback *erc,
                struct Buffer *buf, int max_cols, void *data, MuttFormatFlags flags)
{
  if (!node || !node->render)
    return 0;

  return node->render(node, erc, buf, max_cols, data, flags);
}
