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
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "buffy.h"
#include "mapping.h"
#include "sort.h"
#include "mailbox.h"
#include "browser.h"
#ifdef USE_IMAP
#include "imap.h"
#endif

#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

static struct mapping_t FolderHelp[] = {
  { N_("Exit"),  OP_EXIT },
  { N_("Chdir"), OP_CHANGE_DIRECTORY },
  { N_("Mask"),  OP_ENTER_MASK },
  { N_("Help"),  OP_HELP },
  { NULL }
};

typedef struct folder_t
{
  const char *name;
  const struct stat *f;
  int new;
} FOLDER;

static char LastDir[_POSIX_PATH_MAX] = "";

/* Frees up the memory allocated for the local-global variables.  */
static void destroy_state (struct browser_state *state)
{
  int c;

  for (c = 0; c < state->entrylen; c++)
  {
    safe_free ((void **) &((state->entry)[c].name));
    safe_free ((void **) &((state->entry)[c].desc));
  }
#ifdef USE_IMAP
  safe_free ((void **) &state->folder);
#endif
  safe_free ((void **) &state->entry);
}

static int browser_compare_subject (const void *a, const void *b)
{
  struct folder_file *pa = (struct folder_file *) a;
  struct folder_file *pb = (struct folder_file *) b;

  int r = mutt_strcmp (pa->name, pb->name);

  return ((BrowserSort & SORT_REVERSE) ? -r : r);
}

static int browser_compare_date (const void *a, const void *b)
{
  struct folder_file *pa = (struct folder_file *) a;
  struct folder_file *pb = (struct folder_file *) b;

  int r = pa->mtime - pb->mtime;

  return ((BrowserSort & SORT_REVERSE) ? -r : r);
}

static int browser_compare_size (const void *a, const void *b)
{
  struct folder_file *pa = (struct folder_file *) a;
  struct folder_file *pb = (struct folder_file *) b;

  int r = pa->size - pb->size;

  return ((BrowserSort & SORT_REVERSE) ? -r : r);
}

static void browser_sort (struct browser_state *state)
{
  int (*f) (const void *, const void *);

  switch (BrowserSort & SORT_MASK)
  {
    case SORT_ORDER:
      return;
    case SORT_DATE:
      f = browser_compare_date;
      break;
    case SORT_SIZE:
      f = browser_compare_size;
      break;
    case SORT_SUBJECT:
    default:
      f = browser_compare_subject;
      break;
  }
  qsort (state->entry, state->entrylen, sizeof (struct folder_file), f);
}

static int link_is_dir (const char *path)
{
  struct stat st;

  if (stat (path, &st) == 0)
    return (S_ISDIR (st.st_mode));
  else
    return (-1);
}

