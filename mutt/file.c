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

/**
 * @page file File management functions
 *
 * Commonly used file/dir management routines.
 */

#include "config.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "file.h"
#include "logging.h"
#include "memory.h"
#include "message.h"
#include "string2.h"

/* these characters must be escaped in regular expressions */
static const char rx_special_chars[] = "^.[$()|*+?{\\";

static const char safe_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+@{}._-:%/";

#define MAX_LOCK_ATTEMPTS 5

/* This is defined in POSIX:2008 which isn't a build requirement */
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

/**
 * compare_stat - Compare the struct stat's of two files/dirs
 * @param osb struct stat of the first file/dir
 * @param nsb struct stat of the second file/dir
 * @retval boolean
 *
 * This compares the device id (st_dev), inode number (st_ino) and special id
 * (st_rdev) of the files/dirs.
 */
static bool compare_stat(struct stat *osb, struct stat *nsb)
{
  return ((osb->st_dev == nsb->st_dev) && (osb->st_ino == nsb->st_ino) &&
          (osb->st_rdev == nsb->st_rdev));
}

/**
 * mkwrapdir - Create a temporary directory next to a file name
 * @param path    Existing filename
 * @param newfile New filename
 * @param nflen   Length of new filename
 * @param newdir  New directory name
 * @param ndlen   Length of new directory name
 * @retval  0 Success
 * @retval -1 Error
 */
static int mkwrapdir(const char *path, char *newfile, size_t nflen, char *newdir, size_t ndlen)
{
  const char *basename = NULL;
  char parent[_POSIX_PATH_MAX];
  char *p = NULL;

  mutt_str_strfcpy(parent, NONULL(path), sizeof(parent));

  p = strrchr(parent, '/');
  if (p)
  {
    *p = '\0';
    basename = p + 1;
  }
  else
  {
    mutt_str_strfcpy(parent, ".", sizeof(parent));
    basename = path;
  }

  snprintf(newdir, ndlen, "%s/%s", parent, ".muttXXXXXX");
  if (!mkdtemp(newdir))
  {
    mutt_debug(1, "mkdtemp() failed\n");
    return -1;
  }

  if (snprintf(newfile, nflen, "%s/%s", newdir, NONULL(basename)) >= nflen)
  {
    rmdir(newdir);
    mutt_debug(1, "string was truncated\n");
    return -1;
  }
  return 0;
}

/**
 * put_file_in_place - Move a file into place
 * @param path      Destination path
 * @param safe_file Current filename
 * @param safe_dir  Current directory name
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
static int put_file_in_place(const char *path, const char *safe_file, const char *safe_dir)
{
  int rc;

  rc = mutt_file_safe_rename(safe_file, path);
  unlink(safe_file);
  rmdir(safe_dir);
  return rc;
}

/**
 * mutt_file_fclose - Close a FILE handle (and NULL the pointer)
 * @param f FILE handle to close
 * @retval 0   Success
 * @retval EOF Error, see errno
 */
int mutt_file_fclose(FILE **f)
{
  int r = 0;

  if (*f)
    r = fclose(*f);

  *f = NULL;
  return r;
}

/**
 * mutt_file_fsync_close - Flush the data, before closing a file (and NULL the pointer)
 * @param f FILE handle to close
 * @retval 0   Success
 * @retval EOF Error, see errno
 */
int mutt_file_fsync_close(FILE **f)
{
  int r = 0;

  if (*f)
  {
    if (fflush(*f) || fsync(fileno(*f)))
    {
      int save_errno = errno;
      r = -1;
      mutt_file_fclose(f);
      errno = save_errno;
    }
    else
      r = mutt_file_fclose(f);
  }

  return r;
}

/**
 * mutt_file_unlink - Delete a file, carefully
 * @param s Filename
 *
 * This won't follow symlinks.
 */
