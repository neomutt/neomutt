/**
 * @file
 * Sidebar commands
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Whitney Cumber
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
 * @page sidebar_commands Sidebar commands
 *
 * Sidebar commands
 */

#include "config.h"
#include <stdint.h>
#include "private.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "muttlib.h"

/**
 * sb_parse_sidebar_pin - Parse the 'sidebar_pin' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult sb_parse_sidebar_pin(struct Buffer *buf, struct Buffer *s,
                                        intptr_t data, struct Buffer *err)
{
  struct Buffer *path = buf_pool_get();

  do
  {
    parse_extract_token(path, s, TOKEN_BACKTICK_VARS);
    buf_expand_path(path);
    add_to_stailq(&SidebarPinned, buf_string(path));
  } while (MoreArgs(s));
  buf_pool_release(&path);

  return MUTT_CMD_SUCCESS;
}

/**
 * sb_parse_sidebar_unpin - Parse the 'sidebar_unpin' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult sb_parse_sidebar_unpin(struct Buffer *buf, struct Buffer *s,
                                          intptr_t data, struct Buffer *err)
{
  struct Buffer *path = buf_pool_get();

  do
  {
    parse_extract_token(path, s, TOKEN_BACKTICK_VARS);
    /* Check for deletion of entire list */
    if (mutt_str_equal(buf_string(path), "*"))
    {
      mutt_list_free(&SidebarPinned);
      break;
    }
    buf_expand_path(path);
    remove_from_stailq(&SidebarPinned, buf_string(path));
  } while (MoreArgs(s));
  buf_pool_release(&path);

  return MUTT_CMD_SUCCESS;
}
