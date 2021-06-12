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
 * | File                 | Description                 |
 * | :------------------- | :-------------------------- |
 * | index/config.c       | @subpage index_config       |
 * | index/dlg_index.c    | @subpage index_dialog       |
 * | index/ibar.c         | @subpage index_ibar         |
 * | index/index.c        | @subpage index_index        |
 * | index/ipanel.c       | @subpage index_ipanel       |
 * | index/private_data.c | @subpage index_private_data |
 * | index/shared_data.c  | @subpage index_shared_data  |
 */

#ifndef MUTT_INDEX_LIB_H
#define MUTT_INDEX_LIB_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "core/lib.h"
#include "shared_data.h" // IWYU pragma: keep

struct Context;
struct Email;
struct Menu;
struct MuttWindow;

typedef uint8_t NotifyIndex;         ///< Flags, e.g. #NT_INDEX_ACCOUNT
#define NT_INDEX_NO_FLAGS        0   ///< No flags are set
#define NT_INDEX_CONFIG    (1 << 0)  ///< Config subset has changed
#define NT_INDEX_CONTEXT   (1 << 1)  ///< Context has changed
#define NT_INDEX_ACCOUNT   (1 << 2)  ///< Account has changed
#define NT_INDEX_MAILBOX   (1 << 3)  ///< Mailbox has changed
#define NT_INDEX_EMAIL     (1 << 4)  ///< Email has changed
#define NT_INDEX_CLOSING   (1 << 5)  ///< The Index is about to close
#define NT_INDEX_SUBSET    (1 << 6)  ///< Config Subset has changed

int  index_color(struct Menu *menu, int line);
void index_make_entry(struct Menu *menu, char *buf, size_t buflen, int line);
void mutt_draw_statusline(struct MuttWindow *win, int cols, const char *buf, size_t buflen);
struct Mailbox *mutt_index_menu(struct MuttWindow *dlg, struct Mailbox *m);
void mutt_set_header_color(struct Mailbox *m, struct Email *e);
void mutt_update_index(struct Menu *menu, struct Context *ctx, enum MxStatus check, int oldcount, struct IndexSharedData *shared);
struct MuttWindow *index_pager_init(void);

#endif /* MUTT_INDEX_LIB_H */
