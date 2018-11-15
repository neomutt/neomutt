#define TEST_NO_MAIN
#include "acutest.h"
#include "email/url.h"

static struct
{
  const char *source;
  bool valid;
  struct Url url;
} test[] = {
  {
    "foobar foobar",
    false,
    {}
  },
  {
    "imaps://foouser:foopass@imap.example.com:456",
    true,
    {
      U_IMAPS,
      "foouser",
      "foopass",
      "imap.example.com",
      456,
      NULL
    }
  }
};

void test_url(void)
{
  for (size_t i = 0; i < mutt_array_size(test); ++i)
  {
    struct Url *url = url_parse(test[i].source);
    if (!TEST_CHECK(!(!!url ^ !!test[i].valid)))
    {
      TEST_MSG("Expected: %sNULL", test[i].valid ? "not " : "");
      TEST_MSG("Actual  : %sNULL", url ? "not " : "");
    }

    if (!url)
    {
      continue;
    }

    if (!TEST_CHECK(test[i].url.scheme == url->scheme))
    {
      TEST_MSG("Expected: %d", test[i].url.scheme);
      TEST_MSG("Actual  : %d", url->scheme);
    }
    if (!TEST_CHECK(mutt_str_strcmp(test[i].url.user, url->user) == 0))
    {
      TEST_MSG("Expected: %s", test[i].url.user);
      TEST_MSG("Actual  : %s", url->user);
    }
    if (!TEST_CHECK(mutt_str_strcmp(test[i].url.pass, url->pass) == 0))
    {
      TEST_MSG("Expected: %s", test[i].url.pass);
      TEST_MSG("Actual  : %s", url->pass);
    }
    if (!TEST_CHECK(mutt_str_strcmp(test[i].url.host, url->host) == 0))
    {
      TEST_MSG("Expected: %s", test[i].url.host);
      TEST_MSG("Actual  : %s", url->host);
    }
    if (!TEST_CHECK(test[i].url.port == url->port))
    {
      TEST_MSG("Expected: %hu", test[i].url.port);
      TEST_MSG("Actual  : %hu", url->port);
    }
    if (!TEST_CHECK(mutt_str_strcmp(test[i].url.path, url->path) == 0))
    {
      TEST_MSG("Expected: %s", test[i].url.path);
      TEST_MSG("Actual  : %s", url->path);
    }

    // TODO - test query string

    url_free(&url);
  }
}
