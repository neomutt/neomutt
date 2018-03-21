/**
 * @file
 * Store attributes associated with a MIME part
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

/**
 * @page parameter Store attributes associated with a MIME part
 *
 * Store attributes associated with a MIME part
 */

#include "config.h"
#include <stddef.h>
#include "parameter.h"
#include "memory.h"
#include "queue.h"
#include "string2.h"

/**
 * mutt_param_new - Create a new Parameter
 * @retval ptr Newly allocated Parameter
 */
struct Parameter *mutt_param_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct Parameter));
}

/**
 * mutt_param_free_one - Free a Parameter
 * @param p Parameter to free
 */
void mutt_param_free_one(struct Parameter **p)
{
  FREE(&(*p)->attribute);
  FREE(&(*p)->value);
  FREE(p);
}

/**
 * mutt_param_free - Free a ParameterList
 * @param p ParameterList to free
 */
void mutt_param_free(struct ParameterList *p)
{
  if (!p)
    return;

  struct Parameter *np = TAILQ_FIRST(p), *next = NULL;
  while (np)
  {
    next = TAILQ_NEXT(np, entries);
    mutt_param_free_one(&np);
    np = next;
  }
  TAILQ_INIT(p);
}

/**
 * mutt_param_get - Find a matching Parameter
 * @param p ParameterList
 * @param s String to match
 * @retval ptr Matching Parameter
 * @retval NULL No match
 */
char *mutt_param_get(const struct ParameterList *p, const char *s)
{
  if (!p)
    return NULL;

  struct Parameter *np;
  TAILQ_FOREACH(np, p, entries)
  {
    if (mutt_str_strcasecmp(s, np->attribute) == 0)
      return np->value;
  }

  return NULL;
}

/**
 * mutt_param_set - Set a Parameter
 * @param[in]  p         ParameterList
 * @param[in]  attribute Attribute to match
 * @param[in]  value     Value to set
 *
 * @note If value is NULL, the Parameter will be deleted
 *
 * @note If a matching Parameter isn't found a new one will be allocated.
 *       The new Parameter will be inserted at the front of the list.
 */
void mutt_param_set(struct ParameterList *p, const char *attribute, const char *value)
{
  if (!p)
    return;

  if (!value)
  {
    mutt_param_delete(p, attribute);
    return;
  }

  struct Parameter *np;
  TAILQ_FOREACH(np, p, entries)
  {
    if (mutt_str_strcasecmp(attribute, np->attribute) == 0)
    {
      mutt_str_replace(&np->value, value);
      return;
    }
  }

  np = mutt_param_new();
  np->attribute = mutt_str_strdup(attribute);
  np->value = mutt_str_strdup(value);
  TAILQ_INSERT_HEAD(p, np, entries);
}

/**
 * mutt_param_delete - Delete a matching Parameter
 * @param[in]  p         ParameterList
 * @param[in]  attribute Attribute to match
 */
void mutt_param_delete(struct ParameterList *p, const char *attribute)
{
  if (!p)
    return;

  struct Parameter *np;
  TAILQ_FOREACH(np, p, entries)
  {
    if (mutt_str_strcasecmp(attribute, np->attribute) == 0)
    {
      TAILQ_REMOVE(p, np, entries);
      mutt_param_free_one(&np);
      return;
    }
  }
}

/**
 * mutt_param_cmp_strict - Strictly compare two ParameterLists
 * @param p1 First parameter
 * @param p2 Second parameter
 * @retval true Parameters are strictly identical
 */
int mutt_param_cmp_strict(const struct ParameterList *p1, const struct ParameterList *p2)
{
  if (!p1 && !p2)
  {
    return 0;
  }

  if ((p1 == NULL) ^ (p2 == NULL))
  {
    return 1;
  }

  struct Parameter *np1 = TAILQ_FIRST(p1);
  struct Parameter *np2 = TAILQ_FIRST(p2);

  while (np1 && np2)
  {
    if ((mutt_str_strcmp(np1->attribute, np2->attribute) != 0) ||
        (mutt_str_strcmp(np1->value, np2->value) != 0))
    {
      return 0;
    }

    np1 = TAILQ_NEXT(np1, entries);
    np2 = TAILQ_NEXT(np2, entries);
  }

  if (np1 || np2)
    return 0;

  return 1;
}
