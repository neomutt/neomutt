/**
 * @file
 * Some very miscellaneous functions
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2004,2006-2007 Thomas Roessler <roessler@does-not-exist.org>
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

/*
 * This file used to contain some more functions, namely those
 * which are now in muttlib.c.  They have been removed, so we have
 * some of our "standard" functions in external programs, too.
 */

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "lib.h"

#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#else /* Make sure EX_OK is defined <philiph@pobox.com> */
#define EX_OK 0
#endif

/**
 * struct SysExits - Lookup table of error messages
 */
static const struct SysExits
{
  int v;
  const char *str;
} sysexits_h[] = {
#ifdef EX_USAGE
  { 0xff & EX_USAGE, "Bad usage." },
#endif
#ifdef EX_DATAERR
  { 0xff & EX_DATAERR, "Data format error." },
#endif
#ifdef EX_NOINPUT
  { 0xff & EX_NOINPUT, "Cannot open input." },
#endif
#ifdef EX_NOUSER
  { 0xff & EX_NOUSER, "User unknown." },
#endif
#ifdef EX_NOHOST
  { 0xff & EX_NOHOST, "Host unknown." },
#endif
#ifdef EX_UNAVAILABLE
  { 0xff & EX_UNAVAILABLE, "Service unavailable." },
#endif
#ifdef EX_SOFTWARE
  { 0xff & EX_SOFTWARE, "Internal error." },
#endif
#ifdef EX_OSERR
  { 0xff & EX_OSERR, "Operating system error." },
#endif
#ifdef EX_OSFILE
  { 0xff & EX_OSFILE, "System file missing." },
#endif
#ifdef EX_CANTCREAT
  { 0xff & EX_CANTCREAT, "Can't create output." },
#endif
#ifdef EX_IOERR
  { 0xff & EX_IOERR, "I/O error." },
#endif
#ifdef EX_TEMPFAIL
  { 0xff & EX_TEMPFAIL, "Deferred." },
#endif
#ifdef EX_PROTOCOL
  { 0xff & EX_PROTOCOL, "Remote protocol error." },
#endif
#ifdef EX_NOPERM
  { 0xff & EX_NOPERM, "Insufficient permission." },
#endif
#ifdef EX_CONFIG
  { 0xff & EX_NOPERM, "Local configuration error." },
#endif
  { S_ERR, "Exec error." },
  { -1, NULL },
};

void mutt_nocurses_error(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
}

void *safe_calloc(size_t nmemb, size_t size)
{
  void *p = NULL;

  if (!nmemb || !size)
    return NULL;

  if (((size_t) -1) / nmemb <= size)
  {
    mutt_error(_("Integer overflow -- can't allocate memory!"));
    sleep(1);
    mutt_exit(1);
  }

  if (!(p = calloc(nmemb, size)))
  {
    mutt_error(_("Out of memory!"));
    sleep(1);
    mutt_exit(1);
  }
  return p;
}

void *safe_malloc(size_t siz)
{
  void *p = NULL;

  if (siz == 0)
    return 0;
  if ((p = malloc(siz)) == NULL)
  {
    mutt_error(_("Out of memory!"));
    sleep(1);
    mutt_exit(1);
  }
  return p;
}

void safe_realloc(void *ptr, size_t siz)
{
  void *r = NULL;
  void **p = (void **) ptr;

  if (siz == 0)
  {
    if (*p)
    {
      free(*p);
      *p = NULL;
    }
    return;
  }

  r = realloc(*p, siz);
  if (!r)
  {
    mutt_error(_("Out of memory!"));
    sleep(1);
    mutt_exit(1);
  }

  *p = r;
}

void safe_free(void *ptr)
{
  if (!ptr)
    return;
  void **p = (void **) ptr;
  if (*p)
  {
    free(*p);
    *p = 0;
  }
}

int safe_fclose(FILE **f)
{
  int r = 0;

  if (*f)
    r = fclose(*f);

  *f = NULL;
  return r;
}

int safe_fsync_close(FILE **f)
{
  int r = 0;

  if (*f)
  {
    if (fflush(*f) || fsync(fileno(*f)))
    {
      int save_errno = errno;
      r = -1;
      safe_fclose(f);
      errno = save_errno;
    }
    else
      r = safe_fclose(f);
  }

  return r;
}

