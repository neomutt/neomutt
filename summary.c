/**
 * @file
 * Sample command
 *
 * @authors
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include "mutt/mutt.h"
#include "mutt.h"
#include "summary.h"
#include "curs_lib.h"
#include "keymap.h"
#include "muttlib.h"
#include "opcodes.h"
#include "pager.h"
#include "protos.h"

void mutt_summary(void)
{
  char filename[_POSIX_PATH_MAX];
  char banner[SHORT_STRING];

  mutt_mktemp(filename, sizeof(filename));

  snprintf(banner, sizeof(banner), "This is a summary page");

  do
  {
    FILE *fp = mutt_file_fopen(filename, "w"); /* create/truncate file */
    if (!fp)
    {
      mutt_perror(filename);
      return;
    }

    for (int i = 0; i < 200; i++)
    {
      fprintf(fp, "Summary message %d\n", i);
    }

    mutt_file_fclose(&fp);
  } while (mutt_do_pager(banner, filename, MUTT_PAGER_RETWINCH, NULL) == OP_REFORMAT_WINCH);
}
