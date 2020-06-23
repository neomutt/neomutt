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
 * @page email_parameter Store attributes associated with a MIME part
 *
 * Store attributes associated with a MIME part
 */

#include "config.h"
#include <stddef.h>
#include <stdbool.h>
#include "mutt/lib.h"
#include "parameter.h"

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
 * @param[out] p Parameter to free
 */
void mutt_param_free_one(struct Parameter **p)
{
  if (!p || !*p)
    return;
  FREE(&(*p)->attribute);
  FREE(&(*p)->value);
  FREE(p);
}

/**
 * mutt_param_free - Free a ParameterList
 * @param pl ParameterList to free
 */
void mutt_param_free(struct ParameterList *pl)
{
  if (!pl)
    return;

  struct Parameter *np = TAILQ_FIRST(pl);
  struct Parameter *next = NULL;
  while (np)
  {
    next = TAILQ_NEXT(np, entries);
    mutt_param_free_one(&np);
    np = next;
  }
  TAILQ_INIT(pl);
}

/**
 * mutt_param_get - Find a matching Parameter
 * @param pl ParameterList
 * @param s  String to match
 * @retval ptr Matching Parameter
 * @retval NULL No match
 */
char *mutt_param_get(const struct ParameterList *pl, const char *s)
{
  if (!pl)
    return NULL;

  struct Parameter *np = NULL;
  TAILQ_FOREACH(np, pl, entries)
  {
    if (mutt_istr_equal(s, np->attribute))
      return np->value;
  }

  return NULL;
}

/**
 * mutt_param_set - Set a Parameter
 * @param[in]  pl        ParameterList
 * @param[in]  attribute Attribute to match
 * @param[in]  value     Value to set
 *
 * @note If value is NULL, the Parameter will be deleted
 *
 * @note If a matching Parameter isn't found a new one will be allocated.
 *       The new Parameter will be inserted at the front of the list.
 */
void mutt_param_set(struct ParameterList *pl, const char *attribute, const char *value)
{
  if (!pl)
    return;

  if (!value)
  {
    mutt_param_delete(pl, attribute);
    return;
  }

  struct Parameter *np = NULL;
  TAILQ_FOREACH(np, pl, entries)
  {
    if (mutt_istr_equal(attribute, np->attribute))
    {
      mutt_str_replace(&np->value, value);
      return;
    }
  }

  np = mutt_param_new();
  np->attribute = mutt_str_dup(attribute);
  np->value = mutt_str_dup(value);
  TAILQ_INSERT_HEAD(pl, np, entries);
}

/**
 * mutt_param_delete - Delete a matching Parameter
 * @param[in] pl        ParameterList
 * @param[in] attribute Attribute to match
 */
void mutt_param_delete(struct ParameterList *pl, const char *attribute)
{
  if (!pl)
    return;

  struct Parameter *np = NULL;
  TAILQ_FOREACH(np, pl, entries)
  {
    if (mutt_istr_equal(attribute, np->attribute))
    {
      TAILQ_REMOVE(pl, np, entries);
      mutt_param_free_one(&np);
      return;
    }
  }
}

/**
 * mutt_param_cmp_strict - Strictly compare two ParameterLists
 * @param pl1 First parameter
 * @param pl2 Second parameter
 * @retval true Parameters are strictly identical
 */
bool mutt_param_cmp_strict(const struct ParameterList *pl1, const struct ParameterList *pl2)
{
  if (!pl1 && !pl2)
    return false;

  if ((pl1 == NULL) ^ (pl2 == NULL))
    return true;

  struct Parameter *np1 = TAILQ_FIRST(pl1);
  struct Parameter *np2 = TAILQ_FIRST(pl2);

  while (np1 && np2)
  {
    if (!mutt_str_equal(np1->attribute, np2->attribute) ||
        !mutt_str_equal(np1->value, np2->value))
    {
      return false;
    }

    np1 = TAILQ_NEXT(np1, entries);
    np2 = TAILQ_NEXT(np2, entries);
  }

  if (np1 || np2)
    return false;

  return true;
}
