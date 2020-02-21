/**
 * @file
 * Test code for the IMAP MxOps Path functions
 *
 * @authors
 * Copyright (C) 2020 Richard Russon <rich@flatcap.org>
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
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "common.h"
#include "imap/path.h"

void test_imap_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "imap://user@example.com:123/INBOX",        "imap://user@example.com:123/INBOX",  0 },
    { "junk://user@example.com:123/INBOX",        "junk://user@example.com:123/INBOX", -1 },
    { "imap://example.com:123/INBOX",             "imap://user@example.com:123/INBOX",  0 },
    { "imap://user@example.com/INBOX",            "imap://user@example.com:123/INBOX",  0 },
    { "imap://user:secret@example.com:123/INBOX", "imap://user@example.com:123/INBOX",  0 },
    { "imap://example.com/INBOX",                 "imap://user@example.com:123/INBOX",  0 },
  };
  // clang-format on

  struct Path path = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);
    path.type = MUTT_IMAP;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = imap_path2_canon(&path, "user", 123);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.flags & MPATH_CANONICAL);
      TEST_CHECK(path.canon != NULL);
      TEST_CHECK(mutt_str_strcmp(path.canon, tests[i].second) == 0);
    }
    FREE(&path.canon);
  }
}

void test_imap_path2_compare(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "imap://user@example.com:123/INBOX",        "imap://user@example.com:123/INBOX",        0 }, // Match
    { "imap://user@example.com:123/INBOX",        "imaps://user@example.com:123/INBOX",      -1 }, // Scheme differs
    { "imaps://user@example.com:123/INBOX",       "imap://user@example.com:123/INBOX",        1 }, // Scheme differs
    { "imap://adam@example.com:123/INBOX",        "imap://zach@example.com:123/INBOX",       -1 }, // User differs
    { "imap://zach@example.com:123/INBOX",        "imap://adam@example.com:123/INBOX",        1 }, // User differs
    { "imap://adam@example.com:123/INBOX",        "imap://example.com:123/INBOX",             0 }, // User missing
    { "imap://adam:secret@example.com:123/INBOX", "imap://adam:magic@example.com:123/INBOX",  0 }, // Password ignored
    { "imap://user@example.com:123/INBOX",        "imap://user@flatcap.org:123/INBOX",       -1 }, // Host differs
    { "imap://user@flatcap.org:123/INBOX",        "imap://user@example.com:123/INBOX",        1 }, // Host differs
    { "imap://user@example.com:123/INBOX",        "imap://user@example.com:456/INBOX",       -1 }, // Port differs
    { "imap://user@example.com:456/INBOX",        "imap://user@example.com:123/INBOX",        1 }, // Port differs
    { "imap://user@example.com:456/INBOX",        "imap://user@example.com/INBOX",            0 }, // Port missing
    { "imap://user@example.com:123/INBOX",        "imap://user@example.com:123/junk",        -1 }, // Path differs
    { "imap://user@example.com:123/junk",         "imap://user@example.com:123/INBOX",        1 }, // Path differs
    { "imap://user@example.com:123/INBOX",        "imap://user@example.com:123/apple",       -1 }, // Inbox sorts first
    { "imap://user@example.com:123/apple",        "imap://user@example.com:123/INBOX",        1 }, // Inbox sorts first
  };
  // clang-format on

  struct Path path1 = {
    .type = MUTT_IMAP,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };
  struct Path path2 = {
    .type = MUTT_IMAP,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path1.canon = (char *) tests[i].first;
    TEST_CASE(path1.canon);

    path2.canon = (char *) tests[i].second;
    TEST_CASE(path2.canon);

    rc = imap_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_imap_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "imap://example.com/apple/banana/cherry", "imap://example.com/apple/banana",  0 },
    { "imap://example.com/apple/banana",        "imap://example.com/apple",         0 },
    { "imap://example.com/apple",               "imap://example.com/INBOX",         0 },
    { "imap://example.com/",                    NULL,                              -1 },
    { "imap://example.com/INBOX",               NULL,                              -1 },
    { "junk://example.com/junk",                NULL,                              -2 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_IMAP,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = imap_path2_parent(&path, '/', &parent);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(parent != NULL);
      TEST_CHECK(parent->orig != NULL);
      TEST_CHECK(parent->type == path.type);
      TEST_CHECK(parent->flags & MPATH_RESOLVED);
      TEST_CHECK(parent->flags & MPATH_TIDY);
      TEST_CHECK(mutt_str_strcmp(parent->orig, tests[i].second) == 0);
      mutt_path_free(&parent);
    }
  }
}

void test_imap_path2_pretty(void)
{
  static const char *folder = "imap://user@example.com:123/";
  // clang-format off
  static struct TestValue tests[] = {
    { "imap://example.com/INBOX",         "+INBOX", 1 },
    { "imaps://example.com/INBOX",        NULL,     0 }, // Scheme differs
    { "imap://flatcap.org/INBOX",         NULL,     0 }, // Host differs
    { "imap://another@example.com/INBOX", NULL,     0 }, // User differs
    { "imap://example.com:456/INBOX",     NULL,     0 }, // Port differs
    { "imap://example.com/",              NULL,     0 }, // Folder is entire path
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_IMAP,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = imap_path2_pretty(&path, folder);
    TEST_CHECK(rc == tests[i].retval);
    if (rc > 0)
    {
      TEST_CHECK(path.pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(path.pretty, tests[i].second) == 0);
    }
  }
}

void test_imap_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "imap://example.com/",  NULL,  0 },
    { "imaps://example.com/", NULL,  0 },
    { "pop://example.com/",   NULL, -1 },
  };
  // clang-format on

  struct Path path = { 0 };
  struct stat st;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);
    path.type = MUTT_UNKNOWN;
    path.flags = MPATH_NO_FLAGS;
    memset(&st, 0, sizeof(st));
    stat(path.orig, &st);
    rc = imap_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_imap_path2_tidy(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "imap://example.com/INBOX", "imap://example.com/INBOX",   0 },
    { "IMAP://example.com/INBOX", "imap://example.com/INBOX",   0 },
    { "imap://example.com/inbox", "imap://example.com/INBOX",   0 },
    { "imap://example.com/",      "imap://example.com/INBOX",   0 },
    { "imap://example.com",       "imap://example.com/INBOX",   0 },
    { "imaps://example.com/",     "imaps://example.com/INBOX",  0 },
    { "junk://example.com/",      "junk://example.com/",       -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_IMAP,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = mutt_str_strdup(tests[i].first);
    TEST_CASE(path.orig);

    rc = imap_path2_tidy(&path);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.orig != NULL);
      TEST_CHECK(path.flags & MPATH_TIDY);
      TEST_CHECK(mutt_str_strcmp(path.orig, tests[i].second) == 0);
    }
    FREE(&path.orig);
  }
}
