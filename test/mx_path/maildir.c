/**
 * @file
 * Test code for the Maildir MxOps Path functions
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
#include "maildir/path.h"
#include "test_common.h"

void test_maildir_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "%s/maildir/apple",         "%s/maildir/apple",  0 }, // Real path
    { "%s/maildir/symlink/apple", "%s/maildir/apple",  0 }, // Symlink
    { "%s/maildir/missing",       NULL,               -1 }, // Missing
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = first;
    TEST_CASE(path.orig);
    path.type = MUTT_MAILDIR;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = maildir_path2_canon(&path);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.flags & MPATH_CANONICAL);
      TEST_CHECK(path.canon != NULL);
      TEST_CHECK(mutt_str_strcmp(path.canon, second) == 0);
    }
    FREE(&path.canon);
  }
}

void test_maildir_path2_compare(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "%s/maildir/apple",  "%s/maildir/apple",   0 }, // Match
    { "%s/maildir/apple",  "%s/maildir/orange", -1 }, // Differ
    { "%s/maildir/orange", "%s/maildir/apple",   1 }, // Differ
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path1 = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };
  struct Path path2 = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path1.canon = first;
    TEST_CASE(path1.canon);

    path2.canon = second;
    TEST_CASE(path2.canon);

    rc = maildir_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_maildir_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "%s/maildir/apple/child", "%s/maildir/apple",  0 },
    { "%s/maildir/empty/child", NULL,               -1 },
    { "/",                      NULL,               -1 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = first;
    TEST_CASE(path.orig);

    rc = maildir_path2_parent(&path, &parent);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(parent != NULL);
      TEST_CHECK(parent->orig != NULL);
      TEST_CHECK(parent->type == path.type);
      TEST_CHECK(parent->flags & MPATH_RESOLVED);
      TEST_CHECK(parent->flags & MPATH_TIDY);
      TEST_CHECK(mutt_str_strcmp(parent->orig, second) == 0);
      mutt_path_free(&parent);
    }
  }
}

void test_mh_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "%s/maildir/mh2/child",   "%s/maildir/mh2",  0 },
    { "%s/maildir/empty/child", NULL,             -1 },
    { "/",                      NULL,             -1 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = {
    .type = MUTT_MH,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = first;
    TEST_CASE(path.orig);

    rc = maildir_path2_parent(&path, &parent);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(parent != NULL);
      TEST_CHECK(parent->orig != NULL);
      TEST_CHECK(parent->type == path.type);
      TEST_CHECK(parent->flags & MPATH_RESOLVED);
      TEST_CHECK(parent->flags & MPATH_TIDY);
      TEST_CHECK(mutt_str_strcmp(parent->orig, second) == 0);
      mutt_path_free(&parent);
    }
  }
}

void test_maildir_path2_pretty(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "%s/maildir/apple.maildir",         "+maildir/apple.maildir",         1 },
    { "%s/maildir/symlink/apple.maildir", "+maildir/symlink/apple.maildir", 1 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };
  char folder[256] = { 0 };

  test_gen_path(folder, sizeof(folder), "%s");

  struct Path path = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = first;
    TEST_CASE(path.orig);

    rc = maildir_path2_pretty(&path, folder);
    TEST_CHECK(rc == tests[i].retval);
    if (rc > 0)
    {
      TEST_CHECK(path.pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(path.pretty, second) == 0);
    }
  }

  test_gen_path(first, sizeof(first),  "%s/maildir/apple.maildir");
  test_gen_path(second, sizeof(second), "~/maildir/apple.maildir");
  path.orig = first;
  HomeDir = test_get_test_dir();
  rc = maildir_path2_pretty(&path, "nowhere");
  TEST_CHECK(rc == 1);
  TEST_CHECK(path.pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(path.pretty, second) == 0);

  test_gen_path(first, sizeof(first), tests[0].first);
  test_gen_path(second, sizeof(second), tests[0].first);
  path.orig = first;
  HomeDir = "/home/another";
  rc = maildir_path2_pretty(&path, "nowhere");
  TEST_CHECK(rc == 0);
  TEST_CHECK(path.pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(path.pretty, second) == 0);
}

void test_maildir_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "%s/maildir/apple",          NULL,  0 }, // Normal, all 3 subdirs
    { "%s/maildir/banana",         NULL,  0 }, // Normal, just 'cur' subdir
    { "%s/maildir/symlink/banana", NULL,  0 }, // Symlink
    { "%s/maildir/cherry",         NULL, -1 }, // No subdirs
    { "%s/maildir/damson",         NULL, -1 }, // Unreadable
    { "%s/maildir/endive",         NULL, -1 }, // File
  };
  // clang-format on

  char first[256] = { 0 };

  struct Path path = { 0 };
  struct stat st;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);

    path.orig = first;
    TEST_CASE(path.orig);
    path.type = MUTT_UNKNOWN;
    path.flags = MPATH_NO_FLAGS;
    memset(&st, 0, sizeof(st));
    stat(path.orig, &st);
    rc = maildir_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_mh_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "%s/maildir/mh1",         NULL,  0 }, // Contains .mh_sequences
    { "%s/maildir/mh2",         NULL,  0 }, // Contains .xmhcache
    { "%s/maildir/symlink/mh2", NULL,  0 }, // Symlink
    { "%s/maildir/mh3",         NULL,  0 }, // Contains .mew_cache
    { "%s/maildir/mh4",         NULL,  0 }, // Contains .mew-cache
    { "%s/maildir/mh5",         NULL,  0 }, // Contains .sylpheed_cache
    { "%s/maildir/mh6",         NULL,  0 }, // Contains .overview
    { "%s/maildir/mh7",         NULL, -1 }, // Empty
    { "%s/maildir/mh8",         NULL, -1 }, // File
  };
  // clang-format on

  char first[256] = { 0 };

  struct Path path = { 0 };
  struct stat st;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);

    path.orig = first;
    TEST_CASE(path.orig);
    path.type = MUTT_UNKNOWN;
    path.flags = MPATH_NO_FLAGS;
    memset(&st, 0, sizeof(st));
    stat(path.orig, &st);
    rc = mh_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_maildir_path2_tidy(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "%s/./maildir/../maildir///apple", "%s/maildir/apple", 0 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = mutt_str_strdup(first);
    rc = maildir_path2_tidy(&path);
    TEST_CHECK(rc == 0);
    TEST_CHECK(path.orig != NULL);
    TEST_CHECK(path.flags & MPATH_TIDY);
    TEST_CHECK(strcmp(path.orig, second) == 0);
    FREE(&path.orig);
  }
}
