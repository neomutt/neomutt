/**
 * @file
 * Some very miscellaneous functions
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005,2007 Thomas Roessler <roessler@does-not-exist.org>
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

/* mutt functions which are generally useful. */

#ifndef _MUTT_LIB_H
#define _MUTT_LIB_H

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#undef MAX
#undef MIN
#define MAX(a, b) ((a) < (b) ? (b) : (a))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* Use this with care.  If the compiler can't see the array
 * definition, it obviously won't produce a correct result. */
#define mutt_array_size(x) (sizeof(x) / sizeof((x)[0]))

/* For mutt_simple_format() justifications */
/* Making left 0 and center -1 is of course completely nonsensical, but
 * it retains compatibility for any patches that call mutt_simple_format.
 * Once patches are updated to use FMT_*, these can be made sane. */
#define FMT_LEFT   0
#define FMT_RIGHT  1
#define FMT_CENTER -1

#ifdef DEBUG
extern char debugfilename[_POSIX_PATH_MAX];
extern FILE *debugfile;
extern int debuglevel;
extern char *debugfile_cmdline;
extern int debuglevel_cmdline;
#endif

/* Exit values used in send_msg() */
#define S_ERR 127
#define S_BKG 126

/* Flags for mutt_read_line() */
#define MUTT_CONT (1 << 0) /**< \-continuation */
#define MUTT_EOL  (1 << 1) /**< don't strip `\n` / `\r\n` */

/* The actual library functions. */

FILE *safe_fopen(const char *path, const char *mode);

char *mutt_concatn_path(char *dst, size_t dstlen, const char *dir,
                        size_t dirlen, const char *fname, size_t fnamelen);
char *mutt_concat_path(char *d, const char *dir, const char *fname, size_t l);
char *mutt_read_line(char *s, size_t *size, FILE *fp, int *line, int flags);

const char *mutt_basename(const char *f);

int mutt_copy_stream(FILE *fin, FILE *fout);
int mutt_copy_bytes(FILE *in, FILE *out, size_t size);
int mutt_rx_sanitize_string(char *dest, size_t destlen, const char *src);
int safe_asprintf(char **, const char *, ...);
int safe_open(const char *path, int flags);
int safe_rename(const char *src, const char *target);
int safe_symlink(const char *oldpath, const char *newpath);
int safe_fclose(FILE **f);
int safe_fsync_close(FILE **f);
int mutt_rmtree(const char *path);
int mutt_mkdir(const char *path, mode_t mode);

size_t mutt_quote_filename(char *d, size_t l, const char *f);

void mutt_nocurses_error(const char *, ...);
void mutt_sanitize_filename(char *f, short slash);
void mutt_unlink(const char *s);
int mutt_inbox_cmp(const char *a, const char *b);

const char *mutt_strsysexit(int e);

#endif /* _MUTT_LIB_H */
