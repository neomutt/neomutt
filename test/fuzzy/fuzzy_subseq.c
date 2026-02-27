/**
 * @file
 * Test code for fuzzy subsequence matching
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

int fuzzy_subseq_match(const char *pattern, const char *candidate,
                       const struct FuzzyOptions *opts, struct FuzzyResult *out);

void test_fuzzy_subseq_basic(void)
{
  struct FuzzyResult result = { 0 };
  struct FuzzyOptions opts = { 0 };

  // Degenerate test
  {
    int score = fuzzy_subseq_match(NULL, "mailinglists/neomutt-dev", &opts, &result);
    TEST_CHECK(score < 0);
    score = fuzzy_subseq_match("mlnd", NULL, &opts, &result);
    TEST_CHECK(score < 0);
  }

  // Basic subsequence match
  {
    int score = fuzzy_match("mlnd", "mailinglists/neomutt-dev",
                            FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'mlnd' should match 'mailinglists/neomutt-dev'");
    TEST_MSG("Score: %d", score);
  }

  // Consecutive match
  {
    int score = fuzzy_match("box", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'box' should match 'mailbox'");
    TEST_MSG("Score: %d", score);
  }

  // Prefix match
  {
    int score = fuzzy_match("mail", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'mail' should match 'mailbox'");
    TEST_MSG("Score: %d", score);
  }

  // Full match
  {
    int score = fuzzy_match("mailbox", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'mailbox' should match 'mailbox'");
    TEST_MSG("Score: %d", score);
  }

  // No match
  {
    int score = fuzzy_match("xyz", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Pattern 'xyz' should not match 'mailbox'");
  }

  // Pattern longer than candidate
  {
    int score = fuzzy_match("mailboxes", "box", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Pattern 'mailboxes' should not match 'box'");
  }
}

void test_fuzzy_subseq_case_sensitive(void)
{
  struct FuzzyResult result = { 0 };

  // Case insensitive (default)
  {
    struct FuzzyOptions opts = { 0 };
    int score1 = fuzzy_match("inbox", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score1 >= 0);
    TEST_MSG("Case insensitive: 'inbox' should match 'INBOX'");

    int score2 = fuzzy_match("INBOX", "inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score2 >= 0);
    TEST_MSG("Case insensitive: 'INBOX' should match 'inbox'");
  }

  // Case sensitive
  {
    struct FuzzyOptions opts = { .case_sensitive = true };
    int score1 = fuzzy_match("inbox", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score1 == -1);
    TEST_MSG("Case sensitive: 'inbox' should not match 'INBOX'");

    int score2 = fuzzy_match("INBOX", "inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score2 == -1);
    TEST_MSG("Case sensitive: 'INBOX' should not match 'inbox'");

    int score3 = fuzzy_match("INBOX", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score3 >= 0);
    TEST_MSG("Case sensitive: 'INBOX' should match 'INBOX'");
  }
}

void test_fuzzy_subseq_smart_case(void)
{
  struct FuzzyResult result = { 0 };
  struct FuzzyOptions opts = { .smart_case = true };

  // All lowercase pattern -> case insensitive
  {
    int score = fuzzy_match("inbox", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Smart case: lowercase pattern 'inbox' should match 'INBOX'");
  }

  // Pattern with uppercase -> case sensitive
  {
    int score1 = fuzzy_match("INBOX", "inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score1 == -1);
    TEST_MSG("Smart case: uppercase pattern 'INBOX' should not match 'inbox'");

    int score2 = fuzzy_match("INBOX", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score2 >= 0);
    TEST_MSG("Smart case: uppercase pattern 'INBOX' should match 'INBOX'");
  }

  // Mixed case pattern -> case sensitive
  {
    int score1 = fuzzy_match("InBox", "inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score1 == -1);
    TEST_MSG("Smart case: mixed pattern 'InBox' should not match 'inbox'");

    int score2 = fuzzy_match("InBox", "InBox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score2 >= 0);
    TEST_MSG("Smart case: mixed pattern 'InBox' should match 'InBox'");
  }
}

void test_fuzzy_subseq_scoring(void)
{
  struct FuzzyResult result1 = { 0 };
  struct FuzzyResult result2 = { 0 };
  struct FuzzyOptions opts = { 0 };

  // Prefix match should score higher than scattered match
  {
    int score1 = fuzzy_match("mail", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result1);
    int score2 = fuzzy_match("mail", "my_mail", FUZZY_ALGO_SUBSEQ, &opts, &result2);
    TEST_CHECK(score1 > score2);
    TEST_MSG("Prefix match 'mail' in 'mailbox' (%d) should score higher than scattered 'my_mail' (%d)",
             score1, score2);
  }

  // Consecutive match should score higher than gapped match
  {
    int score1 = fuzzy_match("box", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result1);
    int score2 = fuzzy_match("box", "big_old_ox", FUZZY_ALGO_SUBSEQ, &opts, &result2);
    TEST_CHECK(score1 > score2);
    TEST_MSG("Consecutive 'box' in 'mailbox' (%d) should score higher than scattered 'big_old_ox' (%d)",
             score1, score2);
  }

  // Boundary matches should score higher than mid-word matches
  {
    int score1 = fuzzy_match("md", "mailinglists/dev", FUZZY_ALGO_SUBSEQ, &opts, &result1);
    int score2 = fuzzy_match("md", "command", FUZZY_ALGO_SUBSEQ, &opts, &result2);
    TEST_CHECK(score1 > score2);
    TEST_MSG("Boundary match 'md' in 'mailinglists/dev' (%d) should score higher than mid-word 'command' (%d)",
             score1, score2);
  }

  // Shorter candidates should score higher (with similar matches)
  {
    int score1 = fuzzy_match("box", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result1);
    int score2 = fuzzy_match("box", "very_long_mailbox_name", FUZZY_ALGO_SUBSEQ,
                             &opts, &result2);
    TEST_CHECK(score1 > score2);
    TEST_MSG("Shorter candidate 'mailbox' (%d) should score higher than longer 'very_long_mailbox_name' (%d)",
             score1, score2);
  }
}

void test_fuzzy_subseq_prefer_prefix(void)
{
  struct FuzzyResult result1 = { 0 };
  struct FuzzyResult result2 = { 0 };

  // Without prefer_prefix
  {
    struct FuzzyOptions opts = { .prefer_prefix = false };
    int score1 = fuzzy_match("mail", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result1);
    int score2 = fuzzy_match("mail", "archive/mail", FUZZY_ALGO_SUBSEQ, &opts, &result2);

    TEST_MSG("Without prefer_prefix: 'mail' in 'mailbox' = %d", score1);
    TEST_MSG("Without prefer_prefix: 'mail' in 'archive/mail' = %d", score2);
  }

  // With prefer_prefix
  {
    struct FuzzyOptions opts = { .prefer_prefix = true };
    int score1 = fuzzy_match("mail", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result1);
    int score2 = fuzzy_match("mail", "archive/mail", FUZZY_ALGO_SUBSEQ, &opts, &result2);

    TEST_CHECK(score1 > score2);
    TEST_MSG("With prefer_prefix: 'mail' in 'mailbox' (%d) should score higher than 'archive/mail' (%d)",
             score1, score2);
  }
}

void test_fuzzy_subseq_result_fields(void)
{
  struct FuzzyResult result = { 0 };
  struct FuzzyOptions opts = { 0 };

  // Test result fields are populated correctly
  {
    int score = fuzzy_match("box", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_CHECK(result.score == score);
    TEST_CHECK(result.start == 4); // 'b' in mailbox
    TEST_CHECK(result.end == 6);   // 'x' in mailbox
    TEST_CHECK(result.span == 3);  // "box" is 3 characters
    TEST_MSG("Score: %d, Start: %d, End: %d, Span: %d", result.score,
             result.start, result.end, result.span);
  }

  // Test prefix match positions
  {
    int score = fuzzy_match("mail", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_CHECK(result.start == 0); // 'm' at start
    TEST_CHECK(result.end == 3);   // 'l' at position 3
    TEST_CHECK(result.span == 4);  // "mail" is 4 characters
    TEST_MSG("Score: %d, Start: %d, End: %d, Span: %d", result.score,
             result.start, result.end, result.span);
  }
}

void test_fuzzy_subseq_separators(void)
{
  struct FuzzyResult result = { 0 };
  struct FuzzyOptions opts = { 0 };

  // Path separator '/'
  {
    int score = fuzzy_match("nd", "neomutt/dev", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'nd' should match across '/' in 'neomutt/dev'");
  }

  // Underscore separator
  {
    int score = fuzzy_match("mn", "my_name", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'mn' should match across '_' in 'my_name'");
  }

  // Dash separator
  {
    int score = fuzzy_match("nd", "neomutt-dev", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'nd' should match across '-' in 'neomutt-dev'");
  }

  // Dot separator
  {
    int score = fuzzy_match("fc", "file.conf", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'fc' should match across '.' in 'file.conf'");
  }
}

void test_fuzzy_subseq_camelcase(void)
{
  struct FuzzyResult result1 = { 0 };
  struct FuzzyResult result2 = { 0 };
  struct FuzzyOptions opts = { 0 };

  // CamelCase boundary should score well
  {
    int score1 = fuzzy_match("MM", "MyMailbox", FUZZY_ALGO_SUBSEQ, &opts, &result1);
    int score2 = fuzzy_match("MM", "mailman", FUZZY_ALGO_SUBSEQ, &opts, &result2);

    TEST_CHECK(score1 > score2);
    TEST_MSG("CamelCase match 'MM' in 'MyMailbox' (%d) should score higher than 'mailman' (%d)",
             score1, score2);
  }
}

void test_fuzzy_subseq_max_pattern(void)
{
  struct FuzzyResult result = { 0 };

  // Pattern within default limit (256)
  {
    char pattern[200];
    memset(pattern, 'a', sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = '\0';

    struct FuzzyOptions opts = { 0 };
    int score = fuzzy_match(pattern, "candidate", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1); // Won't match, but should not crash
    TEST_MSG("Long pattern within limit should be processed");
  }

  // Pattern exceeding default limit
  {
    char pattern[300];
    memset(pattern, 'a', sizeof(pattern) - 1);
    pattern[sizeof(pattern) - 1] = '\0';

    struct FuzzyOptions opts = { 0 };
    int score = fuzzy_match(pattern, "candidate", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score == -1);
    TEST_MSG("Pattern exceeding default limit should be rejected");
  }

  // Custom max_pattern
  {
    struct FuzzyOptions opts = { .max_pattern = 10 };
    int score1 = fuzzy_match("short", "candidate", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score1 == -1); // Won't match, but within limit

    int score2 = fuzzy_match("toolongpattern", "candidate", FUZZY_ALGO_SUBSEQ,
                             &opts, &result);
    TEST_CHECK(score2 == -1); // Exceeds custom limit
    TEST_MSG("Custom max_pattern should be respected");
  }
}

void test_fuzzy_subseq_edge_cases(void)
{
  struct FuzzyResult result = { 0 };
  struct FuzzyOptions opts = { 0 };

  // Single character pattern
  {
    int score = fuzzy_match("m", "mailbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Single character pattern 'm' should match 'mailbox'");
  }

  // Single character candidate
  {
    int score1 = fuzzy_match("a", "a", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score1 >= 0);
    TEST_MSG("Single character match should work");

    int score2 = fuzzy_match("a", "b", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score2 == -1);
    TEST_MSG("Single character non-match should fail");
  }

  // Pattern same as candidate
  {
    int score = fuzzy_match("test", "test", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Exact match should score highly");
  }

  // Repeated characters
  {
    int score = fuzzy_match("aaa", "banana", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern 'aaa' should match 'banana' (has 3 a's)");
  }

  // Special characters
  {
    int score = fuzzy_match("@ex", "user@example.com", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern '@ex' should match 'user@example.com'");
  }

  // Numbers
  {
    int score = fuzzy_match("123", "test123", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("Pattern '123' should match 'test123'");
  }
}

void test_fuzzy_subseq_real_world(void)
{
  struct FuzzyResult result = { 0 };
  struct FuzzyOptions opts = { .smart_case = true };

  // Mailbox path examples
  {
    int score = fuzzy_match("inb", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("'inb' should match 'INBOX'");

    score = fuzzy_match("mlnd", "mailinglists/neomutt-dev", FUZZY_ALGO_SUBSEQ,
                        &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("'mlnd' should match 'mailinglists/neomutt-dev'");

    score = fuzzy_match("arch", "Archive/2023", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("'arch' should match 'Archive/2023'");
  }

  // Command examples
  {
    int score = fuzzy_match("setfrom", "set from", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("'setfrom' should match 'set from'");

    score = fuzzy_match("bind", "bind-key", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("'bind' should match 'bind-key'");
  }

  // Alias examples
  {
    int score = fuzzy_match("rich", "Richard Russon", FUZZY_ALGO_SUBSEQ, &opts, &result);
    TEST_CHECK(score >= 0);
    TEST_MSG("'rich' should match 'Richard Russon'");
  }
}
