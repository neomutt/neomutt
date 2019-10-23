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

#ifndef MUTT_COPY_H
#define MUTT_COPY_H

#include "config.h"
#include <stdint.h>
#include <stdio.h>

struct Email;
struct Mailbox;

typedef uint16_t CopyMessageFlags;     ///< Flags for mutt_copy_message(), e.g. #MUTT_CM_NOHEADER
#define MUTT_CM_NO_FLAGS           0   ///< No flags are set
#define MUTT_CM_NOHEADER     (1 << 0)  ///< Don't copy the message header
#define MUTT_CM_PREFIX       (1 << 1)  ///< Quote the header and body
#define MUTT_CM_DECODE       (1 << 2)  ///< Decode the message body into text/plain
#define MUTT_CM_DISPLAY      (1 << 3)  ///< Output is displayed to the user
#define MUTT_CM_UPDATE       (1 << 4)  ///< Update structs on sync
#define MUTT_CM_WEED         (1 << 5)  ///< Weed message/rfc822 attachment headers
#define MUTT_CM_CHARCONV     (1 << 6)  ///< Perform character set conversions
#define MUTT_CM_PRINTING     (1 << 7)  ///< Printing the message - display light
#define MUTT_CM_REPLYING     (1 << 8)  ///< Replying the message
#define MUTT_CM_DECODE_PGP   (1 << 9)  ///< Used for decoding PGP messages
#define MUTT_CM_DECODE_SMIME (1 << 10) ///< Used for decoding S/MIME messages
#define MUTT_CM_VERIFY       (1 << 11) ///< Do signature verification
#define MUTT_CM_DECODE_CRYPT (MUTT_CM_DECODE_PGP | MUTT_CM_DECODE_SMIME)

typedef uint32_t CopyHeaderFlags;   ///< Flags for mutt_copy_header(), e.g. #CH_UPDATE
#define CH_NO_FLAGS             0   ///< No flags are set
#define CH_UPDATE         (1 << 0)  ///< Update the status and x-status fields?
#define CH_WEED           (1 << 1)  ///< Weed the headers?
#define CH_DECODE         (1 << 2)  ///< Do RFC2047 header decoding
#define CH_XMIT           (1 << 3)  ///< Transmitting this message? (Ignore Lines: and Content-Length:)
#define CH_FROM           (1 << 4)  ///< Retain the "From " message separator?
#define CH_PREFIX         (1 << 5)  ///< Quote header using #C_IndentString string?
#define CH_NOSTATUS       (1 << 6)  ///< Suppress the status and x-status fields
#define CH_REORDER        (1 << 7)  ///< Re-order output of headers (specified by 'hdr_order')
#define CH_NONEWLINE      (1 << 8)  ///< Don't output terminating newline after the header
#define CH_MIME           (1 << 9)  ///< Ignore MIME fields
#define CH_UPDATE_LEN     (1 << 10) ///< Update Lines: and Content-Length:
#define CH_TXTPLAIN       (1 << 11) ///< Generate text/plain MIME headers
#define CH_NOLEN          (1 << 12) ///< Don't write Content-Length: and Lines:
#define CH_WEED_DELIVERED (1 << 13) ///< Weed eventual Delivered-To headers
#define CH_FORCE_FROM     (1 << 14) ///< Give CH_FROM precedence over CH_WEED?
#define CH_NOQFROM        (1 << 15) ///< Ignore ">From " line
#define CH_UPDATE_IRT     (1 << 16) ///< Update In-Reply-To:
#define CH_UPDATE_REFS    (1 << 17) ///< Update References:
#define CH_DISPLAY        (1 << 18) ///< Display result to user
#define CH_UPDATE_LABEL   (1 << 19) ///< Update X-Label: from email->env->x_label?
#define CH_UPDATE_SUBJECT (1 << 20) ///< Update Subject: protected header update
#define CH_VIRTUAL        (1 << 21) ///< Write virtual header lines too

int mutt_copy_hdr(FILE *fp_in, FILE *fp_out, LOFF_T off_start, LOFF_T off_end, CopyHeaderFlags chflags, const char *prefix, int wraplen);

int mutt_copy_header(FILE *fp_in, struct Email *e, FILE *fp_out, CopyHeaderFlags chflags, const char *prefix, int wraplen);

int mutt_copy_message_fp(FILE *fp_out, FILE *fp_in,       struct Email *e, CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen);
int mutt_copy_message   (FILE *fp_out, struct Mailbox *m, struct Email *e, CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen);

int mutt_append_message(struct Mailbox *dest, struct Mailbox *src, struct Email *e, CopyMessageFlags cmflags, CopyHeaderFlags chflags);

#endif /* MUTT_COPY_H */