void mutt_file_unlink(const char *s)
{
  struct stat sb;
  /* Defend against symlink attacks */

  const bool is_regular_file = (lstat(s, &sb) == 0) && S_ISREG(sb.st_mode);
  if (!is_regular_file)
  {
    return;
  }

  const int fd = open(s, O_RDWR | O_NOFOLLOW);
  if (fd < 0)
    return;

  struct stat sb2;
  if ((fstat(fd, &sb2) != 0) || !S_ISREG(sb2.st_mode) ||
      (sb.st_dev != sb2.st_dev) || (sb.st_ino != sb2.st_ino))
  {
    close(fd);
    return;
  }

  FILE *f = fdopen(fd, "r+");
  if (f)
  {
    unlink(s);
    char buf[2048];
    memset(buf, 0, sizeof(buf));
    while (sb.st_size > 0)
    {
      fwrite(buf, 1, MIN(sizeof(buf), sb.st_size), f);
      sb.st_size -= MIN(sizeof(buf), sb.st_size);
    }
    mutt_file_fclose(&f);
  }
}

/**
 * mutt_file_copy_bytes - Copy some content from one file to another
 * @param in   Source file
 * @param out  Destination file
 * @param size Maximum number of bytes to copy
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int mutt_file_copy_bytes(FILE *in, FILE *out, size_t size)
{
  while (size > 0)
  {
    char buf[2048];
    size_t chunk = (size > sizeof(buf)) ? sizeof(buf) : size;
    chunk = fread(buf, 1, chunk, in);
    if (chunk < 1)
      break;
    if (fwrite(buf, 1, chunk, out) != chunk)
      return -1;

    size -= chunk;
  }

  if (fflush(out) != 0)
    return -1;
  return 0;
}

/**
 * mutt_file_copy_stream - Copy the contents of one file into another
 * @param fin  Source file
 * @param fout Destination file
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int mutt_file_copy_stream(FILE *fin, FILE *fout)
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

/**
 * mutt_file_symlink - Create a symlink
 * @param oldpath Existing pathname
 * @param newpath New pathname
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int mutt_file_symlink(const char *oldpath, const char *newpath)
{
  struct stat osb, nsb;

  if (!oldpath || !newpath)
    return -1;

  if ((unlink(newpath) == -1) && (errno != ENOENT))
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
        ((strlen(abs_oldpath) + 1 + strlen(oldpath) + 1) > sizeof(abs_oldpath)))
    {
      return -1;
    }

    strcat(abs_oldpath, "/");
    strcat(abs_oldpath, oldpath);
    if (symlink(abs_oldpath, newpath) == -1)
      return -1;
  }

  if ((stat(oldpath, &osb) == -1) || (stat(newpath, &nsb) == -1) ||
      !compare_stat(&osb, &nsb))
  {
    unlink(newpath);
    return -1;
  }

  return 0;
}

/**
 * mutt_file_safe_rename - NFS-safe renaming of files
 * @param src    Original filename
 * @param target New filename
 * @retval  0 Success
 * @retval -1 Error, see errno
 *
 * Warning: We don't check whether src and target are equal.
 */
