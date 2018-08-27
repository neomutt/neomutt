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

#ifndef _MUTT_COMMANDS_H
#define _MUTT_COMMANDS_H

struct Buffer;

/**
 * typedef command_t - Prototype for a function to parse a command
 * @param buf  Temporary Buffer space
 * @param s    Buffer containing string to be parsed
 * @param data Flags associated with the command
 * @param err  Buffer for error messages
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Warning
 */
typedef int (*command_t)(struct Buffer *buf, struct Buffer *s, unsigned long data, struct Buffer *err);

/**
 * struct Command - A user-callable command
 */
struct Command
{
  char *name;         /**< Name of the command */
  command_t func;     /**< Function to parse the command */
  unsigned long data; /**< Data or flags to pass to the command */
};

const struct Command *mutt_command_get(const char *s);
void mutt_commands_apply(void *data, void (*application)(void *, const struct Command *));

#endif /* _MUTT_COMMANDS_H */
