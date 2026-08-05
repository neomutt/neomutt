/**
 * @file
 * Test code for mutt_adv_mktemp_cfg()
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "config/lib.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "muttlib.h"
#include "test_common.h" // IWYU pragma: keep

void test_mutt_adv_mktemp_cfg(void)
{
  // void mutt_adv_mktemp_cfg(struct Buffer *buf, const char *cfg);

  const char *tmp_dir = cs_subset_path(NeoMutt->sub, "tmp_dir");
  TEST_CHECK(tmp_dir != NULL);

  struct Buffer *existing = buf_pool_get();
  buf_printf(existing, "%s/%s", tmp_dir, "report.pdf");

  FILE *fp = mutt_file_fopen(buf_string(existing), "w");
  TEST_CHECK(fp != NULL);
  if (fp)
    fclose(fp);

  struct Buffer *candidate = buf_pool_get();
  buf_strcpy(candidate, "report.pdf");
  mutt_adv_mktemp_cfg(candidate, "tmp_dir");

  TEST_CHECK(!mutt_str_equal(buf_string(candidate), buf_string(existing)));
  TEST_CHECK(strncmp(buf_string(candidate), tmp_dir, mutt_str_len(tmp_dir)) == 0);
  TEST_CHECK(mutt_str_equal(strrchr(buf_string(candidate), '.'), ".pdf"));

  unlink(buf_string(existing));
  unlink(buf_string(candidate));
  buf_pool_release(&candidate);
  buf_pool_release(&existing);
}
