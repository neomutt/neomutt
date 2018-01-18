#include "acutest.h"

#define NEOMUTT_TEST_LIST \
  NEOMUTT_TEST_ITEM(test_base64_encode) \
  NEOMUTT_TEST_ITEM(test_base64_decode) \

#define NEOMUTT_TEST_ITEM(x) void x(void);
NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM

TEST_LIST = {
#define NEOMUTT_TEST_ITEM(x) { #x, x },
  NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM
  { 0}
};
