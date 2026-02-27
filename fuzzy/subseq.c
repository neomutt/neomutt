/**
 * @file
 * Subsequence fuzzy matching implementation
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
 * @page fuzzy_subseq Subsequence fuzzy matching
 *
 * High-performance FZF-style subsequence matching implementation
 *
 * ## Algorithm Overview
 *
 * This implements a single-pass subsequence matcher optimized for interactive
 * completion. Characters from the pattern must appear in the candidate string
 * in the same order, but not necessarily consecutively.
 *
 * ### Example Matches
 *
 * Pattern "mlnd" matches:
 * - "mailinglists/neomutt-dev" (high score: matches at word boundaries)
 * - "mailing_list_node" (medium score: some boundaries)
 * - "my_long_nested_dir" (lower score: scattered matches)
 *
 * Pattern "inb" matches:
 * - "INBOX" (highest: start of string + consecutive)
 * - "Archive/INBOX" (medium: not at root)
 * - "mailbox/inbox-archive" (lower: gaps between matches)
 *
 * ## UTF-8 Support
 *
 * The matcher is **UTF-8 aware** but uses **ASCII-only case folding**:
 *
 * - **Byte-wise matching**: Treats strings as sequences of bytes
 * - **ASCII case folding**: Only A-Z are folded to a-z
 * - **UTF-8 preservation**: Multi-byte UTF-8 sequences are never split
 * - **ASCII boundaries**: Only ASCII separators (/.-_) get boundary bonuses
 *
 * This approach provides:
 * - ✓ Fast performance (no Unicode decoding overhead)
 * - ✓ Correct UTF-8 handling (multi-byte sequences preserved)
 * - ✓ ASCII case-insensitive matching (works for English text)
 * - ✓ UTF-8 matching works (as byte sequences)
 *
 * Examples:
 * - "inbox" matches "INBOX" (ASCII folding)
 * - "café" matches "Café" (exact bytes, no case folding on é)
 * - "mail" matches "郵件/mail/box" (ASCII substring matches)
 * - "郵件" matches "郵件/mail" (exact byte sequence)
 *
 * **Note**: Non-ASCII characters are matched case-sensitively as byte sequences.
 * This is intentional for performance and simplicity.
 *
 * ## Scoring Rules
 *
 * The algorithm assigns scores based on multiple factors:
 *
 * ### Base Score
 * - Each matched character: +10 points
 *
 * ### Bonuses
 * - **Start of string**: +30 points (e.g., "I" in "INBOX")
 * - **After separator** ('/', '.', '-', '_'): +15 points
 * - **Prefix match** (when prefer_prefix=true): +40 points
 * - **Consecutive matches**: +15 points per consecutive char
 *   (e.g., "box" all together in "mailbox")
 * - **CamelCase boundary**: +10 points (e.g., "M" in "MyMailbox")
 *   (Only detects ASCII A-Z boundaries)
 *
 * ### Penalties
 * - **Gaps between matches**: -2 points per character gap
 *   (encourages compact matches)
 * - **Total span**: -1 point per character in match span
 *   (first to last matched char)
 * - **String length**: -length/4 points
 *   (slightly favors shorter candidates)
 *
 * ### Smart Case Matching
 *
 * By default, matching is case-insensitive (ASCII only). With smart_case enabled:
 * - All-lowercase pattern → case-insensitive (ASCII)
 * - Pattern with uppercase → case-sensitive
 *
 * **Note**: Smart case only examines ASCII characters (A-Z).
 *
 * ## Performance Characteristics
 *
 * - **Time complexity**: O(n) where n = length of candidate string
 * - **Space complexity**: O(m) where m = length of pattern (stack only)
 * - **No heap allocation**: Uses fixed-size stack array for match positions
 * - **No backtracking**: Single forward pass through candidate
 * - **No recursion**: Purely iterative
 * - **No UTF-8 decoding**: Byte-wise comparison for maximum speed
 *
 * This makes it suitable for interactive use even with thousands of candidates.
 *
 * ## Why These Rules?
 *
 * The scoring model is optimized for hierarchical paths and structured strings:
 *
 * 1. **Boundary bonuses** help match path components and words
 * 2. **Consecutive bonuses** reward compact substring matches
 * 3. **Gap penalties** discourage scattered character matches
 * 4. **Span penalties** favor matches clustered together
 * 5. **Length penalties** prevent long strings from dominating
 *
 * This produces intuitive rankings for mailbox paths, commands, and names.
 */

#include "config.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include "lib.h"
#include "fuzzy.h"

/// Default maximum pattern length
#define DEFAULT_MAX_PATTERN 256

/**
 * ascii_tolower - Convert ASCII character to lowercase
 * @param c Character to convert
 * @retval num Lowercase character if ASCII A-Z, otherwise unchanged
 *
 * This function only converts ASCII characters (A-Z to a-z).
 * All other bytes (including UTF-8 continuation bytes) are left unchanged.
 * This preserves UTF-8 sequences while allowing case-insensitive ASCII matching.
 */
