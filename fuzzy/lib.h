/**
 * @file
 * Fuzzy matching library
 *
 * @authors
 * Copyright (C) 2026 Richard Russon <rich@flatcap.org>
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
 * @page lib_fuzzy Fuzzy matching library
 *
 * High-performance fuzzy matching library for NeoMutt
 *
 * ## Overview
 *
 * This library provides fuzzy string matching capabilities optimized for
 * interactive use in NeoMutt. It's designed to help users quickly find items
 * by typing approximate or abbreviated input.
 *
 * ## Design Principles
 *
 * - **No heap allocation**: All operations use stack memory only
 * - **No global state**: Fully reentrant and thread-safe
 * - **Pure string matching**: No dependencies on mailbox, alias, or email structures
 * - **O(n) performance**: Linear time complexity for interactive responsiveness
 * - **Pluggable algorithms**: Easy to add new matching algorithms
 *
 * ## Use Cases
 *
 * - Mailbox/folder selection (e.g., "mlnd" → "mailinglists/neomutt-dev")
 * - Alias lookup (e.g., "rich" → "Richard Smith")
 * - Command completion (e.g., "set to" → "set timeout")
 * - Config variable lookup (e.g., "timeo" → "timeout")
 * - Email subject/sender filtering
 *
 * ## API Usage
 *
 * ```c
 * struct FuzzyOptions opts = { .smart_case = true };
 * struct FuzzyResult result = { 0 };
 * int score = fuzzy_match("mlnd", "mailinglists/neomutt-dev",
 *                         FUZZY_ALGO_SUBSEQ, &opts, &result);
 * if (score >= 0)
 *   printf("Match! Score: %d, Span: %d\n", result.score, result.span);
 * ```
 *
 * | File                   | Description                   |
 * | :--------------------- | :---------------------------- |
 * | fuzzy/fuzzy.c          | @subpage fuzzy_fuzzy          |
 * | fuzzy/subseq.c         | @subpage fuzzy_subseq         |
 */

#ifndef MUTT_FUZZY_LIB_H
#define MUTT_FUZZY_LIB_H

#include <stdbool.h>

/**
 * enum FuzzyAlgo - Fuzzy matching algorithm types
 */
enum FuzzyAlgo
{
  FUZZY_ALGO_SUBSEQ = 0, ///< Subsequence matching algorithm
  // extensible
  // FUZZY_ALGO_EDIT,
  // FUZZY_ALGO_TOKEN,
};

/**
 * struct FuzzyOptions - Options for fuzzy matching
 */
struct FuzzyOptions
{
  bool case_sensitive;   ///< Match case exactly
  bool smart_case;       ///< Auto case-sensitive if pattern has uppercase
  bool prefer_prefix;    ///< Extra weight for prefix matches
  int  max_pattern;      ///< Safety bound (<=0 = default 256, capped at 256)
};

/**
 * struct FuzzyResult - Result of a fuzzy match
 */
struct FuzzyResult
{
  int score;             ///< Score (<0 = no match)
  int span;              ///< Match span
  int start;             ///< First match position
  int end;               ///< Last match position
};

int fuzzy_match(const char *pattern, const char *candidate, enum FuzzyAlgo algo, const struct FuzzyOptions *opts, struct FuzzyResult *out);

#endif /* MUTT_FUZZY_LIB_H */
