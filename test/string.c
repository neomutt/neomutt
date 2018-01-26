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