char *safe_strdup(const char *s)
{
  char *p = NULL;
  size_t l;

  if (!s || !*s)
    return 0;
  l = strlen(s) + 1;
  p = safe_malloc(l);
  memcpy(p, s, l);
  return p;
}

char *safe_strcat(char *d, size_t l, const char *s)
{
  char *p = d;

  if (!l)
    return d;

  l--; /* Space for the trailing '\0'. */

  for (; *d && l; l--)
    d++;
  for (; *s && l; l--)
    *d++ = *s++;

  *d = '\0';

  return p;
}

char *safe_strncat(char *d, size_t l, const char *s, size_t sl)
{
  char *p = d;

  if (!l)
    return d;

  l--; /* Space for the trailing '\0'. */

  for (; *d && l; l--)
    d++;
  for (; *s && l && sl; l--, sl--)
    *d++ = *s++;

  *d = '\0';

  return p;
}


void mutt_str_replace(char **p, const char *s)
{
  FREE(p);
  *p = safe_strdup(s);
}

void mutt_str_adjust(char **p)
{
  if (!p || !*p)
    return;
  safe_realloc(p, strlen(*p) + 1);
}

/**
 * mutt_strlower - convert all characters in the string to lowercase
 */
char *mutt_strlower(char *s)
{
  char *p = s;

  while (*p)
  {
    *p = tolower((unsigned char) *p);
    p++;
  }

  return s;
}

/**
 * mutt_strchrnul - find first occurrence of character in string
 * @param s Haystack
 * @param c Needle
 * @retval ptr First occurrence if found or to the NULL character
 *
 * This function is like GNU's strchrnul, which is similar to the standard
 * strchr function: it looks for the c character in the NULL-terminated string
 * s and returns a pointer to its location. If c is not in s, instead of
 * returning NULL like its standard counterpart, this function returns a
 * pointer to the terminating NULL character.
 */
const char *mutt_strchrnul(const char *s, char c)
{
  for (; *s && (*s != c); s++)
    ;
  return s;
}

void mutt_unlink(const char *s)
{
  int fd;
  int flags;
  FILE *f = NULL;
  struct stat sb, sb2;
  char buf[2048];

/* Defend against symlink attacks */

#ifdef O_NOFOLLOW
  flags = O_RDWR | O_NOFOLLOW;
#else
  flags = O_RDWR;
#endif

  if (lstat(s, &sb) == 0 && S_ISREG(sb.st_mode))
  {
    if ((fd = open(s, flags)) < 0)
      return;

    if ((fstat(fd, &sb2) != 0) || !S_ISREG(sb2.st_mode) ||
        (sb.st_dev != sb2.st_dev) || (sb.st_ino != sb2.st_ino))
    {
      close(fd);
      return;
    }

    if ((f = fdopen(fd, "r+")))
    {
      unlink(s);
      memset(buf, 0, sizeof(buf));
      while (sb.st_size > 0)
      {
        fwrite(buf, 1, MIN(sizeof(buf), sb.st_size), f);
        sb.st_size -= MIN(sizeof(buf), sb.st_size);
      }
      safe_fclose(&f);
    }
  }
}

int mutt_copy_bytes(FILE *in, FILE *out, size_t size)
{
  char buf[2048];
  size_t chunk;

  while (size > 0)
  {
    chunk = (size > sizeof(buf)) ? sizeof(buf) : size;
    if ((chunk = fread(buf, 1, chunk, in)) < 1)
      break;
    if (fwrite(buf, 1, chunk, out) != chunk)
    {
      return -1;
    }
    size -= chunk;
  }

  if (fflush(out) != 0)
    return -1;
  return 0;
}

int mutt_copy_stream(FILE *fin, FILE *fout)
{
  size_t l;
  char buf[LONG_STRING];

  while ((l = fread(buf, 1, sizeof(buf), fin)) > 0)
  {
    if (fwrite(buf, 1, l, fout) != l)
      return -1;
  }

  if (fflush(fout) != 0)
    return -1;
  return 0;
}

static bool compare_stat(struct stat *osb, struct stat *nsb)
{
  if (osb->st_dev != nsb->st_dev || osb->st_ino != nsb->st_ino || osb->st_rdev != nsb->st_rdev)
  {
    return false;
  }

  return true;
}