int mutt_file_safe_rename(const char *src, const char *target)
{
  struct stat ssb, tsb;

  if (!src || !target)
    return -1;

  if (link(src, target) != 0)
  {
    /* Coda does not allow cross-directory links, but tells
     * us it's a cross-filesystem linking attempt.
     *
     * However, the Coda rename call is allegedly safe to use.
     *
     * With other file systems, rename should just fail when
     * the files reside on different file systems, so it's safe
     * to try it here. */
    mutt_debug(1, "link (%s, %s) failed: %s (%d)\n", src, target, strerror(errno), errno);

    /* FUSE may return ENOSYS. VFAT may return EPERM. FreeBSD's
     * msdosfs may return EOPNOTSUPP.  ENOTSUP can also appear. */
    if (errno == EXDEV || errno == ENOSYS || errno == EPERM
#ifdef ENOTSUP
        || errno == ENOTSUP
#endif
#ifdef EOPNOTSUPP
        || errno == EOPNOTSUPP
#endif
    )
    {
      mutt_debug(1, "trying rename...\n");
      if (rename(src, target) == -1)
      {
        mutt_debug(1, "rename (%s, %s) failed: %s (%d)\n", src, target,
                   strerror(errno), errno);
        return -1;
      }
      mutt_debug(1, "rename succeeded.\n");

      return 0;
    }

    return -1;
  }

  /* Stat both links and check if they are equal. */

  if (lstat(src, &ssb) == -1)
  {
    mutt_debug(1, "#1 can't stat %s: %s (%d)\n", src, strerror(errno), errno);
    return -1;
  }

  if (lstat(target, &tsb) == -1)
  {
    mutt_debug(1, "#2 can't stat %s: %s (%d)\n", src, strerror(errno), errno);
    return -1;
  }

  /* pretend that the link failed because the target file did already exist. */

  if (!compare_stat(&ssb, &tsb))
  {
    mutt_debug(1, "stat blocks for %s and %s diverge; pretending EEXIST.\n", src, target);
    errno = EEXIST;
    return -1;
  }

  /* Unlink the original link.
   * Should we really ignore the return value here? XXX */
  if (unlink(src) == -1)
  {
    mutt_debug(1, "unlink (%s) failed: %s (%d)\n", src, strerror(errno), errno);
  }

  return 0;
}

/**
 * mutt_file_rmtree - Recursively remove a directory
 * @param path Directory to delete
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int mutt_file_rmtree(const char *path)
{
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char cur[LONG_STRING];
  struct stat statbuf;
  int rc = 0;

  dirp = opendir(path);
  if (!dirp)
  {
    mutt_debug(1, "error opening directory %s\n", path);
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
      rc |= mutt_file_rmtree(cur);
    else
      rc |= unlink(cur);
  }
  closedir(dirp);

  rc |= rmdir(path);

  return rc;
}

/**
 * mutt_file_open - Open a file
 * @param path  Pathname to open
 * @param flags Flags, e.g. O_EXCL
 * @retval >0 Success, file handle
 * @retval -1 Error
 */
int mutt_file_open(const char *path, int flags)
{
  struct stat osb, nsb;
  int fd;

  if (flags & O_EXCL)
  {
    char safe_file[_POSIX_PATH_MAX];
    char safe_dir[_POSIX_PATH_MAX];

    if (mkwrapdir(path, safe_file, sizeof(safe_file), safe_dir, sizeof(safe_dir)) == -1)
      return -1;

    fd = open(safe_file, flags, 0600);
    if (fd < 0)
    {
      rmdir(safe_dir);
      return fd;
    }

    /* NFS and I believe cygwin do not handle movement of open files well */
    close(fd);
    if (put_file_in_place(path, safe_file, safe_dir) == -1)
      return -1;
  }

  fd = open(path, flags & ~O_EXCL, 0600);
  if (fd < 0)
    return fd;

  /* make sure the file is not symlink */
  if ((lstat(path, &osb) < 0 || fstat(fd, &nsb) < 0) || !compare_stat(&osb, &nsb))
  {
    close(fd);
    return -1;
  }

  return fd;
}

/**
 * mutt_file_fopen - Call fopen() safely
 * @param path Filename
 * @param mode Mode e.g. "r" readonly; "w" read-write
 * @retval ptr  FILE handle
 * @retval NULL Error, see errno
 *
 * When opening files for writing, make sure the file doesn't already exist to
 * avoid race conditions.
 */
FILE *mutt_file_fopen(const char *path, const char *mode)
{
  if (!path || !mode)
    return NULL;

  if (mode[0] == 'w')
  {
    int fd;
    int flags = O_CREAT | O_EXCL | O_NOFOLLOW;

    if (mode[1] == '+')
      flags |= O_RDWR;
    else
      flags |= O_WRONLY;

    fd = mutt_file_open(path, flags);
    if (fd < 0)
      return NULL;

    return (fdopen(fd, mode));
  }
  else
    return (fopen(path, mode));
}

