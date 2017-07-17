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
 * struct Command - A user-callable command
 */
struct Command
{
  char *name;
  int (*func)(struct Buffer *, struct Buffer *, unsigned long, struct Buffer *);
  unsigned long data;
};

const struct Command *mutt_command_get(const char *s);
void mutt_commands_apply(void *data, void (*application)(void *, const struct Command *));

#endif /* _MUTT_COMMANDS_H */
