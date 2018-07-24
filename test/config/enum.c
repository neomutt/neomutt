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
#include "config/enum.h"
#include "config/set.h"
#include "config/types.h"
#include "config/common.h"

static short VarApple;

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
  { "Apple", DT_ENUM, IP &AnimalDef, &VarApple, ANIMAL_DINGO, NULL },
  { NULL },
};
// clang-format on

static bool test_initial_values(struct ConfigSet *cs, struct Buffer *err)
{
  log_line(__func__);
  printf("Apple = %d\n", VarApple);

  const char *name = "Apple";

  int rc = cs_str_string_set(cs, name, "herbivore", err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }
  printf("%s = %d, %s\n", name, VarApple, err->data);

  rc = cs_str_string_get(cs, name, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("Get failed: %s\n", err->data);
    return false;
  }

  short value = 41;
  mutt_buffer_reset(err);
  rc = cs_str_native_set(cs, name, value, err);
  if (CSR_RESULT(rc) != CSR_SUCCESS)
  {
    printf("%s\n", err->data);
    return false;
  }

  if (VarApple != value)
  {
    printf("Value of %s wasn't changed\n", name);
    return false;
  }

  printf("%s = %d, %s\n", name, VarApple, err->data);
  return true;
}

bool enum_test(void)
{
  log_line(__func__);

  struct Buffer err;
  mutt_buffer_init(&err);
  err.data = mutt_mem_calloc(1, STRING);
  err.dsize = STRING;
  mutt_buffer_reset(&err);

  struct ConfigSet *cs = cs_create(30);

  enum_init(cs);
  if (!cs_register_variables(cs, Vars, 0))
    return false;

  cs_add_listener(cs, log_listener);

  set_list(cs);

  if (!test_initial_values(cs, &err))
    return false;

  cs_free(&cs);
  FREE(&err.data);

  return true;
}
