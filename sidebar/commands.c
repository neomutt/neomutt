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
#include "mutt.h"
#include "lib.h"
#include "init.h"
#include "mutt_commands.h"
#include "muttlib.h"

/**
 * sb_parse_whitelist - Parse the 'sidebar_whitelist' command - Implements Command::parse()
 */
enum CommandResult sb_parse_whitelist(struct Buffer *buf, struct Buffer *s,
                                      intptr_t data, struct Buffer *err)
{
  struct Buffer *path = mutt_buffer_pool_get();

  do
  {
    mutt_extract_token(path, s, MUTT_TOKEN_BACKTICK_VARS);
    mutt_buffer_expand_path(path);
    add_to_stailq(&SidebarWhitelist, mutt_b2s(path));
  } while (MoreArgs(s));
  mutt_buffer_pool_release(&path);

  return MUTT_CMD_SUCCESS;
}

/**
 * sb_parse_unwhitelist - Parse the 'unsidebar_whitelist' command - Implements Command::parse()
 */
enum CommandResult sb_parse_unwhitelist(struct Buffer *buf, struct Buffer *s,
                                        intptr_t data, struct Buffer *err)
{
  struct Buffer *path = mutt_buffer_pool_get();

  do
  {
    mutt_extract_token(path, s, MUTT_TOKEN_BACKTICK_VARS);
    /* Check for deletion of entire list */
    if (mutt_str_equal(mutt_b2s(path), "*"))
    {
      mutt_list_free(&SidebarWhitelist);
      break;
    }
    mutt_buffer_expand_path(path);
    remove_from_stailq(&SidebarWhitelist, mutt_b2s(path));
  } while (MoreArgs(s));
  mutt_buffer_pool_release(&path);

  return MUTT_CMD_SUCCESS;
}
