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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "common.h" // IWYU pragma: keep
#include "test_common.h"

// clang-format off
struct ConfigDef VarsColon[] = {
  { "Apple",      DT_SLIST|SLIST_SEP_COLON, IP "apple",               0, NULL, }, /* test_initial_values */
  { "Banana",     DT_SLIST|SLIST_SEP_COLON, IP "apple:banana",        0, NULL, },
  { "Cherry",     DT_SLIST|SLIST_SEP_COLON, IP "apple:banana:cherry", 0, NULL, },
  { "Damson",     DT_SLIST|SLIST_SEP_COLON, IP "apple:banana",        0, NULL, }, /* test_string_set */
  { "Elderberry", DT_SLIST|SLIST_SEP_COLON, 0,                        0, NULL, },
  { "Fig",        DT_SLIST|SLIST_SEP_COLON, IP ":apple",              0, NULL, }, /* test_string_get */
  { "Guava",      DT_SLIST|SLIST_SEP_COLON, IP "apple::cherry",       0, NULL, },
  { "Hawthorn",   DT_SLIST|SLIST_SEP_COLON, IP "apple:",              0, NULL, },
  { NULL },
};

struct ConfigDef VarsComma[] = {
  { "Apple",      DT_SLIST|SLIST_SEP_COMMA, IP "apple",               0, NULL, }, /* test_initial_values */
  { "Banana",     DT_SLIST|SLIST_SEP_COMMA, IP "apple,banana",        0, NULL, },
  { "Cherry",     DT_SLIST|SLIST_SEP_COMMA, IP "apple,banana,cherry", 0, NULL, },
  { "Damson",     DT_SLIST|SLIST_SEP_COLON, IP "apple,banana",        0, NULL, }, /* test_string_set */
  { "Elderberry", DT_SLIST|SLIST_SEP_COLON, 0,                        0, NULL, },
  { "Fig",        DT_SLIST|SLIST_SEP_COLON, IP ",apple",              0, NULL, }, /* test_string_get */
  { "Guava",      DT_SLIST|SLIST_SEP_COLON, IP "apple,,cherry",       0, NULL, },
  { "Hawthorn",   DT_SLIST|SLIST_SEP_COLON, IP "apple,",              0, NULL, },
  { NULL },
};

struct ConfigDef VarsSpace[] = {
  { "Apple",      DT_SLIST|SLIST_SEP_SPACE, IP "apple",               0, NULL, }, /* test_initial_values */
  { "Banana",     DT_SLIST|SLIST_SEP_SPACE, IP "apple banana",        0, NULL, },
  { "Cherry",     DT_SLIST|SLIST_SEP_SPACE, IP "apple banana cherry", 0, NULL, },
  { "Damson",     DT_SLIST|SLIST_SEP_COLON, IP "apple banana",        0, NULL, }, /* test_string_set */
  { "Elderberry", DT_SLIST|SLIST_SEP_COLON, 0,                        0, NULL, },
  { "Fig",        DT_SLIST|SLIST_SEP_COLON, IP " apple",              0, NULL, }, /* test_string_get */
  { "Guava",      DT_SLIST|SLIST_SEP_COLON, IP "apple  cherry",       0, NULL, },
  { "Hawthorn",   DT_SLIST|SLIST_SEP_COLON, IP "apple ",              0, NULL, },
  { NULL },
};

struct ConfigDef VarsOther[] = {
  { "Ilama",      DT_SLIST|SLIST_SEP_COLON, 0,                        0, NULL,                    }, /* test_native_set */
  { "Jackfruit",  DT_SLIST|SLIST_SEP_COLON, IP "apple:banana:cherry", 0, NULL,                    }, /* test_native_get */
  { "Lemon",      DT_SLIST|SLIST_SEP_COLON, IP "lemon",               0, NULL,                    }, /* test_reset */
  { "Mango",      DT_SLIST|SLIST_SEP_COLON, IP "mango",               0, validator_fail,          },
  { "Nectarine",  DT_SLIST|SLIST_SEP_COLON, IP "nectarine",           0, validator_succeed,       }, /* test_validator */
  { "Olive",      DT_SLIST|SLIST_SEP_COLON, IP "olive",               0, validator_warn,          },
  { "Papaya",     DT_SLIST|SLIST_SEP_COLON, IP "papaya",              0, validator_fail,          },
  { "Quince",     DT_SLIST|SLIST_SEP_COLON, 0,                        0, NULL,                    }, /* test_inherit */
  { "Raspberry",  DT_SLIST|SLIST_SEP_COLON, 0,                        0, NULL,                    }, /* test_plus_equals */
  { "Strawberry", DT_SLIST|SLIST_SEP_COLON, 0,                        0, NULL,                    }, /* test_minus_equals */
  { "Wolfberry",  DT_SLIST|SLIST_SEP_COLON, IP "utf-8",               0, charset_slist_validator, }, /* test_charset_validator */
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
  TEST_MSG("%s\n", mutt_buffer_string(buf));
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

