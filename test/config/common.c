/**
 * @file
 * Shared Testing Code
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mutt/buffer.h"
#include "mutt/hash.h"
#include "mutt/memory.h"
#include "mutt/string2.h"
#include "config/inheritance.h"
#include "config/set.h"
#include "config/types.h"
#include "common.h"

const char *line = "----------------------------------------"
                   "----------------------------------------";

bool dont_fail = false;

int validator_fail(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                   intptr_t value, struct Buffer *result)
{
  if (dont_fail)
    return CSR_SUCCESS;

  if (value > 1000000)
    mutt_buffer_printf(result, "%s: %s, (ptr)", __func__, cdef->name);
  else
    mutt_buffer_printf(result, "%s: %s, %ld", __func__, cdef->name, value);
  return CSR_ERR_INVALID;
}

int validator_warn(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                   intptr_t value, struct Buffer *result)
{
  if (value > 1000000)
    mutt_buffer_printf(result, "%s: %s, (ptr)", __func__, cdef->name);
  else
    mutt_buffer_printf(result, "%s: %s, %ld", __func__, cdef->name, value);
  return CSR_SUCCESS | CSR_SUC_WARNING;
}

int validator_succeed(const struct ConfigSet *cs, const struct ConfigDef *cdef,
                      intptr_t value, struct Buffer *result)
{
  if (value > 1000000)
    mutt_buffer_printf(result, "%s: %s, (ptr)", __func__, cdef->name);
  else
    mutt_buffer_printf(result, "%s: %s, %ld", __func__, cdef->name, value);
  return CSR_SUCCESS;
}

void log_line(const char *fn)
{
  int len = 54 - mutt_str_strlen(fn);
  printf("---- %s %.*s\n", fn, len, line);
}

bool log_listener(const struct ConfigSet *cs, struct HashElem *he,
                  const char *name, enum ConfigEvent ev)
{
  struct Buffer result;
  mutt_buffer_init(&result);
  result.data = mutt_mem_calloc(1, STRING);
  result.dsize = STRING;

  const char *events[] = { "set", "reset", "initial-set" };

  mutt_buffer_reset(&result);

  if (ev != CE_INITIAL_SET)
    cs_he_string_get(cs, he, &result);
  else
    cs_he_initial_get(cs, he, &result);

  printf("Event: %s has been %s to '%s'\n", name, events[ev - 1], result.data);

  FREE(&result.data);
  return true;
}

void set_list(const struct ConfigSet *cs)
{
  log_line(__func__);
  cs_dump_set(cs);
  // hash_dump(cs->hash);
}

void hash_dump(struct Hash *table)
{
  if (!table)
    return;

  struct HashElem *he = NULL;

  for (int i = 0; i < table->nelem; i++)
  {
    he = table->table[i];
    if (!he)
      continue;

    if (he->type == DT_SYNONYM)
      continue;

    printf("%03d ", i);
    for (; he; he = he->next)
    {
      if (he->type & DT_INHERITED)
      {
        struct Inheritance *inh = he->data;
        printf("\033[1;32m[%s]\033[m ", inh->name);
      }
      else
      {
        printf("%s ", *(char **) he->data);
      }
    }
    printf("\n");
  }
}

int sort_list_cb(const void *a, const void *b)
{
  const char *stra = *(char **) a;
  const char *strb = *(char **) b;
  return strcmp(stra, strb);
}

void cs_dump_set(const struct ConfigSet *cs)
{
  if (!cs)
    return;

  struct HashElem *he = NULL;
  struct HashWalkState state;
  memset(&state, 0, sizeof(state));

  struct Buffer result;
  mutt_buffer_init(&result);
  result.data = mutt_mem_calloc(1, STRING);
  result.dsize = STRING;

  char tmp[128];
  char *list[26] = { 0 };
  size_t index = 0;
  size_t i = 0;

  for (i = 0; (he = mutt_hash_walk(cs->hash, &state)); i++)
  {
    if (he->type == DT_SYNONYM)
      continue;

    const char *name = NULL;

    if (he->type & DT_INHERITED)
    {
      struct Inheritance *inh = he->data;
      he = inh->parent;
      name = inh->name;
    }
    else
    {
      name = he->key.strkey;
    }

    const struct ConfigSetType *cst = cs_get_type_def(cs, he->type);
    if (!cst)
    {
      snprintf(tmp, sizeof(tmp), "Unknown type: %d", he->type);
      list[index] = mutt_str_strdup(tmp);
      index++;
      continue;
    }

    mutt_buffer_reset(&result);
    const struct ConfigDef *cdef = he->data;

    int rc = cst->string_get(cs, cdef->var, cdef, &result);
    if (CSR_RESULT(rc) == CSR_SUCCESS)
      snprintf(tmp, sizeof(tmp), "%s %s = %s", cst->name, name, result.data);
    else
      snprintf(tmp, sizeof(tmp), "%s %s: ERROR: %s", cst->name, name, result.data);
    list[index] = mutt_str_strdup(tmp);
    index++;
  }

  qsort(list, index, sizeof(list[0]), sort_list_cb);
  for (i = 0; list[i]; i++)
  {
    puts(list[i]);
    FREE(&list[i]);
  }

  FREE(&result.data);
}
