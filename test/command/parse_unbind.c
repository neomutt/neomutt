/**
 * @file
 * Test code for parse_unbind()
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
#include "acutest.h"
#include <stddef.h>
#include <string.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "editor/lib.h"
#include "index/lib.h"
#include "key/lib.h"
#include "pager/lib.h"
#include "parse/lib.h"
#include "sidebar/lib.h"
#include "common.h"
#include "key/module_data.h"
#include "test_common.h"

// clang-format off
static const struct Command UnBind  = { "unbind",  CMD_UNBIND,  NULL};
static const struct Command UnMacro = { "unmacro", CMD_UNMACRO, NULL};
// clang-format on

static const struct CommandTest UnBindTests[] = {
  // clang-format off
  // unbind { * | <map>[,<map> ... ] } [ <key> ]
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_SUCCESS, "* *" },
  { MUTT_CMD_SUCCESS, "* d" },
  { MUTT_CMD_SUCCESS, "* missing" },
  { MUTT_CMD_SUCCESS, "index" },
  { MUTT_CMD_SUCCESS, "index,pager" },
  { MUTT_CMD_SUCCESS, "index *" },
  { MUTT_CMD_SUCCESS, "index,pager *" },
  { MUTT_CMD_SUCCESS, "index d" },
  { MUTT_CMD_SUCCESS, "index missing" },
  { MUTT_CMD_SUCCESS, "index,pager d" },
  { MUTT_CMD_SUCCESS, "index,pager missing" },
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_WARNING, "bad" },
  { MUTT_CMD_WARNING, "bad,index" },
  { MUTT_CMD_WARNING, "index,bad" },
  { MUTT_CMD_WARNING, "index d extra" },
  { MUTT_CMD_WARNING, "index,pager d extra" },
  { MUTT_CMD_WARNING, "* d extra" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static const struct CommandTest UnMacroTests[] = {
  // clang-format off
  // unmacro { * | <map>[,<map> ... ] } [ <key> ]
  { MUTT_CMD_SUCCESS, "*" },
  { MUTT_CMD_SUCCESS, "* *" },
  { MUTT_CMD_SUCCESS, "* d" },
  { MUTT_CMD_SUCCESS, "* missing" },
  { MUTT_CMD_SUCCESS, "index" },
  { MUTT_CMD_SUCCESS, "index,pager" },
  { MUTT_CMD_SUCCESS, "index *" },
  { MUTT_CMD_SUCCESS, "index,pager *" },
  { MUTT_CMD_SUCCESS, "index d" },
  { MUTT_CMD_SUCCESS, "index missing" },
  { MUTT_CMD_SUCCESS, "index,pager d" },
  { MUTT_CMD_SUCCESS, "index,pager missing" },
  { MUTT_CMD_WARNING, "" },
  { MUTT_CMD_WARNING, "bad" },
  { MUTT_CMD_WARNING, "bad,index" },
  { MUTT_CMD_WARNING, "index,bad" },
  { MUTT_CMD_WARNING, "index d extra" },
  { MUTT_CMD_WARNING, "index,pager d extra" },
  { MUTT_CMD_WARNING, "* d extra" },
  { MUTT_CMD_ERROR,   NULL },
  // clang-format on
};

static void init_menus(void)
{
  struct SubMenu *sm_generic = generic_init_keys(NeoMutt);

  editor_init_keys(NeoMutt, sm_generic);
  sidebar_init_keys(NeoMutt, sm_generic);
  index_init_keys(NeoMutt, sm_generic);
  pager_init_keys(NeoMutt, sm_generic);
}

static bool has_binding(const char *menu, int op)
{
  return km_find_func(menu_find_by_name(menu), op) != NULL;
}

static bool has_keymap(const char *menu, const char *key, int op)
{
  struct MenuDefinition *md = menu_find_by_name(menu);
  struct SubMenu **smp = ARRAY_GET(&md->submenus, 0);
  struct SubMenu *sm = *smp;
  keycode_t key_bytes[KEY_SEQ_MAX_LEN] = { 0 };
  int key_len = parse_keys(key, key_bytes, KEY_SEQ_MAX_LEN);

  struct Keymap *km = NULL;
  STAILQ_FOREACH(km, &sm->keymaps, entries)
  {
    if ((km->op == op) && (km->len == key_len) &&
        (memcmp(km->keys, key_bytes, key_len) == 0))
    {
      return true;
    }
  }

  return false;
}

static void run_unbind_command(const struct Command *cmd, struct Buffer *line,
                               const struct ParseContext *pc,
                               struct ParseError *pe, const char *command)
{
  parse_error_reset(pe);
  buf_strcpy(line, command);
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(cmd, line, pc, pe), MUTT_CMD_SUCCESS);
}

static void test_parse_unbind2(void)
{
  // enum CommandResult parse_unbind(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; UnBindTests[i].line; i++)
  {
    init_menus();

    TEST_CASE(UnBindTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnBindTests[i].line);
    buf_seek(line, 0);
    rc = parse_unbind(&UnBind, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnBindTests[i].rc);

    km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unmacro(void)
{
  // enum CommandResult parse_unbind(const struct Command *cmd, struct Buffer *line, const struct ParseContext *pc, struct ParseError *pe)

  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();
  enum CommandResult rc;

  for (int i = 0; UnMacroTests[i].line; i++)
  {
    init_menus();
    km_bind(menu_find_by_name("index"), "x", OP_MACRO, "<exit>", NULL, NULL);
    km_bind(menu_find_by_name("pager"), "x", OP_MACRO, "<exit>", NULL, NULL);
    km_bind(menu_find_by_name("index"), "y", OP_MACRO, "<exit>", NULL, NULL);

    TEST_CASE(UnMacroTests[i].line);
    parse_error_reset(pe);
    buf_strcpy(line, UnMacroTests[i].line);
    buf_seek(line, 0);
    rc = parse_unbind(&UnMacro, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, UnMacroTests[i].rc);

    km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));
  }

  const char *commands[] = {
    "index x", "* x", "index", "index *", "*", "* *",
  };
  for (size_t i = 0; i < (sizeof(commands) / sizeof(commands[0])); i++)
  {
    init_menus();
    km_bind(menu_find_by_name("index"), "x", OP_MACRO, "<exit>", NULL, NULL);
    km_bind(menu_find_by_name("pager"), "x", OP_MACRO, "<exit>", NULL, NULL);
    km_bind(menu_find_by_name("index"), "y", OP_MACRO, "<exit>", NULL, NULL);

    TEST_CASE(commands[i]);
    TEST_CHECK(has_keymap("index", "x", OP_MACRO));
    TEST_CHECK(has_keymap("pager", "x", OP_MACRO));
    TEST_CHECK(has_keymap("index", "y", OP_MACRO));
    TEST_CHECK(has_binding("index", OP_DISPLAY_MESSAGE));

    parse_error_reset(pe);
    buf_strcpy(line, commands[i]);
    buf_seek(line, 0);
    rc = parse_unbind(&UnMacro, line, pc, pe);
    TEST_CHECK_NUM_EQ(rc, MUTT_CMD_SUCCESS);

    if (mutt_str_equal(commands[i], "index x"))
    {
      TEST_CHECK(!has_keymap("index", "x", OP_MACRO));
      TEST_CHECK(has_keymap("pager", "x", OP_MACRO));
      TEST_CHECK(has_keymap("index", "y", OP_MACRO));
    }
    else if (mutt_str_equal(commands[i], "* x"))
    {
      TEST_CHECK(!has_keymap("index", "x", OP_MACRO));
      TEST_CHECK(!has_keymap("pager", "x", OP_MACRO));
      TEST_CHECK(has_keymap("index", "y", OP_MACRO));
    }
    else
    {
      TEST_CHECK(!has_keymap("index", "x", OP_MACRO));
      TEST_CHECK(!has_keymap("index", "y", OP_MACRO));
      if (mutt_str_equal(commands[i], "index") || mutt_str_equal(commands[i], "index *"))
        TEST_CHECK(has_keymap("pager", "x", OP_MACRO));
      else
        TEST_CHECK(!has_keymap("pager", "x", OP_MACRO));
    }

    TEST_CHECK(has_binding("index", OP_DISPLAY_MESSAGE));
    km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));
  }

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unbind_behaviour(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  TEST_CASE("unbind index y removes one regular binding from one menu");
  init_menus();
  km_bind(menu_find_by_name("index"), "y", OP_DISPLAY_MESSAGE, NULL, NULL, NULL);
  km_bind(menu_find_by_name("pager"), "y", OP_EXIT, NULL, NULL, NULL);
  km_bind(menu_find_by_name("index"), "x", OP_MACRO, "<exit>", NULL, NULL);
  TEST_CHECK(has_keymap("index", "y", OP_DISPLAY_MESSAGE));
  TEST_CHECK(has_keymap("pager", "y", OP_EXIT));
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  TEST_CHECK(has_binding("index", OP_SELECT_NEXT_UNDELETED_ENTRY));
  run_unbind_command(&UnBind, line, pc, pe, "index y");
  TEST_CHECK(!has_keymap("index", "y", OP_DISPLAY_MESSAGE));
  TEST_CHECK(has_keymap("pager", "y", OP_EXIT));
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  TEST_CHECK(has_binding("index", OP_SELECT_NEXT_UNDELETED_ENTRY));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind index removes all regular bindings and restores fallbacks");
  init_menus();
  km_bind(menu_find_by_name("index"), "y", OP_DISPLAY_MESSAGE, NULL, NULL, NULL);
  km_bind(menu_find_by_name("pager"), "y", OP_EXIT, NULL, NULL, NULL);
  TEST_CHECK(has_keymap("index", "y", OP_DISPLAY_MESSAGE));
  TEST_CHECK(has_keymap("pager", "y", OP_EXIT));
  TEST_CHECK(has_binding("index", OP_SELECT_NEXT_UNDELETED_ENTRY));
  run_unbind_command(&UnBind, line, pc, pe, "index");
  TEST_CHECK(!has_keymap("index", "y", OP_DISPLAY_MESSAGE));
  TEST_CHECK(has_keymap("pager", "y", OP_EXIT));
  TEST_CHECK(has_binding("index", OP_DISPLAY_MESSAGE));
  TEST_CHECK(!has_binding("index", OP_SELECT_NEXT_UNDELETED_ENTRY));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind * y removes one regular binding from all menus");
  init_menus();
  km_bind(menu_find_by_name("generic"), "y", OP_DISPLAY_HELP, NULL, NULL, NULL);
  km_bind(menu_find_by_name("index"), "y", OP_DISPLAY_MESSAGE, NULL, NULL, NULL);
  km_bind(menu_find_by_name("pager"), "y", OP_EXIT, NULL, NULL, NULL);
  km_bind(menu_find_by_name("index"), "x", OP_MACRO, "<exit>", NULL, NULL);
  km_bind(menu_find_by_name("pager"), "x", OP_MACRO, "<exit>", NULL, NULL);
  TEST_CHECK(has_keymap("generic", "y", OP_DISPLAY_HELP));
  TEST_CHECK(has_keymap("index", "y", OP_DISPLAY_MESSAGE));
  TEST_CHECK(has_keymap("pager", "y", OP_EXIT));
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  TEST_CHECK(has_keymap("pager", "x", OP_MACRO));
  run_unbind_command(&UnBind, line, pc, pe, "* y");
  TEST_CHECK(!has_keymap("generic", "y", OP_DISPLAY_HELP));
  TEST_CHECK(!has_keymap("index", "y", OP_DISPLAY_MESSAGE));
  TEST_CHECK(!has_keymap("pager", "y", OP_EXIT));
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  TEST_CHECK(has_keymap("pager", "x", OP_MACRO));
  TEST_CHECK(has_binding("generic", OP_DISPLAY_HELP));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

static void test_parse_unbind_restore_defaults(void)
{
  struct Buffer *line = buf_pool_get();
  struct ParseContext *pc = parse_context_new();
  struct ParseError *pe = parse_error_new();

  TEST_CASE("unbind * restores fallback bindings");
  init_menus();
  TEST_CHECK(has_binding("generic", OP_DISPLAY_HELP));
  TEST_CHECK(has_binding("index", OP_SELECT_NEXT_UNDELETED_ENTRY));
  parse_error_reset(pe);
  buf_strcpy(line, "*");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(has_binding("generic", OP_DISPLAY_HELP));
  TEST_CHECK(has_binding("index", OP_DISPLAY_MESSAGE));
  TEST_CHECK(!has_binding("index", OP_SELECT_NEXT_UNDELETED_ENTRY));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind * * removes fallback bindings too");
  init_menus();
  TEST_CHECK(has_binding("generic", OP_DISPLAY_HELP));
  parse_error_reset(pe);
  buf_strcpy(line, "* *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(!has_binding("generic", OP_DISPLAY_HELP));
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unmacro * does not remove bindings");
  init_menus();
  TEST_CHECK(has_binding("generic", OP_DISPLAY_HELP));
  parse_error_reset(pe);
  buf_strcpy(line, "*");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnMacro, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(has_binding("generic", OP_DISPLAY_HELP));
  TEST_CHECK(has_binding("index", OP_DISPLAY_MESSAGE));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind * * does not remove macros");
  init_menus();
  km_bind(menu_find_by_name("index"), "x", OP_MACRO, "<exit>", NULL, NULL);
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  parse_error_reset(pe);
  buf_strcpy(line, "* *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind * restores fallback bindings after unbind * *");
  init_menus();
  parse_error_reset(pe);
  buf_strcpy(line, "* *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(!has_binding("generic", OP_DISPLAY_HELP));
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  parse_error_reset(pe);
  buf_strcpy(line, "*");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(has_binding("generic", OP_DISPLAY_HELP));
  TEST_CHECK(has_binding("index", OP_DISPLAY_MESSAGE));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind index * does not restore fallback bindings for one menu");
  init_menus();
  parse_error_reset(pe);
  buf_strcpy(line, "* *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  TEST_CHECK(!has_binding("pager", OP_EXIT));
  parse_error_reset(pe);
  buf_strcpy(line, "index *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  TEST_CHECK(!has_binding("pager", OP_EXIT));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind index restores fallback bindings for one menu");
  init_menus();
  parse_error_reset(pe);
  buf_strcpy(line, "* *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  TEST_CHECK(!has_binding("pager", OP_EXIT));
  parse_error_reset(pe);
  buf_strcpy(line, "index");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(has_binding("index", OP_DISPLAY_MESSAGE));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unmacro index * does not restore fallback bindings");
  init_menus();
  km_bind(menu_find_by_name("index"), "x", OP_MACRO, "<exit>", NULL, NULL);
  parse_error_reset(pe);
  buf_strcpy(line, "* *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  parse_error_reset(pe);
  buf_strcpy(line, "index *");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnMacro, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(!has_keymap("index", "x", OP_MACRO));
  TEST_CHECK(!has_binding("index", OP_DISPLAY_MESSAGE));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  TEST_CASE("unbind index x does not remove macro x");
  init_menus();
  km_bind(menu_find_by_name("index"), "x", OP_MACRO, "<exit>", NULL, NULL);
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  parse_error_reset(pe);
  buf_strcpy(line, "index x");
  buf_seek(line, 0);
  TEST_CHECK_NUM_EQ(parse_unbind(&UnBind, line, pc, pe), MUTT_CMD_SUCCESS);
  TEST_CHECK(has_keymap("index", "x", OP_MACRO));
  km_cleanup(neomutt_get_module_data(NeoMutt, MODULE_ID_KEY));

  parse_context_free(&pc);
  parse_error_free(&pe);
  buf_pool_release(&line);
}

void test_parse_unbind(void)
{
  test_parse_unbind2();
  test_parse_unbind_behaviour();
  test_parse_unbind_restore_defaults();
  test_parse_unmacro();
}
