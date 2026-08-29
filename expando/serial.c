/**
 * @file
 * Dump the details of an Expando Tree
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page expando_serial Dump the details of an Expando Tree
 *
 * Dump the details of an Expando Tree
 */

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "debug/lib.h"
#include "serial.h"
#include "definition.h"
#include "expando.h"
#include "node.h"
#include "node_conddate.h"
#include "node_condition.h"
#include "node_padding.h"

static void dump_node(const struct ExpandoNode *node, struct Buffer *buf);

/**
 * dump_did_uid - Serialise the Domain ID and UID of an Expando Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_did_uid(const struct ExpandoNode *node, struct Buffer *buf)
{
  const char *did = name_expando_domain(node->did);
  const char *uid = name_expando_uid(node->did, node->uid);
  buf_add_printf(buf, "(%s,%s)", did, uid);
}

/**
 * dump_node_condition - Serialise a Condition Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_condition(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<COND");

  // These shouldn't happen
  if (node->text)
    buf_add_printf(buf, ",text=%s", node->text);

  struct ExpandoNode *node_cond = node_get_child(node, ENC_CONDITION);
  struct ExpandoNode *node_true = node_get_child(node, ENC_TRUE);
  struct ExpandoNode *node_false = node_get_child(node, ENC_FALSE);

  ASSERT(node_cond);
  buf_addstr(buf, ":");
  dump_node(node_cond, buf);
  buf_addstr(buf, "|");
  dump_node(node_true, buf);
  buf_addstr(buf, "|");
  dump_node(node_false, buf);

  const struct ExpandoFormat *fmt = node->format;
  if (fmt)
  {
    const char *just = name_format_justify(fmt->justification);
    if (fmt->max_cols == INT_MAX)
      buf_add_printf(buf, ":{%d,MAX,%s,'%c'}", fmt->min_cols, just + 8, fmt->leader);
    else
      buf_add_printf(buf, ":{%d,%d,%s,'%c'}", fmt->min_cols, fmt->max_cols,
                     just + 8, fmt->leader);
  }

  buf_addstr(buf, ">");
}

/**
 * dump_node_condbool - Serialise a Conditional Bool Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_condbool(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<BOOL");
  dump_did_uid(node, buf);

  ASSERT(node->ndata != NULL);
  ASSERT(node->ndata_free != NULL);

  buf_addstr(buf, ">");
}

/**
 * dump_node_conddate - Serialise a Conditional Date Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_conddate(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<DATE:");

  dump_did_uid(node, buf);

  ASSERT(node->ndata);
  ASSERT(node->ndata_free);
  struct NodeCondDatePrivate *priv = node->ndata;
  buf_add_printf(buf, ":%d:%c", priv->count, priv->period);

  // These shouldn't happen
  if (node->text)
    buf_add_printf(buf, ",text=%s", node->text);

  buf_addstr(buf, ">");
}

/**
 * dump_node_container - Serialise a Container Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_container(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<CONT:");

  struct ExpandoNode **np = NULL;
  ARRAY_FOREACH(np, &node->children)
  {
    if (!np || !*np)
      continue;

    dump_node(*np, buf);
  }

  buf_addstr(buf, ">");
}

/**
 * dump_node_empty - Serialise an Empty Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_empty(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<EMPTY");

  // These shouldn't happen
  if (node->did != 0)
    buf_add_printf(buf, ",did=%d", node->did);
  if (node->uid != 0)
    buf_add_printf(buf, ",uid=%d", node->uid);
  if (node->text)
    buf_add_printf(buf, ",text=%s", node->text);
  if (node->ndata)
    buf_add_printf(buf, ",ndata=%p", node->ndata);
  if (node->ndata_free)
    buf_add_printf(buf, ",ndata_free=%p", (void *) (intptr_t) node->ndata_free);

  buf_addstr(buf, ">");
}

/**
 * dump_node_expando - Serialise an Expando Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_expando(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<EXP:");

  if (node->text)
    buf_add_printf(buf, "'%s'", node->text);

  ASSERT(node->did != 0);
  ASSERT(node->uid != 0);
  dump_did_uid(node, buf);

  ASSERT(node->ndata);
  ASSERT(node->ndata_free);

  const struct ExpandoFormat *fmt = node->format;
  if (fmt)
  {
    const char *just = name_format_justify(fmt->justification);
    if (fmt->max_cols == INT_MAX)
      buf_add_printf(buf, ":{%d,MAX,%s,'%c'}", fmt->min_cols, just + 8, fmt->leader);
    else
      buf_add_printf(buf, ":{%d,%d,%s,'%c'}", fmt->min_cols, fmt->max_cols,
                     just + 8, fmt->leader);
  }

  buf_addstr(buf, ">");
}

/**
 * dump_node_padding - Serialise a Padding Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_padding(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<PAD:");

  ASSERT(node->ndata);
  ASSERT(node->ndata_free);
  struct NodePaddingPrivate *priv = node->ndata;

  struct ExpandoNode *left = node_get_child(node, ENP_LEFT);
  struct ExpandoNode *right = node_get_child(node, ENP_RIGHT);

  const char *pt = name_expando_pad_type(priv->pad_type);
  pt += 4; // Skip "EPT_" prefix
  buf_add_printf(buf, "%s:", pt);

  ASSERT(node->text);
  buf_add_printf(buf, "'%s'", node->text);

  buf_addstr(buf, ":");
  dump_node(left, buf);
  buf_addstr(buf, "|");
  dump_node(right, buf);

  buf_addstr(buf, ">");
}

/**
 * dump_node_text - Serialise a Text Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node_text(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<TEXT:");

  ASSERT(node->text);
  buf_add_printf(buf, "'%s'", node->text);

  // These shouldn't happen
  if (node->ndata)
    buf_add_printf(buf, ",ndata=%p", node->ndata);
  if (node->ndata_free)
    buf_add_printf(buf, ",ndata_free=%p", (void *) (intptr_t) node->ndata_free);

  buf_addstr(buf, ">");
}

/**
 * dump_node - Serialise an Expando Node
 * @param node Expando Node
 * @param buf  Buffer for the result
 */
