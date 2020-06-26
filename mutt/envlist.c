/**
 * @file
 * Private copy of the environment variables
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
 * @page envlist Private copy of the environment variables
 *
 * Private copy of the environment variables
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include "envlist.h"
#include "memory.h"
#include "string2.h"

char **EnvList = NULL; ///< Private copy of the environment variables

/**
 * mutt_envlist_free - Free the private copy of the environment
 */
void mutt_envlist_free(void)
{
  if (!EnvList)
    return;

  for (char **p = EnvList; p && *p; p++)
    FREE(p);

  FREE(&EnvList);
}

/**
 * mutt_envlist_init - Create a copy of the environment
 * @param envp Environment variables
 */
void mutt_envlist_init(char *envp[])
{
  if (EnvList)
    mutt_envlist_free();

  if (!envp)
    return;

  char **src = NULL, **dst = NULL;
  int count = 0;
  for (src = envp; src && *src; src++)
    count++;

  EnvList = mutt_mem_calloc(count + 1, sizeof(char *));
  for (src = envp, dst = EnvList; src && *src; src++, dst++)
    *dst = mutt_str_dup(*src);
}

/**
 * mutt_envlist_set - Set an environment variable
 * @param name      Name of the variable
 * @param value     New value
 * @param overwrite Should the variable be overwritten?
 * @retval true  Success: variable set, or overwritten
 * @retval false Variable exists and overwrite was false
 *
 * It's broken out because some other parts of neomutt (filter.c) need to
 * set/overwrite environment variables in EnvList before calling exec().
 */
bool mutt_envlist_set(const char *name, const char *value, bool overwrite)
{
  if (!name)
    return false;

  char **envp = EnvList;
  char work[1024];

  /* Look for current slot to overwrite */
  int count = 0;
  while (envp && *envp)
  {
    size_t len = mutt_str_startswith(*envp, name);
    if ((len != 0) && ((*envp)[len] == '='))
    {
      if (!overwrite)
        return false;
      break;
    }
    envp++;
    count++;
  }

  /* Format var=value string */
  snprintf(work, sizeof(work), "%s=%s", name, NONULL(value));

  if (envp && *envp)
  {
    /* slot found, overwrite */
    mutt_str_replace(envp, work);
  }
  else
  {
    /* not found, add new slot */
    mutt_mem_realloc(&EnvList, sizeof(char *) * (count + 2));
    EnvList[count] = mutt_str_dup(work);
    EnvList[count + 1] = NULL;
  }
  return true;
}

/**
 * mutt_envlist_unset - Unset an environment variable
 * @param name Variable to unset
 * @retval true  Success: Variable unset
 * @retval false Error: Variable doesn't exist
 */
bool mutt_envlist_unset(const char *name)
{
  if (!name || (name[0] == '\0'))
    return false;

  char **envp = EnvList;

  int count = 0;
  while (envp && *envp)
  {
    size_t len = mutt_str_startswith(*envp, name);
    if ((len != 0) && ((*envp)[len] == '='))
    {
      FREE(envp);
      /* shuffle down */
      char **save = envp++;
      while (*envp)
      {
        *save++ = *envp++;
        count++;
      }
      *save = NULL;
      mutt_mem_realloc(&EnvList, sizeof(char *) * (count + 1));
      return true;
    }
    envp++;
    count++;
  }
  return false;
}

/**
 * mutt_envlist_getlist - Get the private environment
 * @retval ptr Array of strings
 *
 * @note The caller must not free the strings
 */
char **mutt_envlist_getlist(void)
{
  return EnvList;
}
