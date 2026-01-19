/**
 * @file
 * GUI display the mailboxes in a side panel
 *
 * @authors
 * Copyright (C) 2019-2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_sidebar Sidebar
 *
 * Display the mailboxes in a side panel
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | sidebar/commands.c  | @subpage sidebar_commands  |
 * | sidebar/config.c    | @subpage sidebar_config    |
 * | sidebar/expando.c   | @subpage sidebar_expando   |
 * | sidebar/functions.c | @subpage sidebar_functions |
 * | sidebar/module.c    | @subpage sidebar_module    |
 * | sidebar/observer.c  | @subpage sidebar_observers |
 * | sidebar/sidebar.c   | @subpage sidebar_sidebar   |
 * | sidebar/sort.c      | @subpage sidebar_sort      |
 * | sidebar/wdata.c     | @subpage sidebar_wdata     |
 * | sidebar/window.c    | @subpage sidebar_window    |
 */

#ifndef MUTT_SIDEBAR_LIB_H
#define MUTT_SIDEBAR_LIB_H

struct Buffer;
struct Command;
struct KeyEvent;
struct MuttWindow;
struct SubMenu;

void sb_init   (void);
void sb_cleanup(void);

int sb_function_dispatcher(struct MuttWindow *win, const struct KeyEvent *event);

enum CommandResult parse_sidebar_pin  (const struct Command *cmd, struct Buffer *line, struct Buffer *err);
enum CommandResult parse_sidebar_unpin(const struct Command *cmd, struct Buffer *line, struct Buffer *err);

void sidebar_init_keys(struct SubMenu *sm_generic);
struct SubMenu *sidebar_get_submenu(void);

#endif /* MUTT_SIDEBAR_LIB_H */
