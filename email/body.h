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
  char *xtype;                    ///< content-type if x-unknown
  char *subtype;                  ///< content-type subtype
  char *language;                 ///< content-language (RFC8255)
  struct ParameterList parameter; ///< parameters of the content-type
  char *description;              ///< content-description
  char *form_name;                ///< Content-Disposition form-data name param
  long hdr_offset;                ///< Offset in stream where the headers begin.
                                  ///< This info is used when invoking metamail, where we need to send the headers of the attachment
  LOFF_T offset;                  ///< offset where the actual data begins
  LOFF_T length;                  ///< length (in bytes) of attachment
  char *filename;                 ///< when sending a message, this is the file to which this structure refers
  char *d_filename;               ///< filename to be used for the content-disposition header.
                                  ///< If NULL, filename is used instead.
  char *charset;                  ///< Send mode: charset of attached file as stored on disk.
                                  ///< The charset used in the generated message is stored in parameter.
  struct Content *content;        ///< Detailed info about the content of the attachment.
                                  ///< Used to determine what content-transfer-encoding is required when sending mail.
  struct Body *next;              ///< next attachment in the list
  struct Body *parts;             ///< parts of a multipart or message/rfc822
  struct Email *email;            ///< header information for message/rfc822

  struct AttachPtr *aptr;         ///< Menu information, used in recvattach.c

  signed short attach_count;      ///< Number of attachments

  time_t stamp;                   ///< Time stamp of last encoding update

  struct Envelope *mime_headers;  ///< Memory hole protected headers

  unsigned int type : 4;          ///< content-type primary type, #ContentType
  unsigned int encoding : 3;      ///< content-transfer-encoding, #ContentEncoding
  unsigned int disposition : 2;   ///< content-disposition, #ContentDisposition
  bool use_disp : 1;              ///< Content-Disposition uses filename= ?
  bool unlink : 1;                ///< If true, `filename` should be unlink()ed before free()ing this structure
  bool tagged : 1;                ///< This attachment is tagged
  bool deleted : 1;               ///< Attachment marked for deletion

  bool noconv : 1;                ///< Don't do character set conversion
  bool force_charset : 1;         ///< Send mode: don't adjust the character set when in send-mode.
  bool goodsig : 1;               ///< Good cryptographic signature
  bool warnsig : 1;               ///< Maybe good signature
  bool badsig : 1;                ///< Bad cryptographic signature (needed to check encrypted s/mime-signatures)
#ifdef USE_AUTOCRYPT
  bool is_autocrypt : 1;          ///< Flag autocrypt-decrypted messages for replying
#endif

  bool collapsed : 1;             ///< Used by recvattach
  bool attach_qualifies : 1;      ///< This attachment should be counted
};

bool         mutt_body_cmp_strict(const struct Body *b1, const struct Body *b2);
void         mutt_body_free      (struct Body **ptr);
char *       mutt_body_get_charset(struct Body *b, char *buf, size_t buflen);
struct Body *mutt_body_new       (void);

#endif /* MUTT_EMAIL_BODY_H */
