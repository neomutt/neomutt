/**
 * @file
 * Duplicate the structure of an entire email
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
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

#ifndef _MUTT_COPY_H
#define _MUTT_COPY_H

#include <stdio.h>

struct Body;
struct Header;
struct Context;

/**< flags to _mutt_copy_message */
#define MUTT_CM_NOHEADER     (1 << 0) /**< don't copy the message header */
#define MUTT_CM_PREFIX       (1 << 1) /**< quote the message */
#define MUTT_CM_DECODE       (1 << 2) /**< decode the message body into text/plain */
#define MUTT_CM_DISPLAY      (1 << 3) /**< output is displayed to the user */
#define MUTT_CM_UPDATE       (1 << 4) /**< update structs on sync */
#define MUTT_CM_WEED         (1 << 5) /**< weed message/rfc822 attachment headers */
#define MUTT_CM_CHARCONV     (1 << 6) /**< perform character set conversions */
#define MUTT_CM_PRINTING     (1 << 7) /**< printing the message - display light */
#define MUTT_CM_REPLYING     (1 << 8) /**< replying the message */
#define MUTT_CM_DECODE_PGP   (1 << 9)  /**< used for decoding PGP messages */
#define MUTT_CM_DECODE_SMIME (1 << 10) /**< used for decoding S/MIME messages */
#define MUTT_CM_DECODE_CRYPT (MUTT_CM_DECODE_PGP | MUTT_CM_DECODE_SMIME)
#define MUTT_CM_VERIFY       (1 << 11) /**< do signature verification */

/* flags for mutt_copy_header() */
#define CH_UPDATE         (1 << 0)  /**< update the status and x-status fields? */
#define CH_WEED           (1 << 1)  /**< weed the headers? */
#define CH_DECODE         (1 << 2)  /**< do RFC1522 decoding? */
#define CH_XMIT           (1 << 3)  /**< transmitting this message? */
#define CH_FROM           (1 << 4)  /**< retain the "From " message separator? */
#define CH_PREFIX         (1 << 5)  /**< use Prefix string? */
#define CH_NOSTATUS       (1 << 6)  /**< suppress the status and x-status fields */
#define CH_REORDER        (1 << 7)  /**< Re-order output of headers */
#define CH_NONEWLINE      (1 << 8)  /**< don't output terminating newline */
#define CH_MIME           (1 << 9)  /**< ignore MIME fields */
#define CH_UPDATE_LEN     (1 << 10) /**< update Lines: and Content-Length: */
#define CH_TXTPLAIN       (1 << 11) /**< generate text/plain MIME headers */
#define CH_NOLEN          (1 << 12) /**< don't write Content-Length: and Lines: */
#define CH_WEED_DELIVERED (1 << 13) /**< weed eventual Delivered-To headers */
#define CH_FORCE_FROM     (1 << 14) /**< give CH_FROM precedence over CH_WEED? */
#define CH_NOQFROM        (1 << 15) /**< ignore ">From " line */
#define CH_UPDATE_IRT     (1 << 16) /**< update In-Reply-To: */
#define CH_UPDATE_REFS    (1 << 17) /**< update References: */
#define CH_DISPLAY        (1 << 18) /**< display result to user */
#define CH_UPDATE_LABEL   (1 << 19) /**< update X-Label: from hdr->env->x_label? */
#define CH_VIRTUAL        (1 << 20) /**< write virtual header lines too */

int mutt_copy_hdr(FILE *in, FILE *out, LOFF_T off_start, LOFF_T off_end,
                  int flags, const char *prefix);

int mutt_copy_header(FILE *in, struct Header *h, FILE *out, int flags, const char *prefix);

int _mutt_copy_message(FILE *fpout, FILE *fpin, struct Header *hdr, struct Body *body,
                       int flags, int chflags);

int mutt_copy_message(FILE *fpout, struct Context *src, struct Header *hdr, int flags, int chflags);

int mutt_append_message(struct Context *dest, struct Context *src, struct Header *hdr, int cmflags, int chflags);

#endif /* _MUTT_COPY_H */
