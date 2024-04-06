/**
 * @file
 * Test code for Padding Expandos
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
#include "expando/lib.h"
#include "common.h" // IWYU pragma: keep

void test_expando_padding(void)
{
  static const struct ExpandoDefinition FormatDef[] = {
    // clang-format off
    { "*", "padding-soft", ED_GLOBAL, ED_GLO_PADDING_SOFT, E_TYPE_STRING, node_padding_parse },
    { ">", "padding-hard", ED_GLOBAL, ED_GLO_PADDING_HARD, E_TYPE_STRING, node_padding_parse },
    { "|", "padding-eol",  ED_GLOBAL, ED_GLO_PADDING_EOL,  E_TYPE_STRING, node_padding_parse },
    { NULL, NULL, 0, -1, -1, NULL }
    // clang-format on
  };

  const char *input = "%|A %>B %*C";

  struct Buffer *err = buf_pool_get();
  struct Expando *exp = expando_parse(input, FormatDef, err);
  TEST_CHECK(exp != NULL);
  TEST_CHECK(buf_is_empty(err));

  check_node_padding(exp->node, "A", EPT_FILL_EOL);

  struct ExpandoNode *left = node_get_child(exp->node, ENP_LEFT);
  struct ExpandoNode *right = node_get_child(exp->node, ENP_RIGHT);

  TEST_CHECK(left == NULL);
  TEST_CHECK(right != NULL);

  check_node_test(node_get_child(right, 0), " ");
  check_node_padding(node_get_child(right, 1), "B", EPT_HARD_FILL);
  check_node_test(node_get_child(right, 2), " ");
  check_node_padding(node_get_child(right, 3), "C", EPT_SOFT_FILL);

  expando_free(&exp);
  buf_pool_release(&err);
}
