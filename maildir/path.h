/**
 * @file
 * Maildir Path handling
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MAILDIR_PATH_H
#define MUTT_MAILDIR_PATH_H

#include "core/lib.h"

struct Buffer;
struct stat;

int              maildir_path_canon   (struct Buffer *path);
int              maildir_path_is_empty(struct Buffer *path);
enum MailboxType maildir_path_probe   (const char *path, const struct stat *st);

#endif /* MUTT_MAILDIR_PATH_H */
