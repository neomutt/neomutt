/**
 * @file
 * Test code for url_parse()
 *
 * @authors
 * Copyright (C) 2019 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"

struct UrlTest
{
  const char *source;  // source URL to parse
  bool valid;          // expected validity
  struct Url url;      // expected resulting URL
  const char *qs_elem; // expected elements of the query string, separated
                       // and terminated by a pipe '|' character
};

// clang-format off
static struct UrlTest test[] = {
  {
    "mailto:mail@example.com",
    true,
    {
      U_MAILTO,
      NULL,
      NULL,
      NULL,
      0,
      "mail@example.com"
    },
    NULL
  },
  {
    "mailto:mail@example.com?subject=see%20this&cc=me%40example.com",
    true,
    {
      U_MAILTO,
      NULL,
      NULL,
      NULL,
      0,
      "mail@example.com"
    },
    "subject|see this|cc|me@example.com|"
  },
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
    "pop://user@example.com@pop.example.com:234/some/where?encoding=binary"
    "&second=third&some%20space=%22quoted%20content%22",
    true,
    {
      U_POP,
      "user@example.com",
      NULL,
      "pop.example.com",
      234,
      "some/where",
    },
    "encoding|binary|second|third|some space|\"quoted content\"|"
  },
  {
    "snews://user@[2000:4860:0:2001::68]:563",
    true,
    {
      U_NNTPS,
      "user",
      NULL,
      "2000:4860:0:2001::68",
      563
    }
  },
  {
    "notmuch:///Users/bob/.mail/gmail?type=messages&query=tag%3Ainbox",
    true,
    {
      U_NOTMUCH,
      NULL,
      NULL,
      NULL,
      0,
      "/Users/bob/.mail/gmail"
    },
    "type|messages|query|tag:inbox|",
  },
  {
    "imaps://gmail.com/[GMail]/Sent messages",
    true,
    {
      U_IMAPS,
      NULL,
      NULL,
      "gmail.com",
      0,
      "[GMail]/Sent messages"
    }
  },
  {
    /* Invalid fragment (#) character, see also
     * https://github.com/neomutt/neomutt/issues/2276 */
    "mailto:a@b?subject=#",
    false,
  },
  {
    /* Correctly escaped fragment (#) chracter, see also
     * https://github.com/neomutt/neomutt/issues/2276 */
    "mailto:a@b?subject=%23",
    true,
    {
      U_MAILTO,
      NULL,
      NULL,
      NULL,
      0,
      "a@b"
    },
    "subject|#|"
  },
  {
    /* UTF-8 mailbox name */
    "imaps://foobar@gmail.com@imap.gmail.com/Отправленные письма",
    true,
    {
      U_IMAPS,
      "foobar@gmail.com",
      NULL,
      "imap.gmail.com",
      0,
      "Отправленные письма"
    }
  },
  {
    /* Notmuch queries */
    "notmuch://?query=folder:\"[Gmail]/Sent Mail\"",
    true,
    {
      U_NOTMUCH
    },
    "query|folder:\"[Gmail]/Sent Mail\"|"
  }
};
// clang-format on

void check_query_string(const char *exp, const struct UrlQueryList *act)
{
  char *next = NULL;
  char tmp[64] = { 0 };
  const struct UrlQuery *np = STAILQ_FIRST(act);
  while (exp && *exp)
  {
    next = strchr(exp, '|');
    mutt_str_copy(tmp, exp, next - exp + 1);
    exp = next + 1;
    if (!TEST_CHECK(strcmp(tmp, np->name) == 0))
    {
      TEST_MSG("Expected: <%s>", tmp);
      TEST_MSG("Actual  : <%s>", np->name);
    }

    next = strchr(exp, '|');
    mutt_str_copy(tmp, exp, next - exp + 1);
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

void test_url_parse(void)
{
  // struct Url *url_parse(const char *src);

  {
    TEST_CHECK(!url_parse(NULL));
  }

  {
    for (size_t i = 0; i < mutt_array_size(test); i++)
    {
      TEST_CASE(test[i].source);
      struct Url *url = url_parse(test[i].source);
      if (!TEST_CHECK(!((!!url) ^ (!!test[i].valid))))
      {
        TEST_MSG("Expected: %sNULL", test[i].valid ? "not " : "");
        TEST_MSG("Actual  : %sNULL", url ? "not " : "");
      }

      if (!url)
        continue;

      if (!TEST_CHECK(test[i].url.scheme == url->scheme))
      {
        TEST_MSG("Expected: %d", test[i].url.scheme);
        TEST_MSG("Actual  : %d", url->scheme);
      }
      if (!TEST_CHECK(mutt_str_equal(test[i].url.user, url->user)))
      {
        TEST_MSG("Expected: %s", test[i].url.user);
        TEST_MSG("Actual  : %s", url->user);
      }
      if (!TEST_CHECK(mutt_str_equal(test[i].url.pass, url->pass)))
      {
        TEST_MSG("Expected: %s", test[i].url.pass);
        TEST_MSG("Actual  : %s", url->pass);
      }
      if (!TEST_CHECK(mutt_str_equal(test[i].url.host, url->host)))
      {
        TEST_MSG("Expected: %s", test[i].url.host);
        TEST_MSG("Actual  : %s", url->host);
      }
      if (!TEST_CHECK(test[i].url.port == url->port))
      {
        TEST_MSG("Expected: %hu", test[i].url.port);
        TEST_MSG("Actual  : %hu", url->port);
      }
      if (!TEST_CHECK(mutt_str_equal(test[i].url.path, url->path)))
      {
        TEST_MSG("Expected: %s", test[i].url.path);
        TEST_MSG("Actual  : %s", url->path);
      }
      check_query_string(test[i].qs_elem, &url->query_strings);

      url_free(&url);
    }
  }

  {
    /* Test automatically generated URLs */
    const char *const al[] = { "imap", "imaps" };
    const char *const bl[] = { "", "user@", "user@host.com@", "user:pass@" };
    const char *const cl[] = { "host.com", "[12AB::EF89]", "127.0.0.1" };
    const char *const dl[] = { "", ":123" };
    const char *const el[] = { "", "/", "/path", "/path/one/two", "/path.one.two" };
    for (size_t a = 0; a < mutt_array_size(al); a++)
    {
      for (size_t b = 0; b < mutt_array_size(bl); b++)
      {
        for (size_t c = 0; c < mutt_array_size(cl); c++)
        {
          for (size_t d = 0; d < mutt_array_size(dl); d++)
          {
            for (size_t e = 0; e < mutt_array_size(el); e++)
            {
              char s[1024];
              snprintf(s, sizeof(s), "%s://%s%s%s%s", al[a], bl[b], cl[c], dl[d], el[e]);
              TEST_CASE(s);
              struct Url *u = url_parse(s);
              if (!TEST_CHECK(u != NULL))
              {
                TEST_MSG("Expected: parsed <%s>", s);
                TEST_MSG("Actual:   NULL");
              }
              url_free(&u);
            }
          }
        }
      }
    }
  }
}
