/**
 * @file
 * Test code for parsing config files
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
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

extern const struct Mapping MboxTypeMap[];
extern struct EnumDef MboxTypeDef;

/**
 * SortMethods - Sort methods for '$sort' for the index
 */
const struct Mapping SortMethods[] = {
  // clang-format off
  { "date",    SORT_DATE },
  { "from",    SORT_FROM },
  { "label",   SORT_LABEL },
  { "size",    SORT_SIZE },
  { "spam",    SORT_SPAM },
  { "subject", SORT_SUBJECT },
  { "to",      SORT_TO },
  { NULL, 0 },
  // clang-format on
};

static struct ConfigDef Vars[] = {
  // clang-format off
  { "from",              DT_ADDRESS,                 0,                     0,               NULL, },
  { "beep",              DT_BOOL,                    true,                  0,               NULL, },
  { "ispell",            DT_STRING|DT_COMMAND,       IP "ispell",           0,               NULL, },
  { "mbox_type",         DT_ENUM,                    MUTT_MBOX,             IP &MboxTypeDef, NULL, },
  { "to_chars",          DT_MBTABLE|R_INDEX,         IP " +TCFLR",          0,               NULL, },
  { "net_inc",           DT_NUMBER|DT_NOT_NEGATIVE,  10,                    0,               NULL, },
  { "signature",         DT_PATH|DT_PATH_FILE,       IP "~/.signature",     0,               NULL, },
  { "print",             DT_QUAD,                    MUTT_ASKNO,            0,               NULL, },
  { "mask",              DT_REGEX|DT_REGEX_NOSUB,    IP "!^\\.[^.]",        0,               NULL, },
  { "sort",              DT_SORT|DT_SORT_LAST,       SORT_DATE,             IP SortMethods,  NULL, },
  { "attribution_intro", DT_STRING,                  IP "On %d, %n wrote:", 0,               NULL, },
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

  struct Buffer *err = buf_pool_get();
  char line[64];

  for (size_t v = 0; v < mutt_array_size(vars); v++)
  {
    for (size_t c = 0; c < mutt_array_size(commands); c++)
    {
      TEST_CASE_("%s %s", commands[c], vars[v]);
      for (size_t t = 0; t < mutt_array_size(tests); t++)
      {
        buf_reset(err);

        snprintf(line, sizeof(line), tests[t], commands[c], vars[v]);
        // enum CommandResult rc =
        parse_rc_line(line, err);
      }
    }
  }

  buf_pool_release(&err);
}

void test_parse_rc(void)
{
  enum CommandResult rc = MUTT_CMD_ERROR;

  TEST_CASE("parse_rc_line");
  rc = parse_rc_line(NULL, NULL);
  TEST_CHECK(rc == MUTT_CMD_ERROR);

  TEST_CASE("parse_rc_buffer");
  rc = parse_rc_buffer(NULL, NULL, NULL);
  TEST_CHECK(rc == MUTT_CMD_SUCCESS);

  TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, Vars, DT_NO_FLAGS));
  cs_str_initial_set(NeoMutt->sub->cs, "from", "rich@flatcap.org", NULL);
  cs_str_reset(NeoMutt->sub->cs, "from", NULL);
  test_parse_set();
}
