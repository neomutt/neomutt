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
#include "definition.h"
#include "helpers.h"
#include "node.h"
#include "parse.h"
#include "render.h"

/**
 * node_condbool_new - Create a new CondBool ExpandoNode
 * @param start Start of string to store
 * @param end   End of string to store
 * @param did   Domain ID
 * @param uid   Unique ID
 * @retval ptr New CondBool ExpandoNode
 */
struct ExpandoNode *node_condbool_new(const char *start, const char *end, int did, int uid)
{
  struct ExpandoNode *node = node_new();

  node->type = ENT_CONDBOOL;
  node->start = start;
  node->end = end;

  node->did = did;
  node->uid = uid;
  node->render = node_condbool_render;

  return node;
}

/**
 * node_condbool_parse - Parse a CondBool format string - Implements ExpandoDefinition::parse() - @ingroup expando_parse_api
 */
struct ExpandoNode *node_condbool_parse(const char *str, const char **parsed_until,
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

  while (definition && definition->short_name)
  {
    if (mutt_str_equal(definition->short_name, expando))
    {
      if (definition->parse)
      {
        return definition->parse(str, parsed_until, definition->did,
                                 definition->uid, flags, error);
      }
      else
      {
        *parsed_until = expando_end;
        return node_condbool_new(format_end, expando_end, definition->did,
                                 definition->uid);
      }
    }

    definition++;
  }

  error->position = format_end;
  // L10N: e.g. "Unknown expando: %Q"
  snprintf(error->message, sizeof(error->message), _("Unknown expando: %%%.*s"),
           expando_len, format_end);
  return NULL;
}

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
    rd_match->get_string(node, data, flags, max_cols, buf_str);
    const size_t len = buf_len(buf_str);
    buf_pool_release(&buf_str);

    return (len > 0); // bool-ify
  }

  return 0;
}
