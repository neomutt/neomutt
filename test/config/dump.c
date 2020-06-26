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
#include "config.h"
#include "acutest.h"
#include "mutt/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

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
static char *VarOlive;

// clang-format off
static struct Mapping MboxTypeMap[] = {
  { "mbox",    MUTT_MBOX,    },
  { "MMDF",    MUTT_MMDF,    },
  { "MH",      MUTT_MH,      },
  { "Maildir", MUTT_MAILDIR, },
  { NULL,      0,            },
};
// clang-format on

struct EnumDef MboxTypeDef = {
  "mbox_type",
  4,
  (struct Mapping *) &MboxTypeMap,
};

// clang-format off
static struct ConfigDef Vars[] = {
  { "Apple",      DT_BOOL,                           &VarApple,      false,                       0,               NULL },
  { "Banana",     DT_BOOL,                           &VarBanana,     true,                        0,               NULL },
  { "Cherry",     DT_NUMBER,                         &VarCherry,     0,                           0,               NULL },
  { "Damson",     DT_SYNONYM,                        NULL,           IP "Cherry",                 0,               NULL },
  { "Elderberry", DT_ADDRESS,                        &VarElderberry, IP "elderberry@example.com", 0,               NULL },
  { "Fig",        DT_STRING|DT_COMMAND|DT_NOT_EMPTY, &VarFig,        IP "fig",                    0,               NULL },
  { "Guava",      DT_LONG,                           &VarGuava,      0,                           0,               NULL },
  { "Hawthorn",   DT_ENUM,                           &VarHawthorn,   2,                           IP &MboxTypeDef, NULL },
  { "Ilama",      DT_MBTABLE,                        &VarIlama,      0,                           0,               NULL },
  { "Jackfruit",  DT_PATH|DT_PATH_FILE,              &VarJackfruit,  IP "/etc/passwd",            0,               NULL },
  { "Kumquat",    DT_QUAD,                           &VarKumquat,    0,                           0,               NULL },
  { "Lemon",      DT_REGEX,                          &VarLemon,      0,                           0,               NULL },
  { "Mango",      DT_SORT,                           &VarMango,      1,                           0,               NULL },
  { "Nectarine",  DT_STRING|DT_SENSITIVE,            &VarNectarine,  IP "nectarine",              0,               NULL },
  { "Olive",      DT_STRING|DT_DEPRECATED,           &VarOlive,      IP "olive",                  0,               NULL },
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
    struct Buffer buf = mutt_buffer_make(0);
    if (!TEST_CHECK(pretty_var(NULL, &buf) == 0))
      return false;
  }

  {
    if (!TEST_CHECK(pretty_var("apple", NULL) == 0))
      return false;
  }

  {
    struct Buffer buf = mutt_buffer_make(64);
    if (!TEST_CHECK(pretty_var("apple", &buf) > 0))
    {
      mutt_buffer_dealloc(&buf);
      return false;
    }

    if (!TEST_CHECK(mutt_str_equal("\"apple\"", mutt_b2s(&buf))))
    {
      mutt_buffer_dealloc(&buf);
      return false;
    }

    mutt_buffer_dealloc(&buf);
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
    struct Buffer buf = mutt_buffer_make(0);
    if (!TEST_CHECK(escape_string(&buf, NULL) == 0))
      return false;
  }

  {
    const char *before = "apple\nbanana\rcherry\tdam\007son\\endive\"fig'grape";
    const char *after =
        "apple\\nbanana\\rcherry\\tdam\\gson\\\\endive\\\"fig'grape";
    struct Buffer buf = mutt_buffer_make(256);
    if (!TEST_CHECK(escape_string(&buf, before) > 0))
    {
      mutt_buffer_dealloc(&buf);
      return false;
    }

    if (!TEST_CHECK(mutt_str_equal(mutt_b2s(&buf), after)))
    {
      mutt_buffer_dealloc(&buf);
      return false;
    }

    mutt_buffer_dealloc(&buf);
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
  enum_init(cs);
  long_init(cs);
  mbtable_init(cs);
  number_init(cs);
  path_init(cs);
  quad_init(cs);
  path_init(cs);
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

bool test_dump_config_neo(void)
{
  // void dump_config_neo(struct ConfigSet *cs, struct HashElem *he, struct Buffer *value, struct Buffer *initial, ConfigDumpFlags flags, FILE *fp);

  {
    struct ConfigSet *cs = create_sample_data();
    if (!cs)
      return false;

    struct HashElem *he = cs_get_elem(cs, "Banana");

    struct Buffer buf_val = mutt_buffer_make(0);
    mutt_buffer_addstr(&buf_val, "yes");
    struct Buffer buf_init = mutt_buffer_make(0);
    mutt_buffer_addstr(&buf_init, "yes");

    FILE *fp = fopen("/dev/null", "w");
    if (!fp)
      return false;

    // Degenerate tests

    dump_config_neo(NULL, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_neo(NULL, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, NULL, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_neo(cs, NULL, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, he, NULL, &buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_neo(cs, he, NULL, &buf_init, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, he, &buf_val, NULL, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1, "dump_config_neo(cs, he, &buf_val, NULL, CS_DUMP_NO_FLAGS, fp)");
    dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, NULL);
    TEST_CHECK_(
        1,
        "dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, NULL)");

    // Normal tests

    dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp)");

    dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_ONLY_CHANGED, fp);
    TEST_CHECK_(1, "dump_config_neo(cs, he, &buf_val, &buf_init, "
                   "CS_DUMP_ONLY_CHANGED, fp)");

    dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_SHOW_DEFAULTS, fp);
    TEST_CHECK_(1, "dump_config_neo(cs, he, &buf_val, &buf_init, "
                   "CS_DUMP_SHOW_DEFAULTS, fp)");

    he = mutt_hash_find_elem(cs->hash, "Damson");
    dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp);
    TEST_CHECK_(
        1,
        "dump_config_neo(cs, he, &buf_val, &buf_init, CS_DUMP_NO_FLAGS, fp)");

    fclose(fp);
    mutt_buffer_dealloc(&buf_val);
    mutt_buffer_dealloc(&buf_init);
    cs_free(&cs);
  }

  return true;
}

