/**
 * @file
 * Common code for MD5 tests
 *
 * @authors
 * Copyright (C) 2018 Pietro Cerutti <gahr@gahr.ch>
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

#ifndef TEST_MD5_COMMON_H
#define TEST_MD5_COMMON_H

#define TEST_NO_MAIN
#include "acutest.h"
#include "mutt/lib.h"

struct Md5TestData
{
  const char *text; // clear text input string
  const char *hash; // MD5 hash digest
};

extern const struct Md5TestData md5_test_data[];

#endif /* TEST_MD5_COMMON_H */
