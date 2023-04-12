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
 * @page mutt_file File management functions
 *
 * Commonly used file/dir management routines.
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "file.h"
#include "buffer.h"
#include "date.h"
#include "logging2.h"
#include "memory.h"
#include "message.h"
#include "path.h"
#include "pool.h"
#include "string2.h"
#ifdef USE_FLOCK
#include <sys/file.h>
#endif

/// These characters must be escaped in regular expressions
static const char RxSpecialChars[] = "^.[$()|*+?{\\";

/// Set of characters that are safe to use in filenames
const char FilenameSafeChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+@{}._-:%/";

#define MAX_LOCK_ATTEMPTS 5

/* This is defined in POSIX:2008 which isn't a build requirement */
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

/**
 * compare_stat - Compare the struct stat's of two files/dirs
 * @param st_old struct stat of the first file/dir
 * @param st_new struct stat of the second file/dir
 * @retval true They match
 *
 * This compares the device id (st_dev), inode number (st_ino) and special id
 * (st_rdev) of the files/dirs.
 */
static bool compare_stat(struct stat *st_old, struct stat *st_new)
{
  return (st_old->st_dev == st_new->st_dev) && (st_old->st_ino == st_new->st_ino) &&
         (st_old->st_rdev == st_new->st_rdev);
}

/**
 * mkwrapdir - Create a temporary directory next to a file name
 * @param path    Existing filename
 * @param newfile New filename
 * @param newdir  New directory name
 * @retval  0 Success
 * @retval -1 Error
 */
