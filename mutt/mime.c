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
 * @page mime Constants and macros for managing MIME encoding
 *
 * Constants and macros for managing MIME encoding.
 */

#include "config.h"
#include <stdbool.h>
#include "mime.h"
#include "memory.h"
#include "string2.h"

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

/**
 * BodyLanguages - Common MIME body languages
 */
const char *const BodyLanguages[] = {
  "cs-cz",
  "da", "da-dk",
  "de", "de-at", "de-ch", "de-de",
  "el", "el-gr",
  "en", "en-au", "en-ca", "en-dk", "en-fi", "en-gb", "en-id", "en-ie", "en-il",
  "en-in", "en-my", "en-no", "en-nz", "en-ph", "en-sg", "en-th", "en-us", "en-ww",
  "en-xa", "en-za",
  "es", "es-ar", "es-cl", "es-co", "es-es", "es-la", "es-mx", "es-pr", "es-us",
  "fi", "fi-fi",
  "fr", "fr-be", "fr-ca", "fr-ch", "fr-fr", "fr-lu",
  "he", "he-il",
  "hu", "hu-hu",
  "it", "it-it",
  "ja", "ja-jp",
  "ko", "ko-kr",
  "nl", "nl-be", "nl-nl",
  "no", "no-no",
  "pl", "pl-pl",
  "pt", "pt-br", "pt-pt",
  "ru", "ru-ru",
  "sl", "sl-sl",
  "sv", "sv-se",
  "tr", "tr-tr",
  "zh", "zh-cn", "zh-hk", "zh-tw",
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

/**
 * mutt_mime_valid_language - Is it a valid language code?
 * @param lang Language code to check (e.g., en)
 * @retval true Valid language code
 *
 * @note Currently this checking does not strictly adhere to RFC3282 and
 * RFC5646. See #BodyLanguages for all supported languages.
 */
bool mutt_mime_valid_language(const char *lang)
{
  for (size_t i = 0; i < mutt_array_size(BodyLanguages); i++)
  {
    if (mutt_str_strcasecmp(lang, BodyLanguages[i]) == 0)
      return true;
  }
  return false;
}
