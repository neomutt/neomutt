/**
 * @file
 * GUI editor for an email's headers
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
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
 * @page lib_compose Compose an Email
 *
 * Compose an email
 *
 * | File                   | Description                    |
 * | :--------------------- | :----------------------------- |
 * | compose/attach.c       | @subpage compose_attach        |
 * | compose/attach_data.c  | @subpage compose_attach_data   |
 * | compose/cbar.c         | @subpage compose_cbar          |
 * | compose/cbar_data.c    | @subpage compose_cbar_data     |
 * | compose/config.c       | @subpage compose_config        |
 * | compose/dlg_compose.c  | @subpage compose_dlg_compose   |
 * | compose/functions.c    | @subpage compose_functions     |
 * | compose/shared_data.c  | @subpage compose_shared_data   |
 */

#ifndef MUTT_COMPOSE_LIB_H
#define MUTT_COMPOSE_LIB_H

#include <stdint.h>

struct Buffer;
struct ConfigSubset;
struct Email;

/* flags for dlg_compose() */
#define MUTT_COMPOSE_NOFREEHEADER (1 << 0)

int dlg_compose(struct Email *e, struct Buffer *fcc, uint8_t flags, struct ConfigSubset *sub);

#endif /* MUTT_COMPOSE_LIB_H */
