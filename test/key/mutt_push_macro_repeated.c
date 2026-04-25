/**
 * @file
 * Test code for mutt_push_macro_repeated()
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
#include <stddef.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "key/lib.h"
#include "key/module_data.h"
#include "test_common.h"

void test_mutt_push_macro_repeated(void)
{
  // void mutt_push_macro_repeated(char *macro, int count);

  struct KeyModuleData *mod_data = neomutt_get_module_data(NeoMutt, MODULE_ID_KEY);

  // Make sure we start with an empty queue
  mutt_flushinp();
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 0);

  // Bare push: count == 0 should expand once
  char macro1[] = "abc";
  mutt_push_macro_repeated(macro1, 0);
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 3);
  mutt_flushinp();

  // count == 1 should also expand exactly once
  mutt_push_macro_repeated(macro1, 1);
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 3);
  mutt_flushinp();

  // count == 5 should expand 5 times -> 15 events
  mutt_push_macro_repeated(macro1, 5);
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 15);
  mutt_flushinp();

  // Negative count is treated as a single expansion
  mutt_push_macro_repeated(macro1, -3);
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 3);
  mutt_flushinp();

  // Counts above the cap are clamped to MacroRepeatMax (== 1000)
  char macro2[] = "x";
  mutt_push_macro_repeated(macro2, 9999);
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 1000);
  mutt_flushinp();

  // Verify ordering: a 3-char macro expanded twice should pop in macro order, twice
  mutt_push_macro_repeated(macro1, 2);
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 6);

  const char expected[] = "abcabc";
  for (int i = 0; i < 6; i++)
  {
    struct KeyEvent *ev = array_pop(&mod_data->macro_events);
    TEST_CHECK(ev != NULL);
    TEST_CHECK_NUM_EQ(ev->ch, (unsigned char) expected[i]);
  }
  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&mod_data->macro_events), 0);
}
