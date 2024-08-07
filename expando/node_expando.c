/**
 * @file
 * Expando Node for an Expando
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
 * @page expando_node_expando Expando Node
 *
 * Expando Node for an Expando
 */

#include "config.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "node_expando.h"
#include "color/lib.h"
#include "definition.h"
#include "format.h"
#include "helpers.h"
#include "mutt_thread.h"
#include "node.h"
#include "parse.h"
#include "render.h"

/**
 * node_expando_private_new - Create new Expando private data
 * @retval ptr New Expando private data
 */
struct NodeExpandoPrivate *node_expando_private_new(void)
{
  struct NodeExpandoPrivate *priv = MUTT_MEM_CALLOC(1, struct NodeExpandoPrivate);

  // NOTE(g0mb4): Expando definition should contain this
  priv->color = -1;

  return priv;
}

/**
 * node_expando_private_free - Free Expando private data - Implements ExpandoNode::ndata_free()
 * @param ptr Data to free
 */
void node_expando_private_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  FREE(ptr);
}

/**
 * node_expando_new - Create a new Expando ExpandoNode
 * @param start  Start of Expando string
 * @param end    End of Expando string
 * @param fmt    Formatting data
 * @param did    Domain ID
 * @param uid    Unique ID
 * @retval ptr New Expando ExpandoNode
 */
struct ExpandoNode *node_expando_new(const char *start, const char *end,
                                     struct ExpandoFormat *fmt, int did, int uid)
{
  struct ExpandoNode *node = node_new();

  node->type = ENT_EXPANDO;
  node->start = start;
  node->end = end;

  node->did = did;
  node->uid = uid;
  node->render = node_expando_render;

  node->format = fmt;

  node->ndata = node_expando_private_new();
  node->ndata_free = node_expando_private_free;

  return node;
}

/**
 * node_expando_set_color - Set the colour for an Expando
 * @param node Node to alter
 * @param cid  Colour
 */
void node_expando_set_color(const struct ExpandoNode *node, int cid)
{
  if (!node || (node->type != ENT_EXPANDO) || !node->ndata)
    return;

  struct NodeExpandoPrivate *priv = node->ndata;

  priv->color = cid;
}

/**
 * node_expando_set_has_tree - Set the has_tree flag for an Expando
 * @param node     Node to alter
 * @param has_tree Flag to set
 */
void node_expando_set_has_tree(const struct ExpandoNode *node, bool has_tree)
{
  if (!node || (node->type != ENT_EXPANDO) || !node->ndata)
    return;

  struct NodeExpandoPrivate *priv = node->ndata;

  priv->has_tree = has_tree;
}

/**
 * parse_format - Parse a format string
 * @param start Start of string
 * @param end   End of string
 * @param error Buffer for errors
 * @retval ptr New ExpandoFormat object
 *
 * Parse a printf()-style format, e.g. '-15.20x'
 */
struct ExpandoFormat *parse_format(const char *start, const char *end,
                                   struct ExpandoParseError *error)
{
  if (start == end)
    return NULL;

  struct ExpandoFormat *fmt = MUTT_MEM_CALLOC(1, struct ExpandoFormat);

  fmt->leader = ' ';
  fmt->start = start;
  fmt->end = end;
  fmt->justification = JUSTIFY_RIGHT;
  fmt->min_cols = 0;
  fmt->max_cols = INT_MAX;

  if (*start == '-')
  {
    fmt->justification = JUSTIFY_LEFT;
    start++;
  }
  else if (*start == '=')
  {
    fmt->justification = JUSTIFY_CENTER;
    start++;
  }

  if (*start == '0')
  {
    fmt->leader = '0';
    start++;
  }

  if (isdigit(*start))
  {
    unsigned short number = 0;
    const char *end_ptr = mutt_str_atous(start, &number);

    // NOTE(g0mb4): start is NOT null-terminated
    if (!end_ptr || (end_ptr > end) || (number == USHRT_MAX))
    {
      error->position = start;
      snprintf(error->message, sizeof(error->message), _("Invalid number: %s"), start);
      FREE(&fmt);
      return NULL;
    }

    fmt->min_cols = number;
    start = end_ptr;
  };

  if (*start == '.')
  {
    start++;

    if (!isdigit(*start))
    {
      error->position = start;
      // L10N: Expando format expected a number
      snprintf(error->message, sizeof(error->message), _("Number is expected"));
      FREE(&fmt);
      return NULL;
    }

    unsigned short number = 0;
    const char *end_ptr = mutt_str_atous(start, &number);

    // NOTE(g0mb4): start is NOT null-terminated
    if (!end_ptr || (end_ptr > end) || (number == USHRT_MAX))
    {
      error->position = start;
      snprintf(error->message, sizeof(error->message), _("Invalid number: %s"), start);
      FREE(&fmt);
      return NULL;
    }

    fmt->max_cols = number;
    start = end_ptr;
  }

  if (*start == '_')
  {
    fmt->lower = true;
    start++;
  }

  return fmt;
}

/**
 * node_expando_parse - Parse an Expando format string
 * @param[in]  str          String to parse
 * @param[out] parsed_until First character after parsed string
 * @param[in]  defs         Expando definitions
 * @param[in]  flags        Flag for conditional expandos
 * @param[out] error        Buffer for errors
 * @retval ptr New ExpandoNode
 */
