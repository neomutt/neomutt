/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1998-2001,2007 Thomas Roessler <roessler@does-not-exist.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

/*
 * This module either be compiled into Mutt, or it can be
 * built as a separate program. For building it
 * separately, define the DL_STANDALONE preprocessor
 * macro.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>

#ifndef _POSIX_PATH_MAX
#include <limits.h>
#endif

#include "dotlock.h"

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#ifdef DL_STANDALONE
# include "reldate.h"
#endif

#define MAXLINKS 1024 /* maximum link depth */

#ifdef DL_STANDALONE

# define LONG_STRING 1024
# define MAXLOCKATTEMPT 5

# define strfcpy(A,B,C) strncpy (A,B,C), *(A+(C)-1)=0

# ifdef USE_SETGID

#  ifdef HAVE_SETEGID
#   define SETEGID setegid
#  else
#   define SETEGID setgid
#  endif
#  ifndef S_ISLNK
#   define S_ISLNK(x) (((x) & S_IFMT) == S_IFLNK ? 1 : 0)
#  endif

# endif

# ifndef HAVE_SNPRINTF
extern int snprintf (char *, size_t, const char *, ...);
# endif

#else  /* DL_STANDALONE */

# ifdef USE_SETGID
#   error Do not try to compile dotlock as a mutt module when requiring egid switching!
# endif

# include "mutt.h"
# include "mx.h"

#endif /* DL_STANDALONE */

static int DotlockFlags;
static int Retry = MAXLOCKATTEMPT;

#ifdef DL_STANDALONE
static char *Hostname;
#endif

#ifdef USE_SETGID
static gid_t UserGid;
static gid_t MailGid;
#endif

static int dotlock_deference_symlink (char *, size_t, const char *);
static int dotlock_prepare (char *, size_t, const char *, int fd);
static int dotlock_check_stats (struct stat *, struct stat *);
static int dotlock_dispatch (const char *, int fd);

#ifdef DL_STANDALONE
static int dotlock_init_privs (void);
static void usage (const char *);
#endif

static void dotlock_expand_link (char *, const char *, const char *);
static void BEGIN_PRIVILEGED (void);
static void END_PRIVILEGED (void);

/* These functions work on the current directory.
 * Invoke dotlock_prepare () before and check their
 * return value.
 */

static int dotlock_try (void);
static int dotlock_unlock (const char *);
static int dotlock_unlink (const char *);
static int dotlock_lock (const char *);


#ifdef DL_STANDALONE

#define check_flags(a) if (a & DL_FL_ACTIONS) usage (argv[0])

int main (int argc, char **argv)
{
  int i;
  char *p;
  struct utsname utsname;

  /* first, drop privileges */
  
  if (dotlock_init_privs () == -1)
    return DL_EX_ERROR;


  /* determine the system's host name */
  
  uname (&utsname);
  if (!(Hostname = strdup (utsname.nodename)))	/* __MEM_CHECKED__ */
    return DL_EX_ERROR;
  if ((p = strchr (Hostname, '.')))
    *p = '\0';


  /* parse the command line options. */
  DotlockFlags = 0;
  
  while ((i = getopt (argc, argv, "dtfupr:")) != EOF)
  {
    switch (i)
    {
      /* actions, mutually exclusive */
      case 't': check_flags (DotlockFlags); DotlockFlags |= DL_FL_TRY; break;
      case 'd': check_flags (DotlockFlags); DotlockFlags |= DL_FL_UNLINK; break;
      case 'u': check_flags (DotlockFlags); DotlockFlags |= DL_FL_UNLOCK; break;

      /* other flags */
      case 'f': DotlockFlags |= DL_FL_FORCE; break;
      case 'p': DotlockFlags |= DL_FL_USEPRIV; break;
      case 'r': DotlockFlags |= DL_FL_RETRY; Retry = atoi (optarg); break;
      
      default: usage (argv[0]);
    }
  }

  if (optind == argc || Retry < 0)
    usage (argv[0]);

  return dotlock_dispatch (argv[optind], -1);
}


/* 
 * Determine our effective group ID, and drop 
 * privileges.
 * 
 * Return value:
 * 
 *  0 - everything went fine
 * -1 - we couldn't drop privileges.
 * 
 */


static int
dotlock_init_privs (void)
{

# ifdef USE_SETGID
  
  UserGid = getgid ();
  MailGid = getegid ();

  if (SETEGID (UserGid) != 0)
    return -1;

# endif

  return 0;
}
  

