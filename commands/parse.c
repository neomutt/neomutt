/**
 * @file
 * Parse Simple Commands
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
 * @page commands_parse Parse Simple Commands
 *
 * Parse Simple Commands
 */

#include "config.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "parse.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "globals.h"
#include "muttlib.h"
#include "version.h"

/**
 * parse_cd - Parse the 'cd' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `cd [ <directory> ]`
 */
enum CommandResult parse_cd(const struct Command *cmd, struct Buffer *line, struct ParseContext *pctx, struct ConfigParseError *perr)
{
  struct Buffer *err = buf_pool_get();
  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  if (buf_is_empty(token))
  {
    buf_strcpy(token, NeoMutt->home_dir);
  }
  else
  {
    expand_path(token, false);
  }

  if (chdir(buf_string(token)) != 0)
  {
    buf_printf(err, "%s: %s", cmd->name, strerror(errno));
    goto done;
  }

  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&token);
  buf_pool_release(&err);
  return rc;
}

/**
 * parse_echo - Parse the 'echo' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `echo <message>`
 */
enum CommandResult parse_echo(const struct Command *cmd, struct Buffer *line, struct ParseContext *pctx, struct ConfigParseError *perr)
{
  struct Buffer *err = buf_pool_get();
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();

  parse_extract_token(token, line, TOKEN_NO_FLAGS);
  OptForceRefresh = true;
  mutt_message("%s", buf_string(token));
  OptForceRefresh = false;
  mutt_sleep(0);

  buf_pool_release(&token);
  return MUTT_CMD_SUCCESS;
}

/**
 * parse_version - Parse the 'version' command - Implements Command::parse() - @ingroup command_parse
 *
 * Parse:
 * - `version`
 */
enum CommandResult parse_version(const struct Command *cmd, struct Buffer *line, struct ParseContext *pctx, struct ConfigParseError *perr)
{
  struct Buffer *err = buf_pool_get();
  // silently ignore 'version' if it's in a config file
  if (!StartupComplete)
    return MUTT_CMD_SUCCESS;

  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *tempfile = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_ERROR;

  buf_mktemp(tempfile);

  FILE *fp_out = mutt_file_fopen(buf_string(tempfile), "w");
  if (!fp_out)
  {
    // L10N: '%s' is the file name of the temporary file
    buf_printf(err, _("Could not create temporary file %s"), buf_string(tempfile));
    goto done;
  }

  print_version(fp_out, false);
  mutt_file_fclose(&fp_out);

  struct PagerData pdata = { 0 };
  struct PagerView pview = { &pdata };

  pdata.fname = buf_string(tempfile);

  pview.banner = cmd->name;
  pview.flags = MUTT_PAGER_NO_FLAGS;
  pview.mode = PAGER_MODE_OTHER;

  mutt_do_pager(&pview, NULL);
  rc = MUTT_CMD_SUCCESS;

done:
  buf_pool_release(&tempfile);
  buf_pool_release(&err);
  return rc;
}
