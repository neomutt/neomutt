/**
 * @file
 * Test code for mutt_ch_convert_string()
 *
 * @authors
 * Copyright (C) 2022 Michal Siedlaczek <michal@siedlaczek.me>
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

#ifndef TEST_CONVERT_CONVERT_COMMON_H
#define TEST_CONVERT_CONVERT_COMMON_H

#include <stdbool.h>
#include "email/lib.h"

struct Content static const initial_info = {
  .hibin = 0,
  .lobin = 0,
  .nulbin = 0,
  .crlf = 0,
  .ascii = 0,
  .linemax = 0,
  .space = false,
  .binary = false,
  .from = false,
  .dot = false,
  .cr = false,
};

#endif /* TEST_CONVERT_CONVERT_COMMON_H */
