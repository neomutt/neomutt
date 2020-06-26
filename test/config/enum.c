/**
 * @file
 * Test code for the Enum object
 *
 * @authors
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

static unsigned char VarApple;
static unsigned char VarBanana;
static unsigned char VarCherry;
static unsigned char VarDamson;
static unsigned char VarElderberry;
static unsigned char VarFig;
static unsigned char VarGuava;
static unsigned char VarHawthorn;
static unsigned char VarIlama;
static unsigned char VarJackfruit;
static unsigned char VarKumquat;
static unsigned char VarLemon;
static unsigned char VarMango;
static unsigned char VarNectarine;
static unsigned char VarOlive;

// clang-format off
enum AnimalType
{
  ANIMAL_ANTELOPE  =  1,
  ANIMAL_BADGER    =  2,
  ANIMAL_CASSOWARY =  3,
  ANIMAL_DINGO     = 40,
  ANIMAL_ECHIDNA   = 41,
  ANIMAL_FROG      = 42,
};

static struct Mapping AnimalMap[] = {
  { "Antelope",  ANIMAL_ANTELOPE,  },
  { "Badger",    ANIMAL_BADGER,    },
  { "Cassowary", ANIMAL_CASSOWARY, },
  { "Dingo",     ANIMAL_DINGO,     },
  { "Echidna",   ANIMAL_ECHIDNA,   },
  { "Frog",      ANIMAL_FROG,      },
  // Alternatives
  { "bird",      ANIMAL_CASSOWARY, },
  { "amphibian", ANIMAL_FROG,      },
  { "carnivore", ANIMAL_BADGER,    },
  { "herbivore", ANIMAL_ANTELOPE,  },
  { NULL,        0,                },
};

struct EnumDef AnimalDef = {
  "animal",
  5,
  (struct Mapping *) &AnimalMap,
};

static struct ConfigDef Vars[] = {
  { "Apple",      DT_ENUM, &VarApple,      ANIMAL_DINGO,    IP &AnimalDef, NULL              }, /* test_initial_values */
  { "Banana",     DT_ENUM, &VarBanana,     ANIMAL_BADGER,   IP &AnimalDef, NULL              },
  { "Cherry",     DT_ENUM, &VarCherry,     ANIMAL_FROG,     IP &AnimalDef, NULL              },
  { "Damson",     DT_ENUM, &VarDamson,     ANIMAL_ANTELOPE, IP &AnimalDef, NULL              }, /* test_string_set */
  { "Elderberry", DT_ENUM, &VarElderberry, ANIMAL_ANTELOPE, 0,             NULL              }, /* broken */
  { "Fig",        DT_ENUM, &VarFig,        ANIMAL_ANTELOPE, IP &AnimalDef, NULL              }, /* test_string_get */
  { "Guava",      DT_ENUM, &VarGuava,      ANIMAL_ANTELOPE, IP &AnimalDef, NULL              }, /* test_native_set */
  { "Hawthorn",   DT_ENUM, &VarHawthorn,   ANIMAL_ANTELOPE, IP &AnimalDef, NULL              },
  { "Ilama",      DT_ENUM, &VarIlama,      ANIMAL_ANTELOPE, IP &AnimalDef, NULL              }, /* test_native_get */
  { "Jackfruit",  DT_ENUM, &VarJackfruit,  ANIMAL_ANTELOPE, IP &AnimalDef, NULL              }, /* test_reset */
  { "Kumquat",    DT_ENUM, &VarKumquat,    ANIMAL_ANTELOPE, IP &AnimalDef, validator_fail    },
  { "Lemon",      DT_ENUM, &VarLemon,      ANIMAL_ANTELOPE, IP &AnimalDef, validator_succeed }, /* test_validator */
  { "Mango",      DT_ENUM, &VarMango,      ANIMAL_ANTELOPE, IP &AnimalDef, validator_warn    },
  { "Nectarine",  DT_ENUM, &VarNectarine,  ANIMAL_ANTELOPE, IP &AnimalDef, validator_fail    },
  { "Olive",      DT_ENUM, &VarOlive,      ANIMAL_ANTELOPE, IP &AnimalDef, NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Banana = %d\n", VarBanana);

  if (!TEST_CHECK(VarApple == ANIMAL_DINGO))
  {
    TEST_MSG("Expected: %d\n", ANIMAL_DINGO);
    TEST_MSG("Actual  : %d\n", VarApple);
  }

  if (!TEST_CHECK(VarBanana == ANIMAL_BADGER))
  {
    TEST_MSG("Expected: %d\n", ANIMAL_BADGER);
    TEST_MSG("Actual  : %d\n", VarBanana);
  }

  cs_str_string_set(cs, "Apple", "Cassowary", err);
  cs_str_string_set(cs, "Banana", "herbivore", err);

  struct Buffer value;
  mutt_buffer_init(&value);
  value.dsize = 256;
  value.data = mutt_mem_calloc(1, value.dsize);

  int rc;

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Apple", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, "Dingo")))
  {
    TEST_MSG("Apple's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Apple = %d\n", VarApple);
  TEST_MSG("Apple's initial value is '%s'\n", value.data);

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Banana", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(value.data, "Badger")))
  {
    TEST_MSG("Banana's initial value is wrong: '%s'\n", value.data);
    FREE(&value.data);
    return false;
  }
  TEST_MSG("Banana = %d\n", VarBanana);
  TEST_MSG("Banana's initial value is '%s'\n", NONULL(value.data));

  mutt_buffer_reset(&value);
  rc = cs_str_initial_set(cs, "Cherry", "bird", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  mutt_buffer_reset(&value);
  rc = cs_str_initial_get(cs, "Cherry", &value);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", value.data);
    FREE(&value.data);
    return false;
  }

  TEST_MSG("Cherry = %d\n", VarCherry);
  TEST_MSG("Cherry's initial value is %s\n", value.data);

  FREE(&value.data);
  log_line(__func__);
  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *valid[] = { "Antelope", "ECHIDNA", "herbivore", "BIRD" };
  int numbers[] = { 1, 41, 1, 3 };
  const char *invalid[] = { "Frogs", "", NULL };
  const char *name = "Damson";

  int rc;
  for (unsigned int i = 0; i < mutt_array_size(valid); i++)
  {
    VarDamson = ANIMAL_CASSOWARY;

    TEST_MSG("Setting %s to %s\n", name, valid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, valid[i], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("%s\n", err->data);
      return false;
    }

    if (rc & CSR_SUC_NO_CHANGE)
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      continue;
    }

    if (!TEST_CHECK(VarDamson == numbers[i]))
    {
      TEST_MSG("Value of %s wasn't changed\n", name);
      return false;
    }
    TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, valid[i]);
    short_line();
  }

  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    TEST_MSG("Setting %s to %s\n", name, NONULL(invalid[i]));
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
      TEST_MSG("%s = %d, set by '%s'\n", name, VarDamson, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
    short_line();
  }

  name = "Elderberry";
  const char *value2 = "Dingo";
  short_line();
  TEST_MSG("Setting %s to '%s'\n", name, value2);
  rc = cs_str_string_set(cs, name, value2, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Fig";

  VarFig = ANIMAL_ECHIDNA;
  mutt_buffer_reset(err);
  int rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarFig, err->data);

  VarFig = ANIMAL_DINGO;
  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %d, %s\n", name, VarFig, err->data);

  log_line(__func__);
  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Guava";
  unsigned char value = ANIMAL_CASSOWARY;

  TEST_MSG("Setting %s to %d\n", name, value);
  VarGuava = 0;
  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, value, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarGuava == value))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("%s = %d, set to '%d'\n", name, VarGuava, value);

  short_line();
  TEST_MSG("Setting %s to %d\n", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK((rc & CSR_SUC_NO_CHANGE) != 0))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  name = "Hawthorn";
  value = -42;
  short_line();
  TEST_MSG("Setting %s to %d\n", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  int invalid[] = { -1, 256 };
  for (unsigned int i = 0; i < mutt_array_size(invalid); i++)
  {
    short_line();
    VarGuava = ANIMAL_CASSOWARY;
    TEST_MSG("Setting %s to %d\n", name, invalid[i]);
    mutt_buffer_reset(err);
    rc = cs_str_native_set(cs, name, invalid[i], err);
    if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
    {
      TEST_MSG("Expected error: %s\n", err->data);
    }
    else
    {
      TEST_MSG("%s = %d, set by '%d'\n", name, VarGuava, invalid[i]);
      TEST_MSG("This test should have failed\n");
      return false;
    }
  }

  name = "Elderberry";
  value = ANIMAL_ANTELOPE;
  short_line();
  TEST_MSG("Setting %s to %d\n", name, value);
  rc = cs_str_native_set(cs, name, value, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("This test should have failed\n");
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Ilama";

  VarIlama = 253;
  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  if (!TEST_CHECK(value != INT_MIN))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }
  TEST_MSG("%s = %ld\n", name, value);

  log_line(__func__);
  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Jackfruit";
  VarJackfruit = 253;
  mutt_buffer_reset(err);

  TEST_MSG("%s = %d\n", name, VarJackfruit);
  int rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarJackfruit != 253))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarJackfruit);

  short_line();
  name = "Kumquat";
  mutt_buffer_reset(err);

  TEST_MSG("Initial: %s = %d\n", name, VarKumquat);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "Dingo", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  TEST_MSG("Set: %s = %d\n", name, VarKumquat);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(VarKumquat == ANIMAL_DINGO))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = %d\n", name, VarKumquat);

  short_line();
  name = "Jackfruit";
  VarJackfruit = ANIMAL_ANTELOPE;
  mutt_buffer_reset(err);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Lemon";
  VarLemon = ANIMAL_ANTELOPE;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "Dingo", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarLemon);
  short_line();

  VarLemon = 253;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, ANIMAL_ECHIDNA, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarLemon);
  short_line();

  name = "Mango";
  VarMango = 123;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "bird", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarMango);
  short_line();

  VarMango = 253;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, ANIMAL_DINGO, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarMango);
  short_line();

  name = "Nectarine";
  VarNectarine = 123;
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "Cassowary", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("String: %s = %d\n", name, VarNectarine);
  short_line();

  VarNectarine = 253;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, ANIMAL_CASSOWARY, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }
  TEST_MSG("Native: %s = %d\n", name, VarNectarine);

  log_line(__func__);
  return true;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  TEST_MSG("%15s = %ld\n", parent, pval);
  TEST_MSG("%15s = %ld\n", child, cval);
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  bool result = false;

  const char *account = "fruit";
  const char *parent = "Olive";
  char child[128];
  snprintf(child, sizeof(child), "%s:%s", account, parent);

  struct ConfigSubset *sub = cs_subset_new(NULL, NULL, NeoMutt->notify);
  sub->cs = cs;
  struct Account *a = account_new(account, sub);

  struct HashElem *he = cs_subset_create_inheritance(a->sub, parent);
  if (!he)
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }

  // set parent
  VarOlive = ANIMAL_BADGER;
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "Dingo", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "Cassowary", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);
  short_line();

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  log_line(__func__);
  result = true;
ti_out:
  account_free(&a);
  cs_subset_free(&sub);
  return result;
}

void test_config_enum(void)
{
  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, 256);
  err.dsize = 256;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  enum_init(cs);
  if (!cs_register_variables(cs, Vars, 0))
    return;

  notify_observer_add(NeoMutt->notify, log_observer, 0);

  set_list(cs);

  TEST_CHECK(test_initial_values(cs, &err));
  TEST_CHECK(test_string_set(cs, &err));
  TEST_CHECK(test_string_get(cs, &err));
  TEST_CHECK(test_native_set(cs, &err));
  TEST_CHECK(test_native_get(cs, &err));
  TEST_CHECK(test_reset(cs, &err));
  TEST_CHECK(test_validator(cs, &err));
  TEST_CHECK(test_inherit(cs, &err));

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  FREE(&err.data);
  log_line(__func__);
}
