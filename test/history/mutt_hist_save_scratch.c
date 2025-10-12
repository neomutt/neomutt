/**
 * @file
 * Test code for mutt_hist_save_scratch()
 *
 * @authors
 * Copyright (C) 2019-2021 Richard Russon <rich@flatcap.org>
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
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"
#include "history/lib.h"
#include "test_common.h" // IWYU pragma: keep

extern struct ConfigDef HistoryVars[];

void test_mutt_hist_save_scratch(void)
{
  // void mutt_hist_save_scratch(enum HistoryClass hclass, const char *str);

  cs_register_variables(NeoMutt->sub->cs, HistoryVars);

  {
    mutt_hist_save_scratch(0, NULL);
    TEST_CHECK_(1, "mutt_hist_save_scratch(0, NULL)");
  }
}
