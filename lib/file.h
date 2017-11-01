/**
 * @file
 * File management functions
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

#ifndef _LIB_FILE_H
#define _LIB_FILE_H

#include <stdio.h>
#include <sys/types.h>
#include <time.h>

struct stat;

/* Flags for mutt_read_line() */
#define MUTT_CONT (1 << 0) /**< \-continuation */
#define MUTT_EOL  (1 << 1) /**< don't strip `\n` / `\r\n` */

char *      file_read_keyword(const char *file, char *buffer, size_t buflen);
int         mbox_check_empty(const char *path);
const char *mutt_basename(const char *f);
char *      mutt_concatn_path(char *dst, size_t dstlen, const char *dir, size_t dirlen, const char *fname, size_t fnamelen);
char *      mutt_concat_path(char *d, const char *dir, const char *fname, size_t l);
int         mutt_copy_bytes(FILE *in, FILE *out, size_t size);
int         mutt_copy_stream(FILE *fin, FILE *fout);
time_t      mutt_decrease_mtime(const char *f, struct stat *st);
const char *mutt_dirname(const char *p);
int         mutt_lock_file(const char *path, int fd, int excl, int timeout);
int         mutt_mkdir(const char *path, mode_t mode);
size_t      mutt_quote_filename(char *d, size_t l, const char *f);
char *      mutt_read_line(char *s, size_t *size, FILE *fp, int *line, int flags);
int         mutt_regex_sanitize_string(char *dest, size_t destlen, const char *src);
int         mutt_rename_file(char *oldfile, char *newfile);
int         mutt_rmtree(const char *path);
void        mutt_sanitize_filename(char *f, short slash);
void        mutt_set_mtime(const char *from, const char *to);
void        mutt_touch_atime(int f);
void        mutt_unlink(const char *s);
void        mutt_unlink_empty(const char *path);
int         mutt_unlock_file(const char *path, int fd);
int         safe_fclose(FILE **f);
FILE *      safe_fopen(const char *path, const char *mode);
int         safe_fsync_close(FILE **f);
int         safe_open(const char *path, int flags);
int         safe_rename(const char *src, const char *target);
int         safe_symlink(const char *oldpath, const char *newpath);
int         to_absolute_path(char *path, const char *reference);

#endif /* _LIB_FILE_H */
