/**
 * @file
 * All user-callable functions
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
 * @page neo_opcode All user-callable functions
 *
 * All user-callable functions
 */

#include "config.h"
#include <stddef.h>
#include "opcodes.h"

#define DEFINE_HELP_MESSAGE(opcode, op_string) { #opcode, op_string },

/// Lookup table mapping an opcode to its name and description
/// e.g. `OpStrings[OP_EDIT_LABEL] = { "OP_EDIT_LABEL", "edit an email's label" }`
const char *OpStrings[][2] = {
  OPS(DEFINE_HELP_MESSAGE){ NULL, NULL },
};

#undef DEFINE_HELP_MESSAGE

/**
 * opcodes_get_name - Get the name of an opcode
 * @param op Opcode, e.g. OP_HELP
 * @retval str Name of the opcode
 */
const char *opcodes_get_name(int op)
{
  if ((op < OP_TIMEOUT) || (op >= OP_MAX))
    return "[UNKNOWN]";

  if (op == OP_ABORT)
    return "OP_ABORT";
  if (op == OP_TIMEOUT)
    return "OP_TIMEOUT";

  return OpStrings[op][0];
}

/**
 * opcodes_get_description - Get the description of an opcode
 * @param op Opcode, e.g. OP_HELP
 * @retval str Description of the opcode
 */
const char *opcodes_get_description(int op)
{
  if ((op < OP_TIMEOUT) || (op >= OP_MAX))
    return "[UNKNOWN]";

  if (op == OP_ABORT)
    return "abort the current action";
  if (op == OP_TIMEOUT)
    return "timeout occurred";

  return OpStrings[op][1];
}
