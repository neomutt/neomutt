/**
 * @file
 * Test code for parsing config files
 *
 * @authors
 * Copyright (C) 2023-2025 Richard Russon <rich@flatcap.org>
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
#include "common.h"      // IWYU pragma: keep
#include "test_common.h" // IWYU pragma: keep

extern const struct Mapping MboxTypeMap[];
extern struct EnumDef MboxTypeDef;

/**
 * SortMethods - Sort methods for '$sort' for the index
 */
const struct Mapping SortMethods[] = {
  // clang-format off
  { "date",    EMAIL_SORT_DATE },
  { "from",    EMAIL_SORT_FROM },
  { "label",   EMAIL_SORT_LABEL },
  { "size",    EMAIL_SORT_SIZE },
  { "spam",    EMAIL_SORT_SPAM },
  { "subject", EMAIL_SORT_SUBJECT },
  { "to",      EMAIL_SORT_TO },
  { NULL, 0 },
  // clang-format on
};

static struct ConfigDef Vars[] = {
  // clang-format off
  { "from",              DT_ADDRESS,                       0,                     0,               NULL, },
  { "beep",              DT_BOOL,                          true,                  0,               NULL, },
  { "ispell",            DT_STRING|D_STRING_COMMAND,       IP "ispell",           0,               NULL, },
  { "mbox_type",         DT_ENUM,                          MUTT_MBOX,             IP &MboxTypeDef, NULL, },
  { "to_chars",          DT_MBTABLE,                       IP " +TCFLR",          0,               NULL, },
  { "net_inc",           DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 10,                    0,               NULL, },
  { "signature",         DT_PATH|D_PATH_FILE,              IP "~/.signature",     0,               NULL, },
  { "print",             DT_QUAD,                          MUTT_ASKNO,            0,               NULL, },
  { "mask",              DT_REGEX|D_REGEX_NOSUB,           IP "!^\\.[^.]",        0,               NULL, },
  { "sort",              DT_SORT|D_SORT_LAST,              EMAIL_SORT_DATE,             IP SortMethods,  NULL, },
  { "attribution_intro", DT_STRING,                        IP "On %d, %n wrote:", 0,               NULL, },
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

  struct Buffer *line = buf_pool_get();
  struct Buffer *token = buf_pool_get();
  struct Buffer *err = buf_pool_get();
  char linestr[64];

  for (size_t v = 0; v < countof(vars); v++)
  {
    for (size_t c = 0; c < countof(commands); c++)
    {
      TEST_CASE_("%s %s", commands[c], vars[v]);
      for (size_t t = 0; t < countof(tests); t++)
      {
        buf_reset(err);

        snprintf(linestr, sizeof(linestr), tests[t], commands[c], vars[v]);
        buf_strcpy(line, linestr);
        // enum CommandResult rc =
        parse_rc_line(line, token, err);
      }
    }
  }

  buf_pool_release(&line);
  buf_pool_release(&token);
  buf_pool_release(&err);
}

void test_parse_rc(void)
{
  enum CommandResult rc = MUTT_CMD_ERROR;
  struct Buffer *line = buf_pool_get();
  struct Buffer *token = buf_pool_get();

  commands_register(&NeoMutt->commands, mutt_commands);

  // enum CommandResult parse_rc_line(struct Buffer *line, struct Buffer *token, struct Buffer *err);
  TEST_CASE("parse_rc_line");
  rc = parse_rc_line(NULL, NULL, NULL);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  TEST_CASE("parse_rc_line");
  buf_strcpy(line, "; set");
  rc = parse_rc_line(line, token, NULL);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  TEST_CASE("parse_rc_line");
  buf_strcpy(line, "# set");
  rc = parse_rc_line(line, token, NULL);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

  TEST_CASE("parse_rc_line");
  buf_strcpy(line, "unknown");
  rc = parse_rc_line(line, token, NULL);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

  buf_pool_release(&line);
  buf_pool_release(&token);

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars));
  struct HashElem *he = cs_get_elem(NeoMutt->sub->cs, "from");
  cs_he_initial_set(NeoMutt->sub->cs, he, "rich@flatcap.org", NULL);
  cs_str_reset(NeoMutt->sub->cs, "from", NULL);
  test_parse_set();

  commands_clear(&NeoMutt->commands);
}
