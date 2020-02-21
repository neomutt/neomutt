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
#include "acutest.h"
#include "config.h"
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "common.h"
#include "maildir/path.h"

void test_maildir_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "/home/mutt/path/maildir/apple",         "/home/mutt/path/maildir/apple",  0 }, // Real path
    { "/home/mutt/path/maildir/symlink/apple", "/home/mutt/path/maildir/apple",  0 }, // Symlink
    { "/home/mutt/path/maildir/missing",       NULL,                            -1 }, // Missing
  };
  // clang-format on

  struct Path path = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = tests[i].first;
    TEST_CASE(path.orig);
    path.type = MUTT_MAILDIR;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = maildir_path2_canon(&path);
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

void test_maildir_path2_compare(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "/home/mutt/path/maildir/apple",  "/home/mutt/path/maildir/apple",   0 }, // Match
    { "/home/mutt/path/maildir/apple",  "/home/mutt/path/maildir/orange", -1 }, // Differ
    { "/home/mutt/path/maildir/orange", "/home/mutt/path/maildir/apple",   1 }, // Differ
  };
  // clang-format on

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
    path1.canon = (char *) tests[i].first;
    TEST_CASE(path1.canon);

    path2.canon = (char *) tests[i].second;
    TEST_CASE(path2.canon);

    rc = maildir_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_maildir_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "/home/mutt/path/maildir/apple/child", "/home/mutt/path/maildir/apple",  0 },
    { "/home/mutt/path/maildir/empty/child", NULL,                            -1 },
    { "/",                                   NULL,                            -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = tests[i].first;
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
      TEST_CHECK(mutt_str_strcmp(parent->orig, tests[i].second) == 0);
      mutt_path_free(&parent);
    }
  }
}

void test_mh_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "/home/mutt/path/maildir/mh2/child",   "/home/mutt/path/maildir/mh2",  0 },
    { "/home/mutt/path/maildir/empty/child", NULL,                          -1 },
    { "/",                                   NULL,                          -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_MH,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = tests[i].first;
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
      TEST_CHECK(mutt_str_strcmp(parent->orig, tests[i].second) == 0);
      mutt_path_free(&parent);
    }
  }
}

void test_maildir_path2_pretty(void)
{
  static const char *folder = "/home/mutt/path";
  // clang-format off
  static struct TestValue tests[] = {
    { "/home/mutt/path/maildir/apple.maildir",         "+maildir/apple.maildir",         1 },
    { "/home/mutt/path/maildir/symlink/apple.maildir", "+maildir/symlink/apple.maildir", 1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  char *pretty = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = maildir_path2_pretty(&path, folder, &pretty);
    TEST_CHECK(rc == tests[i].retval);
    if (rc > 0)
    {
      TEST_CHECK(pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(pretty, tests[i].second) == 0);
    }
    FREE(&pretty);
  }

  path.orig = tests[0].first;
  HomeDir = "/home/mutt";
  rc = maildir_path2_pretty(&path, "nowhere", &pretty);
  TEST_CHECK(rc == 1);
  TEST_CHECK(pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(pretty, "~/path/maildir/apple.maildir") == 0);
  FREE(&pretty);

  path.orig = tests[0].first;
  HomeDir = "/home/another";
  rc = maildir_path2_pretty(&path, "nowhere", &pretty);
  TEST_CHECK(rc == 0);
  TEST_CHECK(pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(pretty, tests[0].first) == 0);
  FREE(&pretty);
}

void test_maildir_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "/home/mutt/path/maildir/apple",          NULL,  0 }, // Normal, all 3 subdirs
    { "/home/mutt/path/maildir/banana",         NULL,  0 }, // Normal, just 'cur' subdir
    { "/home/mutt/path/maildir/symlink/banana", NULL,  0 }, // Symlink
    { "/home/mutt/path/maildir/cherry",         NULL, -1 }, // No subdirs
    { "/home/mutt/path/maildir/damson",         NULL, -1 }, // Unreadable
    { "/home/mutt/path/maildir/endive",         NULL, -1 }, // File
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
    { "/home/mutt/path/maildir/mh1",         NULL,  0 }, // Contains .mh_sequences
    { "/home/mutt/path/maildir/mh2",         NULL,  0 }, // Contains .xmhcache
    { "/home/mutt/path/maildir/symlink/mh2", NULL,  0 }, // Symlink
    { "/home/mutt/path/maildir/mh3",         NULL,  0 }, // Contains .mew_cache
    { "/home/mutt/path/maildir/mh4",         NULL,  0 }, // Contains .mew-cache
    { "/home/mutt/path/maildir/mh5",         NULL,  0 }, // Contains .sylpheed_cache
    { "/home/mutt/path/maildir/mh6",         NULL,  0 }, // Contains .overview
    { "/home/mutt/path/maildir/mh7",         NULL, -1 }, // Empty
    { "/home/mutt/path/maildir/mh8",         NULL, -1 }, // File
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
    { "/home/mutt/path/./maildir/../maildir///apple", "/home/mutt/path/maildir/apple", 0 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_MAILDIR,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = mutt_str_strdup(tests[i].first);
    rc = maildir_path2_tidy(&path);
    TEST_CHECK(rc == 0);
    TEST_CHECK(path.orig != NULL);
    TEST_CHECK(path.flags & MPATH_TIDY);
    TEST_CHECK(strcmp(path.orig, tests[i].second) == 0);
    FREE(&path.orig);
  }
}
