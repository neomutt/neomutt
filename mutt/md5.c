/**
 * @file
 * Calculate the MD5 checksum of a buffer
 *
 * @authors
 * Copyright (C) 1995 Ulrich Drepper <drepper@gnu.ai.mit.edu>
 * Copyright (C) 1995,1996,1997,1999,2000,2001,2005,2006,2008 Free Software Foundation, Inc.
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

/**
 * @page mutt_md5 Calculate the MD5 checksum of a buffer
 *
 * Calculate the MD5 cryptographic hash of a string, according to RFC1321.
 */

#include "config.h"
#include <stdbool.h>
#include <stddef.h> // IWYU pragma: keep
#include <stdio.h>
#include <string.h>
#include "md5.h"

#ifdef WORDS_BIGENDIAN
#define SWAP(n)                                                                \
  (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#else
#define SWAP(n) (n)
#endif

/** This array contains the bytes used to pad the buffer to the next
 * 64-byte boundary.  (RFC1321, 3.1: Step 1)  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ... */ };

/* These are the four functions used in the four steps of the MD5 algorithm
 * and defined in the RFC1321.  The first function is a little bit optimized
 * (as found in Colin Plumbs public domain implementation). */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF(d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

/**
 * mutt_md5_process_block - Process a block with MD5
 * @param buffer Buffer to hash
 * @param len    Length of buffer
 * @param md5ctx MD5 context
 *
 * Process LEN bytes of Buffer, accumulating context into MD5CTX.
 * LEN must be a multiple of 64.
 */
static void mutt_md5_process_block(const void *buffer, size_t len, struct Md5Ctx *md5ctx)
{
  md5_uint32 correct_words[16];
  const md5_uint32 *words = buffer;
  size_t nwords = len / sizeof(md5_uint32);
  const md5_uint32 *endp = words + nwords;
  md5_uint32 A = md5ctx->A;
  md5_uint32 B = md5ctx->B;
  md5_uint32 C = md5ctx->C;
  md5_uint32 D = md5ctx->D;

  /* First increment the byte count.  RFC1321 specifies the possible length of
   * the file up to 2^64 bits.  Here we only compute the number of bytes.  Do a
   * double word increment. */
  md5ctx->total[0] += len;
  if (md5ctx->total[0] < len)
    md5ctx->total[1]++; // LCOV_EXCL_LINE

  /* Process all bytes in the buffer with 64 bytes in each round of the loop. */
  while (words < endp)
  {
    md5_uint32 *cwp = correct_words;
    md5_uint32 save_A = A;
    md5_uint32 save_B = B;
    md5_uint32 save_C = C;
    md5_uint32 save_D = D;

    /* First round: using the given function, the context and a constant the
     * next context is computed.  Because the algorithms processing unit is a
     * 32-bit word and it is determined to work on words in little endian byte
     * order we perhaps have to change the byte order before the computation.
     * To reduce the work for the next steps we store the swapped words in the
     * array CORRECT_WORDS. */

#define OP(a, b, c, d, s, T)                                                   \
  do                                                                           \
  {                                                                            \
    a += FF(b, c, d) + (*cwp++ = SWAP(*words)) + T;                            \
    words++;                                                                   \
    CYCLIC(a, s);                                                              \
    a += b;                                                                    \
  } while (false)

/* It is unfortunate that C does not provide an operator for
 * cyclic rotation.  Hope the C compiler is smart enough. */
#define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

    /* Before we start, one word to the strange constants.
     * They are defined in RFC1321 as
     * T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64
     * Here is an equivalent invocation using Perl:
     * perl -e 'foreach(1..64){printf "0x%08x\n", int (4294967296 * abs (sin $_))}'
     */

    /* Round 1. */
    OP(A, B, C, D, 7, 0xd76aa478);
    OP(D, A, B, C, 12, 0xe8c7b756);
    OP(C, D, A, B, 17, 0x242070db);
    OP(B, C, D, A, 22, 0xc1bdceee);
    OP(A, B, C, D, 7, 0xf57c0faf);
    OP(D, A, B, C, 12, 0x4787c62a);
    OP(C, D, A, B, 17, 0xa8304613);
    OP(B, C, D, A, 22, 0xfd469501);
    OP(A, B, C, D, 7, 0x698098d8);
    OP(D, A, B, C, 12, 0x8b44f7af);
    OP(C, D, A, B, 17, 0xffff5bb1);
    OP(B, C, D, A, 22, 0x895cd7be);
    OP(A, B, C, D, 7, 0x6b901122);
    OP(D, A, B, C, 12, 0xfd987193);
    OP(C, D, A, B, 17, 0xa679438e);
    OP(B, C, D, A, 22, 0x49b40821);

/* For the second to fourth round we have the possibly swapped words
 * in CORRECT_WORDS.  Redefine the macro to take an additional first
 * argument specifying the function to use. */
#undef OP
#define OP(f, a, b, c, d, k, s, T)                                             \
  do                                                                           \
  {                                                                            \
    a += f(b, c, d) + correct_words[k] + T;                                    \
    CYCLIC(a, s);                                                              \
    a += b;                                                                    \
  } while (false)

    /* Round 2. */
    OP(FG, A, B, C, D, 1, 5, 0xf61e2562);
    OP(FG, D, A, B, C, 6, 9, 0xc040b340);
    OP(FG, C, D, A, B, 11, 14, 0x265e5a51);
    OP(FG, B, C, D, A, 0, 20, 0xe9b6c7aa);
    OP(FG, A, B, C, D, 5, 5, 0xd62f105d);
    OP(FG, D, A, B, C, 10, 9, 0x02441453);
    OP(FG, C, D, A, B, 15, 14, 0xd8a1e681);
    OP(FG, B, C, D, A, 4, 20, 0xe7d3fbc8);
    OP(FG, A, B, C, D, 9, 5, 0x21e1cde6);
    OP(FG, D, A, B, C, 14, 9, 0xc33707d6);
    OP(FG, C, D, A, B, 3, 14, 0xf4d50d87);
    OP(FG, B, C, D, A, 8, 20, 0x455a14ed);
    OP(FG, A, B, C, D, 13, 5, 0xa9e3e905);
    OP(FG, D, A, B, C, 2, 9, 0xfcefa3f8);
    OP(FG, C, D, A, B, 7, 14, 0x676f02d9);
    OP(FG, B, C, D, A, 12, 20, 0x8d2a4c8a);

    /* Round 3. */
    OP(FH, A, B, C, D, 5, 4, 0xfffa3942);
    OP(FH, D, A, B, C, 8, 11, 0x8771f681);
    OP(FH, C, D, A, B, 11, 16, 0x6d9d6122);
    OP(FH, B, C, D, A, 14, 23, 0xfde5380c);
    OP(FH, A, B, C, D, 1, 4, 0xa4beea44);
    OP(FH, D, A, B, C, 4, 11, 0x4bdecfa9);
    OP(FH, C, D, A, B, 7, 16, 0xf6bb4b60);
    OP(FH, B, C, D, A, 10, 23, 0xbebfbc70);
    OP(FH, A, B, C, D, 13, 4, 0x289b7ec6);
    OP(FH, D, A, B, C, 0, 11, 0xeaa127fa);
    OP(FH, C, D, A, B, 3, 16, 0xd4ef3085);
    OP(FH, B, C, D, A, 6, 23, 0x04881d05);
    OP(FH, A, B, C, D, 9, 4, 0xd9d4d039);
    OP(FH, D, A, B, C, 12, 11, 0xe6db99e5);
    OP(FH, C, D, A, B, 15, 16, 0x1fa27cf8);
    OP(FH, B, C, D, A, 2, 23, 0xc4ac5665);

    /* Round 4. */
    OP(FI, A, B, C, D, 0, 6, 0xf4292244);
    OP(FI, D, A, B, C, 7, 10, 0x432aff97);
    OP(FI, C, D, A, B, 14, 15, 0xab9423a7);
    OP(FI, B, C, D, A, 5, 21, 0xfc93a039);
    OP(FI, A, B, C, D, 12, 6, 0x655b59c3);
    OP(FI, D, A, B, C, 3, 10, 0x8f0ccc92);
    OP(FI, C, D, A, B, 10, 15, 0xffeff47d);
    OP(FI, B, C, D, A, 1, 21, 0x85845dd1);
    OP(FI, A, B, C, D, 8, 6, 0x6fa87e4f);
    OP(FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
    OP(FI, C, D, A, B, 6, 15, 0xa3014314);
    OP(FI, B, C, D, A, 13, 21, 0x4e0811a1);
    OP(FI, A, B, C, D, 4, 6, 0xf7537e82);
    OP(FI, D, A, B, C, 11, 10, 0xbd3af235);
    OP(FI, C, D, A, B, 2, 15, 0x2ad7d2bb);
    OP(FI, B, C, D, A, 9, 21, 0xeb86d391);

    /* Add the starting values of the context. */
    A += save_A;
    B += save_B;
    C += save_C;
    D += save_D;
  }

  /* Put checksum in context given as argument. */
  md5ctx->A = A;
  md5ctx->B = B;
  md5ctx->C = C;
  md5ctx->D = D;
}

/**
 * set_uint32 - Write a 32 bit number
 * @param cp Destination for data
 * @param v  Value to write
 *
 * Copy the 4 byte value from v into the memory location pointed to by *cp, If
 * your architecture allows unaligned access this is equivalent to
 * `*(md5_uint32*) cp = v`
 */
static inline void set_uint32(char *cp, md5_uint32 v)
{
  memcpy(cp, &v, sizeof(v));
}

/**
 * mutt_md5_read_ctx - Read from the context into a buffer
 * @param md5ctx MD5 context
 * @param resbuf Buffer for result
 * @retval ptr Results buffer
 *
 * Put result from MD5CTX in first 16 bytes following RESBUF.
 * The result must be in little endian byte order.
 */
static void *mutt_md5_read_ctx(const struct Md5Ctx *md5ctx, void *resbuf)
{
  if (!md5ctx || !resbuf)
    return NULL;

  char *r = resbuf;

  set_uint32(r + 0 * sizeof(md5ctx->A), SWAP(md5ctx->A));
  set_uint32(r + 1 * sizeof(md5ctx->B), SWAP(md5ctx->B));
  set_uint32(r + 2 * sizeof(md5ctx->C), SWAP(md5ctx->C));
  set_uint32(r + 3 * sizeof(md5ctx->D), SWAP(md5ctx->D));

  return resbuf;
}

/**
 * mutt_md5_init_ctx - Initialise the MD5 computation
 * @param md5ctx MD5 context
 *
 * RFC1321, 3.3: Step 3
 */
void mutt_md5_init_ctx(struct Md5Ctx *md5ctx)
{
  if (!md5ctx)
    return;

  md5ctx->A = 0x67452301;
  md5ctx->B = 0xefcdab89;
  md5ctx->C = 0x98badcfe;
  md5ctx->D = 0x10325476;

  md5ctx->total[0] = 0;
  md5ctx->total[1] = 0;
  md5ctx->buflen = 0;
}

/**
 * mutt_md5_finish_ctx - Process the remaining bytes in the buffer
 * @param md5ctx MD5 context
 * @param resbuf Buffer for result
 * @retval ptr Results buffer
 *
 * Process the remaining bytes in the internal buffer and the usual prologue
 * according to the standard and write the result to RESBUF.
 */
void *mutt_md5_finish_ctx(struct Md5Ctx *md5ctx, void *resbuf)
{
  if (!md5ctx)
    return NULL;

  /* Take yet unprocessed bytes into account. */
  md5_uint32 bytes = md5ctx->buflen;
  size_t size = (bytes < 56) ? 64 / 4 : 64 * 2 / 4;

  /* Now count remaining bytes. */
  md5ctx->total[0] += bytes;
  if (md5ctx->total[0] < bytes)
    md5ctx->total[1]++; // LCOV_EXCL_LINE

  /* Put the 64-bit file length in *bits* at the end of the buffer. */
  md5ctx->buffer[size - 2] = SWAP(md5ctx->total[0] << 3);
  md5ctx->buffer[size - 1] = SWAP((md5ctx->total[1] << 3) | (md5ctx->total[0] >> 29));

  memcpy(&((char *) md5ctx->buffer)[bytes], fillbuf, (size - 2) * 4 - bytes);

  /* Process last bytes. */
  mutt_md5_process_block(md5ctx->buffer, size * 4, md5ctx);

  return mutt_md5_read_ctx(md5ctx, resbuf);
}

/**
 * mutt_md5 - Calculate the MD5 hash of a NUL-terminated string
 * @param str String to hash
 * @param buf Buffer for result
 * @retval ptr Results buffer
 */
void *mutt_md5(const char *str, void *buf)
{
  if (!str)
    return NULL;

  return mutt_md5_bytes(str, strlen(str), buf);
}

/**
 * mutt_md5_bytes - Calculate the MD5 hash of a buffer
 * @param buffer  Buffer to hash
 * @param len     Length of buffer
 * @param resbuf  Buffer for result
 * @retval ptr Results buffer
 *
 * Compute MD5 message digest for LEN bytes beginning at Buffer.  The result is
 * always in little endian byte order, so that a byte-wise output yields to the
 * wanted ASCII representation of the message digest.
 */
void *mutt_md5_bytes(const void *buffer, size_t len, void *resbuf)
{
  struct Md5Ctx md5ctx = { 0 };

  /* Initialize the computation context. */
  mutt_md5_init_ctx(&md5ctx);

  /* Process whole buffer but last len % 64 bytes. */
  mutt_md5_process_bytes(buffer, len, &md5ctx);

  /* Put result in desired memory area. */
  return mutt_md5_finish_ctx(&md5ctx, resbuf);
}

/**
 * mutt_md5_process - Process a NUL-terminated string
 * @param str    String to process
 * @param md5ctx MD5 context
 */
void mutt_md5_process(const char *str, struct Md5Ctx *md5ctx)
{
  if (!str)
    return;

  mutt_md5_process_bytes(str, strlen(str), md5ctx);
}

/**
 * mutt_md5_process_bytes - Process a block of data
 * @param buf    Buffer to process
 * @param buflen Length of buffer
 * @param md5ctx MD5 context
 *
 * Starting with the result of former calls of this function (or the
 * initialization function update the context for the next BUFLEN bytes starting
 * at Buffer.  It is NOT required that BUFLEN is a multiple of 64.
 */
void mutt_md5_process_bytes(const void *buf, size_t buflen, struct Md5Ctx *md5ctx)
{
  if (!buf || !md5ctx)
    return;

  /* When we already have some bits in our internal buffer concatenate both
   * inputs first. */
  if (md5ctx->buflen != 0)
  {
    size_t left_over = md5ctx->buflen;
    size_t add = ((128 - left_over) > buflen) ? buflen : (128 - left_over);

    memcpy(&((char *) md5ctx->buffer)[left_over], buf, add);
    md5ctx->buflen += add;

    if (md5ctx->buflen > 64)
    {
      mutt_md5_process_block(md5ctx->buffer, md5ctx->buflen & ~63, md5ctx);

      md5ctx->buflen &= 63;
      /* The regions in the following copy operation can't overlap. */
      memcpy(md5ctx->buffer, &((char *) md5ctx->buffer)[(left_over + add) & ~63],
             md5ctx->buflen);
    }

    buf = (const char *) buf + add;
    buflen -= add;
  }

  /* Process available complete blocks. */
  if (buflen >= 64)
  {
#if !defined(_STRING_ARCH_unaligned)
#define alignof(type)                                                          \
  offsetof(                                                                    \
      struct {                                                                 \
        char c;                                                                \
        type x;                                                                \
      },                                                                       \
      x)
#define UNALIGNED_P(p) (((size_t) p) % alignof(md5_uint32) != 0)
    if (UNALIGNED_P(buf))
    {
      while (buflen > 64)
      {
        mutt_md5_process_block(memcpy(md5ctx->buffer, buf, 64), 64, md5ctx);
        buf = (const char *) buf + 64;
        buflen -= 64;
      }
    }
    else
#endif
    {
      mutt_md5_process_block(buf, buflen & ~63, md5ctx);
      buf = (const char *) buf + (buflen & ~63);
      buflen &= 63;
    }
  }

  /* Move remaining bytes in internal buffer. */
  if (buflen > 0)
  {
    size_t left_over = md5ctx->buflen;

    memcpy(&((char *) md5ctx->buffer)[left_over], buf, buflen);
    left_over += buflen;
    if (left_over >= 64)
    { // LCOV_EXCL_START
      mutt_md5_process_block(md5ctx->buffer, 64, md5ctx);
      left_over -= 64;
      memmove(md5ctx->buffer, &md5ctx->buffer[16], left_over);
    } // LCOV_EXCL_STOP
    md5ctx->buflen = left_over;
  }
}

/**
 * mutt_md5_toascii - Convert a binary MD5 digest into ASCII Hexadecimal
 * @param digest Binary MD5 digest
 * @param resbuf Buffer for the ASCII result
 *
 * @note refbuf must be at least 33 bytes long.
 */
void mutt_md5_toascii(const void *digest, char *resbuf)
{
  if (!digest || !resbuf)
    return;

  const unsigned char *c = digest;
  sprintf(resbuf, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
          c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8], c[9], c[10],
          c[11], c[12], c[13], c[14], c[15]);
}
