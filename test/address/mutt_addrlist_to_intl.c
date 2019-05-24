/**
 * @file
 * Test code for mutt_addrlist_to_intl()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"
#include "address/lib.h"
#include "common.h"

void test_mutt_addrlist_to_intl(void)
{
  // int mutt_addrlist_to_intl(struct AddressList *al, char **err);

  {
    char *err = NULL;
    TEST_CHECK(mutt_addrlist_to_intl(NULL, &err) == 0);
  }

  {
    struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
    TEST_CHECK(mutt_addrlist_to_intl(&al, NULL) == 0);
  }

  {
    struct
    {
      const char *local;
      const char *intl;
    } local2intl[] = { { .local = "test@äöüss.com", .intl = "test@xn--ss-uia6e4a.com" },
                       { .local = "test@nixieröhre.nixieclock-tube.com",
                         .intl = "test@xn--nixierhre-57a.nixieclock-tube.com" },
                       { .local = "test@வலைப்பூ.com", .intl = "test@xn--xlcawl2e7azb.com" } };

    C_Charset = "utf-8";
#ifdef HAVE_LIBIDN
    C_IdnEncode = true;
    C_IdnDecode = true;
#endif
    for (size_t i = 0; i < mutt_array_size(local2intl); ++i)
    {
      struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
      mutt_addrlist_append(&al, mutt_addr_create(NULL, local2intl[i].local));
      mutt_addrlist_to_intl(&al, NULL);
      struct Address *a = TAILQ_FIRST(&al);
#ifdef HAVE_LIBIDN
      TEST_CHECK_STR_EQ(local2intl[i].intl, a->mailbox);
#else
      TEST_CHECK_STR_EQ(local2intl[i].local, a->mailbox);
#endif
      mutt_addrlist_to_local(&al);
      TEST_CHECK_STR_EQ(local2intl[i].local, a->mailbox);
      mutt_addrlist_clear(&al);
    }
  }
}
