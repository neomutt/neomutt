/**
 * @file
 * Nntp-specific Email data
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

#ifndef MUTT_NNTP_EDATA_H
#define MUTT_NNTP_EDATA_H

#include "lib.h"

/**
 * struct NntpEmailData - NNTP-specific Email data - @extends Email
 */
struct NntpEmailData
{
  anum_t article_num;
  bool parsed : 1;
};

void                  nntp_edata_free(void **ptr);
struct NntpEmailData *nntp_edata_new (void);
struct NntpEmailData *nntp_edata_get (struct Email *e);

#endif /* MUTT_NNTP_EDATA_H */
