/**
 * @file
 * Fuzzy matching dispatcher
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
 * @page fuzzy_fuzzy Fuzzy matching dispatcher
 *
 * Algorithm dispatcher for fuzzy matching
 *
 * This file implements the main entry point for the fuzzy matching library.
 * It dispatches to specific algorithm implementations based on the requested
 * FuzzyAlgo type.
 *
 * ## Architecture
 *
 * The dispatcher pattern allows multiple matching algorithms to coexist
 * without breaking the API. Currently implemented:
 *
 * - FUZZY_ALGO_SUBSEQ: Subsequence matching (FZF-style)
 *
 * Future algorithms could include:
 * - FUZZY_ALGO_EDIT: Edit distance (Levenshtein/Damerau-Levenshtein)
 * - FUZZY_ALGO_TOKEN: Token-based matching for multi-word strings
 */

#include "config.h"
#include "fuzzy.h"
#include "lib.h"

/**
 * fuzzy_match - Perform fuzzy matching
 * @param pattern   Pattern to match
 * @param candidate Candidate string to match against
 * @param algo      Fuzzy matching algorithm to use
 * @param opts      Fuzzy matching options
 * @param out       Output result structure
 * @retval >=0 Match score
 * @retval  -1 Error or no match
 */
int fuzzy_match(const char *pattern, const char *candidate, enum FuzzyAlgo algo,
                const struct FuzzyOptions *opts, struct FuzzyResult *out)
{
  if (!pattern || !candidate)
    return -1;

  if (algo == FUZZY_ALGO_SUBSEQ)
    return fuzzy_subseq_match(pattern, candidate, opts, out);

  return -1;
}
