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

#ifndef MUTT_ENVLIST_H
#define MUTT_ENVLIST_H

#include <stdbool.h>

void   envlist_free (char ***envp);
char **envlist_init (char  **envp);
bool   envlist_set  (char ***envp, const char *name, const char *value, bool overwrite);
bool   envlist_unset(char ***envp, const char *name);

#endif /* MUTT_ENVLIST_H */
