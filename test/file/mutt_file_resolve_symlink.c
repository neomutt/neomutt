/**
 * @file
 * Test code for mutt_file_resolve_symlink()
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
#include "mutt/lib.h"

void test_mutt_file_resolve_symlink(void)
{
  // void mutt_file_resolve_symlink(struct Buffer *buf);

  {
    struct Buffer file = mutt_buffer_make(0);
    mutt_file_resolve_symlink(&file);
    TEST_CHECK_(1, "mutt_file_resolve_symlink(&file)");
    mutt_buffer_dealloc(&file);
  }
}
