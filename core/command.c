/**
 * @file
 * NeoMutt Commands
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

/**
 * @page core_command NeoMutt Commands
 *
 * NeoMutt Commands
 */

#include "config.h"
#include <string.h>
#include "mutt/lib.h"
#include "command.h"

ARRAY_HEAD(CommandArray, struct Command);
struct CommandArray commands = ARRAY_HEAD_INITIALIZER;

/**
 * commands_cmp - Compare two commands by name - Implements ::sort_t - @ingroup sort_api
 */
static int commands_cmp(const void *a, const void *b)
{
  struct Command x = *(const struct Command *) a;
  struct Command y = *(const struct Command *) b;

  return strcmp(x.name, y.name);
}

/**
 * commands_register - Add commands to Commands array
 * @param cmds     Array of Commands
 * @param num_cmds Number of Commands in the Array
 */
void commands_register(const struct Command *cmds, const size_t num_cmds)
{
  for (int i = 0; i < num_cmds; i++)
  {
    ARRAY_ADD(&commands, cmds[i]);
  }
  ARRAY_SORT(&commands, commands_cmp);
}

/**
 * commands_free - Free Commands array
 */
void commands_free(void)
{
  ARRAY_FREE(&commands);
}

/**
 * commands_array - Get Commands array
 * @param first Set to first element of Commands array
 * @retval num Size of Commands array
 */
size_t commands_array(struct Command **first)
{
  *first = ARRAY_FIRST(&commands);
  return ARRAY_SIZE(&commands);
}

/**
 * command_get - Get a Command by its name
 * @param s Command string to lookup
 * @retval ptr  Success, Command
 * @retval NULL Error, no such command
 */
struct Command *command_get(const char *s)
{
  struct Command *cmd = NULL;
  ARRAY_FOREACH(cmd, &commands)
  {
    if (mutt_str_equal(s, cmd->name))
      return cmd;
  }
  return NULL;
}