#else  /* DL_STANDALONE */

/* 
 * This function is intended to be invoked from within
 * mutt instead of mx.c's invoke_dotlock ().
 */

int dotlock_invoke (const char *path, int fd, int flags, int retry)
{
  int currdir;
  int r;

  DotlockFlags = flags;
  
  if ((currdir = open (".", O_RDONLY)) == -1)
    return DL_EX_ERROR;

  if (!(DotlockFlags & DL_FL_RETRY) || retry)
    Retry = MAXLOCKATTEMPT;
  else
    Retry = 0;
  
  r = dotlock_dispatch (path, fd);
  
  fchdir (currdir);
  close (currdir);
  
  return r;
}

#endif  /* DL_STANDALONE */


static int dotlock_dispatch (const char *f, int fd)
{
  char realpath[_POSIX_PATH_MAX];

  /* If dotlock_prepare () succeeds [return value == 0],
   * realpath contains the basename of f, and we have
   * successfully changed our working directory to
   * `dirname $f`.  Additionally, f has been opened for
   * reading to verify that the user has at least read
   * permissions on that file.
   * 
   * For a more detailed explanation of all this, see the
   * lengthy comment below.
   */

  if (dotlock_prepare (realpath, sizeof (realpath), f, fd) != 0)
    return DL_EX_ERROR;

  /* Actually perform the locking operation. */

  if (DotlockFlags & DL_FL_TRY)
    return dotlock_try ();
  else if (DotlockFlags & DL_FL_UNLOCK)
    return dotlock_unlock (realpath);
  else if (DotlockFlags & DL_FL_UNLINK)
    return dotlock_unlink (realpath);
  else /* lock */
    return dotlock_lock (realpath);
}

  
/*
 * Get privileges 
 * 
 * This function re-acquires the privileges we may have
 * if the user told us to do so by giving the "-p"
 * command line option.
 * 
 * BEGIN_PRIVILEGES () won't return if an error occurs.
 * 
 */

static void
BEGIN_PRIVILEGED (void)
{
#ifdef USE_SETGID
  if (DotlockFlags & DL_FL_USEPRIV)
  {
    if (SETEGID (MailGid) != 0)
    {
      /* perror ("setegid"); */
      exit (DL_EX_ERROR);
    }
  }
#endif
}

/*
 * Drop privileges
 * 
 * This function drops the group privileges we may have.
 * 
 * END_PRIVILEGED () won't return if an error occurs.
 *
 */

static void
END_PRIVILEGED (void)
{
#ifdef USE_SETGID
  if (DotlockFlags & DL_FL_USEPRIV)
  {
    if (SETEGID (UserGid) != 0)
    {
      /* perror ("setegid"); */
      exit (DL_EX_ERROR);
    }
  }
#endif
}

#ifdef DL_STANDALONE

/*
 * Usage information.
 * 
 * This function doesn't return.
 * 
 */

static void 
usage (const char *av0)
{
  fprintf (stderr, "dotlock [Mutt %s (%s)]\n", VERSION, ReleaseDate);
  fprintf (stderr, "usage: %s [-t|-f|-u|-d] [-p] [-r <retries>] file\n",
	  av0);

  fputs ("\noptions:"
	"\n  -t\t\ttry"
	"\n  -f\t\tforce"
	"\n  -u\t\tunlock"
	"\n  -d\t\tunlink"
	"\n  -p\t\tprivileged"
#ifndef USE_SETGID
	" (ignored)"
#endif
	"\n  -r <retries>\tRetry locking"
	"\n", stderr);
  
  exit (DL_EX_ERROR);
}

#endif

/*
 * Access checking: Let's avoid to lock other users' mail
 * spool files if we aren't permitted to read them.
 * 
 * Some simple-minded access (2) checking isn't sufficient
 * here: The problem is that the user may give us a
 * deeply nested path to a file which has the same name
 * as the file he wants to lock, but different
 * permissions, say, e.g.
 * /tmp/lots/of/subdirs/var/spool/mail/root.
 * 
 * He may then try to replace /tmp/lots/of/subdirs by a
 * symbolic link to / after we have invoked access () to
 * check the file's permissions.  The lockfile we'd
 * create or remove would then actually be
 * /var/spool/mail/root.
 * 
 * To avoid this attack, we proceed as follows:
 * 
 * - First, follow symbolic links a la
 *   dotlock_deference_symlink ().
 * 
 * - get the result's dirname.
 * 
 * - chdir to this directory.  If you can't, bail out.
 * 
 * - try to open the file in question, only using its
 *   basename.  If you can't, bail out.
 * 
 * - fstat that file and compare the result to a
 *   subsequent lstat (only using the basename).  If
 *   the comparison fails, bail out.
 * 
 * dotlock_prepare () is invoked from main () directly
 * after the command line parsing has been done.
 *
 * Return values:
 * 
 * 0 - Evereything's fine.  The program's new current
 *     directory is the contains the file to be locked.
 *     The string pointed to by bn contains the name of
 *     the file to be locked.
 * 
 * -1 - Something failed. Don't continue.
 * 
 * tlr, Jul 15 1998
 */

