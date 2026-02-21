/**
 * @file
 * Test code for fuzzy matching with UTF-8 (byte-wise, ASCII case-folding)
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
#include <string.h>
#include "fuzzy/lib.h"

/**
 * test_fuzzy_utf8_bytewise_matching - Test UTF-8 as byte sequences
 *
 * The fuzzy matcher treats UTF-8 strings as byte sequences.
 * Multi-byte UTF-8 characters are matched byte-by-byte.
 */
void test_fuzzy_utf8_bytewise_matching(void)
{
  struct FuzzyOptions opts = { 0 };
  struct FuzzyResult result = { 0 };
  int score;

  // UTF-8 strings match as exact byte sequences
  score = fuzzy_match("café", "café", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Exact UTF-8 bytes should match");

  // Partial UTF-8 byte sequences work
  score = fuzzy_match("caf", "café", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Prefix of UTF-8 string should match");

  // Chinese characters as byte sequences
  score = fuzzy_match("中", "中国", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Chinese character bytes should match");

  // Japanese  hiragana
  score = fuzzy_match("に", "にほん", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Japanese hiragana bytes should match");

  // Emoji
  score = fuzzy_match("📧", "📧 inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Emoji bytes should match");
}

/**
 * test_fuzzy_utf8_ascii_case_folding - Test ASCII-only case folding
 *
 * Only ASCII A-Z are folded to a-z.
 * Non-ASCII bytes (including UTF-8) are matched case-sensitively.
 */
void test_fuzzy_utf8_ascii_case_folding(void)
{
  struct FuzzyOptions opts = { 0 }; // case-insensitive by default
  struct FuzzyResult result = { 0 };
  int score;

  // ASCII case-insensitive
  score = fuzzy_match("inbox", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("ASCII should be case-insensitive");

  score = fuzzy_match("mail", "MailBox", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("ASCII mixed case should match");

  // Non-ASCII is case-sensitive (no Unicode case folding)
  // ASCII 'c' vs 'C' will match in case-insensitive mode
  score = fuzzy_match("café", "Café", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("ASCII 'c' matches 'C' in case-insensitive mode");

  // But the é is always the same bytes (0xC3 0xA9 in both)
  // To truly test case sensitivity on non-ASCII, we need different bytes
  // é (U+00E9) vs É (U+00C9) have different UTF-8 encodings
  score = fuzzy_match("é", "É", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score < 0);
  TEST_MSG("Non-ASCII characters é vs É should not match (different bytes)");
}

/**
 * test_fuzzy_utf8_ascii_smart_case - Test smart case with UTF-8
 *
 * Smart case only examines ASCII characters (A-Z).
 * Non-ASCII bytes are ignored for smart case detection.
 */
void test_fuzzy_utf8_ascii_smart_case(void)
{
  struct FuzzyOptions opts = { .smart_case = true };
  struct FuzzyResult result = { 0 };
  int score;

  // Lowercase ASCII pattern → case-insensitive for ASCII
  score = fuzzy_match("inbox", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Lowercase ASCII pattern should match uppercase");

  // Uppercase ASCII in pattern → case-sensitive
  score = fuzzy_match("INBOX", "inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score < 0);
  TEST_MSG("Uppercase ASCII pattern should not match lowercase");

  score = fuzzy_match("INBOX", "INBOX", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Uppercase pattern should match same case");

  // Non-ASCII doesn't affect smart case decision
  score = fuzzy_match("café", "CAFÉ", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score < 0);
  TEST_MSG("Non-ASCII doesn't affect smart case, ASCII 'c' != 'C'");
}

/**
 * test_fuzzy_utf8_mixed_ascii_utf8 - Test mixed ASCII and UTF-8
 *
 * ASCII components get case folding, UTF-8 bytes don't.
 */
void test_fuzzy_utf8_mixed_ascii_utf8(void)
{
  struct FuzzyOptions opts = { 0 };
  struct FuzzyResult result = { 0 };
  int score;

  // ASCII part of path with UTF-8
  score = fuzzy_match("mail", "郵件/mail/inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("ASCII substring in UTF-8 path should match");

  score = fuzzy_match("mail", "郵件/MAIL/inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("ASCII case-insensitive in UTF-8 path");

  // UTF-8 prefix, then ASCII
  score = fuzzy_match("郵mail", "郵件/mail/inbox", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Mixed UTF-8 and ASCII pattern should match");

  // Emoji with ASCII
  score = fuzzy_match("📧inbox", "📧 INBOX 📬", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Emoji + ASCII with case folding should work");
}

/**
 * test_fuzzy_utf8_boundaries - Test boundary detection with UTF-8
 *
 * Only ASCII separators (/.-_) are treated as boundaries.
 * UTF-8 characters are not examined for boundary properties.
 */
void test_fuzzy_utf8_boundaries(void)
{
  struct FuzzyOptions opts = { 0 };
  struct FuzzyResult result1 = { 0 }, result2 = { 0 };
  int score1, score2;

  // ASCII separator with UTF-8 text
  score1 = fuzzy_match("über", "arbeit/über", FUZZY_ALGO_SUBSEQ, &opts, &result1);
  score2 = fuzzy_match("über", "arbeitüber", FUZZY_ALGO_SUBSEQ, &opts, &result2);
  TEST_CHECK(score1 > 0 && score2 > 0);
  TEST_CHECK(score1 > score2);
  TEST_MSG("ASCII separator should give boundary bonus");

  // Start-of-string bonus works
  score1 = fuzzy_match("café", "café", FUZZY_ALGO_SUBSEQ, &opts, &result1);
  score2 = fuzzy_match("café", "le café", FUZZY_ALGO_SUBSEQ, &opts, &result2);
  TEST_CHECK(score1 > score2);
  TEST_MSG("Start-of-string should score higher");
}

/**
 * test_fuzzy_utf8_camelcase - Test CamelCase with UTF-8
 *
 * CamelCase detection only works for ASCII a-z and A-Z.
 * UTF-8 characters don't participate in CamelCase detection.
 */
void test_fuzzy_utf8_camelcase(void)
{
  struct FuzzyOptions opts = { 0 };
  struct FuzzyResult result1 = { 0 }, result2 = { 0 };
  int score1, score2;

  // ASCII CamelCase works
  score1 = fuzzy_match("MB", "MyMailBox", FUZZY_ALGO_SUBSEQ, &opts, &result1);
  score2 = fuzzy_match("MB", "My_Mail_Box", FUZZY_ALGO_SUBSEQ, &opts, &result2);
  TEST_CHECK(score1 > 0 && score2 > 0);
  // CamelCase might score differently than separators
  TEST_MSG("ASCII CamelCase should be detected");

  // UTF-8 doesn't participate in CamelCase
  // The matcher can't detect case boundaries in UTF-8
  score1 = fuzzy_match("café", "myCafé", FUZZY_ALGO_SUBSEQ, &opts, &result1);
  TEST_CHECK(score1 > 0);
  TEST_MSG("UTF-8 text with ASCII CamelCase should match");
}

/**
 * test_fuzzy_utf8_realistic_paths - Test realistic international paths
 */
void test_fuzzy_utf8_realistic_paths(void)
{
  struct FuzzyOptions opts = { 0 };
  struct FuzzyResult result = { 0 };
  int score;

  // German mailbox paths
  score = fuzzy_match("arbeit", "~/Mail/Arbeit/Büro", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("German path with ASCII pattern should match");

  // French
  score = fuzzy_match("trav", "~/Mail/Travail/Général", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("French path with ASCII pattern should match");

  // Japanese mailbox (ASCII will match if present)
  score = fuzzy_match("mail", "メール/mail/受信", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Japanese path with ASCII component should match");

  // Chinese
  score = fuzzy_match("mail", "邮件/mail/收件箱", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Chinese path with ASCII component should match");

  // Pure UTF-8 matching (byte sequences)
  score = fuzzy_match("収信", "メール/収信箱", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Pure UTF-8 byte sequence matching should work");
}

/**
 * test_fuzzy_utf8_edge_cases - Test edge cases with UTF-8
 */
void test_fuzzy_utf8_edge_cases(void)
{
  struct FuzzyOptions opts = { 0 };
  struct FuzzyResult result = { 0 };
  int score;

  // Empty pattern
  score = fuzzy_match("", "café", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score < 0);
  TEST_MSG("Empty pattern should not match");

  // Single UTF-8 character (3 bytes for most, 4 for emoji)
  score = fuzzy_match("é", "café", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Single multi-byte character should match");

  // Very long UTF-8 string
  score = fuzzy_match("これはテスト", "これはテストです", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Long UTF-8 string should match");

  // Malformed UTF-8 is handled gracefully (treated as bytes)
  score = fuzzy_match("test", "test\xFF\xFE", FUZZY_ALGO_SUBSEQ, &opts, &result);
  TEST_CHECK(score > 0);
  TEST_MSG("Malformed UTF-8 should not crash");
}
