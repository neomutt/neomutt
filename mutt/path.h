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

#ifndef MUTT_MUTT_PATH_H
#define MUTT_MUTT_PATH_H

#include <stdbool.h>
#include <stdio.h>

struct Buffer;

bool        mutt_path_abbr_folder(struct Buffer *path, const char *folder);
const char *mutt_path_basename(const char *path);
bool        mutt_path_canon(struct Buffer *path, const char *homedir, bool is_dir);
char *      mutt_path_concat(char *dest, const char *dir, const char *file, size_t dlen);
char *      mutt_path_dirname(const char *path);
char *      mutt_path_escape(const char *src);
const char *mutt_path_getcwd(struct Buffer *cwd);
bool        mutt_path_parent(struct Buffer *path);
bool        mutt_path_pretty(struct Buffer *path, const char *homedir, bool is_dir);
size_t      mutt_path_realpath(struct Buffer *path);
bool        mutt_path_tidy(char *buf, bool is_dir);
bool        mutt_path_tidy_dotdot(char *buf);
bool        mutt_path_tidy_slash(char *buf, bool is_dir);
bool        mutt_path_tilde(struct Buffer *path, const char *homedir);
bool        mutt_path_to_absolute(char *path, const char *reference);

#endif /* MUTT_MUTT_PATH_H */
