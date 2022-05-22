/**
 * @file
 * Test code for patterns
 *
 * @authors
 * Copyright (C) 2022 Pietro Cerutti <gahr@gahr.ch>
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
#define MAIN_C 1

#include "config.h"
#include "acutest.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "pattern/lib.h"
#include "test_common.h"

static struct ConfigDef Vars[] = {
  // clang-format off
  { "external_search_command", DT_STRING, IP "grep", 0, NULL },
  { NULL },
  // clang-format on
};

static void test_one_leak(const char *pattern)
{
  struct Buffer *err = mutt_buffer_pool_get();
  struct PatternList *p = mutt_pattern_comp(NULL, NULL, pattern, 0, err);
  mutt_pattern_free(&p);
  mutt_buffer_pool_release(&err);
}

void test_mutt_pattern_leak(void)
{
  MuttLogger = log_disp_null;
  NeoMutt = test_neomutt_create();
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars, 0));

  test_one_leak("~E ~F | ~D");
  test_one_leak("~D | ~E ~F");
  test_one_leak("~D | (~E ~F)");

  test_one_leak("~A");
  test_one_leak("~D");
  test_one_leak("~E");
  test_one_leak("~F");
  test_one_leak("~g");
  test_one_leak("~G");
  test_one_leak("~N");
  test_one_leak("~O");
  test_one_leak("~R");
  test_one_leak("~S");
  test_one_leak("~T");
  test_one_leak("~U");
  test_one_leak("~V");
  test_one_leak("~=");
  test_one_leak("~$");

  test_one_leak("~b EXPR");
  test_one_leak("~B EXPR");
  test_one_leak("~c EXPR");
  test_one_leak("~C EXPR");
  test_one_leak("~d <1d");
  test_one_leak("~d <1w");
  test_one_leak("~d <1m");
  test_one_leak("~d <1y");
  test_one_leak("~d <1H");
  test_one_leak("~d <1M");
  test_one_leak("~d <1S");
  test_one_leak("~d 01/01/2020-31/12/2023");
  test_one_leak("~d 31/12/2023-01/01/2020");
  test_one_leak("~d 20210309");
  test_one_leak("~d 01/01/2020+30d");
  test_one_leak("~d 01/01/2020*30d");
  test_one_leak("~e EXPR");
  test_one_leak("~f EXPR");
  test_one_leak("~h EXPR");
  test_one_leak("~H EXPR");
  test_one_leak("~i EXPR");
  test_one_leak("~I /dev/null");
  test_one_leak("~k");
  test_one_leak("~l");
  test_one_leak("~L EXPR");
  test_one_leak("~m 50-100");
  test_one_leak("~m -5,.");
  test_one_leak("~M EXPR");
  test_one_leak("~n 5-10");
  test_one_leak("~p");
  test_one_leak("~P");
  test_one_leak("~Q");
  test_one_leak("~r <7d");
  test_one_leak("~s EXPR");
  test_one_leak("~t EXPR");
  test_one_leak("~u");
  test_one_leak("~v");
  test_one_leak("~x EXPR");
  test_one_leak("~X >5");
  test_one_leak("~y EXPR");
  test_one_leak("~Y EXPR");
  test_one_leak("~z <10K");
  test_one_leak("~(~P)");
  test_one_leak("~<(~P)");
  test_one_leak("~>(~P)");

  // Bad Patterns
  test_one_leak("~d 00/01/2020");
  test_one_leak("~d 01/00/2020");
  test_one_leak("~d 20210009");
  test_one_leak("~d 20210300");

  test_neomutt_destroy(&NeoMutt);
}
