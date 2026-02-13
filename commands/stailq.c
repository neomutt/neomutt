/**
 * @file
 * Parse Stailq Commands
 *
 * @authors
 * Copyright (C) 1996-2016 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
 * Copyright (C) 2019-2026 Richard Russon <rich@flatcap.org>
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
 * @page commands_stailq Parse Stailq Commands
 *
 * Parse Stailq Commands
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "stailq.h"
#include "parse/lib.h"

/**
 * parse_stailq_list - Parse a list command - Implements Command::parse() - @ingroup command_parse
 * @param cmd  Command being parsed
 * @param line Buffer containing string to be parsed
 * @param list List for the results
 * @param pc   Parse Context
 * @param pe   Parse Errors
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult parse_stailq_list(const struct Command *cmd,
                                     struct Buffer *line, struct ListHead *list,
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
    add_to_stailq(list, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_stailq - Parse a list command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `alternative-order <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `auto-view <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `header-order <header> [ <header> ... ]`
 */
enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line,
                                const struct ParseContext *pc, struct ParseError *pe)
{
  return parse_stailq_list(cmd, line, (struct ListHead *) cmd->data, pc, pe);
}

/**
 * parse_unstailq_list - Parse an unlist command - Implements Command::parse() - @ingroup command_parse
 * @param cmd  Command being parsed
 * @param line Buffer containing string to be parsed
 * @param list List for the results
 * @param pc   Parse Context
 * @param pe   Parse Errors
 * @retval #CommandResult Result e.g. #MUTT_CMD_SUCCESS
 */
enum CommandResult parse_unstailq_list(const struct Command *cmd,
                                       struct Buffer *line, struct ListHead *list,
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
    /* Check for deletion of entire list */
    if (mutt_str_equal(buf_string(token), "*"))
    {
      mutt_list_free(list);
      break;
    }
    remove_from_stailq(list, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unstailq - Parse an unlist command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unalternative-order { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unauto-view { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unheader-order { * | <header> ... }`
 * - `unmime-lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 */
enum CommandResult parse_unstailq(const struct Command *cmd, struct Buffer *line,
                                  const struct ParseContext *pc, struct ParseError *pe)
{
  return parse_unstailq_list(cmd, line, (struct ListHead *) cmd->data, pc, pe);
}
