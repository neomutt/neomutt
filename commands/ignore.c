/**
 * @file
 * Parse Ignore Commands
 *
 * @authors
 * Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2020 Aditya De Saha <adityadesaha@gmail.com>
 * Copyright (C) 2020 Matthew Hughes <matthewhughes934@gmail.com>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
 * Copyright (C) 2020-2022 Pietro Cerutti <gahr@gahr.ch>
 * Copyright (C) 2022 Marco Sirabella <marco@sirabella.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
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
 * @page commands_ignore Parse Ignore Commands
 *
 * Parse Ignore Commands
 */

#include "config.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "ignore.h"
#include "parse/lib.h"

/**
 * parse_ignore - Parse the 'ignore' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `ignore <string> [ <string> ...]`
 */
enum CommandResult parse_ignore(const struct Command *cmd, struct Buffer *line,
                                const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    remove_from_stailq(&UnIgnore, buf_string(token));
    add_to_stailq(&Ignore, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unignore - Parse the 'unignore' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unignore { * | <string> ... }`
 */
enum CommandResult parse_unignore(const struct Command *cmd, struct Buffer *line,
                                  const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);

    /* don't add "*" to the unignore list */
    if (!mutt_str_equal(buf_string(token), "*"))
      add_to_stailq(&UnIgnore, buf_string(token));

    remove_from_stailq(&Ignore, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}
