/**
 * @file
 * View of an Email
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_CORE_EVIEW_H
#define MUTT_CORE_EVIEW_H

struct Email;

/**
 * struct EmailView - View of an Email
 */
struct EmailView
{
  struct Email *email;              ///< Email data
};

void              eview_free(struct EmailView **ptr);
struct EmailView *eview_new (struct Email *e);

#endif /* MUTT_CORE_EVIEW_H */