static const char *
folder_format_str (char *dest, size_t destlen, char op, const char *src,
		   const char *fmt, const char *ifstring, const char *elsestring,
		   unsigned long data, format_flag flags)
{
  char fn[SHORT_STRING], tmp[SHORT_STRING], permission[11];
  char date[16], *t_fmt;
  time_t tnow;
  FOLDER *folder = (FOLDER *) data;
  struct passwd *pw;
  struct group *gr;

  switch (op)
  {
    case 'd':
      if (folder->f != NULL)
      {
	tnow = time (NULL);
	t_fmt = tnow - folder->f->st_mtime < 31536000 ? "%b %d %H:%M" : "%b %d  %Y";
	strftime (date, sizeof (date), t_fmt, localtime (&folder->f->st_mtime));
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, date);
      }
      else
      {
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, "");
      }
      break;
    case 'f':
      strfcpy (fn, folder->name, sizeof(fn));
      if (folder->f != NULL)
      {
	strcat (fn, S_ISLNK (folder->f->st_mode) ? "@" : 
		(S_ISDIR (folder->f->st_mode) ? "/" : 
		 ((folder->f->st_mode & S_IXUSR) != 0 ? "*" : "")));
      }
      snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
      snprintf (dest, destlen, tmp, fn);
      break;
    case 'F':
      if (folder->f != NULL)
      {
	sprintf (permission, "%c%c%c%c%c%c%c%c%c%c",
		 S_ISDIR(folder->f->st_mode) ? 'd' : (S_ISLNK(folder->f->st_mode) ? 'l' : '-'),
		 (folder->f->st_mode & S_IRUSR) != 0 ? 'r': '-',
		 (folder->f->st_mode & S_IWUSR) != 0 ? 'w' : '-',
		 (folder->f->st_mode & S_ISUID) != 0 ? 's' : (folder->f->st_mode & S_IXUSR) != 0 ? 'x': '-',
		 (folder->f->st_mode & S_IRGRP) != 0 ? 'r' : '-',
		 (folder->f->st_mode & S_IWGRP) != 0 ? 'w' : '-',
		 (folder->f->st_mode & S_ISGID) != 0 ? 's' : (folder->f->st_mode & S_IXGRP) != 0 ? 'x': '-',
		 (folder->f->st_mode & S_IROTH) != 0 ? 'r' : '-',
		 (folder->f->st_mode & S_IWOTH) != 0 ? 'w' : '-',
		 (folder->f->st_mode & S_ISVTX) != 0 ? 't' : (folder->f->st_mode & S_IXOTH) != 0 ? 'x': '-');
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, permission);
      }
      else
      {
#ifdef USE_IMAP
	if (strchr(folder->name, '{'))
	{
	  snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	  snprintf (dest, destlen, tmp, "IMAP");
	}                                        
#endif
      }
      break;
    case 'g':
      if (folder->f != NULL)
      {
	if ((gr = getgrgid (folder->f->st_gid)))
	{
	  snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	  snprintf (dest, destlen, tmp, gr->gr_name);
	}
	else
	{
	  snprintf (tmp, sizeof (tmp), "%%%sld", fmt);
	  snprintf (dest, destlen, tmp, folder->f->st_gid);
	}
      }
      else
      {
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, "");
      }
      break;
    case 'l':
      if (folder->f != NULL)
      {
	snprintf (tmp, sizeof (tmp), "%%%sd", fmt);
	snprintf (dest, destlen, tmp, folder->f->st_nlink);
      }
      else
      {
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, "");
      }
      break;
    case 'N':
      snprintf (tmp, sizeof (tmp), "%%%sc", fmt);
      snprintf (dest, destlen, tmp, folder->new ? 'N' : ' ');
      break;
    case 's':
      if (folder->f != NULL)
      {
	snprintf (tmp, sizeof (tmp), "%%%sld", fmt);
	snprintf (dest, destlen, tmp, (long) folder->f->st_size);
      }
      else
      {
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, "");
      }
      break;
    case 'u':
      if (folder->f != NULL)
      {
	if ((pw = getpwuid (folder->f->st_uid)))
	{
	  snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	  snprintf (dest, destlen, tmp, pw->pw_name);
	}
	else
	{
	  snprintf (tmp, sizeof (tmp), "%%%sld", fmt);
	  snprintf (dest, destlen, tmp, folder->f->st_uid);
	}
      }
      else
      {
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, "");
      }
      break;
  }
  return (src);
}

static void add_folder (MUTTMENU *m, struct browser_state *state,
			const char *name, const struct stat *s, int new)
{
  char buffer[_POSIX_PATH_MAX + SHORT_STRING];
  FOLDER folder;

  folder.name = name;
  folder.f = s;
  folder.new = new;
  mutt_FormatString (buffer, sizeof (buffer), NONULL(FolderFormat),
		     folder_format_str, (unsigned long) &folder,
		     M_FORMAT_ARROWCURSOR);

  if (state->entrylen == state->entrymax)
  {
    /* need to allocate more space */
    safe_realloc ((void **) &state->entry,
		  sizeof (struct folder_file) * (state->entrymax += 256));
    if (m)
      m->data = state->entry;
  }

  if (s != NULL)
  {
  (state->entry)[state->entrylen].mode = s->st_mode;
  (state->entry)[state->entrylen].mtime = s->st_mtime;
  (state->entry)[state->entrylen].size = s->st_size;
  }
  (state->entry)[state->entrylen].name = safe_strdup (name);
  (state->entry)[state->entrylen].desc = safe_strdup (buffer);
#ifdef USE_IMAP
  (state->entry)[state->entrylen].notfolder = 0;
#endif

  (state->entrylen)++;
}

