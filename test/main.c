#include "acutest.h"

#include "base64.h"

TEST_LIST = {
  { "base64/encode", test_base64_encode },
  { "base64/decode", test_base64_decode },
  { 0 }
};