/**
 * mutt_file_sanitize_filename - Replace unsafe characters in a filename
 * @param f     Filename to make safe
 * @param slash Replace '/' characters too
 */
void mutt_file_sanitize_filename(char *f, short slash)
{
  if (!f)
    return;

  for (; *f; f++)
  {
    if ((slash && *f == '/') || !strchr(safe_chars, *f))
      *f = '_';
  }
}

/**
 * mutt_file_sanitize_regex - Escape any regex-magic characters in a string
 * @param dest    Buffer for result
 * @param destlen Length of buffer
 * @param src     String to transform
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_file_sanitize_regex(char *dest, size_t destlen, const char *src)
{
  while (*src && (--destlen > 2))
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
 * mutt_file_read_line - Read a line from a file
 * @param[out] s     Buffer allocated on the head (optional)
 * @param[in]  size  Length of buffer (optional)
 * @param[in]  fp    File to read
 * @param[out] line  Current line number
 * @param[in]  flags Flags, e.g. #MUTT_CONT
 * @retval ptr The allocated string
 *
 * Read a line from ``fp'' into the dynamically allocated ``s'', increasing
 * ``s'' if necessary. The ending "\n" or "\r\n" is removed.  If a line ends
 * with "\", this char and the linefeed is removed, and the next line is read
 * too.
 */
char *mutt_file_read_line(char *s, size_t *size, FILE *fp, int *line, int flags)
{
  size_t offset = 0;
  char *ch = NULL;

  if (!s)
  {
    s = mutt_mem_malloc(STRING);
    *size = STRING;
  }

  while (true)
  {
    if (fgets(s + offset, *size - offset, fp) == NULL)
    {
      FREE(&s);
      return NULL;
    }
    ch = strchr(s + offset, '\n');
    if (ch)
    {
      if (line)
        (*line)++;
      if (flags & MUTT_EOL)
        return s;
      *ch = '\0';
      if ((ch > s) && (*(ch - 1) == '\r'))
        *--ch = '\0';
      if (!(flags & MUTT_CONT) || (ch == s) || (*(ch - 1) != '\\'))
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
        mutt_mem_realloc(&s, *size);
      }
    }
  }
}

/**
 * mutt_file_quote_filename - Quote a filename to survive the shell's quoting rules
 * @param d Buffer for the result
 * @param l Length of buffer
 * @param f String to convert
 * @retval num Number of bytes written to the buffer
 *
 * From the Unix programming FAQ by way of Liviu.
 */
size_t mutt_file_quote_filename(char *d, size_t l, const char *f)
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

  for (size_t i = 0; (j < l) && f[i]; i++)
  {
    if ((f[i] == '\'') || (f[i] == '`'))
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

/**
 * mutt_file_concatn_path - Concatenate directory and filename
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
char *mutt_file_concatn_path(char *dst, size_t dstlen, const char *dir,
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
     * It doesn't appear that the return value is actually checked anywhere mutt_file_concat_path()
     * is called, so we should just copy set dst to nul and let the calling function fail later.
     */
    dst[0] = '\0'; /* safe since we bail out early if dstlen == 0 */
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
  dst[offset] = '\0';
  return dst;
}

/**
 * mutt_file_concat_path - Join a directory name and a filename
 * @param d     Buffer for the result
 * @param dir   Directory name
 * @param fname File name
 * @param l     Length of buffer
 * @retval ptr Destination buffer
 *
 * If both dir and fname are supplied, they are separated with '/'.
 * If either is missing, then the other will be copied exactly.
 */
