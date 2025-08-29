/**
 * @file
 * Parse the output of CLI PGP programinclude "pgpkey.h"
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2017 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_NCRYPT_GNUPGPARSE_H
#define MUTT_NCRYPT_GNUPGPARSE_H

#include "pgpkey.h"

struct ListHead;

struct PgpKeyInfo * pgp_get_candidates(enum PgpRing keyring, struct ListHead *hints);

#endif /* MUTT_NCRYPT_GNUPGPARSE_H */
