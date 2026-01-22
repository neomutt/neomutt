/**
 * @file
 * Parse Spam Commands
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
 * @page commands_spam Parse Spam Commands
 *
 * Parse Spam Commands
 */

#include "config.h"
#include <stdio.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "spam.h"
#include "parse/lib.h"

/**
 * parse_nospam - Parse the 'nospam' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `nospam { * | <regex> }`
 */
enum CommandResult parse_nospam(const struct Command *cmd, struct Buffer *line,
                                const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_WARNING;

  // Extract the first token, a regex or "*"
  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    goto done;
  }

  // "*" is special - clear both spam and nospam lists
  if (mutt_str_equal(buf_string(token), "*"))
  {
    mutt_replacelist_free(&SpamList);
    mutt_regexlist_free(&NoSpamList);
    rc = MUTT_CMD_SUCCESS;
    goto done;
  }

  // If it's on the spam list, just remove it
  if (mutt_replacelist_remove(&SpamList, buf_string(token)) != 0)
  {
    rc = MUTT_CMD_SUCCESS;
    goto done;
  }

  // Otherwise, add it to the nospam list
  if (mutt_regexlist_add(&NoSpamList, buf_string(token), REG_ICASE, err) != 0)
  {
    rc = MUTT_CMD_ERROR;
    goto done;
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_spam - Parse the 'spam' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `spam <regex> [ <format> ]`
 */
enum CommandResult parse_spam(const struct Command *cmd, struct Buffer *line,
                              const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  struct Buffer *templ = NULL;
  enum CommandResult rc = MUTT_CMD_ERROR;

  // Extract the first token, a regex
  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  // If there's a second parameter, it's a template for the spam tag
  if (MoreArgs(line))
  {
    templ = buf_pool_get();
    parse_extract_token(templ, line, TOKEN_NO_FLAGS);

    // Add to the spam list
    if (mutt_replacelist_add(&SpamList, buf_string(token), buf_string(templ), err) != 0)
      goto done;
  }
  else
  {
    // If not, try to remove from the nospam list
    mutt_regexlist_remove(&NoSpamList, buf_string(token));
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&templ);
  buf_pool_release(&token);
  return rc;
}
