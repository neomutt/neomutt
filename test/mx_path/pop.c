/**
 * @file
 * Test code for the POP MxOps Path functions
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
#include "pop/path.h"

void test_pop_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "pop://user@example.com:123/INBOX",        "pop://user@example.com:123/INBOX",   0 },
    { "junk://user@example.com:123/INBOX",       "junk://user@example.com:123/INBOX", -1 },
    { "pop://example.com:123/INBOX",             "pop://user@example.com:123/INBOX",   0 },
    { "pop://user@example.com/INBOX",            "pop://user@example.com:123/INBOX",   0 },
    { "pop://user:secret@example.com:123/INBOX", "pop://user@example.com:123/INBOX",   0 },
    { "pop://example.com/INBOX",                 "pop://user@example.com:123/INBOX",   0 },
  };
  // clang-format on

  struct Path path = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);
    path.type = MUTT_POP;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = pop_path2_canon(&path, "user", 123);
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

void test_pop_path2_compare(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "pop://user@example.com:123/INBOX",        "pop://user@example.com:123/INBOX",        0 }, // Match
    { "pop://user@example.com:123/INBOX",        "pops://user@example.com:123/INBOX",      -1 }, // Scheme differs
    { "pops://user@example.com:123/INBOX",       "pop://user@example.com:123/INBOX",        1 }, // Scheme differs
    { "pop://adam@example.com:123/INBOX",        "pop://zach@example.com:123/INBOX",       -1 }, // User differs
    { "pop://zach@example.com:123/INBOX",        "pop://adam@example.com:123/INBOX",        1 }, // User differs
    { "pop://adam@example.com:123/INBOX",        "pop://example.com:123/INBOX",             0 }, // User missing
    { "pop://adam:secret@example.com:123/INBOX", "pop://adam:magic@example.com:123/INBOX",  0 }, // Password ignored
    { "pop://user@example.com:123/INBOX",        "pop://user@flatcap.org:123/INBOX",       -1 }, // Host differs
    { "pop://user@flatcap.org:123/INBOX",        "pop://user@example.com:123/INBOX",        1 }, // Host differs
    { "pop://user@example.com:123/INBOX",        "pop://user@example.com:456/INBOX",       -1 }, // Port differs
    { "pop://user@example.com:456/INBOX",        "pop://user@example.com:123/INBOX",        1 }, // Port differs
    { "pop://user@example.com:456/INBOX",        "pop://user@example.com/INBOX",            0 }, // Port missing
    { "pop://user@example.com:123/INBOX",        "pop://user@example.com:123/junk",        -1 }, // Path differs
    { "pop://user@example.com:123/junk",         "pop://user@example.com:123/INBOX",        1 }, // Path differs
  };
  // clang-format on

  struct Path path1 = {
    .type = MUTT_POP,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };
  struct Path path2 = {
    .type = MUTT_POP,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path1.canon = (char *) tests[i].first;
    TEST_CASE(path1.canon);

    path2.canon = (char *) tests[i].second;
    TEST_CASE(path2.canon);

    rc = pop_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_pop_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "pop://example.com/", NULL, -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_POP,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = pop_path2_parent(&path, &parent);
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

void test_pop_path2_pretty(void)
{
  static const char *folder = "pop://user@example.com:123/";
  // clang-format off
  static struct TestValue tests[] = {
    { "pop://example.com/INBOX",         "+INBOX", 1 },
    { "pops://example.com/INBOX",        NULL,     0 }, // Scheme differs
    { "pop://flatcap.org/INBOX",         NULL,     0 }, // Host differs
    { "pop://another@example.com/INBOX", NULL,     0 }, // User differs
    { "pop://example.com:456/INBOX",     NULL,     0 }, // Port differs
    { "pop://example.com/",              NULL,     0 }, // Folder is entire path
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_POP,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = pop_path2_pretty(&path, folder);
    TEST_CHECK(rc == tests[i].retval);
    if (rc > 0)
    {
      TEST_CHECK(path.pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(path.pretty, tests[i].second) == 0);
    }
  }
}

void test_pop_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "pop://example.com/",  NULL,  0 },
    { "pops://example.com/", NULL,  0 },
    { "imap://example.com",  NULL, -1 },
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
    rc = pop_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_pop_path2_tidy(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "pop://example.com/INBOX", "pop://example.com/INBOX",   0 },
    { "POP://example.com/INBOX", "pop://example.com/INBOX",   0 },
    { "pop://example.com/inbox", "pop://example.com/INBOX",   0 },
    { "pop://example.com/",      "pop://example.com/INBOX",   0 },
    { "pop://example.com",       "pop://example.com/INBOX",   0 },
    { "pops://example.com/",     "pops://example.com/INBOX",  0 },
    { "junk://example.com/",     "junk://example.com/",      -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_POP,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = mutt_str_strdup(tests[i].first);
    TEST_CASE(path.orig);

    rc = pop_path2_tidy(&path);
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
