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

#ifndef _EMAIL_MIME_H
#define _EMAIL_MIME_H

/**
 * enum ContentType - Content-Type
 */
enum ContentType
{
  TYPE_OTHER,
  TYPE_AUDIO,
  TYPE_APPLICATION,
  TYPE_IMAGE,
  TYPE_MESSAGE,
  TYPE_MODEL,
  TYPE_MULTIPART,
  TYPE_TEXT,
  TYPE_VIDEO,
  TYPE_ANY
};

/**
 * enum ContentEncoding - Content-Transfer-Encoding
 */
enum ContentEncoding
{
  ENC_OTHER,
  ENC_7BIT,
  ENC_8BIT,
  ENC_QUOTED_PRINTABLE,
  ENC_BASE64,
  ENC_BINARY,
  ENC_UUENCODED
};

/**
 * enum ContentDisposition - Content-Disposition values
 */
enum ContentDisposition
{
  DISP_INLINE,
  DISP_ATTACH,
  DISP_FORM_DATA,
  DISP_NONE /* no preferred disposition */
};

/* MIME encoding/decoding global vars */

extern const int IndexHex[128];
extern const char *const BodyTypes[];
extern const char *const BodyEncodings[];
extern const char MimeSpecials[];

#define hexval(c) IndexHex[(unsigned int) (c)]

#define is_multipart(x)                                                             \
  (((x)->type == TYPE_MULTIPART) || (((x)->type == TYPE_MESSAGE) && ((x)->subtype) && \
                                    ((strcasecmp((x)->subtype, "rfc822") == 0) ||   \
                                     (strcasecmp((x)->subtype, "news") == 0))))

#define TYPE(X)                                                                \
  ((X->type == TYPE_OTHER) && (X->xtype != NULL) ? X->xtype : BodyTypes[(X->type)])
#define ENCODING(X) BodyEncodings[(X)]

#endif /* _EMAIL_MIME_H */
