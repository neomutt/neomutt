/**
 * @file
 * Test code for parsing "set" command
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
#include <stddef.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "parse/lib.h"
#include "common.h"
#include "test_common.h"

// clang-format off
static struct ConfigDef ConfigVars[] = {
  { "Apple",      DT_BOOL,              true,            0, NULL, },
  { "Banana",     DT_QUAD,              MUTT_ASKYES,     0, NULL, },
  { "Cherry",     DT_NUMBER,            555,             0, NULL, },
  { "Damson",     DT_STRING,            IP "damson",     0, NULL, },
  { "Elderberry", DT_STRING|DT_MAILBOX, IP "elderberry", 0, NULL, },
  { "Fig",        DT_STRING|DT_COMMAND, IP "fig",        0, NULL, },
  { "Guava",      DT_PATH|DT_PATH_FILE, IP "guava",      0, NULL, },
  { NULL },
};
static struct ConfigDef MyVarDef =
  { "my_var",     DT_MYVAR,        IP NULL,         0, NULL, };
// clang-format on

/**
 * Set the predefined config elements to something trueish/non-empty-ish.
 *
 * @return false if the setup fails
 */
static bool set_non_empty_values(void)
{
  bool ret = true;
  struct Buffer *err = buf_pool_get();
  for (int v = 0; v < mutt_array_size(ConfigVars) - 1; v++)
  {
    buf_reset(err);
    int rc = cs_str_reset(NeoMutt->sub->cs, ConfigVars[v].name, err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", ConfigVars[v].name, buf_string(err));
      ret = false;
    }
  }
  buf_pool_release(&err);
  return ret;
}

/**
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
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Failed to set dummy value for %s: %s", "Apple", buf_string(err));
    ret = false;
  }

  buf_reset(err);
  rc = cs_str_string_set(NeoMutt->sub->cs, "Banana", "no", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Failed to set dummy value for %s: %s", "Banana", buf_string(err));
    ret = false;
  }

  buf_reset(err);
  rc = cs_str_string_set(NeoMutt->sub->cs, "Cherry", "0", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
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
  for (int i = 0; i < mutt_array_size(stringlike); i++)
  {
    buf_reset(err);
    rc = cs_str_string_set(NeoMutt->sub->cs, stringlike[i], "", err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", stringlike[i], buf_string(err));
      ret = false;
    }
  }

  buf_pool_release(&err);
  return ret;
}

/**
 * Test the set command of the forms:
 *
 * * set foo = bar
 * * set foo  (for bool and quad)
 */
static bool test_set(struct Buffer *err)
{
  // set bool / quad config variable
  {
    const char *template[] = {
      "set %s = yes",
      "set %s",
    };
    for (int t = 0; t < mutt_array_size(template); t++)
    {
      if (!TEST_CHECK(set_empty_values()))
      {
        TEST_MSG("setup failed");
        return false;
      }

      const char *boolish[] = {
        "Apple",
        "Banana",
      };
      for (int v = 0; v < mutt_array_size(boolish); v++)
      {
        char line[64] = { 0 };
        snprintf(line, sizeof(line), template[t], boolish[v]);
        buf_reset(err);
        enum CommandResult rc = parse_rc_line(line, err);
        if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return false;
        }

        // Check effect
        buf_reset(err);
        int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
        if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
          return false;
        }
        if (!TEST_CHECK_STR_EQ(err->data, "yes"))
        {
          TEST_MSG("Variable not set %s: %s", boolish[v], buf_string(err));
          return false;
        }
      }
    }
  }

  // set string
  {
    buf_reset(err);
    enum CommandResult rc = parse_rc_line("set Damson = newfoo", err);
    if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return false;
    }

    // Check effect
    buf_reset(err);
    int grc = cs_str_string_get(NeoMutt->sub->cs, "Damson", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to get %s: %s", "Damson", buf_string(err));
      return false;
    }
    if (!TEST_CHECK_STR_EQ(err->data, "newfoo"))
    {
      TEST_MSG("Variable not set %s: %s", "Damson", buf_string(err));
      return false;
    }
  }

  // set on my_var succeeds even if not existent.
  {
    int grc = cs_str_delete(NeoMutt->sub->cs, "my_var", err);
    // return value grc is irrelevant.

    buf_reset(err);
    enum CommandResult rc = parse_rc_line("set my_var = newbar", err);
    if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return false;
    }

    // Check effect
    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to get %s: %s", "my_var", buf_string(err));
      return false;
    }
    if (!TEST_CHECK(mutt_str_equal(err->data, "newbar")))
    {
      TEST_MSG("Variable not set %s: %s", "my_var", buf_string(err));
      return false;
    }
  }

  // set fails on unknown variable
  {
    buf_reset(err);
    enum CommandResult rc = parse_rc_line("set zzz = newbaz", err);
    if (!TEST_CHECK(rc == MUTT_CMD_ERROR))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_ERROR, rc,
               buf_string(err));
      return false;
    }
  }

  return true;
}

