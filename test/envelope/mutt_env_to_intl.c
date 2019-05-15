/**
 * @file
 * Test code for mutt_env_to_intl()
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"
#include "address/lib.h"
#include "email/lib.h"

void test_mutt_env_to_intl(void)
{
  // int mutt_env_to_intl(struct Envelope *env, const char **tag, char **err);

  {
    const char *tag = NULL;
    char *err = NULL;
    TEST_CHECK(mutt_env_to_intl(NULL, &tag, &err) == 1);
  }

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    char *err = NULL;
    TEST_CHECK(mutt_env_to_intl(&envelope, NULL, &err) == 0);
  }

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    const char *tag = NULL;
    TEST_CHECK(mutt_env_to_intl(&envelope, &tag, NULL) == 0);
  }
}
