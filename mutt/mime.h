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

#ifndef _MUTT_MIME_H
#define _MUTT_MIME_H

#include <stdbool.h>

/**
 * enum ContentType - Content-Type
 */
enum ContentType
{
  TYPEOTHER,
  TYPEAUDIO,
  TYPEAPPLICATION,
  TYPEIMAGE,
  TYPEMESSAGE,
  TYPEMODEL,
  TYPEMULTIPART,
  TYPETEXT,
  TYPEVIDEO,
  TYPEANY
};

/**
 * enum ContentEncoding - Content-Transfer-Encoding
 */
enum ContentEncoding
{
  ENCOTHER,
  ENC7BIT,
  ENC8BIT,
  ENCQUOTEDPRINTABLE,
  ENCBASE64,
  ENCBINARY,
  ENCUUENCODED
};

/**
 * enum ContentDisposition - Content-Disposition values
 */
enum ContentDisposition
{
  DISPINLINE,
  DISPATTACH,
  DISPFORMDATA,
  DISPNONE /* no preferred disposition */
};

/* MIME encoding/decoding global vars */

extern const int IndexHex[128];
extern const char *const BodyTypes[];
extern const char *const BodyEncodings[];
extern const char *const BodyLanguages[];
extern const char MimeSpecials[];

bool mutt_mime_valid_language(const char *lang);

#define hexval(c) IndexHex[(unsigned int) (c)]

#define is_multipart(x)                                                             \
  (((x)->type == TYPEMULTIPART) || (((x)->type == TYPEMESSAGE) && ((x)->subtype) && \
                                    ((strcasecmp((x)->subtype, "rfc822") == 0) ||   \
                                     (strcasecmp((x)->subtype, "news") == 0))))

#define TYPE(X)                                                                \
  ((X->type == TYPEOTHER) && (X->xtype != NULL) ? X->xtype : BodyTypes[(X->type)])
#define ENCODING(X) BodyEncodings[(X)]

#endif /* _MUTT_MIME_H */
