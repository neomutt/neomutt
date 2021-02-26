/**
 * @file
 * Representation of the body of an email
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

#ifndef MUTT_EMAIL_BODY_H
#define MUTT_EMAIL_BODY_H

#include "config.h"
#include <stdbool.h>
#include <time.h>
#include "parameter.h"

/**
 * struct Body - The body of an email
 */
struct Body
{
  // ---------------------------------------------------------------------------
  // Data that gets stored in the Header Cache

  unsigned int type        : 4;   ///< content-type primary type, #ContentType
  unsigned int encoding    : 3;   ///< content-transfer-encoding, #ContentEncoding
  unsigned int disposition : 2;   ///< content-disposition, #ContentDisposition
  bool badsig              : 1;   ///< Bad cryptographic signature (needed to check encrypted s/mime-signatures)
  bool force_charset       : 1;   ///< Send mode: don't adjust the character set when in send-mode.
  bool goodsig             : 1;   ///< Good cryptographic signature
  bool noconv              : 1;   ///< Don't do character set conversion
  bool use_disp            : 1;   ///< Content-Disposition uses filename= ?
  bool warnsig             : 1;   ///< Maybe good signature
#ifdef USE_AUTOCRYPT
  bool is_autocrypt        : 1;   ///< Flag autocrypt-decrypted messages for replying
#endif
  LOFF_T offset;                  ///< offset where the actual data begins
  LOFF_T length;                  ///< length (in bytes) of attachment

  char *description;              ///< content-description
  char *d_filename;               ///< filename to be used for the content-disposition header
                                  ///< If NULL, filename is used instead.
  char *filename;                 ///< When sending a message, this is the file to which this structure refers
  char *form_name;                ///< Content-Disposition form-data name param
  char *subtype;                  ///< content-type subtype
  char *xtype;                    ///< content-type if x-unknown
  struct ParameterList parameter; ///< Parameters of the content-type

  // ---------------------------------------------------------------------------
  // Management data - Runtime info and glue to hold the objects together

  bool unlink           : 1;      ///< If true, `filename` should be unlink()ed before free()ing this structure

  struct Content *content;        ///< Detailed info about the content of the attachment.
                                  ///< Used to determine what content-transfer-encoding is required when sending mail.
  struct Body *next;              ///< next attachment in the list
  struct Body *parts;             ///< parts of a multipart or message/rfc822
  struct Email *email;            ///< header information for message/rfc822
  struct AttachPtr *aptr;         ///< Menu information, used in recvattach.c
  struct Envelope *mime_headers;  ///< Memory hole protected headers
  time_t stamp;                   ///< Time stamp of last encoding update
  char *language;                 ///< content-language (RFC8255)
  char *charset;                  ///< Send mode: charset of attached file as stored on disk.
                                  ///< The charset used in the generated message is stored in parameter.
  long hdr_offset;                ///< Offset in stream where the headers begin.
                                  ///< This info is used when invoking metamail, where we need to send the headers of the attachment

  // ---------------------------------------------------------------------------
  // View data - Used by the GUI

  bool attach_qualifies : 1;      ///< This attachment should be counted
  bool collapsed        : 1;      ///< Used by recvattach
  bool deleted          : 1;      ///< Attachment marked for deletion
  bool nowrap           : 1;      ///< Do not wrap the output in the pager
  bool tagged           : 1;      ///< This attachment is tagged
  signed short attach_count;      ///< Number of attachments
};

bool         mutt_body_cmp_strict(const struct Body *b1, const struct Body *b2);
void         mutt_body_free      (struct Body **ptr);
char *       mutt_body_get_charset(struct Body *b, char *buf, size_t buflen);
struct Body *mutt_body_new       (void);

#endif /* MUTT_EMAIL_BODY_H */
