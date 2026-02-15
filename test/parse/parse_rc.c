/**
 * @file
 * Test code for parsing config files
 *
 * @authors
 * Copyright (C) 2023-2026 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Dennis Schön <mail@dennis-schoen.de>
 * Copyright (C) 2023 наб <nabijaczleweli@nabijaczleweli.xyz>
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
#include <stdio.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "test_common.h" // IWYU pragma: keep

extern const struct Mapping MboxTypeMap[];
extern struct EnumDef MboxTypeDef;
extern const struct Mapping SortMethods[];

static const struct Command mutt_commands[] = {
  // clang-format off
  { "reset",  CMD_RESET,  parse_set},
  { "set",    CMD_SET,    parse_set},
  { "toggle", CMD_TOGGLE, parse_set},
  { "unset",  CMD_UNSET,  parse_set},
  { NULL, CMD_NONE, NULL},
  // clang-format on
};

static struct ConfigDef Vars[] = {
  // clang-format off
  { "from",              DT_ADDRESS,                       0,                     0,               NULL, },
  { "beep",              DT_BOOL,                          true,                  0,               NULL, },
  { "ispell",            DT_STRING|D_STRING_COMMAND,       IP "ispell",           0,               NULL, },
  { "mbox_type",         DT_ENUM,                          MUTT_MBOX,             IP &MboxTypeDef, NULL, },
  { "net_inc",           DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 10,                    0,               NULL, },
  { "print",             DT_QUAD,                          MUTT_ASKNO,            0,               NULL, },
  { "mask",              DT_REGEX|D_REGEX_NOSUB,           IP "!^\\.[^.]",        0,               NULL, },
  { "sort",              DT_SORT|D_SORT_LAST,              EMAIL_SORT_DATE,       IP SortMethods,  NULL, },
  { NULL },
  // clang-format on
};

static void test_parse_set(void)
{
  const char *vars[] = {
    "from",              // ADDRESS
    "beep",              // BOOL
    "ispell",            // COMMAND
    "mbox_type",         // ENUM
    "to_chars",          // MBTABLE
    "net_inc",           // NUMBER
    "signature",         // PATH
    "print",             // QUAD
    "mask",              // REGEX
    "sort",              // SORT
    "attribution_intro", // STRING
    "zzz",               // UNKNOWN
  };

  const char *commands[] = {
    "set",
    "toggle",
    "reset",
    "unset",
  };

  const char *tests[] = {
    "%s %s",       "%s %s=42",  "%s %s?",     "%s ?%s",    "%s ?%s=42",
    "%s ?%s?",     "%s no%s",   "%s no%s=42", "%s no%s?",  "%s inv%s",
    "%s inv%s=42", "%s inv%s?", "%s &%s",     "%s &%s=42", "%s &%s?",
  };

  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  struct Buffer *line = buf_pool_get();

  for (size_t v = 0; v < countof(vars); v++)
  {
    for (size_t c = 0; c < countof(commands); c++)
    {
      TEST_CASE_("%s %s", commands[c], vars[v]);
      for (size_t t = 0; t < countof(tests); t++)
      {
        parse_error_reset(pe);

        buf_printf(line, tests[t], commands[c], vars[v]);
        // enum CommandResult rc =
        parse_rc_line(line, pc, pe);
      }
    }
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_rc(void)
{
  enum CommandResult rc = MUTT_CMD_ERROR;
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  commands_register(&NeoMutt->commands, mutt_commands);

  // enum CommandResult parse_rc_line(struct Buffer *line, struct Buffer *err);
  TEST_CASE("parse_rc_line");
  buf_reset(line);
  rc = parse_rc_line(line, pc, pe);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  buf_strcpy(line, "; set");
  TEST_CASE("parse_rc_line");
  rc = parse_rc_line(line, pc, pe);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  TEST_CASE("parse_rc_line");
  buf_strcpy(line, "# set");
  rc = parse_rc_line(line, pc, pe);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  TEST_CASE("parse_rc_line");
  buf_strcpy(line, "unknown");
  rc = parse_rc_line(line, pc, pe);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));
  struct HashElem *he = cs_get_elem(NeoMutt->sub->cs, "from");
  cs_he_initial_set(NeoMutt->sub->cs, he, "rich@flatcap.org", NULL);
  cs_str_reset(NeoMutt->sub->cs, "from", NULL);
  test_parse_set();

  buf_pool_release(&line);
  parse_context_free(&pc);
  parse_error_free(&pe);
  commands_clear(&NeoMutt->commands);
}
