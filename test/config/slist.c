/**
 * @file
 * Test code for the Slist object
 *
 * @authors
 * Copyright (C) 2018-2019 Richard Russon <rich@flatcap.org>
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
#include <string.h>
#include "mutt/lib.h"
#include "config/common.h"
#include "config/lib.h"
#include "core/lib.h"

static struct Slist *VarApple;
static struct Slist *VarBanana;
static struct Slist *VarCherry;
static struct Slist *VarDamson;
static struct Slist *VarElderberry;
static struct Slist *VarFig;
static struct Slist *VarGuava;
static struct Slist *VarHawthorn;
static struct Slist *VarIlama;
static struct Slist *VarJackfruit;
// static struct Slist *VarKumquat;
static struct Slist *VarLemon;
static struct Slist *VarMango;
static struct Slist *VarNectarine;
static struct Slist *VarOlive;
static struct Slist *VarPapaya;
static struct Slist *VarQuince;
#if 0
static struct Slist *VarRaspberry;
static struct Slist *VarStrawberry;
#endif

// clang-format off
static struct ConfigDef VarsColon[] = {
  { "Apple",      DT_SLIST|SLIST_SEP_COLON, &VarApple,      IP "apple",               0, NULL }, /* test_initial_values */
  { "Banana",     DT_SLIST|SLIST_SEP_COLON, &VarBanana,     IP "apple:banana",        0, NULL },
  { "Cherry",     DT_SLIST|SLIST_SEP_COLON, &VarCherry,     IP "apple:banana:cherry", 0, NULL },
  { "Damson",     DT_SLIST|SLIST_SEP_COLON, &VarDamson,     IP "apple:banana",        0, NULL }, /* test_string_set */
  { "Elderberry", DT_SLIST|SLIST_SEP_COLON, &VarElderberry, 0,                        0, NULL },
  { "Fig",        DT_SLIST|SLIST_SEP_COLON, &VarFig,        IP ":apple",              0, NULL }, /* test_string_get */
  { "Guava",      DT_SLIST|SLIST_SEP_COLON, &VarGuava,      IP "apple::cherry",       0, NULL },
  { "Hawthorn",   DT_SLIST|SLIST_SEP_COLON, &VarHawthorn,   IP "apple:",              0, NULL },
  { NULL },
};

static struct ConfigDef VarsComma[] = {
  { "Apple",      DT_SLIST|SLIST_SEP_COMMA, &VarApple,      IP "apple",               0, NULL }, /* test_initial_values */
  { "Banana",     DT_SLIST|SLIST_SEP_COMMA, &VarBanana,     IP "apple,banana",        0, NULL },
  { "Cherry",     DT_SLIST|SLIST_SEP_COMMA, &VarCherry,     IP "apple,banana,cherry", 0, NULL },
  { "Damson",     DT_SLIST|SLIST_SEP_COLON, &VarDamson,     IP "apple,banana",        0, NULL }, /* test_string_set */
  { "Elderberry", DT_SLIST|SLIST_SEP_COLON, &VarElderberry, 0,                        0, NULL },
  { "Fig",        DT_SLIST|SLIST_SEP_COLON, &VarFig,        IP ",apple",              0, NULL }, /* test_string_get */
  { "Guava",      DT_SLIST|SLIST_SEP_COLON, &VarGuava,      IP "apple,,cherry",       0, NULL },
  { "Hawthorn",   DT_SLIST|SLIST_SEP_COLON, &VarHawthorn,   IP "apple,",              0, NULL },
  { NULL },
};

static struct ConfigDef VarsSpace[] = {
  { "Apple",      DT_SLIST|SLIST_SEP_SPACE, &VarApple,      IP "apple",               0, NULL }, /* test_initial_values */
  { "Banana",     DT_SLIST|SLIST_SEP_SPACE, &VarBanana,     IP "apple banana",        0, NULL },
  { "Cherry",     DT_SLIST|SLIST_SEP_SPACE, &VarCherry,     IP "apple banana cherry", 0, NULL },
  { "Damson",     DT_SLIST|SLIST_SEP_COLON, &VarDamson,     IP "apple banana",        0, NULL }, /* test_string_set */
  { "Elderberry", DT_SLIST|SLIST_SEP_COLON, &VarElderberry, 0,                        0, NULL },
  { "Fig",        DT_SLIST|SLIST_SEP_COLON, &VarFig,        IP " apple",              0, NULL }, /* test_string_get */
  { "Guava",      DT_SLIST|SLIST_SEP_COLON, &VarGuava,      IP "apple  cherry",       0, NULL },
  { "Hawthorn",   DT_SLIST|SLIST_SEP_COLON, &VarHawthorn,   IP "apple ",              0, NULL },
  { NULL },
};

