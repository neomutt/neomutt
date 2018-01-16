#define TEST_NO_MAIN
#include "acutest.h"

#include "mutt/base64.h"

#include <string.h>

static const char clear[] = "Hello";
static const char encoded[] = "SGVsbG8=";

void test_base64_encode(void)
{
  char buffer[16];
  size_t len = mutt_b64_encode(buffer, clear, sizeof(clear)-1, sizeof(buffer));
  TEST_CHECK(len == sizeof(encoded)-1);
  TEST_CHECK(strcmp(buffer, encoded) == 0);
}

void test_base64_decode(void)
{
  char buffer[16];
  int len = mutt_b64_decode(buffer, encoded);
  TEST_CHECK(len == sizeof(clear)-1);
  buffer[len] = '\0';
  TEST_CHECK(strcmp(buffer, clear) == 0);
}