char *mutt_file_concat_path(char *d, const char *dir, const char *fname, size_t l)
{
  const char *fmt = "%s/%s";

  if (!*fname || (*dir && dir[strlen(dir) - 1] == '/'))
    fmt = "%s%s";

  snprintf(d, l, fmt, dir, fname);
  return d;
}

/**
 * mutt_file_basename - Find the last component for a pathname
 * @param f String to be examined
 * @retval ptr Part of pathname after last '/' character
 */
const char *mutt_file_basename(const char *f)
{
  const char *p = strrchr(f, '/');
  if (p)
    return p + 1;
  else
    return f;
}

/**
 * mutt_file_mkdir - Recursively create directories
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
int mutt_file_mkdir(const char *path, mode_t mode)
{
  if (!path || !*path)
  {
    errno = EINVAL;
    return -1;
  }

  errno = 0;
  char tmp_path[PATH_MAX];
  const size_t len = strlen(path);

  if (len >= sizeof(tmp_path))
  {
    errno = ENAMETOOLONG;
    return -1;
  }

  struct stat sb;
  if ((stat(path, &sb) == 0) && S_ISDIR(sb.st_mode))
    return 0;

  /* Create a mutable copy */
  mutt_str_strfcpy(tmp_path, path, sizeof(tmp_path));

  for (char *p = tmp_path + 1; *p; p++)
  {
    if (*p != '/')
      continue;

    /* Temporarily truncate the path */
    *p = '\0';

    if ((mkdir(tmp_path, S_IRWXU | S_IRWXG | S_IRWXO) != 0) && (errno != EEXIST))
      return -1;

    *p = '/';
  }

  if ((mkdir(tmp_path, mode) != 0) && (errno != EEXIST))
    return -1;

  return 0;
}

/**
 * mutt_file_decrease_mtime - Decrease a file's modification time by 1 second
 * @param f  Filename
 * @param st struct stat for the file (optional)
 * @retval num Updated Unix mtime
 * @retval -1  Error, see errno
 *
 * If a file's mtime is NOW, then set it to 1 second in the past.
 */
time_t mutt_file_decrease_mtime(const char *f, struct stat *st)
{
  struct utimbuf utim;
  struct stat st2;
  time_t mtime;

  if (!st)
  {
    if (stat(f, &st2) == -1)
      return -1;
    st = &st2;
  }

  mtime = st->st_mtime;
  if (mtime == time(NULL))
  {
    mtime -= 1;
    utim.actime = mtime;
    utim.modtime = mtime;
    utime(f, &utim);
  }

  return mtime;
}

/**
 * mutt_file_dirname - Return a path up to, but not including, the final '/'
 * @param  p    Path
 * @retval ptr  The directory containing p
 *
 * Unlike the IEEE Std 1003.1-2001 specification of dirname(3), this
 * implementation does not modify its parameter, so callers need not manually
 * copy their paths into a modifiable buffer prior to calling this function.
 *
 * @warning mutt_file_dirname() returns a static string which must not be free()'d.
 */
const char *mutt_file_dirname(const char *p)
{
  static char buf[_POSIX_PATH_MAX];
  mutt_str_strfcpy(buf, p, sizeof(buf));
  return dirname(buf);
}

/**
 * mutt_file_set_mtime - Set the modification time of one file from another
 * @param from Filename whose mtime should be copied
 * @param to   Filename to update
 */
void mutt_file_set_mtime(const char *from, const char *to)
{
  struct utimbuf utim;
  struct stat st;

  if (stat(from, &st) != -1)
  {
    utim.actime = st.st_mtime;
    utim.modtime = st.st_mtime;
    utime(to, &utim);
  }
}

/**
 * mutt_file_touch_atime - Set the access time to current time
 * @param f File descriptor of the file to alter
 *
 * This is just as read() would do on !noatime.
 * Silently ignored if futimens() isn't supported.
 */
void mutt_file_touch_atime(int f)
{
#ifdef HAVE_FUTIMENS
  struct timespec times[2] = { { 0, UTIME_NOW }, { 0, UTIME_OMIT } };
  futimens(f, times);
#endif
}

