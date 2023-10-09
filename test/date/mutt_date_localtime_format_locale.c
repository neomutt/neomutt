/**
 * @file
 * Test code for mutt_date_localtime_format_locale()
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "mutt/lib.h"
#include "core/lib.h"

void test_mutt_date_localtime_format_locale(void)
{
  // size_t mutt_date_localtime_format_locale(char *buf, size_t buflen, const char *format, time_t t, locale_t loc);

  putenv("TZ=UTC0");

  {
    char buf[64] = { 0 };
    mutt_date_localtime_format_locale(NULL, sizeof(buf), "%y", 1698521050,
                                      NeoMutt->time_c_locale);
    mutt_date_localtime_format_locale(buf, sizeof(buf), NULL, 1698521050,
                                      NeoMutt->time_c_locale);
  }

  {
    char buf[64] = { 0 };
    mutt_date_localtime_format_locale(buf, sizeof(buf), "%y", 1698521050,
                                      NeoMutt->time_c_locale);
  }
}
