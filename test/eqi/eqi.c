/**
 * @file
 * Test code for eqi string comparison functions
 *
 * @authors
 * Copyright (C) 2023 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include "mutt/lib.h"
#include "test_common.h"

void test_eqi(void)
{
  TEST_CHECK(eqi1("a", "a"));
  TEST_CHECK(eqi1("A", "a"));
  TEST_CHECK(!eqi1("b", "a"));

  TEST_CHECK(eqi2("ab", "ab"));
  TEST_CHECK(eqi2("Ab", "ab"));
  TEST_CHECK(!eqi2("dj", "ab"));

  // eqi3() doesn't exist

  TEST_CHECK(eqi4("abcd", "abcd"));
  TEST_CHECK(eqi4("AbCd", "abcd"));
  TEST_CHECK(!eqi4("djei", "abcd"));

  TEST_CHECK(eqi5("abcde", "abcde"));
  TEST_CHECK(eqi5("AbCde", "abcde"));
  TEST_CHECK(!eqi5("djeig", "abcde"));

  TEST_CHECK(eqi6("abcdef", "abcdef"));
  TEST_CHECK(eqi6("AbCdeF", "abcdef"));
  TEST_CHECK(!eqi6("djeigw", "abcdef"));

  // eqi7() doesn't exist

  TEST_CHECK(eqi8("abcdefgh", "abcdefgh"));
  TEST_CHECK(eqi8("AbCdeFGH", "abcdefgh"));
  TEST_CHECK(!eqi8("djeigwdj", "abcdefgh"));

  TEST_CHECK(eqi9("abcdefghi", "abcdefghi"));
  TEST_CHECK(eqi9("AbCdeFGHi", "abcdefghi"));
  TEST_CHECK(!eqi9("djeigwdjs", "abcdefghi"));

  TEST_CHECK(eqi10("abcdefghij", "abcdefghij"));
  TEST_CHECK(eqi10("AbCdeFGHij", "abcdefghij"));
  TEST_CHECK(!eqi10("djeigwdjsd", "abcdefghij"));

  TEST_CHECK(eqi11("abcdefghijk", "abcdefghijk"));
  TEST_CHECK(eqi11("AbCdeFGHijK", "abcdefghijk"));
  TEST_CHECK(!eqi11("djeigwdjsdj", "abcdefghijk"));

  TEST_CHECK(eqi12("abcdefghijkl", "abcdefghijkl"));
  TEST_CHECK(eqi12("AbCdeFGHijKL", "abcdefghijkl"));
  TEST_CHECK(!eqi12("djeigwdjsdjf", "abcdefghijkl"));

  TEST_CHECK(eqi13("abcdefghijklm", "abcdefghijklm"));
  TEST_CHECK(eqi13("AbCdeFGHijKLm", "abcdefghijklm"));
  TEST_CHECK(!eqi13("djeigwdjsdjfs", "abcdefghijklm"));

  TEST_CHECK(eqi14("abcdefghijklmn", "abcdefghijklmn"));
  TEST_CHECK(eqi14("AbCdeFGHijKLmN", "abcdefghijklmn"));
  TEST_CHECK(!eqi14("djeigwdjsdjfsd", "abcdefghijklmn"));

  TEST_CHECK(eqi15("abcdefghijklmno", "abcdefghijklmno"));
  TEST_CHECK(eqi15("AbCdeFGHijKLmNo", "abcdefghijklmno"));
  TEST_CHECK(!eqi15("djeigwdjsdjfsdf", "abcdefghijklmno"));

  TEST_CHECK(eqi16("abcdefghijklmnop", "abcdefghijklmnop"));
  TEST_CHECK(eqi16("AbCdeFGHijKLmNoP", "abcdefghijklmnop"));
  TEST_CHECK(!eqi16("djeigwdjsdjfsdfj", "abcdefghijklmnop"));

  TEST_CHECK(eqi17("abcdefghijklmnopq", "abcdefghijklmnopq"));
  TEST_CHECK(eqi17("AbCdeFGHijKLmNoPq", "abcdefghijklmnopq"));
  TEST_CHECK(!eqi17("djeigwdjsdjfsdfjj", "abcdefghijklmnopq"));
}
