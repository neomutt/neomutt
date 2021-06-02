/**
 * @file
 * Compose Attach Data
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_COMPOSE_ATTACH_DATA_H
#define MUTT_COMPOSE_ATTACH_DATA_H

/**
 * struct ComposeAttachData - Data to fill the Compose Attach Window
 */
struct ComposeAttachData
{
  struct AttachCtx *actx;
  struct Menu *menu;
};

struct MuttWindow;

void attach_data_free(struct MuttWindow *win, void **ptr);
struct ComposeAttachData *attach_data_new(void);

#endif /* MUTT_COMPOSE_ATTACH_DATA_H */
