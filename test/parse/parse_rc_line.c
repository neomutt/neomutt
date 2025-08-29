/**
 * @file
 * Test code for parsing "set" command
 *
 * @authors
 * Copyright (C) 2023-2025 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2023 Rayford Shireman
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
#include <stddef.h>
#include <stdint.h>
#include "mutt/lib.h"
#include "config/common.h" // IWYU pragma: keep
#include "config/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

struct ConfigDef ConfigVars[] = {
  // clang-format off
  { "Apple",      DT_BOOL,                          true,            0, NULL, },
  { "Banana",     DT_QUAD,                          MUTT_ASKYES,     0, NULL, },
  { "Cherry",     DT_NUMBER,                        555,             0, NULL, },
  { "Damson",     DT_STRING,                        IP "damson",     0, NULL, },
  { "Elderberry", DT_STRING|D_STRING_MAILBOX,       IP "elderberry", 0, NULL, },
  { "Fig",        DT_STRING|D_STRING_COMMAND,       IP "fig",        0, NULL, },
  { "Guava",      DT_PATH|D_PATH_FILE,              IP "guava",      0, NULL, },
  { "Hawthorn",   DT_STRING|D_INTERNAL_DEPRECATED,  0,               0, NULL, },
  { "Ilama",      DT_NUMBER|D_INTEGER_NOT_NEGATIVE, 0,               0, NULL, },
  { "Jackfruit",  DT_NUMBER,                        100,             0, NULL, },
  { "my_var2",    DT_MYVAR,                         IP "kumquat",    0, NULL, },
  { NULL },
  // clang-format on
};

// clang-format off
static struct ConfigDef MyVarDef =
  { "my_var",     DT_MYVAR,                   IP NULL,         0, NULL, };
// clang-format on

/**
 * test_command_set_expand_value
 *
 * @note buf_expand_path() simply prepends the string with "expanded"
 */
static void test_command_set_expand_value(void)
{
  void command_set_expand_value(uint32_t type, struct Buffer *value);

  mutt_str_replace(&NeoMutt->home_dir, "/home/neomutt");
  struct Buffer *buf = buf_pool_get();
  int type;

  type = DT_PATH | D_PATH_DIR;
  buf_strcpy(buf, "apple");
  command_set_expand_value(type, buf);
  TEST_CHECK_STR_EQ(buf_string(buf), "expanded/apple");

  type = DT_PATH;
  buf_strcpy(buf, "~/banana");
  command_set_expand_value(type, buf);
  TEST_CHECK_STR_EQ(buf_string(buf), "/home/neomutt/banana");

  type = DT_STRING | D_STRING_MAILBOX;
  buf_strcpy(buf, "cherry");
  command_set_expand_value(type, buf);
  TEST_CHECK_STR_EQ(buf_string(buf), "expanded/cherry");

  type = DT_STRING | D_STRING_COMMAND;
  buf_strcpy(buf, "damson");
  command_set_expand_value(type, buf);
  TEST_CHECK_STR_EQ(buf_string(buf), "expanded/damson");

  type = DT_STRING | D_STRING_COMMAND;
  buf_strcpy(buf, "builtin");
  command_set_expand_value(type, buf);
  TEST_CHECK_STR_EQ(buf_string(buf), "builtin");

  type = DT_BOOL;
  buf_strcpy(buf, "endive");
  command_set_expand_value(type, buf);
  TEST_CHECK_STR_EQ(buf_string(buf), "endive");

  buf_pool_release(&buf);
}

