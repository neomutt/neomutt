#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/buffer.h"
#include "mutt/regex3.h"
#include "config/common.h"

static bool test_simple_cases(void)
{
  log_line(__func__);

  struct Buffer buf = mutt_buffer_make(0);
  { /* handle edge cases */
    struct Regex *rx = regex_new("hello bob", 0, &buf);

    const bool failed = !TEST_CHECK(mutt_regex_match(NULL, NULL) == false) ||
                        !TEST_CHECK(mutt_regex_match(NULL, "bob the string") == false) ||
                        !TEST_CHECK(mutt_regex_match(rx, NULL) == false);
    regex_free(&rx);

    if (failed)
      return false;
  }

  { /* handle normal cases */
    struct Regex *rx = regex_new("hell", 0, &buf);

    const bool failed =
        !TEST_CHECK(mutt_regex_match(rx, "hello there")) ||
        !TEST_CHECK(mutt_regex_match(rx, "hell is not a greeting")) ||
        !TEST_CHECK(mutt_regex_match(rx, "a demonic elavator is a hellevator"));
    regex_free(&rx);

    if (failed)
      return false;
  }

  { /* test more elaborate regex */
    const char *input = "bob bob bob mary bob jonny bob jon jon joe bob";
    struct Regex *rx = regex_new("bob", 0, &buf);

    const bool result = mutt_regex_capture(rx, input, 0, NULL);
    const bool failed = !TEST_CHECK(result);
    regex_free(&rx);

    if (failed)
      return false;
  }

  { /* test passing simple flags */
    const char *input = "BOB";
    struct Regex *rx = regex_new("bob", 0, &buf);
    const bool failed = !TEST_CHECK(mutt_regex_capture(rx, input, 0, 0));
    regex_free(&rx);

    if (failed)
      return false;
  }

  mutt_buffer_dealloc(&buf);
  return true;
}