int safe_symlink(const char *oldpath, const char *newpath)
{
  struct stat osb, nsb;

  if (!oldpath || !newpath)
    return -1;

  if (unlink(newpath) == -1 && errno != ENOENT)
    return -1;

  if (oldpath[0] == '/')
  {
    if (symlink(oldpath, newpath) == -1)
      return -1;
  }
  else
  {
    char abs_oldpath[_POSIX_PATH_MAX];

    if ((getcwd(abs_oldpath, sizeof(abs_oldpath)) == NULL) ||
        (strlen(abs_oldpath) + 1 + strlen(oldpath) + 1 > sizeof(abs_oldpath)))
      return -1;

    strcat(abs_oldpath, "/");
    strcat(abs_oldpath, oldpath);
    if (symlink(abs_oldpath, newpath) == -1)
      return -1;
  }

  if (stat(oldpath, &osb) == -1 || stat(newpath, &nsb) == -1 || !compare_stat(&osb, &nsb))
  {
    unlink(newpath);
    return -1;
  }

  return 0;
}


/**
 * safe_rename - NFS-safe renaming of files
 *
 * Warning: We don't check whether src and target are equal.
 */
int safe_rename(const char *src, const char *target)
{
  struct stat ssb, tsb;

  if (!src || !target)
    return -1;

  if (link(src, target) != 0)
  {
    /*
     * Coda does not allow cross-directory links, but tells
     * us it's a cross-filesystem linking attempt.
     *
     * However, the Coda rename call is allegedly safe to use.
     *
     * With other file systems, rename should just fail when
     * the files reside on different file systems, so it's safe
     * to try it here.
     *
     */

    mutt_debug(1, "safe_rename: link (%s, %s) failed: %s (%d)\n", src, target,
               strerror(errno), errno);

    /*
     * FUSE may return ENOSYS. VFAT may return EPERM. FreeBSD's
     * msdosfs may return EOPNOTSUPP.  ENOTSUP can also appear.
     */
    if (errno == EXDEV || errno == ENOSYS || errno == EPERM
#ifdef ENOTSUP
        || errno == ENOTSUP
#endif
#ifdef EOPNOTSUPP
        || errno == EOPNOTSUPP
#endif
        )
    {
      mutt_debug(1, "safe_rename: trying rename...\n");
      if (rename(src, target) == -1)
      {
        mutt_debug(1, "safe_rename: rename (%s, %s) failed: %s (%d)\n", src,
                   target, strerror(errno), errno);
        return -1;
      }
      mutt_debug(1, "safe_rename: rename succeeded.\n");

      return 0;
    }

    return -1;
  }

  /*
   * Stat both links and check if they are equal.
   */

  if (lstat(src, &ssb) == -1)
  {
    mutt_debug(1, "safe_rename: can't stat %s: %s (%d)\n", src, strerror(errno), errno);
    return -1;
  }

  if (lstat(target, &tsb) == -1)
  {
    mutt_debug(1, "safe_rename: can't stat %s: %s (%d)\n", src, strerror(errno), errno);
    return -1;
  }

  /*
   * pretend that the link failed because the target file
   * did already exist.
   */

  if (!compare_stat(&ssb, &tsb))
  {
    mutt_debug(1, "safe_rename: stat blocks for %s and %s diverge; "
                  "pretending EEXIST.\n",
               src, target);
    errno = EEXIST;
    return -1;
  }

  /*
   * Unlink the original link.  Should we really ignore the return
   * value here? XXX
   */

  if (unlink(src) == -1)
  {
    mutt_debug(1, "safe_rename: unlink (%s) failed: %s (%d)\n", src, strerror(errno), errno);
  }


  return 0;
}


/**
 * mkwrapdir - Create a temporary directory next to a file name
 */
static int mkwrapdir(const char *path, char *newfile, size_t nflen, char *newdir, size_t ndlen)
{
  const char *basename = NULL;
  char parent[_POSIX_PATH_MAX];
  char *p = NULL;

  strfcpy(parent, NONULL(path), sizeof(parent));

  if ((p = strrchr(parent, '/')))
  {
    *p = '\0';
    basename = p + 1;
  }
  else
  {
    strfcpy(parent, ".", sizeof(parent));
    basename = path;
  }

  snprintf(newdir, ndlen, "%s/%s", parent, ".muttXXXXXX");
  if (mkdtemp(newdir) == NULL)
  {
    mutt_debug(1, "mkwrapdir: mkdtemp() failed\n");
    return -1;
  }

  if (snprintf(newfile, nflen, "%s/%s", newdir, NONULL(basename)) >= nflen)
  {
    rmdir(newdir);
    mutt_debug(1, "mkwrapdir: string was truncated\n");
    return -1;
  }
  return 0;
}

