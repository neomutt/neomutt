/**
 * @file
 * IMAP MSN helper functions
 *
 * @authors
 * Copyright (C) 2020 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_IMAP_MSN_H
#define MUTT_IMAP_MSN_H

#include <stdlib.h>

struct MSN;
struct Email;

void          imap_msn_free   (struct MSN *msn);
struct Email *imap_msn_get    (const struct MSN *msn, size_t idx);
size_t        imap_msn_highest(const struct MSN *msn);
void          imap_msn_remove (struct MSN *msn, size_t idx);
void          imap_msn_reserve(struct MSN *msn, size_t num);
void          imap_msn_set    (struct MSN *msn, size_t idx, struct Email *e);
size_t        imap_msn_shrink (struct MSN *msn, size_t num);

#endif /* MUTT_IMAP_MSN_H */
