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
#include "mutt/lib.h"

struct Command;

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
#define MUTT_NAMED   (1 << 0)

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
