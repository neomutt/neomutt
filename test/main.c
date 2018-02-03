#include "acutest.h"
/******************************************************************************
 * Add your test cases to this list.
 *****************************************************************************/
#define NEOMUTT_TEST_LIST                                                      \
  NEOMUTT_TEST_ITEM(test_base64_encode)                                        \
  NEOMUTT_TEST_ITEM(test_base64_decode)                                        \
  NEOMUTT_TEST_ITEM(test_base64_lengths)                                       \
  NEOMUTT_TEST_ITEM(test_rfc2047)                                              \
  NEOMUTT_TEST_ITEM(test_md5)                                                  \
  NEOMUTT_TEST_ITEM(test_md5_ctx)                                              \
  NEOMUTT_TEST_ITEM(test_md5_ctx_bytes)                                        \
  NEOMUTT_TEST_ITEM(test_string_strfcpy)                                       \
  NEOMUTT_TEST_ITEM(test_string_strnfcpy)                                      \
  NEOMUTT_TEST_ITEM(test_file_tidy_path)

/******************************************************************************
 * You probably don't need to touch what follows.
 *****************************************************************************/
#define NEOMUTT_TEST_ITEM(x) void x(void);
NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM

TEST_LIST = {
#define NEOMUTT_TEST_ITEM(x) { #x, x },
  NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM
  { 0 }
};