static void test_command_set_decrement(void)
{
  enum CommandResult command_set_decrement(struct Buffer * name, struct Buffer * value,
                                           struct Buffer * err);

  enum CommandResult rc;
  struct Buffer *err = buf_pool_get();
  struct Buffer *name = buf_pool_get();
  struct Buffer *value = buf_pool_get();

  buf_strcpy(name, "unknown");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_decrement(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "Hawthorn");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_decrement(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Ilama");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_decrement(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "Jackfruit");
  buf_strcpy(value, "10");
  buf_reset(err);
  rc = command_set_decrement(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_pool_release(&err);
  buf_pool_release(&name);
  buf_pool_release(&value);
}

static void test_command_set_increment(void)
{
  enum CommandResult command_set_increment(struct Buffer * name, struct Buffer * value,
                                           struct Buffer * err);

  enum CommandResult rc;
  struct Buffer *err = buf_pool_get();
  struct Buffer *name = buf_pool_get();
  struct Buffer *value = buf_pool_get();

  buf_strcpy(name, "unknown");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_increment(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "my_var");
  buf_strcpy(value, "42");
  buf_reset(err);
  rc = command_set_increment(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Hawthorn");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_increment(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Ilama");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_increment(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Banana");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_increment(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_pool_release(&err);
  buf_pool_release(&name);
  buf_pool_release(&value);
}

static void test_command_set_query(void)
{
  enum CommandResult command_set_query(struct Buffer * name, struct Buffer * err);

  enum CommandResult rc;
  struct Buffer *err = buf_pool_get();
  struct Buffer *name = buf_pool_get();

  StartupComplete = false;
  buf_reset(name);
  buf_reset(err);
  rc = command_set_query(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  StartupComplete = true;
  buf_reset(name);
  buf_reset(err);
  rc = command_set_query(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(!buf_is_empty(err));

  StartupComplete = false;
  buf_strcpy(name, "all");
  buf_reset(err);
  rc = command_set_query(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  StartupComplete = true;
  buf_strcpy(name, "all");
  buf_reset(err);
  rc = command_set_query(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "unknown");
  buf_reset(err);
  rc = command_set_query(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "Hawthorn");
  buf_reset(err);
  rc = command_set_query(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Guava");
  buf_reset(err);
  rc = command_set_query(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(!buf_is_empty(err));

  buf_pool_release(&err);
  buf_pool_release(&name);
}

static void test_command_set_reset(void)
{
  enum CommandResult command_set_reset(struct Buffer * name, struct Buffer * err);

  enum CommandResult rc;
  struct Buffer *err = buf_pool_get();
  struct Buffer *name = buf_pool_get();

  buf_strcpy(name, "unknown");
  buf_reset(err);
  rc = command_set_reset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "Hawthorn");
  buf_reset(err);
  rc = command_set_reset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Jackfruit");
  buf_reset(err);
  rc = command_set_reset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "my_var2");
  buf_reset(err);
  rc = command_set_reset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "all");
  buf_reset(err);
  rc = command_set_reset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_pool_release(&err);
  buf_pool_release(&name);
}

static void test_command_set_set(void)
{
  enum CommandResult command_set_set(struct Buffer * name,
                                     struct Buffer * value, struct Buffer * err);

  enum CommandResult rc;
  struct Buffer *err = buf_pool_get();
  struct Buffer *name = buf_pool_get();
  struct Buffer *value = buf_pool_get();

  buf_strcpy(name, "unknown");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_set(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "my_var2");
  buf_strcpy(value, "42");
  buf_reset(err);
  rc = command_set_set(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Hawthorn");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_set(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Ilama");
  buf_strcpy(value, "1");
  buf_reset(err);
  rc = command_set_set(name, value, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_pool_release(&err);
  buf_pool_release(&name);
  buf_pool_release(&value);
}

static void test_command_set_toggle(void)
{
  enum CommandResult command_set_toggle(struct Buffer * name, struct Buffer * err);

  enum CommandResult rc;
  struct Buffer *err = buf_pool_get();
  struct Buffer *name = buf_pool_get();

  buf_strcpy(name, "unknown");
  buf_reset(err);
  rc = command_set_toggle(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "Hawthorn");
  buf_reset(err);
  rc = command_set_toggle(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Apple");
  buf_reset(err);
  rc = command_set_toggle(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Banana");
  buf_reset(err);
  rc = command_set_toggle(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Cherry");
  buf_reset(err);
  rc = command_set_toggle(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Damson");
  buf_reset(err);
  rc = command_set_toggle(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_pool_release(&err);
  buf_pool_release(&name);
}

static void test_command_set_unset(void)
{
  enum CommandResult command_set_unset(struct Buffer * name, struct Buffer * err);

  enum CommandResult rc;
  struct Buffer *err = buf_pool_get();
  struct Buffer *name = buf_pool_get();

  buf_strcpy(name, "unknown");
  buf_reset(err);
  rc = command_set_unset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);
  TEST_CHECK(!buf_is_empty(err));

  buf_strcpy(name, "Hawthorn");
  buf_reset(err);
  rc = command_set_unset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Jackfruit");
  buf_reset(err);
  rc = command_set_unset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Apple");
  buf_reset(err);
  rc = command_set_unset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "Banana");
  buf_reset(err);
  rc = command_set_unset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_strcpy(name, "my_var2");
  buf_reset(err);
  rc = command_set_unset(name, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);
  TEST_CHECK(buf_is_empty(err));

  buf_pool_release(&err);
  buf_pool_release(&name);
}

static void test_parse_set(void)
{
  enum CommandResult parse_set(struct Buffer * buf, struct Buffer * s,
                               intptr_t data, struct Buffer *err);

  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();
  enum CommandResult rc;

  rc = parse_set(NULL, NULL, MUTT_SET_SET, NULL);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

  buf_reset(err);
  buf_strcpy(line, "invwrap");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_RESET, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);

  buf_reset(err);
  buf_strcpy(line, "wrap?");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_RESET, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);

  buf_reset(err);
  buf_strcpy(line, "invwrap++");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_SET, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);

  buf_reset(err);
  buf_strcpy(line, "invwrap = 42");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_SET, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);

  buf_reset(err);
  buf_strcpy(line, "wrap = 42");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_RESET, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);

  buf_reset(err);
  buf_strcpy(line, "wrap++");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_RESET, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);

  buf_reset(err);
  buf_strcpy(line, "index_format");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_INV, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_WARNING);

  buf_reset(err);
  buf_strcpy(line, "`missing");
  buf_seek(line, 0);
  rc = parse_set(tmp, line, MUTT_SET_INV, err);
  TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR);

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * set_non_empty_values
 *
 * Set the predefined config elements to something trueish/non-empty-ish.
 *
 * @return false if the setup fails
 */
static bool set_non_empty_values(void)
{
  bool ret = true;
  struct Buffer *err = buf_pool_get();
  // Just Apple..Fig
  for (int v = 0; v < 6; v++)
  {
    buf_reset(err);
    int rc = cs_str_reset(NeoMutt->sub->cs, ConfigVars[v].name, err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", ConfigVars[v].name, buf_string(err));
      ret = false;
    }
  }
  buf_pool_release(&err);
  return ret;
}

/**
 * set_empty_values
 *
 * Set the predefined config elements to something false-ish/empty-ish.
 *
 * @return false if the setup fails
 */
static bool set_empty_values(void)
{
  bool ret = true;
  struct Buffer *err = buf_pool_get();
  int rc = 0;

  buf_reset(err);
  rc = cs_str_string_set(NeoMutt->sub->cs, "Apple", "no", err);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
  {
    TEST_MSG("Failed to set dummy value for %s: %s", "Apple", buf_string(err));
    ret = false;
  }

  buf_reset(err);
  rc = cs_str_string_set(NeoMutt->sub->cs, "Banana", "no", err);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
  {
    TEST_MSG("Failed to set dummy value for %s: %s", "Banana", buf_string(err));
    ret = false;
  }

  buf_reset(err);
  rc = cs_str_string_set(NeoMutt->sub->cs, "Cherry", "0", err);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
  {
    TEST_MSG("Failed to set dummy value for %s: %s", "Cherry", buf_string(err));
    ret = false;
  }

  const char *stringlike[] = {
    "Damson",
    "Elderberry",
    "Fig",
    "Guava",
  };
  for (int i = 0; i < countof(stringlike); i++)
  {
    buf_reset(err);
    rc = cs_str_string_set(NeoMutt->sub->cs, stringlike[i], "", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(rc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", stringlike[i], buf_string(err));
      ret = false;
    }
  }

  buf_pool_release(&err);
  return ret;
}

/**
 * test_set
 *
 * Test the set command of the forms:
 *
 * * set foo = bar
 * * set foo  (for bool and quad)
 */
static void test_set(void)
{
  // set bool / quad config variable

  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  {
    const char *template[] = {
      "%s = yes",
      "%s",
    };
    for (int t = 0; t < countof(template); t++)
    {
      if (!TEST_CHECK(set_empty_values()))
      {
        TEST_MSG("setup failed");
        return;
      }

      const char *boolish[] = {
        "Apple",
        "Banana",
      };
      for (int v = 0; v < countof(boolish); v++)
      {
        buf_reset(err);
        buf_printf(line, template[t], boolish[v]);
        buf_seek(line, 0);
        enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
        if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return;
        }

        // Check effect
        buf_reset(err);
        int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
        if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
          return;
        }
        if (!TEST_CHECK_STR_EQ(buf_string(err), "yes"))
        {
          TEST_MSG("Variable not set %s: %s", boolish[v], buf_string(err));
          return;
        }
      }
    }
  }

  // set string
  {
    buf_reset(err);
    buf_strcpy(line, "Damson = newfoo");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return;
    }

    // Check effect
    buf_reset(err);
    int grc = cs_str_string_get(NeoMutt->sub->cs, "Damson", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to get %s: %s", "Damson", buf_string(err));
      return;
    }
    if (!TEST_CHECK_STR_EQ(buf_string(err), "newfoo"))
    {
      TEST_MSG("Variable not set %s: %s", "Damson", buf_string(err));
      return;
    }
  }

  // set on my_var succeeds even if not existent.
  {
    int grc = cs_str_delete(NeoMutt->sub->cs, "my_var", err);
    // return value grc is irrelevant.

    buf_reset(err);
    buf_strcpy(line, "my_var = newbar");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return;
    }

    // Check effect
    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to get %s: %s", "my_var", buf_string(err));
      return;
    }
    if (!TEST_CHECK_STR_EQ(buf_string(err), "newbar"))
    {
      TEST_MSG("Variable not set %s: %s", "my_var", buf_string(err));
      return;
    }
  }

  // set fails on unknown variable
  {
    buf_reset(err);
    buf_strcpy(line, "zzz = newbaz");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_ERROR, rc,
               buf_string(err));
      return;
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_unset
 *
 * Test the set command of the forms:
 *
 * * unset foo
 * * set nofoo (for bool and quad)
 * * unset my_foo
 */