/**
 * mutt_rmtree - remove a directory and everything under it
 */
int mutt_rmtree(const char *path)
{
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char cur[_POSIX_PATH_MAX];
  struct stat statbuf;
  int rc = 0;

  if (!(dirp = opendir(path)))
  {
    mutt_debug(1, "mutt_rmtree: error opening directory %s\n", path);
    return -1;
  }
  while ((de = readdir(dirp)))
  {
    if ((strcmp(".", de->d_name) == 0) || (strcmp("..", de->d_name) == 0))
      continue;

    snprintf(cur, sizeof(cur), "%s/%s", path, de->d_name);
    /* XXX make nonrecursive version */

    if (stat(cur, &statbuf) == -1)
    {
      rc = 1;
      continue;
    }

    if (S_ISDIR(statbuf.st_mode))
      rc |= mutt_rmtree(cur);
    else
      rc |= unlink(cur);
  }
  closedir(dirp);

  rc |= rmdir(path);

  return rc;
}

static int put_file_in_place(const char *path, const char *safe_file, const char *safe_dir)
{
  int rv;

  rv = safe_rename(safe_file, path);
  unlink(safe_file);
  rmdir(safe_dir);
  return rv;
}

int safe_open(const char *path, int flags)
{
  struct stat osb, nsb;
  int fd;

  if (flags & O_EXCL)
  {
    char safe_file[_POSIX_PATH_MAX];
    char safe_dir[_POSIX_PATH_MAX];

    if (mkwrapdir(path, safe_file, sizeof(safe_file), safe_dir, sizeof(safe_dir)) == -1)
      return -1;

    if ((fd = open(safe_file, flags, 0600)) < 0)
    {
      rmdir(safe_dir);
      return fd;
    }

    /* NFS and I believe cygwin do not handle movement of open files well */
    close(fd);
    if (put_file_in_place(path, safe_file, safe_dir) == -1)
      return -1;
  }

  if ((fd = open(path, flags & ~O_EXCL, 0600)) < 0)
    return fd;

  /* make sure the file is not symlink */
  if (lstat(path, &osb) < 0 || fstat(fd, &nsb) < 0 || !compare_stat(&osb, &nsb))
  {
    close(fd);
    return -1;
  }

  return fd;
}

/**
 * safe_fopen - Call fopen() safely
 *
 * when opening files for writing, make sure the file doesn't already exist to
 * avoid race conditions.
 */
FILE *safe_fopen(const char *path, const char *mode)
{
  if (mode[0] == 'w')
  {
    int fd;
    int flags = O_CREAT | O_EXCL;

#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    if (mode[1] == '+')
      flags |= O_RDWR;
    else
      flags |= O_WRONLY;

    if ((fd = safe_open(path, flags)) < 0)
      return NULL;

    return (fdopen(fd, mode));
  }
  else
    return (fopen(path, mode));
}

static const char safe_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+@{}._-:%/";

void mutt_sanitize_filename(char *f, short slash)
{
  if (!f)
    return;

  for (; *f; f++)
  {
    if ((slash && *f == '/') || !strchr(safe_chars, *f))
      *f = '_';
  }
}

/* these characters must be escaped in regular expressions */

static const char rx_special_chars[] = "^.[$()|*+?{\\";

int mutt_rx_sanitize_string(char *dest, size_t destlen, const char *src)
{
  while (*src && --destlen > 2)
  {
    if (strchr(rx_special_chars, *src))
    {
      *dest++ = '\\';
      destlen--;
    }
    *dest++ = *src++;
  }

  *dest = '\0';

  if (*src)
    return -1;
  else
    return 0;
}

/**
 * mutt_read_line - Read a line from a file
 *
 * Read a line from ``fp'' into the dynamically allocated ``s'', increasing
 * ``s'' if necessary. The ending "\n" or "\r\n" is removed.  If a line ends
 * with "\", this char and the linefeed is removed, and the next line is read
 * too.
 */
