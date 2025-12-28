/**
 * @file
 * Parse Stailq Commands
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2010,2012-2013,2016 Michael R. Elkins <me@mutt.org>
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
 * @page commands_stailq Parse Stailq Commands
 *
 * Parse Stailq Commands
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "stailq.h"
#include "parse/lib.h"
#include "muttlib.h"

/**
 * parse_stailq - Parse a list command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `alternative_order <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `auto_view <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 * - `hdr_order <header> [ <header> ... ]`
 * - `mailto_allow { * | <header-field> ... }`
 * - `mime_lookup <mime-type>[/<mime-subtype> ] [ <mime-type>[/<mime-subtype> ] ... ]`
 */
enum CommandResult parse_stailq(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  do
  {
    parse_extract_token(token, line, TOKEN_NO_FLAGS);
    add_to_stailq((struct ListHead *) cmd->data, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_unstailq - Parse an unlist command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `unalternative_order { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unauto_view { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 * - `unhdr_order { * | <header> ... }`
 * - `unmailto_allow { * | <header-field> ... }`
 * - `unmime_lookup { * | [ <mime-type>[/<mime-subtype> ] ... ] }`
 */
enum CommandResult parse_unstailq(const struct Command *cmd,
                                  struct Buffer *line, struct Buffer *err)
{
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
      mutt_list_free((struct ListHead *) cmd->data);
      break;
    }
    remove_from_stailq((struct ListHead *) cmd->data, buf_string(token));
  } while (MoreArgs(line));

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}
