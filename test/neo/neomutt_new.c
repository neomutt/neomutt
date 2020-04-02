/**
 * @file
 * Test code for neomutt_new()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

static short VarApple;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",  DT_NUMBER,  &VarApple,  42, 0, NULL },
  { NULL },
};
// clang-format on

void test_neomutt_new(void)
{
  // struct NeoMutt *neomutt_new(struct ConfigSet *cs);

  {
    NeoMutt = neomutt_new(NULL);
    TEST_CHECK(NeoMutt == NULL);
  }

  {
    struct ConfigSet *cs = cs_new(30);
    number_init(cs);
    TEST_CHECK(cs_register_variables(cs, Vars, 0));

    NeoMutt = neomutt_new(cs);
    TEST_CHECK(NeoMutt != NULL);

    neomutt_free(&NeoMutt);
    cs_free(&cs);
  }
}
