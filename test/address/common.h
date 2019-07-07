/**
 * @file
 * Test code for the Address object
 *
 * @authors
 * Copyright (C) 2018 Simon Symeonidis <lethaljellybean@gmail.com>
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

#ifndef TEST_ADDRESS_COMMON_H
#define TEST_ADDRESS_COMMON_H

#define TEST_NO_MAIN
#include "acutest.h"
#include <string.h>
#include "mutt/memory.h"
#include "address/lib.h"

#define TEST_CHECK_STR_EQ(expected, actual)                                    \
  do                                                                           \
  {                                                                            \
    if (!TEST_CHECK(strcmp(expected, actual) == 0))                            \
    {                                                                          \
      TEST_MSG("Expected: %s", expected);                                      \
      TEST_MSG("Actual  : %s", actual);                                        \
    }                                                                          \
  } while (false)

#endif /* TEST_ADDRESS_COMMON_H */