static int mkwrapdir(const char *path, struct Buffer *newfile, struct Buffer *newdir)
{
  const char *basename = NULL;
  int rc = 0;

  struct Buffer parent = buf_make(PATH_MAX);
  buf_strcpy(&parent, NONULL(path));

  char *p = strrchr(parent.data, '/');
  if (p)
  {
    *p = '\0';
    basename = p + 1;
  }
  else
  {
    buf_strcpy(&parent, ".");
    basename = path;
  }

  buf_printf(newdir, "%s/%s", buf_string(&parent), ".muttXXXXXX");
  if (!mkdtemp(newdir->data))
  {
    mutt_debug(LL_DEBUG1, "mkdtemp() failed\n");
    rc = -1;
    goto cleanup;
  }

  buf_printf(newfile, "%s/%s", newdir->data, NONULL(basename));

cleanup:
  buf_dealloc(&parent);
  return rc;
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
 * @param[out] fp FILE handle to close
 * @retval 0   Success
 * @retval EOF Error, see errno
 */
int mutt_file_fclose(FILE **fp)
{
  if (!fp || !*fp)
    return 0;

  int rc = fclose(*fp);
  *fp = NULL;
  return rc;
}

/**
 * mutt_file_fsync_close - Flush the data, before closing a file (and NULL the pointer)
 * @param[out] fp FILE handle to close
 * @retval 0   Success
 * @retval EOF Error, see errno
 */
int mutt_file_fsync_close(FILE **fp)
{
  if (!fp || !*fp)
    return 0;

  int rc = 0;

  if (fflush(*fp) || fsync(fileno(*fp)))
  {
    int save_errno = errno;
    rc = -1;
    mutt_file_fclose(fp);
    errno = save_errno;
  }
  else
  {
    rc = mutt_file_fclose(fp);
  }

  return rc;
}

/**
 * mutt_file_unlink - Delete a file, carefully
 * @param s Filename
 *
 * This won't follow symlinks.
 */
void mutt_file_unlink(const char *s)
{
  if (!s)
    return;

  struct stat st = { 0 };
  /* Defend against symlink attacks */

  const bool is_regular_file = (lstat(s, &st) == 0) && S_ISREG(st.st_mode);
  if (!is_regular_file)
    return;

  const int fd = open(s, O_RDWR | O_NOFOLLOW);
  if (fd < 0)
    return;

  struct stat st2 = { 0 };
  if ((fstat(fd, &st2) != 0) || !S_ISREG(st2.st_mode) ||
      (st.st_dev != st2.st_dev) || (st.st_ino != st2.st_ino))
  {
    close(fd);
    return;
  }

  unlink(s);
  close(fd);
}

/**
 * mutt_file_copy_bytes - Copy some content from one file to another
 * @param fp_in  Source file
 * @param fp_out Destination file
 * @param size   Maximum number of bytes to copy
 * @retval  0 Success
 * @retval -1 Error, see errno
 */
int mutt_file_copy_bytes(FILE *fp_in, FILE *fp_out, size_t size)
{
  if (!fp_in || !fp_out)
    return -1;

  while (size > 0)
  {
    char buf[2048] = { 0 };
    size_t chunk = (size > sizeof(buf)) ? sizeof(buf) : size;
    chunk = fread(buf, 1, chunk, fp_in);
    if (chunk < 1)
      break;
    if (fwrite(buf, 1, chunk, fp_out) != chunk)
      return -1;

    size -= chunk;
  }

  if (fflush(fp_out) != 0)
    return -1;
  return 0;
}

/**
 * mutt_file_copy_stream - Copy the contents of one file into another
 * @param fp_in  Source file
 * @param fp_out Destination file
 * @retval  n Success, number of bytes copied
 * @retval -1 Error, see errno
 */
int mutt_file_copy_stream(FILE *fp_in, FILE *fp_out)
{
  if (!fp_in || !fp_out)
    return -1;

  size_t total = 0;
  size_t l;
  char buf[1024] = { 0 };

  while ((l = fread(buf, 1, sizeof(buf), fp_in)) > 0)
  {
    if (fwrite(buf, 1, l, fp_out) != l)
      return -1;
    total += l;
  }

  if (fflush(fp_out) != 0)
    return -1;
  return total;
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
  struct stat st_old = { 0 };
  struct stat st_new = { 0 };

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
    struct Buffer abs_oldpath = buf_make(PATH_MAX);

    if (!mutt_path_getcwd(&abs_oldpath))
    {
      buf_dealloc(&abs_oldpath);
      return -1;
    }

    buf_addch(&abs_oldpath, '/');
    buf_addstr(&abs_oldpath, oldpath);
    if (symlink(buf_string(&abs_oldpath), newpath) == -1)
    {
      buf_dealloc(&abs_oldpath);
      return -1;
    }

    buf_dealloc(&abs_oldpath);
  }

  if ((stat(oldpath, &st_old) == -1) || (stat(newpath, &st_new) == -1) ||
      !compare_stat(&st_old, &st_new))
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
  struct stat st_src = { 0 };
  struct stat st_target = { 0 };
  int link_errno;

  if (!src || !target)
    return -1;

  if (link(src, target) != 0)
  {
    link_errno = errno;

    /* It is historically documented that link can return -1 if NFS
     * dies after creating the link.  In that case, we are supposed
     * to use stat to check if the link was created.
     *
     * Derek Martin notes that some implementations of link() follow a
     * source symlink.  It might be more correct to use stat() on src.
     * I am not doing so to minimize changes in behavior: the function
     * used lstat() further below for 20 years without issue, and I
     * believe was never intended to be used on a src symlink.  */
    if ((lstat(src, &st_src) == 0) && (lstat(target, &st_target) == 0) &&
        (compare_stat(&st_src, &st_target) == 0))
    {
      mutt_debug(LL_DEBUG1, "link (%s, %s) reported failure: %s (%d) but actually succeeded\n",
                 src, target, strerror(errno), errno);
      goto success;
    }

    errno = link_errno;

    /* Coda does not allow cross-directory links, but tells
     * us it's a cross-filesystem linking attempt.
     *
     * However, the Coda rename call is allegedly safe to use.
     *
     * With other file systems, rename should just fail when
     * the files reside on different file systems, so it's safe
     * to try it here. */
    mutt_debug(LL_DEBUG1, "link (%s, %s) failed: %s (%d)\n", src, target,
               strerror(errno), errno);

    /* FUSE may return ENOSYS. VFAT may return EPERM. FreeBSD's
     * msdosfs may return EOPNOTSUPP.  ENOTSUP can also appear. */
    if ((errno == EXDEV) || (errno == ENOSYS) || errno == EPERM
#ifdef ENOTSUP
        || errno == ENOTSUP
#endif
#ifdef EOPNOTSUPP
        || errno == EOPNOTSUPP
#endif
    )
    {
      mutt_debug(LL_DEBUG1, "trying rename\n");
      if (rename(src, target) == -1)
      {
        mutt_debug(LL_DEBUG1, "rename (%s, %s) failed: %s (%d)\n", src, target,
                   strerror(errno), errno);
        return -1;
      }
      mutt_debug(LL_DEBUG1, "rename succeeded\n");

      return 0;
    }

    return -1;
  }

  /* Remove the compare_stat() check, because it causes problems with maildir
   * on filesystems that don't properly support hard links, such as sshfs.  The
   * filesystem creates the link, but the resulting file is given a different
   * inode number by the sshfs layer.  This results in an infinite loop
   * creating links.  */
#if 0
  /* Stat both links and check if they are equal. */
  if (lstat(src, &st_src) == -1)
  {
    mutt_debug(LL_DEBUG1, "#1 can't stat %s: %s (%d)\n", src, strerror(errno), errno);
    return -1;
  }

  if (lstat(target, &st_target) == -1)
  {
    mutt_debug(LL_DEBUG1, "#2 can't stat %s: %s (%d)\n", src, strerror(errno), errno);
    return -1;
  }

  /* pretend that the link failed because the target file did already exist. */

  if (!compare_stat(&st_src, &st_target))
  {
    mutt_debug(LL_DEBUG1, "stat blocks for %s and %s diverge; pretending EEXIST\n", src, target);
    errno = EEXIST;
    return -1;
  }
#endif

success:
  /* Unlink the original link.
   * Should we really ignore the return value here? XXX */
  if (unlink(src) == -1)
  {
    mutt_debug(LL_DEBUG1, "unlink (%s) failed: %s (%d)\n", src, strerror(errno), errno);
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
  if (!path)
    return -1;

  struct dirent *de = NULL;
  struct stat st = { 0 };
  int rc = 0;

  DIR *dir = mutt_file_opendir(path, MUTT_OPENDIR_NONE);
  if (!dir)
  {
    mutt_debug(LL_DEBUG1, "error opening directory %s\n", path);
    return -1;
  }

  /* We avoid using the buffer pool for this function, because it
   * invokes recursively to an unknown depth. */
  struct Buffer cur = buf_make(PATH_MAX);

  while ((de = readdir(dir)))
  {
    if ((mutt_str_equal(".", de->d_name)) || (mutt_str_equal("..", de->d_name)))
      continue;

    buf_printf(&cur, "%s/%s", path, de->d_name);
    /* XXX make nonrecursive version */

    if (stat(buf_string(&cur), &st) == -1)
    {
      rc = 1;
      continue;
    }

    if (S_ISDIR(st.st_mode))
      rc |= mutt_file_rmtree(buf_string(&cur));
    else
      rc |= unlink(buf_string(&cur));
  }
  closedir(dir);

  rc |= rmdir(path);

  buf_dealloc(&cur);
  return rc;
}

/**
 * mutt_file_rotate - Rotate a set of numbered files
 * @param path  Template filename
 * @param count Maximum number of files
 * @retval ptr Name of the 0'th file
 *
 * Given a template 'temp', rename files numbered 0 to (count-1).
 *
 * Rename:
 * - ...
 * - temp1 -> temp2
 * - temp0 -> temp1
 */
const char *mutt_file_rotate(const char *path, int count)
{
  if (!path)
    return NULL;

  struct Buffer *old_file = buf_pool_get();
  struct Buffer *new_file = buf_pool_get();

  /* rotate the old debug logs */
  for (count -= 2; count >= 0; count--)
  {
    buf_printf(old_file, "%s%d", path, count);
    buf_printf(new_file, "%s%d", path, count + 1);
    (void) rename(buf_string(old_file), buf_string(new_file));
  }

  path = buf_strdup(old_file);
  buf_pool_release(&old_file);
  buf_pool_release(&new_file);

  return path;
}

/**
 * mutt_file_open - Open a file
 * @param path  Pathname to open
 * @param flags Flags, e.g. O_EXCL
 * @retval >0 Success, file handle
 * @retval -1 Error
 */
int mutt_file_open(const char *path, uint32_t flags)
{
  if (!path)
    return -1;

  int fd;
  struct Buffer safe_file = buf_make(0);
  struct Buffer safe_dir = buf_make(0);

  if (flags & O_EXCL)
  {
    buf_alloc(&safe_file, PATH_MAX);
    buf_alloc(&safe_dir, PATH_MAX);

    if (mkwrapdir(path, &safe_file, &safe_dir) == -1)
    {
      fd = -1;
      goto cleanup;
    }

    fd = open(buf_string(&safe_file), flags, 0600);
    if (fd < 0)
    {
      rmdir(buf_string(&safe_dir));
      goto cleanup;
    }

    /* NFS and I believe cygwin do not handle movement of open files well */
    close(fd);
    if (put_file_in_place(path, buf_string(&safe_file), buf_string(&safe_dir)) == -1)
    {
      fd = -1;
      goto cleanup;
    }
  }

  fd = open(path, flags & ~O_EXCL, 0600);
  if (fd < 0)
    goto cleanup;

  /* make sure the file is not symlink */
  struct stat st_old = { 0 };
  struct stat st_new = { 0 };
  if (((lstat(path, &st_old) < 0) || (fstat(fd, &st_new) < 0)) ||
      !compare_stat(&st_old, &st_new))
  {
    close(fd);
    fd = -1;
    goto cleanup;
  }

cleanup:
  buf_dealloc(&safe_file);
  buf_dealloc(&safe_dir);

  return fd;
}

/**
 * mutt_file_opendir - Open a directory
 * @param path Directory path
 * @param mode See MuttOpenDirMode
 * @retval ptr DIR handle
 * @retval NULL Error, see errno
 */
DIR *mutt_file_opendir(const char *path, enum MuttOpenDirMode mode)
{
  if ((mode == MUTT_OPENDIR_CREATE) && (mutt_file_mkdir(path, S_IRWXU) == -1))
  {
    return NULL;
  }
  errno = 0;
  return opendir(path);
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
    uint32_t flags = O_CREAT | O_EXCL | O_NOFOLLOW;

    if (mode[1] == '+')
      flags |= O_RDWR;
    else
      flags |= O_WRONLY;

    int fd = mutt_file_open(path, flags);
    if (fd < 0)
      return NULL;

    return fdopen(fd, mode);
  }
  else
  {
    return fopen(path, mode);
  }
}

