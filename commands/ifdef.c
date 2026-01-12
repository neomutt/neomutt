/**
 * @file
 * Parse Ifdef Commands
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
 * @page commands_ifdef Parse Ifdef Commands
 *
 * Parse Ifdef Commands
 */

#include "config.h"
#include <stdbool.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "ifdef.h"
#include "color/lib.h"
#include "key/lib.h"
#include "parse/lib.h"
#include "store/lib.h"
#include "version.h"

/**
 * is_function - Is the argument a neomutt function?
 * @param name  Command name to be searched for
 * @retval true  Function found
 * @retval false Function not found
 */
static bool is_function(const char *name)
{
  int op = km_get_op(name);

  return (op != OP_NULL);
}

/**
 * is_color_object - Is the argument a neomutt colour?
 * @param name  Colour name to be searched for
 * @retval true  Function found
 * @retval false Function not found
 */
static bool is_color_object(const char *name)
{
  int cid = mutt_map_get_value(name, ColorFields);

  return (cid > 0);
}

/**
 * parse_ifdef - Parse the 'ifdef' and 'ifndef' commands - Implements Command::parse() - @ingroup command_parse
 *
 * The 'ifdef' command allows conditional elements in the config file.
 * If a given variable, function, command or compile-time symbol exists, then
 * read the rest of the line of config commands.
 * e.g.
 *      ifdef sidebar source ~/.neomutt/sidebar.rc
 *
 * Parse:
 * - `ifdef  \<symbol\> '<config-command> [ \<args\> ... ]'`
 * - `ifndef \<symbol\> '<config-command> [ \<args\> ... ]'`
 */
enum CommandResult parse_ifdef(const struct Command *cmd, struct Buffer *line,
                               struct Buffer *err)
{
  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  struct Buffer *token = buf_pool_get();
  enum CommandResult rc = MUTT_CMD_SUCCESS;

  parse_extract_token(token, line, TOKEN_NO_FLAGS);

  // is the item defined as:
  bool res = cs_subset_lookup(NeoMutt->sub, buf_string(token)) // a variable?
             || feature_enabled(buf_string(token)) // a compiled-in feature?
             || is_function(buf_string(token))     // a function?
             || commands_get(&NeoMutt->commands, buf_string(token)) // a command?
             || is_color_object(buf_string(token))                  // a color?
#ifdef USE_HCACHE
             || store_is_valid_backend(buf_string(token)) // a store? (database)
#endif
             || mutt_str_getenv(buf_string(token)); // an environment variable?

  if (!MoreArgs(line))
  {
    buf_printf(err, _("%s: too few arguments"), cmd->name);
    rc = MUTT_CMD_WARNING;
    goto done;
  }
  parse_extract_token(token, line, TOKEN_SPACE);

  /* ifdef KNOWN_SYMBOL or ifndef UNKNOWN_SYMBOL */
  if ((res && (cmd->id == CMD_IFDEF)) || (!res && (cmd->id == CMD_IFNDEF)))
  {
    rc = parse_rc_line(token, err);
    if (rc == MUTT_CMD_ERROR)
      mutt_error(_("Error: %s"), buf_string(err));

    goto done;
  }

done:
  buf_pool_release(&token);
  return rc;
}

/**
 * parse_finish - Parse the 'finish' command - Implements Command::parse() - @ingroup command_parse
 * @retval  #MUTT_CMD_FINISH Stop processing the current file
 * @retval  #MUTT_CMD_WARNING Failed
 *
 * If the 'finish' command is found, we should stop reading the current file.
 *
 * Parse:
 * - `finish`
 */
enum CommandResult parse_finish(const struct Command *cmd, struct Buffer *line,
                                struct Buffer *err)
{
  if (MoreArgs(line))
  {
    buf_printf(err, _("%s: too many arguments"), cmd->name);
    return MUTT_CMD_WARNING;
  }

  return MUTT_CMD_FINISH;
}
