/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2000 Thomas Roessler <roessler@does-not-exist.org>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 59 Temple Place - Suite 330,
 *     Boston, MA  02111, USA.
 */ 

/* mutt functions which are generally useful. */

#ifndef _LIB_H
# define _LIB_H

# include "config.h"

# include <stdio.h>
# include <string.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h> /* needed for SEEK_SET */
# endif
# include <sys/types.h>
# include <sys/stat.h>
# include <time.h>
# include <limits.h>
# include <stdarg.h>
# include <signal.h>

# ifndef _POSIX_PATH_MAX
#  include <posix1_lim.h>
# endif

# ifdef ENABLE_NLS
#  include <libintl.h>
# define _(a) (gettext (a))
#  ifdef gettext_noop
#   define N_(a) gettext_noop (a)
#  else
#   define N_(a) (a)
#  endif
# else
#  define _(a) (a)
#  define N_(a) a
# endif

# define TRUE 1
# define FALSE 0

# define HUGE_STRING	5120
# define LONG_STRING     1024
# define STRING          256
# define SHORT_STRING    128

/*
 * Create a format string to be used with scanf.
 * To use it, write, for instance, MUTT_FORMAT(HUGE_STRING).
 * 
 * See K&R 2nd ed, p. 231 for an explanation.
 */
# define _MUTT_FORMAT_2(a,b)	"%" a  b
# define _MUTT_FORMAT_1(a, b)	_MUTT_FORMAT_2(#a, b)
# define MUTT_FORMAT(a)		_MUTT_FORMAT_1(a, "s")
# define MUTT_FORMAT2(a,b)	_MUTT_FORMAT_1(a, b)


# define FREE(x) safe_free((void **)x)
# define NONULL(x) x?x:""
# define ISSPACE(c) isspace((unsigned char)c)
# define strfcpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)=0

# undef MAX
# undef MIN
# define MAX(a,b) ((a) < (b) ? (b) : (a))
# define MIN(a,b) ((a) < (b) ? (a) : (b))


#define FOREVER while (1)

/* this macro must check for *c == 0 since isspace(0) has unreliable behavior
   on some systems */
# define SKIPWS(c) while (*(c) && isspace ((unsigned char) *(c))) c++;

/*
 * These functions aren't defined in lib.c, but
 * they are used there.
 *
 * A non-mutt "implementation" (ahem) can be found in extlib.c.
 */

# ifndef _EXTLIB_C
extern void (*mutt_error) (const char *, ...);
# endif
void mutt_exit (int);

/* The actual library functions. */

FILE *safe_fopen (const char *, const char *);

char *mutt_concat_path (char *, const char *, const char *, size_t);
char *mutt_read_line (char *, size_t *, FILE *, int *);
char *mutt_skip_whitespace (char *);
char *mutt_strlower (char *);
char *mutt_substrcpy (char *, const char *, const char *, size_t);
char *mutt_substrdup (const char *, const char *);
char *safe_strdup (const char *);

const char *mutt_stristr (const char *, const char *);

int mutt_copy_stream (FILE *, FILE *);
int mutt_copy_bytes (FILE *, FILE *, size_t);
int mutt_rx_sanitize_string (char *, size_t, const char *);
int mutt_strcasecmp (const char *, const char *);
int mutt_strcmp (const char *, const char *);
int mutt_strncasecmp (const char *, const char *, size_t);
int mutt_strncmp (const char *, const char *, size_t);
int mutt_strcoll (const char *, const char *);
int safe_open (const char *, int);
int safe_symlink (const char *, const char *);
int safe_rename (const char *, const char *);
int safe_fclose (FILE **);

size_t mutt_quote_filename (char *, size_t, const char *);
size_t mutt_strlen (const char *);

void *safe_calloc (size_t, size_t);
void *safe_malloc (size_t);
void mutt_nocurses_error (const char *, ...);
void mutt_remove_trailing_ws (char *);
void mutt_sanitize_filename (char *, short);
void mutt_str_replace (char **p, const char *s);
void mutt_str_adjust (char **p);
void mutt_unlink (const char *);
void safe_free (void **);
void safe_realloc (void **, size_t);

#endif