/**
 * mutt_file_sanitize_filename - Replace unsafe characters in a filename
 * @param path  Filename to make safe
 * @param slash Replace '/' characters too
 */
void mutt_file_sanitize_filename(char *path, bool slash)
{
  if (!path)
    return;

  for (; *path; path++)
  {
    if ((slash && (*path == '/')) || !strchr(FilenameSafeChars, *path))
      *path = '_';
  }
}

/**
 * mutt_file_sanitize_regex - Escape any regex-magic characters in a string
 * @param dest Buffer for result
 * @param src  String to transform
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_file_sanitize_regex(struct Buffer *dest, const char *src)
{
  if (!dest || !src)
    return -1;

  buf_reset(dest);
  while (*src != '\0')
  {
    if (strchr(RxSpecialChars, *src))
      buf_addch(dest, '\\');
    buf_addch(dest, *src++);
  }

  return 0;
}

/**
 * mutt_file_seek - Wrapper for fseeko with error handling
 * @param[in] fp File to seek
 * @param[in] offset Offset
 * @param[in] whence Seek mode
 * @retval true Seek was successful
 * @retval false Seek failed
 */
bool mutt_file_seek(FILE *fp, LOFF_T offset, int whence)
{
  if (!fp)
  {
    return false;
  }

  if (fseeko(fp, offset, whence) != 0)
  {
    mutt_perror(_("Failed to seek file: %s"), strerror(errno));
    return false;
  }

  return true;
}

