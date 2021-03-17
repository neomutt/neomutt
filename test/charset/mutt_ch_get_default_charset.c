/**
 * @file
 * Test code for mutt_ch_get_default_charset()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h"

static struct ConfigDef Vars[] = {
  // clang-format off
  { "assumed_charset", DT_STRING, NULL, 0, 0, NULL, },
  { NULL },
  // clang-format on
};

void test_mutt_ch_get_default_charset(void)
{
  // char *mutt_ch_get_default_charset(void);

  {
    NeoMutt = test_neomutt_create();
    TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars, DT_NO_VARIABLE));

    char *cs = mutt_ch_get_default_charset();
    TEST_CHECK(strlen(cs) != 0);

    test_neomutt_destroy(&NeoMutt);
  }
}