/**
 * mutt_file_chmod - Set permissions of a file
 * @param path Filename
 * @param mode the permissions to set
 * @retval int same as chmod(2)
 *
 * This is essentially chmod(path, mode), see chmod(2).
 */
int mutt_file_chmod(const char *path, mode_t mode)
{
  return chmod(path, mode);
}

/**
 * mutt_file_chmod_add - Add permissions to a file
 * @param path Filename
 * @param mode the permissions to add
 * @retval int same as chmod(2)
 * @see   mutt_file_chmod_add_stat()
 *
 * Adds the given permissions to the file. Permissions not mentioned in mode
 * will stay as they are. This function resembles the `chmod ugoa+rwxXst`
 * command family. Example:
 *
 *     mutt_file_chmod_add(path, S_IWUSR | S_IWGRP | S_IWOTH);
 *
 * will add write permissions to path but does not alter read and other
 * permissions.
 */
int mutt_file_chmod_add(const char *path, mode_t mode)
{
  return mutt_file_chmod_add_stat(path, mode, NULL);
}

/**
 * mutt_file_chmod_add_stat - Add permissions to a file
 * @param path Filename
 * @param mode the permissions to add
 * @param st   struct stat for the file (optional)
 * @retval int same as chmod(2)
 * @see   mutt_file_chmod_add()
 *
 * Same as mutt_file_chmod_add() but saves a system call to stat() if a
 * non-NULL stat structure is given. Useful if the stat structure of the file
 * was retrieved before by the calling code. Example:
 *
 *     struct stat st;
 *     stat(path, &st);
 *     // ... do something else with st ...
 *     mutt_file_chmod_add_stat(path, S_IWUSR | S_IWGRP | S_IWOTH, st);
 *
 */
int mutt_file_chmod_add_stat(const char *path, mode_t mode, struct stat *st)
{
  struct stat st2;

  if (!st)
  {
    if (stat(path, &st2) == -1)
      return -1;
    st = &st2;
  }
  return chmod(path, st->st_mode | mode);
}

/**
 * mutt_file_chmod_rm - Remove permissions from a file
 * @param path Filename
 * @param mode the permissions to remove
 * @retval int same as chmod(2)
 * @see   mutt_file_chmod_rm_stat()
 *
 * Removes the given permissions from the file. Permissions not mentioned in
 * mode will stay as they are. This function resembles the `chmod ugoa-rwxXst`
 * command family. Example:
 *
 *     mutt_file_chmod_rm(path, S_IWUSR | S_IWGRP | S_IWOTH);
 *
 * will remove write permissions from path but does not alter read and other
 * permissions.
 */
int mutt_file_chmod_rm(const char *path, mode_t mode)
{
  return mutt_file_chmod_rm_stat(path, mode, NULL);
}

/**
 * mutt_file_chmod_rm_stat - Remove permissions from a file
 * @param path Filename
 * @param mode the permissions to remove
 * @param st   struct stat for the file (optional)
 * @retval int same as chmod(2)
 * @see   mutt_file_chmod_rm()
 *
 * Same as mutt_file_chmod_rm() but saves a system call to stat() if a non-NULL
 * stat structure is given. Useful if the stat structure of the file was
 * retrieved before by the calling code. Example:
 *
 *     struct stat st;
 *     stat(path, &st);
 *     // ... do something else with st ...
 *     mutt_file_chmod_rm_stat(path, S_IWUSR | S_IWGRP | S_IWOTH, st);
 *
 */
int mutt_file_chmod_rm_stat(const char *path, mode_t mode, struct stat *st)
{
  struct stat st2;

  if (!st)
  {
    if (stat(path, &st2) == -1)
      return -1;
    st = &st2;
  }
  return chmod(path, st->st_mode & ~mode);
}

