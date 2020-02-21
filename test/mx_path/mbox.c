/**
 * @file
 * Test code for the Mbox MxOps Path functions
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
#include "mbox/path.h"
#include "test_common.h"

void test_mbox_path2_canon(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "%s/mbox/apple.mbox",         "%s/mbox/apple.mbox",  0 }, // Real path
    { "%s/mbox/symlink/apple.mbox", "%s/mbox/apple.mbox",  0 }, // Symlink
    { "%s/mbox/missing",            NULL,                 -1 }, // Missing
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
    path.type = MUTT_MBOX;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = mbox_path2_canon(&path);
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

void test_mbox_path2_compare(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "%s/mbox/apple.mbox",  "%s/mbox/apple.mbox",   0 }, // Match
    { "%s/mbox/apple.mbox",  "%s/mbox/orange.mbox", -1 }, // Differ
    { "%s/mbox/orange.mbox", "%s/mbox/apple.mbox",   1 }, // Differ
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path1 = {
    .type = MUTT_MBOX,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };
  struct Path path2 = {
    .type = MUTT_MBOX,
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

    rc = mbox_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_mbox_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "%s/mbox/apple.mbox", NULL, -1 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = {
    .type = MUTT_MBOX,
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

    rc = mbox_path2_parent(&path, &parent);
    TEST_CHECK(rc == tests[i].retval);
    TEST_CHECK(mutt_str_strcmp(parent ? parent->orig : NULL, second) == 0);
  }
}

void test_mbox_path2_pretty(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "%s/mbox/apple.mbox",         "+mbox/apple.mbox",         1 },
    { "%s/mbox/symlink/apple.mbox", "+mbox/symlink/apple.mbox", 1 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };
  char folder[256] = { 0 };

  test_gen_path(folder, sizeof(folder), "%s");

  struct Path path = {
    .type = MUTT_MBOX,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = first;
    TEST_CASE(path.orig);

    rc = mbox_path2_pretty(&path, folder);
    TEST_CHECK(rc == tests[i].retval);
    if (rc >= 0)
    {
      TEST_CHECK(path.pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(path.pretty, second) == 0);
    }
  }

  test_gen_path(first, sizeof(first),  "%s/mbox/apple.mbox");
  test_gen_path(second, sizeof(second), "~/mbox/apple.mbox");
  path.orig = first;
  HomeDir = test_get_test_dir();
  rc = mbox_path2_pretty(&path, "nowhere");
  TEST_CHECK(rc == 1);
  TEST_CHECK(path.pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(path.pretty, second) == 0);

  test_gen_path(first, sizeof(first), tests[0].first);
  test_gen_path(second, sizeof(second), tests[0].first);
  path.orig = first;
  HomeDir = "/home/another";
  rc = mbox_path2_pretty(&path, "nowhere");
  TEST_CHECK(rc == 0);
  TEST_CHECK(path.pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(path.pretty, second) == 0);
}

void test_mbox_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "%s/mbox/apple.mbox",          NULL,  0 }, // Empty
    { "%s/mbox/banana.mbox",         NULL,  0 }, // Normal
    { "%s/mbox/symlink/banana.mbox", NULL,  0 }, // Symlink
    { "%s/mbox/cherry.mbox",         NULL, -1 }, // Junk
    { "%s/mbox/damson.mbox",         NULL, -1 }, // Directory
    { "%s/mbox/endive.mbox",         NULL, -1 }, // Unreadable
    { "%s/mbox/fig.mbox",            NULL,  0 }, // Mmdf
    { "%s/mbox/guava.mbox",          NULL,  0 }, // Missing
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
    rc = mbox_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_mbox_path2_tidy(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "%s/./mbox/../mbox///apple.mbox", "%s/mbox/apple.mbox", 0 },
  };
  // clang-format on

  char first[256] = { 0 };
  char second[256] = { 0 };

  struct Path path = {
    .type = MUTT_MBOX,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    test_gen_path(first, sizeof(first), tests[i].first);
    test_gen_path(second, sizeof(second), tests[i].second);

    path.orig = mutt_str_strdup(first);
    rc = mbox_path2_tidy(&path);
    TEST_CHECK(rc == 0);
    TEST_CHECK(path.orig != NULL);
    TEST_CHECK(path.flags & MPATH_TIDY);
    TEST_CHECK(strcmp(path.orig, second) == 0);
    FREE(&path.orig);
  }
}
