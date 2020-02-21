/**
 * @file
 * Test code for the Notmuch MxOps Path functions
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
#include "notmuch/path.h"

void test_nm_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",             "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",              0 }, // Same
    { "notmuch:///home/mutt/path/notmuch/symlink?one=apple&two=banana",           "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",              0 }, // Symlink
    { "notmuch:///home/mutt/path/notmuch/cherry?one=apple&two=banana",            "notmuch:///home/mutt/path/notmuch/cherry?one=apple&two=banana",            -1 }, // Missing
    { "notmuch:///home/mutt/path/notmuch/apple?two=banana&one=apple",             "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",              0 }, // Query (sort)
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana&one=cherry",  "notmuch:///home/mutt/path/notmuch/apple?one=apple&one=cherry&two=banana",   0 }, // Query (dupe)
    { "notmuch:///home/mutt/path/notmuch/apple?one=cherry&two=banana&one=cherry", "notmuch:///home/mutt/path/notmuch/apple?one=cherry&one=cherry&two=banana",  0 }, // Query (dupe)
    { "pop://example.com/",                                                       "pop://example.com/",                                                       -1 },
    { "junk://example.com/",                                                      "junk://example.com/",                                                      -1 },
  };
  // clang-format on

  struct Path path = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);
    path.type = MUTT_NOTMUCH;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = nm_path2_canon(&path);
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

void test_nm_path2_compare(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",           "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",                0 }, // Match
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",           "pop://example.com/",                                                          1 }, // Scheme differs
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",           "notmuch:///home/mutt/path/notmuch/banana?one=apple&two=banana",              -1 }, // Path differs
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",           "notmuch://?one=apple&two=banana",                                             0 }, // Path missing
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",           "notmuch:///home/mutt/path/notmuch/apple?one=apple",                           1 }, // Query differs (fewer)
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",           "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana&three=cherry",  -1 }, // Query differs (more)
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&one=apple&two=banana", "notmuch:///home/mutt/path/notmuch/apple?one=apple&one=apple&two=banana",      0 }, // Query (dupes)
  };
  // clang-format on

  struct Path path1 = {
    .type = MUTT_NOTMUCH,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };
  struct Path path2 = {
    .type = MUTT_NOTMUCH,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path1.canon = (char *) tests[i].first;
    TEST_CASE(path1.canon);

    path2.canon = (char *) tests[i].second;
    TEST_CASE(path2.canon);

    rc = nm_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_nm_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch:///home/mutt/path/notmuch/apple", NULL, -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_NOTMUCH,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = tests[i].first;
    TEST_CASE(path.orig);

    rc = nm_path2_parent(&path, &parent);
    TEST_CHECK(rc == tests[i].retval);
    TEST_CHECK(mutt_str_strcmp(parent ? parent->orig : NULL, tests[i].second) == 0);
  }
}

void test_nm_path2_pretty(void)
{
  const char *folder = "notmuch:///home/mutt/path/notmuch/apple";
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",  "notmuch://?one=apple&two=banana", 1 },
    { "notmuch:///home/mutt/path/notmuch/cherry?one=apple&two=banana", NULL,                              0 },
    { "pop://example.com/",                                            NULL,                              0 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_NOTMUCH,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  char *pretty = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = nm_path2_pretty(&path, folder, &pretty);
    TEST_CHECK(rc == tests[i].retval);
    if (rc > 0)
    {
      TEST_CHECK(pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(pretty, tests[i].second) == 0);
    }
    FREE(&pretty);
  }
}

void test_nm_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "notmuch:///home/mutt/path/notmuch/apple",   NULL,  0 }, // OK
    { "notmuch:///home/mutt/path/notmuch/symlink", NULL,  0 }, // Symlink
    { "notmuch:///home/mutt/path/notmuch/banana",  NULL, -1 }, // Missing .notmuch dir
    { "notmuch:///home/mutt/path/notmuch/cherry",  NULL, -1 }, // Missing dir
    { "pop://example.com/",                        NULL, -1 },
    { "junk://example.com/",                       NULL, -1 },
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
    rc = nm_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_nm_path2_tidy(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",          "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",  0 },
    { "NOTMUCH:///home/mutt/path/notmuch/apple?one=apple&two=banana",          "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",  0 },
    { "notmuch:///home/mutt/path/../path/notmuch//apple?one=apple&two=banana", "notmuch:///home/mutt/path/notmuch/apple?one=apple&two=banana",  0 },
    { "pop://example.com/",                                                    "pop://example.com/",                                           -1 },
    { "junk://example.com/",                                                   "junk://example.com/",                                          -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_NOTMUCH,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = mutt_str_strdup(tests[i].first);
    TEST_CASE(path.orig);

    rc = nm_path2_tidy(&path);
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
