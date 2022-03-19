/**
 * @file
 * Compose Private Data
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

#ifndef MUTT_COMPOSE_PRIVATE_H
#define MUTT_COMPOSE_PRIVATE_H

#include "config.h"
#include <stdbool.h>

struct AttachCtx;
struct ComposeAttachData;
struct ComposeSharedData;
struct ConfigSubset;
struct Menu;
struct MuttWindow;

struct MuttWindow *attach_new(struct MuttWindow *parent, struct ComposeSharedData *shared);
unsigned long cum_attachs_size(struct ConfigSubset *sub, struct ComposeAttachData *adata);
int num_attachments(struct ComposeAttachData *adata);
void update_menu(struct AttachCtx *actx, struct Menu *menu, bool init);

#endif /* MUTT_COMPOSE_PRIVATE_H */
