/*
 * Copyright (C) 1996-2000,2007 Michael R. Elkins <me@mutt.org>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "attach.h"
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
#include <errno.h>
#include <locale.h>

static const struct mapping_t FolderHelp[] = {
  { N_("Exit"),  OP_EXIT },
  { N_("Chdir"), OP_CHANGE_DIRECTORY },
  { N_("Mask"),  OP_ENTER_MASK },
  { N_("Help"),  OP_HELP },
  { NULL,	 0 }
};

typedef struct folder_t
{
  struct folder_file *ff;
  int num;
} FOLDER;

static char LastDir[_POSIX_PATH_MAX] = "";
static char LastDirBackup[_POSIX_PATH_MAX] = "";

/* Frees up the memory allocated for the local-global variables.  */
static void destroy_state (struct browser_state *state)
{
  int c;

  for (c = 0; c < state->entrylen; c++)
  {
    FREE (&((state->entry)[c].name));
    FREE (&((state->entry)[c].desc));
    FREE (&((state->entry)[c].st));
  }
#ifdef USE_IMAP
  FREE (&state->folder);
#endif
  FREE (&state->entry);
}

static int browser_compare_subject (const void *a, const void *b)
{
  struct folder_file *pa = (struct folder_file *) a;
  struct folder_file *pb = (struct folder_file *) b;

  int r = mutt_strcoll (pa->name, pb->name);

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

static int link_is_dir (const char *folder, const char *path)
{
  struct stat st;
  char fullpath[_POSIX_PATH_MAX];
  
  mutt_concat_path (fullpath, folder, path, sizeof (fullpath));
  
  if (stat (fullpath, &st) == 0)
    return (S_ISDIR (st.st_mode));
  else
    return 0;
}

static const char *
folder_format_str (char *dest, size_t destlen, size_t col, char op, const char *src,
		   const char *fmt, const char *ifstring, const char *elsestring,
		   unsigned long data, format_flag flags)
{
  char fn[SHORT_STRING], tmp[SHORT_STRING], permission[11];
  char date[16], *t_fmt;
  time_t tnow;
  FOLDER *folder = (FOLDER *) data;
  struct passwd *pw;
  struct group *gr;
  int optional = (flags & M_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'C':
      snprintf (tmp, sizeof (tmp), "%%%sd", fmt);
      snprintf (dest, destlen, tmp, folder->num + 1);
      break;
      
    case 'd':
    case 'D':
      if (folder->ff->st != NULL)
      {
	int do_locales = TRUE;

	if (op == 'D') {
	  t_fmt = NONULL(DateFmt);
	  if (*t_fmt == '!') {
	    ++t_fmt;
	    do_locales = FALSE;
	  }
	} else {
	  tnow = time (NULL);
	  t_fmt = tnow - folder->ff->st->st_mtime < 31536000 ? "%b %d %H:%M" : "%b %d  %Y";
	}
	if (do_locales)
	  setlocale(LC_TIME, NONULL (Locale)); /* use environment if $locale is not set */
	else
	  setlocale(LC_TIME, "C");
	strftime (date, sizeof (date), t_fmt, localtime (&folder->ff->st->st_mtime));

	mutt_format_s (dest, destlen, fmt, date);
      }
      else
	mutt_format_s (dest, destlen, fmt, "");
      break;
      
    case 'f':
    {
      char *s;
#ifdef USE_IMAP
      if (folder->ff->imap)
	s = NONULL (folder->ff->desc);
      else
#endif
	s = NONULL (folder->ff->name);

      snprintf (fn, sizeof (fn), "%s%s", s,
		folder->ff->st ? (S_ISLNK (folder->ff->st->st_mode) ? "@" :		
				  (S_ISDIR (folder->ff->st->st_mode) ? "/" : 
				   ((folder->ff->st->st_mode & S_IXUSR) != 0 ? "*" : ""))) : "");
      
      mutt_format_s (dest, destlen, fmt, fn);
      break;
    }
    case 'F':
      if (folder->ff->st != NULL)
      {
	snprintf (permission, sizeof (permission), "%c%c%c%c%c%c%c%c%c%c",
		  S_ISDIR(folder->ff->st->st_mode) ? 'd' : (S_ISLNK(folder->ff->st->st_mode) ? 'l' : '-'),
		  (folder->ff->st->st_mode & S_IRUSR) != 0 ? 'r': '-',
		  (folder->ff->st->st_mode & S_IWUSR) != 0 ? 'w' : '-',
		  (folder->ff->st->st_mode & S_ISUID) != 0 ? 's' : (folder->ff->st->st_mode & S_IXUSR) != 0 ? 'x': '-',
		  (folder->ff->st->st_mode & S_IRGRP) != 0 ? 'r' : '-',
		  (folder->ff->st->st_mode & S_IWGRP) != 0 ? 'w' : '-',
		  (folder->ff->st->st_mode & S_ISGID) != 0 ? 's' : (folder->ff->st->st_mode & S_IXGRP) != 0 ? 'x': '-',
		  (folder->ff->st->st_mode & S_IROTH) != 0 ? 'r' : '-',
		  (folder->ff->st->st_mode & S_IWOTH) != 0 ? 'w' : '-',
		  (folder->ff->st->st_mode & S_ISVTX) != 0 ? 't' : (folder->ff->st->st_mode & S_IXOTH) != 0 ? 'x': '-');
	mutt_format_s (dest, destlen, fmt, permission);
      }
#ifdef USE_IMAP
      else if (folder->ff->imap)
      {
	/* mark folders with subfolders AND mail */
	snprintf (permission, sizeof (permission), "IMAP %c",
		  (folder->ff->inferiors && folder->ff->selectable) ? '+' : ' ');
	mutt_format_s (dest, destlen, fmt, permission);
      }                                        
#endif
      else
	mutt_format_s (dest, destlen, fmt, "");
      break;
      
    case 'g':
      if (folder->ff->st != NULL)
      {
	if ((gr = getgrgid (folder->ff->st->st_gid)))
	  mutt_format_s (dest, destlen, fmt, gr->gr_name);
	else
	{
	  snprintf (tmp, sizeof (tmp), "%%%sld", fmt);
	  snprintf (dest, destlen, tmp, folder->ff->st->st_gid);
	}
      }
      else
	mutt_format_s (dest, destlen, fmt, "");
      break;
      
    case 'l':
      if (folder->ff->st != NULL)
      {
	snprintf (tmp, sizeof (tmp), "%%%sd", fmt);
	snprintf (dest, destlen, tmp, folder->ff->st->st_nlink);
      }
      else
	mutt_format_s (dest, destlen, fmt, "");
      break;
      
    case 'N':
#ifdef USE_IMAP
      if (mx_is_imap (folder->ff->desc))
      {
	if (!optional)
	{
	  snprintf (tmp, sizeof (tmp), "%%%sd", fmt);
	  snprintf (dest, destlen, tmp, folder->ff->new);
	}
	else if (!folder->ff->new)
	  optional = 0;
	break;
      }
#endif
      snprintf (tmp, sizeof (tmp), "%%%sc", fmt);
      snprintf (dest, destlen, tmp, folder->ff->new ? 'N' : ' ');
      break;
      
    case 's':
      if (folder->ff->st != NULL)
      {
	mutt_pretty_size(fn, sizeof(fn), folder->ff->st->st_size);
	snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
	snprintf (dest, destlen, tmp, fn);
      }
      else
	mutt_format_s (dest, destlen, fmt, "");
      break;

    case 't':
      snprintf (tmp, sizeof (tmp), "%%%sc", fmt);
      snprintf (dest, destlen, tmp, folder->ff->tagged ? '*' : ' ');
      break;

    case 'u':
      if (folder->ff->st != NULL)
      {
	if ((pw = getpwuid (folder->ff->st->st_uid)))
	  mutt_format_s (dest, destlen, fmt, pw->pw_name);
	else
	{
	  snprintf (tmp, sizeof (tmp), "%%%sld", fmt);
	  snprintf (dest, destlen, tmp, folder->ff->st->st_uid);
	}
      }
      else
	mutt_format_s (dest, destlen, fmt, "");
      break;

    default:
      snprintf (tmp, sizeof (tmp), "%%%sc", fmt);
      snprintf (dest, destlen, tmp, op);
      break;
  }

  if (optional)
    mutt_FormatString (dest, destlen, col, ifstring, folder_format_str, data, 0);
  else if (flags & M_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, col, elsestring, folder_format_str, data, 0);

  return (src);
}

static void add_folder (MUTTMENU *m, struct browser_state *state,
			const char *name, const struct stat *s, unsigned int new)
{
  if (state->entrylen == state->entrymax)
  {
    /* need to allocate more space */
    safe_realloc (&state->entry,
		  sizeof (struct folder_file) * (state->entrymax += 256));
    memset (&state->entry[state->entrylen], 0,
	    sizeof (struct folder_file) * 256);
    if (m)
      m->data = state->entry;
  }

  if (s != NULL)
  {
    (state->entry)[state->entrylen].mode = s->st_mode;
    (state->entry)[state->entrylen].mtime = s->st_mtime;
    (state->entry)[state->entrylen].size = s->st_size;
    
    (state->entry)[state->entrylen].st = safe_malloc (sizeof (struct stat));
    memcpy ((state->entry)[state->entrylen].st, s, sizeof (struct stat));
  }

  (state->entry)[state->entrylen].new = new;
  (state->entry)[state->entrylen].name = safe_strdup (name);
  (state->entry)[state->entrylen].desc = safe_strdup (name);
#ifdef USE_IMAP
  (state->entry)[state->entrylen].imap = 0;
#endif
  (state->entrylen)++;
}

static void init_state (struct browser_state *state, MUTTMENU *menu)
{
  state->entrylen = 0;
  state->entrymax = 256;
  state->entry = (struct folder_file *) safe_calloc (state->entrymax, sizeof (struct folder_file));
#ifdef USE_IMAP
  state->imap_browse = 0;
#endif
  if (menu)
    menu->data = state->entry;
}

static int examine_directory (MUTTMENU *menu, struct browser_state *state,
			      char *d, const char *prefix)
{
  struct stat s;
  DIR *dp;
  struct dirent *de;
  char buffer[_POSIX_PATH_MAX + SHORT_STRING];
  BUFFY *tmp;

  while (stat (d, &s) == -1)
  {
    if (errno == ENOENT)
    {
      /* The last used directory is deleted, try to use the parent dir. */
      char *c = strrchr (d, '/');

      if (c && (c > d))
      {
	*c = 0;
	continue;
      }
    }
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

    mutt_concat_path (buffer, d, de->d_name, sizeof (buffer));
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
#ifdef USE_IMAP
  struct mailbox_state mbox;
#endif

  if (!Incoming)
    return (-1);
  mutt_buffy_check (0);

  init_state (state, menu);

  do
  {
#ifdef USE_IMAP
    if (mx_is_imap (tmp->path))
    {
      imap_mailbox_state (tmp->path, &mbox);
      add_folder (menu, state, tmp->path, NULL, mbox.new);
      continue;
    }
#endif
#ifdef USE_POP
    if (mx_is_pop (tmp->path))
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

    if (mx_is_maildir (tmp->path))
    {
      struct stat st2;
      char md[_POSIX_PATH_MAX];

      snprintf (md, sizeof (md), "%s/new", tmp->path);
      if (stat (md, &s) < 0)
	s.st_mtime = 0;
      snprintf (md, sizeof (md), "%s/cur", tmp->path);
      if (stat (md, &st2) < 0)
	st2.st_mtime = 0;
      if (st2.st_mtime > s.st_mtime)
	s.st_mtime = st2.st_mtime;
    }
    
    strfcpy (buffer, NONULL(tmp->path), sizeof (buffer));
    mutt_pretty_mailbox (buffer, sizeof (buffer));

    add_folder (menu, state, buffer, &s, tmp->new);
  }
  while ((tmp = tmp->next));
  browser_sort (state);
  return 0;
}

static int select_file_search (MUTTMENU *menu, regex_t *re, int n)
{
  return (regexec (re, ((struct folder_file *) menu->data)[n].name, 0, NULL, 0));
}

static void folder_entry (char *s, size_t slen, MUTTMENU *menu, int num)
{
  FOLDER folder;

  folder.ff = &((struct folder_file *) menu->data)[num];
  folder.num = num;
  
  mutt_FormatString (s, slen, 0, NONULL(FolderFormat), folder_format_str, 
      (unsigned long) &folder, M_FORMAT_ARROWCURSOR);
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

  menu->tagged = 0;
  
  if (buffy)
    snprintf (title, titlelen, _("Mailboxes [%d]"), mutt_buffy_check (0));
  else
  {
    strfcpy (path, LastDir, sizeof (path));
    mutt_pretty_mailbox (path, sizeof (path));
#ifdef USE_IMAP
  if (state->imap_browse && option (OPTIMAPLSUB))
    snprintf (title, titlelen, _("Subscribed [%s], File mask: %s"),
	      path, NONULL (Mask.pattern));
  else
#endif
    snprintf (title, titlelen, _("Directory [%s], File mask: %s"),
	      path, NONULL(Mask.pattern));
  }
  menu->redraw = REDRAW_FULL;
}

static int file_tag (MUTTMENU *menu, int n, int m)
{
  struct folder_file *ff = &(((struct folder_file *)menu->data)[n]);
  int ot;
  if (S_ISDIR (ff->mode) || (S_ISLNK (ff->mode) && link_is_dir (LastDir, ff->name)))
  {
    mutt_error _("Can't attach a directory!");
    return 0;
  }
  
  ot = ff->tagged;
  ff->tagged = (m >= 0 ? m : !ff->tagged);
  
  return ff->tagged - ot;
}

void _mutt_select_file (char *f, size_t flen, int flags, char ***files, int *numfiles)
{
  char buf[_POSIX_PATH_MAX];
  char prefix[_POSIX_PATH_MAX] = "";
  char helpstr[LONG_STRING];
  char title[STRING];
  struct browser_state state;
  MUTTMENU *menu;
  struct stat st;
  int i, killPrefix = 0;
  int multiple = (flags & M_SEL_MULTI)  ? 1 : 0;
  int folder   = (flags & M_SEL_FOLDER) ? 1 : 0;
  int buffy    = (flags & M_SEL_BUFFY)  ? 1 : 0;

  buffy = buffy && folder;
  
  memset (&state, 0, sizeof (struct browser_state));

  if (!folder)
    strfcpy (LastDirBackup, LastDir, sizeof (LastDirBackup));

  if (*f)
  {
    mutt_expand_path (f, flen);
#ifdef USE_IMAP
    if (mx_is_imap (f))
    {
      init_state (&state, NULL);
      state.imap_browse = 1;
      if (!imap_browse (f, &state))
        strfcpy (LastDir, state.folder, sizeof (LastDir));
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
	safe_strcat (LastDir, sizeof (LastDir), "/");
	safe_strncat (LastDir, sizeof (LastDir), f, i);
      }
    }
    else
    {
      if (f[0] == '/')
	strcpy (LastDir, "/");		/* __STRCPY_CHECKED__ */
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
    if (!folder)
      getcwd (LastDir, sizeof (LastDir));
    else if (!LastDir[0])
      strfcpy (LastDir, NONULL(Maildir), sizeof (LastDir));
    
#ifdef USE_IMAP
    if (!buffy && mx_is_imap (LastDir))
    {
      init_state (&state, NULL);
      state.imap_browse = 1;
      imap_browse (LastDir, &state);
      browser_sort (&state);
    }
    else
#endif
    {
      i = mutt_strlen (LastDir);
      while (i && LastDir[--i] == '/')
        LastDir[i] = '\0';
      if (!LastDir[0])
        getcwd (LastDir, sizeof (LastDir));
    }
  }

  *f = 0;

  if (buffy)
  {
    if (examine_mailboxes (NULL, &state) == -1)
      goto bail;
  }
  else
#ifdef USE_IMAP
  if (!state.imap_browse)
#endif
  if (examine_directory (NULL, &state, LastDir, prefix) == -1)
    goto bail;

  menu = mutt_new_menu (MENU_FOLDER);
  menu->make_entry = folder_entry;
  menu->search = select_file_search;
  menu->title = title;
  menu->data = state.entry;
  if (multiple)
    menu->tag = file_tag;

  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_FOLDER,
    FolderHelp);

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
	    link_is_dir (LastDir, state.entry[menu->current].name)) 
#ifdef USE_IMAP
	    || state.entry[menu->current].inferiors
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
	    mutt_concat_path (buf, LastDir, state.entry[menu->current].name, sizeof (buf));

	  if ((mx_get_magic (buf) <= 0)
#ifdef USE_IMAP
	    || state.entry[menu->current].inferiors
#endif
	    )
	  {
	    char OldLastDir[_POSIX_PATH_MAX];

	    /* save the old directory */
	    strfcpy (OldLastDir, LastDir, sizeof (OldLastDir));

	    if (mutt_strcmp (state.entry[menu->current].name, "..") == 0)
	    {
	      if (mutt_strcmp ("..", LastDir + mutt_strlen (LastDir) - 2) == 0)
		strcat (LastDir, "/..");	/* __STRCAT_CHECKED__ */
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
		    strcat (LastDir, "/..");	/* __STRCAT_CHECKED__ */
		}
	      }
	    }
	    else if (buffy)
	    {
	      strfcpy (LastDir, state.entry[menu->current].name, sizeof (LastDir));
	      mutt_expand_path (LastDir, sizeof (LastDir));
	    }
