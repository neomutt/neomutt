/**
 * @file
 * MMDF Mailbox test harness
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

/**
 * @page harness_mmdf MMDF Mailbox test harness
 *
 * MMDF Mailbox test harness
 */

#include "config.h"
#include "core/lib.h"
#include "common.h"

int main(int argc, char *argv[])
{
  struct HarnessOpts opts = { 0 };
  if (harness_parse_args(&opts, argc, argv) != 0)
    return 1;

  opts.type = MUTT_MMDF;

  if (!harness_init(Modules, opts.quiet))
    return 1;

  int rc = harness_run(&opts);
  harness_cleanup();
  return rc;
}