static struct ConfigDef VarsOther[] = {
  { "Ilama",      DT_SLIST|SLIST_SEP_COLON, &VarIlama,      0,                        0, NULL              }, /* test_native_set */
  { "Jackfruit",  DT_SLIST|SLIST_SEP_COLON, &VarJackfruit,  IP "apple:banana:cherry", 0, NULL              }, /* test_native_get */
  { "Lemon",      DT_SLIST|SLIST_SEP_COLON, &VarLemon,      IP "lemon",               0, NULL              }, /* test_reset */
  { "Mango",      DT_SLIST|SLIST_SEP_COLON, &VarMango,      IP "mango",               0, validator_fail    },
  { "Nectarine",  DT_SLIST|SLIST_SEP_COLON, &VarNectarine,  IP "nectarine",           0, validator_succeed }, /* test_validator */
  { "Olive",      DT_SLIST|SLIST_SEP_COLON, &VarOlive,      IP "olive",               0, validator_warn    },
  { "Papaya",     DT_SLIST|SLIST_SEP_COLON, &VarPapaya,     IP "papaya",              0, validator_fail    },
  { "Quince",     DT_SLIST|SLIST_SEP_COLON, &VarQuince,     0,                        0, NULL              }, /* test_inherit */
  { NULL },
};
// clang-format on

static void slist_flags(unsigned int flags, struct Buffer *buf)
{
  if (!buf)
    return;

  switch (flags & SLIST_SEP_MASK)
  {
    case SLIST_SEP_SPACE:
      mutt_buffer_addstr(buf, "SPACE");
      break;
    case SLIST_SEP_COMMA:
      mutt_buffer_addstr(buf, "COMMA");
      break;
    case SLIST_SEP_COLON:
      mutt_buffer_addstr(buf, "COLON");
      break;
    default:
      mutt_buffer_addstr(buf, "UNKNOWN");
      return;
  }

  if (flags & SLIST_ALLOW_DUPES)
    mutt_buffer_addstr(buf, " | SLIST_ALLOW_DUPES");
  if (flags & SLIST_ALLOW_EMPTY)
    mutt_buffer_addstr(buf, " | SLIST_ALLOW_EMPTY");
  if (flags & SLIST_CASE_SENSITIVE)
    mutt_buffer_addstr(buf, " | SLIST_CASE_SENSITIVE");
}

static void slist_dump(const struct Slist *list, struct Buffer *buf)
{
  if (!list || !buf)
    return;

  mutt_buffer_printf(buf, "[%ld] ", list->count);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &list->head, entries)
  {
    if (np->data)
      mutt_buffer_add_printf(buf, "'%s'", np->data);
    else
      mutt_buffer_addstr(buf, "NULL");
    if (STAILQ_NEXT(np, entries))
      mutt_buffer_addstr(buf, ",");
  }
  TEST_MSG("%s\n", mutt_b2s(buf));
  mutt_buffer_reset(buf);
}

static bool test_slist_parse(struct Buffer *err)
{
  mutt_buffer_reset(err);

  static const char *init[] = {
    NULL,
    "",
    "apple",
    "apple:banana",
    "apple:banana:cherry",
    ":apple",
    "banana:",
    ":",
    "::",
    "apple:banana:apple",
    "apple::banana",
  };

  unsigned int flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
  slist_flags(flags, err);
  TEST_MSG("Flags: %s", mutt_b2s(err));
  TEST_MSG("\n");
  mutt_buffer_reset(err);

  struct Slist *list = NULL;
  for (size_t i = 0; i < mutt_array_size(init); i++)
  {
    TEST_MSG(">>%s<<\n", init[i] ? init[i] : "NULL");
    list = slist_parse(init[i], flags);
    slist_dump(list, err);
    slist_free(&list);
  }

  return true;
}

static bool test_slist_add_string(struct Buffer *err)
{
  {
    struct Slist *list = slist_parse(NULL, SLIST_ALLOW_EMPTY);
    slist_dump(list, err);

    slist_add_string(list, NULL);
    slist_dump(list, err);

    slist_empty(&list);
    slist_add_string(list, "");
    slist_dump(list, err);

    slist_empty(&list);
    slist_add_string(list, "apple");
    slist_dump(list, err);
    slist_add_string(list, "banana");
    slist_dump(list, err);
    slist_add_string(list, "apple");
    slist_dump(list, err);

    slist_free(&list);
  }

  {
    struct Slist *list = slist_parse("apple", 0);
    slist_add_string(NULL, "apple");
    slist_add_string(list, NULL);
    slist_free(&list);
  }

  return true;
}