static void dump_node(const struct ExpandoNode *node, struct Buffer *buf)
{
  if (!node || !buf)
    return;

  switch (node->type)
  {
    case ENT_CONDITION:
      dump_node_condition(node, buf);
      break;
    case ENT_CONDBOOL:
      dump_node_condbool(node, buf);
      break;
    case ENT_CONDDATE:
      dump_node_conddate(node, buf);
      break;
    case ENT_CONTAINER:
      dump_node_container(node, buf);
      break;
    case ENT_EMPTY:
      dump_node_empty(node, buf);
      break;
    case ENT_EXPANDO:
      dump_node_expando(node, buf);
      break;
    case ENT_PADDING:
      dump_node_padding(node, buf);
      break;
    case ENT_TEXT:
      dump_node_text(node, buf);
      break;
    default:
      ASSERT(false);
  }
}

/**
 * expando_serialise - Serialise an Expando into a string
 * @param exp Expando
 * @param buf Buffer for the result
 */
void expando_serialise(const struct Expando *exp, struct Buffer *buf)
{
  if (!exp || !buf)
    return;

  dump_node(exp->node, buf);
}

/**
 * find_long_name - Find an Expando definition by its long name
 * @param str  String beginning with a long name
 * @param defs Expando definitions
 * @retval ptr Matching definition
 * @retval NULL No match
 */
static const struct ExpandoDefinition *find_long_name(const char *str,
                                                      const struct ExpandoDefinition *defs)
{
  for (const struct ExpandoDefinition *def = defs;
       def && (def->short_name || def->long_name); def++)
  {
    if (!def->long_name)
      continue;

    const size_t len = mutt_str_len(def->long_name);
    if (mutt_strn_equal(str, def->long_name, len) && (str[len] == '}'))
      return def;
  }

  return NULL;
}

/**
 * find_short_name - Find an Expando definition by its short name
 * @param str  String beginning with a short name
 * @param defs Expando definitions
 * @retval ptr Matching definition
 * @retval NULL No match
 */
