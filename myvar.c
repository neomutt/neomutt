/**
 * @file
 * Handling of personal config ('my' variables)
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

/**
 * @page myvar Handling of personal config ('my' variables)
 *
 * Handling of personal config ('my' variables)
 */

#include "config.h"
#include <stddef.h>
#include "mutt/mutt.h"
#include "myvar.h"

struct MyVarList MyVars = TAILQ_HEAD_INITIALIZER(MyVars);

/**
 * myvar_get - Get the value of a "my_" variable
 * @param var Variable name
 * @retval ptr  Success, value of variable
 * @retval NULL Error, variable doesn't exist
 */
const char *myvar_get(const char *var)
{
  struct MyVar *myv = NULL;

  TAILQ_FOREACH(myv, &MyVars, entries)
  {
    if (mutt_str_strcmp(myv->name, var) == 0)
      return NONULL(myv->value);
  }

  return NULL;
}

/**
 * myvar_set - Set the value of a "my_" variable
 * @param var Variable name
 * @param val Value to set
 */
void myvar_set(const char *var, const char *val)
{
  struct MyVar *myv = NULL;

  TAILQ_FOREACH(myv, &MyVars, entries)
  {
    if (mutt_str_strcmp(myv->name, var) == 0)
    {
      mutt_str_replace(&myv->value, val);
      return;
    }
  }

  myv = mutt_mem_calloc(1, sizeof(struct MyVar));
  myv->name = mutt_str_strdup(var);
  myv->value = mutt_str_strdup(val);
  TAILQ_INSERT_TAIL(&MyVars, myv, entries);
}

/**
 * myvar_del - Unset the value of a "my_" variable
 * @param var Variable name
 */
void myvar_del(const char *var)
{
  struct MyVar *myv = NULL;

  TAILQ_FOREACH(myv, &MyVars, entries)
  {
    if (mutt_str_strcmp(myv->name, var) == 0)
    {
      TAILQ_REMOVE(&MyVars, myv, entries);
      FREE(&myv->name);
      FREE(&myv->value);
      FREE(&myv);
      return;
    }
  }
}
