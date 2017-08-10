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
 *
 * | Function                  | Description
 * | :------------------------ | :-----------------------------------------------------------
 * | mutt_basename()           | Find the last component for a pathname
 * | mutt_concatn_path()       | Concatenate directory and filename
 * | mutt_concat_path()        | Join a directory name and a filename
 * | mutt_copy_bytes()         | Copy some content from one file to another
 * | mutt_copy_stream()        | Copy the contents of one file into another
 * | mutt_decrease_mtime()     | Decrease a file's modification time by 1 second
 * | mutt_lock_file()          | (try to) lock a file
 * | mutt_mkdir()              | Recursively create directories
 * | mutt_quote_filename()     | Quote a filename to survive the shell's quoting rules
 * | mutt_read_line()          | Read a line from a file
 * | mutt_rmtree()             | Recursively remove a directory
 * | mutt_rx_sanitize_string() | Escape any regex-magic characters in a string
 * | mutt_sanitize_filename()  | Replace unsafe characters in a filename
 * | mutt_set_mtime()          | Set the modification time of one file from another
 * | mutt_touch_atime()        | Set the access time to current time
 * | mutt_unlink()             | Delete a file, carefully
 * | mutt_unlink_empty()       | Delete a file if it's empty
 * | mutt_unlock_file()        | Unlock a file previously locked by mutt_lock_file()
 * | safe_fclose()             | Close a FILE handle (and NULL the pointer)
 * | safe_fopen()              | Call fopen() safely
 * | safe_fsync_close()        | Flush the data, before closing a file (and NULL the pointer)
 * | safe_open()               | Open a file
 * | safe_rename()             | NFS-safe renaming of files
 * | safe_symlink()            | Create a symlink
 */

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#include "file.h"
#include "debug.h"
#include "memory.h"
#include "message.h"
#include "string2.h"

/* these characters must be escaped in regular expressions */
static const char rx_special_chars[] = "^.[$()|*+?{\\";

static const char safe_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+@{}._-:%/";

#define MAX_LOCK_ATTEMPTS 5

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
 * @retval  0 Succes 0 Success
 * @retval -1 Error
 */
static int mkwrapdir(const char *path, char *newfile, size_t nflen, char *newdir, size_t ndlen)
{
  const char *basename = NULL;
  char parent[_POSIX_PATH_MAX];
  char *p = NULL;

  strfcpy(parent, NONULL(path), sizeof(parent));

  p = strrchr(parent, '/');
  if (p)
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
 * put_file_in_place - Move a file into place
 * @param path      Destination path
 * @param safe_file Current filename
 * @param safe_dir  Current directory name
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
static int put_file_in_place(const char *path, const char *safe_file, const char *safe_dir)
{
  int rv;

  rv = safe_rename(safe_file, path);
  unlink(safe_file);
  rmdir(safe_dir);
  return rv;
}

/**
 * safe_fclose - Close a FILE handle (and NULL the pointer)
 * @param f FILE handle to close
 * @retval 0   Success
 * @retval EOF Error, see errno
 */
int safe_fclose(FILE **f)
{
  int r = 0;

  if (*f)
    r = fclose(*f);

  *f = NULL;
  return r;
}

/**
 * safe_fsync_close - Flush the data, before closing a file (and NULL the pointer)
 * @param f FILE handle to close
 * @retval 0   Success
 * @retval EOF Error, see errno
 */
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

/**
 * mutt_unlink - Delete a file, carefully
 * @param s Filename
 *
 * This won't follow symlinks.
 */
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

  if ((lstat(s, &sb) == 0) && S_ISREG(sb.st_mode))
  {
    fd = open(s, flags);
    if (fd < 0)
      return;

    if ((fstat(fd, &sb2) != 0) || !S_ISREG(sb2.st_mode) ||
        (sb.st_dev != sb2.st_dev) || (sb.st_ino != sb2.st_ino))
    {
      close(fd);
      return;
    }

    f = fdopen(fd, "r+");
    if (f)
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

/**
 * mutt_copy_bytes - Copy some content from one file to another
 * @param in   Source file
 * @param out  Destination file
 * @param size Maximum number of bytes to copy
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int mutt_copy_bytes(FILE *in, FILE *out, size_t size)
{
  char buf[2048];
  size_t chunk;

  while (size > 0)
  {
    chunk = (size > sizeof(buf)) ? sizeof(buf) : size;
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
 * mutt_copy_stream - Copy the contents of one file into another
 * @param fin  Source file
 * @param fout Destination file
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
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

/**
 * safe_symlink - Create a symlink
 * @param oldpath Existing pathname
 * @param newpath New pathname
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int safe_symlink(const char *oldpath, const char *newpath)
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
 * safe_rename - NFS-safe renaming of files
 * @param src    Original filename
 * @param target New filename
 * @retval  0 Success
 * @retval -1 Error, see errno
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
    /* Coda does not allow cross-directory links, but tells
     * us it's a cross-filesystem linking attempt.
     *
     * However, the Coda rename call is allegedly safe to use.
     *
     * With other file systems, rename should just fail when
     * the files reside on different file systems, so it's safe
     * to try it here. */
    mutt_debug(1, "safe_rename: link (%s, %s) failed: %s (%d)\n", src, target,
               strerror(errno), errno);

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

  /* Stat both links and check if they are equal. */

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

  /* pretend that the link failed because the target file did already exist. */

  if (!compare_stat(&ssb, &tsb))
  {
    mutt_debug(1, "safe_rename: stat blocks for %s and %s diverge; "
                  "pretending EEXIST.\n",
               src, target);
    errno = EEXIST;
    return -1;
  }

  /* Unlink the original link.
   * Should we really ignore the return value here? XXX */
  if (unlink(src) == -1)
  {
    mutt_debug(1, "safe_rename: unlink (%s) failed: %s (%d)\n", src, strerror(errno), errno);
  }

  return 0;
}

/**
 * mutt_rmtree - Recursively remove a directory
 * @param path Directory to delete
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int mutt_rmtree(const char *path)
{
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char cur[_POSIX_PATH_MAX];
  struct stat statbuf;
  int rc = 0;

  dirp = opendir(path);
  if (!dirp)
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

/**
 * safe_open - Open a file
 * @param path  Pathname to open
 * @param flags Flags, e.g. O_EXCL
 * @retval >0 Success, file handle
 * @retval -1 Error
 */
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
 * safe_fopen - Call fopen() safely
 * @param path Filename
 * @param mode Mode e.g. "r" readonly; "w" read-write
 * @retval ptr  FILE handle
 * @retval NULL Error, see errno
 *
 * When opening files for writing, make sure the file doesn't already exist to
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

    fd = safe_open(path, flags);
    if (fd < 0)
      return NULL;

    return (fdopen(fd, mode));
  }
  else
    return (fopen(path, mode));
}