/**
 * mutt_file_lock - (try to) lock a file
 * @param fd      File descriptor to file
 * @param excl    If set, try to lock exclusively
 * @param timeout Retry after this time
 * @retval 0 on success
 * @retval -1 on failure
 *
 * The type of file locking depends on how NeoMutt was compiled.
 * It could use fcntl() or flock() to perform the locking.
 *
 * Use mutt_file_unlock() to unlock the file.
 */
int mutt_file_lock(int fd, int excl, int timeout)
{
#if defined(USE_FCNTL) || defined(USE_FLOCK)
  int count;
  int attempt;
  struct stat sb = { 0 }, prev_sb = { 0 };
#endif
  int r = 0;

#ifdef USE_FCNTL
  struct flock lck;

  memset(&lck, 0, sizeof(struct flock));
  lck.l_type = excl ? F_WRLCK : F_RDLCK;
  lck.l_whence = SEEK_SET;

  count = 0;
  attempt = 0;
  while (fcntl(fd, F_SETLK, &lck) == -1)
  {
    mutt_debug(1, "fcntl errno %d.\n", errno);
    if ((errno != EAGAIN) && (errno != EACCES))
    {
      mutt_perror("fcntl");
      return -1;
    }

    if (fstat(fd, &sb) != 0)
      sb.st_size = 0;

    if (count == 0)
      prev_sb = sb;

    /* only unlock file if it is unchanged */
    if ((prev_sb.st_size == sb.st_size) && (++count >= (timeout ? MAX_LOCK_ATTEMPTS : 0)))
    {
      if (timeout)
        mutt_error(_("Timeout exceeded while attempting fcntl lock!"));
      return -1;
    }

    prev_sb = sb;

    mutt_message(_("Waiting for fcntl lock... %d"), ++attempt);
    sleep(1);
  }
#endif /* USE_FCNTL */

#ifdef USE_FLOCK
  count = 0;
  attempt = 0;
  while (flock(fd, (excl ? LOCK_EX : LOCK_SH) | LOCK_NB) == -1)
  {
    if (errno != EWOULDBLOCK)
    {
      mutt_perror("flock");
      r = -1;
      break;
    }

    if (fstat(fd, &sb) != 0)
      sb.st_size = 0;

    if (count == 0)
      prev_sb = sb;

    /* only unlock file if it is unchanged */
    if ((prev_sb.st_size == sb.st_size) && (++count >= (timeout ? MAX_LOCK_ATTEMPTS : 0)))
    {
      if (timeout)
        mutt_error(_("Timeout exceeded while attempting flock lock!"));
      r = -1;
      break;
    }

    prev_sb = sb;

    mutt_message(_("Waiting for flock attempt... %d"), ++attempt);
    sleep(1);
  }
#endif /* USE_FLOCK */

  /* release any other locks obtained in this routine */
  if (r != 0)
  {
#ifdef USE_FCNTL
    lck.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lck);
#endif /* USE_FCNTL */

#ifdef USE_FLOCK
    flock(fd, LOCK_UN);
#endif /* USE_FLOCK */
  }

  return r;
}

/**
 * mutt_file_unlock - Unlock a file previously locked by mutt_file_lock()
 * @param fd File descriptor to file
 * @retval 0 Always
 */
int mutt_file_unlock(int fd)
{
#ifdef USE_FCNTL
  struct flock unlockit = { F_UNLCK, 0, 0, 0, 0 };

  memset(&unlockit, 0, sizeof(struct flock));
  unlockit.l_type = F_UNLCK;
  unlockit.l_whence = SEEK_SET;
  fcntl(fd, F_SETLK, &unlockit);
#endif

#ifdef USE_FLOCK
  flock(fd, LOCK_UN);
#endif

  return 0;
}

/**
 * mutt_file_unlink_empty - Delete a file if it's empty
 * @param path File to delete
 */
