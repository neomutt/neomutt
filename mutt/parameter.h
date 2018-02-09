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

#ifndef _MUTT_PARAMETER_H
#define _MUTT_PARAMETER_H

#include "queue.h"

/**
 * struct ParameterList - List of parameters.
 */
TAILQ_HEAD(ParameterList, Parameter);

/**
 * struct Parameter - Attribute associated with a MIME part
 */
struct Parameter
{
  char *attribute;
  char *value;
  TAILQ_ENTRY(Parameter) entries;
};

struct Parameter *mutt_param_new(void);
int               mutt_param_cmp_strict(const struct ParameterList *p1, const struct ParameterList *p2);
void              mutt_param_delete(struct ParameterList *p, const char *attribute);
void              mutt_param_free(struct ParameterList *p);
void              mutt_param_free_one(struct Parameter **p);
char *            mutt_param_get(const struct ParameterList *p, const char *s);
void              mutt_param_set(struct ParameterList *p, const char *attribute, const char *value);

#endif /* _MUTT_PARAMETER_H */
