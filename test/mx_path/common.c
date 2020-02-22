/**
 * @file
 * Shared code for the MxOps Path functions
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

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"

const char *HomeDir;

static const char *get_test_path(void)
{
  static const char *path = NULL;
  if (!path)
    path = mutt_str_getenv("NEOMUTT_TEST_DIR");
  if (!path)
    path = "";

  return path;
}

void test_gen_path(char *buf, size_t buflen, const char *fmt)
{
  snprintf(buf, buflen, NONULL(fmt), get_test_path());
}

void test_gen_dir(char *buf, size_t buflen, const char *fmt)
{
  static const char *dir = NULL;
  if (!dir)
  {
    const char *path = get_test_path();
    const char *slash = strrchr(path, '/');
    dir = slash + 1;
  }
  if (!dir)
    dir = "";

  snprintf(buf, buflen, NONULL(fmt), dir);
}
