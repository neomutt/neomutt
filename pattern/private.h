/**
 * @file
 * Shared constants/structs that are private to libpattern
 *
 * @authors
 * Copyright (C) 2020-2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_PATTERN_PRIVATE_H
#define MUTT_PATTERN_PRIVATE_H

#include <stdbool.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "lib.h"

struct MailboxView;

/**
 * ExpandoDataPattern - Expando UIDs for Patterns
 *
 * @sa ED_PATTERN, ExpandoDomain
 */
enum ExpandoDataPattern
{
  ED_PAT_DESCRIPTION = 1,      ///< PatternEntry.desc
  ED_PAT_EXPRESSION,           ///< PatternEntry.expr
  ED_PAT_NUMBER,               ///< PatternEntry.num
};

/**
 * enum PatternEat - Function to process pattern arguments
 *
 * Values for PatternFlags.eat_arg
 */
enum PatternEat
{
  EAT_NONE,          ///< No arguments required
  EAT_REGEX,         ///< Process a regex
  EAT_DATE,          ///< Process a date (range)
  EAT_RANGE,         ///< Process a number (range)
  EAT_MESSAGE_RANGE, ///< Process a message number (range)
  EAT_QUERY,         ///< Process a query string
};

/**
 * struct PatternFlags - Mapping between user character and internal constant
 */
struct PatternFlags
{
  char tag;                ///< Character used to represent this operation, e.g. 'A' for '~A'
  int op;                  ///< Operation to perform, e.g. #MUTT_PAT_SCORE
  PatternCompFlags flags;  ///< Pattern flags, e.g. #MUTT_PC_FULL_MSG

  enum PatternEat eat_arg; ///< Type of function needed to parse flag, e.g. #EAT_DATE
  char *desc;              ///< Description of flag
};

/**
 * struct RangeRegex - Regular expression representing a range
 */
struct RangeRegex
{
  const char *raw; ///< Regex as string
  int lgrp;        ///< Paren group matching the left side
  int rgrp;        ///< Paren group matching the right side
  bool ready;      ///< Compiled yet?
  regex_t cooked;  ///< Compiled form
};

/**
 * enum RangeType - Type of range
 */
enum RangeType
{
  RANGE_K_REL,  ///< Relative range
  RANGE_K_ABS,  ///< Absolute range
  RANGE_K_LT,   ///< Less-than range
  RANGE_K_GT,   ///< Greater-than range
  RANGE_K_BARE, ///< Single symbol
  /* add new ones HERE */
  RANGE_K_INVALID, ///< Range is invalid
};

/* The regexes in a modern format */
#define RANGE_NUM_RX      "([[:digit:]]+|0x[[:xdigit:]]+)[MmKk]?"
#define RANGE_REL_SLOT_RX "[[:blank:]]*([.^$]|-?" RANGE_NUM_RX ")?[[:blank:]]*"
#define RANGE_REL_RX      "^" RANGE_REL_SLOT_RX "," RANGE_REL_SLOT_RX

/* Almost the same, but no negative numbers allowed */
#define RANGE_ABS_SLOT_RX "[[:blank:]]*([.^$]|" RANGE_NUM_RX ")?[[:blank:]]*"
#define RANGE_ABS_RX      "^" RANGE_ABS_SLOT_RX "-" RANGE_ABS_SLOT_RX

/* First group is intentionally empty */
#define RANGE_LT_RX "^()[[:blank:]]*(<[[:blank:]]*" RANGE_NUM_RX ")[[:blank:]]*"
#define RANGE_GT_RX "^()[[:blank:]]*(>[[:blank:]]*" RANGE_NUM_RX ")[[:blank:]]*"

/* Single group for min and max */
#define RANGE_BARE_RX "^[[:blank:]]*([.^$]|" RANGE_NUM_RX ")[[:blank:]]*"
#define RANGE_RX_GROUPS 5

#define RANGE_DOT    '.'
#define RANGE_CIRCUM '^'
#define RANGE_DOLLAR '$'
#define RANGE_LT     '<'
#define RANGE_GT     '>'

/**
 * enum RangeSide - Which side of the range
 */
enum RangeSide
{
  RANGE_S_LEFT,  ///< Left side of range
  RANGE_S_RIGHT, ///< Right side of range
};

/**
 * email_msgno - Helper to get the Email's message number
 * @param e Email
 * @retval num Message number
 */
static inline int email_msgno(struct Email *e)
{
  return e->msgno + 1;
}

#define MUTT_MAXRANGE -1

extern struct RangeRegex RangeRegexes[];
extern const struct PatternFlags Flags[];

const struct PatternFlags *lookup_op(int op);
const struct PatternFlags *lookup_tag(char tag);
bool eval_date_minmax(struct Pattern *pat, const char *s, struct Buffer *err);
bool eat_message_range(struct Pattern *pat, PatternCompFlags flags, struct Buffer *s, struct Buffer *err, struct MailboxView *mv);

#endif /* MUTT_PATTERN_PRIVATE_H */
