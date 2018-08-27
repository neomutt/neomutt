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

#include <stdbool.h>

#ifndef _MUTT_ENVLIST_H
#define _MUTT_ENVLIST_H

void   mutt_envlist_free(void);
char **mutt_envlist_getlist(void);
void   mutt_envlist_init(char *envp[]);
bool   mutt_envlist_set(const char *name, const char *value, bool overwrite);
bool   mutt_envlist_unset(const char *name);

#endif /* _MUTT_ENVLIST_H */
