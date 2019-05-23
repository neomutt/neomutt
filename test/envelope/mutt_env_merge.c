/**
 * @file
 * Test code for mutt_env_merge()
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
#include "email/lib.h"

void test_mutt_env_merge(void)
{
  // void mutt_env_merge(struct Envelope *base, struct Envelope **extra);

  {
    struct Envelope envelope;
    memset(&envelope, 0, sizeof(struct Envelope));
    struct Envelope *envp = &envelope;

    mutt_env_merge(NULL, &envp);
    TEST_CHECK_(1, "mutt_env_merge(NULL, &envp)");
  }

  {
    struct Envelope base;
    memset(&base, 0, sizeof(struct Envelope));
    mutt_env_merge(&base, NULL);
    TEST_CHECK_(1, "mutt_env_merge(&base, NULL)");
  }

  {
    struct Envelope base;
    memset(&base, 0, sizeof(struct Envelope));
    struct Envelope *envp = NULL;
    mutt_env_merge(&base, &envp);
    TEST_CHECK_(1, "mutt_env_merge(&base, &envp)");
  }
}
