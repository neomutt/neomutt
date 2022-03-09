#define TEST_NO_MAIN
#define MAIN_C 1

#include "pattern/lib.h"

static void test_one_leak(const char *pattern)
{
  struct PatternList *p = mutt_pattern_comp(NULL, NULL, pattern, 0, NULL);
  mutt_pattern_free(&p);
}

void test_mutt_pattern_leak(void)
{
  test_one_leak("~E ~F | ~D");
  test_one_leak("~D | ~E ~F");
  test_one_leak("~D | (~E ~F)");
}
