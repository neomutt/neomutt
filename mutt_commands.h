/**
 * @file
 * Definitions of NeoMutt commands
 *
 * @authors
 * Copyright (C) 2016 Bernard Pratz <z+mutt+pub@m0g.net>
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

#ifndef MUTT_MUTT_COMMANDS_H
#define MUTT_MUTT_COMMANDS_H

#include "config.h"
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"

/**
 * enum CommandResult - Error codes for command_t parse functions
 */
enum CommandResult
{
  MUTT_CMD_ERROR   = -1, ///< Error: Can't help the user
  MUTT_CMD_WARNING = -2, ///< Warning: Help given to the user
  MUTT_CMD_SUCCESS =  0, ///< Success: Command worked
  MUTT_CMD_FINISH  =  1  ///< Finish: Stop processing this file
};

/**
 * struct Command - A user-callable command
 */
struct Command
{
  const char *name; ///< Name of the command

  /**
   * parse - Function to parse a command
   * @param buf  Temporary Buffer space
   * @param s    Buffer containing string to be parsed
   * @param data Flags associated with the command
   * @param err  Buffer for error messages
   * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
   */
  enum CommandResult (*parse)(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

  intptr_t data; ///< Data or flags to pass to the command
};

/**
 * enum MuttSetCommand - Flags for parse_set()
 */
enum MuttSetCommand
{
  MUTT_SET_SET,   ///< default is to set all vars
  MUTT_SET_INV,   ///< default is to invert all vars
  MUTT_SET_UNSET, ///< default is to unset all vars
  MUTT_SET_RESET, ///< default is to reset all vars to default
};

/* parameter to parse_mailboxes */
#define MUTT_NAMED              (1 << 0)
#define MUTT_COMMAND_DEPRECATED (1 << 1)

/* command registry functions */
#define COMMANDS_REGISTER(cmds) commands_register(cmds, mutt_array_size(cmds))

void            mutt_commands_init     (void);
void            commands_register      (const struct Command *cmdv, const size_t cmds);
void            mutt_commands_free     (void);
size_t          mutt_commands_array    (struct Command **first);
struct Command *mutt_command_get       (const char *s);
#ifdef USE_LUA
void            mutt_commands_apply    (void *data, void (*application)(void *, const struct Command *));
#endif

#endif /* MUTT_MUTT_COMMANDS_H */