/**
 * mutt_file_read_line - Read a line from a file
 * @param[out] line     Buffer allocated on the head (optional)
 * @param[in]  size     Length of buffer
 * @param[in]  fp       File to read
 * @param[out] line_num Current line number (optional)
 * @param[in]  flags    Flags, e.g. #MUTT_RL_CONT
 * @retval ptr          The allocated string
 *
 * Read a line from "fp" into the dynamically allocated "line", increasing
 * "line" if necessary. The ending "\n" or "\r\n" is removed.  If a line ends
 * with "\", this char and the linefeed is removed, and the next line is read
 * too.
 */
char *mutt_file_read_line(char *line, size_t *size, FILE *fp, int *line_num, ReadLineFlags flags)
{
  if (!size || !fp)
    return NULL;

  size_t offset = 0;
  char *ch = NULL;

  if (!line)
  {
    *size = 256;
    line = mutt_mem_malloc(*size);
  }

  while (true)
  {
    if (!fgets(line + offset, *size - offset, fp))
    {
      FREE(&line);
      return NULL;
    }
    ch = strchr(line + offset, '\n');
    if (ch)
    {
      if (line_num)
        (*line_num)++;
      if (flags & MUTT_RL_EOL)
        return line;
      *ch = '\0';
      if ((ch > line) && (*(ch - 1) == '\r'))
        *--ch = '\0';
      if (!(flags & MUTT_RL_CONT) || (ch == line) || (*(ch - 1) != '\\'))
        return line;
      offset = ch - line - 1;
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
        if (line_num)
          (*line_num)++;
        return line;
      }
      else
      {
        ungetc(c, fp); /* undo our damage */
        /* There wasn't room for the line -- increase "line" */
        offset = *size - 1; /* overwrite the terminating 0 */
        *size += 256;
        mutt_mem_realloc(&line, *size);
      }
    }
  }
}

/**
 * mutt_file_iter_line - Iterate over the lines from an open file pointer
 * @param iter  State of iteration including ptr to line
 * @param fp    File pointer to read from
 * @param flags Same as mutt_file_read_line()
 * @retval true Data read
 * @retval false On eof
 *
 * This is a slightly cleaner interface for mutt_file_read_line() which avoids
 * the eternal C loop initialization ugliness.  Use like this:
 *
 * ```
 * struct MuttFileIter iter = { 0 };
 * while (mutt_file_iter_line(&iter, fp, flags))
 * {
 *   do_stuff(iter.line, iter.line_num);
 * }
 * ```
 */
bool mutt_file_iter_line(struct MuttFileIter *iter, FILE *fp, ReadLineFlags flags)
{
  if (!iter)
    return false;

  char *p = mutt_file_read_line(iter->line, &iter->size, fp, &iter->line_num, flags);
  if (!p)
    return false;
  iter->line = p;
  return true;
}

/**
 * mutt_file_map_lines - Process lines of text read from a file pointer
 * @param func      Callback function to call for each line, see mutt_file_map_t
 * @param user_data Arbitrary data passed to "func"
 * @param fp        File pointer to read from
 * @param flags     Same as mutt_file_read_line()
 * @retval true  All data mapped
 * @retval false "func" returns false
 */