static int
dotlock_check_stats (struct stat *fsb, struct stat *lsb)
{
  /* S_ISLNK (fsb->st_mode) should actually be impossible,
   * but we may have mixed up the parameters somewhere.
   * play safe.
   */

  if (S_ISLNK (lsb->st_mode) || S_ISLNK (fsb->st_mode))
    return -1;
  
  if ((lsb->st_dev != fsb->st_dev) ||
     (lsb->st_ino != fsb->st_ino) ||
     (lsb->st_mode != fsb->st_mode) ||
     (lsb->st_nlink != fsb->st_nlink) ||
     (lsb->st_uid != fsb->st_uid) ||
     (lsb->st_gid != fsb->st_gid) ||
     (lsb->st_rdev != fsb->st_rdev) ||
     (lsb->st_size != fsb->st_size))
  {
    /* something's fishy */
    return -1;
  }
  
  return 0;
}

static int
dotlock_prepare (char *bn, size_t l, const char *f, int _fd)
{
  struct stat fsb, lsb;
  char realpath[_POSIX_PATH_MAX];
  char *basename, *dirname;
  char *p;
  int fd;
  int r;
  
  if (dotlock_deference_symlink (realpath, sizeof (realpath), f) == -1)
    return -1;
  
  if ((p = strrchr (realpath, '/')))
  {
    *p = '\0';
    basename = p + 1;
    dirname = realpath;
  }
  else
  {
    basename = realpath;
    dirname = ".";
  }

  if (strlen (basename) + 1 > l)
    return -1;
  
  strfcpy (bn, basename, l);
  
  if (chdir (dirname) == -1)
    return -1;

  if (_fd != -1)
    fd = _fd;
  else if ((fd = open (basename, O_RDONLY)) == -1)
    return -1;
  
  r = fstat (fd, &fsb);
  
  if (_fd == -1)
    close (fd);
  
  if (r == -1)
    return -1;
  
  if (lstat (basename, &lsb) == -1)
    return -1;

  if (dotlock_check_stats (&fsb, &lsb) == -1)
    return -1;

  return 0;
}

/*
 * Expand a symbolic link.
 * 
 * This function expects newpath to have space for
 * at least _POSIX_PATH_MAX characters.
 *
 */

static void 
dotlock_expand_link (char *newpath, const char *path, const char *link)
{
  const char *lb = NULL;
  size_t len;

  /* link is full path */
  if (*link == '/')
  {
    strfcpy (newpath, link, _POSIX_PATH_MAX);
    return;
  }

  if ((lb = strrchr (path, '/')) == NULL)
  {
    /* no path in link */
    strfcpy (newpath, link, _POSIX_PATH_MAX);
    return;
  }

  len = lb - path + 1;
  memcpy (newpath, path, len);
  strfcpy (newpath + len, link, _POSIX_PATH_MAX - len);
}


/*
 * Deference a chain of symbolic links
 * 
 * The final path is written to d.
 *
 */

static int
dotlock_deference_symlink (char *d, size_t l, const char *path)
{
  struct stat sb;
  char realpath[_POSIX_PATH_MAX];
  const char *pathptr = path;
  int count = 0;
  
  while (count++ < MAXLINKS)
  {
    if (lstat (pathptr, &sb) == -1)
    {
      /* perror (pathptr); */
      return -1;
    }
    
    if (S_ISLNK (sb.st_mode))
    {
      char linkfile[_POSIX_PATH_MAX];
      char linkpath[_POSIX_PATH_MAX];
      int len;

      if ((len = readlink (pathptr, linkfile, sizeof (linkfile) - 1)) == -1)
      {
	/* perror (pathptr); */
	return -1;
      }
      
      linkfile[len] = '\0';
      dotlock_expand_link (linkpath, pathptr, linkfile);
      strfcpy (realpath, linkpath, sizeof (realpath));
      pathptr = realpath;
    }
    else
      break;
  }

  strfcpy (d, pathptr, l);
  return 0;
}

