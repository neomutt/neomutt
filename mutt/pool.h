/**
 * @file
 * A global pool of Buffers
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_LIB_POOL_H
#define MUTT_LIB_POOL_H

struct Buffer;

void           mutt_buffer_pool_free    (void);
struct Buffer *mutt_buffer_pool_get     (void);
void           mutt_buffer_pool_release (struct Buffer **pbuf);

#endif /* MUTT_LIB_POOL_H */
