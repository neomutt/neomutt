/**
 * @file
 * Code backtrace
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
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
 * @page backtrace Code backtrace
 *
 * Code backtrace
 */

#include "config.h"
#include <libunwind.h>
#include <stdio.h>
#include "mutt/mutt.h"

/**
 * show_backtrace - Log the program's call stack
 */
void show_backtrace(void)
{
  unw_cursor_t cursor;
  unw_context_t uc;
  unw_word_t ip, sp;
  char buf[256];

  printf("\nBacktrace\n");
  mutt_debug(LL_DEBUG1, "\nBacktrace\n");
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (unw_step(&cursor) > 0)
  {
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    unw_get_proc_name(&cursor, buf, sizeof(buf), &ip);
    if (buf[0] == '_')
      break;
    printf("\t%s() ip = %lx, sp = %lx\n", buf, (long) ip, (long) sp);
    mutt_debug(LL_DEBUG1, "\t%s() ip = %lx, sp = %lx\n", buf, (long) ip, (long) sp);
  }
  printf("\n");
}
