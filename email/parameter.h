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

#ifndef MUTT_EMAIL_PARAMETER_H
#define MUTT_EMAIL_PARAMETER_H

#include <stdbool.h>
#include "mutt/lib.h"

/**
 * struct Parameter - Attribute associated with a MIME part
 */
struct Parameter
{
  char *attribute;                ///< Parameter name
  char *value;                    ///< Parameter value
  TAILQ_ENTRY(Parameter) entries; ///< Linked list
};
TAILQ_HEAD(ParameterList, Parameter);

bool              mutt_param_cmp_strict(const struct ParameterList *pl1, const struct ParameterList *pl2);
void              mutt_param_delete    (struct ParameterList *pl, const char *attribute);
void              mutt_param_free      (struct ParameterList *pl);
void              mutt_param_free_one  (struct Parameter **pl);
char *            mutt_param_get       (const struct ParameterList *pl, const char *s);
struct Parameter *mutt_param_new       (void);
void              mutt_param_set       (struct ParameterList *pl, const char *attribute, const char *value);

#endif /* MUTT_EMAIL_PARAMETER_H */