/**
 * Test the set command of the forms:
 *
 * * unset foo
 * * set nofoo (for bool and quad)
 * * unset my_foo
 */
static bool test_unset(struct Buffer *err)
{
  // unset bool / quad config variable
  {
    const char *template[] = {
      "unset %s",
      "set no%s",
    };
    for (int t = 0; t < mutt_array_size(template); t++)
    {
      if (!TEST_CHECK(set_non_empty_values()))
      {
        TEST_MSG("setup failed");
        return false;
      }

      const char *boolish[] = {
        "Apple",
        "Banana",
      };
      for (int v = 0; v < mutt_array_size(boolish); v++)
      {
        char line[64] = { 0 };
        snprintf(line, sizeof(line), "unset %s", boolish[v]);
        buf_reset(err);
        enum CommandResult rc = parse_rc_line(line, err);
        if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return false;
        }

        // Check effect
        buf_reset(err);
        int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
        if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
          return false;
        }
        if (!TEST_CHECK_STR_EQ(err->data, "no"))
        {
          TEST_MSG("Variable not unset %s: %s", boolish[v], buf_string(err));
          return false;
        }
      }
    }
  }

  // unset number must fail
  {
    buf_reset(err);
    enum CommandResult rc = parse_rc_line("unset Cherry", err);
    if (!TEST_CHECK(rc == MUTT_CMD_ERROR))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_ERROR, rc,
               buf_string(err));
      return false;
    }
  }

  // unset string
  {
    buf_reset(err);
    enum CommandResult rc = parse_rc_line("unset Damson", err);
    if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return false;
    }

    // Check effect
    buf_reset(err);
    int grc = cs_str_string_get(NeoMutt->sub->cs, "Damson", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to get %s: %s", "Damson", buf_string(err));
      return false;
    }
    if (!TEST_CHECK_STR_EQ(err->data, ""))
    {
      TEST_MSG("Variable not unset %s: %s", "Damson", buf_string(err));
      return false;
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
      return false;
    }
    buf_reset(err);
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return false;
    }

    buf_reset(err);
    enum CommandResult rc = parse_rc_line("unset my_var", err);
    if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return false;
    }

    // Check effect
    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_ERR_UNKNOWN))
    {
      TEST_MSG("my_var was not an unknown config variable: %s", buf_string(err));
      return false;
    }
  }

  // unset fails on unknown variable
  {
    buf_reset(err);
    enum CommandResult rc = parse_rc_line("unset zzz", err);
    if (!TEST_CHECK(rc == MUTT_CMD_ERROR))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_ERROR, rc,
               buf_string(err));
      return false;
    }
  }

  return true;
}

/**
 * Test the set command of the forms:
 *
 * * reset foo
 * * set &foo
 */