static bool test_slist_remove_string(struct Buffer *err)
{
  {
    mutt_buffer_reset(err);

    unsigned int flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
    struct Slist *list = slist_parse("apple:banana::cherry", flags);
    slist_dump(list, err);

    slist_remove_string(list, NULL);
    slist_dump(list, err);

    slist_remove_string(list, "apple");
    slist_dump(list, err);

    slist_remove_string(list, "damson");
    slist_dump(list, err);

    slist_free(&list);
  }

  {
    struct Slist *list = slist_parse("apple:banana::cherry", SLIST_SEP_COLON);
    TEST_CHECK(slist_remove_string(NULL, "apple") == NULL);
    TEST_CHECK(slist_remove_string(list, NULL) == list);
    slist_free(&list);
  }

  {
    struct Slist *list = slist_parse("apple:ba\\:nana::cherry", SLIST_SEP_COLON);
    slist_dump(list, err);
    TEST_CHECK(slist_empty(&list) == NULL);
    slist_free(&list);
  }

  return true;
}

static bool test_slist_is_member(struct Buffer *err)
{
  {
    mutt_buffer_reset(err);

    TEST_CHECK(slist_is_member(NULL, "apple") == false);

    unsigned int flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
    struct Slist *list = slist_parse("apple:banana::cherry", flags);
    slist_dump(list, err);

    static const char *values[] = { "apple", "", "damson", NULL };

    for (size_t i = 0; i < mutt_array_size(values); i++)
    {
      TEST_MSG("member '%s' : %s\n", values[i],
               slist_is_member(list, values[i]) ? "yes" : "no");
    }

    slist_free(&list);
  }

  {
    struct Slist *list = slist_parse("apple", 0);
    TEST_CHECK(slist_is_member(list, NULL) == false);
    slist_free(&list);
  }
  return true;
}

static bool test_slist_add_list(struct Buffer *err)
{
  mutt_buffer_reset(err);

  unsigned int flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;

  struct Slist *list1 = slist_parse("apple:banana::cherry", flags);
  slist_dump(list1, err);

  struct Slist *list2 = slist_parse("damson::apple:apple", flags);
  slist_dump(list2, err);

  list1 = slist_add_list(list1, list2);
  slist_dump(list1, err);

  list1 = slist_add_list(list1, NULL);
  slist_dump(list1, err);

  slist_free(&list1);
  slist_free(&list2);

  list1 = NULL;
  slist_dump(list1, err);

  list2 = slist_parse("damson::apple:apple", flags);
  slist_dump(list2, err);

  list1 = slist_add_list(list1, list2);
  slist_dump(list1, err);

  slist_free(&list1);
  slist_free(&list2);

  return true;
}

