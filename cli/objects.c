/**
 * @file
 * Parse objects
 *
 * @authors
 * Copyright (C) 2025 Richard Russon <rich@flatcap.org>
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
 * @page cli_objects Parse objects
 *
 * Parse objects
 */

#include "config.h"
#include <stddef.h>
#include "mutt/lib.h"
#include "objects.h"

/**
 * sa_clear - Empty a StringArray
 * @param sa String Array
 *
 * @note The Array itself isn't freed
 */
static void sa_clear(struct StringArray *sa)
{
  const char **cp = NULL;
  ARRAY_FOREACH(cp, sa)
  {
    FREE(cp);
  }

  ARRAY_FREE(sa);
}

/**
 * cli_shared_clear - Clear a CliShared
 * @param shared CliShared to clear
 */
static void cli_shared_clear(struct CliShared *shared)
{
  buf_dealloc(&shared->log_file);
  buf_dealloc(&shared->log_level);
  buf_dealloc(&shared->mbox_type);

  sa_clear(&shared->commands);
  sa_clear(&shared->user_files);
}

/**
 * cli_info_clear - Clear a CliInfo
 * @param info CliInfo to clear
 */
static void cli_info_clear(struct CliInfo *info)
{
  sa_clear(&info->alias_queries);
  sa_clear(&info->queries);
}

/**
 * cli_send_clear - Clear a CliSend
 * @param send CliSend to clear
 */
static void cli_send_clear(struct CliSend *send)
{
  sa_clear(&send->addresses);
  sa_clear(&send->attach);
  sa_clear(&send->bcc_list);
  sa_clear(&send->cc_list);

  buf_dealloc(&send->draft_file);
  buf_dealloc(&send->include_file);
  buf_dealloc(&send->subject);
}

/**
 * cli_tui_clear - Clear a CliTui
 * @param tui CliTui to clear
 */
static void cli_tui_clear(struct CliTui *tui)
{
  buf_dealloc(&tui->folder);
  buf_dealloc(&tui->nntp_server);
}

/**
 * command_line_new - Create a new CommandLine
 * @retval ptr New CommandLine
 */
struct CommandLine *command_line_new(void)
{
  return mutt_mem_calloc_T(1, struct CommandLine);
}

/**
 * command_line_free - Free a CommandLine
 * @param ptr CommandLine to free
 */
void command_line_free(struct CommandLine **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct CommandLine *cl = *ptr;

  // cl->help - nothing to do
  cli_shared_clear(&cl->shared);
  cli_info_clear(&cl->info);
  cli_send_clear(&cl->send);
  cli_tui_clear(&cl->tui);

  FREE(ptr);
}