/*
 * Dotlock a file.
 * 
 * realpath is assumed _not_ to be an absolute path to
 * the file we are about to lock.  Invoke
 * dotlock_prepare () before using this function!
 * 
 */

#define HARDMAXATTEMPTS 10

static int
dotlock_lock (const char *realpath)
{
  char lockfile[_POSIX_PATH_MAX + LONG_STRING];
  char nfslockfile[_POSIX_PATH_MAX + LONG_STRING];
  size_t prev_size = 0;
  int fd;
  int count = 0;
  int hard_count = 0;
  struct stat sb;
  time_t t;
  
  snprintf (nfslockfile, sizeof (nfslockfile), "%s.%s.%d",
	   realpath, Hostname, (int) getpid ());
  snprintf (lockfile, sizeof (lockfile), "%s.lock", realpath);

  
  BEGIN_PRIVILEGED ();

  unlink (nfslockfile);

  while ((fd = open (nfslockfile, O_WRONLY | O_EXCL | O_CREAT, 0)) < 0)
  {
    END_PRIVILEGED ();

  
    if (errno != EAGAIN)
    {
      /* perror ("cannot open NFS lock file"); */
      return DL_EX_ERROR;
    }

    
    BEGIN_PRIVILEGED ();
  }

  END_PRIVILEGED ();

  
  close (fd);
  
  while (hard_count++ < HARDMAXATTEMPTS)
  {

    BEGIN_PRIVILEGED ();
    link (nfslockfile, lockfile);
    END_PRIVILEGED ();

    if (stat (nfslockfile, &sb) != 0)
    {
      /* perror ("stat"); */
      return DL_EX_ERROR;
    }

    if (sb.st_nlink == 2)
      break;

    if (count == 0)
      prev_size = sb.st_size;

    if (prev_size == sb.st_size && ++count > Retry)
    {
      if (DotlockFlags & DL_FL_FORCE)
      {
	BEGIN_PRIVILEGED ();
	unlink (lockfile);
	END_PRIVILEGED ();

	count = 0;
	continue;
      }
      else
      {
	BEGIN_PRIVILEGED ();
	unlink (nfslockfile);
	END_PRIVILEGED ();
	return DL_EX_EXIST;
      }
    }
    
    prev_size = sb.st_size;
    
    /* don't trust sleep (3) as it may be interrupted
     * by users sending signals. 
     */
    
    t = time (NULL);
    do {
      sleep (1);
    } while (time (NULL) == t);
  }

  BEGIN_PRIVILEGED ();
  unlink (nfslockfile);
  END_PRIVILEGED ();

  return DL_EX_OK;
}


/*
 * Unlock a file. 
 * 
 * The same comment as for dotlock_lock () applies here.
 * 
 */

static int
dotlock_unlock (const char *realpath)
{
  char lockfile[_POSIX_PATH_MAX + LONG_STRING];
  int i;

  snprintf (lockfile, sizeof (lockfile), "%s.lock",
	   realpath);
  
  BEGIN_PRIVILEGED ();
  i = unlink (lockfile);
  END_PRIVILEGED ();
  
  if (i == -1)
    return DL_EX_ERROR;
  
  return DL_EX_OK;
}

/* remove an empty file */

static int
dotlock_unlink (const char *realpath)
{
  struct stat lsb;
  int i = -1;

  if (dotlock_lock (realpath) != DL_EX_OK)
    return DL_EX_ERROR;

  if ((i = lstat (realpath, &lsb)) == 0 && lsb.st_size == 0)
    unlink (realpath);

  dotlock_unlock (realpath);

  return (i == 0) ?  DL_EX_OK : DL_EX_ERROR;
}


/*
 * Check if a file can be locked at all.
 * 
 * The same comment as for dotlock_lock () applies here.
 * 
 */

static int
dotlock_try (void)
{
#ifdef USE_SETGID
  struct stat sb;
#endif

  if (access (".", W_OK) == 0)
    return DL_EX_OK;

#ifdef USE_SETGID
  if (stat (".", &sb) == 0)
  {
    if ((sb.st_mode & S_IWGRP) == S_IWGRP && sb.st_gid == MailGid)
      return DL_EX_NEED_PRIVS;
  }
#endif

  return DL_EX_IMPOSSIBLE;
}
