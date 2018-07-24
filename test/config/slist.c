/**
 * @file
 * Test code for the Slist object
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

#include "config.h"
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "mutt/buffer.h"
#include "mutt/mapping.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "config/account.h"
#include "config/bool.h"
#include "config/set.h"
#include "config/slist.h"
#include "config/types.h"
#include "config/common.h"

static struct Slist *VarApple;
static struct Slist *VarBanana;
static struct Slist *VarCherry;
static struct Slist *VarDamson;
static struct Slist *VarElderberry;
static struct Slist *VarFig;
static struct Slist *VarGuava;
static struct Slist *VarHawthorn;
#if 0
static struct Slist *VarIlama;
static struct Slist *VarJackfruit;
static struct Slist *VarKumquat;
static struct Slist *VarLemon;
static struct Slist *VarMango;
static struct Slist *VarNectarine;
static struct Slist *VarOlive;
static struct Slist *VarPapaya;
static struct Slist *VarQuince;
static struct Slist *VarRaspberry;
static struct Slist *VarStrawberry;
#endif

// clang-format off
static struct ConfigDef VarsColon[] = {
  { "Apple",      DT_SLIST, SLIST_SEP_COLON, &VarApple,      IP "apple",               NULL }, /* test_initial_values */
  { "Banana",     DT_SLIST, SLIST_SEP_COLON, &VarBanana,     IP "apple:banana",        NULL },
  { "Cherry",     DT_SLIST, SLIST_SEP_COLON, &VarCherry,     IP "apple:banana:cherry", NULL },
  { "Damson",     DT_SLIST, SLIST_SEP_COLON, &VarDamson,     IP "apple:banana",        NULL }, /* test_string_set */
  { "Elderberry", DT_SLIST, SLIST_SEP_COLON, &VarElderberry, 0,                        NULL },
  { "Fig",        DT_SLIST, SLIST_SEP_COLON, &VarFig,        IP ":apple",              NULL }, /* test_string_get */
  { "Guava",      DT_SLIST, SLIST_SEP_COLON, &VarGuava,      IP "apple::cherry",       NULL },
  { "Hawthorn",   DT_SLIST, SLIST_SEP_COLON, &VarHawthorn,   IP "apple:",              NULL },
  { NULL },
};

static struct ConfigDef VarsComma[] = {
  { "Apple",      DT_SLIST, SLIST_SEP_COMMA, &VarApple,      IP "apple",               NULL }, /* test_initial_values */
  { "Banana",     DT_SLIST, SLIST_SEP_COMMA, &VarBanana,     IP "apple,banana",        NULL },
  { "Cherry",     DT_SLIST, SLIST_SEP_COMMA, &VarCherry,     IP "apple,banana,cherry", NULL },
  { "Damson",     DT_SLIST, SLIST_SEP_COLON, &VarDamson,     IP "apple,banana",        NULL }, /* test_string_set */
  { "Elderberry", DT_SLIST, SLIST_SEP_COLON, &VarElderberry, 0,                        NULL },
  { "Fig",        DT_SLIST, SLIST_SEP_COLON, &VarFig,        IP ",apple",              NULL }, /* test_string_get */
  { "Guava",      DT_SLIST, SLIST_SEP_COLON, &VarGuava,      IP "apple,,cherry",       NULL },
  { "Hawthorn",   DT_SLIST, SLIST_SEP_COLON, &VarHawthorn,   IP "apple,",              NULL },
  { NULL },
};

static struct ConfigDef VarsSpace[] = {
  { "Apple",      DT_SLIST, SLIST_SEP_SPACE, &VarApple,      IP "apple",               NULL }, /* test_initial_values */
  { "Banana",     DT_SLIST, SLIST_SEP_SPACE, &VarBanana,     IP "apple banana",        NULL },
  { "Cherry",     DT_SLIST, SLIST_SEP_SPACE, &VarCherry,     IP "apple banana cherry", NULL },
  { "Damson",     DT_SLIST, SLIST_SEP_COLON, &VarDamson,     IP "apple banana",        NULL }, /* test_string_set */
  { "Elderberry", DT_SLIST, SLIST_SEP_COLON, &VarElderberry, 0,                        NULL },
  { "Fig",        DT_SLIST, SLIST_SEP_COLON, &VarFig,        IP " apple",              NULL }, /* test_string_get */
  { "Guava",      DT_SLIST, SLIST_SEP_COLON, &VarGuava,      IP "apple  cherry",       NULL },
  { "Hawthorn",   DT_SLIST, SLIST_SEP_COLON, &VarHawthorn,   IP "apple ",              NULL },
  { NULL },
};
// clang-format on

