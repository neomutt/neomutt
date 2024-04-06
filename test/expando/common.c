/**
 * @file
 * Shared test code for Expandos
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "common.h"

void check_node_test(struct ExpandoNode *node, const char *text)
{
  TEST_CHECK(node != NULL);
  TEST_CHECK(node->type == ENT_TEXT);

  const size_t n = mutt_str_len(text);
  const size_t m = (node->end - node->start);
  TEST_CHECK(n == m);
  TEST_CHECK(mutt_strn_equal(node->start, text, n));
}

void check_node_expando(struct ExpandoNode *node, const char *expando,
                        const struct ExpandoFormat *fmt_expected)
{
  TEST_CHECK(node != NULL);
  TEST_CHECK(node->type == ENT_EXPANDO);
  TEST_CHECK(node->ndata != NULL);

  const size_t n = mutt_str_len(expando);
  const size_t m = node->end - node->start;

  TEST_CHECK(n == m);
  TEST_CHECK(mutt_strn_equal(node->start, expando, n));

  struct ExpandoFormat *fmt = node->format;

  if (fmt_expected == NULL)
  {
    TEST_CHECK(fmt == NULL);
  }
  else
  {
    TEST_CHECK(fmt != NULL);
    TEST_CHECK(fmt->justification == fmt_expected->justification);
    TEST_CHECK(fmt->leader == fmt_expected->leader);
    TEST_CHECK(fmt->min_cols == fmt_expected->min_cols);
    TEST_CHECK(fmt->max_cols == fmt_expected->max_cols);
  }
}

void check_node_padding(struct ExpandoNode *node, const char *pad_char, enum ExpandoPadType pad_type)
{
  TEST_CHECK(node != NULL);
  TEST_CHECK(node->type == ENT_PADDING);

  const size_t n = mutt_str_len(pad_char);
  const size_t m = node->end - node->start;

  TEST_CHECK(n == m);
  TEST_CHECK(mutt_strn_equal(node->start, pad_char, n));

  TEST_CHECK(node->ndata != NULL);
  struct NodePaddingPrivate *priv = node->ndata;
  TEST_CHECK(priv->pad_type == pad_type);
}

void check_node_cond(struct ExpandoNode *node)
{
  TEST_CHECK(node != NULL);
  TEST_CHECK(node->type == ENT_CONDITION);
}

void check_node_condbool(struct ExpandoNode *node, const char *expando)
{
  TEST_CHECK(node != NULL);
  TEST_CHECK(node->type == ENT_CONDBOOL);
  TEST_CHECK(node->ndata == NULL);

  const size_t n = mutt_str_len(expando);
  const size_t m = node->end - node->start;

  TEST_CHECK(n == m);
  TEST_CHECK(mutt_strn_equal(node->start, expando, n));

  struct ExpandoFormat *fmt = node->format;
  TEST_CHECK(fmt == NULL);
}

void check_node_conddate(struct ExpandoNode *node, int count, char period)
{
  TEST_CHECK(node != NULL);
  TEST_CHECK(node->type == ENT_CONDDATE);

  TEST_CHECK(node->ndata != NULL);
  struct NodeCondDatePrivate *priv = node->ndata;

  TEST_CHECK(priv->count == count);
  TEST_CHECK(priv->period == period);
}

struct ExpandoNode *parse_date(const char *s, const char **parsed_until,
                               int did, int uid, ExpandoParserFlags flags,
                               struct ExpandoParseError *error)
{
  // s-1 is always something valid
  if (*(s - 1) == '<')
  {
    return node_conddate_parse(s + 1, parsed_until, did, uid, error);
  }

  return node_expando_parse_enclosure(s, parsed_until, did, uid, ']', error);
}