static bool test_slist_compare(struct Buffer *err)
{
  struct Slist *list1 =
      slist_parse("apple:banana::cherry", SLIST_SEP_COLON | SLIST_ALLOW_EMPTY);
  slist_dump(list1, err);

  struct Slist *list2 =
      slist_parse("apple,banana,,cherry", SLIST_SEP_COMMA | SLIST_ALLOW_EMPTY);
  slist_dump(list2, err);

  struct Slist *list3 =
      slist_parse("apple,banana,,cherry,damson", SLIST_SEP_COMMA | SLIST_ALLOW_EMPTY);
  slist_dump(list2, err);

  bool result = true;

  if (!TEST_CHECK(slist_compare(NULL, NULL) == true))
    result = false;

  if (!TEST_CHECK(slist_compare(list1, NULL) == false))
    result = false;

  if (!TEST_CHECK(slist_compare(NULL, list2) == false))
    result = false;

  if (!TEST_CHECK(slist_compare(list1, list2) == true))
    result = false;

  if (!TEST_CHECK(slist_compare(list1, list3) == false))
    result = false;

  slist_free(&list1);
  slist_free(&list2);
  slist_free(&list3);

  return result;
}

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  static const char *values[] = { "apple", "banana", "cherry", NULL };

  slist_flags(VarApple->flags, err);
  TEST_MSG("Apple, %ld items, %s flags\n", VarApple->count, mutt_b2s(err));
  mutt_buffer_reset(err);
  if (VarApple->count != 1)
  {
    TEST_MSG("Apple should have 1 item\n");
    return false;
  }

  struct ListNode *np = NULL;
  size_t i = 0;
  STAILQ_FOREACH(np, &VarApple->head, entries)
  {
    if (!mutt_str_equal(values[i], np->data))
      return false;
    i++;
  }

  slist_flags(VarBanana->flags, err);
  TEST_MSG("Banana, %ld items, %s flags\n", VarBanana->count, mutt_b2s(err));
  mutt_buffer_reset(err);
  if (VarBanana->count != 2)
  {
    TEST_MSG("Banana should have 2 items\n");
    return false;
  }

  np = NULL;
  i = 0;
  STAILQ_FOREACH(np, &VarBanana->head, entries)
  {
    if (!mutt_str_equal(values[i], np->data))
      return false;
    i++;
  }

  slist_flags(VarCherry->flags, err);
  TEST_MSG("Cherry, %ld items, %s flags\n", VarCherry->count, mutt_b2s(err));
  mutt_buffer_reset(err);
  if (VarCherry->count != 3)
  {
    TEST_MSG("Cherry should have 3 items\n");
    return false;
  }

  np = NULL;
  i = 0;
  STAILQ_FOREACH(np, &VarCherry->head, entries)
  {
    if (!mutt_str_equal(values[i], np->data))
      return false;
    i++;
  }

  const char *name = "Cherry";
  const char *value = "raspberry";
  int rc = cs_str_initial_set(cs, name, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  value = "strawberry";
  rc = cs_str_initial_set(cs, name, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  name = "Elderberry";
  mutt_buffer_reset(err);
  rc = cs_str_initial_get(cs, name, err);
  if (!TEST_CHECK(rc == (CSR_SUCCESS | CSR_SUC_EMPTY)))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  return true;
}

static bool test_string_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  int rc;

  mutt_buffer_reset(err);
  char *name = "Damson";
  rc = cs_str_string_set(cs, name, "pig:quail:rhino", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  mutt_buffer_reset(err);
  name = "Damson";
  rc = cs_str_string_set(cs, name, "", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  mutt_buffer_reset(err);
  name = "Damson";
  rc = cs_str_string_set(cs, name, NULL, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  mutt_buffer_reset(err);
  name = "Elderberry";
  rc = cs_str_string_set(cs, name, "pig:quail:rhino", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct Buffer initial;
  mutt_buffer_init(&initial);
  initial.data = mutt_mem_calloc(1, 256);
  initial.dsize = 256;

  mutt_buffer_reset(err);
  mutt_buffer_reset(&initial);
  char *name = "Fig";

  int rc = cs_str_initial_get(cs, name, &initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!mutt_str_equal(initial.data, err->data))
  {
    TEST_MSG("Differ: %s '%s' '%s'\n", name, initial.data, err->data);
    return false;
  }
  TEST_MSG("Match: %s '%s' '%s'\n", name, initial.data, err->data);

  mutt_buffer_reset(err);
  mutt_buffer_reset(&initial);
  name = "Guava";

  rc = cs_str_initial_get(cs, name, &initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!mutt_str_equal(initial.data, err->data))
  {
    TEST_MSG("Differ: %s '%s' '%s'\n", name, initial.data, err->data);
    return false;
  }
  TEST_MSG("Match: %s '%s' '%s'\n", name, initial.data, err->data);

  mutt_buffer_reset(err);
  mutt_buffer_reset(&initial);
  name = "Hawthorn";

  rc = cs_str_initial_get(cs, name, &initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!mutt_str_equal(initial.data, err->data))
  {
    TEST_MSG("Differ: %s '%s' '%s'\n", name, initial.data, err->data);
    return false;
  }
  TEST_MSG("Match: %s '%s' '%s'\n", name, initial.data, err->data);

  FREE(&initial.data);
  return true;
}

static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  const char *init = "apple:banana::cherry";
  const char *name = "Ilama";
  struct Slist *list = slist_parse(init, SLIST_SEP_COLON);

  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) list, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  if (!TEST_CHECK(strcmp(mutt_b2s(err), init) == 0))
    return false;

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) NULL, err);
  if (!TEST_CHECK(rc == (CSR_SUCCESS | CSR_SUC_EMPTY)))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  slist_free(&list);
  return true;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Jackfruit";

  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct Slist *sl = (struct Slist *) value;

  if (!TEST_CHECK(VarJackfruit == sl))
  {
    TEST_MSG("Get failed: %s\n", err->data);
    return false;
  }

  return true;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  const char *name = "Lemon";

  mutt_buffer_reset(err);

  char *item = STAILQ_FIRST(&VarLemon->head)->data;
  TEST_MSG("Initial: %s = '%s'\n", name, item);
  int rc = cs_str_string_set(cs, name, "apple", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  item = STAILQ_FIRST(&VarLemon->head)->data;
  TEST_MSG("Set: %s = '%s'\n", name, item);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
    return false;
  }

  item = STAILQ_FIRST(&VarLemon->head)->data;
  if (!TEST_CHECK(mutt_str_equal(item, "lemon")))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, item);

  name = "Mango";
  mutt_buffer_reset(err);

  item = STAILQ_FIRST(&VarMango->head)->data;
  TEST_MSG("Initial: %s = '%s'\n", name, item);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "banana", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  item = STAILQ_FIRST(&VarMango->head)->data;
  TEST_MSG("Set: %s = '%s'\n", name, item);
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

  item = STAILQ_FIRST(&VarMango->head)->data;
  if (!TEST_CHECK(mutt_str_equal(item, "banana")))
  {
    TEST_MSG("Value of %s changed\n", name);
    return false;
  }

  item = STAILQ_FIRST(&VarMango->head)->data;
  TEST_MSG("Reset: %s = '%s'\n", name, item);

  log_line(__func__);
  return true;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  char *item = NULL;
  struct Slist *list = slist_parse("apple", SLIST_SEP_COMMA);
  bool result = false;

  const char *name = "Nectarine";
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "banana", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  item = STAILQ_FIRST(&VarNectarine->head)->data;
  TEST_MSG("Address: %s = %s\n", name, item);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP list, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  item = STAILQ_FIRST(&VarNectarine->head)->data;
  TEST_MSG("Native: %s = %s\n", name, item);

  name = "Olive";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "cherry", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  item = STAILQ_FIRST(&VarOlive->head)->data;
  TEST_MSG("Address: %s = %s\n", name, item);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP list, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  item = STAILQ_FIRST(&VarOlive->head)->data;
  TEST_MSG("Native: %s = %s\n", name, item);

  name = "Papaya";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "damson", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  item = STAILQ_FIRST(&VarPapaya->head)->data;
  TEST_MSG("Address: %s = %s\n", name, item);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP list, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", err->data);
  }
  else
  {
    TEST_MSG("%s\n", err->data);
    goto tv_out;
  }
  item = STAILQ_FIRST(&VarPapaya->head)->data;
  TEST_MSG("Native: %s = %s\n", name, item);

  result = true;
