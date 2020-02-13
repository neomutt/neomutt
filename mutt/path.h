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

#ifndef MUTT_LIB_PATH_H
#define MUTT_LIB_PATH_H

#include <stdbool.h>
#include <stdio.h>

struct Buffer;

bool        mutt_path_abbr_folder(char *buf, size_t buflen, const char *folder);
bool        mutt_path2_abbr_folder(const char *path, const char *folder, char **abbr);
const char *mutt_path_basename(const char *f);
bool        mutt_path_canon(char *buf, size_t buflen, const char *homedir, bool is_dir);
bool        mutt_path_canon2(const char *orig, char **canon);
char *      mutt_path_concat(char *d, const char *dir, const char *fname, size_t l);
char *      mutt_path_dirname(const char *path);
char *      mutt_path_escape(const char *src);
const char *mutt_path_getcwd(struct Buffer *cwd);
bool        mutt_path_parent(char *buf, size_t buflen);
bool        mutt_path_pretty(char *buf, size_t buflen, const char *homedir, bool is_dir);
bool        mutt_path2_pretty(const char *path, const char *homedir, char **pretty);
size_t      mutt_path_realpath(char *buf);
bool        mutt_path_tidy(char *buf, bool is_dir);
char *      mutt_path_tidy2(const char *orig, bool is_dir);
bool        mutt_path_tidy_dotdot(char *buf);
bool        mutt_path_tidy_slash(char *buf, bool is_dir);
bool        mutt_path_tilde(char *buf, size_t buflen, const char *homedir);
bool        mutt_path_to_absolute(char *path, const char *reference);

#endif /* MUTT_LIB_PATH_H */
