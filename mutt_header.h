/**
 * @file
 * Representation of the email's header
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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

#ifndef _MUTT_HEADER2_H
#define _MUTT_HEADER2_H

#include <stddef.h>

struct Context;
struct Header;

void mutt_edit_headers(const char *editor, const char *body, struct Header *msg, char *fcc, size_t fcclen);
void mutt_label_hash_add(struct Context *ctx, struct Header *hdr);
void mutt_label_hash_remove(struct Context *ctx, struct Header *hdr);
int  mutt_label_message(struct Header *hdr);
void mutt_make_label_hash(struct Context *ctx);

#endif /* _MUTT_HEADER2_H */
