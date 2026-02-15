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
  { "assumed_charset",         DT_SLIST|D_SLIST_SEP_COLON|D_SLIST_ALLOW_EMPTY, 0,                         0, NULL },
  { "charset",                 DT_STRING|D_NOT_EMPTY|D_CHARSET_SINGLE,         IP "utf-8",                0, NULL },
  { "color_directcolor",       DT_BOOL,                                        true,                      0, NULL },
  { "config_charset",          DT_STRING,                                      0,                         0, NULL },
  { "debug_level",             DT_NUMBER,                                      0,                         0, NULL },
  { "folder",                  DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/Mail",      0, NULL },
  { "history",                 DT_NUMBER | D_INTEGER_NOT_NEGATIVE,             10,                        0, NULL },
  { "history_file",            DT_PATH | D_PATH_FILE,                          IP "~/.mutthistory",       0, NULL },
  { "history_remove_dups",     DT_BOOL,                                        false,                     0, NULL },
  { "maildir_field_delimiter", DT_STRING,                                      IP ":",                    0, NULL },
  { "mbox",                    DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/mbox",      0, NULL },
  { "postponed",               DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/postponed", 0, NULL },
  { "record",                  DT_STRING|D_STRING_MAILBOX,                     IP "/home/mutt/sent",      0, NULL },
  { "save_history",            DT_NUMBER | D_INTEGER_NOT_NEGATIVE,             0,                         0, NULL },
  { "simple_search",           DT_STRING,                                      IP "~f %s | ~s %s",        0, NULL },
  { "sleep_time",              DT_NUMBER|D_INTEGER_NOT_NEGATIVE,               0,                         0, NULL },
  { "tmp_dir",                 DT_PATH|D_PATH_DIR|D_NOT_EMPTY,                 IP TMPDIR,                 0, NULL },
  { NULL },
  // clang-format on
};

#define CONFIG_INIT_TYPE(CS, NAME)                                             \
  extern const struct ConfigSetType Cst##NAME;                                 \
  cs_register_type(CS, &Cst##NAME)

/**
 * test_config_define_types - Set up Config Types - Implements Module::config_define_types()
 */
static bool test_config_define_types(struct NeoMutt *n, struct ConfigSet *cs)
{
  CONFIG_INIT_TYPE(cs, Address);
  CONFIG_INIT_TYPE(cs, Bool);
  CONFIG_INIT_TYPE(cs, Enum);
  CONFIG_INIT_TYPE(cs, Expando);
  CONFIG_INIT_TYPE(cs, Long);
  CONFIG_INIT_TYPE(cs, Mbtable);
  CONFIG_INIT_TYPE(cs, MyVar);
  CONFIG_INIT_TYPE(cs, Number);
  CONFIG_INIT_TYPE(cs, Path);
  CONFIG_INIT_TYPE(cs, Quad);
  CONFIG_INIT_TYPE(cs, Regex);
  CONFIG_INIT_TYPE(cs, Slist);
  CONFIG_INIT_TYPE(cs, Sort);
  CONFIG_INIT_TYPE(cs, String);
  return true;
}

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
  test_config_define_types,
  test_config_define_variables,
  NULL, // commands_register
  NULL, // gui_init
  NULL, // gui_cleanup
  NULL, // cleanup
};
