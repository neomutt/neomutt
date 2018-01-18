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
  TEST_CHECK_(len == sizeof(encoded)-1,
      "Expected %zu, got %zu", sizeof(encoded)-1, len);
  TEST_CHECK_(strcmp(buffer, encoded) == 0,
      "Expected %s, got %s", encoded, buffer);
}

void test_base64_decode(void)
{
  char buffer[16];
  int len = mutt_b64_decode(buffer, encoded);
  TEST_CHECK_(len == sizeof(clear)-1,
      "Expected %zu, got %zu", sizeof(clear)-1, len);
  buffer[len] = '\0';
  TEST_CHECK_(strcmp(buffer, clear) == 0,
      "Expected %s, got %s", clear, buffer);
}
