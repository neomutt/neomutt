/**
 * @file
 * Test code for mutt_hash_free()
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

void test_mutt_hash_free(void)
{
  // void mutt_hash_free(struct Hash **ptr);

  {
    mutt_hash_free(NULL);
    TEST_CHECK_(1, "mutt_hash_free(NULL)");
  }

  {
    struct Hash *hash = NULL;
    mutt_hash_free(&hash);
    TEST_CHECK_(1, "mutt_hash_free(&hash)");
  }
}