bool mutt_file_map_lines(mutt_file_map_t func, void *user_data, FILE *fp, ReadLineFlags flags)
{
  if (!func || !fp)
    return false;

  struct MuttFileIter iter = { 0 };
  while (mutt_file_iter_line(&iter, fp, flags))
  {
    if (!(*func)(iter.line, iter.line_num, user_data))
    {
      FREE(&iter.line);
      return false;
    }
  }
  return true;
}

/**
 * mutt_file_quote_filename - Quote a filename to survive the shell's quoting rules
 * @param filename String to convert
 * @param buf      Buffer for the result
 * @param buflen   Length of buffer
 * @retval num Bytes written to the buffer
 *
 * From the Unix programming FAQ by way of Liviu.
 */
size_t mutt_file_quote_filename(const char *filename, char *buf, size_t buflen)
{
  if (!buf)
    return 0;

  if (!filename)
  {
    *buf = '\0';
    return 0;
  }

  size_t j = 0;

  /* leave some space for the trailing characters. */
  buflen -= 6;

  buf[j++] = '\'';

  for (size_t i = 0; (j < buflen) && filename[i]; i++)
  {
    if ((filename[i] == '\'') || (filename[i] == '`'))
    {
      buf[j++] = '\'';
      buf[j++] = '\\';
      buf[j++] = filename[i];
      buf[j++] = '\'';
    }
    else
    {
      buf[j++] = filename[i];
    }
  }

  buf[j++] = '\'';
  buf[j] = '\0';

  return j;
}

/**
 * buf_quote_filename - Quote a filename to survive the shell's quoting rules
 * @param buf       Buffer for the result
 * @param filename  String to convert
 * @param add_outer If true, add 'single quotes' around the result
 */
