/**
 * @file
 * Test code for fuzzy_match()
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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <stdbool.h>
#include <string.h>
#include "fuzzy/lib.h"

void test_fuzzy_match(void)
{
  // NULL parameter checks
  {
    struct FuzzyResult result = { 0 };
    struct FuzzyOptions opts = { 0 };

    int score = fuzzy_match(NULL, "candidate", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Expected: -1");
    TEST_MSG("Actual:   %d", score);

    score = fuzzy_match("pattern", NULL, FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Expected: -1");
    TEST_MSG("Actual:   %d", score);

    score = fuzzy_match(NULL, NULL, FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Expected: -1");
    TEST_MSG("Actual:   %d", score);
  }

  // Basic match
  {
    struct FuzzyResult result = { 0 };
    struct FuzzyOptions opts = { 0 };

    int score = fuzzy_match("box", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Expected: >=0");
    TEST_MSG("Actual:   %d", score);
    TEST_CHECK(result.score >= 0);
    TEST_CHECK(result.start >= 0);
    TEST_CHECK(result.end >= result.start);
  }

  // No match
  {
    struct FuzzyResult result = { 0 };
    struct FuzzyOptions opts = { 0 };

    int score = fuzzy_match("xyz", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Expected: -1");
    TEST_MSG("Actual:   %d", score);
  }

  // Empty pattern
  {
    struct FuzzyResult result = { 0 };
    struct FuzzyOptions opts = { 0 };

    int score = fuzzy_match("", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Expected: -1");
    TEST_MSG("Actual:   %d", score);
  }

  // NULL options (should use defaults)
  {
    struct FuzzyResult result = { 0 };

    int score = fuzzy_match("box", "mailbox", FUZZY_ALGO_SUBSEQ, NULL, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Expected: >=0");
    TEST_MSG("Actual:   %d", score);
  }

  // NULL result (should not crash)
  {
    struct FuzzyOptions opts = { 0 };

    int score = fuzzy_match("box", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, NULL);
    TEST_CHECK(score >= 0);
    TEST_MSG("Expected: >=0");
    TEST_MSG("Actual:   %d", score);
  }

  // Unknown algorithm
  {
    struct FuzzyResult result = { 0 };
    struct FuzzyOptions opts = { 0 };

    int score = fuzzy_match("box", "mailbox", (enum FuzzyAlgo) 999, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Expected: -1");
    TEST_MSG("Actual:   %d", score);
  }
}
