/**
 * @file
 * Calculate the MD5 checksum of a buffer
 *
 * @authors
 * Copyright (C) 1995-1997,1999,2000,2001,2004,2005,2006,2008
 *    Free Software Foundation, Inc.
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
 *
 * Declaration of functions and data types used for MD5 sum computing
 * library functions.
 *
 * NOTE: The canonical source of this file is maintained with the GNU C
 * Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu.
 */

#ifndef _MUTT_MD5_H
#define _MUTT_MD5_H

#include <stdint.h>
#include <stdio.h>

typedef uint32_t md5_uint32;

/**
 * struct Md5Ctx - Cursor for the MD5 hashing
 *
 * Structure to save state of computation between the single steps.
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

/*
 * The following three functions are build up the low level used in
 * the functions `md5_stream' and `md5_buffer'.
 */

/* Initialize structure containing state of computation.
 * (RFC1321, 3.3: Step 3)  */
void md5_init_ctx(struct Md5Ctx *ctx);

/* Starting with the result of former calls of this function (or the
 * initialization function update the context for the next LEN bytes
 * starting at Buffer.
 * It is necessary that LEN is a multiple of 64!!! */
void md5_process_block(const void *buffer, size_t len, struct Md5Ctx *ctx);

/* Starting with the result of former calls of this function (or the
 * initialization function update the context for the next LEN bytes
 * starting at Buffer.
 * It is NOT required that LEN is a multiple of 64.  */
void md5_process_bytes(const void *buffer, size_t len, struct Md5Ctx *ctx);

/* Process the remaining bytes in the buffer and put result from CTX
 * in first 16 bytes following RESBUF.  The result is always in little
 * endian byte order, so that a byte-wise output yields to the wanted
 * ASCII representation of the message digest.  */
void *md5_finish_ctx(struct Md5Ctx *ctx, void *resbuf);

/* Put result from CTX in first 16 bytes following RESBUF.  The result is
 * always in little endian byte order, so that a byte-wise output yields
 * to the wanted ASCII representation of the message digest.  */
void *md5_read_ctx(const struct Md5Ctx *ctx, void *resbuf);

/* Compute MD5 message digest for bytes read from STREAM.  The
 * resulting message digest number will be written into the 16 bytes
 * beginning at RESBLOCK.  */
int md5_stream(FILE *stream, void *resblock);

/* Compute MD5 message digest for LEN bytes beginning at Buffer.  The
 * result is always in little endian byte order, so that a byte-wise
 * output yields to the wanted ASCII representation of the message
 * digest.  */
void *md5_buffer(const char *buffer, size_t len, void *resblock);

#endif /* _MUTT_MD5_H */
