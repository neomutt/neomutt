#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"

void test_email_header_add(void)
{
  // struct ListNode *header_add(struct ListHead *hdrlist, const struct Buffer *buf)
  const char *header = "X-TestHeader: 123";

  struct ListHead hdrlist = STAILQ_HEAD_INITIALIZER(hdrlist);

  {
    struct ListNode *n = header_add(&hdrlist, header);
    TEST_CHECK(strcmp(n->data, header) == 0);       /* header stored in node */
    TEST_CHECK(n == header_find(&hdrlist, header)); /* node added to list */
  }
  mutt_list_free(&hdrlist);
}