void buf_quote_filename(struct Buffer *buf, const char *filename, bool add_outer)
{
  if (!buf || !filename)
    return;

  buf_reset(buf);
  if (add_outer)
    buf_addch(buf, '\'');

  for (; *filename != '\0'; filename++)
  {
    if ((*filename == '\'') || (*filename == '`'))
    {
      buf_addch(buf, '\'');
      buf_addch(buf, '\\');
      buf_addch(buf, *filename);
      buf_addch(buf, '\'');
    }
    else
    {
      buf_addch(buf, *filename);
    }
  }

  if (add_outer)
    buf_addch(buf, '\'');
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
  if (!path || (*path == '\0'))
  {
    errno = EINVAL;
    return -1;
  }

  errno = 0;
  char tmp_path[PATH_MAX] = { 0 };
  const size_t len = strlen(path);

  if (len >= sizeof(tmp_path))
  {
    errno = ENAMETOOLONG;
    return -1;
  }

  struct stat st = { 0 };
  if ((stat(path, &st) == 0) && S_ISDIR(st.st_mode))
    return 0;

  /* Create a mutable copy */
  mutt_str_copy(tmp_path, path, sizeof(tmp_path));

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
 * @param fp Filename
 * @param st struct stat for the file (optional)
 * @retval num Updated Unix mtime
 * @retval -1  Error, see errno
 *
 * If a file's mtime is NOW, then set it to 1 second in the past.
 */
time_t mutt_file_decrease_mtime(const char *fp, struct stat *st)
{
  if (!fp)
    return -1;

  struct utimbuf utim;
  struct stat st2 = { 0 };
  time_t mtime;

  if (!st)
  {
    if (stat(fp, &st2) == -1)
      return -1;
    st = &st2;
  }

  mtime = st->st_mtime;
  if (mtime == mutt_date_now())
  {
    mtime -= 1;
    utim.actime = mtime;
    utim.modtime = mtime;
    int rc;
    do
    {
      rc = utime(fp, &utim);
    } while ((rc == -1) && (errno == EINTR));

    if (rc == -1)
      return -1;
  }

  return mtime;
}

/**
 * mutt_file_set_mtime - Set the modification time of one file from another
 * @param from Filename whose mtime should be copied
 * @param to   Filename to update
 */
void mutt_file_set_mtime(const char *from, const char *to)
{
  if (!from || !to)
    return;

  struct utimbuf utim;
  struct stat st = { 0 };

  if (stat(from, &st) != -1)
  {
    utim.actime = st.st_mtime;
    utim.modtime = st.st_mtime;
    utime(to, &utim);
  }
}

/**
 * mutt_file_touch_atime - Set the access time to current time
 * @param fd File descriptor of the file to alter
 *
 * This is just as read() would do on !noatime.
 * Silently ignored if futimens() isn't supported.
 */
void mutt_file_touch_atime(int fd)
{
#ifdef HAVE_FUTIMENS
  struct timespec times[2] = { { 0, UTIME_NOW }, { 0, UTIME_OMIT } };
  futimens(fd, times);
#endif
}

/**
 * mutt_file_chmod - Set permissions of a file
 * @param path Filename
 * @param mode the permissions to set
 * @retval num Same as chmod(2)
 *
 * This is essentially chmod(path, mode), see chmod(2).
 */
int mutt_file_chmod(const char *path, mode_t mode)
{
  if (!path)
    return -1;

  return chmod(path, mode);
}

/**
 * mutt_file_chmod_add - Add permissions to a file
 * @param path Filename
 * @param mode the permissions to add
 * @retval num Same as chmod(2)
 *
 * Adds the given permissions to the file. Permissions not mentioned in mode
 * will stay as they are. This function resembles the `chmod ugoa+rwxXst`
 * command family. Example:
 *
 *     mutt_file_chmod_add(path, S_IWUSR | S_IWGRP | S_IWOTH);
 *
 * will add write permissions to path but does not alter read and other
 * permissions.
 *
 * @sa mutt_file_chmod_add_stat()
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
 * @retval num Same as chmod(2)
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
 * @sa mutt_file_chmod_add()
 */
int mutt_file_chmod_add_stat(const char *path, mode_t mode, struct stat *st)
{
  if (!path)
    return -1;

  struct stat st2 = { 0 };

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
 * @retval num Same as chmod(2)
 *
 * Removes the given permissions from the file. Permissions not mentioned in
 * mode will stay as they are. This function resembles the `chmod ugoa-rwxXst`
 * command family. Example:
 *
 *     mutt_file_chmod_rm(path, S_IWUSR | S_IWGRP | S_IWOTH);
 *
 * will remove write permissions from path but does not alter read and other
 * permissions.
 *
 * @sa mutt_file_chmod_rm_stat()
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
 * @retval num Same as chmod(2)
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
 * @sa mutt_file_chmod_rm()
 */
int mutt_file_chmod_rm_stat(const char *path, mode_t mode, struct stat *st)
{
  if (!path)
    return -1;

  struct stat st2 = { 0 };

  if (!st)
  {
    if (stat(path, &st2) == -1)
      return -1;
    st = &st2;
  }
  return chmod(path, st->st_mode & ~mode);
}

#if defined(USE_FCNTL)
/**
 * mutt_file_lock - (Try to) Lock a file using fcntl()
 * @param fd      File descriptor to file
 * @param excl    If true, try to lock exclusively
 * @param timeout If true, Retry #MAX_LOCK_ATTEMPTS times
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Use fcntl() to lock a file.
 *
 * Use mutt_file_unlock() to unlock the file.
 */
int mutt_file_lock(int fd, bool excl, bool timeout)
{
  struct stat st = { 0 }, prev_sb = { 0 };
  int count = 0;
  int attempt = 0;

  struct flock lck;
  memset(&lck, 0, sizeof(struct flock));
  lck.l_type = excl ? F_WRLCK : F_RDLCK;
  lck.l_whence = SEEK_SET;

  while (fcntl(fd, F_SETLK, &lck) == -1)
  {
    mutt_debug(LL_DEBUG1, "fcntl errno %d\n", errno);
    if ((errno != EAGAIN) && (errno != EACCES))
    {
      mutt_perror("fcntl");
      return -1;
    }

    if (fstat(fd, &st) != 0)
      st.st_size = 0;

    if (count == 0)
      prev_sb = st;

    /* only unlock file if it is unchanged */
    if ((prev_sb.st_size == st.st_size) && (++count >= (timeout ? MAX_LOCK_ATTEMPTS : 0)))
    {
      if (timeout)
        mutt_error(_("Timeout exceeded while attempting fcntl lock"));
      return -1;
    }

    prev_sb = st;

    mutt_message(_("Waiting for fcntl lock... %d"), ++attempt);
    sleep(1);
  }

  return 0;
}

/**
 * mutt_file_unlock - Unlock a file previously locked by mutt_file_lock()
 * @param fd File descriptor to file
 * @retval 0 Always
 */
int mutt_file_unlock(int fd)
{
  struct flock unlockit;

  memset(&unlockit, 0, sizeof(struct flock));
  unlockit.l_type = F_UNLCK;
  unlockit.l_whence = SEEK_SET;
  (void) fcntl(fd, F_SETLK, &unlockit);

  return 0;
}
#elif defined(USE_FLOCK)
/**
 * mutt_file_lock - (Try to) Lock a file using flock()
 * @param fd      File descriptor to file
 * @param excl    If true, try to lock exclusively
 * @param timeout If true, Retry #MAX_LOCK_ATTEMPTS times
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Use flock() to lock a file.
 *
 * Use mutt_file_unlock() to unlock the file.
 */
int mutt_file_lock(int fd, bool excl, bool timeout)
{
  struct stat st = { 0 }, prev_sb = { 0 };
  int rc = 0;
  int count = 0;
  int attempt = 0;

  while (flock(fd, (excl ? LOCK_EX : LOCK_SH) | LOCK_NB) == -1)
  {
    if (errno != EWOULDBLOCK)
    {
      mutt_perror("flock");
      rc = -1;
      break;
    }

    if (fstat(fd, &st) != 0)
      st.st_size = 0;

    if (count == 0)
      prev_sb = st;

    /* only unlock file if it is unchanged */
    if ((prev_sb.st_size == st.st_size) && (++count >= (timeout ? MAX_LOCK_ATTEMPTS : 0)))
    {
      if (timeout)
        mutt_error(_("Timeout exceeded while attempting flock lock"));
      rc = -1;
      break;
    }

    prev_sb = st;

    mutt_message(_("Waiting for flock attempt... %d"), ++attempt);
    sleep(1);
  }

  /* release any other locks obtained in this routine */
  if (rc != 0)
  {
    flock(fd, LOCK_UN);
  }

  return rc;
}

/**
 * mutt_file_unlock - Unlock a file previously locked by mutt_file_lock()
 * @param fd File descriptor to file
 * @retval 0 Always
 */
int mutt_file_unlock(int fd)
{
  flock(fd, LOCK_UN);
  return 0;
}
#else
#error "You must select a locking mechanism via USE_FCNTL or USE_FLOCK"
#endif

/**
 * mutt_file_unlink_empty - Delete a file if it's empty
 * @param path File to delete
 */
void mutt_file_unlink_empty(const char *path)
{
  if (!path)
    return;

  struct stat st = { 0 };

  int fd = open(path, O_RDWR);
  if (fd == -1)
    return;

  if (mutt_file_lock(fd, true, true) == -1)
  {
    close(fd);
    return;
  }

  if ((fstat(fd, &st) == 0) && (st.st_size == 0))
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
 * @note on access(2) use No dangling symlink problems here due to
 * mutt_file_fopen().
 */
int mutt_file_rename(const char *oldfile, const char *newfile)
{
  if (!oldfile || !newfile)
    return -1;
  if (access(oldfile, F_OK) != 0)
    return 1;
  if (access(newfile, F_OK) == 0)
    return 2;

  FILE *fp_old = fopen(oldfile, "r");
  if (!fp_old)
    return 3;
  FILE *fp_new = mutt_file_fopen(newfile, "w");
  if (!fp_new)
  {
    mutt_file_fclose(&fp_old);
    return 3;
  }
  mutt_file_copy_stream(fp_old, fp_new);
  mutt_file_fclose(&fp_new);
  mutt_file_fclose(&fp_old);
  mutt_file_unlink(oldfile);
  return 0;
}

/**
 * mutt_file_read_keyword - Read a keyword from a file
 * @param file   File to read
 * @param buf    Buffer to store the keyword
 * @param buflen Length of the buf
 * @retval ptr Start of the keyword
 *
 * Read one line from the start of a file.
 * Skip any leading whitespace and extract the first token.
 */
char *mutt_file_read_keyword(const char *file, char *buf, size_t buflen)
{
  FILE *fp = mutt_file_fopen(file, "r");
  if (!fp)
    return NULL;

  buf = fgets(buf, buflen, fp);
  mutt_file_fclose(&fp);

  if (!buf)
    return NULL;

  SKIPWS(buf);
  char *start = buf;

  while ((*buf != '\0') && !isspace(*buf))
    buf++;

  *buf = '\0';

  return start;
}

/**
 * mutt_file_check_empty - Is the mailbox empty
 * @param path Path to mailbox
 * @retval  1 Mailbox is empty
 * @retval  0 Mailbox is not empty
 * @retval -1 Error
 */
int mutt_file_check_empty(const char *path)
{
  if (!path)
    return -1;

  struct stat st = { 0 };
  if (stat(path, &st) == -1)
    return -1;

  return st.st_size == 0;
}

/**
 * buf_file_expand_fmt_quote - Replace `%s` in a string with a filename
 * @param dest    Buffer for the result
 * @param fmt     printf-like format string
 * @param src     Filename to substitute
 *
 * This function also quotes the file to prevent shell problems.
 */
void buf_file_expand_fmt_quote(struct Buffer *dest, const char *fmt, const char *src)
{
  struct Buffer tmp = buf_make(PATH_MAX);

  buf_quote_filename(&tmp, src, true);
  mutt_file_expand_fmt(dest, fmt, buf_string(&tmp));
  buf_dealloc(&tmp);
}

/**
 * mutt_file_expand_fmt - Replace `%s` in a string with a filename
 * @param dest    Buffer for the result
 * @param fmt     printf-like format string
 * @param src     Filename to substitute
 */
void mutt_file_expand_fmt(struct Buffer *dest, const char *fmt, const char *src)
{
  if (!dest || !fmt || !src)
    return;

  const char *p = NULL;
  bool found = false;

  buf_reset(dest);

  for (p = fmt; *p; p++)
  {
    if (*p == '%')
    {
      switch (p[1])
      {
        case '%':
          buf_addch(dest, *p++);
          break;
        case 's':
          found = true;
          buf_addstr(dest, src);
          p++;
          break;
        default:
          buf_addch(dest, *p);
          break;
      }
    }
    else
    {
      buf_addch(dest, *p);
    }
  }

  if (!found)
  {
    buf_addch(dest, ' ');
    buf_addstr(dest, src);
  }
}

/**
 * mutt_file_get_size - Get the size of a file
 * @param path File to measure
 * @retval num Size in bytes
 * @retval 0   Error
 */
long mutt_file_get_size(const char *path)
{
  if (!path)
    return 0;

  struct stat st = { 0 };
  if (stat(path, &st) != 0)
    return 0;

  return st.st_size;
}

/**
 * mutt_file_get_size_fp - Get the size of a file
 * @param fp FILE* to measure
 * @retval num Size in bytes
 * @retval 0   Error
 */
long mutt_file_get_size_fp(FILE *fp)
{
  if (!fp)
    return 0;

  struct stat st = { 0 };
  if (fstat(fileno(fp), &st) != 0)
    return 0;

  return st.st_size;
}

/**
 * mutt_file_timespec_compare - Compare to time values
 * @param a First time value
 * @param b Second time value
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_file_timespec_compare(struct timespec *a, struct timespec *b)
{
  if (!a || !b)
    return 0;
  if (a->tv_sec < b->tv_sec)
    return -1;
  if (a->tv_sec > b->tv_sec)
    return 1;

  if (a->tv_nsec < b->tv_nsec)
    return -1;
  if (a->tv_nsec > b->tv_nsec)
    return 1;
  return 0;
}

/**
 * mutt_file_get_stat_timespec - Read the stat() time into a time value
 * @param dest Time value to populate
 * @param st   stat info
 * @param type Type of stat info to read, e.g. #MUTT_STAT_ATIME
 */
void mutt_file_get_stat_timespec(struct timespec *dest, struct stat *st, enum MuttStatType type)
{
  if (!dest || !st)
    return;

  dest->tv_sec = 0;
  dest->tv_nsec = 0;

  switch (type)
  {
    case MUTT_STAT_ATIME:
      dest->tv_sec = st->st_atime;
#ifdef HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC
      dest->tv_nsec = st->st_atim.tv_nsec;
#endif
      break;
    case MUTT_STAT_MTIME:
      dest->tv_sec = st->st_mtime;
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
      dest->tv_nsec = st->st_mtim.tv_nsec;
#endif
      break;
    case MUTT_STAT_CTIME:
      dest->tv_sec = st->st_ctime;
#ifdef HAVE_STRUCT_STAT_ST_CTIM_TV_NSEC
      dest->tv_nsec = st->st_ctim.tv_nsec;
#endif
      break;
  }
}

/**
 * mutt_file_stat_timespec_compare - Compare stat info with a time value
 * @param st   stat info
 * @param type Type of stat info, e.g. #MUTT_STAT_ATIME
 * @param b    Time value
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_file_stat_timespec_compare(struct stat *st, enum MuttStatType type,
                                    struct timespec *b)
{
  if (!st || !b)
    return 0;

  struct timespec a = { 0 };

  mutt_file_get_stat_timespec(&a, st, type);
  return mutt_file_timespec_compare(&a, b);
}

/**
 * mutt_file_stat_compare - Compare two stat infos
 * @param st1      First stat info
 * @param st1_type Type of first stat info, e.g. #MUTT_STAT_ATIME
 * @param st2      Second stat info
 * @param st2_type Type of second stat info, e.g. #MUTT_STAT_ATIME
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
int mutt_file_stat_compare(struct stat *st1, enum MuttStatType st1_type,
                           struct stat *st2, enum MuttStatType st2_type)
{
  if (!st1 || !st2)
    return 0;

  struct timespec a = { 0 };
  struct timespec b = { 0 };

  mutt_file_get_stat_timespec(&a, st1, st1_type);
  mutt_file_get_stat_timespec(&b, st2, st2_type);
  return mutt_file_timespec_compare(&a, &b);
}

/**
 * mutt_file_resolve_symlink - Resolve a symlink in place
 * @param buf Input/output path
 */
void mutt_file_resolve_symlink(struct Buffer *buf)
{
  struct stat st = { 0 };
  int rc = lstat(buf_string(buf), &st);
  if ((rc != -1) && S_ISLNK(st.st_mode))
  {
    char path[PATH_MAX] = { 0 };
    if (realpath(buf_string(buf), path))
    {
      buf_strcpy(buf, path);
    }
  }
}