static void test_unset(void)
{
  // unset bool / quad config variable

  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  {
    const struct Mapping template[] = {
      // clang-format off
      { "%s",   MUTT_SET_UNSET },
      { "no%s", MUTT_SET_SET   },
      // clang-format on
    };
    for (int t = 0; t < countof(template); t++)
    {
      if (!TEST_CHECK(set_non_empty_values()))
      {
        TEST_MSG("setup failed");
        return;
      }

      const char *boolish[] = {
        "Apple",
        "Banana",
      };
      for (int v = 0; v < countof(boolish); v++)
      {
        buf_reset(err);
        buf_strcpy(line, boolish[v]);
        buf_seek(line, 0);
        enum CommandResult rc = parse_set(tmp, line, MUTT_SET_UNSET, err);
        if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return;
        }

        // Check effect
        buf_reset(err);
        int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
        if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
          return;
        }
        if (!TEST_CHECK_STR_EQ(buf_string(err), "no"))
        {
          TEST_MSG("Variable not unset %s: %s", boolish[v], buf_string(err));
          return;
        }
      }
    }
  }

  // unset number sets it to 0
  {
    buf_reset(err);
    buf_strcpy(line, "Cherry");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_UNSET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return;
    }
  }

  // unset string
  {
    buf_reset(err);
    buf_strcpy(line, "Damson");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_UNSET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return;
    }

    // Check effect
    buf_reset(err);
    int grc = cs_str_string_get(NeoMutt->sub->cs, "Damson", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to get %s: %s", "Damson", buf_string(err));
      return;
    }
    if (!TEST_CHECK_STR_EQ(buf_string(err), ""))
    {
      TEST_MSG("Variable not unset %s: %s", "Damson", buf_string(err));
      return;
    }
  }

  // unset on my_var deletes it
  {
    // Delete any trace of my_var if existent
    cs_str_delete(NeoMutt->sub->cs, "my_var", err); // return value is irrelevant.
    buf_reset(err);
    if (!TEST_CHECK(cs_register_variable(NeoMutt->sub->cs, &MyVarDef, err) != NULL))
    {
      TEST_MSG("Failed to register my_var config variable: %s", buf_string(err));
      return;
    }
    buf_reset(err);
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return;
    }

    buf_reset(err);
    buf_strcpy(line, "my_var");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_UNSET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return;
    }

    // Check effect
    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_ERR_UNKNOWN))
    {
      TEST_MSG("my_var was not an unknown config variable: %s", buf_string(err));
      return;
    }
  }

  // unset fails on unknown variable
  {
    buf_reset(err);
    buf_strcpy(line, "zzz");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_UNSET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_ERROR))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_ERROR, rc,
               buf_string(err));
      return;
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_reset
 *
 * Test the set command of the forms:
 *
 * * reset foo
 * * set &foo
 */
