/**
 * @file
 * GUI manage the main index (list of emails)
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page lib_index INDEX: GUI manage the main index (list of emails)
 *
 * GUI manage the main index (list of emails)
 *
 * | File               | Description              |
 * | :----------------- | :----------------------- |
 * | index/config.c     | @subpage index_config    |
 * | index/index.c      | @subpage index_index     |
 * | index/observer.c   | @subpage index_observer  |
 */

#ifndef MUTT_INDEX_LIB_H
#define MUTT_INDEX_LIB_H

#include <stdbool.h>
#include <stdio.h>

struct Context;
struct Email;
struct Mailbox;
struct Menu;
struct MuttWindow;

int  index_color(struct Menu *menu, int line);
void index_make_entry(struct Menu *menu, char *buf, size_t buflen, int line);
void mutt_draw_statusline(int cols, const char *buf, size_t buflen);
struct Mailbox *mutt_index_menu(struct MuttWindow *dlg, struct Mailbox *m);
void mutt_set_header_color(struct Mailbox *m, struct Email *e);
void mutt_update_index(struct Menu *menu, struct Context *ctx, int check, int oldcount, const struct Email *curr_email);
struct MuttWindow *index_pager_init(void);
void index_pager_shutdown(struct MuttWindow *dlg);

#endif /* MUTT_INDEX_LIB_H */
