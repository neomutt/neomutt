/**
 * @file
 * Expando Node for Padding
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
 * @page expando_node_padding Padding Node
 *
 * Expando Node for Padding
 */

#include "config.h"
#include <stddef.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "gui/lib.h"
#include "node_padding.h"
#include "definition.h"
#include "node.h"
#include "node_condition.h"
#include "parse.h"
#include "render.h"

/**
 * node_padding_private_new - Create new Padding private data
 * @param pad_type Padding type
 * @retval ptr New Padding private data
 */
struct NodePaddingPrivate *node_padding_private_new(enum ExpandoPadType pad_type)
{
  struct NodePaddingPrivate *priv = MUTT_MEM_CALLOC(1, struct NodePaddingPrivate);

  priv->pad_type = pad_type;

  return priv;
}

/**
 * node_padding_private_free - Free Padding private data - Implements ExpandoNode::ndata_free()
 * @param ptr Data to free
 */
void node_padding_private_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * pad_string - Pad a buffer with a character
 * @param node     Node with padding type
 * @param buf      Buffer to populate
 * @param max_cols Number of screen columns available
 *
 * Fill buf with the padding char (Node.start) to a maximum of max_cols screen cells.
 */
int pad_string(const struct ExpandoNode *node, struct Buffer *buf, int max_cols)
{
  const int pad_len = node->end - node->start;
  const int pad_cols = mutt_strnwidth(node->start, pad_len);
  int total_cols = 0;

  while (pad_cols <= max_cols)
  {
    buf_addstr_n(buf, node->start, pad_len);

    max_cols -= pad_cols;
    total_cols += pad_cols;
  }

  for (; (max_cols > 0); buf++, max_cols--)
  {
    buf_addch(buf, ' ');
    total_cols++;
  }

  return total_cols;
}

/**
 * node_padding_render_eol - Render End-of-Line Padding - Implements ExpandoNode::render() - @ingroup expando_render
 */
int node_padding_render_eol(const struct ExpandoNode *node,
                            const struct ExpandoRenderData *rdata, struct Buffer *buf,
                            int max_cols, void *data, MuttFormatFlags flags)
{
  struct ExpandoNode *left = node_get_child(node, ENP_LEFT);

  int total_cols = node_render(left, rdata, buf, max_cols, data, flags);

  total_cols += pad_string(node, buf, max_cols - total_cols);

  return total_cols;
}

/**
 * node_padding_render_hard - Render Hard Padding - Implements ExpandoNode::render() - @ingroup expando_render
 *
 * Text to the left of the padding is hard and will be preserved if possible.
 * Text to the right of the padding will be truncated.
 */
int node_padding_render_hard(const struct ExpandoNode *node,
                             const struct ExpandoRenderData *rdata, struct Buffer *buf,
                             int max_cols, void *data, MuttFormatFlags flags)
{
  struct Buffer *buf_left = buf_pool_get();
  struct Buffer *buf_pad = buf_pool_get();
  struct Buffer *buf_right = buf_pool_get();

  int cols_used = 0;

  struct ExpandoNode *left = node_get_child(node, ENP_LEFT);
  if (left)
    cols_used += node_render(left, rdata, buf_left, max_cols - cols_used, data, flags);

  struct ExpandoNode *right = node_get_child(node, ENP_RIGHT);
  if (right)
    cols_used += node_render(right, rdata, buf_right, max_cols - cols_used, data, flags);

  if (max_cols > cols_used)
    cols_used += pad_string(node, buf_pad, max_cols - cols_used);

  buf_addstr(buf, buf_string(buf_left));
  buf_addstr(buf, buf_string(buf_pad));
  buf_addstr(buf, buf_string(buf_right));

  buf_pool_release(&buf_left);
  buf_pool_release(&buf_pad);
  buf_pool_release(&buf_right);

  return cols_used;
}

/**
 * node_padding_render_soft - Render Soft Padding - Implements ExpandoNode::render() - @ingroup expando_render
 *
 * Text to the right of the padding is hard and will be preserved if possible.
 * Text to the left of the padding will be truncated.
 */
