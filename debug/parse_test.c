/**
 * @file
 * Test the config parsing code
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page debug_parse Test the config parsing code
 *
 * Test the config parsing code
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "lib.h"
#include "init.h"
#include "mutt_commands.h"

/**
 * test_parse_set - Test the config parsing
 */
void test_parse_set(void)
{
  const char *vars[] = {
    "from",        // ADDRESS
    "beep",        // BOOL
    "ispell",      // COMMAND
    "mbox_type",   // ENUM
    "to_chars",    // MBTABLE
    "net_inc",     // NUMBER
    "signature",   // PATH
    "print",       // QUAD
    "mask",        // REGEX
    "sort",        // SORT
    "attribution", // STRING
    "zzz",         // UNKNOWN
    "my_var",      // MY_VAR
  };

  const char *commands[] = {
    "set",
    "toggle",
    "reset",
    "unset",
  };

  const char *tests[] = {
    "%s %s",       "%s %s=42",  "%s %s?",     "%s ?%s",    "%s ?%s=42",
    "%s ?%s?",     "%s no%s",   "%s no%s=42", "%s no%s?",  "%s inv%s",
    "%s inv%s=42", "%s inv%s?", "%s &%s",     "%s &%s=42", "%s &%s?",
  };

  struct Buffer err = mutt_buffer_make(256);
  char line[64];

  for (size_t v = 0; v < mutt_array_size(vars); v++)
  {
    // printf("--------------------------------------------------------------------------------\n");
    // printf("VARIABLE %s\n", vars[v]);
    for (size_t c = 0; c < mutt_array_size(commands); c++)
    {
      // printf("----------------------------------------\n");
      // printf("COMMAND %s\n", commands[c]);
      for (size_t t = 0; t < mutt_array_size(tests); t++)
      {
        mutt_buffer_reset(&err);

        snprintf(line, sizeof(line), tests[t], commands[c], vars[v]);
        printf("%-26s", line);
        enum CommandResult rc = mutt_parse_rc_line(line, &err);
        printf("%2d %s\n", rc, err.data);
      }
      printf("\n");
    }
    // printf("\n");
  }

  mutt_buffer_dealloc(&err);
}
