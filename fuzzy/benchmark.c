/**
 * @file
 * Standalone benchmark for libfuzzy
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
 * @page fuzzy_benchmark Fuzzy matching benchmark
 *
 * Standalone benchmark program for measuring libfuzzy performance.
 *
 * ## Usage
 *
 * ```
 * ./fuzzy-benchmark [iterations]
 * ```
 *
 * Default iterations: 100000
 *
 * ## Test Scenarios
 *
 * 1. Short patterns vs short candidates
 * 2. Short patterns vs long candidates
 * 3. Long patterns vs long candidates
 * 4. Realistic mailbox paths
 * 5. Case-sensitive vs case-insensitive
 * 6. With and without prefer_prefix
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lib.h"

// Sample mailbox paths for realistic testing
static const char *mailbox_paths[] = {
  "INBOX",
  "Archive/2023",
  "Archive/2024/January",
  "Archive/2024/February",
  "Sent",
  "Drafts",
  "Trash",
  "mailinglists/neomutt-dev",
  "mailinglists/neomutt-users",
  "mailinglists/linux-kernel",
  "mailinglists/debian-devel",
  "work/projects/libfuzzy",
  "work/projects/neomutt",
  "work/reports/weekly",
  "work/reports/monthly",
  "personal/family",
  "personal/friends",
  "personal/receipts",
  "notifications/github",
  "notifications/gitlab",
  "shopping/amazon",
  "shopping/ebay",
  "travel/bookings",
  "travel/confirmations",
};

static const int NUM_MAILBOXES = sizeof(mailbox_paths) / sizeof(mailbox_paths[0]);

/**
 * get_time_ms - Get current time in milliseconds
 * @retval num Current time in milliseconds
 */
static double get_time_ms(void)
{
#if defined(_POSIX_TIMERS) && _POSIX_TIMERS > 0
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * 1000.0) + (ts.tv_nsec / 1000000.0);
#else
  // Fallback to less accurate clock()
  return (double) clock() / CLOCKS_PER_SEC * 1000.0;
#endif
}

/**
 * benchmark_basic - Basic pattern matching benchmark
 * @param pattern     Pattern to match
 * @param candidate   Candidate string
 * @param iterations  Number of iterations
 * @param description Test description
 */
static void benchmark_basic(const char *pattern, const char *candidate,
                            int iterations, const char *description)
{
  struct FuzzyOptions opts = { 0 };
  struct FuzzyResult result = { 0 };

  double start = get_time_ms();

  int matches = 0;
  for (int i = 0; i < iterations; i++)
  {
    int score = fuzzy_match(pattern, candidate, FUZZY_ALGO_SUBSEQ, &opts, &result);
    if (score >= 0)
      matches++;
  }

  double end = get_time_ms();
  double elapsed = end - start;
  double per_match = elapsed / iterations;
  double throughput = iterations / (elapsed / 1000.0);

  printf("%-50s ", description);
  printf("%8.2f ms  ", elapsed);
  printf("%8.3f µs/op  ", per_match * 1000.0);
  printf("%10.0f ops/sec  ", throughput);
  printf("(%d/%d matches)\n", matches, iterations);
}

/**
 * benchmark_mailbox_list - Benchmark searching through mailbox list
 * @param pattern    Pattern to search for
 * @param iterations Number of times to search through all mailboxes
 */
static void benchmark_mailbox_list(const char *pattern, int iterations)
{
  struct FuzzyOptions opts = { .smart_case = true };
  struct FuzzyResult result = { 0 };

  double start = get_time_ms();

  int total_matches = 0;
  for (int i = 0; i < iterations; i++)
  {
    for (int j = 0; j < NUM_MAILBOXES; j++)
    {
      int score = fuzzy_match(pattern, mailbox_paths[j], FUZZY_ALGO_SUBSEQ, &opts, &result);
      if (score >= 0)
        total_matches++;
    }
  }

  double end = get_time_ms();
  double elapsed = end - start;
  int total_ops = iterations * NUM_MAILBOXES;
  double per_match = elapsed / total_ops;
  double throughput = total_ops / (elapsed / 1000.0);

  printf("%-50s ", "Mailbox list search");
  printf("%8.2f ms  ", elapsed);
  printf("%8.3f µs/op  ", per_match * 1000.0);
  printf("%10.0f ops/sec  ", throughput);
  printf("(%d matches in %d ops)\n", total_matches, total_ops);
}

/**
 * benchmark_options - Benchmark with different options
 * @param pattern    Pattern to match
 * @param candidate  Candidate string
 * @param iterations Number of iterations
 */
