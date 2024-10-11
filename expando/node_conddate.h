/**
 * @file
 * Expando Node for a Conditional Date
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

#ifndef MUTT_EXPANDO_NODE_CONDDATE_H
#define MUTT_EXPANDO_NODE_CONDDATE_H

struct ExpandoParseError;

/**
 * struct NodeCondDatePrivate - Private data for a Conditional Date - @extends ExpandoNode
 */
struct NodeCondDatePrivate
{
  int    count;         ///< Number of 'units' to count
  char   period;        ///< Units, e.g. 'd' Day or 'm' Month
};

struct ExpandoNode *node_conddate_parse(const char *str, const char **parsed_until, int did, int uid, struct ExpandoParseError *err);

#endif /* MUTT_EXPANDO_NODE_CONDDATE_H */