static void init_state (struct browser_state *state, MUTTMENU *menu)
{
  state->entrylen = 0;
  state->entrymax = 256;
  state->entry = (struct folder_file *) safe_malloc (sizeof (struct folder_file) * state->entrymax);
  memset (state->entry, 0, sizeof (struct folder_file) * state->entrymax);
  if (menu)
    menu->data = state->entry;
}

static int examine_directory (MUTTMENU *menu, struct browser_state *state,
			      const char *d, const char *prefix)
{
  struct stat s;
  DIR *dp;
  struct dirent *de;
  char buffer[_POSIX_PATH_MAX + SHORT_STRING];
  BUFFY *tmp;

  if (stat (d, &s) == -1)
  {
    mutt_perror (d);
    return (-1);
  }

  if (!S_ISDIR (s.st_mode))
  {
    mutt_error (_("%s is not a directory."), d);
    return (-1);
  }

  mutt_buffy_check (0);

  if ((dp = opendir (d)) == NULL)
  {
    mutt_perror (d);
    return (-1);
  }

  init_state (state, menu);

  while ((de = readdir (dp)) != NULL)
  {
    if (mutt_strcmp (de->d_name, ".") == 0)
      continue;    /* we don't need . */
    
    if (prefix && *prefix && mutt_strncmp (prefix, de->d_name, mutt_strlen (prefix)) != 0)
      continue;
    if (!((regexec (Mask.rx, de->d_name, 0, NULL, 0) == 0) ^ Mask.not))
      continue;

    snprintf (buffer, sizeof (buffer), "%s/%s", d, de->d_name);
    if (lstat (buffer, &s) == -1)
      continue;
    
    if ((! S_ISREG (s.st_mode)) && (! S_ISDIR (s.st_mode)) &&
	(! S_ISLNK (s.st_mode)))
      continue;
    
    tmp = Incoming;
    while (tmp && mutt_strcmp (buffer, tmp->path))
      tmp = tmp->next;
    add_folder (menu, state, de->d_name, &s, (tmp) ? tmp->new : 0);
  }
  closedir (dp);  
  browser_sort (state);
  return 0;
}

static int examine_mailboxes (MUTTMENU *menu, struct browser_state *state)
{
  struct stat s;
  char buffer[LONG_STRING];
  BUFFY *tmp = Incoming;

  if (!Incoming)
    return (-1);
  mutt_buffy_check (0);

  init_state (state, menu);

  do
  {
#ifdef USE_IMAP
    if (tmp->path[0] == '{')
    {
      add_folder (menu, state, tmp->path, NULL, tmp->new);
      continue;
    }
#endif
    if (lstat (tmp->path, &s) == -1)
      continue;

    if ((! S_ISREG (s.st_mode)) && (! S_ISDIR (s.st_mode)) &&
	(! S_ISLNK (s.st_mode)))
      continue;
    
    strfcpy (buffer, NONULL(tmp->path), sizeof (buffer));
    mutt_pretty_mailbox (buffer);

    add_folder (menu, state, buffer, &s, tmp->new);
  }
  while ((tmp = tmp->next));
  browser_sort (state);
  return 0;
}

int select_file_search (MUTTMENU *menu, regex_t *re, int n)
{
  return (regexec (re, ((struct folder_file *) menu->data)[n].name, 0, NULL, 0));
}

void folder_entry (char *s, size_t slen, MUTTMENU *menu, int num)
{
  snprintf (s, slen, "%2d %c %s", 
	    num + 1, 
	    ((struct folder_file *) menu->data)[num].tagged ? '*' : ' ',
	    ((struct folder_file *) menu->data)[num].desc);
}

