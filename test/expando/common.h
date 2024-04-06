/**
 * @file
 * Shared test code for Expandos
 *
 * @authors
 * Copyright (C) 2023-2024 Tóth János <gomba007@gmail.com>
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

#ifndef TEST_EXPANDO_COMMON_H
#define TEST_EXPANDO_COMMON_H

#include "expando/lib.h"

void check_node_cond    (struct ExpandoNode *node);
void check_node_condbool(struct ExpandoNode *node, const char *expando);
void check_node_conddate(struct ExpandoNode *node, int count, char period);
void check_node_expando (struct ExpandoNode *node, const char *expando, const struct ExpandoFormat *fmt_expected);
void check_node_padding (struct ExpandoNode *node, const char *pad_char, enum ExpandoPadType pad_type);
void check_node_test    (struct ExpandoNode *node, const char *text);

struct ExpandoNode *parse_date(const char *s, const char **parsed_until, int did, int uid, ExpandoParserFlags flags, struct ExpandoParseError *error);

#endif /* TEST_EXPANDO_COMMON_H */