static void slist_flags(unsigned int flags)
{
  switch (flags & SLIST_SEP_MASK)
  {
    case SLIST_SEP_SPACE:
      printf("SPACE");
      break;
    case SLIST_SEP_COMMA:
      printf("COMMA");
      break;
    case SLIST_SEP_COLON:
      printf("COLON");
      break;
    default:
      printf("UNKNOWN");
      return;
  }

  if (flags & SLIST_ALLOW_DUPES)
    printf(" | SLIST_ALLOW_DUPES");
  if (flags & SLIST_ALLOW_EMPTY)
    printf(" | SLIST_ALLOW_EMPTY");
  if (flags & SLIST_CASE_SENSITIVE)
    printf(" | SLIST_CASE_SENSITIVE");
}

static void slist_dump(const struct Slist *list)
{
  if (!list)
    return;

  printf("[%ld] ", list->count);

  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &list->head, entries)
  {
    if (np->data)
      printf("'%s'", np->data);
    else
      printf("NULL");
    if (STAILQ_NEXT(np, entries))
      printf(",");
  }
  printf("\n");
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
  slist_flags(flags);
  printf("\n");

  struct Slist *list = NULL;
  for (size_t i = 0; i < mutt_array_size(init); i++)
  {
    printf(">>%s<<\n", init[i] ? init[i] : "NULL");
    list = slist_parse(init[i], flags);
    slist_dump(list);
    slist_free(&list);
  }

  return true;
}

static bool test_slist_add_string(struct Buffer *err)
{
  mutt_buffer_reset(err);

  struct Slist *list = slist_parse(NULL, SLIST_ALLOW_EMPTY);
  slist_dump(list);

  slist_add_string(list, NULL);
  slist_dump(list);

  slist_empty(&list);
  slist_add_string(list, "");
  slist_dump(list);

  slist_empty(&list);
  slist_add_string(list, "apple");
  slist_dump(list);
  slist_add_string(list, "banana");
  slist_dump(list);
  slist_add_string(list, "apple");
  slist_dump(list);

  slist_free(&list);

  return true;
}

static bool test_slist_remove_string(struct Buffer *err)
{
  mutt_buffer_reset(err);

  unsigned int flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
  struct Slist *list = slist_parse("apple:banana::cherry", flags);
  slist_dump(list);

  slist_remove_string(list, NULL);
  slist_dump(list);

  slist_remove_string(list, "apple");
  slist_dump(list);

  slist_remove_string(list, "damson");
  slist_dump(list);

  slist_free(&list);

  return true;
}

static bool test_slist_is_member(struct Buffer *err)
{
  mutt_buffer_reset(err);

  unsigned int flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;
  struct Slist *list = slist_parse("apple:banana::cherry", flags);
  slist_dump(list);

  static const char *values[] = { "apple", "", "damson", NULL };

  for (size_t i = 0; i < mutt_array_size(values); i++)
  {
    printf("member '%s' : %s\n", values[i], slist_is_member(list, values[i]) ? "yes" : "no");
  }

  slist_free(&list);
  return true;
}