char *mutt_read_line(char *s, size_t *size, FILE *fp, int *line, int flags)
{
  size_t offset = 0;
  char *ch = NULL;

  if (!s)
  {
    s = safe_malloc(STRING);
    *size = STRING;
  }

  while (true)
  {
    if (fgets(s + offset, *size - offset, fp) == NULL)
    {
      FREE(&s);
      return NULL;
    }
    if ((ch = strchr(s + offset, '\n')) != NULL)
    {
      if (line)
        (*line)++;
      if (flags & MUTT_EOL)
        return s;
      *ch = 0;
      if (ch > s && *(ch - 1) == '\r')
        *--ch = 0;
      if (!(flags & MUTT_CONT) || ch == s || *(ch - 1) != '\\')
        return s;
      offset = ch - s - 1;
    }
    else
    {
      int c;
      c = getc(fp); /* This is kind of a hack. We want to know if the
                        char at the current point in the input stream is EOF.
                        feof() will only tell us if we've already hit EOF, not
                        if the next character is EOF. So, we need to read in
                        the next character and manually check if it is EOF. */
      if (c == EOF)
      {
        /* The last line of fp isn't \n terminated */
        if (line)
          (*line)++;
        return s;
      }
      else
      {
        ungetc(c, fp); /* undo our damage */
        /* There wasn't room for the line -- increase ``s'' */
        offset = *size - 1; /* overwrite the terminating 0 */
        *size += STRING;
        safe_realloc(&s, *size);
      }
    }
  }
}

char *mutt_substrcpy(char *dest, const char *beg, const char *end, size_t destlen)
{
  size_t len;

  len = end - beg;
  if (len > destlen - 1)
    len = destlen - 1;
  memcpy(dest, beg, len);
  dest[len] = 0;
  return dest;
}

char *mutt_substrdup(const char *begin, const char *end)
{
  size_t len;
  char *p = NULL;

  if (end)
    len = end - begin;
  else
    len = strlen(begin);

  p = safe_malloc(len + 1);
  memcpy(p, begin, len);
  p[len] = 0;
  return p;
}

/**
 * mutt_quote_filename - Quote a filename to survive the shell's quoting rules
 *
 * From the Unix programming FAQ by way of Liviu.
 */
size_t mutt_quote_filename(char *d, size_t l, const char *f)
{
  size_t j = 0;

  if (!f)
  {
    *d = '\0';
    return 0;
  }

  /* leave some space for the trailing characters. */
  l -= 6;

  d[j++] = '\'';

  for (size_t i = 0; j < l && f[i]; i++)
  {
    if (f[i] == '\'' || f[i] == '`')
    {
      d[j++] = '\'';
      d[j++] = '\\';
      d[j++] = f[i];
      d[j++] = '\'';
    }
    else
      d[j++] = f[i];
  }

  d[j++] = '\'';
  d[j] = '\0';

  return j;
}

/* NULL-pointer aware string comparison functions */

int mutt_strcmp(const char *a, const char *b)
{
  return strcmp(NONULL(a), NONULL(b));
}

int mutt_strcasecmp(const char *a, const char *b)
{
  return strcasecmp(NONULL(a), NONULL(b));
}

int mutt_strncmp(const char *a, const char *b, size_t l)
{
  return strncmp(NONULL(a), NONULL(b), l);
}

int mutt_strncasecmp(const char *a, const char *b, size_t l)
{
  return strncasecmp(NONULL(a), NONULL(b), l);
}

size_t mutt_strlen(const char *a)
{
  return a ? strlen(a) : 0;
}

int mutt_strcoll(const char *a, const char *b)
{
  return strcoll(NONULL(a), NONULL(b));
}

const char *mutt_stristr(const char *haystack, const char *needle)
{
  const char *p = NULL, *q = NULL;

  if (!haystack)
    return NULL;
  if (!needle)
    return haystack;

  while (*(p = haystack))
  {
    for (q = needle;
         *p && *q && tolower((unsigned char) *p) == tolower((unsigned char) *q); p++, q++)
      ;
    if (!*q)
      return haystack;
    haystack++;
  }
  return NULL;
}

