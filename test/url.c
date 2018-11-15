#define TEST_NO_MAIN
#include "acutest.h"
#include "email/url.h"

typedef bool (*check_query_string)(const struct UrlQueryStringHead *q);

bool check_query_string_eq(const struct UrlQueryStringHead *q)
{
  const struct UrlQueryString *np = STAILQ_FIRST(q);
  if (strcmp(np->name, "encoding") != 0)
    return false;
  if (strcmp(np->value, "binary") != 0)
    return false;
  if (STAILQ_NEXT(np, entries) != NULL)
    return false;
  return true;
}

static struct
{
  const char *source;
  bool valid;
  check_query_string f;
  struct Url url;
} test[] = {
  {
    "foobar foobar",
    false,
  },
  {
    "imaps://foouser:foopass@imap.example.com:456",
    true,
    NULL,
    {
      U_IMAPS,
      "foouser",
      "foopass",
      "imap.example.com",
      456,
      NULL
    }
  },
  {
    "SmTp://user@example.com", /* scheme is lower-cased */
    true,
    NULL,
    {
      U_SMTP,
      "user",
      NULL,
      "example.com",
      0,
      NULL
    }
  },
  {
    "pop://user@example.com@pop.example.com:234/some/where?encoding=binary",
    true,
    check_query_string_eq,
    {
      U_POP,
      "user@example.com",
      NULL,
      "pop.example.com",
      234,
      "some/where",
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
    if (test[i].f)
    {
      TEST_CHECK(test[i].f(&url->query_strings));
    }

    url_free(&url);
  }
}
