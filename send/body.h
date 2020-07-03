/**
 * @file
 * Convenience wrapper for the send headers
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_SEND_BODY_H
#define MUTT_SEND_BODY_H

#include <stdio.h>

struct Body;
struct ConfigSubset;

int mutt_write_mime_body(struct Body *a, FILE *fp, struct ConfigSubset *sub);

#endif /* MUTT_SEND_BODY_H */