char *mutt_skip_whitespace(char *p)
{
  SKIPWS(p);
  return p;
}

void mutt_remove_trailing_ws(char *s)
{
  char *p = NULL;

  for (p = s + mutt_strlen(s) - 1; p >= s && ISSPACE(*p); p--)
    *p = 0;
}

/**
 * mutt_concatn_path - Concatenate directory and filename
 * @param dst      Buffer for result
 * @param dstlen   Buffer length
 * @param dir      Directory
 * @param dirlen   Directory length
 * @param fname    Filename
 * @param fnamelen Filename length
 * @retval NULL Error
 * @retval ptr  Pointer to \a dst on success
 *
 * Write the concatenated pathname (dir + "/" + fname) into dst.
 * The slash is omitted when dir or fname is of 0 length.
 */
char *mutt_concatn_path(char *dst, size_t dstlen, const char *dir,
                        size_t dirlen, const char *fname, size_t fnamelen)
{
  size_t req;
  size_t offset = 0;

  if (dstlen == 0)
    return NULL; /* probably should not mask errors like this */

  /* size check */
  req = dirlen + fnamelen + 1; /* +1 for the trailing nul */
  if (dirlen && fnamelen)
    req++; /* when both components are non-nul, we add a "/" in between */
  if (req > dstlen)
  { /* check for condition where the dst length is too short */
    /* Two options here:
     * 1) assert(0) or return NULL to signal error
     * 2) copy as much of the path as will fit
     * It doesn't appear that the return value is actually checked anywhere mutt_concat_path()
     * is called, so we should just copy set dst to nul and let the calling function fail later.
     */
    dst[0] = 0; /* safe since we bail out early if dstlen == 0 */
    return NULL;
  }

  if (dirlen)
  { /* when dir is not empty */
    memcpy(dst, dir, dirlen);
    offset = dirlen;
    if (fnamelen)
      dst[offset++] = '/';
  }
  if (fnamelen)
  { /* when fname is not empty */
    memcpy(dst + offset, fname, fnamelen);
    offset += fnamelen;
  }
  dst[offset] = 0;
  return dst;
}

char *mutt_concat_path(char *d, const char *dir, const char *fname, size_t l)
{
  const char *fmt = "%s/%s";

  if (!*fname || (*dir && dir[strlen(dir) - 1] == '/'))
    fmt = "%s%s";

  snprintf(d, l, fmt, dir, fname);
  return d;
}

const char *mutt_basename(const char *f)
{
  const char *p = strrchr(f, '/');
  if (p)
    return p + 1;
  else
    return f;
}

const char *mutt_strsysexit(int e)
{
  int i;

  for (i = 0; sysexits_h[i].str; i++)
  {
    if (e == sysexits_h[i].v)
      break;
  }

  return sysexits_h[i].str;
}

#ifdef DEBUG
char debugfilename[_POSIX_PATH_MAX];
FILE *debugfile = NULL;
int debuglevel;
char *debugfile_cmdline = NULL;
int debuglevel_cmdline;

void mutt_debug(int level, const char *fmt, ...)
{
  va_list ap;
  time_t now = time(NULL);
  static char buf[23] = "";
  static time_t last = 0;

  if (debuglevel < level || !debugfile)
    return;

  if (now > last)
  {
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    last = now;
  }
  fprintf(debugfile, "[%s] ", buf);
  va_start(ap, fmt);
  vfprintf(debugfile, fmt, ap);
  va_end(ap);
}
#endif

static int mutt_atol(const char *str, long *dst)
{
  long r;
  long *res = dst ? dst : &r;
  char *e = NULL;

  /* no input: 0 */
  if (!str || !*str)
  {
    *res = 0;
    return 0;
  }

  *res = strtol(str, &e, 10);
  if ((*res == LONG_MAX && errno == ERANGE) || (e && *e != '\0'))
    return -1;
  return 0;
}

int mutt_atos(const char *str, short *dst)
{
  int rc;
  long res;
  short tmp;
  short *t = dst ? dst : &tmp;

  *t = 0;

  if ((rc = mutt_atol(str, &res)) < 0)
    return rc;
  if ((short) res != res)
    return -2;

  *t = (short) res;
  return 0;
}

