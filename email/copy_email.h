/**
 * @file
 * Duplicate the structure of an entire email
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018-2021 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2021 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef MUTT_EMAIL_COPY_EMAIL_H
#define MUTT_EMAIL_COPY_EMAIL_H

#include "config.h"
#include <stdint.h>
#include <stdio.h>

struct Email;
struct Mailbox;
struct Message;

/**
 * enum CopyMessageFlag - Flags for mutt_copy_message(), e.g. #MUTT_CM_NOHEADER
 */
enum CopyMessageFlag
{
  MUTT_CM_NONE         =       0,  ///< No flags are set
  MUTT_CM_NOHEADER     = 1U <<  0, ///< Don't copy the message header
  MUTT_CM_PREFIX       = 1U <<  1, ///< Quote the header and body
  MUTT_CM_DECODE       = 1U <<  2, ///< Decode the message body into text/plain
  MUTT_CM_DISPLAY      = 1U <<  3, ///< Output is displayed to the user
  MUTT_CM_UPDATE       = 1U <<  4, ///< Update structs on sync
  MUTT_CM_WEED         = 1U <<  5, ///< Weed message/rfc822 attachment headers
  MUTT_CM_CHARCONV     = 1U <<  6, ///< Perform character set conversions
  MUTT_CM_PRINTING     = 1U <<  7, ///< Printing the message - display light
  MUTT_CM_REPLYING     = 1U <<  8, ///< Replying the message
  MUTT_CM_DECODE_PGP   = 1U <<  9, ///< Used for decoding PGP messages
  MUTT_CM_DECODE_SMIME = 1U << 10, ///< Used for decoding S/MIME messages
  MUTT_CM_VERIFY       = 1U << 11, ///< Do signature verification
};
typedef uint16_t CopyMessageFlags;

/// Combination flag for decoding any kind of cryptography (PGP or S/MIME)
#define MUTT_CM_DECODE_CRYPT (MUTT_CM_DECODE_PGP | MUTT_CM_DECODE_SMIME)

/**
 * enum CopyHeaderFlag - Flags for mutt_copy_header(), e.g. #CH_UPDATE
 */
enum CopyHeaderFlag
{
  CH_NONE           =       0,  ///< No flags are set
  CH_UPDATE         = 1U <<  0, ///< Update the status and x-status fields?
  CH_WEED           = 1U <<  1, ///< Weed the headers?
  CH_DECODE         = 1U <<  2, ///< Do RFC2047 header decoding
  CH_XMIT           = 1U <<  3, ///< Transmitting this message? (Ignore Lines: and Content-Length:)
  CH_FROM           = 1U <<  4, ///< Retain the "From " message separator?
  CH_PREFIX         = 1U <<  5, ///< Quote header using `$indent_string` string?
  CH_NOSTATUS       = 1U <<  6, ///< Suppress the status and x-status fields
  CH_REORDER        = 1U <<  7, ///< Re-order output of headers (specified by 'header-order')
  CH_NONEWLINE      = 1U <<  8, ///< Don't output terminating newline after the header
  CH_MIME           = 1U <<  9, ///< Ignore MIME fields
  CH_UPDATE_LEN     = 1U << 10, ///< Update Lines: and Content-Length:
  CH_TXTPLAIN       = 1U << 11, ///< Generate text/plain MIME headers
  CH_NOLEN          = 1U << 12, ///< Don't write Content-Length: and Lines:
  CH_WEED_DELIVERED = 1U << 13, ///< Weed eventual Delivered-To headers
  CH_FORCE_FROM     = 1U << 14, ///< Give CH_FROM precedence over CH_WEED?
  CH_NOQFROM        = 1U << 15, ///< Ignore ">From " line
  CH_UPDATE_IRT     = 1U << 16, ///< Update In-Reply-To:
  CH_UPDATE_REFS    = 1U << 17, ///< Update References:
  CH_DISPLAY        = 1U << 18, ///< Display result to user
  CH_UPDATE_LABEL   = 1U << 19, ///< Update X-Label: from email->env->x_label?
  CH_UPDATE_SUBJECT = 1U << 20, ///< Update Subject: protected header update
  CH_VIRTUAL        = 1U << 21, ///< Write virtual header lines too
};
typedef uint32_t CopyHeaderFlags;

int mutt_copy_hdr(FILE *fp_in, FILE *fp_out, LOFF_T off_start, LOFF_T off_end, CopyHeaderFlags chflags, const char *prefix, int wraplen);

int mutt_copy_header(FILE *fp_in, struct Email *e, FILE *fp_out, CopyHeaderFlags chflags, const char *prefix, int wraplen);

int mutt_copy_message_fp(FILE *fp_out, FILE *fp_in, struct Email *e, CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen);
int mutt_copy_message   (FILE *fp_out, struct Email *e, struct Message *msg, CopyMessageFlags cmflags, CopyHeaderFlags chflags, int wraplen);

int mutt_append_message(struct Mailbox *m_dst, struct Mailbox *m_src, struct Email *e, struct Message *msg, CopyMessageFlags cmflags, CopyHeaderFlags chflags);

#endif /* MUTT_EMAIL_COPY_EMAIL_H */
