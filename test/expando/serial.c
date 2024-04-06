/**
 * @file
 * Dump the details of an Expando Tree
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "debug/lib.h"
#include "expando/lib.h"

static void dump_node(const struct ExpandoNode *node, struct Buffer *buf);

static void dump_did_uid(const struct ExpandoNode *node, struct Buffer *buf)
{
  const char *did = name_expando_domain(node->did);
  const char *uid = name_expando_uid(node->did, node->uid);
  buf_add_printf(buf, "(%s,%s)", did + 3, uid + 7);
}

static void dump_node_condition(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<COND");

  // ASSERT(node->start);
  // ASSERT(node->end);
  // int len = node->end - node->start;
  // buf_add_printf(buf, ":'%.*s':", len, node->start);

  // dump_did_uid(node, buf);

  // These shouldn't happen
  if (node->start)
    buf_add_printf(buf, ",start=%p", node->start);
  if (node->end)
    buf_add_printf(buf, ",end=%p", node->end);

  struct ExpandoNode *condition = node_get_child(node, ENC_CONDITION);
  struct ExpandoNode *if_true_tree = node_get_child(node, ENC_TRUE);
  struct ExpandoNode *if_false_tree = node_get_child(node, ENC_FALSE);

  ASSERT(condition);
  buf_addstr(buf, ":");
  dump_node(condition, buf);
  buf_addstr(buf, "|");
  dump_node(if_true_tree, buf);
  buf_addstr(buf, "|");
  dump_node(if_false_tree, buf);

  buf_addstr(buf, ">");
}

static void dump_node_condbool(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<BOOL");

  ASSERT(node->start);
  ASSERT(node->end);
  int len = node->end - node->start;
  buf_add_printf(buf, ":'%.*s':", len, node->start);

  dump_did_uid(node, buf);

  ASSERT(node->ndata == NULL);
  ASSERT(node->ndata_free == NULL);

  buf_addstr(buf, ">");
}

static void dump_node_conddate(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<DATE:");

  // ASSERT(node->start);
  // ASSERT(node->end);
  // int len = node->end - node->start;
  // buf_add_printf(buf, ":'%.*s':", len, node->start);

  dump_did_uid(node, buf);

  ASSERT(node->ndata);
  ASSERT(node->ndata_free);
  struct NodeCondDatePrivate *priv = node->ndata;
  buf_add_printf(buf, ":%d:%c", priv->count, priv->period);

  // These shouldn't happen
  if (node->start)
    buf_add_printf(buf, ",start=%p", node->start);
  if (node->end)
    buf_add_printf(buf, ",end=%p", node->end);

  buf_addstr(buf, ">");
}

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

static void dump_node_empty(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<EMPTY");

  // These shouldn't happen
  if (node->did != 0)
    buf_add_printf(buf, ",did=%d", node->did);
  if (node->uid != 0)
    buf_add_printf(buf, ",uid=%d", node->uid);
  if (node->start)
    buf_add_printf(buf, ",start=%p", node->start);
  if (node->end)
    buf_add_printf(buf, ",end=%p", node->end);
  if (node->ndata)
    buf_add_printf(buf, ",ndata=%p", node->ndata);
  if (node->ndata_free)
    buf_add_printf(buf, ",ndata_free=%p", (void *) (intptr_t) node->ndata_free);

  buf_addstr(buf, ">");
}

static void dump_node_expando(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<EXP:");

  ASSERT(node->start);
  ASSERT(node->end);
  int len = node->end - node->start;
  buf_add_printf(buf, "'%.*s'", len, node->start);

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

  // These shouldn't happen
  // if (node->ndata)
  //   buf_add_printf(buf, ",ndata=%p", node->ndata);
  // if (node->ndata_free)
  //   buf_add_printf(buf, ",ndata_free=%p", (void *) (intptr_t) node->ndata_free);

  buf_addstr(buf, ">");
}

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

  ASSERT(node->start);
  ASSERT(node->end);
  int len = node->end - node->start;
  if (len > 0)
    buf_add_printf(buf, "'%.*s'", len, node->start);

  buf_addstr(buf, ":");
  dump_node(left, buf);
  buf_addstr(buf, "|");
  dump_node(right, buf);

  // buf_add_printf(buf, ":(%s,%zu)", priv->buffer_start, priv->buffer_len);

  // These shouldn't happen
  // if (node->did != 0)
  //   buf_add_printf(buf, ",did=%d", node->did);
  // if (node->uid != 0)
  //   buf_add_printf(buf, ",uid=%d", node->uid);

  buf_addstr(buf, ">");
}

static void dump_node_text(const struct ExpandoNode *node, struct Buffer *buf)
{
  buf_addstr(buf, "<TEXT:");

  ASSERT(node->start);
  ASSERT(node->end);
  int len = node->end - node->start;
  buf_add_printf(buf, "'%.*s'", len, node->start);

  // These shouldn't happen
  // if (node->did != 0)
  //   buf_add_printf(buf, ",did=%d", node->did);
  // if (node->uid != 0)
  //   buf_add_printf(buf, ",uid=%d", node->uid);
  if (node->ndata)
    buf_add_printf(buf, ",ndata=%p", node->ndata);
  if (node->ndata_free)
    buf_add_printf(buf, ",ndata_free=%p", (void *) (intptr_t) node->ndata_free);

  buf_addstr(buf, ">");
}

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

void expando_serialise(const struct Expando *exp, struct Buffer *buf)
{
  if (!exp || !buf)
    return;

  dump_node(exp->node, buf);
}
