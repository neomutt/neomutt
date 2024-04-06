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

#include "node_condition.h"

struct ExpandoDefinition;

/**
 * struct ExpandoParseError - Buffer for parsing errors
 */
struct ExpandoParseError
{
  char        message[256];   ///< Error message
  const char *position;       ///< Position of error in original string
};

struct ExpandoNode *node_parse(const char *str, const char *end,
                               enum ExpandoConditionStart condition_start,
                               const char **parsed_until,
                               const struct ExpandoDefinition *defs,
                               struct ExpandoParseError *error);

#endif /* MUTT_EXPANDO_PARSER_H */
