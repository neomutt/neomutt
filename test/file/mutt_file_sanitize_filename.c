/**
 * @file
 * Test code for mutt_file_sanitize_filename()
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
#include <locale.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_mutt_file_sanitize_filename(void)
{
  // void mutt_file_sanitize_filename(char *fp, bool slash);

  {
    mutt_file_sanitize_filename(NULL, false);
    TEST_CHECK_(1, "mutt_file_sanitize_filename(NULL, false)");
  }

  {
    setlocale(LC_CTYPE, "C.UTF-8");
    char buf[] = "żupan/tłusty";
    mutt_file_sanitize_filename(buf, false);
    TEST_CHECK_STR_EQ(buf, "żupan/tłusty");
  }

  {
    setlocale(LC_CTYPE, "C.UTF-8");
    char buf[] = "żupan/t\xC5\xC5ust\xC5";
    mutt_file_sanitize_filename(buf, false);
    TEST_CHECK_STR_EQ(buf, "żupan/t__ust_");
  }
}
