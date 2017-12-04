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

#ifndef _MUTT_FILE_H
#define _MUTT_FILE_H

#include <stdio.h>
#include <sys/types.h>
#include <time.h>

struct stat;

/* Flags for mutt_file_read_line() */
#define MUTT_CONT (1 << 0) /**< \-continuation */
#define MUTT_EOL  (1 << 1) /**< don't strip `\n` / `\r\n` */

const char *mutt_file_basename(const char *f);
int         mutt_file_check_empty(const char *path);
int         mutt_file_chmod(const char *path, mode_t mode);
int         mutt_file_chmod_add(const char *path, mode_t mode);
int         mutt_file_chmod_add_stat(const char *path, mode_t mode, struct stat *st);
int         mutt_file_chmod_rm(const char *path, mode_t mode);
int         mutt_file_chmod_rm_stat(const char *path, mode_t mode, struct stat *st);
char *      mutt_file_concatn_path(char *dst, size_t dstlen, const char *dir, size_t dirlen, const char *fname, size_t fnamelen);
char *      mutt_file_concat_path(char *d, const char *dir, const char *fname, size_t l);
int         mutt_file_copy_bytes(FILE *in, FILE *out, size_t size);
int         mutt_file_copy_stream(FILE *fin, FILE *fout);
time_t      mutt_file_decrease_mtime(const char *f, struct stat *st);
const char *mutt_file_dirname(const char *p);
int         mutt_file_fclose(FILE **f);
FILE *      mutt_file_fopen(const char *path, const char *mode);
int         mutt_file_fsync_close(FILE **f);
int         mutt_file_lock(int fd, int excl, int timeout);
int         mutt_file_mkdir(const char *path, mode_t mode);
int         mutt_file_open(const char *path, int flags);
size_t      mutt_file_quote_filename(char *d, size_t l, const char *f);
char *      mutt_file_read_keyword(const char *file, char *buffer, size_t buflen);
char *      mutt_file_read_line(char *s, size_t *size, FILE *fp, int *line, int flags);
int         mutt_file_rename(char *oldfile, char *newfile);
int         mutt_file_rmtree(const char *path);
int         mutt_file_safe_rename(const char *src, const char *target);
void        mutt_file_sanitize_filename(char *f, short slash);
int         mutt_file_sanitize_regex(char *dest, size_t destlen, const char *src);
void        mutt_file_set_mtime(const char *from, const char *to);
int         mutt_file_symlink(const char *oldpath, const char *newpath);
int         mutt_file_to_absolute_path(char *path, const char *reference);
void        mutt_file_touch_atime(int f);
void        mutt_file_unlink(const char *s);
void        mutt_file_unlink_empty(const char *path);
int         mutt_file_unlock(int fd);

#endif /* _MUTT_FILE_H */
