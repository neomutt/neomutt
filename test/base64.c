#define TEST_NO_MAIN
#include "acutest.h"

#include "mutt/base64.h"

#include <string.h>

static const char clear[] = "Hello";
static const char encoded[] = "SGVsbG8=";

void test_base64_encode(void)
{
  char buffer[16];
  size_t len = mutt_b64_encode(buffer, clear, sizeof(clear) - 1, sizeof(buffer));
  if (!TEST_CHECK(len == sizeof(encoded) - 1))
  {
    TEST_MSG("Expected: %zu", sizeof(encoded) - 1);
    TEST_MSG("Actual  : %zu", len);
  }
  if (!TEST_CHECK(strcmp(buffer, encoded) == 0))
  {
    TEST_MSG("Expected: %zu", encoded);
    TEST_MSG("Actual  : %zu", buffer);
  }
}

void test_base64_decode(void)
{
  char buffer[16];
  int len = mutt_b64_decode(buffer, encoded, sizeof(buffer));
  if (!TEST_CHECK(len == sizeof(clear) - 1))
  {
    TEST_MSG("Expected: %zu", sizeof(clear) - 1);
    TEST_MSG("Actual  : %zu", len);
  }
  buffer[len] = '\0';
  if (!TEST_CHECK(strcmp(buffer, clear) == 0))
  {
    TEST_MSG("Expected: %s", clear);
    TEST_MSG("Actual  : %s", buffer);
  }
}

void test_base64_lengths(void)
{
  const char *in = "FuseMuse";
  char out1[32];
  char out2[32];
  size_t enclen;
  int declen;

  /* Encoding a zero-length string should fail */
  enclen = mutt_b64_encode(out1, in, 0, 32);
  if (!TEST_CHECK(enclen == 0))
  {
    TEST_MSG("Expected: %zu", 0);
    TEST_MSG("Actual  : %zu", enclen);
  }

  /* Decoding a zero-length string should fail, too */
  out1[0] = '\0';
  declen = mutt_b64_decode(out2, out1, sizeof(out2));
  if (!TEST_CHECK(declen == -1))
  {
    TEST_MSG("Expected: %zu", -1);
    TEST_MSG("Actual  : %zu", declen);
  }

  /* Encode one to eight bytes, check the lengths of the returned string */
  for (size_t i = 1; i <= 8; ++i)
  {
    enclen = mutt_b64_encode(out1, in, i, 32);
    size_t exp = ((i + 2) / 3) << 2;
    if (!TEST_CHECK(enclen == exp))
    {
      TEST_MSG("Expected: %zu", exp);
      TEST_MSG("Actual  : %zu", enclen);
    }
    declen = mutt_b64_decode(out2, out1, sizeof(out2));
    if (!TEST_CHECK(declen == i))
    {
      TEST_MSG("Expected: %zu", i);
      TEST_MSG("Actual  : %zu", declen);
    }
    out2[declen] = '\0';
    if (!TEST_CHECK(strncmp(out2, in, i) == 0))
    {
      TEST_MSG("Expected: %s", in);
      TEST_MSG("Actual  : %s", out2);
    }
  }
}