static void init_menu (struct browser_state *state, MUTTMENU *menu, char *title,
		       size_t titlelen, int buffy)
{
  char path[_POSIX_PATH_MAX];

  menu->max = state->entrylen;

  if(menu->current >= menu->max)
    menu->current = menu->max - 1;
  if (menu->current < 0)
    menu->current = 0;
  if (menu->top > menu->current)
    menu->top = 0;

  if (buffy)
    snprintf (title, titlelen, _("Mailboxes [%d]"), mutt_buffy_check (0));
  else
  {
    strfcpy (path, LastDir, sizeof (path));
    mutt_pretty_mailbox (path);
#ifdef USE_IMAP
  if (state->imap_browse && option (OPTIMAPLSUB))
    snprintf (title, titlelen, _("Subscribed [%s], File mask: %s"),
	      path, Mask.pattern);
  else
#endif
    snprintf (title, titlelen, _("Directory [%s], File mask: %s"),
	      path, Mask.pattern);
  }
  menu->redraw = REDRAW_FULL;
}

int file_tag (MUTTMENU *menu, int n)
{
  struct folder_file *ff = &(((struct folder_file *)menu->data)[n]);
  if (S_ISDIR (ff->mode) || (S_ISLNK (ff->mode) && link_is_dir (ff->name)))
  {
    mutt_error _("Can't attach a directory!");
    return 0;
  }
  
  return ((ff->tagged = !ff->tagged) ? 1 : -1);
}

