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

#ifndef MUTT_LIB_FILE_H
#define MUTT_LIB_FILE_H

#include "config.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

struct Buffer;
struct stat;
extern const char filename_safe_chars[];

/* Flags for mutt_file_read_line() */
typedef uint8_t ReadLineFlags;             ///< Flags for mutt_file_read_line(), e.g. #MUTT_RL_CONT
#define MUTT_RL_NO_FLAGS        0  ///< No flags are set
#define MUTT_RL_CONT      (1 << 0) ///< \-continuation
#define MUTT_RL_EOL       (1 << 1) ///< don't strip `\n` / `\r\n`

#ifdef HAVE_STRUCT_TIMESPEC
struct timespec;
#else
/**
 * struct timespec - Time value with nanosecond precision
 */
struct timespec
{
  time_t tv_sec; ///< Number of seconds since the epoch
  long tv_nsec;  ///< Number of nanosecond, on top
};
#endif

/**
 * enum MuttStatType - Flags for mutt_file_get_stat_timespec
 *
 * These represent filesystem timestamps returned by stat()
 */
enum MuttStatType
{
  MUTT_STAT_ATIME, ///< File/dir's atime - last accessed time
  MUTT_STAT_MTIME, ///< File/dir's mtime - last modified time
  MUTT_STAT_CTIME, ///< File/dir's ctime - creation time
};

/**
 * struct MuttFileIter - State record for mutt_file_iter_line()
 */
struct MuttFileIter
{
  char *line;   ///< the line data
  size_t size;  ///< allocated size of line data
  int line_num; ///< line number
};

/**
 * @defgroup mutt_file_map_api File Mapping API
 *
 * Prototype for a text handler function for mutt_file_map_lines()
 *
 * @param line      Line of text read
 * @param line_num  Line number
 * @param user_data Data to pass to the callback function
 * @retval true  Read was successful
 * @retval false Abort the reading and free the string
 */
typedef bool (*mutt_file_map_t)(char *line, int line_num, void *user_data);

int         mutt_file_check_empty(const char *path);
int         mutt_file_chmod(const char *path, mode_t mode);
int         mutt_file_chmod_add(const char *path, mode_t mode);
int         mutt_file_chmod_add_stat(const char *path, mode_t mode, struct stat *st);
int         mutt_file_chmod_rm(const char *path, mode_t mode);
int         mutt_file_chmod_rm_stat(const char *path, mode_t mode, struct stat *st);
int         mutt_file_copy_bytes(FILE *fp_in, FILE *fp_out, size_t size);
int         mutt_file_copy_stream(FILE *fp_in, FILE *fp_out);
time_t      mutt_file_decrease_mtime(const char *fp, struct stat *st);
void        mutt_file_expand_fmt(struct Buffer *dest, const char *fmt, const char *src);
void        mutt_file_expand_fmt_quote(char *dest, size_t destlen, const char *fmt, const char *src);
int         mutt_file_fclose(FILE **fp);
FILE *      mutt_file_fopen(const char *path, const char *mode);
int         mutt_file_fsync_close(FILE **fp);
long        mutt_file_get_size(const char *path);
long        mutt_file_get_size_fp(FILE* fp);
void        mutt_file_get_stat_timespec(struct timespec *dest, struct stat *st, enum MuttStatType type);
bool        mutt_file_iter_line(struct MuttFileIter *iter, FILE *fp, ReadLineFlags flags);
int         mutt_file_lock(int fd, bool excl, bool timeout);
bool        mutt_file_map_lines(mutt_file_map_t func, void *user_data, FILE *fp, ReadLineFlags flags);
int         mutt_file_mkdir(const char *path, mode_t mode);
FILE *      mutt_file_mkstemp_full(const char *file, int line, const char *func);
#define     mutt_file_mkstemp() mutt_file_mkstemp_full(__FILE__, __LINE__, __func__)
int         mutt_file_open(const char *path, uint32_t flags);
size_t      mutt_file_quote_filename(const char *filename, char *buf, size_t buflen);
char *      mutt_file_read_keyword(const char *file, char *buf, size_t buflen);
char *      mutt_file_read_line(char *line, size_t *size, FILE *fp, int *line_num, ReadLineFlags flags);
int         mutt_file_rename(const char *oldfile, const char *newfile);
int         mutt_file_rmtree(const char *path);
const char *mutt_file_rotate(const char *path, int num);
int         mutt_file_safe_rename(const char *src, const char *target);
void        mutt_file_sanitize_filename(char *path, bool slash);
int         mutt_file_sanitize_regex(struct Buffer *dest, const char *src);
bool        mutt_file_seek(FILE *fp, LOFF_T offset, int whence);
void        mutt_file_set_mtime(const char *from, const char *to);
int         mutt_file_stat_compare(struct stat *st1, enum MuttStatType st1_type, struct stat *st2, enum MuttStatType st2_type);
int         mutt_file_stat_timespec_compare(struct stat *st, enum MuttStatType type, struct timespec *b);
int         mutt_file_symlink(const char *oldpath, const char *newpath);
int         mutt_file_timespec_compare(struct timespec *a, struct timespec *b);
void        mutt_file_touch_atime(int fd);
void        mutt_file_unlink(const char *s);
void        mutt_file_unlink_empty(const char *path);
int         mutt_file_unlock(int fd);
void        mutt_file_resolve_symlink(struct Buffer *buf);

void        mutt_buffer_quote_filename(struct Buffer *buf, const char *filename, bool add_outer);
void        mutt_buffer_file_expand_fmt_quote(struct Buffer *dest, const char *fmt, const char *src);

#endif /* MUTT_LIB_FILE_H */