#ifdef USE_IMAP
	    else if (state.imap_browse)
	    {
	      int n;
	      ciss_url_t url;
	      
              strfcpy (LastDir, state.entry[menu->current].name,
                sizeof (LastDir));
	      /* tack on delimiter here */
	      n = strlen (LastDir)+1;
	      
	      /* special case "" needs no delimiter */
	      url_parse_ciss (&url, state.entry[menu->current].name);
	      if (url.path &&
		  (state.entry[menu->current].delim != '\0') &&
		  (n < sizeof (LastDir)))
	      {
		LastDir[n] = '\0';
		LastDir[n-1] = state.entry[menu->current].delim;
	      }
	    }
#endif
	    else
	    {
	      char tmp[_POSIX_PATH_MAX];
	      mutt_concat_path (tmp, LastDir, state.entry[menu->current].name, sizeof (tmp));
	      strfcpy (LastDir, tmp, sizeof (LastDir));
	    }

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
	      imap_browse (LastDir, &state);
	      browser_sort (&state);
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
		goto bail;
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
          strfcpy (f, state.entry[menu->current].name, flen);
#endif
	else
	  mutt_concat_path (f, LastDir, state.entry[menu->current].name, flen);

	/* Fall through to OP_EXIT */

      case OP_EXIT:

	if (multiple)
	{
	  char **tfiles;
	  int i, j;

	  if (menu->tagged)
	  {
	    *numfiles = menu->tagged;
	    tfiles = safe_calloc (*numfiles, sizeof (char *));
	    for (i = 0, j = 0; i < state.entrylen; i++)
	    {
	      struct folder_file ff = state.entry[i];
	      char full[_POSIX_PATH_MAX];
	      if (ff.tagged)
	      {
		mutt_concat_path (full, LastDir, ff.name, sizeof (full));
		mutt_expand_path (full, sizeof (full));
		tfiles[j++] = safe_strdup (full);
	      }
	    }
	    *files = tfiles;
	  }
	  else if (f[0]) /* no tagged entries. return selected entry */
	  {
	    *numfiles = 1;
	    tfiles = safe_calloc (*numfiles, sizeof (char *));
	    mutt_expand_path (f, flen);
	    tfiles[0] = safe_strdup (f);
	    *files = tfiles;
	  }
	}

	destroy_state (&state);
	mutt_menuDestroy (&menu);
	goto bail;

      case OP_BROWSER_TELL:
        if(state.entrylen)
	  mutt_message("%s", state.entry[menu->current].name);
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
	  unset_option (OPTIMAPLSUB);
	else
	  set_option (OPTIMAPLSUB);

	mutt_ungetch (0, OP_CHECK_NEW);
	break;

      case OP_CREATE_MAILBOX:
	if (!state.imap_browse)
	{
	  mutt_error (_("Create is only supported for IMAP mailboxes"));
	  break;
	}

	if (!imap_mailbox_create (LastDir))
	{
	  /* TODO: find a way to detect if the new folder would appear in
	   *   this window, and insert it without starting over. */
	  destroy_state (&state);
	  init_state (&state, NULL);
	  state.imap_browse = 1;
	  imap_browse (LastDir, &state);
	  browser_sort (&state);
	  menu->data = state.entry;
	  menu->current = 0; 
	  menu->top = 0; 
	  init_menu (&state, menu, title, sizeof (title), buffy);
	  MAYBE_REDRAW (menu->redraw);
	}
	/* else leave error on screen */
	break;

      case OP_RENAME_MAILBOX:
	if (!state.entry[menu->current].imap)
	  mutt_error (_("Rename is only supported for IMAP mailboxes"));
	else
	{
	  int nentry = menu->current;

	  if (imap_mailbox_rename (state.entry[nentry].name) >= 0)
	  {
	    destroy_state (&state);
	    init_state (&state, NULL);
	    state.imap_browse = 1;
	    imap_browse (LastDir, &state);
	    browser_sort (&state);
	    menu->data = state.entry;
	    menu->current = 0;
	    menu->top = 0;
	    init_menu (&state, menu, title, sizeof (title), buffy);
	    MAYBE_REDRAW (menu->redraw);
	  }
	}
	break;

    case OP_DELETE_MAILBOX:
	if (!state.entry[menu->current].imap)
	  mutt_error (_("Delete is only supported for IMAP mailboxes"));
	else
        {
	  char msg[SHORT_STRING];
	  IMAP_MBOX mx;
	  int nentry = menu->current;

	  imap_parse_path (state.entry[nentry].name, &mx);
	  if (!mx.mbox)
	  {
	    mutt_error _("Cannot delete root folder");
	    break;
	  }
	  snprintf (msg, sizeof (msg), _("Really delete mailbox \"%s\"?"),
            mx.mbox);
	  if (mutt_yesorno (msg, M_NO) == M_YES)
          {
	    if (!imap_delete_mailbox (Context, mx))
            {
	      /* free the mailbox from the browser */
	      FREE (&((state.entry)[nentry].name));
	      FREE (&((state.entry)[nentry].desc));
	      /* and move all other entries up */
	      if (nentry+1 < state.entrylen)
		memmove (state.entry + nentry, state.entry + nentry + 1,
                  sizeof (struct folder_file) * (state.entrylen - (nentry+1)));
	      state.entrylen--;
	      mutt_message _("Mailbox deleted.");
	      init_menu (&state, menu, title, sizeof (title), buffy);
	      MAYBE_REDRAW (menu->redraw);
	    }
	  }
	  else
	    mutt_message _("Mailbox not deleted.");
	  FREE (&mx.mbox);
        }
        break;
