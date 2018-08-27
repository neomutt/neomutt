#define TEST_NO_MAIN
#include "acutest.h"
#include "email/address.h"
#include "mutt/memory.h"
#include <string.h>

#define TEST_CHECK_STR_EQ(expected, actual)                                    \
  do                                                                           \
  {                                                                            \
    if (!TEST_CHECK(strcmp(expected, actual) == 0))                            \
    {                                                                          \
      TEST_MSG("Expected: %s", expected);                                      \
      TEST_MSG("Actual  : %s", actual);                                        \
    }                                                                          \
  } while (false)

void test_addr_mbox_to_udomain(void)
{
  { /* edge cases */
    char *user = NULL;
    char *domain = NULL;

    if (!TEST_CHECK(mutt_addr_mbox_to_udomain("bobnodomain@", &user, &domain) == -1) ||
        !TEST_CHECK(mutt_addr_mbox_to_udomain("bobnodomain", &user, &domain) == -1) ||
        !TEST_CHECK(mutt_addr_mbox_to_udomain("@nobobohnoez", &user, &domain) == -1) ||
        !TEST_CHECK(mutt_addr_mbox_to_udomain("", &user, &domain) == -1))
    {
      TEST_MSG("bad return");
    }
  }

  { /* common */
    int ret = 0;
    const char *str = "bob@bobsdomain";

    char *user = NULL;
    char *domain = NULL;
    ret = mutt_addr_mbox_to_udomain(str, &user, &domain);

    if (!TEST_CHECK(ret == 0))
    {
      TEST_MSG("bad return");
      TEST_MSG("Expected: %d", 0);
      TEST_MSG("Actual  : %d", ret);
    }

    TEST_CHECK_STR_EQ("bob", user);
    TEST_CHECK_STR_EQ("bobsdomain", domain);

    FREE(&user);
    FREE(&domain);
  }

  { /* integration */
    char buf[256] = { 0 };
    char *per = "bobby bob";
    char *mbx = "bob@bobsdomain";

    struct Address addr = {
      .personal = per,
      .mailbox = mbx,
      .group = 0,
      .next = NULL,
      .is_intl = 0,
      .intl_checked = 0,
    };

    mutt_addr_write_single(buf, sizeof(buf), &addr, false);

    const char *expected = "bobby bob <bob@bobsdomain>";

    TEST_CHECK_STR_EQ(expected, buf);
  }

  { /* integration */
    char *per = "bobby bob";
    char *mbx = "bob@bobsdomain";

    struct Address addr = {
      .personal = per,
      .mailbox = mbx,
      .group = 0,
      .next = NULL,
      .is_intl = 0,
      .intl_checked = 0,
    };

    const char *expected = "bob@bobsdomain";
    const char *actual = mutt_addr_for_display(&addr);

    TEST_CHECK_STR_EQ(expected, actual);
  }
}
