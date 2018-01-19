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

void test_base64_lengths(void)
{
    const char *in = "FuseMuse";
    char out1[32];
    char out2[32];
    size_t enclen;
    int declen;

    /* Encoding a zero-length string should fail */
    enclen = mutt_b64_encode(out1, in, 0, 32);
    TEST_CHECK_(enclen == 0,
        "Expected %zu, got %zu", 0, enclen);

    /* Decoding a zero-length string should fail, too */
    out1[0] = '\0';
    declen = mutt_b64_decode(out2, out1);
    TEST_CHECK_(declen == -1,
        "Expected %zu, got %zu", -1, declen);

    /* Encode one to eight bytes, check the lengths of the returned string */
    for (size_t i = 1; i <= 8; ++i)
    {
      enclen = mutt_b64_encode(out1, in, i, 32);
      size_t exp = ((i + 2) / 3) << 2;
      TEST_CHECK_(enclen == exp, "Expected %zu, got %zu", exp, enclen);
      declen = mutt_b64_decode(out2, out1);
      TEST_CHECK_(declen == i, "Expected %zu, got %zu", i, declen);
      out2[declen] = '\0';
      TEST_CHECK(strncmp(out2, in, i) == 0);
    }
}
