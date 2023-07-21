/**
 * @file
 * Test code for Expando Debugging
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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
#include <assert.h>
#include <stdio.h>
#include "debug_print.h"
#include "expando/lib.h"
#include "mutt_thread.h"

static void print_node(FILE *fp, const struct ExpandoNode *node, int indent);
static void expando_tree_fprint_rec(FILE *fp, struct ExpandoNode **root, int indent);

#define EXPANDO_DEBUG_PRINT_INDENT 4

static void print_empty_node(FILE *fp, const struct ExpandoNode *node, int indent)
{
  fprintf(fp, "%*sEMPTY\n", indent, "");
}

static void print_text_node(FILE *fp, const struct ExpandoNode *node, int indent)
{
  const int len = node->end - node->start;
  fprintf(fp, "%*sTEXT: `%.*s`\n", indent, "", len, node->start);
}

static void print_expando_node(FILE *fp, const struct ExpandoNode *node, int indent)
{
  const struct ExpandoFormat *fmt = node->format;
  if (fmt)
  {
    const int elen = node->end - node->start;
    fprintf(fp, "%*sEXPANDO: `%.*s`", indent, "", elen, node->start);

    const char *just = NULL;

    switch (fmt->justification)
    {
      case JUSTIFY_LEFT:
        just = "LEFT";
        break;

      case JUSTIFY_CENTER:
        just = "CENTER";
        break;

      case JUSTIFY_RIGHT:
        just = "RIGHT";
        break;

      default:
        assert(0 && "Unknown justification.");
    }
    fprintf(fp, " (did=%d, uid=%d) (min=%d, max=%d, just=%s, leader=`%c`)\n",
            node->did, node->uid, fmt->min_cols, fmt->max_cols, just, fmt->leader);
  }
  else
  {
    const int len = node->end - node->start;
    fprintf(fp, "%*sEXPANDO: `%.*s` (did=%d, uid=%d)\n", indent, "", len,
            node->start, node->did, node->uid);
  }
}

static void print_pad_node(FILE *fp, const struct ExpandoNode *node, int indent)
{
  assert(node->ndata);
  struct NodePaddingPrivate *priv = node->ndata;

  const char *pt = NULL;
  switch (priv->pad_type)
  {
    case EPT_FILL_EOL:
      pt = "FILL_EOL";
      break;

    case EPT_HARD_FILL:
      pt = "HARD_FILL";
      break;

    case EPT_SOFT_FILL:
      pt = "SOFT_FILL";
      break;

    default:
      assert(0 && "Unknown pad type.");
  }

  const int len = node->end - node->start;
  fprintf(fp, "%*sPAD: `%.*s` (type=%s)\n", indent, "", len, node->start, pt);
}

static void print_condition_node(FILE *fp, const struct ExpandoNode *node, int indent)
{
  struct ExpandoNode *condition = node_get_child(node, ENC_CONDITION);
  struct ExpandoNode *if_true_tree = node_get_child(node, ENC_TRUE);
  struct ExpandoNode *if_false_tree = node_get_child(node, ENC_FALSE);

  fprintf(fp, "%*sCONDITION:\n", indent, "");
  print_node(fp, condition, indent + 2 * EXPANDO_DEBUG_PRINT_INDENT);
  fprintf(fp, "%*sIF TRUE :\n", indent + EXPANDO_DEBUG_PRINT_INDENT, "");
  expando_tree_fprint_rec(fp, &if_true_tree, indent + 2 * EXPANDO_DEBUG_PRINT_INDENT);

  if (if_false_tree)
  {
    fprintf(fp, "%*sIF FALSE:\n", indent + EXPANDO_DEBUG_PRINT_INDENT, "");
    expando_tree_fprint_rec(fp, &if_false_tree, indent + 2 * EXPANDO_DEBUG_PRINT_INDENT);
  }
}

static void print_conditional_date_node(FILE *fp, const struct ExpandoNode *node, int indent)
{
  assert(node->ndata);
  struct NodeCondDatePrivate *priv = node->ndata;
  fprintf(fp, "%*sCOND DATE: (did=%d, uid=%d)(period=`%c`, count=%d)\n", indent,
          "", node->did, node->uid, priv->period, priv->count);
}

static void print_node(FILE *fp, const struct ExpandoNode *node, int indent)
{
  if (!node)
  {
    fprintf(fp, "<null>\n");
    return;
  }

  switch (node->type)
  {
    case ENT_EMPTY:
    {
      print_empty_node(fp, node, indent);
    }
    break;

    case ENT_TEXT:
    {
      print_text_node(fp, node, indent);
    }
    break;

    case ENT_EXPANDO:
    {
      print_expando_node(fp, node, indent);
    }
    break;

    case ENT_PADDING:
    {
      print_pad_node(fp, node, indent);
    }
    break;

    case ENT_CONDITION:
    {
      print_condition_node(fp, node, indent);
    }
    break;

    case ENT_CONDDATE:
    {
      print_conditional_date_node(fp, node, indent);
    }
    break;

    default:
      assert(0 && "Unknown node.");
  }
}

void expando_tree_fprint_rec(FILE *fp, struct ExpandoNode **root, int indent)
{
  const struct ExpandoNode *node = *root;
  while (node)
  {
    print_node(fp, node, indent);
    node = node->next;
  }
}

void expando_tree_print(struct ExpandoNode **root)
{
  expando_tree_fprint_rec(stdout, root, 0);
}

void expando_print_color_string(const char *s)
{
  while (*s)
  {
    if (*s == MUTT_SPECIAL_INDEX)
    {
      printf("SPEC %d ", *(s + 1));
      s += 2;
    }
    else
    {
      printf("'%c' ", *s);
      s++;
    }
  }
  printf("\n");
}
