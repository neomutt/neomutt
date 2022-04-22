/**
 * @file
 * Private Index Functions
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

#ifndef MUTT_INDEX_PRIVATE_H
#define MUTT_INDEX_PRIVATE_H

#include <stdbool.h>

struct IndexPrivateData;
struct IndexSharedData;
struct ConfigSubset;

struct MuttWindow *index_window_new(struct IndexPrivateData *priv);
struct MuttWindow *ipanel_new(bool status_on_top, struct IndexSharedData *shared);
int index_adjust_sort_threads(const struct ConfigSubset *sub);

#endif /* MUTT_INDEX_PRIVATE_H */
