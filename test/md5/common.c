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

#define TEST_NO_MAIN
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"
#include "common.h"

// clang-format off
const struct Md5TestData md5_test_data[] =
{
  {
    "The quick brown fox jumps over the lazy dog",
    "9e107d9d372bb6826bd81d3542a419d6"
  },
  {
    "", // The empty string
    "d41d8cd98f00b204e9800998ecf8427e"
  },
  { NULL, NULL },
};
// clang-format on