/**
 * mutt_sanitize_filename - Replace unsafe characters in a filename
 * @param f     Filename to make safe
 * @param slash Replace '/' characters too
 */
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

/**
 * mutt_rx_sanitize_string - Escape any regex-magic characters in a string
 * @param dest    Buffer for result
 * @param destlen Length of buffer
 * @param src     String to transform
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_rx_sanitize_string(char *dest, size_t destlen, const char *src)
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
 * mutt_read_line - Read a line from a file
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
      if ((ch > s) && (*(ch - 1) == '\r'))
        *--ch = 0;
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
        safe_realloc(&s, *size);
      }
    }
  }
}

/**
 * mutt_quote_filename - Quote a filename to survive the shell's quoting rules
 * @param d Buffer for the result
 * @param l Length of buffer
 * @param f String to convert
 * @retval num Number of bytes written to the buffer
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

/**
 * mutt_concat_path - Join a directory name and a filename
 * @param d     Buffer for the result
 * @param dir   Directory name
 * @param fname File name
 * @param l     Length of buffer
 * @retval ptr Destination buffer
 *
 * If both dir and fname are supplied, they are separated with '/'.
 * If either is missing, then the other will be copied exactly.
 */
char *mutt_concat_path(char *d, const char *dir, const char *fname, size_t l)
{
  const char *fmt = "%s/%s";

  if (!*fname || (*dir && dir[strlen(dir) - 1] == '/'))
    fmt = "%s%s";

  snprintf(d, l, fmt, dir, fname);
  return d;
}

/**
 * mutt_basename - Find the last component for a pathname
 * @param f String to be examined
 * @retval ptr Part of pathname after last '/' character
 */
const char *mutt_basename(const char *f)
{
  const char *p = strrchr(f, '/');
  if (p)
    return p + 1;
  else
    return f;
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

/**
 * mutt_decrease_mtime - Decrease a file's modification time by 1 second
 * @param f  Filename
 * @param st struct stat for the file (optional)
 * @retval num Updated Unix mtime
 * @retval -1  Error, see errno
 *
 * If a file's mtime is NOW, then set it to 1 second in the past.
 */
time_t mutt_decrease_mtime(const char *f, struct stat *st)
{
  struct utimbuf utim;
  struct stat _st;
  time_t mtime;

  if (!st)
  {
    if (stat(f, &_st) == -1)
      return -1;
    st = &_st;
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
 * mutt_set_mtime - Set the modification time of one file from another
 * @param from Filename whose mtime should be copied
 * @param to   Filename to update
 */
void mutt_set_mtime(const char *from, const char *to)
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
 * mutt_touch_atime - Set the access time to current time
 * @param f File descriptor of the file to alter
 *
 * This is just as read() would do on !noatime.
 * Silently ignored if futimens() isn't supported.
 */
void mutt_touch_atime(int f)
{
#ifdef HAVE_FUTIMENS
  struct timespec times[2] = { { 0, UTIME_NOW }, { 0, UTIME_OMIT } };
  futimens(f, times);
#endif
}

/**
 * mutt_lock_file - (try to) lock a file
 * @param path    Path to file
 * @param fd      File descriptor to file
 * @param excl    If set, try to lock exclusively
 * @param timeout Retry after this time
 * @retval 0 on success
 * @retval -1 on failure
 *
 * The type of file locking depends on how NeoMutt was compiled.
 * It could use fcntl() or flock() to perform the locking.
 *
 * Use mutt_unlock_file() to unlock the file.
 */
int mutt_lock_file(const char *path, int fd, int excl, int timeout)
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
    mutt_debug(1, "mutt_lock_file(): fcntl errno %d.\n", errno);
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
 * mutt_unlock_file - Unlock a file previously locked by mutt_lock_file()
 * @param path Path to file
 * @param fd   File descriptor to file
 * @retval 0 Always
 */
int mutt_unlock_file(const char *path, int fd)
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
 * mutt_unlink_empty - Delete a file if it's empty
 * @param path File to delete
 */
void mutt_unlink_empty(const char *path)
{
  int fd;
  struct stat sb;

  fd = open(path, O_RDWR);
  if (fd == -1)
    return;

  if (mutt_lock_file(path, fd, 1, 1) == -1)
  {
    close(fd);
    return;
  }

  if (fstat(fd, &sb) == 0 && sb.st_size == 0)
    unlink(path);

  mutt_unlock_file(path, fd);
  close(fd);
}