static void benchmark_options(const char *pattern, const char *candidate, int iterations)
{
  struct FuzzyResult result = { 0 };

  // Case insensitive
  {
    struct FuzzyOptions opts = { .case_sensitive = false };
    double start = get_time_ms();

    for (int i = 0; i < iterations; i++)
      fuzzy_match(pattern, candidate, FUZZY_ALGO_SUBSEQ, &opts, &result);

    double elapsed = get_time_ms() - start;
    printf("%-50s %8.2f ms  %8.3f µs/op\n", "  Case-insensitive", elapsed,
           elapsed / iterations * 1000.0);
  }

  // Case sensitive
  {
    struct FuzzyOptions opts = { .case_sensitive = true };
    double start = get_time_ms();

    for (int i = 0; i < iterations; i++)
      fuzzy_match(pattern, candidate, FUZZY_ALGO_SUBSEQ, &opts, &result);

    double elapsed = get_time_ms() - start;
    printf("%-50s %8.2f ms  %8.3f µs/op\n", "  Case-sensitive", elapsed,
           elapsed / iterations * 1000.0);
  }

  // Smart case
  {
    struct FuzzyOptions opts = { .smart_case = true };
    double start = get_time_ms();

    for (int i = 0; i < iterations; i++)
      fuzzy_match(pattern, candidate, FUZZY_ALGO_SUBSEQ, &opts, &result);

    double elapsed = get_time_ms() - start;
    printf("%-50s %8.2f ms  %8.3f µs/op\n", "  Smart case", elapsed,
           elapsed / iterations * 1000.0);
  }

  // With prefix preference
  {
    struct FuzzyOptions opts = { .prefer_prefix = true };
    double start = get_time_ms();

    for (int i = 0; i < iterations; i++)
      fuzzy_match(pattern, candidate, FUZZY_ALGO_SUBSEQ, &opts, &result);

    double elapsed = get_time_ms() - start;
    printf("%-50s %8.2f ms  %8.3f µs/op\n", "  Prefer prefix", elapsed,
           elapsed / iterations * 1000.0);
  }
}

/**
 * main - Entry point for fuzzy benchmark
 * @param argc Number of arguments
 * @param argv Argument vector
 * @retval 0 Success
 */
int main(int argc, char *argv[])
{
  int iterations = 100000;

  if (argc > 1)
  {
    iterations = atoi(argv[1]);
    if (iterations <= 0)
      iterations = 100000;
  }

  printf("=================================================================\n");
  printf("libfuzzy Benchmark - %d iterations per test\n", iterations);
  printf("=================================================================\n\n");

  printf("Test                                                Time        Per Op        Throughput      Results\n");
  printf("------------------------------------------------------------------------------------------------------------------------------\n");

  // Short pattern, short candidate
  benchmark_basic("box", "mailbox", iterations, "Short pattern + short candidate");

  // Short pattern, medium candidate
  benchmark_basic("mlnd", "mailinglists/neomutt-dev", iterations,
                  "Short pattern + medium candidate");

  // Short pattern, long candidate
  benchmark_basic("arch", "Archive/2024/January/Projects/NeoMutt", iterations,
                  "Short pattern + long candidate");

  // Medium pattern, long candidate
  benchmark_basic("archjan", "Archive/2024/January/Projects/NeoMutt",
                  iterations, "Medium pattern + long candidate");

  // No match
  benchmark_basic("xyz", "mailbox", iterations, "No match");

  // Prefix match
  benchmark_basic("mail", "mailbox", iterations, "Prefix match");

  // Scattered match
  benchmark_basic("mlnd", "mailing_list_node_database", iterations, "Scattered match");

  // Full match
  benchmark_basic("inbox", "inbox", iterations, "Full match");

  printf("\n");

  // Mailbox list benchmark
  printf("Realistic Scenario - Searching mailbox list (%d mailboxes)\n", NUM_MAILBOXES);
  printf("------------------------------------------------------------------------------------------------------------------------------\n");
  benchmark_mailbox_list("inbox", iterations / 100);
  benchmark_mailbox_list("mlnd", iterations / 100);
  benchmark_mailbox_list("arch", iterations / 100);
  benchmark_mailbox_list("work", iterations / 100);

  printf("\n");

  // Options comparison
  printf("Options Comparison\n");
  printf("------------------------------------------------------------------------------------------------------------------------------\n");
  benchmark_options("inbox", "INBOX", iterations);

  printf("\n");

  printf("=================================================================\n");
  printf("Benchmark Complete\n");
  printf("=================================================================\n");

  return 0;
}
