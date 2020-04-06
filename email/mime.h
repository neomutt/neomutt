/**
 * @file
 * Constants and macros for managing MIME encoding
 *
 * @authors
 * Copyright (C) 1996-2000,2010 Michael R. Elkins <me@mutt.org>
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

#ifndef MUTT_EMAIL_MIME_H
#define MUTT_EMAIL_MIME_H

/**
 * enum ContentType - Content-Type
 */
enum ContentType
{
  TYPE_OTHER,       ///< Unknown Content-Type
  TYPE_AUDIO,       ///< Type: 'audio/*'
  TYPE_APPLICATION, ///< Type: 'application/*'
  TYPE_IMAGE,       ///< Type: 'image/*'
  TYPE_MESSAGE,     ///< Type: 'message/*'
  TYPE_MODEL,       ///< Type: 'model/*'
  TYPE_MULTIPART,   ///< Type: 'multipart/*'
  TYPE_TEXT,        ///< Type: 'text/*'
  TYPE_VIDEO,       ///< Type: 'video/*'
  TYPE_ANY,         ///< Type: '*' or '.*'
};

/**
 * enum ContentEncoding - Content-Transfer-Encoding
 */
enum ContentEncoding
{
  ENC_OTHER,            ///< Encoding unknown
  ENC_7BIT,             ///< 7-bit text
  ENC_8BIT,             ///< 8-bit text
  ENC_QUOTED_PRINTABLE, ///< Quoted-printable text
  ENC_BASE64,           ///< Base-64 encoded text
  ENC_BINARY,           ///< Binary
  ENC_UUENCODED,        ///< UUEncoded text
};

/**
 * enum ContentDisposition - Content-Disposition values
 */
enum ContentDisposition
{
  DISP_INLINE,    ///< Content is inline
  DISP_ATTACH,    ///< Content is attached
  DISP_FORM_DATA, ///< Content is form-data
  DISP_NONE,      ///< No preferred disposition
};

/* MIME encoding/decoding global vars */

extern const int IndexHex[128];
extern const char *const BodyTypes[];
extern const char *const BodyEncodings[];
extern const char MimeSpecials[];

#define hexval(ch) IndexHex[(unsigned int) (ch)]

#define is_multipart(body)                                                     \
  (((body)->type == TYPE_MULTIPART) ||                                         \
   (((body)->type == TYPE_MESSAGE) && ((body)->subtype) &&                     \
    ((strcasecmp((body)->subtype, "rfc822") == 0) ||                           \
     (strcasecmp((body)->subtype, "news") == 0) ||                             \
     (strcasecmp((body)->subtype, "global") == 0))))

#define TYPE(body)                                                             \
  ((body->type == TYPE_OTHER) && body->xtype ? body->xtype : BodyTypes[(body->type)])
#define ENCODING(x) BodyEncodings[(x)]

#endif /* MUTT_EMAIL_MIME_H */
