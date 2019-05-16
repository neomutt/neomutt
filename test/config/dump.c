/**
 * @file
 * Test code for the Config Dump functions
 *
 * @authors
 * Copyright (C) 2017-2018 Richard Russon <rich@flatcap.org>
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
#include "acutest.h"
#include "config.h"
#include "mutt/mutt.h"
#include "config/common.h"
#include "config/lib.h"

static bool VarApple;
static bool VarBanana;
static short VarCherry;
static struct Address *VarElderberry;
static char *VarFig;
static long VarGuava;
static short VarHawthorn;
static struct MbTable *VarIlama;
static char *VarJackfruit;
static char VarKumquat;
static struct Regex *VarLemon;
static short VarMango;
static char *VarNectarine;

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_BOOL,                 0, &VarApple,      false,                       NULL },
  { "Banana",     DT_BOOL,                 0, &VarBanana,     true,                        NULL },
  { "Cherry",     DT_NUMBER,               0, &VarCherry,     0,                           NULL },
  { "Damson",     DT_SYNONYM,              0, NULL,           IP "Cherry",                 NULL },
  { "Elderberry", DT_ADDRESS,              0, &VarElderberry, IP "elderberry@example.com", NULL },
  { "Fig",        DT_COMMAND|DT_NOT_EMPTY, 0, &VarFig,        IP "fig",                    NULL },
  { "Guava",      DT_LONG,                 0, &VarGuava,      0,                           NULL },
  { "Hawthorn",   DT_MAGIC,                0, &VarHawthorn,   1,                           NULL },
  { "Ilama",      DT_MBTABLE,              0, &VarIlama,      0,                           NULL },
  { "Jackfruit",  DT_PATH,                 0, &VarJackfruit,  IP "/etc/passwd",            NULL },
  { "Kumquat",    DT_QUAD,                 0, &VarKumquat,    0,                           NULL },
  { "Lemon",      DT_REGEX,                0, &VarLemon,      0,                           NULL },
  { "Mango",      DT_SORT,                 0, &VarMango,      1,                           NULL },
  { "Nectarine",  DT_STRING,     F_SENSITIVE, &VarNectarine,  IP "nectarine",              NULL },
  { NULL },
};
// clang-format on

void mutt_pretty_mailbox(char *buf, size_t buflen)
{
}

bool test_pretty_var(void)
{
  // size_t pretty_var(const char *str, struct Buffer *buf);

  {
    struct Buffer buf = { 0 };
    if (!TEST_CHECK(pretty_var(NULL, &buf) == 0))
      return false;
  }

  {
    if (!TEST_CHECK(pretty_var("apple", NULL) == 0))
      return false;
  }

  {
    struct Buffer *buf = mutt_buffer_alloc(64);
    if (!TEST_CHECK(pretty_var("apple", buf) > 0))
    {
      mutt_buffer_free(&buf);
      return false;
    }

    if (!TEST_CHECK(mutt_str_strcmp("\"apple\"", mutt_b2s(buf)) == 0))
    {
      mutt_buffer_free(&buf);
      return false;
    }

    mutt_buffer_free(&buf);
  }

  return true;
}

bool test_escape_string(void)
{
  // size_t escape_string(struct Buffer *buf, const char *src);

  {
    if (!TEST_CHECK(escape_string(NULL, "apple") == 0))
      return false;
  }

  {
    struct Buffer buf = { 0 };
    if (!TEST_CHECK(escape_string(&buf, NULL) == 0))
      return false;
  }

  {
    const char *before = "apple\nbanana\rcherry\tdamson\\endive\"fig'grape";
    const char *after =
        "apple\\nbanana\\rcherry\\tdamson\\\\endive\\\"fig'grape";
    struct Buffer *buf = mutt_buffer_alloc(256);
    if (!TEST_CHECK(escape_string(buf, before) > 0))
    {
      mutt_buffer_free(&buf);
      return false;
    }

    if (!TEST_CHECK(mutt_str_strcmp(mutt_b2s(buf), after) == 0))
    {
      mutt_buffer_free(&buf);
      return false;
    }

    mutt_buffer_free(&buf);
  }

  return true;
}

bool test_elem_list_sort(void)
{
  // int elem_list_sort(const void *a, const void *b);

  {
    struct HashElem he = { 0 };
    if (!TEST_CHECK(elem_list_sort(NULL, &he) == 0))
      return false;
  }

  {
    struct HashElem he = { 0 };
    if (!TEST_CHECK(elem_list_sort(&he, NULL) == 0))
      return false;
  }

  return true;
}

struct ConfigSet *create_sample_data(void)
{
  struct ConfigSet *cs = cs_new(30);
  if (!cs)
    return NULL;

  address_init(cs);
  bool_init(cs);
  command_init(cs);
  long_init(cs);
  magic_init(cs);
  mbtable_init(cs);
  number_init(cs);
  path_init(cs);
  quad_init(cs);
  regex_init(cs);
  sort_init(cs);
  string_init(cs);

  if (!cs_register_variables(cs, Vars, 0))
    return NULL;

  return cs;
}

bool test_get_elem_list(void)
{
  // struct HashElem **get_elem_list(struct ConfigSet *cs);

  {
    if (!TEST_CHECK(get_elem_list(NULL) == NULL))
      return false;
  }

  {
    struct ConfigSet *cs = create_sample_data();
    if (!cs)
      return false;

    struct HashElem **list = NULL;
    if (!TEST_CHECK((list = get_elem_list(cs)) != NULL))
    {
      cs_free(&cs);
      return false;
    }

    FREE(&list);
    cs_free(&cs);
  }

  return true;
}

bool test_dump_config_mutt(void)
{
  // void dump_config_mutt(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value, struct Buffer *initial, ConfigDumpFlags flags, FILE *fp);

  {
    struct ConfigSet *cs = create_sample_data();
    if (!cs)
      return false;

    struct HashElem *he = cs_get_elem(cs, "Apple");

    struct Buffer *buf_val = mutt_buffer_from("yes");
    struct Buffer *buf_init = mutt_buffer_from("initial");

    FILE *fp = fopen("/dev/null", "w");
    if (!fp)
      return false;

    // Degenerate tests

    dump_config_mutt(NULL, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_mutt(NULL, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_mutt(cs, NULL, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_mutt(cs, NULL, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_mutt(cs, he, NULL, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_mutt(cs, he, NULL, buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_mutt(cs, he, buf_val, NULL, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_mutt(cs, he, buf_val, NULL, CS_DUMP_NO_FLAGS, fp)");
    dump_config_mutt(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, NULL);
    TEST_CHECK_(
        1,
        "dump_config_mutt(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, NULL)");

    // Normal tests

    dump_config_mutt(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_mutt(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");

    mutt_buffer_reset(buf_val);
    mutt_buffer_addstr(buf_val, "no");

    dump_config_mutt(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_mutt(NULL, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");

    he = cs_get_elem(cs, "Cherry");

    dump_config_mutt(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_mutt(NULL, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");

    fclose(fp);
    mutt_buffer_free(&buf_val);
    mutt_buffer_free(&buf_init);
    cs_free(&cs);
  }

  return true;
}

bool test_dump_config_neo(void)
{
  // void dump_config_neo(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value, struct Buffer *initial, ConfigDumpFlags flags, FILE *fp);

  {
    struct ConfigSet *cs = create_sample_data();
    if (!cs)
      return false;

    struct HashElem *he = cs_get_elem(cs, "Banana");

    struct Buffer *buf_val = mutt_buffer_from("yes");
    struct Buffer *buf_init = mutt_buffer_from("yes");

    FILE *fp = fopen("/dev/null", "w");
    if (!fp)
      return false;

    // Degenerate tests

    dump_config_neo(NULL, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_neo(NULL, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, NULL, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_neo(cs, NULL, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, he, NULL, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_neo(cs, he, NULL, buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, he, buf_val, NULL, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(1,
                "dump_config_neo(cs, he, buf_val, NULL, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, NULL);
    TEST_CHECK_(
        1,
        "dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, NULL)");

    // Normal tests

    dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");

    dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_ONLY_CHANGED, fp);
    TEST_CHECK_(
        1,
        "dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_ONLY_CHANGED, fp)");

    dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_SHOW_DEFAULTS, fp);
    TEST_CHECK_(1, "dump_config_neo(cs, he, buf_val, buf_init, "
                   "CS_DUMP_SHOW_DEFAULTS, fp)");

    he = mutt_hash_find_elem(cs->hash, "Damson");
    dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_neo(cs, he, buf_val, buf_init, CS_DUMP_NO_FLAGS, fp)");

    fclose(fp);
    mutt_buffer_free(&buf_val);
    mutt_buffer_free(&buf_init);
    cs_free(&cs);
  }

  return true;
}

bool test_dump_config(void)
{
  // bool dump_config(struct ConfigSet *cs, enum CsDumpStyle style, ConfigDumpFlags flags, FILE *fp);

  {
    struct ConfigSet *cs = create_sample_data();
    if (!cs)
      return false;

    FILE *fp = fopen("/dev/null", "w");
    if (!fp)
      return false;

    // Degenerate tests

    if (!TEST_CHECK(dump_config(NULL, CS_DUMP_STYLE_NEO, CS_DUMP_NO_FLAGS, fp) == false))
      return false;
    if (!TEST_CHECK(dump_config(cs, CS_DUMP_STYLE_NEO, CS_DUMP_NO_FLAGS, NULL) == true))
      return false;

    // Normal tests

    if (!TEST_CHECK(dump_config(cs, CS_DUMP_STYLE_MUTT, CS_DUMP_NO_FLAGS, fp) == true))
      return false;
    if (!TEST_CHECK(dump_config(cs, CS_DUMP_STYLE_NEO, CS_DUMP_NO_FLAGS, fp) == true))
      return false;
    if (!TEST_CHECK(dump_config(cs, CS_DUMP_STYLE_NEO,
                                CS_DUMP_ONLY_CHANGED | CS_DUMP_HIDE_SENSITIVE, fp) == true))
      return false;
    if (!TEST_CHECK(dump_config(cs, CS_DUMP_STYLE_NEO,
                                CS_DUMP_HIDE_VALUE | CS_DUMP_SHOW_DEFAULTS, fp) == true))
      return false;

    struct ConfigSet *cs_bad = cs_new(30);
    if (!TEST_CHECK(dump_config(cs_bad, CS_DUMP_STYLE_NEO, CS_DUMP_NO_FLAGS, fp) == true))
      return false;

    fclose(fp);
    cs_free(&cs_bad);
    cs_free(&cs);
  }

  return true;
}

void config_dump(void)
{
  if (!test_pretty_var())
    return;
  if (!test_escape_string())
    return;
  if (!test_elem_list_sort())
    return;
  if (!test_get_elem_list())
    return;
  if (!test_dump_config_mutt())
    return;
  if (!test_dump_config_neo())
    return;
  if (!test_dump_config())
    return;
}
