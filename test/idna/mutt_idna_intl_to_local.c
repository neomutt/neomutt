/**
 * @file
 * Test code for mutt_idna_intl_to_local()
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
#include <stddef.h>
#include <stdbool.h>
#include "address/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "test_common.h"

#ifdef HAVE_LIBIDN
static struct ConfigDef Vars[] = {
  // clang-format off
  { "idn_decode", DT_BOOL, true, 0, NULL, },
  { "idn_encode", DT_BOOL, true, 0, NULL, },
  { NULL },
  // clang-format on
};
#endif

void test_mutt_idna_intl_to_local(void)
{
  // char *mutt_idna_intl_to_local(const char *user, const char *domain, uint8_t flags);

#ifdef HAVE_LIBIDN
  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars, DT_NO_FLAGS));

  {
    const char *addr = mutt_idna_intl_to_local(NULL, "banana", MI_NO_FLAGS);
    TEST_CHECK(addr != NULL);
    FREE(&addr);
  }

  {
    const char *addr = mutt_idna_intl_to_local("apple", NULL, MI_NO_FLAGS);
    TEST_CHECK(addr != NULL);
    FREE(&addr);
  }
#endif
}
