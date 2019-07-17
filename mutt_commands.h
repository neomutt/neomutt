/**
 * @file
 * Mapping from user command name to function
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

#include <stdint.h>

struct Buffer;

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
 * typedef command_t - Prototype for a function to parse a command
 * @param buf  Temporary Buffer space
 * @param s    Buffer containing string to be parsed
 * @param data Flags associated with the command
 * @param err  Buffer for error messages
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
typedef enum CommandResult (*command_t)(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);

/**
 * struct Command - A user-callable command
 */
struct Command
{
  const char *name; ///< Name of the command
  command_t func;   ///< Function to parse the command
  intptr_t data;    ///< Data or flags to pass to the command
};

const struct Command *mutt_command_get(const char *s);
void mutt_commands_apply(void *data, void (*application)(void *, const struct Command *));

#endif /* MUTT_MUTT_COMMANDS_H */