tv_out:
  slist_free(&list);
  log_line(__func__);
  return result;
}

static void dump_native(struct ConfigSet *cs, const char *parent, const char *child)
{
  intptr_t pval = cs_str_native_get(cs, parent, NULL);
  intptr_t cval = cs_str_native_get(cs, child, NULL);

  struct Slist *pl = (struct Slist *) pval;
  struct Slist *cl = (struct Slist *) cval;

  char *pstr = pl ? STAILQ_FIRST(&pl->head)->data : NULL;
  char *cstr = cl ? STAILQ_FIRST(&cl->head)->data : NULL;

  TEST_MSG("%15s = %s\n", parent, NONULL(pstr));
  TEST_MSG("%15s = %s\n", child, NONULL(cstr));
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  bool result = false;

  const char *account = "fruit";
  const char *parent = "Quince";
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
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "apple", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "banana", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", err->data);
    goto ti_out;
  }
  dump_native(cs, parent, child);

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

bool slist_test_separator(struct ConfigDef Vars[], struct Buffer *err)
{
  log_line(__func__);

  mutt_buffer_reset(err);
  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  slist_init(cs);
  if (!cs_register_variables(cs, Vars, 0))
    return false;

  notify_observer_add(NeoMutt->notify, log_observer, 0);

  set_list(cs);

  if (!test_initial_values(cs, err))
    return false;
  if (!test_string_set(cs, err))
    return false;
  if (!test_string_get(cs, err))
    return false;

  neomutt_free(&NeoMutt);
  cs_free(&cs);
  return true;
}

void test_config_slist(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, 256);
  err.dsize = 256;

  TEST_CHECK(test_slist_parse(&err));
  TEST_CHECK(test_slist_add_string(&err));
  TEST_CHECK(test_slist_remove_string(&err));
  TEST_CHECK(test_slist_is_member(&err));
  TEST_CHECK(test_slist_add_list(&err));
  TEST_CHECK(test_slist_compare(&err));

  TEST_CHECK(slist_test_separator(VarsColon, &err));
  TEST_CHECK(slist_test_separator(VarsComma, &err));
  TEST_CHECK(slist_test_separator(VarsSpace, &err));

  struct ConfigSet *cs = cs_new(30);
  NeoMutt = neomutt_new(cs);

  slist_init(cs);
  dont_fail = true;
  if (!cs_register_variables(cs, VarsOther, 0))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, log_observer, 0);

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