void mutt_file_unlink_empty(const char *path)
{
  int fd;
  struct stat sb;

  fd = open(path, O_RDWR);
  if (fd == -1)
    return;

  if (mutt_file_lock(fd, 1, 1) == -1)
  {
    close(fd);
    return;
  }

  if (fstat(fd, &sb) == 0 && sb.st_size == 0)
    unlink(path);

  mutt_file_unlock(fd);
  close(fd);
}

/**
 * mutt_file_rename - Rename a file
 * @param oldfile Old filename
 * @param newfile New filename
 * @retval 0 Success
 * @retval 1 Old file doesn't exist
 * @retval 2 New file already exists
 * @retval 3 Some other error
 *
 * note on access(2) use: No dangling symlink problems here due to
 * mutt_file_fopen().
 */
int mutt_file_rename(char *oldfile, char *newfile)
{
  FILE *ofp = NULL, *nfp = NULL;

  if (access(oldfile, F_OK) != 0)
    return 1;
  if (access(newfile, F_OK) == 0)
    return 2;
  ofp = fopen(oldfile, "r");
  if (!ofp)
    return 3;
  nfp = mutt_file_fopen(newfile, "w");
  if (!nfp)
  {
    mutt_file_fclose(&ofp);
    return 3;
  }
  mutt_file_copy_stream(ofp, nfp);
  mutt_file_fclose(&nfp);
  mutt_file_fclose(&ofp);
  mutt_file_unlink(oldfile);
  return 0;
}

/**
 * mutt_file_to_absolute_path - Convert relative filepath to an absolute path
 * @param path      Relative path
 * @param reference Absolute path that \a path is relative to
 * @retval true on success
 * @retval false otherwise
 *
 * Use POSIX functions to convert a path to absolute, relatively to another path
 * @note \a path should be at least of PATH_MAX length
 */
int mutt_file_to_absolute_path(char *path, const char *reference)
{
  const char *dirpath = NULL;
  char abs_path[PATH_MAX];
  int path_len;

  /* if path is already absolute, don't do anything */
  if ((strlen(path) > 1) && (path[0] == '/'))
  {
    return true;
  }

  dirpath = mutt_file_dirname(reference);
  mutt_str_strfcpy(abs_path, dirpath, PATH_MAX);
  mutt_str_strncat(abs_path, sizeof(abs_path), "/", 1); /* append a / at the end of the path */

  path_len = PATH_MAX - strlen(path);

  mutt_str_strncat(abs_path, sizeof(abs_path), path, path_len > 0 ? path_len : 0);

  path = realpath(abs_path, path);

  if (!path)
  {
    printf("Error: issue converting path to absolute (%s)", strerror(errno));
    return false;
  }

  return true;
}

/**
 * mutt_file_read_keyword - Read a keyword from a file
 * @param file   File to read
 * @param buffer Buffer to store the keyword
 * @param buflen Length of the buffer
 * @retval ptr Start of the keyword
 *
 * Read one line from the start of a file.
 * Skip any leading whitespace and extract the first token.
 */
char *mutt_file_read_keyword(const char *file, char *buffer, size_t buflen)
{
  FILE *fp = NULL;
  char *start = NULL;

  fp = mutt_file_fopen(file, "r");
  if (!fp)
    return NULL;

  buffer = fgets(buffer, buflen, fp);
  mutt_file_fclose(&fp);

  if (!buffer)
    return NULL;

  SKIPWS(buffer);
  start = buffer;

  while (*buffer && !isspace(*buffer))
    buffer++;

  *buffer = '\0';

  return start;
}

/**
 * mutt_file_check_empty - Is the mailbox empty
 * @param path Path to mailbox
 * @retval 1 mailbox is not empty
 * @retval 0 mailbox is empty
 * @retval -1 on error
 */
int mutt_file_check_empty(const char *path)
{
  struct stat st;

  if (stat(path, &st) == -1)
    return -1;

  return ((st.st_size == 0));
}
