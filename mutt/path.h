/**
 * @file
 * Path manipulation functions
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

#ifndef _MUTT_PATH_H
#define _MUTT_PATH_H

#include <stdbool.h>
#include <stdio.h>

bool mutt_path_tidy       (char *buf);
bool mutt_path_tidy_dotdot(char *buf);
bool mutt_path_tidy_slash (char *buf);

bool mutt_path_canon (char *buf, size_t buflen, const char *homedir);
bool mutt_path_pretty(char *buf, size_t buflen, const char *homedir);

#endif /* _MUTT_PATH_H */
