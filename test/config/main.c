/**
 * @file
 * Tests for the config code
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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

#include "acutest.h"
#include "config.h"
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "mutt/logging.h"
#include "test/config/account2.h"
#include "test/config/address.h"
#include "test/config/bool.h"
#include "test/config/command.h"
#include "test/config/initial.h"
#include "test/config/long.h"
#include "test/config/magic.h"
#include "test/config/mbtable.h"
#include "test/config/number.h"
#include "test/config/path.h"
#include "test/config/quad.h"
#include "test/config/regex3.h"
#include "test/config/set.h"
#include "test/config/sort.h"
#include "test/config/string4.h"
#include "test/config/synonym.h"

/******************************************************************************
 * Add your test cases to this list.
 *****************************************************************************/
#define NEOMUTT_TEST_LIST                                                      \
  NEOMUTT_TEST_ITEM(config_set)                                                \
  NEOMUTT_TEST_ITEM(config_account)                                            \
  NEOMUTT_TEST_ITEM(config_initial)                                            \
  NEOMUTT_TEST_ITEM(config_synonym)                                            \
  NEOMUTT_TEST_ITEM(config_address)                                            \
  NEOMUTT_TEST_ITEM(config_bool)                                               \
  NEOMUTT_TEST_ITEM(config_command)                                            \
  NEOMUTT_TEST_ITEM(config_long)                                               \
  NEOMUTT_TEST_ITEM(config_magic)                                              \
  NEOMUTT_TEST_ITEM(config_mbtable)                                            \
  NEOMUTT_TEST_ITEM(config_number)                                             \
  NEOMUTT_TEST_ITEM(config_path)                                               \
  NEOMUTT_TEST_ITEM(config_quad)                                               \
  NEOMUTT_TEST_ITEM(config_regex)                                              \
  NEOMUTT_TEST_ITEM(config_sort)                                               \
  NEOMUTT_TEST_ITEM(config_string)

/******************************************************************************
 * You probably don't need to touch what follows.
 *****************************************************************************/
#define NEOMUTT_TEST_ITEM(x) void x(void);
NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM

TEST_LIST = {
#define NEOMUTT_TEST_ITEM(x) { #x, x },
  NEOMUTT_TEST_LIST
#undef NEOMUTT_TEST_ITEM
  { 0 }
};