static bool test_reset(struct Buffer *err)
{
  {
    const char *template[] = {
      "reset %s",
      "set &%s",
    };
    for (int t = 0; t < mutt_array_size(template); t++)
    {
      if (!TEST_CHECK(set_empty_values()))
      {
        TEST_MSG("setup failed");
        return false;
      }

      for (int v = 0; v < mutt_array_size(ConfigVars) - 1; v++)
      {
        char line[64] = { 0 };
        snprintf(line, sizeof(line), template[t], ConfigVars[v].name);
        buf_reset(err);
        enum CommandResult rc = parse_rc_line(line, err);
        if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return false;
        }

        // Check effect
        buf_reset(err);
        int grc = cs_str_string_get(NeoMutt->sub->cs, ConfigVars[v].name, err);
        if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(err));
          return false;
        }
        struct Buffer *buf = buf_pool_get();
        grc = cs_str_initial_get(NeoMutt->sub->cs, ConfigVars[v].name, buf);
        if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
        {
          TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(buf));
          buf_pool_release(&buf);
          return false;
        }
        if (!TEST_CHECK_STR_EQ(err->data, buf->data))
        {
          TEST_MSG("Variable not reset %s: %s != %s", ConfigVars[v].name,
                   buf_string(err), buf_string(buf));
          buf_pool_release(&buf);
          return false;
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
      return false;
    }
    buf_reset(err);
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return false;
    }

    buf_reset(err);
    enum CommandResult rc = parse_rc_line("reset my_var", err);
    if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return false;
    }

    // Check effect
    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_ERR_UNKNOWN))
    {
      TEST_MSG("my_var was not an unknown config variable: %s", buf_string(err));
      return false;
    }
  }

  // "reset all" resets all and also my_var
  {
    if (!TEST_CHECK(set_empty_values()))
    {
      TEST_MSG("setup failed");
      return false;
    }
    // Delete any trace of my_var if existent
    cs_str_delete(NeoMutt->sub->cs, "my_var", err); // return value is irrelevant.
    buf_reset(err);
    if (!TEST_CHECK(cs_register_variable(NeoMutt->sub->cs, &MyVarDef, err) != NULL))
    {
      TEST_MSG("Failed to register my_var config variable: %s", buf_string(err));
      return false;
    }
    buf_reset(err);
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return false;
    }

    buf_reset(err);
    enum CommandResult rc = parse_rc_line("reset all", err);
    if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
    {
      TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
               buf_string(err));
      return false;
    }

    // Check effect
    for (int v = 0; v < mutt_array_size(ConfigVars) - 1; v++)
    {
      buf_reset(err);
      grc = cs_str_string_get(NeoMutt->sub->cs, ConfigVars[v].name, err);
      if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(err));
        return false;
      }
      struct Buffer *buf = buf_pool_get();
      grc = cs_str_initial_get(NeoMutt->sub->cs, ConfigVars[v].name, buf);
      if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", ConfigVars[v].name, buf_string(buf));
        buf_pool_release(&buf);
        return false;
      }
      if (!TEST_CHECK(mutt_str_equal(err->data, buf->data)))
      {
        TEST_MSG("Variable not reset %s: %s != %s", ConfigVars[v].name,
                 buf_string(err), buf_string(buf));
        buf_pool_release(&buf);
        return false;
      }
      buf_pool_release(&buf);
    }

    buf_reset(err);
    grc = cs_str_string_get(NeoMutt->sub->cs, "my_var", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_ERR_UNKNOWN))
    {
      TEST_MSG("my_var was not an unknown config variable: expected = %d, got = %d, err = %s",
               CSR_ERR_UNKNOWN, CSR_RESULT(grc), buf_string(err));
      return false;
    }
  }

  return true;
}

/**
 * Test the set command of the forms:
 *
 * * toggle foo (for bool and quad)
 * * set invfoo (for bool and quad)
 */
static bool test_toggle(struct Buffer *err)
{
  // toggle bool / quad config variable
  {
    const char *template[] = {
      "toggle %s",
      "set inv%s",
    };
    for (int t = 0; t < mutt_array_size(template); t++)
    {
      if (!TEST_CHECK(set_non_empty_values()))
      {
        TEST_MSG("setup failed");
        return false;
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
      for (int v = 0; v < mutt_array_size(boolish); v++)
      {
        // First toggle
        {
          char line[64] = { 0 };
          snprintf(line, sizeof(line), template[t], boolish[v]);
          buf_reset(err);
          enum CommandResult rc = parse_rc_line(line, err);
          if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
          {
            TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                     rc, buf_string(err));
            return false;
          }

          // Check effect
          buf_reset(err);
          int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
          if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
          {
            TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
            return false;
          }
          if (!TEST_CHECK_STR_EQ(err->data, expected1[v]))
          {
            TEST_MSG("Variable %s not toggled off: got = %s, expected = %s",
                     boolish[v], err->data, expected1[v], buf_string(err));
            return false;
          }
        }

        // Second toggle
        {
          char line[64] = { 0 };
          snprintf(line, sizeof(line), template[t], boolish[v]);
          buf_reset(err);
          enum CommandResult rc = parse_rc_line(line, err);
          if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
          {
            TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                     rc, buf_string(err));
            return false;
          }

          // Check effect
          buf_reset(err);
          int grc = cs_str_string_get(NeoMutt->sub->cs, boolish[v], err);
          if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
          {
            TEST_MSG("Failed to get %s: %s", boolish[v], buf_string(err));
            return false;
          }
          if (!TEST_CHECK_STR_EQ(err->data, expected2[v]))
          {
            TEST_MSG("Variable %s not toggled on: got = %s, expected = %s",
                     boolish[v], err->data, expected2[v], buf_string(err));
            return false;
          }
        }
      }
    }
  }

  return true;
}

