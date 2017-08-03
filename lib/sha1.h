/**
 * @file
 * Calculate the SHA1 checksum of a buffer
 *
 * @authors
 * Copyright (C) 2000 Steve Reid <steve@edmweb.com>
 * Copyright (C) 2000 Thomas Roessler <roessler@does-not-exist.org>
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

#ifndef _LIB_SHA1_H
#define _LIB_SHA1_H

#include <stdint.h>

/**
 * struct Sha1Ctx - Cursor for the SHA1 hashing
 */
struct Sha1Ctx
{
  uint32_t state[5];
  uint32_t count[2];
  unsigned char buffer[64];
};

void sha1_final(unsigned char digest[20], struct Sha1Ctx *context);
void sha1_init(struct Sha1Ctx *context);
void sha1_transform(uint32_t state[5], const unsigned char buffer[64]);
void sha1_update(struct Sha1Ctx *context, const unsigned char *data, uint32_t len);

#define SHA_DIGEST_LENGTH 20

#endif /* _LIB_SHA1_H */