struct ExpandoNode *node_expando_parse(const char *str, const char **parsed_until,
                                       const struct ExpandoDefinition *defs,
                                       ExpandoParserFlags flags,
                                       struct ExpandoParseError *error)
{
  const struct ExpandoDefinition *definition = defs;

  const char *format_end = skip_until_classic_expando(str);
  const char *expando_end = skip_classic_expando(format_end, defs);
  char expando[128] = { 0 };
  const int expando_len = expando_end - format_end;
  mutt_strn_copy(expando, format_end, expando_len, sizeof(expando));

  struct ExpandoFormat *fmt = parse_format(str, format_end, error);
  if (error->position)
  {
    FREE(&fmt);
    return NULL;
  }

  while (definition && definition->short_name)
  {
    if (mutt_str_equal(definition->short_name, expando))
    {
      if (definition->parse && !(flags & EP_NO_CUSTOM_PARSE))
      {
        FREE(&fmt);
        return definition->parse(str, parsed_until, definition->did,
                                 definition->uid, flags, error);
      }
      else
      {
        *parsed_until = expando_end;
        return node_expando_new(format_end, expando_end, fmt, definition->did,
                                definition->uid);
      }
    }

    definition++;
  }

  error->position = format_end;
  // L10N: e.g. "Unknown expando: %Q"
  snprintf(error->message, sizeof(error->message), _("Unknown expando: %%%.*s"),
           expando_len, format_end);
  FREE(&fmt);
  return NULL;
}

/**
 * node_expando_parse_enclosure - Parse an enclosed Expando
 * @param str          String to parse
 * @param parsed_until First character after the parsed string
 * @param did          Domain ID
 * @param uid          Unique ID
 * @param terminator   Terminating character
 * @param error        Buffer for errors
 * @retval ptr New ExpandoNode
 */
struct ExpandoNode *node_expando_parse_enclosure(const char *str, const char **parsed_until,
                                                 int did, int uid, char terminator,
                                                 struct ExpandoParseError *error)
{
  const char *format_end = skip_until_classic_expando(str);

  format_end++; // skip opening char

  const char *expando_end = skip_until_ch(format_end, terminator);

  if (*expando_end != terminator)
  {
    error->position = expando_end;
    snprintf(error->message, sizeof(error->message),
             // L10N: Expando is missing a terminator character
             //       e.g. "%[..." is missing the final ']'
             _("Expando is missing terminator: '%c'"), terminator);
    return NULL;
  }

  // revert skipping for fmt
  struct ExpandoFormat *fmt = parse_format(str, format_end - 1, error);
  if (error->position)
  {
    FREE(&fmt);
    return NULL;
  }

  *parsed_until = expando_end + 1;
  return node_expando_new(format_end, expando_end, fmt, did, uid);
}

/**
 * add_color - Add a colour code to a buffer
 * @param buf Buffer for colour code
 * @param cid Colour
 */
void add_color(struct Buffer *buf, enum ColorId cid)
{
  ASSERT(cid < MT_COLOR_MAX);

  buf_addch(buf, MUTT_SPECIAL_INDEX);
  buf_addch(buf, cid);
}

/**
 * node_expando_render - Render an Expando Node - Implements ExpandoNode::render() - @ingroup expando_render
 */
int node_expando_render(const struct ExpandoNode *node,
                        const struct ExpandoRenderData *rdata, struct Buffer *buf,
                        int max_cols, void *data, MuttFormatFlags flags)
{
  ASSERT(node->type == ENT_EXPANDO);

  struct Buffer *buf_expando = buf_pool_get();

  const struct ExpandoRenderData *rd_match = find_get_string(rdata, node->did, node->uid);
  if (rd_match)
  {
    rd_match->get_string(node, data, flags, max_cols, buf_expando);
  }
  else
  {
    rd_match = find_get_number(rdata, node->did, node->uid);
    ASSERT(rd_match && "Unknown UID");

    const long num = rd_match->get_number(node, data, flags);
    buf_printf(buf_expando, "%ld", num);
  }

  int total_cols = 0;

  const struct NodeExpandoPrivate *priv = node->ndata;

  if (priv->color > -1)
    add_color(buf, priv->color);

  struct Buffer *tmp = buf_pool_get();
  const struct ExpandoFormat *fmt = node->format;
  if (fmt)
  {
    max_cols = MIN(max_cols, fmt->max_cols);
    int min_cols = MIN(max_cols, fmt->min_cols);
    total_cols += format_string(tmp, min_cols, max_cols, fmt->justification,
                                fmt->leader, buf_string(buf_expando),
                                buf_len(buf_expando), priv->has_tree);
    if (fmt->lower)
      buf_lower_special(tmp);
  }
  else
  {
    total_cols += format_string(tmp, 0, max_cols, JUSTIFY_LEFT, 0, buf_string(buf_expando),
                                buf_len(buf_expando), priv->has_tree);
  }
  buf_addstr(buf, buf_string(tmp));
  buf_pool_release(&tmp);

  if (priv->color > -1)
    add_color(buf, MT_COLOR_INDEX);

  buf_pool_release(&buf_expando);
  return total_cols;
}