  uint32_t flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
  slist_flags(flags, err);
  TEST_MSG("Flags: %s", mutt_buffer_string(err));
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

    uint32_t flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
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

    uint32_t flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
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

  uint32_t flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;

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
  struct Slist *list1 = slist_parse("apple:banana::cherry", SLIST_SEP_COLON | SLIST_ALLOW_EMPTY);
  slist_dump(list1, err);

  struct Slist *list2 = slist_parse("apple,banana,,cherry", SLIST_SEP_COMMA | SLIST_ALLOW_EMPTY);
  slist_dump(list2, err);

  struct Slist *list3 = slist_parse("apple,banana,,cherry,damson",
                                    SLIST_SEP_COMMA | SLIST_ALLOW_EMPTY);
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

static bool test_initial_values(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  static const char *values[] = { "apple", "banana", "cherry", NULL };
  struct ConfigSet *cs = sub->cs;

  const struct Slist *VarApple = cs_subset_slist(sub, "Apple");
  slist_flags(VarApple->flags, err);
  TEST_MSG("Apple, %ld items, %s flags\n", VarApple->count, mutt_buffer_string(err));
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

  const struct Slist *VarBanana = cs_subset_slist(sub, "Banana");
  slist_flags(VarBanana->flags, err);
  TEST_MSG("Banana, %ld items, %s flags\n", VarBanana->count, mutt_buffer_string(err));
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

  const struct Slist *VarCherry = cs_subset_slist(sub, "Cherry");
  slist_flags(VarCherry->flags, err);
  TEST_MSG("Cherry, %ld items, %s flags\n", VarCherry->count, mutt_buffer_string(err));
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
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  value = "strawberry";
  rc = cs_str_initial_set(cs, name, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  name = "Elderberry";
  mutt_buffer_reset(err);
  rc = cs_str_initial_get(cs, name, err);
  if (!TEST_CHECK(rc == (CSR_SUCCESS | CSR_SUC_EMPTY)))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  return true;
}

static bool test_string_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  int rc;

  mutt_buffer_reset(err);
  char *name = "Damson";
  rc = cs_str_string_set(cs, name, "pig:quail:rhino", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  mutt_buffer_reset(err);
  name = "Damson";
  rc = cs_str_string_set(cs, name, "", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  mutt_buffer_reset(err);
  name = "Damson";
  rc = cs_str_string_set(cs, name, NULL, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  mutt_buffer_reset(err);
  name = "Elderberry";
  rc = cs_str_string_set(cs, name, "pig:quail:rhino", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  return true;
}

static bool test_string_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;

  struct Buffer *initial = mutt_buffer_pool_get();

  mutt_buffer_reset(err);
  char *name = "Fig";

  int rc = cs_str_initial_get(cs, name, initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  if (!mutt_str_equal(mutt_buffer_string(initial), mutt_buffer_string(err)))
  {
    TEST_MSG("Differ: %s '%s' '%s'\n", name, mutt_buffer_string(initial),
             mutt_buffer_string(err));
    return false;
  }
  TEST_MSG("Match: %s '%s' '%s'\n", name, mutt_buffer_string(initial),
           mutt_buffer_string(err));

  mutt_buffer_reset(err);
  mutt_buffer_reset(initial);
  name = "Guava";

  rc = cs_str_initial_get(cs, name, initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  if (!mutt_str_equal(mutt_buffer_string(initial), mutt_buffer_string(err)))
  {
    TEST_MSG("Differ: %s '%s' '%s'\n", name, mutt_buffer_string(initial),
             mutt_buffer_string(err));
    return false;
  }
  TEST_MSG("Match: %s '%s' '%s'\n", name, mutt_buffer_string(initial),
           mutt_buffer_string(err));

  mutt_buffer_reset(err);
  mutt_buffer_reset(initial);
  name = "Hawthorn";

  rc = cs_str_initial_get(cs, name, initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  if (!mutt_str_equal(mutt_buffer_string(initial), mutt_buffer_string(err)))
  {
    TEST_MSG("Differ: %s '%s' '%s'\n", name, mutt_buffer_string(initial),
             mutt_buffer_string(err));
    return false;
  }
  TEST_MSG("Match: %s '%s' '%s'\n", name, mutt_buffer_string(initial),
           mutt_buffer_string(err));

  mutt_buffer_pool_release(&initial);
  return true;
}

static bool test_native_set(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *init = "apple:banana::cherry";
  const char *name = "Ilama";
  struct Slist *list = slist_parse(init, SLIST_SEP_COLON);

  mutt_buffer_reset(err);
  int rc = cs_str_native_set(cs, name, (intptr_t) list, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  mutt_buffer_reset(err);
  rc = cs_str_string_get(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  if (!TEST_CHECK(mutt_str_equal(mutt_buffer_string(err), init)))
    return false;

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, (intptr_t) NULL, err);
  if (!TEST_CHECK(rc == (CSR_SUCCESS | CSR_SUC_EMPTY)))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  slist_free(&list);
  return true;
}

static bool test_native_get(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Jackfruit";

  mutt_buffer_reset(err);
  intptr_t value = cs_str_native_get(cs, name, err);
  struct Slist *sl = (struct Slist *) value;

  const struct Slist *VarJackfruit = cs_subset_slist(sub, "Jackfruit");
  if (!TEST_CHECK(VarJackfruit == sl))
  {
    TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
    return false;
  }

  return true;
}

static bool test_plus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Raspberry";
  struct ConfigSet *cs = sub->cs;

  static char *PlusTests[][3] = {
    // clang-format off
    // Initial,        Plus,     Result
    { "",              "",       ""                   }, // Add nothing to various lists
    { "one",           "",       "one"                },
    { "one:two",       "",       "one:two"            },
    { "one:two:three", "",       "one:two:three"      },

    { "",              "nine",   "nine"               }, // Add an item to various lists
    { "one",           "nine",   "one:nine"           },
    { "one:two",       "nine",   "one:two:nine"       },
    { "one:two:three", "nine",   "one:two:three:nine" },

    { "one:two:three", "one",   "one:two:three"       }, // Add a duplicate to various positions
    { "one:two:three", "two",   "one:two:three"       },
    { "one:two:three", "three", "one:two:three"       },
    // clang-format on
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(PlusTests); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, PlusTests[i][0], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Set failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = cs_str_string_plus_equals(cs, name, PlusTests[i][1], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("PlusEquals failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = cs_str_string_get(cs, name, err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    if (!TEST_CHECK(mutt_str_equal(PlusTests[i][2], mutt_buffer_string(err))))
    {
      TEST_MSG("Expected: %s\n", PlusTests[i][2]);
      TEST_MSG("Actual  : %s\n", mutt_buffer_string(err));
      return false;
    }
  }

  // Test a failing validator
  VarsOther[8].validator = validator_fail; // "Raspberry"
  mutt_buffer_reset(err);
  rc = cs_str_string_plus_equals(cs, name, "nine", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  return true;
}

static bool test_minus_equals(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);
  const char *name = "Strawberry";
  struct ConfigSet *cs = sub->cs;

  static char *MinusTests[][3] = {
    // clang-format off
    // Initial,        Plus,    Result
    { "",              "",      ""              }, // Remove nothing from various lists
    { "one",           "",      "one"           },
    { "one:two",       "",      "one:two"       },
    { "one:two:three", "",      "one:two:three" },

    { "",              "",      ""              }, // Remove an item from various lists
    { "one",           "one",   ""              },
    { "one:two",       "one",   "two"           },
    { "one:two",       "two",   "one"           },
    { "one:two:three", "one",   "two:three"     },
    { "one:two:three", "two",   "one:three"     },
    { "one:two:three", "three", "one:two"       },

    { "",              "nine",  ""              }, // Remove a non-existent item from various lists
    { "one",           "nine",  "one"           },
    { "one:two",       "nine",  "one:two"       },
    { "one:two:three", "nine",  "one:two:three" },
    // clang-format on
  };

  int rc;
  for (size_t i = 0; i < mutt_array_size(MinusTests); i++)
  {
    mutt_buffer_reset(err);
    rc = cs_str_string_set(cs, name, MinusTests[i][0], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Set failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = cs_str_string_minus_equals(cs, name, MinusTests[i][1], err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("MinusEquals failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    rc = cs_str_string_get(cs, name, err);
    if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    {
      TEST_MSG("Get failed: %s\n", mutt_buffer_string(err));
      return false;
    }

    if (!TEST_CHECK(mutt_str_equal(MinusTests[i][2], mutt_buffer_string(err))))
    {
      TEST_MSG("Expected: %s\n", MinusTests[i][2]);
      TEST_MSG("Actual  : %s\n", mutt_buffer_string(err));
      return false;
    }
  }

  // Test a failing validator
  VarsOther[9].validator = validator_fail; // "Strawberry"
  mutt_buffer_reset(err);
  rc = cs_str_string_minus_equals(cs, name, "two", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  return true;
}

static bool test_reset(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  const char *name = "Lemon";

  mutt_buffer_reset(err);

  const struct Slist *VarLemon = cs_subset_slist(sub, "Lemon");
  char *item = STAILQ_FIRST(&VarLemon->head)->data;
  TEST_MSG("Initial: %s = '%s'\n", name, item);
  int rc = cs_str_string_set(cs, name, "apple", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarLemon = cs_subset_slist(sub, "Lemon");
  item = STAILQ_FIRST(&VarLemon->head)->data;
  TEST_MSG("Set: %s = '%s'\n", name, item);

  rc = cs_str_reset(cs, name, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarLemon = cs_subset_slist(sub, "Lemon");
  item = STAILQ_FIRST(&VarLemon->head)->data;
  if (!TEST_CHECK(mutt_str_equal(item, "lemon")))
  {
    TEST_MSG("Value of %s wasn't changed\n", name);
    return false;
  }

  TEST_MSG("Reset: %s = '%s'\n", name, item);

  name = "Mango";
  mutt_buffer_reset(err);

  const struct Slist *VarMango = cs_subset_slist(sub, "Mango");
  item = STAILQ_FIRST(&VarMango->head)->data;
  TEST_MSG("Initial: %s = '%s'\n", name, item);
  dont_fail = true;
  rc = cs_str_string_set(cs, name, "banana", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
    return false;
  VarMango = cs_subset_slist(sub, "Mango");
  item = STAILQ_FIRST(&VarMango->head)->data;
  TEST_MSG("Set: %s = '%s'\n", name, item);
  dont_fail = false;

  rc = cs_str_reset(cs, name, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    return false;
  }

  VarMango = cs_subset_slist(sub, "Mango");
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

static bool test_validator(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  char *item = NULL;
  struct Slist *list = slist_parse("apple", SLIST_SEP_COMMA);
  bool result = false;

  const char *name = "Nectarine";
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "banana", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  const struct Slist *VarNectarine = cs_subset_slist(sub, "Nectarine");
  item = STAILQ_FIRST(&VarNectarine->head)->data;
  TEST_MSG("Address: %s = %s\n", name, item);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP list, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  VarNectarine = cs_subset_slist(sub, "Nectarine");
  item = STAILQ_FIRST(&VarNectarine->head)->data;
  TEST_MSG("Native: %s = %s\n", name, item);

  name = "Olive";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "cherry", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  const struct Slist *VarOlive = cs_subset_slist(sub, "Olive");
  item = STAILQ_FIRST(&VarOlive->head)->data;
  TEST_MSG("Address: %s = %s\n", name, item);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP list, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  VarOlive = cs_subset_slist(sub, "Olive");
  item = STAILQ_FIRST(&VarOlive->head)->data;
  TEST_MSG("Native: %s = %s\n", name, item);

  name = "Papaya";
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "damson", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  const struct Slist *VarPapaya = cs_subset_slist(sub, "Papaya");
  item = STAILQ_FIRST(&VarPapaya->head)->data;
  TEST_MSG("Address: %s = %s\n", name, item);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP list, err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("Expected error: %s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  VarPapaya = cs_subset_slist(sub, "Papaya");
  item = STAILQ_FIRST(&VarPapaya->head)->data;
  TEST_MSG("Native: %s = %s\n", name, item);

  result = true;
tv_out:
  slist_free(&list);
  log_line(__func__);
  return result;
}

static bool test_charset_validator(struct ConfigSubset *sub, struct Buffer *err)
{
  log_line(__func__);

  struct ConfigSet *cs = sub->cs;
  char *item = NULL;
  struct Slist *list = slist_parse("utf-8", SLIST_SEP_COLON);
  bool result = false;

  const char *name = "Wolfberry";
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, name, "utf-8", err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  const struct Slist *VarWolfberry = cs_subset_slist(sub, "Wolfberry");
  item = STAILQ_FIRST(&VarWolfberry->head)->data;
  TEST_MSG("Address: %s = %s\n", name, item);

  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, IP list, err);
  if (TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }
  VarWolfberry = cs_subset_slist(sub, "Wolfberry");
  item = STAILQ_FIRST(&VarWolfberry->head)->data;
  TEST_MSG("Native: %s = %s\n", name, item);

  // When one of the charsets is invalid, it fails
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, name, "us-ascii:utf-3", err);
  if (TEST_CHECK(CSR_RESULT(rc) != CSR_SUCCESS))
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
  }
  else
  {
    TEST_MSG("%s\n", mutt_buffer_string(err));
    goto tv_out;
  }

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
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }

  // set parent
  mutt_buffer_reset(err);
  int rc = cs_str_string_set(cs, parent, "apple", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // set child
  mutt_buffer_reset(err);
  rc = cs_str_string_set(cs, child, "banana", err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset child
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, child, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
    goto ti_out;
  }
  dump_native(cs, parent, child);

  // reset parent
  mutt_buffer_reset(err);
  rc = cs_str_reset(cs, parent, err);
  if (!TEST_CHECK(CSR_RESULT(rc) == CSR_SUCCESS))
  {
    TEST_MSG("Error: %s\n", mutt_buffer_string(err));
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

bool slist_test_separator(struct ConfigDef vars[], struct Buffer *err)
{
  log_line(__func__);

  NeoMutt = test_neomutt_create();
  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  mutt_buffer_reset(err);

  cs_register_type(cs, &CstSlist);
  if (!TEST_CHECK(cs_register_variables(cs, vars, DT_NO_FLAGS)))
    return false;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  set_list(cs);

  if (!test_initial_values(sub, err))
    return false;
  if (!test_string_set(sub, err))
    return false;
  if (!test_string_get(sub, err))
    return false;

  test_neomutt_destroy(&NeoMutt);
  return true;
}

void test_config_slist(void)
{
  log_line(__func__);

  struct Buffer *err = mutt_buffer_pool_get();
  TEST_CHECK(test_slist_parse(err));
  TEST_CHECK(test_slist_add_string(err));
  TEST_CHECK(test_slist_remove_string(err));
  TEST_CHECK(test_slist_is_member(err));
  TEST_CHECK(test_slist_add_list(err));
  TEST_CHECK(test_slist_compare(err));

  TEST_CHECK(slist_test_separator(VarsColon, err));
  TEST_CHECK(slist_test_separator(VarsComma, err));
  TEST_CHECK(slist_test_separator(VarsSpace, err));

  NeoMutt = test_neomutt_create();
  struct ConfigSubset *sub = NeoMutt->sub;
  struct ConfigSet *cs = sub->cs;

  dont_fail = true;
  if (!TEST_CHECK(cs_register_variables(cs, VarsOther, DT_NO_FLAGS)))
    return;
  dont_fail = false;

  notify_observer_add(NeoMutt->notify, NT_CONFIG, log_observer, 0);

  TEST_CHECK(test_native_set(sub, err));
  TEST_CHECK(test_native_get(sub, err));
  TEST_CHECK(test_plus_equals(sub, err));
  TEST_CHECK(test_minus_equals(sub, err));
  TEST_CHECK(test_reset(sub, err));
  TEST_CHECK(test_validator(sub, err));
  TEST_CHECK(test_charset_validator(sub, err));
  TEST_CHECK(test_inherit(cs, err));
  mutt_buffer_pool_release(&err);

  test_neomutt_destroy(&NeoMutt);
  log_line(__func__);
}
