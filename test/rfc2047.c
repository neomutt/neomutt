#define TEST_NO_MAIN
#include "acutest.h"

#include "mutt/rfc2047.h"
#include "mutt/charset.h"
#include "mutt/memory.h"
#include "mutt/string2.h"

#include <locale.h>

static const struct
{
    const char *original; /* the string as received in the original email */
    const char *decoded;  /* the expected plain-text string               */
    const char *encoded;  /* the string as it's encoded by NeoMutt        */
} test_data[] =
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
};

void test_rfc2047(void)
{
  setlocale(LC_ALL, "");
  Charset = "utf-8";

  for (size_t i = 0; i < mutt_array_size(test_data); ++i)
  {
    /* decode the original string */
    char *s = mutt_str_strdup(test_data[i].original);
    mutt_rfc2047_decode(&s);
    TEST_CHECK_(strcmp(s, test_data[i].decoded) == 0,
        "\nExpected: |%s|"
        "\nActual  : |%s|", test_data[i].decoded, s);
    FREE(&s);

    /* encode the expected result */
    s = mutt_str_strdup(test_data[i].decoded);
    mutt_rfc2047_encode(&s, NULL, 0, "utf-8");
    TEST_CHECK_(strcmp(s, test_data[i].encoded) == 0,
        "\nExpected: |%s|"
        "\nActual  : |%s|", test_data[i].encoded, s);
    FREE(&s);

    /* decode the encoded result */
    s = mutt_str_strdup(test_data[i].encoded);
    mutt_rfc2047_decode(&s);
    TEST_CHECK_(strcmp(s, test_data[i].decoded) == 0,
        "\nExpected: |%s|"
        "\nActual  : |%s|", test_data[i].decoded, s);
    FREE(&s);
  }
}