static bool test_old_implementation(void)
{
  log_line(__func__);

  // These tests check that the wrapper has the same behavior as
  // prior, similar implementations

  const char *bob_line = "definitely bob haha";
  const char *not_bob_line = "john dave marty nothing else here";

  struct Buffer buf = mutt_buffer_make(0);
  {
    // from: if (regexec(C_PgpGoodSign->regex, bob_line, 0, NULL, 0) == 0)
    //   to: if (mutt_regex_match(C_PgpGoodSign, bob_line))

    struct Regex *rx = regex_new("bob", 0, &buf);
    const bool old = regexec(rx->regex, bob_line, 0, NULL, 0) == 0;
    const bool new = mutt_regex_match(rx, bob_line);
    const bool failed = !TEST_CHECK(old == new);

    regex_free(&rx);
    if (failed)
      return false;
  }

  {
    // from: if (regexec(C_QuoteRegex->regex, bob_line, 1, pmatch, 0) == 0)
    //   to: if (mutt_regex_capture(QuoteRegex, bob_line, 1, pmatch))

    const int nmatch = 1;
    regmatch_t pmatch_1[nmatch], pmatch_2[nmatch];
    struct Regex *rx = regex_new("bob", 0, &buf);
    const bool old = regexec(rx->regex, bob_line, nmatch, pmatch_1, 0) == 0;
    const bool new = mutt_regex_capture(rx, bob_line, 1, pmatch_2);
    const bool failed_common_behavior = !TEST_CHECK(old == new);

    regex_free(&rx);
    if (failed_common_behavior)
      return false;
  }

  {
    // from: if (C_QuoteRegex && C_QuoteRegex->regex &&
    //          (regexec(C_QuoteRegex->regex, bob_line, 1, pmatch, 0) == 0))
    //   to: if (mutt_regex_capture(QuoteRegex, bob_line, 1, pmatch))

    const int nmatch = 1;
    regmatch_t pmatch_1[nmatch], pmatch_2[nmatch];

    struct Regex *rx = regex_new("bob", 0, &buf);
    const bool old = rx && rx->regex &&
                     (regexec(rx->regex, bob_line, nmatch, pmatch_1, 0) == 0);
    const bool new = mutt_regex_capture(rx, bob_line, 1, pmatch_2);
    regex_free(&rx);

    const bool failed_common_behavior = !TEST_CHECK(old == new);
    if (failed_common_behavior)
      return false;

    const bool failed_pmatch = !TEST_CHECK(pmatch_1->rm_so == pmatch_2->rm_so &&
                                           pmatch_2->rm_eo == pmatch_2->rm_eo);
    if (failed_pmatch)
      return false;
  }

  {
    // from: if ((tmp->type & hook) &&
    //         ((match && (regexec(tmp->regex.regex, match, 0, NULL, 0) == 0)) ^
    //          tmp->regex.pat_not))
    //   to: if ((tmp->type & hook) && mutt_regex_match(&tmp->regex, match))

    struct Regex *rx = regex_new("!bob", DT_REGEX_ALLOW_NOT, &buf);
    const bool old =
        (regexec(rx->regex, not_bob_line, 0, NULL, DT_REGEX_ALLOW_NOT) == 0) ^ rx->pat_not;
    const bool new = mutt_regex_match(rx, not_bob_line);
    regex_free(&rx);

    const bool failed_common_behavior = !TEST_CHECK(old == new);
    if (failed_common_behavior)
      return false;

    // make sure that bob is *NOT* found
    const bool bob_found = !TEST_CHECK(new == true);
    if (bob_found)
      return false;
  }

  {
    // from: if (C_Mask && C_Mask->regex &&
    //       !((regexec(C_Mask->regex, mdata->group, 0, NULL, 0) == 0)
    //       ^ C_Mask->pat_not))
    // to: if(mutt_regex_match(C_Mask, de->d_name))

    struct Regex *rx = regex_new("!bob", DT_REGEX_ALLOW_NOT, &buf);
    const bool old = rx && rx->regex &&
                     !((regexec(rx->regex, not_bob_line, 0, NULL, 0) == 0) ^ rx->pat_not);
    const bool new = mutt_regex_match(rx, bob_line);
    regex_free(&rx);

    const bool failed_common_behavior = !TEST_CHECK(old == new);
    if (failed_common_behavior)
      return false;
  }

  {
    // from: if (C_Mask && C_Mask->regex &&
    //          !((regexec(C_Mask->regex, mdata->group, 0, NULL, 0) == 0) ^ C_Mask->pat_not))
    //   to: if (!mutt_regex_match(C_Mask, mdata->group))
    struct Regex *rx = regex_new("!bob", DT_REGEX_ALLOW_NOT, &buf);
    const bool old = (rx && rx->regex) &&
                     !((regexec(rx->regex, line, 0, NULL, 0) == 0) ^ rx->pat_not);
    const bool new = !mutt_regex_match(rx, line);
    regex_free(&rx);

    const bool failed_common_behavior = !TEST_CHECK(old == new);
    if (failed_common_behavior)
      return false;
  }

  {
    // if ((regexec(hook->regex.regex, url, 0, NULL, 0) == 0) ^ hook->regex.pat_not)
    // if (mutt_regex_match(&hook->regex, url))

    struct Regex *rx = regex_new("bob", 0, &buf);
    const bool old = (regexec(rx->regex, bob_line, 0, NULL, 0) == 0) ^ rx->pat_not;
    const bool new = mutt_regex_match(rx, bob_line);
    regex_free(&rx);

    const bool failed_common_behavior = !TEST_CHECK(old == new);
    if (failed_common_behavior)
      return false;
  }

  mutt_buffer_dealloc(&buf);
  return true;
}

void test_mutt_regex_match(void)
{
  bool (*tests[])(void) = { test_simple_cases, test_old_implementation };

  const size_t num_tests = sizeof(tests) / sizeof(tests[0]);

  for (size_t i = 0; i < num_tests; ++i)
    TEST_CHECK(tests[i]());
}
