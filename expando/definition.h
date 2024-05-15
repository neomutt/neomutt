/**
 * @file
 * Define an Expando format string
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
 * Copyright (C) 2023-2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_EXPANDO_DEFINITION_H
#define MUTT_EXPANDO_DEFINITION_H

#include <stdbool.h>
#include <stdint.h>

struct ExpandoParseError;

/**
 * enum ExpandoDataType - Type of data
 */
enum ExpandoDataType
{
  E_TYPE_STRING = 0,    ///< Data is a string
  E_TYPE_NUMBER,        ///< Data is numeric
};

typedef uint8_t ExpandoParserFlags;       ///< Flags for expando_parse(), e.g. #EP_CONDITIONAL
#define EP_NO_FLAGS                 0     ///< No flags are set
#define EP_CONDITIONAL        (1 << 0)    ///< Expando is being used as a condition
#define EP_NO_CUSTOM_PARSE    (1 << 1)    ///< Don't use the custom parser

/**
 * struct ExpandoDefinition - Definition of a format string
 *
 * This defines which expandos are allowed in a format string.
 */
struct ExpandoDefinition
{
  const char           *short_name;      ///< Short Expando name, e.g. "n"
  const char           *long_name;       ///< Long Expando name, e.g. "name"
  short                 did;             ///< Domain ID
  short                 uid;             ///< Unique ID in domain
  enum ExpandoDataType  data_type;       ///< Type of data

  /**
   * @defgroup expando_parse_api Expando Parse API
   *
   * parse - Custom function to parse a format string into a Node
   * @param[in]     str          Format String to parse
   * @param[in,out] parsed_until First character after the parsed string
   * @param[in]     did          Domain ID of the data
   * @param[in]     uid          Unique ID of the data
   * @param[out]    err          Place for error message
   * @retval ptr Parsed Node
   */
  struct ExpandoNode *(*parse)(const char *str, const char **parsed_until, int did, int uid, ExpandoParserFlags flags, struct ExpandoParseError *err);
};

#endif /* MUTT_EXPANDO_DEFINITION_H */
