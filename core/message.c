/**
 * @file
 * A local copy of an email
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
 * @page core_message Local email
 *
 * A local copy of an email
 */

#include "config.h"
#include "mutt/lib.h"
#include "message.h"

/**
 * message_free - Free a Message
 * @param ptr Message to free
 */
void message_free(struct Message **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct Message *msg = *ptr;
  FREE(&msg->path);
  FREE(&msg->committed_path);

  FREE(ptr);
}

/**
 * message_new - Create a new Message
 * @retval ptr New Message
 */
struct Message *message_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Message));
}
