/**
 * @file
 * Parse Tags Commands
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
 * @page commands_tags Parse Tags Commands
 *
 * Parse Tags Commands
 */

#include "config.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "tags.h"
#include "parse/lib.h"

/**
 * parse_tag_formats - Parse the 'tag-formats' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse config like: `tag-formats pgp GP`
 *
 * @note This maps format -> tag
 *
 * Parse:
 * - `tag-formats <tag> <format-string> [ <tag> <format-string> ... ] }`
 */
enum CommandResult parse_tag_formats(const struct Command *cmd, struct Buffer *line,
                                     const struct ParseContext *pc, struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *tag = buf_pool_get();
  struct Buffer *fmt = buf_pool_get();

  while (MoreArgs(line))
  {
    parse_extract_token(tag, line, TOKEN_NO_FLAGS);
    if (buf_is_empty(tag))
      continue;

    parse_extract_token(fmt, line, TOKEN_NO_FLAGS);

    /* avoid duplicates */
    const char *tmp = mutt_hash_find(TagFormats, buf_string(fmt));
    if (tmp)
    {
      mutt_warning(_("tag format '%s' already registered as '%s'"), buf_string(fmt), tmp);
      continue;
    }

    mutt_hash_insert(TagFormats, buf_string(fmt), buf_strdup(tag));
  }

  buf_pool_release(&tag);
  buf_pool_release(&fmt);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_tag_transforms - Parse the 'tag-transforms' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse config like: `tag-transforms pgp P`
 *
 * @note This maps tag -> transform
 *
 * Parse:
 * - `tag-transforms <tag> <transformed-string> [ <tag> <transformed-string> ... ]}`
 */
enum CommandResult parse_tag_transforms(const struct Command *cmd, struct Buffer *line,
                                        const struct ParseContext *pc,
                                        struct ParseError *pe)
{
  struct Buffer *err = pe->message;

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *tag = buf_pool_get();
  struct Buffer *trans = buf_pool_get();

  while (MoreArgs(line))
  {
    parse_extract_token(tag, line, TOKEN_NO_FLAGS);
    if (buf_is_empty(tag))
      continue;

    parse_extract_token(trans, line, TOKEN_NO_FLAGS);
    const char *trn = buf_string(trans);

    /* avoid duplicates */
    const char *tmp = mutt_hash_find(TagTransforms, buf_string(tag));
    if (tmp)
    {
      mutt_warning(_("tag transform '%s' already registered as '%s'"),
                   buf_string(tag), tmp);
      continue;
    }

    mutt_hash_insert(TagTransforms, buf_string(tag), mutt_str_dup(trn));
  }

  buf_pool_release(&tag);
  buf_pool_release(&trans);
  return MUTT_CMD_SUCCESS;
}
