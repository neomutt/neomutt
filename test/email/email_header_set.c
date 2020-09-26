#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include <string.h>
#include "mutt/lib.h"
#include "email/lib.h"

void test_email_header_set(void)
{
  // struct ListNode *header_set(struct ListHead *hdrlist, const struct Buffer *buf)
  const char *starting_value = "X-TestHeader: 0.57";
  const char *updated_value = "X-TestHeader: 6.28";

  struct ListHead hdrlist = STAILQ_HEAD_INITIALIZER(hdrlist);

  {
    /* Set value for first time */
    struct ListNode *got = header_set(&hdrlist, starting_value);
    TEST_CHECK(strcmp(got->data, starting_value) == 0); /* value set */
    TEST_CHECK(got == STAILQ_FIRST(&hdrlist)); /* header was added to list */
  }

  {
    /* Update value */
    struct ListNode *got = header_set(&hdrlist, updated_value);
    TEST_CHECK(strcmp(got->data, updated_value) == 0); /* value set*/
    TEST_CHECK(got == STAILQ_FIRST(&hdrlist));         /* no new header added*/
  }
  mutt_list_free(&hdrlist);
}
