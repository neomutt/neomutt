/**
 * @file
 * Test code for sb_remove_mailbox()
 *
 * @authors
 * Copyright (C) 2026 Chris Andrew <cjhandrew@gmail.com>
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
#include "core/lib.h"
#include "sidebar/private.h"
#include "test_common.h"

/**
 * test_sb_remove_mailbox - CID 561613: hil_index left dangling when the
 * highlighted, last-in-array entry is deleted and no previous unhidden
 * entry exists to backtrack to.
 *
 * sb_remove_mailbox() removes the highlighted, last entry in the array.
 * The "last entry was deleted, so backtrack" branch calls sb_prev() but
 * didn't check its return value - if every remaining entry is hidden,
 * sb_prev() returns false and hil_index is left at its old value, which
 * is now one past the end of the (shrunk) array, instead of falling back
 * to -1 the way the sibling is_hidden branch a few lines below already
 * does in its own equivalent failure case.
 */
void test_sb_remove_mailbox(void)
{
  // void sb_remove_mailbox(struct SidebarWindowData *wdata, const struct Mailbox *m);

  struct Mailbox mailbox_a = { 0 };
  struct Mailbox mailbox_b = { 0 };

  struct SidebarWindowData wdata = { 0 };

  struct SbEntry *entry_a = MUTT_MEM_CALLOC(1, struct SbEntry);
  entry_a->mailbox = &mailbox_a;
  entry_a->is_hidden = true; // No unhidden entry left for sb_prev() to backtrack to
  ARRAY_ADD(&wdata.entries, entry_a);

  struct SbEntry *entry_b = MUTT_MEM_CALLOC(1, struct SbEntry);
  entry_b->mailbox = &mailbox_b;
  entry_b->is_hidden = false;
  ARRAY_ADD(&wdata.entries, entry_b);

  wdata.top_index = 0;
  wdata.bot_index = 1;
  wdata.opn_index = -1;
  wdata.hil_index = 1; // Highlighting the last entry, about to be removed

  sb_remove_mailbox(&wdata, &mailbox_b);

  TEST_CHECK_NUM_EQ(ARRAY_SIZE(&wdata.entries), 1);
  TEST_CHECK_NUM_EQ(wdata.hil_index, -1);

  struct SbEntry **sbep = NULL;
  ARRAY_FOREACH(sbep, &wdata.entries)
  {
    FREE(sbep);
  }
  ARRAY_FREE(&wdata.entries);
}