static void test_reset(void)
{
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  {
    const struct Mapping template[] = {
      // clang-format off
      { "%s",  MUTT_SET_RESET },
      { "&%s", MUTT_SET_SET   },
      // clang-format on
    };
    for (int t = 0; t < countof(template); t++)
    {
      if (!TEST_CHECK(set_empty_values()))
      {
        TEST_MSG("setup failed");
        return;
      }

      // Just Apple..Fig
      for (int v = 0; v < 6; v++)
      {
        buf_reset(err);
        buf_printf(line, template[t].name, ConfigVars[v].name);
        buf_seek(line, 0);
        enum CommandResult rc = parse_set(tmp, line, template[t].value, err);
        if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return;
        }

        // Check effect
        buf_reset(err);
        int grc = cs_str_string_get(NeoMutt->sub->cs, ConfigVars[v].name, err);
        if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(err));
          return;
        }
        struct Buffer *buf = buf_pool_get();
        grc = cs_str_initial_get(NeoMutt->sub->cs, ConfigVars[v].name, buf);
        if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(buf));
          buf_pool_release(&buf);
          return;
        }
        if (!TEST_CHECK_STR_EQ(buf_string(err), buf->data))
        {
          TEST_MSG("Variable not reset %s: %s != %s", ConfigVars[v].name,
                   buf_string(err), buf_string(buf));
          buf_pool_release(&buf);
          return;
        }
        buf_pool_release(&buf);
      }
    }
  }

  // reset on my_var deletes it
  {
    // Delete any trace of my_var if existent
    cs_str_delete(NeoMutt->sub->cs, "my_var", err); // return value is irrelevant.
    buf_reset(err);
    if (!TEST_CHECK(cs_register_variable(NeoMutt->sub->cs, &MyVarDef, err) != NULL))
    {
      TEST_MSG("Failed to register my_var config variable: %s", buf_string(err));
      return;
    }
    buf_reset(err);
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return;
    }

    buf_reset(err);
    buf_strcpy(line, "my_var");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_RESET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return;
    }

    // Check effect
    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_ERR_UNKNOWN))
    {
      TEST_MSG("my_var was not an unknown config variable: %s", buf_string(err));
      return;
    }
  }

  // "reset all" resets all and also my_var
  {
    if (!TEST_CHECK(set_empty_values()))
    {
      TEST_MSG("setup failed");
      return;
    }
    // Delete any trace of my_var if existent
    cs_str_delete(NeoMutt->sub->cs, "my_var", err); // return value is irrelevant.
    buf_reset(err);
    if (!TEST_CHECK(cs_register_variable(NeoMutt->sub->cs, &MyVarDef, err) != NULL))
    {
      TEST_MSG("Failed to register my_var config variable: %s", buf_string(err));
      return;
    }
    buf_reset(err);
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return;
    }

    buf_reset(err);
    buf_strcpy(line, "all");
    buf_seek(line, 0);
    enum CommandResult rc = parse_set(tmp, line, MUTT_SET_RESET, err);
    if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return;
    }

    // Check effect
    // Just Apple..Fig
    for (int v = 0; v < 6; v++)
    {
      buf_reset(err);
      grc = cs_str_string_get(NeoMutt->sub->cs, ConfigVars[v].name, err);
      if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(err));
        return;
      }
      struct Buffer *buf = buf_pool_get();
      grc = cs_str_initial_get(NeoMutt->sub->cs, ConfigVars[v].name, buf);
      if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(buf));
        buf_pool_release(&buf);
        return;
      }
      if (!TEST_CHECK_STR_EQ(buf_string(err), buf->data))
      {
        TEST_MSG("Variable not reset %s: %s != %s", ConfigVars[v].name,
                 buf_string(err), buf_string(buf));
        buf_pool_release(&buf);
        return;
      }
      buf_pool_release(&buf);
    }

    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_ERR_UNKNOWN))
    {
      TEST_MSG("my_var was not an unknown config variable: expected = %d, got = %d, err = %s",
               CSR_ERR_UNKNOWN, CSR_RESULT(grc), buf_string(err));
      return;
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_toggle
 *
 * Test the set command of the forms:
 *
 * * toggle foo (for bool and quad)
 * * set invfoo (for bool and quad)
 */
static void test_toggle(void)
{
  // toggle bool / quad config variable

  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  {
    const struct Mapping template[] = {
      // clang-format off
      { "%s",    MUTT_SET_INV },
      { "inv%s", MUTT_SET_SET },
      // clang-format on
    };
    for (int t = 0; t < countof(template); t++)
    {
      if (!TEST_CHECK(set_non_empty_values()))
      {
        TEST_MSG("setup failed");
        return;
      }

      const char *boolish[] = {
        "Apple",
        "Banana",
      };
      const char *expected1[] = {
        "no",
        "ask-no",
      };
      const char *expected2[] = {
        "yes",
        "ask-yes",
      };
      for (int v = 0; v < countof(boolish); v++)
      {
        // First toggle
        {
          buf_reset(err);
          buf_printf(line, template[t].name, boolish[v]);
          buf_seek(line, 0);
          enum CommandResult rc = parse_set(tmp, line, template[t].value, err);
          if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
          {
            TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                     rc, buf_string(err));
            return;
          }

          // Check effect
          buf_reset(err);
          int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
          if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
          {
            TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
            return;
          }
          if (!TEST_CHECK_STR_EQ(buf_string(err), expected1[v]))
          {
            TEST_MSG("Variable %s not toggled off: got = %s, expected = %s",
                     boolish[v], buf_string(err), expected1[v], buf_string(err));
            return;
          }
        }

        // Second toggle
        {
          buf_reset(err);
          buf_printf(line, template[t].name, boolish[v]);
          buf_seek(line, 0);
          enum CommandResult rc = parse_set(tmp, line, template[t].value, err);
          if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
          {
            TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                     rc, buf_string(err));
            return;
          }

          // Check effect
          buf_reset(err);
          int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
          if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
          {
            TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
            return;
          }
          if (!TEST_CHECK_STR_EQ(buf_string(err), expected2[v]))
          {
            TEST_MSG("Variable %s not toggled on: got = %s, expected = %s",
                     boolish[v], buf_string(err), expected2[v], buf_string(err));
            return;
          }
        }
      }
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_query
 *
 * Test the set command of the forms:
 *
 * * set foo?
 * * set ?foo
 * * set foo  (for non bool and non quad)
 */
static void test_query(void)
{
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  {
    const char *template[] = {
      "%s?",
      "?%s",
    };
    for (int t = 0; t < countof(template); t++)
    {
      if (!TEST_CHECK(set_non_empty_values()))
      {
        TEST_MSG("setup failed");
        return;
      }
      // Delete any trace of my_var if existent
      cs_str_delete(NeoMutt->sub->cs, "my_var", err); // return value is irrelevant
      buf_reset(err);
      if (!TEST_CHECK(cs_register_variable(NeoMutt->sub->cs, &MyVarDef, err) != NULL))
      {
        TEST_MSG("Failed to register my_var config variable: %s", buf_string(err));
        return;
      }
      buf_reset(err);
      int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
      if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
      {
        TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
        return;
      }

      const char *vars[] = {
        "Apple", "Banana", "Cherry", "Damson", "my_var",
      };
      const char *expected[] = {
        "yes", "ask-yes", "555", "damson", "foo",
      };
      for (int v = 0; v < countof(vars); v++)
      {
        buf_reset(err);
        buf_printf(line, template[t], vars[v]);
        buf_seek(line, 0);
        enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
        if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return;
        }

        // Check effect
        buf_printf(line, "%s=\"%s\"", vars[v], expected[v]);
        if (!TEST_CHECK_STR_EQ(buf_string(err), buf_string(line)))
        {
          TEST_MSG("Variable query failed for %s: got = %s, expected = %s",
                   vars[v], buf_string(err), buf_string(line));
          return;
        }
      }
    }
  }

  // Non-bool or quad variables can also be queried with "set foo"
  {
    if (!TEST_CHECK(set_non_empty_values()))
    {
      TEST_MSG("setup failed");
      return;
    }
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return;
    }

    const char *vars[] = {
      "Cherry",
      "Damson",
      "my_var",
    };
    const char *expected[] = {
      "555",
      "damson",
      "foo",
    };
    for (int v = 0; v < countof(vars); v++)
    {
      buf_reset(err);
      buf_strcpy(line, vars[v]);
      buf_seek(line, 0);
      enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
      if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return;
      }

      // Check effect
      buf_printf(line, "%s=\"%s\"", vars[v], expected[v]);
      if (!TEST_CHECK_STR_EQ(buf_string(err), buf_string(line)))
      {
        TEST_MSG("Variable query failed for %s: got = %s, expected = %s",
                 vars[v], buf_string(err), buf_string(line));
        return;
      }
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_increment
 *
 * Test the set command of the forms:
 *
 * * set foo += bar
 * * set foo += bar (my_var)
 */
static void test_increment(void)
{
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  if (!TEST_CHECK(set_non_empty_values()))
  {
    TEST_MSG("setup failed");
    return;
  }
  int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
  if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
  {
    TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
    return;
  }

  // increment number
  {
    const char *vars[] = {
      "Cherry",
      "Damson",
      "my_var",
    };
    const char *increment[] = {
      "100",
      "smell",
      "bar",
    };
    const char *expected[] = {
      "655",
      "damsonsmell",
      "foobar",
    };
    for (int v = 0; v < countof(vars); v++)
    {
      buf_reset(err);
      buf_printf(line, "%s += %s", vars[v], increment[v]);
      buf_seek(line, 0);
      enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
      if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return;
      }

      // Check effect
      buf_reset(err);
      grc = cs_str_string_get(NeoMutt->sub->cs, vars[v], err);
      if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", vars[v], buf_string(err));
        return;
      }
      if (!TEST_CHECK_STR_EQ(buf_string(err), expected[v]))
      {
        TEST_MSG("Variable not incremented %s: got = %s, expected = %s",
                 vars[v], buf_string(err), expected[v]);
        return;
      }
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_decrement
 *
 * Test the set command of the forms:
 *
 * * set foo -= bar
 */
static void test_decrement(void)
{
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  if (!TEST_CHECK(set_non_empty_values()))
  {
    TEST_MSG("setup failed");
    return;
  }

  // decrement number
  {
    const char *vars[] = {
      "Cherry",
    };
    const char *increment[] = {
      "100",
    };
    const char *expected[] = {
      "455",
    };
    for (int v = 0; v < countof(vars); v++)
    {
      buf_reset(err);
      buf_printf(line, "%s -= %s", vars[v], increment[v]);
      buf_seek(line, 0);
      enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
      if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return;
      }

      // Check effect
      buf_reset(err);
      int grc = cs_str_string_get(NeoMutt->sub->cs, vars[v], err);
      if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", vars[v], buf_string(err));
        return;
      }
      if (!TEST_CHECK_STR_EQ(buf_string(err), expected[v]))
      {
        TEST_MSG("Variable not decremented %s: got = %s, expected = %s",
                 vars[v], buf_string(err), expected[v]);
        return;
      }
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_invalid_syntax
 *
 * Test that invalid syntax forms of "set" error out.
 */
static void test_invalid_syntax(void)
{
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  {
    const char *template[] = {
      // clang-format off
      "&&Cherry",   "?&Cherry",   "&Cherry?",    "no&Cherry",    "inv&Cherry",
      "&?Cherry",   "??Cherry",   "?Cherry?",    "no?Cherry",    "inv?Cherry",
      "&Cherry?",   "?Cherry?",   "noCherry?",   "invCherry?",   "&noCherry",
      "?noCherry",  "noCherry?",  "nonoCherry",  "invnoCherry",  "&invCherry",
      "?invCherry", "invCherry?", "noinvCherry", "invinvCherry",
      "Cherry+",    "Cherry-",
      // clang-format on
    };

    for (int t = 0; t < countof(template); t++)
    {
      buf_reset(err);
      buf_strcpy(line, template[t]);
      buf_seek(line, 0);
      enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
      if (!TEST_CHECK((rc == MUTT_CMD_WARNING) || (rc == MUTT_CMD_ERROR)))
      {
        TEST_MSG("For command '%s': Expected %d or %d, but got %d; err is: '%s'",
                 template[t], MUTT_CMD_WARNING, MUTT_CMD_ERROR, rc, buf_string(err));
        // return;
      }
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

/**
 * test_path_expanding
 *
 * Test if paths are expanded when setting a value (set name = value):
 *
 * * mailbox: =foo, +foo
 * * command: ~/bin/foo
 * * path: ~/bin/foo
 */
static void test_path_expanding(void)
{
  struct Buffer *err = buf_pool_get();
  struct Buffer *line = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  {
    const char *pathlike[] = {
      "Elderberry",
      "Fig",
      "Guava",
    };
    const char *newvalue[] = {
      "<",
      "~/bar",
      "=foo",
    };
    const char *expected[] = {
      "expanded/<",
      "expanded/~/bar",
      "expanded/=foo",
    };
    for (int v = 0; v < countof(pathlike); v++)
    {
      buf_reset(err);
      buf_printf(line, "%s = %s", pathlike[v], newvalue[v]);
      buf_seek(line, 0);
      enum CommandResult rc = parse_set(tmp, line, MUTT_SET_SET, err);
      if (!TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return;
      }

      // Check effect
      buf_reset(err);
      int grc = cs_str_string_get(NeoMutt->sub->cs, pathlike[v], err);
      if (!TEST_CHECK_NUM_EQ(CSR_RESULT(grc), CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", pathlike[v], buf_string(err));
        return;
      }
      if (!TEST_CHECK_STR_EQ(buf_string(err), expected[v]))
      {
        TEST_MSG("Variable not incremented %s: got = %s, expected = %s",
                 pathlike[v], buf_string(err), expected[v]);
        return;
      }
    }
  }

  buf_pool_release(&err);
  buf_pool_release(&line);
  buf_pool_release(&tmp);
}

void test_command_set(void)
{
  if (!TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, ConfigVars)))
  {
    TEST_MSG("Failed to register config variables");
    return;
  }

  commands_register(&NeoMutt->commands, mutt_commands);
  MuttLogger = log_disp_null;

  size_t num = ARRAY_SIZE(&NeoMutt->commands);
  TEST_CHECK_NUM_EQ(num, 4);

  const struct Command *cmd = NULL;
  cmd = commands_get(&NeoMutt->commands, "toggle");
  TEST_CHECK(cmd != NULL);

  cmd = commands_get(&NeoMutt->commands, "apple");
  TEST_CHECK(cmd == NULL);

  test_command_set_expand_value();
  test_command_set_decrement();
  test_command_set_increment();
  test_command_set_query();
  test_command_set_reset();
  test_command_set_set();
  test_command_set_toggle();
  test_command_set_unset();
  test_parse_set();

  test_set();
  test_reset();
  test_unset();
  test_toggle();
  test_query();
  test_increment();
  test_decrement();
  test_invalid_syntax();
  test_path_expanding();

  commands_clear(&NeoMutt->commands);
}
