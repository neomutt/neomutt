/**
 * @file
 * Mailing-list action dialog
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page lib_mlist Mailing-list
 *
 * Mailing-list action dialog
 *
 * This dialog presents the actions advertised by a message's RFC2369 List-*
 * headers (Help, Post, Subscribe, Unsubscribe, Archives, Owner) and lets the
 * user act on them.
 *
 * | File                | Description                |
 * | :------------------ | :------------------------- |
 * | mlist/dlg_mlist.c   | @subpage mlist_dlg_mlist   |
 * | mlist/module.c      | @subpage mlist_module      |
 */

#ifndef MUTT_MLIST_LIB_H
#define MUTT_MLIST_LIB_H

struct Email;
struct Mailbox;
struct NeoMutt;
struct SubMenu;

void dlg_mlist(struct Mailbox *m, struct Email *e);

void mlist_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic);

#endif /* MUTT_MLIST_LIB_H */
