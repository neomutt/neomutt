/**
 * @file
 * GUI display the mailboxes in a side panel
 *
 * @authors
 * Copyright (C) 2004 Justin Hibbits <jrh29@po.cwru.edu>
 * Copyright (C) 2004 Thomer M. Gil <mutt@thomer.com>
 * Copyright (C) 2015-2020 Richard Russon <rich@flatcap.org>
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
 * @page sidebar SIDEBAR: Display the mailboxes in a side panel
 *
 * Display the mailboxes in a side panel
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | sidebar/commands.c  | @subpage sidebar_commands  |
 * | sidebar/config.c    | @subpage sidebar_config    |
 * | sidebar/functions.c | @subpage sidebar_functions |
 * | sidebar/observer.c  | @subpage sidebar_observers |
 * | sidebar/sidebar.c   | @subpage sidebar_sidebar   |
 * | sidebar/wdata.c     | @subpage sidebar_wdata     |
 */

#ifndef MUTT_SIDEBAR_LIB_H
#define MUTT_SIDEBAR_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include "mutt_commands.h"

struct Buffer;
struct ConfigSet;
struct MuttWindow;

void sb_init    (void);
void sb_shutdown(void);

void            sb_change_mailbox(struct MuttWindow *win, int op);
struct Mailbox *sb_get_highlight (struct MuttWindow *win);

enum CommandResult sb_parse_unwhitelist(struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);
enum CommandResult sb_parse_whitelist  (struct Buffer *buf, struct Buffer *s, intptr_t data, struct Buffer *err);

bool config_init_sidebar(struct ConfigSet *cs);

#endif /* MUTT_SIDEBAR_LIB_H */
