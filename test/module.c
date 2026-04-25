/**
 * @file
 * Test Module
 *
 * @authors
 * Copyright (C) 2025-2026 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stddef.h>
#include "config/lib.h"
#include "core/lib.h"

static struct ConfigDef TestVars[] = {
  // clang-format off
  { "abort_key",               DT_STRING|D_NOT_EMPTY|D_ON_STARTUP,             IP "\007",                 0, NULL },
  { "assumed_charset",         DT_SLIST|D_SLIST_SEP_COLON|D_SLIST_ALLOW_EMPTY, 0,                         0, NULL },
  { "charset",                 DT_STRING|D_NOT_EMPTY|D_CHARSET_SINGLE,         IP "utf-8",                0, NULL },
  { "config_charset",          DT_STRING,                                      0,                         0, NULL },
  { "debug_level",             DT_NUMBER,                                      0,                         0, NULL },
  { "folder",                  DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/Mail",      0, NULL },
  { "macro_repeat_max",        DT_NUMBER|D_INTEGER_NOT_NEGATIVE,               1000,                      0, NULL },
  { "mbox",                    DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/mbox",      0, NULL },
  { "postponed",               DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/postponed", 0, NULL },
  { "record",                  DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/sent",      0, NULL },
  { "sleep_time",              DT_NUMBER|D_INTEGER_NOT_NEGATIVE,               0,                         0, NULL },
  { "tmp_dir",                 DT_PATH|D_PATH_DIR|D_NOT_EMPTY,                 IP TMPDIR,                 0, NULL },
  { "tmp_draft_dir",           DT_PATH|D_PATH_DIR|D_NOT_EMPTY,                 IP TMPDIR,                 0, NULL },
  { NULL },
  // clang-format on
};

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

/**
 * test_config_define_variables - Define the Config Variables - Implements Module::config_define_variables()
 */
static bool test_config_define_variables(struct NeoMutt *n, struct ConfigSet *cs)
{
  return cs_register_variables(cs, TestVars);
}

/**
 * ModuleTest - Module for the Test library
 */
const struct Module ModuleTest = {
  MODULE_ID_MAIN,
  "test",
  NULL, // init
  NULL, // config_define_types,
  test_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
};