bool test_dump_config(void)
{
  // bool dump_config(struct ConfigSet *cs, ConfigDumpFlags flags, FILE *fp);

  {
    struct ConfigSet *cs = create_sample_data();
    if (!cs)
      return false;

    FILE *fp = fopen("/dev/null", "w");
    if (!fp)
      return false;

    // Degenerate tests

    TEST_CHECK(!dump_config(NULL, CS_DUMP_NO_FLAGS, fp));
    TEST_CHECK(dump_config(cs, CS_DUMP_NO_FLAGS, NULL));

    // Normal tests

    TEST_CHECK(dump_config(cs, CS_DUMP_NO_FLAGS, fp));
    TEST_CHECK(dump_config(cs, CS_DUMP_ONLY_CHANGED | CS_DUMP_HIDE_SENSITIVE, fp));
    TEST_CHECK(dump_config(cs, CS_DUMP_HIDE_VALUE | CS_DUMP_SHOW_DEFAULTS, fp));

    struct ConfigSet *cs_bad = cs_new(30);
    TEST_CHECK(dump_config(cs_bad, CS_DUMP_NO_FLAGS, fp));

    fclose(fp);
    cs_free(&cs_bad);
    cs_free(&cs);
  }

  return true;
}

void test_config_dump(void)
{
  TEST_CHECK(test_pretty_var());
  TEST_CHECK(test_escape_string());
  TEST_CHECK(test_elem_list_sort());
  TEST_CHECK(test_get_elem_list());
  TEST_CHECK(test_dump_config_neo());
  TEST_CHECK(test_dump_config());
}
