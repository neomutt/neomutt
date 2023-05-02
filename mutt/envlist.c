/**
 * @file
 * Private copy of the environment variables
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
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
 * @page mutt_envlist Private copy of the environment variables
 *
 * Private copy of the environment variables
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "envlist.h"
#include "memory.h"
#include "string2.h"

/**
 * envlist_free - Free the private copy of the environment
 * @param envp Environment to free
 */
void envlist_free(char ***envp)
{
  if (!envp || !*envp)
    return;

  for (char **p = *envp; p && *p; p++)
    FREE(p);

  FREE(envp);
}

/**
 * envlist_init - Create a copy of the environment
 * @param envp Environment to copy
 * @retval ptr Copy of the environment
 */
char **envlist_init(char **envp)
{
  if (!envp)
    return NULL;

  char **src = NULL;
  char **dst = NULL;
  int count = 0;
  for (src = envp; src && *src; src++)
    count++;

  char **env_copy = mutt_mem_calloc(count + 1, sizeof(char *));
  for (src = envp, dst = env_copy; src && *src; src++, dst++)
    *dst = mutt_str_dup(*src);

  return env_copy;
}

/**
 * envlist_set - Set an environment variable
 * @param envp      Environment to modify
 * @param name      Name of the variable
 * @param value     New value
 * @param overwrite Should the variable be overwritten?
 * @retval true  Success: variable set, or overwritten
 * @retval false Variable exists and overwrite was false
 *
 * It's broken out because some other parts of neomutt (filter.c) need to
 * set/overwrite environment variables in EnvList before calling exec().
 */
bool envlist_set(char ***envp, const char *name, const char *value, bool overwrite)
{
  if (!envp || !*envp || !name || (name[0] == '\0'))
    return false;

  // Find a matching entry
  int count = 0;
  int match = -1;
  char *str = NULL;
  for (; (str = (*envp)[count]); count++)
  {
    size_t len = mutt_str_startswith(str, name);
    if ((len != 0) && (str[len] == '='))
    {
      if (!overwrite)
        return false;
      match = count;
      break;
    }
  }

  // Format var=value string
  char work[1024] = { 0 };
  snprintf(work, sizeof(work), "%s=%s", name, NONULL(value));

  if (match >= 0)
  {
    // match found, overwrite
    mutt_str_replace(&(*envp)[match], work);
  }
  else
  {
    // not found, add a new entry
    mutt_mem_realloc(envp, (count + 2) * sizeof(char *));
    (*envp)[count] = mutt_str_dup(work);
    (*envp)[count + 1] = NULL;
  }

  return true;
}

/**
 * envlist_unset - Unset an environment variable
 * @param envp Environment to modify
 * @param name Variable to unset
 * @retval true  Success: Variable unset
 * @retval false Error: Variable doesn't exist
 */
bool envlist_unset(char ***envp, const char *name)
{
  if (!envp || !*envp || !name || (name[0] == '\0'))
    return false;

  int count = 0;
  for (; (*envp)[count]; count++)
    ; // do nothing

  char *str = NULL;
  for (int match = 0; (str = (*envp)[match]); match++)
  {
    size_t len = mutt_str_startswith(str, name);
    if ((len != 0) && (str[len] == '='))
    {
      FREE(&(*envp)[match]);
      // Move down the later entries
      memmove(&(*envp)[match], &(*envp)[match + 1], (count - match) * sizeof(char *));
      // Shrink the array
      mutt_mem_realloc(envp, count * sizeof(char *));
      return true;
    }
  }
  return false;
}
