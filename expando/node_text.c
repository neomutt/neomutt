/**
 * @file
 * Expando Node for Text
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
 * @page expando_node_text Text Node
 *
 * Expando Node for Text
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "node_text.h"
#include "format.h"
#include "node.h"
#include "render.h"

/**
 * node_text_render - Render a Text Node - Implements ExpandoNode::render() - @ingroup expando_render
 */
static int node_text_render(const struct ExpandoNode *node,
                            const struct ExpandoRenderData *rdata, struct Buffer *buf,
                            int max_cols, void *data, MuttFormatFlags flags)
{
  ASSERT(node->type == ENT_TEXT);

  const int num_bytes = node->end - node->start;
  return format_string(buf, 0, max_cols, JUSTIFY_LEFT, ' ', node->start, num_bytes, false);
}

/**
 * node_text_new - Create a new Text ExpandoNode
 * @param start Start of text to store
 * @param end   End of text to store
 * @retval ptr New Text ExpandoNode
 */
struct ExpandoNode *node_text_new(const char *start, const char *end)
{
  struct ExpandoNode *node = node_new();

  node->type = ENT_TEXT;
  node->start = start;
  node->end = end;
  node->render = node_text_render;

  return node;
}

/**
 * skip_until_ch_or_end - Search for a terminator character
 * @param start      Start of string
 * @param terminator Terminating character
 * @param end        End of string
 * @retval ptr Position of terminator character, or end-of-string
 */
static const char *skip_until_ch_or_end(const char *start, char terminator, const char *end)
{
  while (*start)
  {
    if (*start == terminator)
    {
      break;
    }

    if (end && (start > (end - 1)))
    {
      break;
    }

    start++;
  }

  return start;
}

/**
 * node_text_parse - Extract a block of text
 * @param str          String to parse
 * @param end          End of string
 * @param parsed_until First character after parsed text
 * @retval ptr New Text ExpandoNode
 */
struct ExpandoNode *node_text_parse(const char *str, const char *end, const char **parsed_until)
{
  const char *text_end = skip_until_ch_or_end(str, '%', end);
  *parsed_until = text_end;
  return node_text_new(str, text_end);
}
