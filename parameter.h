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

/**
 * struct Parameter - Attribute associated with a MIME part
 */
struct Parameter
{
  char *attribute;
  char *value;
  struct Parameter *next;
};

struct Parameter *mutt_param_new(void);
int               mutt_param_cmp_strict(const struct Parameter *p1, const struct Parameter *p2);
void              mutt_param_delete(const char *attribute, struct Parameter **p);
void              mutt_param_free(struct Parameter **p);
char *            mutt_param_get(const char *s, struct Parameter *p);
void              mutt_param_set(const char *attribute, const char *value, struct Parameter **p);

#endif /* _MUTT_PARAMETER_H */
