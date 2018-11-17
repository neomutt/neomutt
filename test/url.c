#define TEST_NO_MAIN
#include "acutest.h"
#include "email/url.h"
#include "mutt/string2.h"

void check_query_string(const char *exp, const struct UrlQueryStringHead *act)
{
  char *next;
  char tmp[64];
  const struct UrlQueryString *np = STAILQ_FIRST(act);
  while (exp && *exp)
  {
    next = strchr(exp, '|');
    mutt_str_strfcpy(tmp, exp, next - exp + 1);
    exp = next + 1;
    if (!TEST_CHECK(strcmp(tmp, np->name) == 0))
    {
      TEST_MSG("Expected: <%s>", tmp);
      TEST_MSG("Actual  : <%s>", np->name);
    }

    next = strchr(exp, '|');
    mutt_str_strfcpy(tmp, exp, next - exp + 1);
    exp = next + 1;
    if (!TEST_CHECK(strcmp(tmp, np->value) == 0))
    {
      TEST_MSG("Expected: <%s>", tmp);
      TEST_MSG("Actual  : <%s>", np->value);
    }

    np = STAILQ_NEXT(np, entries);
  }

  if (!TEST_CHECK(np == NULL))
  {
    TEST_MSG("Expected: NULL");
    TEST_MSG("Actual  : (%s, %s)", np->name, np->value);
  }
}

static struct
{
  const char *source;   /* source URL to parse                              */
  bool valid;           /* expected validity                                */
  struct Url url;       /* expected resulting URL                           */
  const char *qs_elem;  /* expected elements of the query string, separated
                           and terminated by a pipe '|' character           */
} test[] = {
  {
    "foobar foobar",
    false,
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
    },
    NULL
  },
  {
    "SmTp://user@example.com", /* scheme is lower-cased */
    true,
    {
      U_SMTP,
      "user",
      NULL,
      "example.com",
      0,
      NULL
    },
    NULL
  },
  {
    "pop://user@example.com@pop.example.com:234/some/where?encoding=binary",
    true,
    {
      U_POP,
      "user@example.com",
      NULL,
      "pop.example.com",
      234,
      "some/where",
    },
    "encoding|binary|"
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
    check_query_string(test[i].qs_elem, &url->query_strings);

    url_free(&url);
  }
}