#endif
      
      case OP_CHANGE_DIRECTORY:

	strfcpy (buf, LastDir, sizeof (buf));
#ifdef USE_IMAP
	if (!state.imap_browse)
#endif
	{
	  /* add '/' at the end of the directory name if not already there */
	  int len=mutt_strlen(LastDir);
	  if (len && LastDir[len-1] != '/' && sizeof (buf) > len)
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
	    imap_browse (LastDir, &state);
	    browser_sort (&state);
	    menu->data = state.entry;
	    menu->current = 0; 
	    menu->top = 0; 
	    init_menu (&state, menu, title, sizeof (title), buffy);
	  }
	  else
#endif
	  {
	    if (*buf != '/')
	    {
	      /* in case dir is relative, make it relative to LastDir,
	       * not current working dir */
	      char tmp[_POSIX_PATH_MAX];
	      mutt_concat_path (tmp, LastDir, buf, sizeof (tmp));
	      strfcpy (buf, tmp, sizeof (buf));
	    }
	    if (stat (buf, &st) == 0)
	    {
	      if (S_ISDIR (st.st_mode))
	      {
		destroy_state (&state);
		if (examine_directory (menu, &state, buf, prefix) == 0)
		  strfcpy (LastDir, buf, sizeof (LastDir));
		else
		{
		  mutt_error _("Error scanning directory.");
		  if (examine_directory (menu, &state, LastDir, prefix) == -1)
		  {
		    mutt_menuDestroy (&menu);
		    goto bail;
		  }
		}
		menu->current = 0;
		menu->top = 0;
		init_menu (&state, menu, title, sizeof (title), buffy);
	      }
	      else
		mutt_error (_("%s is not a directory."), buf);
	    }
	    else
	      mutt_perror (buf);
	  }
	}
	MAYBE_REDRAW (menu->redraw);
	break;
	
      case OP_ENTER_MASK:

	strfcpy (buf, NONULL(Mask.pattern), sizeof (buf));
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
	    FREE (&rx);
	    mutt_error ("%s", buf);
	  }
	  else
	  {
	    mutt_str_replace (&Mask.pattern, buf);
	    regfree (Mask.rx);
	    FREE (&Mask.rx);
	    Mask.rx = rx;
	    Mask.not = not;

	    destroy_state (&state);
#ifdef USE_IMAP
	    if (state.imap_browse)
	    {
	      init_state (&state, NULL);
	      state.imap_browse = 1;
	      imap_browse (LastDir, &state);
	      browser_sort (&state);
	      menu->data = state.entry;
	      init_menu (&state, menu, title, sizeof (title), buffy);
	    }
	    else
#endif
	    if (examine_directory (menu, &state, LastDir, NULL) == 0)
	      init_menu (&state, menu, title, sizeof (title), buffy);
	    else
	    {
	      mutt_error _("Error scanning directory.");
	      mutt_menuDestroy (&menu);
	      goto bail;
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
	    goto bail;
	}
#ifdef USE_IMAP
	else if (mx_is_imap (LastDir))
	{
	  init_state (&state, NULL);
	  state.imap_browse = 1;
	  imap_browse (LastDir, &state);
	  browser_sort (&state);
	  menu->data = state.entry;
	}
#endif
	else if (examine_directory (menu, &state, LastDir, prefix) == -1)
	  goto bail;
	init_menu (&state, menu, title, sizeof (title), buffy);
	break;

      case OP_BUFFY_LIST:
	mutt_buffy_list ();
	break;

      case OP_BROWSER_NEW_FILE:

	snprintf (buf, sizeof (buf), "%s/", LastDir);
	if (mutt_get_field (_("New file name: "), buf, sizeof (buf), M_FILE) == 0)
	{
	  strfcpy (f, buf, flen);
	  destroy_state (&state);
	  mutt_menuDestroy (&menu);
	  goto bail;
	}
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_BROWSER_VIEW_FILE:
	if (!state.entrylen)
	{
	  mutt_error _("No files match the file mask");
	  break;
	}

#ifdef USE_IMAP
	if (state.entry[menu->current].selectable)
	{
	  strfcpy (f, state.entry[menu->current].name, flen);
	  destroy_state (&state);
	  mutt_menuDestroy (&menu);
	  goto bail;
	}
	else
#endif
        if (S_ISDIR (state.entry[menu->current].mode) ||
	    (S_ISLNK (state.entry[menu->current].mode) &&
	    link_is_dir (LastDir, state.entry[menu->current].name)))
	{
	  mutt_error _("Can't view a directory");
	  break;
	} 
	else
	{
	  BODY *b;
	  char buf[_POSIX_PATH_MAX];
	  
	  mutt_concat_path (buf, LastDir, state.entry[menu->current].name, sizeof (buf));
	  b = mutt_make_file_attach (buf);
	  if (b != NULL)
	  {
	    mutt_view_attachment (NULL, b, M_REGULAR, NULL, NULL, 0);
	    mutt_free_body (&b);
	    menu->redraw = REDRAW_FULL;
	  }
	  else
	    mutt_error _("Error trying to view file");
	}
    }
  }
  
  bail:
  
  if (!folder)
    strfcpy (LastDir, LastDirBackup, sizeof (LastDir));
  
}
