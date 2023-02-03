/**
 * @file
 * Sidebar commands
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
 * @page sidebar_commands Sidebar commands
 *
 * Sidebar commands
 */

#include "config.h"
#include <stdint.h>
#include "private.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "init.h"
#include "muttlib.h"

/**
 * sb_parse_sidebar_pin - Parse the 'sidebar_pin' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult sb_parse_sidebar_pin(struct Buffer *buf, struct Buffer *s,
                                        intptr_t data, struct Buffer *err)
{
  struct Buffer *path = mutt_buffer_pool_get();

  do
  {
    mutt_extract_token(path, s, MUTT_TOKEN_BACKTICK_VARS);
    mutt_buffer_expand_path(path);
    add_to_stailq(&SidebarPinned, mutt_buffer_string(path));
  } while (MoreArgs(s));
  mutt_buffer_pool_release(&path);

  return MUTT_CMD_SUCCESS;
}

/**
 * sb_parse_sidebar_unpin - Parse the 'sidebar_unpin' command - Implements Command::parse() - @ingroup command_parse
 */
enum CommandResult sb_parse_sidebar_unpin(struct Buffer *buf, struct Buffer *s,
                                          intptr_t data, struct Buffer *err)
{
  struct Buffer *path = mutt_buffer_pool_get();

  do
  {
    mutt_extract_token(path, s, MUTT_TOKEN_BACKTICK_VARS);
    /* Check for deletion of entire list */
    if (mutt_str_equal(mutt_buffer_string(path), "*"))
    {
      mutt_list_free(&SidebarPinned);
      break;
    }
    mutt_buffer_expand_path(path);
    remove_from_stailq(&SidebarPinned, mutt_buffer_string(path));
  } while (MoreArgs(s));
  mutt_buffer_pool_release(&path);

  return MUTT_CMD_SUCCESS;
}
