/**
 * @file
 * Test code for the Compressed MxOps Path functions
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
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "common.h"
#include "compress/path.h"

bool mutt_comp_can_read(const char *path)
{
  static const char *suffix = ".gz";
  size_t len = strlen(path);

  return (strcmp(path + len - 3, suffix) == 0);
}

void test_comp_path2_canon(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "/home/mutt/path/compress/apple.gz",         "/home/mutt/path/compress/apple.gz",  0 }, // Real path
    { "/home/mutt/path/compress/symlink/apple.gz", "/home/mutt/path/compress/apple.gz",  0 }, // Symlink
    { "/home/mutt/path/compress/missing",          NULL,                                -1 }, // Missing
  };
  // clang-format on

  struct Path path = { 0 };
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = tests[i].first;
    TEST_CASE(path.orig);
    path.type = MUTT_COMPRESSED;
    path.flags = MPATH_RESOLVED | MPATH_TIDY;

    rc = comp_path2_canon(&path);
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

void test_comp_path2_compare(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "/home/mutt/path/compress/apple.gz",  "/home/mutt/path/compress/apple.gz",   0 }, // Match
    { "/home/mutt/path/compress/apple.gz",  "/home/mutt/path/compress/orange.gz", -1 }, // Differ
    { "/home/mutt/path/compress/orange.gz", "/home/mutt/path/compress/apple.gz",   1 }, // Differ
  };
  // clang-format on

  struct Path path1 = {
    .type = MUTT_COMPRESSED,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };
  struct Path path2 = {
    .type = MUTT_COMPRESSED,
    .flags = MPATH_RESOLVED | MPATH_TIDY | MPATH_CANONICAL,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path1.canon = (char *) tests[i].first;
    TEST_CASE(path1.canon);

    path2.canon = (char *) tests[i].second;
    TEST_CASE(path2.canon);

    rc = comp_path2_compare(&path1, &path2);
    TEST_CHECK(rc == tests[i].retval);
  }
}

void test_comp_path2_parent(void)
{
  // clang-format off
  static struct TestValue tests[] = {
    { "/home/mutt/path/compress/apple.gz", NULL, -1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_COMPRESSED,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  struct Path *parent = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = tests[i].first;
    TEST_CASE(path.orig);

    rc = comp_path2_parent(&path, &parent);
    TEST_CHECK(rc == tests[i].retval);
    TEST_CHECK(mutt_str_strcmp(parent ? parent->orig : NULL, tests[i].second) == 0);
  }
}

void test_comp_path2_pretty(void)
{
  static const char *folder = "/home/mutt/path";
  // clang-format off
  static struct TestValue tests[] = {
    { "/home/mutt/path/compress/apple.gz",         "+compress/apple.gz",         1 },
    { "/home/mutt/path/compress/symlink/apple.gz", "+compress/symlink/apple.gz", 1 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_COMPRESSED,
    .flags = MPATH_RESOLVED | MPATH_TIDY,
  };

  char *pretty = NULL;
  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = (char *) tests[i].first;
    path.canon = (char *) tests[i].first;
    TEST_CASE(path.orig);

    rc = comp_path2_pretty(&path, folder, &pretty);
    TEST_CHECK(rc == tests[i].retval);
    if (rc >= 0)
    {
      TEST_CHECK(pretty != NULL);
      TEST_CHECK(mutt_str_strcmp(pretty, tests[i].second) == 0);
    }
    FREE(&pretty);
  }

  path.orig = tests[0].first;
  HomeDir = "/home/mutt";
  rc = comp_path2_pretty(&path, "nowhere", &pretty);
  TEST_CHECK(rc == 1);
  TEST_CHECK(pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(pretty, "~/path/compress/apple.gz") == 0);
  FREE(&pretty);

  path.orig = tests[0].first;
  HomeDir = "/home/another";
  rc = comp_path2_pretty(&path, "nowhere", &pretty);
  TEST_CHECK(rc == 0);
  TEST_CHECK(pretty != NULL);
  TEST_CHECK(mutt_str_strcmp(pretty, tests[0].first) == 0);
  FREE(&pretty);
}

void test_comp_path2_probe(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "/home/mutt/path/compress/apple.gz",  NULL,  0 }, // Accepted
    { "/home/mutt/path/compress/banana.gz", NULL, -1 }, // Directory
    { "/home/mutt/path/compress/cherry.xz", NULL, -1 }, // Not accepted
    { "/home/mutt/path/compress/damson.gz", NULL, -1 }, // Missing
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
    path.flags = MPATH_RESOLVED | MPATH_TIDY;
    memset(&st, 0, sizeof(st));
    stat(path.orig, &st);
    rc = comp_path2_probe(&path, &st);
    TEST_CHECK(rc == tests[i].retval);
    if (rc == 0)
    {
      TEST_CHECK(path.type > MUTT_UNKNOWN);
    }
  }
}

void test_comp_path2_tidy(void)
{
  // clang-format off
  static const struct TestValue tests[] = {
    { "/home/mutt/path/./compress/../compress///apple.gz", "/home/mutt/path/compress/apple.gz", 0 },
  };
  // clang-format on

  struct Path path = {
    .type = MUTT_COMPRESSED,
    .flags = MPATH_RESOLVED,
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(tests); i++)
  {
    path.orig = mutt_str_strdup(tests[i].first);
    rc = comp_path2_tidy(&path);
    TEST_CHECK(rc == 0);
    TEST_CHECK(path.orig != NULL);
    TEST_CHECK(path.flags & MPATH_TIDY);
    TEST_CHECK(strcmp(path.orig, tests[i].second) == 0);
    FREE(&path.orig);
  }
}