int node_padding_render_soft(const struct ExpandoNode *node,
                             const struct ExpandoRenderData *rdata, struct Buffer *buf,
                             int max_cols, void *data, MuttFormatFlags flags)
{
  struct Buffer *buf_left = buf_pool_get();
  struct Buffer *buf_pad = buf_pool_get();
  struct Buffer *buf_right = buf_pool_get();

  int cols_used = 0;

  struct ExpandoNode *right = node_get_child(node, ENP_RIGHT);
  if (right)
    cols_used += node_render(right, rdata, buf_right, max_cols - cols_used, data, flags);

  struct ExpandoNode *left = node_get_child(node, ENP_LEFT);
  if (left)
    cols_used += node_render(left, rdata, buf_left, max_cols - cols_used, data, flags);

  if (max_cols > cols_used)
    cols_used += pad_string(node, buf_pad, max_cols - cols_used);

  buf_addstr(buf, buf_string(buf_left));
  buf_addstr(buf, buf_string(buf_pad));
  buf_addstr(buf, buf_string(buf_right));

  buf_pool_release(&buf_left);
  buf_pool_release(&buf_pad);
  buf_pool_release(&buf_right);

  return cols_used;
}

/**
 * node_padding_new - Creata new Padding ExpandoNode
 * @param pad_type Padding type
 * @param start    Start of padding character
 * @param end      End of padding character
 * @retval ptr New Padding ExpandoNode
 */
struct ExpandoNode *node_padding_new(enum ExpandoPadType pad_type,
                                     const char *start, const char *end)
{
  struct ExpandoNode *node = node_new();

  node->type = ENT_PADDING;
  node->start = start;
  node->end = end;

  switch (pad_type)
  {
    case EPT_FILL_EOL:
      node->render = node_padding_render_eol;
      break;
    case EPT_HARD_FILL:
      node->render = node_padding_render_hard;
      break;
    case EPT_SOFT_FILL:
      node->render = node_padding_render_soft;
      break;
  };

  node->ndata = node_padding_private_new(pad_type);
  node->ndata_free = node_padding_private_free;

  return node;
}

/**
 * node_padding_parse - Parse a Padding Expando - Implements ExpandoDefinition::parse() - @ingroup expando_parse_api
 *
 * Parse a Padding Expando of the form, "%|X", "%>X" or "%*X",
 * where the character 'X' will be used to fill the space.
 */
struct ExpandoNode *node_padding_parse(const char *str, const char **parsed_until,
                                       int did, int uid, ExpandoParserFlags flags,
                                       struct ExpandoParseError *error)
{
  if (flags & EP_CONDITIONAL)
  {
    snprintf(error->message, sizeof(error->message),
             // L10N: Conditional Expandos can only depend on other Expandos
             //       e.g. "%<X?apple>" displays "apple" if "%X" is true.
             _("Padding cannot be used as a condition"));
    error->position = str;
    return NULL;
  }

  enum ExpandoPadType pt = 0;
  if (*str == '|')
  {
    pt = EPT_FILL_EOL;
  }
  else if (*str == '>')
  {
    pt = EPT_HARD_FILL;
  }
  else if (*str == '*')
  {
    pt = EPT_SOFT_FILL;
  }
  else
  {
    return NULL;
  }
  str++;

  const size_t consumed = mutt_mb_charlen(str, NULL);

  *parsed_until = str + consumed;
  return node_padding_new(pt, str, str + consumed);
}

/**
 * node_padding_repad - Rearrange Padding in a tree of ExpandoNodes
 * @param parent Parent Node
 */
void node_padding_repad(struct ExpandoNode **parent)
{
  if (!parent || !*parent)
    return;

  struct ExpandoNode *node = *parent;
  struct ExpandoNode *prev = NULL;
  for (; node; prev = node, node = node->next)
  {
    if (node->type == ENT_PADDING)
    {
      if (node != *parent)
        ARRAY_SET(&node->children, ENP_LEFT, *parent); // First sibling

      ARRAY_SET(&node->children, ENP_RIGHT, node->next); // Sibling after Padding

      if (prev)
        prev->next = NULL;
      node->next = NULL;
      *parent = node;
      return;
    }

    if (node->type == ENT_CONDITION)
    {
      struct ExpandoNode **ptr = NULL;

      ptr = ARRAY_GET(&node->children, ENC_TRUE);
      node_padding_repad(ptr);

      ptr = ARRAY_GET(&node->children, ENC_FALSE);
      node_padding_repad(ptr);
    }
  }
}
