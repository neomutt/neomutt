/**
 * @file
 * Constants and macros for managing MIME encoding
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 * @page email_mime Constants and macros for managing MIME encoding
 *
 * Constants and macros for managing MIME encoding.
 */

#include "config.h"
#include "mime.h"

// clang-format off
/**
 * IndexHex - Lookup table for ASCII hex digits
 */
const int IndexHex[128] = {
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
    -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};
// clang-format on

/**
 * BodyTypes - Common MIME body types
 */
const char *const BodyTypes[] = {
  "x-unknown", "audio",     "application", "image", "message",
  "model",     "multipart", "text",        "video", "*",
};

/**
 * BodyEncodings - Common MIME body encodings
 */
const char *const BodyEncodings[] = {
  "x-unknown", "7bit",   "8bit",        "quoted-printable",
  "base64",    "binary", "x-uuencoded",
};

/**
 * MimeSpecials - Characters that need special treatment in MIME
 */
const char MimeSpecials[] = "@.,;:<>[]\\\"()?/= \t";
