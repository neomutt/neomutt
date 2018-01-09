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
 *
 * | Function                | Description
 * | :---------------------- | :------------------------------
 * | mutt_param_cmp_strict() | Strictly compare two Parameters
 * | mutt_param_delete()     | Delete a matching Parameter
 * | mutt_param_free()       | Free a Parameter
 * | mutt_param_get()        | Find a matching Parameter
 * | mutt_param_new()        | Create a new Parameter
 * | mutt_param_set()        | Set a Parameter
 */

#include "config.h"
#include <stddef.h>
#include "mutt/mutt.h"
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
 * mutt_param_free - Free a Parameter
 * @param p Parameter to free
 */
void mutt_param_free(struct Parameter **p)
{
  struct Parameter *t = *p;
  struct Parameter *o = NULL;

  while (t)
  {
    FREE(&t->attribute);
    FREE(&t->value);
    o = t;
    t = t->next;
    FREE(&o);
  }
  *p = 0;
}

/**
 * mutt_param_get - Find a matching Parameter
 * @param s String to match
 * @param p Parameter list
 * @retval ptr Matching Parameter
 * @retval NULL No match
 */
char *mutt_param_get(const char *s, struct Parameter *p)
{
  for (; p; p = p->next)
    if (mutt_str_strcasecmp(s, p->attribute) == 0)
      return p->value;

  return NULL;
}

/**
 * mutt_param_set - Set a Parameter
 * @param[in]  attribute Attribute to match
 * @param[in]  value     Value to set
 * @param[out] p         Parameter that was set
 *
 * @note If value is NULL, the Parameter will be deleted
 *
 * @note If a matching Parameter isn't found a new one will be allocated.
 *       The new Parameter will be inserted at the front of the list.
 */
void mutt_param_set(const char *attribute, const char *value, struct Parameter **p)
{
  struct Parameter *q = NULL;

  if (!value)
  {
    mutt_param_delete(attribute, p);
    return;
  }

  for (q = *p; q; q = q->next)
  {
    if (mutt_str_strcasecmp(attribute, q->attribute) == 0)
    {
      mutt_str_replace(&q->value, value);
      return;
    }
  }

  q = mutt_param_new();
  q->attribute = mutt_str_strdup(attribute);
  q->value = mutt_str_strdup(value);
  q->next = *p;
  *p = q;
}

/**
 * mutt_param_delete - Delete a matching Parameter
 * @param[in]  attribute Attribute to match
 * @param[out] p         Parameter after the deleted Parameter
 */
void mutt_param_delete(const char *attribute, struct Parameter **p)
{
  for (struct Parameter *q = *p; q; p = &q->next, q = q->next)
  {
    if (mutt_str_strcasecmp(attribute, q->attribute) == 0)
    {
      *p = q->next;
      q->next = NULL;
      mutt_param_free(&q);
      return;
    }
  }
}

/**
 * mutt_param_cmp_strict - Strictly compare two Parameters
 * @param p1 First parameter
 * @param p2 Second parameter
 * @retval true Parameters are strictly identical
 */
int mutt_param_cmp_strict(const struct Parameter *p1, const struct Parameter *p2)
{
  while (p1 && p2)
  {
    if ((mutt_str_strcmp(p1->attribute, p2->attribute) != 0) ||
        (mutt_str_strcmp(p1->value, p2->value) != 0))
    {
      return 0;
    }

    p1 = p1->next;
    p2 = p2->next;
  }
  if (p1 || p2)
    return 0;

  return 1;
}
