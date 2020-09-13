#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"
#include "email/lib.h"

void test_email_header_free(void)
{
  char *first_header = "X-First: 0";
  char *second_header = "X-Second: 1";

  struct ListHead hdrlist = STAILQ_HEAD_INITIALIZER(hdrlist);
  struct ListNode *first = header_add(&hdrlist, first_header);
  struct ListNode *second = header_add(&hdrlist, second_header);

  {
    header_free(&hdrlist, first);
    TEST_CHECK(header_find(&hdrlist, first_header) == NULL); /* Removed expected header */
    TEST_CHECK(header_find(&hdrlist, second_header) != NULL); /* Other headers untouched */
  }

  {
    header_free(&hdrlist, second);
    TEST_CHECK(header_find(&hdrlist, second_header) == NULL);
    TEST_CHECK(STAILQ_FIRST(&hdrlist) == NULL); /* List now empty */
  }
}
