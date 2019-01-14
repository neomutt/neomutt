#define TEST_NO_MAIN
#include "acutest.h"

#include "mutt/list.h"
#include "mutt/string2.h"

void test_string_strfcpy(void)
{
  char src[20] = "\0";
  char dst[10];

  { /* empty */
    size_t len = mutt_str_strfcpy(dst, src, sizeof(dst));
    if (!TEST_CHECK(len == 0))
    {
      TEST_MSG("Expected: %zu", 0);
      TEST_MSG("Actual  : %zu", len);
    }
  }

  { /* normal */
    const char trial[] = "Hello";
    mutt_str_strfcpy(src, trial, sizeof(src)); /* let's eat our own dogfood */
    size_t len = mutt_str_strfcpy(dst, src, sizeof(dst));
    if (!TEST_CHECK(len == sizeof(trial) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(trial) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, trial) == 0))
    {
      TEST_MSG("Expected: %s", trial);
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* too long */
    const char trial[] = "Hello Hello Hello";
    mutt_str_strfcpy(src, trial, sizeof(src));
    size_t len = mutt_str_strfcpy(dst, src, sizeof(dst));
    if (!TEST_CHECK(len == sizeof(dst) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(dst) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
  }
}

void test_string_strnfcpy(void)
{
  const char src[] = "One Two Three Four Five";
  char dst[10];
  char big[32];

  { /* copy a substring */
    size_t len = mutt_str_strnfcpy(dst, src, 3, sizeof(dst));
    if (!TEST_CHECK(len == 3))
    {
      TEST_MSG("Expected: %zu", 3);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, "One") == 0))
    {
      TEST_MSG("Expected: %s", "One");
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* try to copy the whole available length */
    size_t len = mutt_str_strnfcpy(dst, src, sizeof(dst), sizeof(dst));
    if (!TEST_CHECK(len == sizeof(dst) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(dst) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, "One Two T") == 0))
    {
      TEST_MSG("Expected: %s", "One Two T");
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* try to copy more than it fits */
    size_t len = mutt_str_strnfcpy(dst, src, strlen(src), sizeof(dst));
    if (!TEST_CHECK(len == sizeof(dst) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(dst) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(dst, "One Two T") == 0))
    {
      TEST_MSG("Expected: %s", "One Two T");
      TEST_MSG("Actual  : %s", dst);
    }
  }

  { /* try to copy more than available */
    size_t len = mutt_str_strnfcpy(big, src, sizeof(big), sizeof(big));
    if (!TEST_CHECK(len == sizeof(src) - 1))
    {
      TEST_MSG("Expected: %zu", sizeof(src) - 1);
      TEST_MSG("Actual  : %zu", len);
    }
    if (!TEST_CHECK(strcmp(big, src) == 0))
    {
      TEST_MSG("Expected: %s", src);
      TEST_MSG("Actual  : %s", big);
    }
  }
}

void test_string_strcasestr(void)
{
  char *haystack_same_size = "hello";
  char *haystack_larger = "hello, world!";
  char *haystack_smaller = "heck";
  char *haystack_mid = "test! hello, world";
  char *haystack_end = ", world! hello";

  char *empty = "";

  const char *needle = "hEllo";
  const char *needle_lower = "hello";
  const char *nonexistent = "goodbye";

  { // Check NULL conditions
    const char *retval1 = mutt_str_strcasestr(NULL, NULL);
    const char *retval2 = mutt_str_strcasestr(NULL, needle);
    const char *retval3 = mutt_str_strcasestr(haystack_same_size, NULL);

    TEST_CHECK_(retval1 == NULL, "Expected: %s, Actual %s", NULL, retval1);
    TEST_CHECK_(retval2 == NULL, "Expected: %s, Actual %s", NULL, retval2);
    TEST_CHECK_(retval3 == NULL, "Expected: %s, Actual %s", NULL, retval3);
  }

  { // Check empty strings
    const char *retval1 = mutt_str_strcasestr(empty, empty);
    const char *retval2 = mutt_str_strcasestr(empty, needle);
    const char *retval3 = mutt_str_strcasestr(haystack_same_size, empty);

    const char *empty_expected = strstr(empty, empty);

    TEST_CHECK_(retval1 == empty_expected, "Expected: %s, Actual %s", "", retval1);
    TEST_CHECK_(retval2 == NULL, "Expected: %s, Actual %s", NULL, retval2);
    TEST_CHECK_(retval3 == haystack_same_size, "Expected: %s, Actual %s", haystack_same_size, retval3);
  }

  { // Check instance where needle is not in haystack.
    const char *retval1 = mutt_str_strcasestr(haystack_same_size, nonexistent);
    const char *retval2 = mutt_str_strcasestr(haystack_smaller, nonexistent);
    const char *retval3 = mutt_str_strcasestr(haystack_larger, nonexistent);

    TEST_CHECK_(retval1 == NULL, "Expected: %s, Actual %s", NULL, retval1);
    TEST_CHECK_(retval2 == NULL, "Expected: %s, Actual %s", NULL, retval2);
    TEST_CHECK_(retval3 == NULL, "Expected: %s, Actual %s", NULL, retval3);
  }

  { // Check instance haystack is the same length as the needle and needle exists.
    const char *retval = mutt_str_strcasestr(haystack_same_size, needle);

    TEST_CHECK_(retval == haystack_same_size, "Expected: %s, Actual: %s", haystack_same_size, retval);
  }

  { // Check instance haystack is larger and needle exists.
    const char *retval = mutt_str_strcasestr(haystack_larger, needle);
    const char *expected = strstr(haystack_larger, needle_lower);

    TEST_CHECK_(retval == expected, "Expected: %s, Actual: %s", haystack_larger, retval);
  }

  { // Check instance where word is in the middle
    const char *retval = mutt_str_strcasestr(haystack_mid, needle);
    const char *expected = strstr(haystack_mid, needle_lower);

    TEST_CHECK_(retval == expected, "Expected: %s, Actual: %s", expected, retval);
  }

  { // Check instance where needle is at the end,
    const char *retval = mutt_str_strcasestr(haystack_end, needle);
    const char *expected = strstr(haystack_end, needle_lower);

    TEST_CHECK_(retval == expected, "Expected: %s, Actual: %s", expected, retval);
  }

  { // Check instance where haystack is smaller than needle.
    const char *retval = mutt_str_strcasestr(haystack_smaller, needle);

    TEST_CHECK_(retval == NULL, "Expected: %s, Actual: %s", NULL, retval);
  }
}


void print_compared_list(struct ListHead expected, struct ListHead actual)
{
  struct ListNode *np = NULL;

  TEST_MSG("Expected:");
  STAILQ_FOREACH(np, &expected, entries)
  {
    TEST_MSG("* '%s'", np->data);
  }

  TEST_MSG("Actual:");
  STAILQ_FOREACH(np, &actual, entries)
  {
    TEST_MSG("* '%s'", np->data);
  }

}

void test_string_split(void)
{
  char *one_word = "hello";
  char *two_words = "hello world";
  char *words = "hello neomutt world! what's up?";
  char *ending_sep = "hello world ";
  char *starting_sep = " hello world";
  char *other_sep = "hello,world";
  char *empty = "";

  { // Check NULL conditions
    struct ListHead retval1 = mutt_str_split(NULL, ' ');
    struct ListHead retval2 = mutt_str_split(empty, ' ');

    if (!TEST_CHECK(STAILQ_EMPTY(&retval1)))
      TEST_MSG("Expected: empty");
    if (!TEST_CHECK(STAILQ_EMPTY(&retval2)))
      TEST_MSG("Expected: empty");
  }

  { // Check different words
    struct ListHead retval1 = mutt_str_split(one_word, ' ');
    struct ListHead retval2 = mutt_str_split(two_words, ' ');
    struct ListHead retval3 = mutt_str_split(words, ' ');
    struct ListHead retval4 = mutt_str_split(ending_sep, ' ');
    struct ListHead retval5 = mutt_str_split(starting_sep, ' ');
    struct ListHead retval6 = mutt_str_split(other_sep, ',');

    struct ListHead expectedval1 = STAILQ_HEAD_INITIALIZER(expectedval1);
    mutt_list_insert_tail(&expectedval1, "hello");
    if (!TEST_CHECK(mutt_list_compare(&expectedval1, &retval1)))
      print_compared_list(expectedval1, retval1);

    struct ListHead expectedval2 = STAILQ_HEAD_INITIALIZER(expectedval2);
    mutt_list_insert_tail(&expectedval2, "hello");
    mutt_list_insert_tail(&expectedval2, "world");
    if (!TEST_CHECK(mutt_list_compare(&expectedval2, &retval2)))
      print_compared_list(expectedval2, retval2);

    struct ListHead expectedval3 = STAILQ_HEAD_INITIALIZER(expectedval3);
    mutt_list_insert_tail(&expectedval3, "hello");
    mutt_list_insert_tail(&expectedval3, "neomutt");
    mutt_list_insert_tail(&expectedval3, "world!");
    mutt_list_insert_tail(&expectedval3, "what's");
    mutt_list_insert_tail(&expectedval3, "up?");
    if (!TEST_CHECK(mutt_list_compare(&expectedval3, &retval3)))
      print_compared_list(expectedval3, retval3);

    struct ListHead expectedval4 = STAILQ_HEAD_INITIALIZER(expectedval4);
    mutt_list_insert_tail(&expectedval4, "hello");
    mutt_list_insert_tail(&expectedval4, "world");
    mutt_list_insert_tail(&expectedval4, "");
    if (!TEST_CHECK(mutt_list_compare(&expectedval4, &retval4)))
      print_compared_list(expectedval4, retval4);

    struct ListHead expectedval5 = STAILQ_HEAD_INITIALIZER(expectedval5);
    mutt_list_insert_tail(&expectedval5, "");
    mutt_list_insert_tail(&expectedval5, "hello");
    mutt_list_insert_tail(&expectedval5, "world");
    if (!TEST_CHECK(mutt_list_compare(&expectedval5, &retval5)))
      print_compared_list(expectedval5, retval5);

    struct ListHead expectedval6 = STAILQ_HEAD_INITIALIZER(expectedval6);
    mutt_list_insert_tail(&expectedval6, "hello");
    mutt_list_insert_tail(&expectedval6, "world");
    if (!TEST_CHECK(mutt_list_compare(&expectedval6, &retval6)))
      print_compared_list(expectedval6, retval6);
  }
}

