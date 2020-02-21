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
#include "config.h"
#include "acutest.h"
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "common.h"
#include "notmuch/path.h"
#include "test_common.h"

void test_nm_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",             "notmuch://%s/notmuch/apple?one=apple&two=banana",              0 }, // Same
    { "notmuch://%s/notmuch/symlink?one=apple&two=banana",           "notmuch://%s/notmuch/apple?one=apple&two=banana",              0 }, // Symlink
    { "notmuch://%s/notmuch/cherry?one=apple&two=banana",            "notmuch://%s/notmuch/cherry?one=apple&two=banana",            -1 }, // Missing
    { "notmuch://%s/notmuch/apple?two=banana&one=apple",             "notmuch://%s/notmuch/apple?one=apple&two=banana",              0 }, // Query (sort)
    { "notmuch://%s/notmuch/apple?one=apple&two=banana&one=cherry",  "notmuch://%s/notmuch/apple?one=apple&one=cherry&two=banana",   0 }, // Query (dupe)
    { "notmuch://%s/notmuch/apple?one=cherry&two=banana&one=cherry", "notmuch://%s/notmuch/apple?one=cherry&one=cherry&two=banana",  0 }, // Query (dupe)
    { "pop://example.com/",                                          "pop://example.com/",                                          -1 },
    { "junk://example.com/",                                         "junk://example.com/",                                         -1 },
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
    path.type = MUTT_NOTMUCH;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = nm_path2_canon(&path);
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

void test_nm_path2_compare(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",           "notmuch://%s/notmuch/apple?one=apple&two=banana",                0 }, // Match
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",           "pop://example.com/",                                             1 }, // Scheme differs
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",           "notmuch://%s/notmuch/banana?one=apple&two=banana",              -1 }, // Path differs
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",           "notmuch://?one=apple&two=banana",                                0 }, // Path missing
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",           "notmuch://%s/notmuch/apple?one=apple",                           1 }, // Query differs (fewer)
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",           "notmuch://%s/notmuch/apple?one=apple&two=banana&three=cherry",  -1 }, // Query differs (more)
    { "notmuch://%s/notmuch/apple?one=apple&one=apple&two=banana", "notmuch://%s/notmuch/apple?one=apple&one=apple&two=banana",      0 }, // Query (dupes)
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

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
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path1.canon = first;
    TEST_CASE(path1.canon);

    path2.canon = second;
    TEST_CASE(path2.canon);

    rc = nm_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_nm_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch://%s/notmuch/apple", NULL, -1 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = {
    .type = MUTT_NOTMUCH,
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

    rc = nm_path2_parent(&path, &parent);
    TEST_CHECK(rc == tests[i].retval);
    TEST_CHECK(mutt_str_strcmp(parent ? parent->orig : NULL, second) == 0);
  }
}

void test_nm_path2_pretty(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",  "notmuch://?one=apple&two=banana", 1 },
    { "notmuch://%s/notmuch/cherry?one=apple&two=banana", NULL,                              0 },
    { "pop://example.com/",                               NULL,                              0 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };
  char folder[256] = { 0 };

  test_gen_path(folder, sizeof(folder), "notmuch://%s/notmuch/apple");

  struct Path path = {
    .type = MUTT_NOTMUCH,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = first;
    TEST_CASE(path.orig);

    rc = nm_path2_pretty(&path, folder);
    TEST_CHECK(rc == tests[i].retval);
    if (rc > 0)
    {
      TEST_CHECK(path.pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(path.pretty, second) == 0);
    }
  }
}

void test_nm_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "notmuch://%s/notmuch/apple",   NULL,  0 }, // OK
    { "notmuch://%s/notmuch/symlink", NULL,  0 }, // Symlink
    { "notmuch://%s/notmuch/banana",  NULL, -1 }, // Missing .notmuch dir
    { "notmuch://%s/notmuch/cherry",  NULL, -1 }, // Missing dir
    { "pop://example.com/",           NULL, -1 },
    { "junk://example.com/",          NULL, -1 },
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
    { "notmuch://%s/notmuch/apple?one=apple&two=banana",            "notmuch://%s/notmuch/apple?one=apple&two=banana",  0 },
    { "NOTMUCH://%s/notmuch/apple?one=apple&two=banana",            "notmuch://%s/notmuch/apple?one=apple&two=banana",  0 },
    { "notmuch://%s/notmuch/../notmuch/apple?one=apple&two=banana", "notmuch://%s/notmuch/apple?one=apple&two=banana",  0 },
    { "pop://example.com/",                                         "pop://example.com/",                              -1 },
    { "junk://example.com/",                                        "junk://example.com/",                             -1 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = {
    .type = MUTT_NOTMUCH,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = mutt_str_strdup(first);
    TEST_CASE(path.orig);

    rc = nm_path2_tidy(&path);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.orig != NULL);
      TEST_CHECK(path.flags & MPATH_TIDY);
      TEST_CHECK(mutt_str_strcmp(path.orig, second) == 0);
    }
    FREE(&path.orig);
  }
}
