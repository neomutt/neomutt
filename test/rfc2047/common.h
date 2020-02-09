/**
 * @file
 * Common code for RFC2047 tests
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

#ifndef TEST_RFC2047_COMMON_H
#define TEST_RFC2047_COMMON_H

#define TEST_NO_MAIN
#include "acutest.h"
#include <locale.h>
#include "mutt/lib.h"
#include "email/lib.h"

struct Rfc2047TestData
{
  const char *original; // the string as received in the original email
  const char *decoded;  // the expected plain-text string
  const char *encoded;  // the string as it's encoded by NeoMutt
};

extern const struct Rfc2047TestData rfc2047_test_data[];

#endif /* TEST_RFC2047_COMMON_H */