static inline unsigned char ascii_tolower(unsigned char c)
{
  if ((c >= 'A') && (c <= 'Z'))
    return c + ('a' - 'A');
  return c;
}

/**
 * lower_if - Convert character to lowercase conditionally
 * @param c    Character to convert
 * @param fold If true, convert ASCII to lowercase
 * @retval num Converted character
 *
 * Only performs ASCII case folding (A-Z to a-z).
 * All other bytes are left unchanged, preserving UTF-8 sequences.
 */
static inline int lower_if(int c, bool fold)
{
  return fold ? ascii_tolower((unsigned char) c) : c;
}

/**
 * compute_case_mode - Determine if case folding should be used
 * @param pattern Pattern string
 * @param opts    Fuzzy matching options
 * @retval true  Use case-insensitive matching (ASCII only)
 * @retval false Use case-sensitive matching
 *
 * Smart case only examines ASCII characters (A-Z).
 * Non-ASCII bytes are ignored for smart case detection.
 */
static bool compute_case_mode(const char *pattern, const struct FuzzyOptions *opts)
{
  if (!opts)
    return true; // default case-insensitive

  if (opts->case_sensitive)
    return false;

  if (opts->smart_case)
  {
    // Check if pattern contains any ASCII uppercase (A-Z)
    for (const char *p = pattern; *p; p++)
    {
      unsigned char c = (unsigned char) *p;
      if ((c >= 'A') && (c <= 'Z'))
        return false; // found uppercase, use case-sensitive
    }
  }

  return true; // fold case
}

/**
 * fuzzy_subseq_match - Perform subsequence fuzzy matching (UTF-8 aware, ASCII case-folding)
 * @param pattern   Pattern to match (UTF-8 byte sequence)
 * @param candidate Candidate string to match against (UTF-8 byte sequence)
 * @param opts      Fuzzy matching options
 * @param out       Output result structure
 * @retval >=0 Match score
 * @retval  -1 Error or no match
 *
 * Performs byte-wise subsequence matching with ASCII-only case folding.
 * UTF-8 multi-byte sequences are preserved but matched as raw bytes.
 * Only ASCII A-Z characters are case-folded to a-z.
 */
int fuzzy_subseq_match(const char *pattern, const char *candidate,
                       const struct FuzzyOptions *opts, struct FuzzyResult *out)
{
  if (!pattern || !candidate)
    return -1;

  size_t plen = strlen(pattern);
  if (plen == 0)
    return -1;

  int max_pattern = (opts && opts->max_pattern) ? opts->max_pattern : DEFAULT_MAX_PATTERN;
  if ((max_pattern <= 0) || (max_pattern > DEFAULT_MAX_PATTERN))
    max_pattern = DEFAULT_MAX_PATTERN;

  if (plen > (size_t) max_pattern)
    return -1;

  bool fold = compute_case_mode(pattern, opts);

  int matchpos[DEFAULT_MAX_PATTERN];

  int pi = 0;
  int ci = 0;
  int score = 0;

  int first = -1;
  int last = -1;

  // Forward subsequence scan
  while (candidate[ci] && pi < (int) plen)
  {
    int pc = lower_if(pattern[pi], fold);
    int cc = lower_if(candidate[ci], fold);

    if (pc == cc)
    {
      matchpos[pi] = ci;

      if (first < 0)
        first = ci;

      last = ci;
      pi++;
    }

    ci++;
  }

  if (pi != (int) plen)
    return -1; // not a subsequence

  // Scoring

  // Base score
  score += plen * 10;

  // Consecutive & gap penalties
  for (int i = 1; i < pi; i++)
  {
    int gap = matchpos[i] - matchpos[i - 1] - 1;

    if (gap == 0)
      score += 15; // compact match bonus
    else
      score -= gap * 2;
  }

  // Span penalty
  int span = last - first + 1;
  score -= span;

  // Prefix bonus
  if (first == 0 && opts && opts->prefer_prefix)
    score += 40;

  // Boundary bonus (ASCII-only separators and CamelCase)
  for (int i = 0; i < pi; i++)
  {
    int pos = matchpos[i];

    if (pos == 0)
      score += 30; // start of string
    else
    {
      unsigned char prev = (unsigned char) candidate[pos - 1];
      unsigned char curr = (unsigned char) candidate[pos];

      // ASCII separator boundaries
      if ((prev == '/') || (prev == '.') || (prev == '-') || (prev == '_'))
        score += 15;
      // ASCII CamelCase boundary (lowercase followed by uppercase)
      else if (((prev >= 'a') && (prev <= 'z')) && ((curr >= 'A') && (curr <= 'Z')))
        score += 10;
    }
  }

  // Mild length penalty
  score -= (int) strlen(candidate) / 4;

  // Ensure valid matches always return non-negative score
  if (score < 0)
    score = 0;

  if (out)
  {
    out->score = score;
    out->span = span;
    out->start = first;
    out->end = last;
  }

  return score;
}