/**
 * Test the set command of the forms:
 *
 * * set foo?
 * * set ?foo
 * * set foo  (for non bool and non quad)
 */
static bool test_query(struct Buffer *err)
{
  {
    const char *template[] = {
      "set %s?",
      "set ?%s",
    };
    for (int t = 0; t < mutt_array_size(template); t++)
    {
      if (!TEST_CHECK(set_non_empty_values()))
      {
        TEST_MSG("setup failed");
        return false;
      }
      // Delete any trace of my_var if existent
      cs_str_delete(NeoMutt->sub->cs, "my_var", err); // return value is irrelevant.
      buf_reset(err);
      if (!TEST_CHECK(cs_register_variable(NeoMutt->sub->cs, &MyVarDef, err) != NULL))
      {
        TEST_MSG("Failed to register my_var config variable: %s", buf_string(err));
        return false;
      }
      buf_reset(err);
      int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
      if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
      {
        TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
        return false;
      }

      const char *vars[] = {
        "Apple", "Banana", "Cherry", "Damson", "my_var",
      };
      const char *expected[] = {
        "yes", "ask-yes", "555", "damson", "foo",
      };
      for (int v = 0; v < mutt_array_size(vars); v++)
      {
        char line[64] = { 0 };
        snprintf(line, sizeof(line), template[t], vars[v]);
        buf_reset(err);
        enum CommandResult rc = parse_rc_line(line, err);
        if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
        {
          TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS,
                   rc, buf_string(err));
          return false;
        }

        // Check effect
        snprintf(line, sizeof(line), "%s=\"%s\"", vars[v], expected[v]);
        if (!TEST_CHECK_STR_EQ(err->data, line))
        {
          TEST_MSG("Variable query failed for %s: got = %s, expected = %s",
                   vars[v], buf_string(err), line);
          return false;
        }
      }
    }
  }

  // Non-bool or quad variables can also be queried with "set foo"
  {
    if (!TEST_CHECK(set_non_empty_values()))
    {
      TEST_MSG("setup failed");
      return false;
    }
    int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
    if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
    {
      TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
      return false;
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
    for (int v = 0; v < mutt_array_size(vars); v++)
    {
      char line[64] = { 0 };
      snprintf(line, sizeof(line), "set %s", vars[v]);
      buf_reset(err);
      enum CommandResult rc = parse_rc_line(line, err);
      if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return false;
      }

      // Check effect
      snprintf(line, sizeof(line), "%s=\"%s\"", vars[v], expected[v]);
      if (!TEST_CHECK_STR_EQ(err->data, line))
      {
        TEST_MSG("Variable query failed for %s: got = %s, expected = %s",
                 vars[v], buf_string(err), line);
        return false;
      }
    }
  }

  return true;
}

/**
 * Test the set command of the forms:
 *
 * * set foo += bar
 * * set foo += bar (my_var)
 */
static bool test_increment(struct Buffer *err)
{
  if (!TEST_CHECK(set_non_empty_values()))
  {
    TEST_MSG("setup failed");
    return false;
  }
  int grc = cs_str_string_set(NeoMutt->sub->cs, "my_var", "foo", err);
  if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
  {
    TEST_MSG("Failed to set dummy value for %s: %s", "my_var", buf_string(err));
    return false;
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
    for (int v = 0; v < mutt_array_size(vars); v++)
    {
      char line[64] = { 0 };
      snprintf(line, sizeof(line), "set %s += %s", vars[v], increment[v]);
      buf_reset(err);
      enum CommandResult rc = parse_rc_line(line, err);
      if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return false;
      }

      // Check effect
      buf_reset(err);
      grc = cs_str_string_get(NeoMutt->sub->cs, vars[v], err);
      if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", vars[v], buf_string(err));
        return false;
      }
      if (!TEST_CHECK_STR_EQ(err->data, expected[v]))
      {
        TEST_MSG("Variable not incremented %s: got = %s, expected = %s",
                 vars[v], buf_string(err), expected[v]);
        return false;
      }
    }
  }

  return true;
}

/**
 * Test the set command of the forms:
 *
 * * set foo -= bar
 */
