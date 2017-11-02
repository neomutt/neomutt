/**
 * @file
 * Calculate the MD5 checksum of a buffer
 *
 * @authors
 * Copyright (C) 1995-2008 Free Software Foundation, Inc.
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

#ifndef _LIB_MD5_H
#define _LIB_MD5_H

#include <stdint.h>
#include <stdio.h>

typedef uint32_t md5_uint32;

/**
 * struct Md5Ctx - Cursor for the MD5 hashing
 *
 * Structure to save state of computation between the single steps
 */
struct Md5Ctx
{
  md5_uint32 A;
  md5_uint32 B;
  md5_uint32 C;
  md5_uint32 D;

  md5_uint32 total[2];
  md5_uint32 buflen;
  md5_uint32 buffer[32];
};

void *md5_buffer(const char *buffer, size_t len, void *resblock);
void *md5_finish_ctx(struct Md5Ctx *ctx, void *resbuf);
void  md5_init_ctx(struct Md5Ctx *ctx);
void  md5_process_block(const void *buffer, size_t len, struct Md5Ctx *ctx);
void  md5_process_bytes(const void *buffer, size_t len, struct Md5Ctx *ctx);
void *md5_read_ctx(const struct Md5Ctx *ctx, void *resbuf);
int   md5_stream(FILE *stream, void *resblock);

#endif /* _LIB_MD5_H */
