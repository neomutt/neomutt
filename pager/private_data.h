/**
 * @file
 * Private state data for the Pager
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

#ifndef MUTT_PAGER_PRIVATE_DATA_H
#define MUTT_PAGER_PRIVATE_DATA_H

struct MuttWindow;

/**
 * struct PagerPrivateData - Private state data for the Pager
 */
struct PagerPrivateData
{
  struct Menu *menu;
  struct MuttWindow *win_pbar;
  struct PagerRedrawData *rd;
};

void                     pager_private_data_free(struct MuttWindow *win, void **ptr);
struct PagerPrivateData *pager_private_data_new (void);

#endif /* MUTT_PAGER_PRIVATE_DATA_H */
