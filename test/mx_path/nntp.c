/**
 * @file
 * Test code for the NNTP MxOps Path functions
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
#include "nntp/path.h"

void test_nntp_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "news://user@example.com:123/alt.apple",        "news://user@example.com:123/alt.apple",  0 },
    { "junk://user@example.com:123/alt.apple",        "junk://user@example.com:123/alt.apple", -1 },
    { "news://example.com:123/alt.apple",             "news://user@example.com:123/alt.apple",  0 },
    { "news://user@example.com/alt.apple",            "news://user@example.com:123/alt.apple",  0 },
    { "news://user:secret@example.com:123/alt.apple", "news://user@example.com:123/alt.apple",  0 },
    { "news://example.com/alt.apple",                 "news://user@example.com:123/alt.apple",  0 },
  };
  // clang-format on

  struct Path path = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    path.canon = (char *) mutt_str_strdup(tests[i].first);
    TEST_CASE(path.orig);
    path.type = MUTT_NNTP;
    path.flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL;

    rc = nntp_path2_canon(&path, "user", 123);
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

void test_nntp_path2_compare(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "news://user@example.com:123/INBOX",        "news://user@example.com:123/INBOX",        0 }, // Match
    { "news://user@example.com:123/INBOX",        "snews://user@example.com:123/INBOX",      -1 }, // Scheme differs
    { "snews://user@example.com:123/INBOX",       "news://user@example.com:123/INBOX",        1 }, // Scheme differs
    { "news://adam@example.com:123/INBOX",        "news://zach@example.com:123/INBOX",       -1 }, // User differs
    { "news://zach@example.com:123/INBOX",        "news://adam@example.com:123/INBOX",        1 }, // User differs
    { "news://adam@example.com:123/INBOX",        "news://example.com:123/INBOX",             0 }, // User missing
    { "news://adam:secret@example.com:123/INBOX", "news://adam:magic@example.com:123/INBOX",  0 }, // Password ignored
    { "news://user@example.com:123/INBOX",        "news://user@flatcap.org:123/INBOX",       -1 }, // Host differs
    { "news://user@flatcap.org:123/INBOX",        "news://user@example.com:123/INBOX",        1 }, // Host differs
    { "news://user@example.com:123/INBOX",        "news://user@example.com:456/INBOX",       -1 }, // Port differs
    { "news://user@example.com:456/INBOX",        "news://user@example.com:123/INBOX",        1 }, // Port differs
    { "news://user@example.com:456/INBOX",        "news://user@example.com/INBOX",            0 }, // Port missing
    { "news://user@example.com:123/INBOX",        "news://user@example.com:123/junk",        -1 }, // Path differs
    { "news://user@example.com:123/junk",         "news://user@example.com:123/INBOX",        1 }, // Path differs
  };
  // clang-format on

  struct Path path1 = {
    .type = MUTT_NNTP,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };
  struct Path path2 = {
    .type = MUTT_NNTP,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path1.canon = (char *) tests[i].first;
    TEST_CASE(path1.canon);

    path2.canon = (char *) tests[i].second;
    TEST_CASE(path2.canon);

    rc = nntp_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_nntp_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "news://example.com/apple.banana.cherry", "news://example.com/apple.banana",  0 },
    { "news://example.com/apple.banana",        "news://example.com/apple",         0 },
    { "news://example.com/apple",               NULL,                              -1 },
    { "news://example.com/",                    NULL,                              -1 },
    { "junk://example.com/",                    NULL,                              -2 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_NNTP,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = nntp_path2_parent(&path, &parent);
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

void test_nntp_path2_pretty(void)
{
  static const char *folder = "news://user@example.com:123/";
  // clang-format off
  static struct TestValue tests[] = {
    { "news://example.com/alt.apple",         "+alt.apple", 1 },
    { "snews://example.com/alt.apple",        NULL,         0 }, // Scheme differs
    { "news://flatcap.org/alt.apple",         NULL,         0 }, // Host differs
    { "news://another@example.com/alt.apple", NULL,         0 }, // User differs
    { "news://example.com:456/alt.apple",     NULL,         0 }, // Port differs
    { "news://example.com/",                  NULL,         0 }, // Folder is entire path
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_NNTP,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = nntp_path2_pretty(&path, folder);
    TEST_CHECK(rc == tests[i].retval);
    if (rc > 0)
    {
      TEST_CHECK(path.pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(path.pretty, tests[i].second) == 0);
    }
  }
}

void test_nntp_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "news://example.com/",  NULL,  0 },
    { "snews://example.com/", NULL,  0 },
    { "imap://example.com/",  NULL, -1 },
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
    rc = nntp_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_nntp_path2_tidy(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "news://example.com/alt.apple", "news://example.com/alt.apple",  0 },
    { "NEWS://example.com/alt.apple", "news://example.com/alt.apple",  0 },
    { "junk://example.com/",          "junk://example.com/",          -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_NNTP,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = mutt_str_strdup(tests[i].first);
    TEST_CASE(path.orig);

    rc = nntp_path2_tidy(&path);
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
