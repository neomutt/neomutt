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
#include <stddef.h>
#include "mutt/lib.h"
#include "command.h"

/**
 * commands_sort - Compare two commands by name - Implements ::sort_t - @ingroup sort_api
 */
static int commands_sort(const void *a, const void *b, void *sdata)
{
  const struct Command *x = *(const struct Command **) a;
  const struct Command *y = *(const struct Command **) b;

  return mutt_str_cmp(x->name, y->name);
}

/**
 * commands_register - Add commands to Commands array
 * @param ca   Command Array
 * @param cmds New Commands to add
 * @retval true Success
 */
bool commands_register(struct CommandArray *ca, const struct Command *cmds)
{
  if (!ca || !cmds)
    return false;

  for (int i = 0; cmds[i].name; i++)
  {
    ARRAY_ADD(ca, &cmds[i]);
  }
  ARRAY_SORT(ca, commands_sort, NULL);

  return true;
}

/**
 * commands_clear - Clear an Array of Commands
 *
 * @note The Array itself is not freed
 */
void commands_clear(struct CommandArray *ca)
{
  ARRAY_FREE(ca);
}

/**
 * commands_get - Get a Command by its name
 * @param ca   Command Array
 * @param name Command name to lookup
 * @retval ptr  Success, Command
 * @retval NULL Error, no such command
 */
const struct Command *commands_get(struct CommandArray *ca, const char *name)
{
  const struct Command **cp = NULL;
  ARRAY_FOREACH(cp, ca)
  {
    const struct Command *cmd = *cp;

    if (mutt_str_equal(name, cmd->name))
      return cmd;
  }
  return NULL;
}
