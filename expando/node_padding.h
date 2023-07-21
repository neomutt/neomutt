/**
 * @file
 * Expando Node for Padding
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

#ifndef MUTT_EXPANDO_NODE_PADDING_H
#define MUTT_EXPANDO_NODE_PADDING_H

#include "definition.h"

struct ExpandoNode;
struct ExpandoParseError;

/**
 * enum ExpandoPadType - Padding type
 *
 * Padding occurs between two sides, left and right.
 * The type of Padding, soft or hard, refers to how the left-hand-side will
 * react if there's too little space.
 *
 * Hard padding: The left-hand-side will fixed and the right-hand-side will be truncated.
 * Soft padding: The right-hand-side will be fixed and the left-hand-side will be truncated.
 */
enum ExpandoPadType
{
  EPT_FILL_EOL,     ///< Fill to the end-of-line
  EPT_HARD_FILL,    ///< Hard-fill: left-hand-side will be truncated
  EPT_SOFT_FILL,    ///< Soft-fill: right-hand-side will be truncated
};

/**
 * enum ENPad - Names for the Padding's children
 *
 * Padding has two children: Left and Right.
 */
enum ENPad
{
  ENP_LEFT,           ///< Index of Left-Hand Nodes
  ENP_RIGHT,          ///< Index of Right-Hand Nodes
};

/**
 * struct NodePaddingPrivate - Private data for a Padding Node
 */
struct NodePaddingPrivate
{
  enum ExpandoPadType  pad_type;        ///< Padding type
};

struct ExpandoNode *node_padding_parse(const char *s, const char **parsed_until, int did, int uid, ExpandoParserFlags flags, struct ExpandoParseError *error);

void node_padding_repad(struct ExpandoNode **parent);

#endif /* MUTT_EXPANDO_NODE_PADDING_H */
