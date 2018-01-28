#define TEST_NO_MAIN
#include "acutest.h"

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
