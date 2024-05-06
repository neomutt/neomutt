/**
 * @file
 * Expando Node for a Condition
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

#ifndef MUTT_EXPANDO_NODE_CONDITION_H
#define MUTT_EXPANDO_NODE_CONDITION_H

struct ExpandoNode;

/**
 * ExpandoConditionStart - Signals node_parse() if the parsing started in a conditional statement or not
 *
 * Easier to read than a simple true, false.
 */
enum ExpandoConditionStart
{
  CON_NO_CONDITION, ///< Parser is not currently in a condition
  CON_START         ///< Parser is working on a condition
};

/**
 * enum ENCondition - Names for the Condition's children
 *
 * A Condition has three children:
 * - Expando that acts as the Condition
 * - Tree of Expandos for the true case
 * - Tree of Expandos for the false case
 */
enum ENCondition
{
  ENC_CONDITION,      ///< Index of Condition Node
  ENC_TRUE,           ///< Index of True Node
  ENC_FALSE,          ///< Index of False Node
};

struct ExpandoNode *node_condition_new(struct ExpandoNode *condition, struct ExpandoNode *node_true, struct ExpandoNode *node_false);

#endif /* MUTT_EXPANDO_NODE_CONDITION_H */
