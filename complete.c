/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"

#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

/* given a partial pathname, this routine fills in as much of the rest of the
 * path as is unique.
 *
 * return 0 if ok, -1 if no matches
 */
int mutt_complete (char *s)
{
  char *p;
  DIR *dirp;
  struct dirent *de;
  int i ,init=0;
  size_t len;
  char dirpart[_POSIX_PATH_MAX], exp_dirpart[_POSIX_PATH_MAX];
  char filepart[_POSIX_PATH_MAX];
  
  if (*s == '=' || *s == '+')
  {
    dirpart[0] = *s;
    dirpart[1] = 0;
    strfcpy (exp_dirpart, NONULL (Maildir), sizeof (exp_dirpart));
    if ((p = strrchr (s, '/')))
    {
      *p++ = 0;
      sprintf (exp_dirpart + mutt_strlen (exp_dirpart), "/%s", s+1);
      sprintf (dirpart + mutt_strlen (dirpart), "%s/", s+1);
      strfcpy (filepart, p, sizeof (filepart));
    }
    else
      strfcpy (filepart, s + 1, sizeof (filepart));
    dirp = opendir (exp_dirpart);
  }
  else
  {
    if ((p = strrchr (s, '/')))
    {
      if (p == s) /* absolute path */
      {
	p = s + 1;
	strfcpy (dirpart, "/", sizeof (dirpart));
	exp_dirpart[0] = 0;
	strfcpy (filepart, p, sizeof (filepart));
	dirp = opendir (dirpart);
      }
      else
      {
	*p = 0;
	len = (size_t)(p - s);
	strncpy (dirpart, s, len);
	dirpart[len]=0;
	p++;
	strfcpy (filepart, p, sizeof (filepart));
	strfcpy (exp_dirpart, dirpart, sizeof (exp_dirpart));
	mutt_expand_path (exp_dirpart, sizeof (exp_dirpart));
	dirp = opendir (exp_dirpart);
      }
    }
    else
    {
      /* no directory name, so assume current directory. */
      dirpart[0] = 0;
      strfcpy (filepart, s, sizeof (filepart));
      dirp = opendir (".");
    }
  }

  if (dirp == NULL)
  {
    dprint (1, (debugfile, "mutt_complete(): %s: %s (errno %d).\n", exp_dirpart, strerror (errno), errno));
    return (-1);
  }

  /*
   * special case to handle when there is no filepart yet.  find the first
   * file/directory which is not ``.'' or ``..''
   */
  if ((len = mutt_strlen (filepart)) == 0)
  {
    while ((de = readdir (dirp)) != NULL)
    {
      if (mutt_strcmp (".", de->d_name) != 0 && mutt_strcmp ("..", de->d_name) != 0)
      {
	strfcpy (filepart, de->d_name, sizeof (filepart));
	init++;
	break;
      }
    }
  }

  while ((de = readdir (dirp)) != NULL)
  {
    if (mutt_strncmp (de->d_name, filepart, len) == 0)
    {
      if (init)
      {
	for (i=0; filepart[i] && de->d_name[i]; i++)
	{
	  if (filepart[i] != de->d_name[i])
	  {
	    filepart[i] = 0;
	    break;
	  }
	}
	filepart[i] = 0;
      }
      else
      {
	char buf[_POSIX_PATH_MAX];
	struct stat st;

	strfcpy (filepart, de->d_name, sizeof(filepart));

	/* check to see if it is a directory */
	if (dirpart[0])
	{
	  strfcpy (buf, exp_dirpart, sizeof (buf));
	  strcat (buf, "/");
	}
	else
	  buf[0] = 0;
	strcat (buf, filepart);
	if (stat (buf, &st) != -1 && (st.st_mode & S_IFDIR))
	  strcat (filepart, "/");
	init = 1;
      }
    }
  }
  closedir (dirp);

  if (dirpart[0])
  {
    strcpy (s, dirpart);
    if (mutt_strcmp ("/", dirpart) != 0 && dirpart[0] != '=' && dirpart[0] != '+')
      strcat (s, "/");
    strcat (s, filepart);
  }
  else
    strcpy (s, filepart);

  return (init ? 0 : -1);
}