int mutt_atoi(const char *str, int *dst)
{
  int rc;
  long res;
  int tmp;
  int *t = dst ? dst : &tmp;

  *t = 0;

  if ((rc = mutt_atol(str, &res)) < 0)
    return rc;
  if ((int) res != res)
    return -2;

  *t = (int) res;
  return 0;
}

/**
 * mutt_inbox_cmp - do two folders share the same path and one is an inbox
 * @param a First path
 * @param b Second path
 * @retval -1 if a is INBOX of b
 * @retval 0 if none is INBOX
 * @retval 1 if b is INBOX for a
 *
 * This function compares two folder paths. It first looks for the position of
 * the last common '/' character. If a valid position is found and it's not the
 * last character in any of the two paths, the remaining parts of the paths are
 * compared (case insensitively) with the string "INBOX". If one of the two
 * paths matches, it's reported as being less than the other and the function
 * returns -1 (a < b) or 1 (a > b). If no paths match the requirements, the two
 * paths are considered equivalent and this function returns 0.
 *
 * Examples:
 * * mutt_inbox_cmp("/foo/bar",      "/foo/baz") --> 0
 * * mutt_inbox_cmp("/foo/bar/",     "/foo/bar/inbox") --> 0
 * * mutt_inbox_cmp("/foo/bar/sent", "/foo/bar/inbox") --> 1
 * * mutt_inbox_cmp("=INBOX",        "=Drafts") --> -1
 */
int mutt_inbox_cmp(const char *a, const char *b)
{
  /* fast-track in case the paths have been mutt_pretty_mailbox'ified */
  if (a[0] == '=' && b[0] == '=')
    return (mutt_strcasecmp(a + 1, "inbox") == 0) ?
               -1 :
               (mutt_strcasecmp(b + 1, "inbox") == 0) ? 1 : 0;

  const char *a_end = strrchr(a, '/');
  const char *b_end = strrchr(b, '/');

  /* If one path contains a '/', but not the other */
  if (!a_end ^ !b_end)
    return 0;

  /* If neither path contains a '/' */
  if (!a_end)
    return 0;

  /* Compare the subpaths */
  size_t a_len = a_end - a;
  size_t b_len = b_end - b;
  size_t min = MIN(a_len, b_len);
  int same = (a[min] == '/') && (b[min] == '/') && (a[min + 1] != '\0') &&
             (b[min + 1] != '\0') && (mutt_strncasecmp(a, b, min) == 0);

  if (!same)
    return 0;

  if (mutt_strcasecmp(&a[min + 1], "inbox") == 0)
    return -1;

  if (mutt_strcasecmp(&b[min + 1], "inbox") == 0)
    return 1;

  return 0;
}

char *strfcpy(char *dest, const char *src, size_t dlen)
{
  char *dest0 = dest;
  while ((--dlen > 0) && (*src != '\0'))
    *dest++ = *src++;

  *dest = '\0';
  return dest0;
}

/**
 * mutt_mkdir - Recursively create directories
 * @param path Directories to create
 * @param mode Permissions for final directory
 * @retval    0  Success
 * @retval   -1  Error (errno set)
 *
 * Create a directory, creating the parents if necessary. (like mkdir -p)
 *
 * @note The permissions are only set on the final directory.
 *       The permissions of any parent directories are determined by the umask.
 *       (This is how "mkdir -p" behaves)
 */
int mutt_mkdir(const char *path, mode_t mode)
{
  if (!path || !*path)
  {
    errno = EINVAL;
    return -1;
  }

  errno = 0;
  char *p = NULL;
  char _path[PATH_MAX];
  const size_t len = strlen(path);

  if (len >= sizeof(_path))
  {
    errno = ENAMETOOLONG;
    return -1;
  }

  struct stat sb;
  if ((stat(path, &sb) == 0) && S_ISDIR(sb.st_mode))
    return 0;

  /* Create a mutable copy */
  strfcpy(_path, path, sizeof(_path));

  for (p = _path + 1; *p; p++)
  {
    if (*p != '/')
      continue;

    /* Temporarily truncate the path */
    *p = '\0';

    if ((mkdir(_path, S_IRWXU | S_IRWXG | S_IRWXO) != 0) && (errno != EEXIST))
      return -1;

    *p = '/';
  }

  if ((mkdir(_path, mode) != 0) && (errno != EEXIST))
    return -1;

  return 0;
}
