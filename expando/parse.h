/**
 * @file
 * Expando Parsing
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

#ifndef MUTT_EXPANDO_PARSER_H
#define MUTT_EXPANDO_PARSER_H

#include <stdbool.h>
#include "node_text.h"

struct ExpandoDefinition;
struct ExpandoNode;

/**
 * struct ExpandoParseError - Buffer for parsing errors
 */
struct ExpandoParseError
{
  char        message[1024];  ///< Error message
  const char *position;       ///< Position of error in original string
};

struct ExpandoNode *node_parse_one(const char *str, NodeTextTermFlags term_chars,
                                   const struct ExpandoDefinition *defs,
                                   const char **parsed_until, struct ExpandoParseError *err);

bool node_parse_many(struct ExpandoNode *node_cont, const char *str,
                     NodeTextTermFlags term_chars, const struct ExpandoDefinition *defs,
                     const char **parsed_until, struct ExpandoParseError *err);

#endif /* MUTT_EXPANDO_PARSER_H */
