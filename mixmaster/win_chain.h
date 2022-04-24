/**
 * @file
 * Mixmaster Chain Window
 *
 * @authors
 * Copyright (C) 2022 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MIXMASTER_WIN_CHAIN_H
#define MUTT_MIXMASTER_WIN_CHAIN_H

#include <stdbool.h>

struct ListHead;
struct MuttWindow;
struct Remailer;

struct MuttWindow *win_chain_new(struct MuttWindow *win_cbar);

void win_chain_init      (struct MuttWindow *win, struct ListHead *chain, struct Remailer **type2_list);
int  win_chain_extract   (struct MuttWindow *win, struct ListHead *chain);
int  win_chain_get_length(struct MuttWindow *win);

bool win_chain_next(struct MuttWindow *win);
bool win_chain_prev(struct MuttWindow *win);

bool win_chain_append  (struct MuttWindow *win, struct Remailer *r);
bool win_chain_delete  (struct MuttWindow *win);
bool win_chain_insert  (struct MuttWindow *win, struct Remailer *r);
bool win_chain_validate(struct MuttWindow *win);

#endif /* MUTT_MIXMASTER_WIN_CHAIN_H */
