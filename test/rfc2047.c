#define TEST_NO_MAIN
#include "acutest.h"

#include "mutt/charset.h"
#include "mutt/memory.h"
#include "mutt/rfc2047.h"
#include "mutt/string2.h"

#include <locale.h>

static const struct
{
  const char *original; /* the string as received in the original email */
  const char *decoded;  /* the expected plain-text string               */
  const char *encoded;  /* the string as it's encoded by NeoMutt        */
} test_data[] =
    /* clang-format off */
{
  {
    /* The string is split in the middle of a multi-byte sequence */
      "=?UTF-8?Q?Kvie=C4=8Diame=20drauge=20pildyti=20ESO=20pasi=C5=BEad=C4?="
      "\n =?UTF-8?Q?=97jim=C5=B3=20girliand=C4=85!?="
    , "Kviečiame drauge pildyti ESO pasižadėjimų girliandą!"
    , "=?utf-8?Q?Kvie=C4=8Diame_drauge_pildyti_ESO_pasi=C5=BEad=C4=97jim=C5=B3_g?="
      "\n\t=?utf-8?Q?irliand=C4=85!?="
  },
  {
    /* Reduced test case for split multi-byte sequence */
      "=?utf-8?Q?=C4?==?utf-8?Q?=97?="
    , "ė"
    , "=?utf-8?B?xJc=?="
  },
  {
    /* Make sure spaces after an encoded word are kept */
      "=?utf-8?B?6IGq5piO55qE?=    Hello"
    , "聪明的    Hello"
    , "=?utf-8?B?6IGq5piO55qE?=    Hello"
  },
  {
    /* Make sure spaces before an encoded word are kept */
      "=?UTF-8?Q?Hello____=E8=81=AA=E6=98=8E=E7=9A=84?=" /* Roundcube style */
    , "Hello    聪明的"
    , "Hello    =?utf-8?B?6IGq5piO55qE?="
  },
  {
    /* Make sure spaces between encoded words are kept */
      "=?utf-8?B?6IGq5piO55qEICAgIOiBquaYjueahA==?="
    , "聪明的    聪明的"
    , "=?utf-8?B?6IGq5piO55qEICAgIOiBquaYjueahA==?="
  }
};
/* clang-format on */

void test_rfc2047(void)
{
  if (!TEST_CHECK((setlocale(LC_ALL, "en_US.UTF-8") != NULL) ||
                  (setlocale(LC_ALL, "C.UTF-8") != NULL)))
  {
    TEST_MSG("Cannot set locale to (en_US|C).UTF-8");
    return;
  }

  Charset = "utf-8";

  for (size_t i = 0; i < mutt_array_size(test_data); ++i)
  {
    /* decode the original string */
    char *s = mutt_str_strdup(test_data[i].original);
    mutt_rfc2047_decode(&s);
    if (!TEST_CHECK(strcmp(s, test_data[i].decoded) == 0))
    {
      TEST_MSG("Iteration: %zu", i);
      TEST_MSG("Expected : %s", test_data[i].decoded);
      TEST_MSG("Actual   : %s", s);
    }
    FREE(&s);

    /* encode the expected result */
    s = mutt_str_strdup(test_data[i].decoded);
    mutt_rfc2047_encode(&s, NULL, 0, "utf-8");
    if (!TEST_CHECK(strcmp(s, test_data[i].encoded) == 0))
    {
      TEST_MSG("Iteration: %zu", i);
      TEST_MSG("Expected : %s", test_data[i].encoded);
      TEST_MSG("Actual   : %s", s);
    }
    FREE(&s);

    /* decode the encoded result */
    s = mutt_str_strdup(test_data[i].encoded);
    mutt_rfc2047_decode(&s);
    if (!TEST_CHECK(strcmp(s, test_data[i].decoded) == 0))
    {
      TEST_MSG("Iteration: %zu", i);
      TEST_MSG("Expected : %s", test_data[i].decoded);
      TEST_MSG("Actual   : %s", s);
    }
    FREE(&s);
  }
}