static const struct ExpandoDefinition *find_short_name(const char *str,
                                                       const struct ExpandoDefinition *defs)
{
  for (const struct ExpandoDefinition *def = defs;
       def && (def->short_name || def->long_name); def++)
  {
    const size_t len = mutt_str_len(def->short_name);
    if ((len > 0) && mutt_strn_equal(str, def->short_name, len))
      return def;
  }

  return NULL;
}

/**
 * add_expando_name - Add an Expando name to a buffer
 * @param buf   Buffer for the result
 * @param def   Expando definition
 * @param named Prefer the named form
 */
static void add_expando_name(struct Buffer *buf, const struct ExpandoDefinition *def, bool named)
{
  if (named && def->long_name)
    buf_add_printf(buf, "{%s}", def->long_name);
  else if (def->short_name)
    buf_addstr(buf, def->short_name);
  else
    buf_add_printf(buf, "{%s}", def->long_name);
}

/**
 * skip_format - Skip printf-style formatting after a percent sign
 * @param str String to examine
 * @retval ptr First character of the Expando name
 */
static const char *skip_format(const char *str)
{
  if ((*str == '-') || (*str == '='))
    str++;
  if (*str == '0')
    str++;
  while (mutt_isdigit(*str))
    str++;
  if (*str == '.')
  {
    str++;
    while (mutt_isdigit(*str))
      str++;
  }
  if (*str == '_')
    str++;

  return str;
}

/**
 * find_enclosure_end - Find an unescaped enclosure terminator
 * @param str  String to examine
 * @param close Terminator character
 * @retval ptr Terminator
 * @retval NULL Terminator not found
 */
static const char *find_enclosure_end(const char *str, char close)
{
  while (*str)
  {
    if ((str[0] == '\\') && str[1])
    {
      str += 2;
      continue;
    }
    if (*str == close)
      return str;
    str++;
  }

  return NULL;
}

/**
 * expando_stringify - Write an Expando using short or named Expando names
 * @param exp   Expando to stringify
 * @param defs  Expando definitions
 * @param named Prefer named Expandos
 * @param buf   Buffer for the result
 *
 * If the preferred name isn't defined, use the other name.  Everything except
 * Expando names is copied verbatim, preserving custom Expando payloads.
 */
void expando_stringify(const struct Expando *exp, const struct ExpandoDefinition *defs,
                       bool named, struct Buffer *buf)
{
  if (!exp || !defs || !buf)
    return;

  const char *str = exp->string;
  while (*str)
  {
    if ((str[0] == '\\') && str[1])
    {
      buf_addstr_n(buf, str, 2);
      str += 2;
      continue;
    }

    if ((str[0] != '%') || (str[1] == '%'))
    {
      const size_t len = ((str[0] == '%') ? 2 : 1);
      buf_addstr_n(buf, str, len);
      str += len;
      continue;
    }

    const char *name = skip_format(str + 1);
    const bool conditional = ((*name == '<') || (*name == '?'));
    if (conditional)
      name++;

    const struct ExpandoDefinition *def = NULL;
    size_t name_len = 0;
    if (*name == '{')
    {
      def = find_long_name(name + 1, defs);
      if (def)
        name_len = mutt_str_len(def->long_name) + 2;
    }
    if (!def)
    {
      def = find_short_name(name, defs);
      if (def)
        name_len = mutt_str_len(def->short_name);
    }

    if (!def)
    {
      buf_addch(buf, *str++);
      continue;
    }

    buf_addstr_n(buf, str, name - str);
    add_expando_name(buf, def, named);
    str = name + name_len;

    // Custom enclosure payloads may contain strftime percent sequences.
    // Their short names are the opening delimiter; copy through the close.
    if (!conditional && def->parse && def->short_name && strchr("([{@", def->short_name[0]))
    {
      char close = def->short_name[0];
      if (close == '(')
        close = ')';
      else if (close == '[')
        close = ']';
      else if (close == '{')
        close = '}';

      const char *end = find_enclosure_end(str, close);
      if (end)
      {
        buf_addstr_n(buf, str, end - str + 1);
        str = end + 1;
      }
    }
  }
}