void _mutt_select_file (char *f, size_t flen, int buffy,
		       int multiple, char ***files, int *numfiles)
{
  char buf[_POSIX_PATH_MAX];
  char prefix[_POSIX_PATH_MAX] = "";
  char helpstr[SHORT_STRING];
  char title[STRING];
  struct browser_state state;
  MUTTMENU *menu;
  struct stat st;
  int i, killPrefix = 0;

  memset (&state, 0, sizeof (struct browser_state));

  if (*f)
  {
    mutt_expand_path (f, flen);
#ifdef USE_IMAP
    if (mx_is_imap (f))
    {
      init_state (&state, NULL);
      state.imap_browse = 1;
      imap_init_browse (f, &state);
      strfcpy (LastDir, f, sizeof (LastDir));
    }
    else
    {
#endif
    for (i = mutt_strlen (f) - 1; i > 0 && f[i] != '/' ; i--);
    if (i > 0)
    {
      if (f[0] == '/')
      {
	if (i > sizeof (LastDir) - 1) i = sizeof (LastDir) - 1;
	strncpy (LastDir, f, i);
	LastDir[i] = 0;
      }
      else
      {
	getcwd (LastDir, sizeof (LastDir));
	strcat (LastDir, "/");
	strncat (LastDir, f, i);
      }
    }
    else
    {
      if (f[0] == '/')
	strcpy (LastDir, "/");
      else
	getcwd (LastDir, sizeof (LastDir));
    }

    if (i <= 0 && f[0] != '/')
      strfcpy (prefix, f, sizeof (prefix));
    else
      strfcpy (prefix, f + i + 1, sizeof (prefix));
    killPrefix = 1;
#ifdef USE_IMAP
    }
#endif
  }
  else 
  {
    if (!LastDir[0])
      strfcpy (LastDir, NONULL(Maildir), sizeof (LastDir));
#ifdef USE_IMAP
    if (!buffy && mx_is_imap (LastDir))
    {
      init_state (&state, NULL);
      state.imap_browse = 1;
      imap_init_browse (LastDir, &state);
    }
#endif
  }

  *f = 0;

  if (buffy)
  {
    if (examine_mailboxes (NULL, &state) == -1)
      return;
  }
  else
#ifdef USE_IMAP
  if (!state.imap_browse)
#endif
  if (examine_directory (NULL, &state, LastDir, prefix) == -1)
    return;

  menu = mutt_new_menu ();
  menu->menu = MENU_FOLDER;
  menu->make_entry = folder_entry;
  menu->search = select_file_search;
  menu->title = title;
  menu->data = state.entry;
  if (multiple)
    menu->tag = file_tag;

  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_FOLDER, FolderHelp);

  init_menu (&state, menu, title, sizeof (title), buffy);

  FOREVER
  {
    switch (i = mutt_menuLoop (menu))
    {
      case OP_GENERIC_SELECT_ENTRY:

	if (!state.entrylen)
	{
	  mutt_error _("No files match the file mask");
	  break;
	}

        if (S_ISDIR (state.entry[menu->current].mode) ||
	    (S_ISLNK (state.entry[menu->current].mode) &&
	    link_is_dir (state.entry[menu->current].name)) 
#ifdef USE_IMAP
	    || state.entry[menu->current].notfolder
#endif
	    )
	{
	  /* make sure this isn't a MH or maildir mailbox */
	  if (buffy)
	  {
	    strfcpy (buf, state.entry[menu->current].name, sizeof (buf));
	    mutt_expand_path (buf, sizeof (buf));
	  }
#ifdef USE_IMAP
	  else if (state.imap_browse)
	  {
	    strfcpy (buf, state.entry[menu->current].name, sizeof (buf));
	  }
#endif
	  else
	    snprintf (buf, sizeof (buf), "%s/%s", LastDir, 
		      state.entry[menu->current].name);

	  if ((mx_get_magic (buf) <= 0)
#ifdef USE_IMAP
	    || state.entry[menu->current].notfolder
#endif
	    )
	  {
	    char OldLastDir[_POSIX_PATH_MAX];

	    /* save the old directory */
	    strfcpy (OldLastDir, LastDir, sizeof (OldLastDir));

	    if (mutt_strcmp (state.entry[menu->current].name, "..") == 0)
	    {
	      if (mutt_strcmp ("..", LastDir + mutt_strlen (LastDir) - 2) == 0)
		strcat (LastDir, "/..");
	      else
	      {
		char *p = strrchr (LastDir + 1, '/');

		if (p)
		  *p = 0;
		else
		{
		  if (LastDir[0] == '/')
		    LastDir[1] = 0;
		  else
		    strcat (LastDir, "/..");
		}
	      }
	    }
	    else if (buffy)
	    {
	      sprintf (LastDir, "%s", state.entry[menu->current].name);
	      mutt_expand_path (LastDir, sizeof (LastDir));
	    }
#ifdef USE_IMAP
	    else if (state.imap_browse)
	    {
	      strfcpy (LastDir, state.entry[menu->current].name, 
		  sizeof (LastDir));
	    }
#endif
	    else
	      sprintf (LastDir + mutt_strlen (LastDir), "/%s",
		       state.entry[menu->current].name);

	    destroy_state (&state);
	    if (killPrefix)
	    {
	      prefix[0] = 0;
	      killPrefix = 0;
	    }
	    buffy = 0;
#ifdef USE_IMAP
	    if (state.imap_browse)
	    {
	      init_state (&state, NULL);
	      state.imap_browse = 1;
	      imap_init_browse (LastDir, &state);
	      menu->data = state.entry;
	    }
	    else
#endif
	    if (examine_directory (menu, &state, LastDir, prefix) == -1)
	    {
	      /* try to restore the old values */
	      strfcpy (LastDir, OldLastDir, sizeof (LastDir));
	      if (examine_directory (menu, &state, LastDir, prefix) == -1)
	      {
		strfcpy (LastDir, NONULL(Homedir), sizeof (LastDir));
		return;
	      }
	    }
	    menu->current = 0; 
	    menu->top = 0; 
	    init_menu (&state, menu, title, sizeof (title), buffy);
	    break;
	  }
	}

	if (buffy)
	{
	  strfcpy (f, state.entry[menu->current].name, flen);
	  mutt_expand_path (f, flen);
	}
#ifdef USE_IMAP
	else if (state.imap_browse)
	{
	  strfcpy (f, state.entry[menu->current].name, flen);
	}
#endif
	else
	  snprintf (f, flen, "%s/%s", LastDir, state.entry[menu->current].name);

	/* Fall through to OP_EXIT */

      case OP_EXIT:

	if (multiple)
	{
	  char **tfiles;
	  int i, j;

	  if (menu->tagged)
	  {
	    *numfiles = menu->tagged;
	    tfiles = safe_malloc (*numfiles * sizeof (char *));
	    for (i = 0, j = 0; i < state.entrylen; i++)
	    {
	      struct folder_file ff = state.entry[i];
	      char full[_POSIX_PATH_MAX];
	      if (ff.tagged)
	      {
		snprintf (full, sizeof (full), "%s/%s", LastDir, ff.name);
		mutt_expand_path (full, sizeof (full));
		tfiles[j++] = safe_strdup (full);
	      }
	    }
	    *files = tfiles;
	  }
	  else if (f[0]) /* no tagged entries. return selected entry */
	  {
	    *numfiles = 1;
	    tfiles = safe_malloc (*numfiles * sizeof (char *));
	    mutt_expand_path (f, flen);
	    tfiles[0] = safe_strdup (f);
	    *files = tfiles;
	  }
	}

	destroy_state (&state);
	mutt_menuDestroy (&menu);
	return;

      case OP_BROWSER_TELL:
        if(state.entrylen)
	  mutt_message(state.entry[menu->current].name);
        break;

#ifdef USE_IMAP
      case OP_BROWSER_SUBSCRIBE:
	imap_subscribe (state.entry[menu->current].name, 1);
	break;

      case OP_BROWSER_UNSUBSCRIBE:
	imap_subscribe (state.entry[menu->current].name, 0);
	break;

      case OP_BROWSER_TOGGLE_LSUB:
	if (option (OPTIMAPLSUB))
	{
	  unset_option (OPTIMAPLSUB);
	}
	else
	{
	  set_option (OPTIMAPLSUB);
	}
	mutt_ungetch (0, OP_CHECK_NEW);
	break;
#endif
      
      case OP_CHANGE_DIRECTORY:

	strfcpy (buf, LastDir, sizeof (buf));
#ifdef USE_IMAP
	if (!state.imap_browse)
#endif
	{/* add '/' at the end of the directory name */
	int len=mutt_strlen(LastDir);
	if (sizeof (buf) > len)
	  buf[len]='/';
	}

	if (mutt_get_field (_("Chdir to: "), buf, sizeof (buf), M_FILE) == 0 &&
	    buf[0])
	{
	  buffy = 0;	  
	  mutt_expand_path (buf, sizeof (buf));
#ifdef USE_IMAP
	  if (mx_is_imap (buf))
	  {
	    strfcpy (LastDir, buf, sizeof (LastDir));
	    destroy_state (&state);
	    init_state (&state, NULL);
	    state.imap_browse = 1;
	    imap_init_browse (LastDir, &state);
	    menu->data = state.entry;
	    menu->current = 0; 
	    menu->top = 0; 
	    init_menu (&state, menu, title, sizeof (title), buffy);
	  }
	  else
#endif
	  if (stat (buf, &st) == 0)
	  {
	    if (S_ISDIR (st.st_mode))
	    {
	      strfcpy (LastDir, buf, sizeof (LastDir));
	      destroy_state (&state);
	      if (examine_directory (menu, &state, LastDir, prefix) == 0)
	      {
		menu->current = 0; 
		menu->top = 0; 
		init_menu (&state, menu, title, sizeof (title), buffy);
	      }
	      else
	      {
		mutt_error _("Error scanning directory.");
		destroy_state (&state);
		mutt_menuDestroy (&menu);
		return;
	      }
	    }
	    else
	      mutt_error (_("%s is not a directory."), buf);
	  }
	  else
	    mutt_perror (buf);
	}
	MAYBE_REDRAW (menu->redraw);
	break;
	
      case OP_ENTER_MASK:

	strfcpy (buf, Mask.pattern, sizeof (buf));
	if (mutt_get_field (_("File Mask: "), buf, sizeof (buf), 0) == 0)
	{
	  regex_t *rx = (regex_t *) safe_malloc (sizeof (regex_t));
	  char *s = buf;
	  int not = 0, err;

	  buffy = 0;
	  /* assume that the user wants to see everything */
	  if (!buf[0])
	    strfcpy (buf, ".", sizeof (buf));
	  SKIPWS (s);
	  if (*s == '!')
	  {
	    s++;
	    SKIPWS (s);
	    not = 1;
	  }

	  if ((err = REGCOMP (rx, s, REG_NOSUB)) != 0)
	  {
	    regerror (err, rx, buf, sizeof (buf));
	    regfree (rx);
	    safe_free ((void **) &rx);
	    mutt_error ("%s", buf);
	  }
	  else
	  {
	    safe_free ((void **) &Mask.pattern);
	    regfree (Mask.rx);
	    safe_free ((void **) &Mask.rx);
	    Mask.pattern = safe_strdup (buf);
	    Mask.rx = rx;
	    Mask.not = not;

	    destroy_state (&state);
	    if (examine_directory (menu, &state, LastDir, NULL) == 0)
	      init_menu (&state, menu, title, sizeof (title), buffy);
	    else
	    {
	      mutt_error _("Error scanning directory.");
	      mutt_menuDestroy (&menu);
	      return;
	    }
	    killPrefix = 0;
	    if (!state.entrylen)
	    {
	      mutt_error _("No files match the file mask");
	      break;
	    }
	  }
	}
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_SORT:
      case OP_SORT_REVERSE:

        {
	  int resort = 1;
	  int reverse = (i == OP_SORT_REVERSE);
	  
	  switch (mutt_multi_choice ((reverse) ?
	      _("Reverse sort by (d)ate, (a)lpha, si(z)e or do(n)'t sort? ") :
	      _("Sort by (d)ate, (a)lpha, si(z)e or do(n)'t sort? "),
	      _("dazn")))
	  {
	    case -1: /* abort */
	      resort = 0;
	      break;

            case 1: /* (d)ate */
	      BrowserSort = SORT_DATE;
	      break;

            case 2: /* (a)lpha */
	      BrowserSort = SORT_SUBJECT;
	      break;

            case 3: /* si(z)e */
	      BrowserSort = SORT_SIZE;
	      break;

            case 4: /* do(n)'t sort */
	      BrowserSort = SORT_ORDER;
	      resort = 0;
	      break;
	  }
	  if (resort)
	  {
	    BrowserSort |= reverse ? SORT_REVERSE : 0;
	    browser_sort (&state);
	    menu->redraw = REDRAW_FULL;
	  }
	  break;
	}

      case OP_TOGGLE_MAILBOXES:
	buffy = 1 - buffy;

      case OP_CHECK_NEW:
	destroy_state (&state);
	prefix[0] = 0;
	killPrefix = 0;

	if (buffy)
	{
	  if (examine_mailboxes (menu, &state) == -1)
	    return;
	}
#ifdef USE_IMAP
	else if (mx_is_imap (LastDir))
	{
	  init_state (&state, NULL);
	  state.imap_browse = 1;
	  imap_init_browse (LastDir, &state);
	  menu->data = state.entry;
	}
#endif
	else if (examine_directory (menu, &state, LastDir, prefix) == -1)
	  return;
	init_menu (&state, menu, title, sizeof (title), buffy);
	break;

      case OP_BROWSER_NEW_FILE:

	snprintf (buf, sizeof (buf), "%s/", LastDir);
	if (mutt_get_field (_("New file name: "), buf, sizeof (buf), M_FILE) == 0)
	{
	  strfcpy (f, buf, flen);
	  destroy_state (&state);
	  mutt_menuDestroy (&menu);
	  return;
	}
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_BROWSER_VIEW_FILE:
	if (!state.entrylen)
	{
	  mutt_error _("No files match the file mask");
	  break;
	}

        if (S_ISDIR (state.entry[menu->current].mode) ||
	    (S_ISLNK (state.entry[menu->current].mode) &&
	    link_is_dir (state.entry[menu->current].name)))
	{
	  mutt_error _("Can't view a directory");
	  break;
	} 
	else
	{
	  BODY *b;
	  char buf[_POSIX_PATH_MAX];

	  snprintf (buf, sizeof (buf), "%s/%s", LastDir,
		    state.entry[menu->current].name);
	  b = mutt_make_file_attach (buf);
	  if (b != NULL)
	  {
	    mutt_view_attachment (NULL, b, M_REGULAR);
	    mutt_free_body (&b);
	    menu->redraw = REDRAW_FULL;
	  }
	  else
	    mutt_error _("Error trying to view file");
	}
    }
  }
  /* not reached */
}
