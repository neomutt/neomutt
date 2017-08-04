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

#include "config.h"
#include "parameter.h"

void mutt_free_parameter(struct Parameter **p)
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

char *mutt_get_parameter(const char *s, struct Parameter *p)
{
  for (; p; p = p->next)
    if (mutt_strcasecmp(s, p->attribute) == 0)
      return p->value;

  return NULL;
}

void mutt_set_parameter(const char *attribute, const char *value, struct Parameter **p)
{
  struct Parameter *q = NULL;

  if (!value)
  {
    mutt_delete_parameter(attribute, p);
    return;
  }

  for (q = *p; q; q = q->next)
  {
    if (mutt_strcasecmp(attribute, q->attribute) == 0)
    {
      mutt_str_replace(&q->value, value);
      return;
    }
  }

  q = mutt_new_parameter();
  q->attribute = safe_strdup(attribute);
  q->value = safe_strdup(value);
  q->next = *p;
  *p = q;
}

void mutt_delete_parameter(const char *attribute, struct Parameter **p)
{
  struct Parameter *q = NULL;

  for (q = *p; q; p = &q->next, q = q->next)
  {
    if (mutt_strcasecmp(attribute, q->attribute) == 0)
    {
      *p = q->next;
      q->next = NULL;
      mutt_free_parameter(&q);
      return;
    }
  }
}