static bool test_decrement(struct Buffer *err)
{
  if (!TEST_CHECK(set_non_empty_values()))
  {
    TEST_MSG("setup failed");
    return false;
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
    for (int v = 0; v < mutt_array_size(vars); v++)
    {
      char line[64] = { 0 };
      snprintf(line, sizeof(line), "set %s -= %s", vars[v], increment[v]);
      buf_reset(err);
      enum CommandResult rc = parse_rc_line(line, err);
      if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return false;
      }

      // Check effect
      buf_reset(err);
      int grc = cs_str_string_get(NeoMutt->sub->cs, vars[v], err);
      if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", vars[v], buf_string(err));
        return false;
      }
      if (!TEST_CHECK_STR_EQ(err->data, expected[v]))
      {
        TEST_MSG("Variable not decremented %s: got = %s, expected = %s",
                 vars[v], buf_string(err), expected[v]);
        return false;
      }
    }
  }

  return true;
}

/**
 * Test that invalid syntax forms of "set" error out.
 */
static bool test_invalid_syntax(struct Buffer *err)
{
  {
    // clang-format off
    const char *template[] =
    {
      "set &&Cherry",   "set ?&Cherry",   "set &Cherry?",   "set no&Cherry",   "set inv&Cherry",   "set &Cherry = 42",
      "set &?Cherry",   "set ??Cherry",   "set ?Cherry?",   "set no?Cherry",   "set inv?Cherry",   "set ?Cherry = 42",
      "set &Cherry?",   "set ?Cherry?",   "set Cherry??",   "set noCherry?",   "set invCherry?",   "set Cherry? = 42",
      "set &noCherry",  "set ?noCherry",  "set noCherry?",  "set nonoCherry",  "set invnoCherry",  "set noCherry = 42",
      "set &invCherry", "set ?invCherry", "set invCherry?", "set noinvCherry", "set invinvCherry", "set invCherry = 42",
      "set Cherry+",    "set Cherry-",
    };
    // clang-format on

    for (int t = 0; t < mutt_array_size(template); t++)
    {
      buf_reset(err);
      enum CommandResult rc = parse_rc_line(template[t], err);
      if (!TEST_CHECK(rc == MUTT_CMD_WARNING || rc == MUTT_CMD_ERROR))
      {
        TEST_MSG("For command '%s': Expected %d or %d, but got %d; err is: '%s'",
                 template[t], MUTT_CMD_WARNING, MUTT_CMD_ERROR, rc, buf_string(err));
        return false;
      }
    }
  }

  return true;
}

/**
 * Test if paths are expanded when setting a value (set name = value):
 *
 * * mailbox: =foo, +foo
 * * command: ~/bin/foo
 * * path: ~/bin/foo
 */
static bool test_path_expanding(struct Buffer *err)
{
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
      "expanded<",
      "expanded~/bar",
      "expanded=foo",
    };
    for (int v = 0; v < mutt_array_size(pathlike); v++)
    {
      char line[64] = { 0 };
      snprintf(line, sizeof(line), "set %s = %s", pathlike[v], newvalue[v]);
      buf_reset(err);
      enum CommandResult rc = parse_rc_line(line, err);
      if (!TEST_CHECK(rc == MUTT_CMD_SUCCESS))
      {
        TEST_MSG("Expected %d, but got %d; err is: '%s'", MUTT_CMD_SUCCESS, rc,
                 buf_string(err));
        return false;
      }

      // Check effect
      buf_reset(err);
      int grc = cs_str_string_get(NeoMutt->sub->cs, pathlike[v], err);
      if (!TEST_CHECK(CSR_RESULT(grc) == CSR_SUCCESS))
      {
        TEST_MSG("Failed to get %s: %s", pathlike[v], buf_string(err));
        return false;
      }
      if (!TEST_CHECK_STR_EQ(err->data, expected[v]))
      {
        TEST_MSG("Variable not incremented %s: got = %s, expected = %s",
                 pathlike[v], buf_string(err), expected[v]);
        return false;
      }
    }
  }

  return true;
}

void test_command_set(void)
{
  if (!TEST_CHECK(cs_register_variables(NeoMutt->sub->cs, ConfigVars, DT_NO_FLAGS)))
  {
    TEST_MSG("Failed to register config variables");
    return;
  }

  commands_register(mutt_commands, 4);

  struct Buffer *err = buf_pool_get();
  TEST_CHECK(test_set(err));
  TEST_CHECK(test_reset(err));
  TEST_CHECK(test_unset(err));
  TEST_CHECK(test_toggle(err));
  TEST_CHECK(test_query(err));
  TEST_CHECK(test_increment(err));
  TEST_CHECK(test_decrement(err));
  TEST_CHECK(test_invalid_syntax(err));
  TEST_CHECK(test_path_expanding(err));
  buf_pool_release(&err);
}