static bool test_slist_add_list(struct Buffer *err)
{
  mutt_buffer_reset(err);

  unsigned int flags = SLIST_SEP_COLON | SLIST_ALLOW_EMPTY;

  struct Slist *list1 = slist_parse("apple:banana::cherry", flags);
  slist_dump(list1);

  struct Slist *list2 = slist_parse("damson::apple:apple", flags);
  slist_dump(list2);

  list1 = slist_add_list(list1, list2);
  slist_dump(list1);

  slist_free(&list1);
  slist_free(&list2);

  list1 = NULL;
  slist_dump(list1);

  list2 = slist_parse("damson::apple:apple", flags);
  slist_dump(list2);

  list1 = slist_add_list(list1, list2);
  slist_dump(list1);

  slist_free(&list1);
  slist_free(&list2);

  return true;
}

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  static const char *values[] = { "apple", "banana", "cherry", NULL };

  printf("Apple, %ld items, %u flags\n", VarApple->count, VarApple->flags);
  if (VarApple->count != 1)
  {
    printf("Apple should have 1 item\n");
    return false;
  }

  struct ListNode *np = NULL;
  size_t i = 0;
  STAILQ_FOREACH(np, &VarApple->head, entries)
  {
    if (mutt_str_strcmp(values[i], np->data) != 0)
      return false;
    i++;
  }

  printf("Banana, %ld items, %u flags\n", VarBanana->count, VarBanana->flags);
  if (VarBanana->count != 2)
  {
    printf("Banana should have 2 items\n");
    return false;
  }

  np = NULL;
  i = 0;
  STAILQ_FOREACH(np, &VarBanana->head, entries)
  {
    if (mutt_str_strcmp(values[i], np->data) != 0)
      return false;
    i++;
  }

  printf("Cherry, %ld items, %u flags\n", VarCherry->count, VarCherry->flags);
  if (VarCherry->count != 3)
  {
    printf("Cherry should have 3 items\n");
    return false;
  }

  np = NULL;
  i = 0;
  STAILQ_FOREACH(np, &VarCherry->head, entries)
  {
    if (mutt_str_strcmp(values[i], np->data) != 0)
      return false;
    i++;
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
    printf("%s\n", err->data);
    return false;
  }

  mutt_buffer_reset(err);
  name = "Elderberry";
  rc = cs_str_string_set(cs, name, "pig:quail:rhino", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  return true;
}

static bool test_string_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  struct Buffer initial;
  mutt_buffer_init(&initial);
  initial.data = mutt_mem_calloc(1, STRING);
  initial.dsize = STRING;

  mutt_buffer_reset(err);
  mutt_buffer_reset(&initial);
  char *name = "Fig";

  int rc = cs_str_initial_get(cs, name, &initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  if (mutt_str_strcmp(initial.data, err->data) != 0)
  {
    printf("Differ: %s '%s' '%s'\n", name, initial.data, err->data);
    return false;
  }
  printf("Match: %s '%s' '%s'\n", name, initial.data, err->data);

  mutt_buffer_reset(err);
  mutt_buffer_reset(&initial);
  name = "Guava";

  rc = cs_str_initial_get(cs, name, &initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  if (mutt_str_strcmp(initial.data, err->data) != 0)
  {
    printf("Differ: %s '%s' '%s'\n", name, initial.data, err->data);
    return false;
  }
  printf("Match: %s '%s' '%s'\n", name, initial.data, err->data);

  mutt_buffer_reset(err);
  mutt_buffer_reset(&initial);
  name = "Hawthorn";

  rc = cs_str_initial_get(cs, name, &initial);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  if (mutt_str_strcmp(initial.data, err->data) != 0)
  {
    printf("Differ: %s '%s' '%s'\n", name, initial.data, err->data);
    return false;
  }
  printf("Match: %s '%s' '%s'\n", name, initial.data, err->data);

  FREE(&initial.data);
  return true;
}

#if 0
static bool test_native_set(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  return false;
}

static bool test_native_get(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  return false;
}

static bool test_reset(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  return false;
}

static bool test_validator(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  return false;
}

static bool test_inherit(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);

  return false;
}
#endif

bool slist_test_separator(struct ConfigDef Vars[], struct Buffer *err)
{
  log_line(__func__);

  mutt_buffer_reset(err);
  struct ConfigSet *cs = cs_create(30);

  slist_init(cs);
  if (!cs_register_variables(cs, Vars, 0))
    return false;

  cs_add_listener(cs, log_listener);

  set_list(cs);

  if (!test_initial_values(cs, err))
    return false;
  if (!test_string_set(cs, err))
    return false;
  if (!test_string_get(cs, err))
    return false;

  cs_free(&cs);
  return true;
}

bool slist_test(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  bool result = false;

  if (!test_slist_parse(&err))
    goto st_done;
  if (!test_slist_add_string(&err))
    goto st_done;
  if (!test_slist_remove_string(&err))
    goto st_done;
  if (!test_slist_is_member(&err))
    goto st_done;
  if (!test_slist_add_list(&err))
    goto st_done;

  if (!slist_test_separator(VarsColon, &err))
    goto st_done;
  if (!slist_test_separator(VarsComma, &err))
    goto st_done;
  if (!slist_test_separator(VarsSpace, &err))
    goto st_done;

  result = true;
st_done:
  FREE(&err.data);
  return result;
}
