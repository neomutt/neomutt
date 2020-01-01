/**
 * @file
 * GUI component for displaying/selecting items from a list
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
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
 * @page browser GUI component for displaying/selecting items from a list
 *
 * GUI component for displaying/selecting items from a list
 */

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/conn.h"
#include "gui/lib.h"
#include "mutt.h"
#include "browser.h"
#include "context.h"
#include "format_flags.h"
#include "globals.h"
#include "keymap.h"
#include "mutt_attach.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#include "sendlib.h"
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NNTP
#include "nntp/nntp.h"
#endif

/* These Config Variables are only used in browser.c */
bool C_BrowserAbbreviateMailboxes; ///< Config: Abbreviate mailboxes using '~' and '=' in the browser
char *C_FolderFormat; ///< Config: printf-like format string for the browser's display of folders
char *C_GroupIndexFormat; ///< Config: (nntp) printf-like format string for the browser's display of newsgroups
char *C_NewsgroupsCharset; ///< Config: (nntp) Character set of newsgroups' descriptions
bool C_ShowOnlyUnread; ///< Config: (nntp) Only show subscribed newsgroups with unread articles
short C_SortBrowser;   ///< Config: Sort method for the browser
char *C_VfolderFormat; ///< Config: (notmuch) printf-like format string for the browser's display of virtual folders

static const struct Mapping FolderHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Chdir"), OP_CHANGE_DIRECTORY },
  { N_("Goto"), OP_BROWSER_GOTO_FOLDER },
  { N_("Mask"), OP_ENTER_MASK },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

#ifdef USE_NNTP
static struct Mapping FolderNewsHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("List"), OP_TOGGLE_MAILBOXES },
  { N_("Subscribe"), OP_BROWSER_SUBSCRIBE },
  { N_("Unsubscribe"), OP_BROWSER_UNSUBSCRIBE },
  { N_("Catchup"), OP_CATCHUP },
  { N_("Mask"), OP_ENTER_MASK },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};
#endif

static struct Buffer LastDir = { 0 };
static struct Buffer LastDirBackup = { 0 };

/**
 * init_lastdir - Initialise the browser directories
 *
 * These keep track of where the browser used to be looking.
 */
static void init_lastdir(void)
{
  static bool done = false;
  if (!done)
  {
    mutt_buffer_alloc(&LastDir, PATH_MAX);
    mutt_buffer_alloc(&LastDirBackup, PATH_MAX);
    done = true;
  }
}

/**
 * mutt_browser_cleanup - Clean up working Buffers
 */
void mutt_browser_cleanup(void)
{
  mutt_buffer_dealloc(&LastDir);
  mutt_buffer_dealloc(&LastDirBackup);
}

/**
 * destroy_state - Free the BrowserState
 * @param state State to free
 *
 * Frees up the memory allocated for the local-global variables.
 */
static void destroy_state(struct BrowserState *state)
{
  for (size_t c = 0; c < state->entrylen; c++)
  {
    FREE(&((state->entry)[c].name));
    FREE(&((state->entry)[c].desc));
  }
#ifdef USE_IMAP
  FREE(&state->folder);
#endif
  FREE(&state->entry);
}

/**
 * browser_compare_subject - Compare the subject of two browser entries
 * @param a First browser entry
 * @param b Second browser entry
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int browser_compare_subject(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  /* inbox should be sorted ahead of its siblings */
  int r = mutt_inbox_cmp(pa->name, pb->name);
  if (r == 0)
    r = mutt_str_strcoll(pa->name, pb->name);
  return (C_SortBrowser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_desc - Compare the descriptions of two browser entries
 * @param a First browser entry
 * @param b Second browser entry
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int browser_compare_desc(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = mutt_str_strcoll(pa->desc, pb->desc);

  return (C_SortBrowser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_date - Compare the date of two browser entries
 * @param a First browser entry
 * @param b Second browser entry
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int browser_compare_date(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = pa->mtime - pb->mtime;

  return (C_SortBrowser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_size - Compare the size of two browser entries
 * @param a First browser entry
 * @param b Second browser entry
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int browser_compare_size(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = pa->size - pb->size;

  return (C_SortBrowser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_count - Compare the message count of two browser entries
 * @param a First browser entry
 * @param b Second browser entry
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int browser_compare_count(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = 0;
  if (pa->has_mailbox && pb->has_mailbox)
    r = pa->msg_count - pb->msg_count;
  else if (pa->has_mailbox)
    r = -1;
  else
    r = 1;

  return (C_SortBrowser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_count_new - Compare the new count of two browser entries
 * @param a First browser entry
 * @param b Second browser entry
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 */
static int browser_compare_count_new(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = 0;
  if (pa->has_mailbox && pb->has_mailbox)
    r = pa->msg_unread - pb->msg_unread;
  else if (pa->has_mailbox)
    r = -1;
  else
    r = 1;

  return (C_SortBrowser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare - Sort the items in the browser
 * @param a First item
 * @param b Second item
 * @retval -1 a precedes b
 * @retval  0 a and b are identical
 * @retval  1 b precedes a
 *
 * Wild compare function that calls the others. It's useful because it provides
 * a way to tell "../" is always on the top of the list, independently of the
 * sort method.
 */
static int browser_compare(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  if ((mutt_str_strcoll(pa->desc, "../") == 0) || (mutt_str_strcoll(pa->desc, "..") == 0))
    return -1;
  if ((mutt_str_strcoll(pb->desc, "../") == 0) || (mutt_str_strcoll(pb->desc, "..") == 0))
    return 1;

  switch (C_SortBrowser & SORT_MASK)
  {
    case SORT_COUNT:
      return browser_compare_count(a, b);
    case SORT_DATE:
      return browser_compare_date(a, b);
    case SORT_DESC:
      return browser_compare_desc(a, b);
    case SORT_SIZE:
      return browser_compare_size(a, b);
    case SORT_UNREAD:
      return browser_compare_count_new(a, b);
    case SORT_SUBJECT:
    default:
      return browser_compare_subject(a, b);
  }
}

/**
 * browser_sort - Sort the entries in the browser
 * @param state Browser state
 *
 * Call to qsort using browser_compare function.
 * Some specific sort methods are not used via NNTP.
 */
static void browser_sort(struct BrowserState *state)
{
  switch (C_SortBrowser & SORT_MASK)
  {
    /* Also called "I don't care"-sort-method. */
    case SORT_ORDER:
      return;
#ifdef USE_NNTP
    case SORT_SIZE:
    case SORT_DATE:
      if (OptNews)
        return;
#endif
    default:
      break;
  }

  qsort(state->entry, state->entrylen, sizeof(struct FolderFile), browser_compare);
}

/**
 * link_is_dir - Does this symlink point to a directory?
 * @param folder Folder
 * @param path   Link name
 * @retval true  Links to a directory
 * @retval false Otherwise
 */
static bool link_is_dir(const char *folder, const char *path)
{
  struct stat st;
  bool retval = false;

  struct Buffer *fullpath = mutt_buffer_pool_get();
  mutt_buffer_concat_path(fullpath, folder, path);

  if (stat(mutt_b2s(fullpath), &st) == 0)
    retval = S_ISDIR(st.st_mode);

  mutt_buffer_pool_release(&fullpath);

  return retval;
}

/**
 * folder_format_str - Format a string for the folder browser - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%C     | Current file number
 * | \%d     | Date/time folder was last modified
 * | \%D     | Date/time folder was last modified using `$date_format.`
 * | \%F     | File permissions
 * | \%f     | Filename (with suffix `/`, `@` or `*`)
 * | \%g     | Group name (or numeric gid, if missing)
 * | \%i     | Description of the folder
 * | \%l     | Number of hard links
 * | \%m     | Number of messages in the mailbox
 * | \%N     | N if mailbox has new mail, blank otherwise
 * | \%n     | Number of unread messages in the mailbox
 * | \%s     | Size in bytes
 * | \%t     | `*` if the file is tagged, blank otherwise
 * | \%u     | Owner name (or numeric uid, if missing)
 */
static const char *folder_format_str(char *buf, size_t buflen, size_t col, int cols,
                                     char op, const char *src, const char *prec,
                                     const char *if_str, const char *else_str,
                                     unsigned long data, MuttFormatFlags flags)
{
  char fn[128], fmt[128];
  struct Folder *folder = (struct Folder *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'C':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, folder->num + 1);
      break;

    case 'd':
    case 'D':
      if (folder->ff->local)
      {
        bool do_locales = true;

        char *t_fmt = NULL;
        if (op == 'D')
        {
          t_fmt = NONULL(C_DateFormat);
          if (*t_fmt == '!')
          {
            t_fmt++;
            do_locales = false;
          }
        }
        else
        {
          static const time_t one_year = 31536000;
          t_fmt = ((mutt_date_epoch() - folder->ff->mtime) < one_year) ?
                      "%b %d %H:%M" :
                      "%b %d  %Y";
        }

        if (!do_locales)
          setlocale(LC_TIME, "C");
        char date[128];
        mutt_date_localtime_format(date, sizeof(date), t_fmt, folder->ff->mtime);
        if (!do_locales)
          setlocale(LC_TIME, "");

        mutt_format_s(buf, buflen, prec, date);
      }
      else
        mutt_format_s(buf, buflen, prec, "");
      break;

    case 'f':
    {
      char *s = NULL;

      s = NONULL(folder->ff->name);

      snprintf(fn, sizeof(fn), "%s%s", s,
               folder->ff->local ?
                   (S_ISLNK(folder->ff->mode) ?
                        "@" :
                        (S_ISDIR(folder->ff->mode) ?
                             "/" :
                             (((folder->ff->mode & S_IXUSR) != 0) ? "*" : ""))) :
                   "");

      mutt_format_s(buf, buflen, prec, fn);
      break;
    }
    case 'F':
    {
      if (folder->ff->local)
      {
        char permission[11];
        snprintf(permission, sizeof(permission), "%c%c%c%c%c%c%c%c%c%c",
                 S_ISDIR(folder->ff->mode) ? 'd' : (S_ISLNK(folder->ff->mode) ? 'l' : '-'),
                 ((folder->ff->mode & S_IRUSR) != 0) ? 'r' : '-',
                 ((folder->ff->mode & S_IWUSR) != 0) ? 'w' : '-',
                 ((folder->ff->mode & S_ISUID) != 0) ?
                     's' :
                     ((folder->ff->mode & S_IXUSR) != 0) ? 'x' : '-',
                 ((folder->ff->mode & S_IRGRP) != 0) ? 'r' : '-',
                 ((folder->ff->mode & S_IWGRP) != 0) ? 'w' : '-',
                 ((folder->ff->mode & S_ISGID) != 0) ?
                     's' :
                     ((folder->ff->mode & S_IXGRP) != 0) ? 'x' : '-',
                 ((folder->ff->mode & S_IROTH) != 0) ? 'r' : '-',
                 ((folder->ff->mode & S_IWOTH) != 0) ? 'w' : '-',
                 ((folder->ff->mode & S_ISVTX) != 0) ?
                     't' :
                     ((folder->ff->mode & S_IXOTH) != 0) ? 'x' : '-');
        mutt_format_s(buf, buflen, prec, permission);
      }
#ifdef USE_IMAP
      else if (folder->ff->imap)
      {
        char permission[11];
        /* mark folders with subfolders AND mail */
        snprintf(permission, sizeof(permission), "IMAP %c",
                 (folder->ff->inferiors && folder->ff->selectable) ? '+' : ' ');
        mutt_format_s(buf, buflen, prec, permission);
      }
#endif
      else
        mutt_format_s(buf, buflen, prec, "");
      break;
    }

    case 'g':
      if (folder->ff->local)
      {
        struct group *gr = getgrgid(folder->ff->gid);
        if (gr)
          mutt_format_s(buf, buflen, prec, gr->gr_name);
        else
        {
          snprintf(fmt, sizeof(fmt), "%%%sld", prec);
          snprintf(buf, buflen, fmt, folder->ff->gid);
        }
      }
      else
        mutt_format_s(buf, buflen, prec, "");
      break;

    case 'i':
    {
      char *s = NULL;
      if (folder->ff->desc)
        s = folder->ff->desc;
      else
        s = folder->ff->name;

      snprintf(fn, sizeof(fn), "%s%s", s,
               folder->ff->local ?
                   (S_ISLNK(folder->ff->mode) ?
                        "@" :
                        (S_ISDIR(folder->ff->mode) ?
                             "/" :
                             (((folder->ff->mode & S_IXUSR) != 0) ? "*" : ""))) :
                   "");

      mutt_format_s(buf, buflen, prec, fn);
      break;
    }

    case 'l':
      if (folder->ff->local)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, folder->ff->nlink);
      }
      else
        mutt_format_s(buf, buflen, prec, "");
      break;

    case 'm':
      if (!optional)
      {
        if (folder->ff->has_mailbox)
        {
          snprintf(fmt, sizeof(fmt), "%%%sd", prec);
          snprintf(buf, buflen, fmt, folder->ff->msg_count);
        }
        else
          mutt_format_s(buf, buflen, prec, "");
      }
      else if (folder->ff->msg_count == 0)
        optional = false;
      break;

    case 'N':
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, folder->ff->has_new_mail ? 'N' : ' ');
      break;

    case 'n':
      if (!optional)
      {
        if (folder->ff->has_mailbox)
        {
          snprintf(fmt, sizeof(fmt), "%%%sd", prec);
          snprintf(buf, buflen, fmt, folder->ff->msg_unread);
        }
        else
          mutt_format_s(buf, buflen, prec, "");
      }
      else if (folder->ff->msg_unread == 0)
        optional = false;
      break;

    case 's':
      if (folder->ff->local)
      {
        mutt_str_pretty_size(fn, sizeof(fn), folder->ff->size);
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, fn);
      }
      else
        mutt_format_s(buf, buflen, prec, "");
      break;

    case 't':
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, folder->ff->tagged ? '*' : ' ');
      break;

    case 'u':
      if (folder->ff->local)
      {
        struct passwd *pw = getpwuid(folder->ff->uid);
        if (pw)
          mutt_format_s(buf, buflen, prec, pw->pw_name);
        else
        {
          snprintf(fmt, sizeof(fmt), "%%%sld", prec);
          snprintf(buf, buflen, fmt, folder->ff->uid);
        }
      }
      else
        mutt_format_s(buf, buflen, prec, "");
      break;

    default:
      snprintf(fmt, sizeof(fmt), "%%%sc", prec);
      snprintf(buf, buflen, fmt, op);
      break;
  }

  if (optional)
    mutt_expando_format(buf, buflen, col, cols, if_str, folder_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_expando_format(buf, buflen, col, cols, else_str, folder_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);

  return src;
}

/**
 * add_folder - Add a folder to the browser list
 * @param menu  Menu to use
 * @param state Browser state
 * @param name  Name of folder
 * @param desc  Description of folder
 * @param s     stat info for the folder
 * @param m     Mailbox
 * @param data  Data to associate with the folder
 */
static void add_folder(struct Menu *menu, struct BrowserState *state,
                       const char *name, const char *desc, const struct stat *s,
                       struct Mailbox *m, void *data)
{
  if (m && (m->flags & MB_HIDDEN))
  {
    return;
  }

  if (state->entrylen == state->entrymax)
  {
    /* need to allocate more space */
    mutt_mem_realloc(&state->entry, sizeof(struct FolderFile) * (state->entrymax += 256));
    memset(&state->entry[state->entrylen], 0, sizeof(struct FolderFile) * 256);
    if (menu)
      menu->data = state->entry;
  }

  if (s)
  {
    (state->entry)[state->entrylen].mode = s->st_mode;
    (state->entry)[state->entrylen].mtime = s->st_mtime;
    (state->entry)[state->entrylen].size = s->st_size;
    (state->entry)[state->entrylen].gid = s->st_gid;
    (state->entry)[state->entrylen].uid = s->st_uid;
    (state->entry)[state->entrylen].nlink = s->st_nlink;

    (state->entry)[state->entrylen].local = true;
  }
  else
    (state->entry)[state->entrylen].local = false;

  if (m)
  {
    (state->entry)[state->entrylen].has_mailbox = true;
    (state->entry)[state->entrylen].has_new_mail = m->has_new;
    (state->entry)[state->entrylen].msg_count = m->msg_count;
    (state->entry)[state->entrylen].msg_unread = m->msg_unread;
  }

  (state->entry)[state->entrylen].name = mutt_str_strdup(name);
  (state->entry)[state->entrylen].desc = mutt_str_strdup(desc ? desc : name);
#ifdef USE_IMAP
  (state->entry)[state->entrylen].imap = false;
#endif
#ifdef USE_NNTP
  if (OptNews)
    (state->entry)[state->entrylen].nd = data;
#endif
  (state->entrylen)++;
}

/**
 * init_state - Initialise a browser state
 * @param state BrowserState to initialise
 * @param menu  Current menu
 */
static void init_state(struct BrowserState *state, struct Menu *menu)
{
  state->entrylen = 0;
  state->entrymax = 256;
  state->entry = mutt_mem_calloc(state->entrymax, sizeof(struct FolderFile));
#ifdef USE_IMAP
  state->imap_browse = false;
#endif
  if (menu)
    menu->data = state->entry;
}

/**
 * examine_directory - Get list of all files/newsgroups with mask
 * @param menu   Current Menu
 * @param state  State of browser
 * @param d      Directory
 * @param prefix Files/newsgroups must match this prefix
 * @retval  0 Success
 * @retval -1 Error
 */
static int examine_directory(struct Menu *menu, struct BrowserState *state,
                             const char *d, const char *prefix)
{
  int rc = -1;
  struct Buffer *buf = mutt_buffer_pool_get();
#ifdef USE_NNTP
  if (OptNews)
  {
    struct NntpAccountData *adata = CurrentNewsSrv;

    init_state(state, menu);

    for (unsigned int i = 0; i < adata->groups_num; i++)
    {
      struct NntpMboxData *mdata = adata->groups_list[i];
      if (!mdata)
        continue;
      if (prefix && *prefix && !mutt_str_startswith(mdata->group, prefix, CASE_MATCH))
        continue;
      if (!mutt_regex_match(C_Mask, mdata->group))
      {
        continue;
      }
      add_folder(menu, state, mdata->group, NULL, NULL, NULL, mdata);
    }
  }
  else
#endif /* USE_NNTP */
  {
    struct stat s;
    DIR *dp = NULL;
    struct dirent *de = NULL;

    while (stat(d, &s) == -1)
    {
      if (errno == ENOENT)
      {
        /* The last used directory is deleted, try to use the parent dir. */
        char *c = strrchr(d, '/');

        if (c && (c > d))
        {
          *c = '\0';
          continue;
        }
      }
      mutt_perror(d);
      goto ed_out;
    }

    if (!S_ISDIR(s.st_mode))
    {
      mutt_error(_("%s is not a directory"), d);
      goto ed_out;
    }

    if (Context && Context->mailbox)
      mutt_mailbox_check(Context->mailbox, 0);

    dp = opendir(d);
    if (!dp)
    {
      mutt_perror(d);
      goto ed_out;
    }

    init_state(state, menu);

    struct MailboxList ml = neomutt_mailboxlist_get_all(NeoMutt, MUTT_MAILBOX_ANY);
    while ((de = readdir(dp)))
    {
      if (mutt_str_strcmp(de->d_name, ".") == 0)
        continue; /* we don't need . */

      if (prefix && *prefix && !mutt_str_startswith(de->d_name, prefix, CASE_MATCH))
      {
        continue;
      }
      if (!mutt_regex_match(C_Mask, de->d_name))
      {
        continue;
      }

      mutt_buffer_concat_path(buf, d, de->d_name);
      if (lstat(mutt_b2s(buf), &s) == -1)
        continue;

      /* No size for directories or symlinks */
      if (S_ISDIR(s.st_mode) || S_ISLNK(s.st_mode))
        s.st_size = 0;
      else if (!S_ISREG(s.st_mode))
        continue;

      struct MailboxNode *np = NULL;
      STAILQ_FOREACH(np, &ml, entries)
      {
        if (mutt_str_strcmp(mutt_b2s(buf), mailbox_path(np->mailbox)) == 0)
          break;
      }

      if (np && Context && Context->mailbox &&
          (mutt_str_strcmp(np->mailbox->realpath, Context->mailbox->realpath) == 0))
      {
        np->mailbox->msg_count = Context->mailbox->msg_count;
        np->mailbox->msg_unread = Context->mailbox->msg_unread;
      }
      add_folder(menu, state, de->d_name, NULL, &s, np ? np->mailbox : NULL, NULL);
    }
    neomutt_mailboxlist_clear(&ml);
    closedir(dp);
  }
  browser_sort(state);
  rc = 0;
ed_out:
  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * examine_mailboxes - Get list of mailboxes/subscribed newsgroups
 * @param menu  Current menu
 * @param state State of browser
 * @retval  0 Success
 * @retval -1 Error
 */
static int examine_mailboxes(struct Menu *menu, struct BrowserState *state)
{
  struct stat s;
  struct Buffer *md = NULL;
  struct Buffer *mailbox = NULL;

#ifdef USE_NNTP
  if (OptNews)
  {
    struct NntpAccountData *adata = CurrentNewsSrv;

    init_state(state, menu);

    for (unsigned int i = 0; i < adata->groups_num; i++)
    {
      struct NntpMboxData *mdata = adata->groups_list[i];
      if (mdata && (mdata->has_new_mail ||
                    (mdata->subscribed && (mdata->unread || !C_ShowOnlyUnread))))
      {
        add_folder(menu, state, mdata->group, NULL, NULL, NULL, mdata);
      }
    }
  }
  else
#endif
  {
    init_state(state, menu);

    if (TAILQ_EMPTY(&NeoMutt->accounts))
      return -1;
    mailbox = mutt_buffer_pool_get();
    md = mutt_buffer_pool_get();
    mutt_mailbox_check(Context ? Context->mailbox : NULL, 0);

    struct MailboxList ml = neomutt_mailboxlist_get_all(NeoMutt, MUTT_MAILBOX_ANY);
    struct MailboxNode *np = NULL;
    STAILQ_FOREACH(np, &ml, entries)
    {
      if (!np->mailbox)
        continue;

      if (Context && Context->mailbox &&
          (mutt_str_strcmp(np->mailbox->realpath, Context->mailbox->realpath) == 0))
      {
        np->mailbox->msg_count = Context->mailbox->msg_count;
        np->mailbox->msg_unread = Context->mailbox->msg_unread;
      }

      mutt_buffer_strcpy(mailbox, mailbox_path(np->mailbox));
      if (C_BrowserAbbreviateMailboxes)
        mutt_buffer_pretty_mailbox(mailbox);

      switch (np->mailbox->magic)
      {
        case MUTT_IMAP:
        case MUTT_POP:
          add_folder(menu, state, mutt_b2s(mailbox), np->mailbox->name, NULL,
                     np->mailbox, NULL);
          continue;
        case MUTT_NOTMUCH:
        case MUTT_NNTP:
          add_folder(menu, state, mailbox_path(np->mailbox), np->mailbox->name,
                     NULL, np->mailbox, NULL);
          continue;
        default: /* Continue */
          break;
      }

      if (lstat(mailbox_path(np->mailbox), &s) == -1)
        continue;

      if ((!S_ISREG(s.st_mode)) && (!S_ISDIR(s.st_mode)) && (!S_ISLNK(s.st_mode)))
        continue;

      if (np->mailbox->magic == MUTT_MAILDIR)
      {
        struct stat st2;

        mutt_buffer_printf(md, "%s/new", mailbox_path(np->mailbox));
        if (stat(mutt_b2s(md), &s) < 0)
          s.st_mtime = 0;
        mutt_buffer_printf(md, "%s/cur", mailbox_path(np->mailbox));
        if (stat(mutt_b2s(md), &st2) < 0)
          st2.st_mtime = 0;
        if (st2.st_mtime > s.st_mtime)
          s.st_mtime = st2.st_mtime;
      }

      add_folder(menu, state, mutt_b2s(mailbox), np->mailbox->name, &s, np->mailbox, NULL);
    }
    neomutt_mailboxlist_clear(&ml);
  }
  browser_sort(state);

  mutt_buffer_pool_release(&mailbox);
  mutt_buffer_pool_release(&md);
  return 0;
}

/**
 * select_file_search - Menu search callback for matching files - Implements Menu::menu_search()
 */
static int select_file_search(struct Menu *menu, regex_t *rx, int line)
{
#ifdef USE_NNTP
  if (OptNews)
    return regexec(rx, ((struct FolderFile *) menu->data)[line].desc, 0, NULL, 0);
#endif
  struct FolderFile current_ff = ((struct FolderFile *) menu->data)[line];
  char *search_on = current_ff.desc ? current_ff.desc : current_ff.name;

  return regexec(rx, search_on, 0, NULL, 0);
}

/**
 * folder_make_entry - Format a menu item for the folder browser - Implements Menu::menu_make_entry()
 */
static void folder_make_entry(char *buf, size_t buflen, struct Menu *menu, int line)
{
  struct Folder folder;

  folder.ff = &((struct FolderFile *) menu->data)[line];
  folder.num = line;

#ifdef USE_NNTP
  if (OptNews)
  {
    mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                        NONULL(C_GroupIndexFormat), group_index_format_str,
                        (unsigned long) &folder, MUTT_FORMAT_ARROWCURSOR);
  }
  else
#endif
  {
    mutt_expando_format(buf, buflen, 0, menu->win_index->state.cols,
                        NONULL(C_FolderFormat), folder_format_str,
                        (unsigned long) &folder, MUTT_FORMAT_ARROWCURSOR);
  }
}

/**
 * browser_highlight_default - Decide which browser item should be highlighted
 * @param state Browser state
 * @param menu  Current Menu
 *
 * This function takes a menu and a state and defines the current entry that
 * should be highlighted.
 */
static void browser_highlight_default(struct BrowserState *state, struct Menu *menu)
{
  menu->top = 0;
  /* Reset menu position to 1.
   * We do not risk overflow as the init_menu function changes
   * current if it is bigger than state->entrylen.  */
  if ((mutt_str_strcmp(state->entry[0].desc, "..") == 0) ||
      (mutt_str_strcmp(state->entry[0].desc, "../") == 0))
  {
    /* Skip the first entry, unless there's only one entry. */
    menu->current = (menu->max > 1);
  }
  else
  {
    menu->current = 0;
  }
}

/**
 * init_menu - Set up a new menu
 * @param state    Browser state
 * @param menu     Current menu
 * @param title    Buffer for the title
 * @param titlelen Length of buffer
 * @param mailbox  If true, select mailboxes
 */
static void init_menu(struct BrowserState *state, struct Menu *menu,
                      char *title, size_t titlelen, bool mailbox)
{
  menu->max = state->entrylen;

  if (menu->current >= menu->max)
    menu->current = menu->max - 1;
  if (menu->current < 0)
    menu->current = 0;
  if (menu->top > menu->current)
    menu->top = 0;

  menu->tagged = 0;

#ifdef USE_NNTP
  if (OptNews)
  {
    if (mailbox)
      snprintf(title, titlelen, _("Subscribed newsgroups"));
    else
    {
      snprintf(title, titlelen, _("Newsgroups on server [%s]"),
               CurrentNewsSrv->conn->account.host);
    }
  }
  else
#endif
  {
    if (mailbox)
    {
      menu->is_mailbox_list = true;
      snprintf(title, titlelen, _("Mailboxes [%d]"),
               mutt_mailbox_check(Context ? Context->mailbox : NULL, 0));
    }
    else
    {
      struct Buffer *path = mutt_buffer_pool_get();
      menu->is_mailbox_list = false;
      mutt_buffer_copy(path, &LastDir);
      mutt_buffer_pretty_mailbox(path);
#ifdef USE_IMAP
      if (state->imap_browse && C_ImapListSubscribed)
      {
        snprintf(title, titlelen, _("Subscribed [%s], File mask: %s"),
                 mutt_b2s(path), NONULL(C_Mask ? C_Mask->pattern : NULL));
      }
      else
#endif
      {
        snprintf(title, titlelen, _("Directory [%s], File mask: %s"),
                 mutt_b2s(path), NONULL(C_Mask ? C_Mask->pattern : NULL));
      }
      mutt_buffer_pool_release(&path);
    }
  }

  /* Browser tracking feature.
   * The goal is to highlight the good directory if LastDir is the parent dir
   * of LastDirBackup (this occurs mostly when one hit "../"). It should also work
   * properly when the user is in examine_mailboxes-mode.  */
  if (mutt_str_startswith(mutt_b2s(&LastDirBackup), mutt_b2s(&LastDir), CASE_MATCH))
  {
    char target_dir[PATH_MAX] = { 0 };

#ifdef USE_IMAP
    /* Check what kind of dir LastDirBackup is. */
    if (imap_path_probe(mutt_b2s(&LastDirBackup), NULL) == MUTT_IMAP)
    {
      mutt_str_strfcpy(target_dir, mutt_b2s(&LastDirBackup), sizeof(target_dir));
      imap_clean_path(target_dir, sizeof(target_dir));
    }
    else
#endif
      mutt_str_strfcpy(target_dir, strrchr(mutt_b2s(&LastDirBackup), '/') + 1,
                       sizeof(target_dir));

    /* If we get here, it means that LastDir is the parent directory of
     * LastDirBackup.  I.e., we're returning from a subdirectory, and we want
     * to position the cursor on the directory we're returning from. */
    bool matched = false;
    for (unsigned int i = 0; i < state->entrylen; i++)
    {
      if (mutt_str_strcmp(state->entry[i].name, target_dir) == 0)
      {
        menu->current = i;
        matched = true;
        break;
      }
    }
    if (!matched)
      browser_highlight_default(state, menu);
  }
  else
    browser_highlight_default(state, menu);

  menu->redraw = REDRAW_FULL;
}

/**
 * file_tag - Tag an entry in the menu - Implements Menu::menu_tag()
 */
static int file_tag(struct Menu *menu, int sel, int act)
{
  struct FolderFile *ff = &(((struct FolderFile *) menu->data)[sel]);
  if (S_ISDIR(ff->mode) || (S_ISLNK(ff->mode) && link_is_dir(mutt_b2s(&LastDir), ff->name)))
  {
    mutt_error(_("Can't attach a directory"));
    return 0;
  }

  bool ot = ff->tagged;
  ff->tagged = ((act >= 0) ? act : !ff->tagged);

  return ff->tagged - ot;
}

/**
 * mutt_browser_select_dir - Remember the last directory selected
 * @param f Directory name to save
 *
 * This function helps the browser to know which directory has been selected.
 * It should be called anywhere a confirm hit is done to open a new
 * directory/file which is a maildir/mbox.
 *
 * We could check if the sort method is appropriate with this feature.
 */
void mutt_browser_select_dir(const char *f)
{
  init_lastdir();

  mutt_buffer_strcpy(&LastDirBackup, f);

  /* Method that will fetch the parent path depending on the type of the path. */
  char buf[PATH_MAX];
  mutt_get_parent_path(mutt_b2s(&LastDirBackup), buf, sizeof(buf));
  mutt_buffer_strcpy(&LastDir, buf);
}

/**
 * mutt_dlg_browser_observer - Listen for config changes affecting the Browser menu - Implements ::observer_t()
 */
int mutt_dlg_browser_observer(struct NotifyCallback *nc)
{
  if (!nc->event_data || !nc->global_data)
    return -1;
  if (nc->event_type != NT_CONFIG)
    return 0;

  struct EventConfig *ec = nc->event_data;
  struct MuttWindow *dlg = nc->global_data;

  if (mutt_str_strcmp(ec->name, "status_on_top") != 0)
    return 0;

  struct MuttWindow *win_first = TAILQ_FIRST(&dlg->children);

  if ((C_StatusOnTop && (win_first->type == WT_INDEX)) ||
      (!C_StatusOnTop && (win_first->type != WT_INDEX)))
  {
    // Swap the Index and the IndexBar Windows
    TAILQ_REMOVE(&dlg->children, win_first, entries);
    TAILQ_INSERT_TAIL(&dlg->children, win_first, entries);
  }

  mutt_window_reflow(dlg);
  return 0;
}

/**
 * mutt_buffer_select_file - Let the user select a file
 * @param[in]  file     Buffer for the result
 * @param[in]  flags    Flags, see #SelectFileFlags
 * @param[out] files    Array of selected files
 * @param[out] numfiles Number of selected files
 */
void mutt_buffer_select_file(struct Buffer *file, SelectFileFlags flags,
                             char ***files, int *numfiles)
{
  char helpstr[1024];
  char title[256];
  struct BrowserState state = { 0 };
  struct Menu *menu = NULL;
  bool kill_prefix = false;
  bool multiple = (flags & MUTT_SEL_MULTI);
  bool folder = (flags & MUTT_SEL_FOLDER);
  bool mailbox = (flags & MUTT_SEL_MAILBOX);

  /* Keeps in memory the directory we were in when hitting '='
   * to go directly to $folder (#C_Folder) */
  char goto_swapper[PATH_MAX] = { 0 };

  mailbox = mailbox && folder;

  struct Buffer *OldLastDir = mutt_buffer_pool_get();
  struct Buffer *tmp = mutt_buffer_pool_get();
  struct Buffer *buf = mutt_buffer_pool_get();
  struct Buffer *prefix = mutt_buffer_pool_get();

  init_lastdir();

#ifdef USE_NNTP
  if (OptNews)
  {
    if (mutt_buffer_is_empty(file))
    {
      struct NntpAccountData *adata = CurrentNewsSrv;

      /* default state for news reader mode is browse subscribed newsgroups */
      mailbox = false;
      for (size_t i = 0; i < adata->groups_num; i++)
      {
        struct NntpMboxData *mdata = adata->groups_list[i];
        if (mdata && mdata->subscribed)
        {
          mailbox = true;
          break;
        }
      }
    }
    else
    {
      mutt_buffer_copy(prefix, file);
    }
  }
  else
#endif
      if (!mutt_buffer_is_empty(file))
  {
    mutt_buffer_expand_path(file);
#ifdef USE_IMAP
    if (imap_path_probe(mutt_b2s(file), NULL) == MUTT_IMAP)
    {
      init_state(&state, NULL);
      state.imap_browse = true;
      if (imap_browse(mutt_b2s(file), &state) == 0)
      {
        mutt_buffer_strcpy(&LastDir, state.folder);
        browser_sort(&state);
      }
    }
    else
    {
#endif
      int i;
      for (i = mutt_buffer_len(file) - 1; (i > 0) && ((mutt_b2s(file))[i] != '/'); i--)
        ;
      if (i > 0)
      {
        if ((mutt_b2s(file))[0] == '/')
          mutt_buffer_strcpy_n(&LastDir, mutt_b2s(file), i);
        else
        {
          mutt_path_getcwd(&LastDir);
          mutt_buffer_addch(&LastDir, '/');
          mutt_buffer_addstr_n(&LastDir, mutt_b2s(file), i);
        }
      }
      else
      {
        if ((mutt_b2s(file))[0] == '/')
          mutt_buffer_strcpy(&LastDir, "/");
        else
          mutt_path_getcwd(&LastDir);
      }

      if ((i <= 0) && (mutt_b2s(file)[0] != '/'))
        mutt_buffer_copy(prefix, file);
      else
        mutt_buffer_strcpy(prefix, mutt_b2s(file) + i + 1);
      kill_prefix = true;
#ifdef USE_IMAP
    }
#endif
  }
  else
  {
    if (!folder)
      mutt_path_getcwd(&LastDir);
    else
    {
      /* Whether we use the tracking feature of the browser depends
       * on which sort method we chose to use. This variable is defined
       * only to help readability of the code.  */
      bool browser_track = false;

      switch (C_SortBrowser & SORT_MASK)
      {
        case SORT_DESC:
        case SORT_SUBJECT:
        case SORT_ORDER:
          browser_track = true;
          break;
      }

      /* We use mutt_browser_select_dir to initialize the two
       * variables (LastDir, LastDirBackup) at the appropriate
       * values.
       *
       * We do it only when LastDir is not set (first pass there)
       * or when CurrentFolder and LastDirBackup are not the same.
       * This code is executed only when we list files, not when
       * we press up/down keys to navigate in a displayed list.
       *
       * We only do this when CurrentFolder has been set (ie, not
       * when listing folders on startup with "neomutt -y").
       *
       * This tracker is only used when browser_track is true,
       * meaning only with sort methods SUBJECT/DESC for now.  */
      if (CurrentFolder)
      {
        if (mutt_buffer_is_empty(&LastDir))
        {
          /* If browsing in "local"-mode, than we chose to define LastDir to
           * MailDir */
          switch (mx_path_probe(CurrentFolder, NULL))
          {
            case MUTT_IMAP:
            case MUTT_MAILDIR:
            case MUTT_MBOX:
            case MUTT_MH:
            case MUTT_MMDF:
              if (C_Folder)
                mutt_buffer_strcpy(&LastDir, C_Folder);
              else if (C_Spoolfile)
                mutt_browser_select_dir(C_Spoolfile);
              break;
            default:
              mutt_browser_select_dir(CurrentFolder);
              break;
          }
        }
        else if (mutt_str_strcmp(CurrentFolder, mutt_b2s(&LastDirBackup)) != 0)
        {
          mutt_browser_select_dir(CurrentFolder);
        }
      }

      /* When browser tracking feature is disabled, clear LastDirBackup */
      if (!browser_track)
        mutt_buffer_reset(&LastDirBackup);
    }

#ifdef USE_IMAP
    if (!mailbox && (imap_path_probe(mutt_b2s(&LastDir), NULL) == MUTT_IMAP))
    {
      init_state(&state, NULL);
      state.imap_browse = true;
      imap_browse(mutt_b2s(&LastDir), &state);
      browser_sort(&state);
    }
    else
#endif
    {
      size_t i = mutt_buffer_len(&LastDir);
      while ((i > 0) && (mutt_b2s(&LastDir)[--i] == '/'))
        LastDir.data[i] = '\0';
      mutt_buffer_fix_dptr(&LastDir);
      if (mutt_buffer_is_empty(&LastDir))
        mutt_path_getcwd(&LastDir);
    }
  }

  mutt_buffer_reset(file);

  if (mailbox)
  {
    examine_mailboxes(NULL, &state);
  }
  else
#ifdef USE_IMAP
      if (!state.imap_browse)
#endif
  {
    if (examine_directory(NULL, &state, mutt_b2s(&LastDir), mutt_b2s(prefix)) == -1)
      goto bail;
  }

  struct MuttWindow *dlg =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  dlg->type = WT_DIALOG;
  struct MuttWindow *index =
      mutt_window_new(MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_MAXIMISE,
                      MUTT_WIN_SIZE_UNLIMITED, MUTT_WIN_SIZE_UNLIMITED);
  index->type = WT_INDEX;
  struct MuttWindow *ibar = mutt_window_new(
      MUTT_WIN_ORIENT_VERTICAL, MUTT_WIN_SIZE_FIXED, 1, MUTT_WIN_SIZE_UNLIMITED);
  ibar->type = WT_INDEX_BAR;

  if (C_StatusOnTop)
  {
    mutt_window_add_child(dlg, ibar);
    mutt_window_add_child(dlg, index);
  }
  else
  {
    mutt_window_add_child(dlg, index);
    mutt_window_add_child(dlg, ibar);
  }

  notify_observer_add(NeoMutt->notify, mutt_dlg_browser_observer, dlg);
  dialog_push(dlg);

  menu = mutt_menu_new(MENU_FOLDER);

  menu->pagelen = index->state.rows;
  menu->win_index = index;
  menu->win_ibar = ibar;

  menu->menu_make_entry = folder_make_entry;
  menu->menu_search = select_file_search;
  menu->title = title;
  menu->data = state.entry;
  if (multiple)
    menu->menu_tag = file_tag;

  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_FOLDER,
#ifdef USE_NNTP
                                 OptNews ? FolderNewsHelp :
#endif
                                           FolderHelp);
  mutt_menu_push_current(menu);

  init_menu(&state, menu, title, sizeof(title), mailbox);

  int op;
  while (true)
  {
    switch (op = mutt_menu_loop(menu))
    {
      case OP_DESCEND_DIRECTORY:
      case OP_GENERIC_SELECT_ENTRY:

        if (state.entrylen == 0)
        {
          mutt_error(_("No files match the file mask"));
          break;
        }

        if (S_ISDIR(state.entry[menu->current].mode) ||
            (S_ISLNK(state.entry[menu->current].mode) &&
             link_is_dir(mutt_b2s(&LastDir), state.entry[menu->current].name))
#ifdef USE_IMAP
            || state.entry[menu->current].inferiors
#endif
        )
        {
          /* make sure this isn't a MH or maildir mailbox */
          if (mailbox)
          {
            mutt_buffer_strcpy(buf, state.entry[menu->current].name);
            mutt_buffer_expand_path(buf);
          }
#ifdef USE_IMAP
          else if (state.imap_browse)
          {
            mutt_buffer_strcpy(buf, state.entry[menu->current].name);
          }
#endif
          else
          {
            mutt_buffer_concat_path(buf, mutt_b2s(&LastDir),
                                    state.entry[menu->current].name);
          }

          enum MailboxType magic = mx_path_probe(mutt_b2s(buf), NULL);
          if ((op == OP_DESCEND_DIRECTORY) || (magic == MUTT_MAILBOX_ERROR) ||
              (magic == MUTT_UNKNOWN)
#ifdef USE_IMAP
              || state.entry[menu->current].inferiors
#endif
          )
          {
            /* save the old directory */
            mutt_buffer_copy(OldLastDir, &LastDir);

            if (mutt_str_strcmp(state.entry[menu->current].name, "..") == 0)
            {
              size_t lastdirlen = mutt_buffer_len(&LastDir);
              if ((lastdirlen > 1) &&
                  (mutt_str_strcmp("..", mutt_b2s(&LastDir) + lastdirlen - 2) == 0))
              {
                mutt_buffer_addstr(&LastDir, "/..");
              }
              else
              {
                char *p = NULL;
                if (lastdirlen > 1)
                  p = strrchr(mutt_b2s(&LastDir) + 1, '/');

                if (p)
                {
                  *p = '\0';
                  mutt_buffer_fix_dptr(&LastDir);
                }
                else
                {
                  if (mutt_b2s(&LastDir)[0] == '/')
                    mutt_buffer_strcpy(&LastDir, "/");
                  else
                    mutt_buffer_addstr(&LastDir, "/..");
                }
              }
            }
            else if (mailbox)
            {
              mutt_buffer_strcpy(&LastDir, state.entry[menu->current].name);
              mutt_buffer_expand_path(&LastDir);
            }
#ifdef USE_IMAP
            else if (state.imap_browse)
            {
              mutt_buffer_strcpy(&LastDir, state.entry[menu->current].name);
              /* tack on delimiter here */

              /* special case "" needs no delimiter */
              struct Url *url = url_parse(state.entry[menu->current].name);
              if (url->path && (state.entry[menu->current].delim != '\0'))
              {
                mutt_buffer_addch(&LastDir, state.entry[menu->current].delim);
              }
              url_free(&url);
            }
#endif
            else
            {
              mutt_buffer_concat_path(tmp, mutt_b2s(&LastDir),
                                      state.entry[menu->current].name);
              mutt_buffer_copy(&LastDir, tmp);
            }

            destroy_state(&state);
            if (kill_prefix)
            {
              mutt_buffer_reset(prefix);
              kill_prefix = false;
            }
            mailbox = false;
#ifdef USE_IMAP
            if (state.imap_browse)
            {
              init_state(&state, NULL);
              state.imap_browse = true;
              imap_browse(mutt_b2s(&LastDir), &state);
              browser_sort(&state);
              menu->data = state.entry;
            }
            else
#endif
            {
              if (examine_directory(menu, &state, mutt_b2s(&LastDir), mutt_b2s(prefix)) == -1)
              {
                /* try to restore the old values */
                mutt_buffer_copy(&LastDir, OldLastDir);
                if (examine_directory(menu, &state, mutt_b2s(&LastDir),
                                      mutt_b2s(prefix)) == -1)
                {
                  mutt_buffer_strcpy(&LastDir, NONULL(HomeDir));
                  goto bail;
                }
              }
              /* resolve paths navigated from GUI */
              if (mutt_path_realpath(LastDir.data) == 0)
                break;
            }

            browser_highlight_default(&state, menu);
            init_menu(&state, menu, title, sizeof(title), mailbox);
            goto_swapper[0] = '\0';
            break;
          }
        }
        else if (op == OP_DESCEND_DIRECTORY)
        {
          mutt_error(_("%s is not a directory"), state.entry[menu->current].name);
          break;
        }

        if (mailbox || OptNews) /* USE_NNTP */
        {
          mutt_buffer_strcpy(file, state.entry[menu->current].name);
          mutt_buffer_expand_path(file);
        }
#ifdef USE_IMAP
        else if (state.imap_browse)
          mutt_buffer_strcpy(file, state.entry[menu->current].name);
#endif
        else
          mutt_buffer_concat_path(file, mutt_b2s(&LastDir),
                                  state.entry[menu->current].name);
        /* fallthrough */

      case OP_EXIT:

        if (multiple)
        {
          char **tfiles = NULL;

          if (menu->tagged)
          {
            *numfiles = menu->tagged;
            tfiles = mutt_mem_calloc(*numfiles, sizeof(char *));
            for (int i = 0, j = 0; i < state.entrylen; i++)
            {
              struct FolderFile ff = state.entry[i];
              if (ff.tagged)
              {
                mutt_buffer_concat_path(tmp, mutt_b2s(&LastDir), ff.name);
                mutt_buffer_expand_path(tmp);
                tfiles[j++] = mutt_buffer_strdup(tmp);
              }
            }
            *files = tfiles;
          }
          else if (!mutt_buffer_is_empty(file)) /* no tagged entries. return selected entry */
          {
            *numfiles = 1;
            tfiles = mutt_mem_calloc(*numfiles, sizeof(char *));
            mutt_buffer_expand_path(file);
            tfiles[0] = mutt_buffer_strdup(file);
            *files = tfiles;
          }
        }

        destroy_state(&state);
        goto bail;

      case OP_BROWSER_TELL:
        if (state.entrylen)
          mutt_message("%s", state.entry[menu->current].name);
        break;

#ifdef USE_IMAP
      case OP_BROWSER_TOGGLE_LSUB:
        bool_str_toggle(NeoMutt->sub, "imap_list_subscribed", NULL);

        mutt_unget_event(0, OP_CHECK_NEW);
        break;

      case OP_CREATE_MAILBOX:
        if (!state.imap_browse)
        {
          mutt_error(_("Create is only supported for IMAP mailboxes"));
          break;
        }

        if (imap_mailbox_create(mutt_b2s(&LastDir)) == 0)
        {
          /* TODO: find a way to detect if the new folder would appear in
           *   this window, and insert it without starting over. */
          destroy_state(&state);
          init_state(&state, NULL);
          state.imap_browse = true;
          imap_browse(mutt_b2s(&LastDir), &state);
          browser_sort(&state);
          menu->data = state.entry;
          browser_highlight_default(&state, menu);
          init_menu(&state, menu, title, sizeof(title), mailbox);
        }
        /* else leave error on screen */
        break;

      case OP_RENAME_MAILBOX:
        if (!state.entry[menu->current].imap)
          mutt_error(_("Rename is only supported for IMAP mailboxes"));
        else
        {
          int nentry = menu->current;

          if (imap_mailbox_rename(state.entry[nentry].name) >= 0)
          {
            destroy_state(&state);
            init_state(&state, NULL);
            state.imap_browse = true;
            imap_browse(mutt_b2s(&LastDir), &state);
            browser_sort(&state);
            menu->data = state.entry;
            browser_highlight_default(&state, menu);
            init_menu(&state, menu, title, sizeof(title), mailbox);
          }
        }
        break;

      case OP_DELETE_MAILBOX:
        if (!state.entry[menu->current].imap)
          mutt_error(_("Delete is only supported for IMAP mailboxes"));
        else
        {
          char msg[128];
          int nentry = menu->current;

          // TODO(sileht): It could be better to select INBOX instead. But I
          // don't want to manipulate Context/Mailboxes/mailbox->account here for now.
          // Let's just protect neomutt against crash for now. #1417
          if (mutt_str_strcmp(mailbox_path(Context->mailbox),
                              state.entry[nentry].name) == 0)
          {
            mutt_error(_("Can't delete currently selected mailbox"));
            break;
          }

          snprintf(msg, sizeof(msg), _("Really delete mailbox \"%s\"?"),
                   state.entry[nentry].name);
          if (mutt_yesorno(msg, MUTT_NO) == MUTT_YES)
          {
            if (imap_delete_mailbox(Context->mailbox, state.entry[nentry].name) == 0)
            {
              /* free the mailbox from the browser */
              FREE(&((state.entry)[nentry].name));
              FREE(&((state.entry)[nentry].desc));
              /* and move all other entries up */
              if ((nentry + 1) < state.entrylen)
              {
                memmove(state.entry + nentry, state.entry + nentry + 1,
                        sizeof(struct FolderFile) * (state.entrylen - (nentry + 1)));
              }
              memset(&state.entry[state.entrylen - 1], 0, sizeof(struct FolderFile));
              state.entrylen--;
              mutt_message(_("Mailbox deleted"));
              init_menu(&state, menu, title, sizeof(title), mailbox);
            }
            else
              mutt_error(_("Mailbox deletion failed"));
          }
          else
            mutt_message(_("Mailbox not deleted"));
        }
        break;
#endif

      case OP_GOTO_PARENT:
      case OP_CHANGE_DIRECTORY:

#ifdef USE_NNTP
        if (OptNews)
          break;
#endif

        mutt_buffer_copy(buf, &LastDir);
#ifdef USE_IMAP
        if (!state.imap_browse)
#endif
        {
          /* add '/' at the end of the directory name if not already there */
          size_t len = mutt_buffer_len(buf);
          if ((len > 0) && (mutt_b2s(&LastDir)[len - 1] != '/'))
            mutt_buffer_addch(buf, '/');
        }

        if (op == OP_CHANGE_DIRECTORY)
        {
          /* buf comes from the buffer pool, so defaults to size 1024 */
          int ret = mutt_buffer_get_field(_("Chdir to: "), buf, MUTT_FILE);
          if ((ret != 0) && mutt_buffer_is_empty(buf))
            break;
        }
        else if (op == OP_GOTO_PARENT)
          mutt_get_parent_path(mutt_b2s(buf), buf->data, buf->dsize);

        if (!mutt_buffer_is_empty(buf))
        {
          mailbox = false;
          mutt_buffer_expand_path(buf);
#ifdef USE_IMAP
          if (imap_path_probe(mutt_b2s(buf), NULL) == MUTT_IMAP)
          {
            mutt_buffer_copy(&LastDir, buf);
            destroy_state(&state);
            init_state(&state, NULL);
            state.imap_browse = true;
            imap_browse(mutt_b2s(&LastDir), &state);
            browser_sort(&state);
            menu->data = state.entry;
            browser_highlight_default(&state, menu);
            init_menu(&state, menu, title, sizeof(title), mailbox);
          }
          else
#endif
          {
            if (mutt_b2s(buf)[0] != '/')
            {
              /* in case dir is relative, make it relative to LastDir,
               * not current working dir */
              mutt_buffer_concat_path(tmp, mutt_b2s(&LastDir), mutt_b2s(buf));
              mutt_buffer_copy(buf, tmp);
            }
            /* Resolve path from <chdir>
             * Avoids buildup such as /a/b/../../c
             * Symlinks are always unraveled to keep code simple */
            if (mutt_path_realpath(buf->data) == 0)
              break;

            struct stat st;
            if (stat(mutt_b2s(buf), &st) == 0)
            {
              if (S_ISDIR(st.st_mode))
              {
                destroy_state(&state);
                if (examine_directory(menu, &state, mutt_b2s(buf), mutt_b2s(prefix)) == 0)
                  mutt_buffer_copy(&LastDir, buf);
                else
                {
                  mutt_error(_("Error scanning directory"));
                  if (examine_directory(menu, &state, mutt_b2s(&LastDir),
                                        mutt_b2s(prefix)) == -1)
                  {
                    goto bail;
                  }
                }
                browser_highlight_default(&state, menu);
                init_menu(&state, menu, title, sizeof(title), mailbox);
              }
              else
                mutt_error(_("%s is not a directory"), mutt_b2s(buf));
            }
            else
              mutt_perror(mutt_b2s(buf));
          }
        }
        break;

      case OP_ENTER_MASK:
      {
        mutt_buffer_strcpy(buf, C_Mask ? C_Mask->pattern : NULL);
        if (mutt_get_field(_("File Mask: "), buf->data, buf->dsize, 0) != 0)
          break;

        mutt_buffer_fix_dptr(buf);

        mailbox = false;
        /* assume that the user wants to see everything */
        if (mutt_buffer_is_empty(buf))
          mutt_buffer_strcpy(buf, ".");

        struct Buffer errmsg = { 0 };
        int rc = cs_str_string_set(NeoMutt->sub->cs, "mask", mutt_b2s(buf), NULL);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
        {
          if (!mutt_buffer_is_empty(&errmsg))
          {
            mutt_error("%s", errmsg.data);
            FREE(&errmsg.data);
          }
          break;
        }

        destroy_state(&state);
#ifdef USE_IMAP
        if (state.imap_browse)
        {
          init_state(&state, NULL);
          state.imap_browse = true;
          imap_browse(mutt_b2s(&LastDir), &state);
          browser_sort(&state);
          menu->data = state.entry;
          init_menu(&state, menu, title, sizeof(title), mailbox);
        }
        else
#endif
            if (examine_directory(menu, &state, mutt_b2s(&LastDir), NULL) == 0)
          init_menu(&state, menu, title, sizeof(title), mailbox);
        else
        {
          mutt_error(_("Error scanning directory"));
          goto bail;
        }
        kill_prefix = false;
        if (state.entrylen == 0)
        {
          mutt_error(_("No files match the file mask"));
          break;
        }
        break;
      }

      case OP_SORT:
      case OP_SORT_REVERSE:

      {
        bool resort = true;
        int sort = -1;
        int reverse = (op == OP_SORT_REVERSE);

        switch (mutt_multi_choice(
            (reverse) ?
                /* L10N: The highlighted letters must match the "Sort" options */
                _("Reverse sort by (d)ate, (a)lpha, si(z)e, d(e)scription, "
                  "(c)ount, ne(w) count, or do(n)'t sort?") :
                /* L10N: The highlighted letters must match the "Reverse Sort" options */
                _("Sort by (d)ate, (a)lpha, si(z)e, d(e)scription, (c)ount, "
                  "ne(w) count, or do(n)'t sort?"),
            /* L10N: These must match the highlighted letters from "Sort" and "Reverse Sort" */
            _("dazecwn")))
        {
          case -1: /* abort */
            resort = false;
            break;

          case 1: /* (d)ate */
            sort = SORT_DATE;
            break;

          case 2: /* (a)lpha */
            sort = SORT_SUBJECT;
            break;

          case 3: /* si(z)e */
            sort = SORT_SIZE;
            break;

          case 4: /* d(e)scription */
            sort = SORT_DESC;
            break;

          case 5: /* (c)ount */
            sort = SORT_COUNT;
            break;

          case 6: /* ne(w) count */
            sort = SORT_UNREAD;
            break;

          case 7: /* do(n)'t sort */
            sort = SORT_ORDER;
            resort = false;
            break;
        }
        if (resort)
        {
          sort |= reverse ? SORT_REVERSE : 0;
          cs_str_native_set(NeoMutt->sub->cs, "sort_browser", sort, NULL);
          browser_sort(&state);
          browser_highlight_default(&state, menu);
          menu->redraw = REDRAW_FULL;
        }
        else
        {
          cs_str_native_set(NeoMutt->sub->cs, "sort_browser", sort, NULL);
        }
        break;
      }

      case OP_TOGGLE_MAILBOXES:
      case OP_BROWSER_GOTO_FOLDER:
      case OP_CHECK_NEW:
        if (op == OP_TOGGLE_MAILBOXES)
          mailbox = !mailbox;

        if (op == OP_BROWSER_GOTO_FOLDER)
        {
          /* When in mailboxes mode, disables this feature */
          if (C_Folder)
          {
            mutt_debug(LL_DEBUG3, "= hit! Folder: %s, LastDir: %s\n", C_Folder,
                       mutt_b2s(&LastDir));
            if (goto_swapper[0] == '\0')
            {
              if (mutt_str_strcmp(mutt_b2s(&LastDir), C_Folder) != 0)
              {
                /* Stores into goto_swapper LastDir, and swaps to C_Folder */
                mutt_str_strfcpy(goto_swapper, mutt_b2s(&LastDir), sizeof(goto_swapper));
                mutt_buffer_copy(&LastDirBackup, &LastDir);
                mutt_buffer_strcpy(&LastDir, C_Folder);
              }
            }
            else
            {
              mutt_buffer_copy(&LastDirBackup, &LastDir);
              mutt_buffer_strcpy(&LastDir, goto_swapper);
              goto_swapper[0] = '\0';
            }
          }
        }
        destroy_state(&state);
        mutt_buffer_reset(prefix);
        kill_prefix = false;

        if (mailbox)
        {
          examine_mailboxes(menu, &state);
        }
#ifdef USE_IMAP
        else if (imap_path_probe(mutt_b2s(&LastDir), NULL) == MUTT_IMAP)
        {
          init_state(&state, NULL);
          state.imap_browse = true;
          imap_browse(mutt_b2s(&LastDir), &state);
          browser_sort(&state);
          menu->data = state.entry;
        }
#endif
        else if (examine_directory(menu, &state, mutt_b2s(&LastDir), mutt_b2s(prefix)) == -1)
          goto bail;
        init_menu(&state, menu, title, sizeof(title), mailbox);
        break;

      case OP_MAILBOX_LIST:
        mutt_mailbox_list();
        break;

      case OP_BROWSER_NEW_FILE:
        mutt_buffer_printf(buf, "%s/", mutt_b2s(&LastDir));
        /* buf comes from the buffer pool, so defaults to size 1024 */
        if (mutt_buffer_get_field(_("New file name: "), buf, MUTT_FILE) == 0)
        {
          mutt_buffer_copy(file, buf);
          destroy_state(&state);
          goto bail;
        }
        break;

      case OP_BROWSER_VIEW_FILE:
        if (state.entrylen == 0)
        {
          mutt_error(_("No files match the file mask"));
          break;
        }

#ifdef USE_IMAP
        if (state.entry[menu->current].selectable)
        {
          mutt_buffer_strcpy(file, state.entry[menu->current].name);
          destroy_state(&state);
          goto bail;
        }
        else
#endif
            if (S_ISDIR(state.entry[menu->current].mode) ||
                (S_ISLNK(state.entry[menu->current].mode) &&
                 link_is_dir(mutt_b2s(&LastDir), state.entry[menu->current].name)))
        {
          mutt_error(_("Can't view a directory"));
          break;
        }
        else
        {
          char buf2[PATH_MAX];

          mutt_path_concat(buf2, mutt_b2s(&LastDir),
                           state.entry[menu->current].name, sizeof(buf2));
          struct Body *b = mutt_make_file_attach(buf2);
          if (b)
          {
            mutt_view_attachment(NULL, b, MUTT_VA_REGULAR, NULL, NULL, menu->win_index);
            mutt_body_free(&b);
            menu->redraw = REDRAW_FULL;
          }
          else
            mutt_error(_("Error trying to view file"));
        }
        break;

#ifdef USE_NNTP
      case OP_CATCHUP:
      case OP_UNCATCHUP:
        if (OptNews)
        {
          struct FolderFile *ff = &state.entry[menu->current];
          struct NntpMboxData *mdata = NULL;

          int rc = nntp_newsrc_parse(CurrentNewsSrv);
          if (rc < 0)
            break;

          if (op == OP_CATCHUP)
            mdata = mutt_newsgroup_catchup(Context->mailbox, CurrentNewsSrv, ff->name);
          else
            mdata = mutt_newsgroup_uncatchup(Context->mailbox, CurrentNewsSrv, ff->name);

          if (mdata)
          {
            nntp_newsrc_update(CurrentNewsSrv);
            if ((menu->current + 1) < menu->max)
              menu->current++;
            menu->redraw = REDRAW_MOTION_RESYNC;
          }
          if (rc)
            menu->redraw = REDRAW_INDEX;
          nntp_newsrc_close(CurrentNewsSrv);
        }
        break;

      case OP_LOAD_ACTIVE:
        if (OptNews)
        {
          struct NntpAccountData *adata = CurrentNewsSrv;

          if (nntp_newsrc_parse(adata) < 0)
            break;

          for (size_t i = 0; i < adata->groups_num; i++)
          {
            struct NntpMboxData *mdata = adata->groups_list[i];
            if (mdata)
              mdata->deleted = true;
          }
          nntp_active_fetch(adata, true);
          nntp_newsrc_update(adata);
          nntp_newsrc_close(adata);

          destroy_state(&state);
          if (mailbox)
            examine_mailboxes(menu, &state);
          else
          {
            if (examine_directory(menu, &state, NULL, NULL) == -1)
              break;
          }
          init_menu(&state, menu, title, sizeof(title), mailbox);
        }
        break;
#endif /* USE_NNTP */

#if defined(USE_IMAP) || defined(USE_NNTP)
      case OP_BROWSER_SUBSCRIBE:
      case OP_BROWSER_UNSUBSCRIBE:
#endif
#ifdef USE_NNTP
      case OP_SUBSCRIBE_PATTERN:
      case OP_UNSUBSCRIBE_PATTERN:
        if (OptNews)
        {
          struct NntpAccountData *adata = CurrentNewsSrv;
          regex_t rx;
          memset(&rx, 0, sizeof(rx));
          char *s = buf->data;
          int i = menu->current;

          if ((op == OP_SUBSCRIBE_PATTERN) || (op == OP_UNSUBSCRIBE_PATTERN))
          {
            char tmp2[256];

            mutt_buffer_reset(buf);
            if (op == OP_SUBSCRIBE_PATTERN)
              snprintf(tmp2, sizeof(tmp2), _("Subscribe pattern: "));
            else
              snprintf(tmp2, sizeof(tmp2), _("Unsubscribe pattern: "));
            /* buf comes from the buffer pool, so defaults to size 1024 */
            if ((mutt_buffer_get_field(tmp2, buf, 0) != 0) || mutt_buffer_is_empty(buf))
            {
              break;
            }

            int err = REG_COMP(&rx, s, REG_NOSUB);
            if (err != 0)
            {
              regerror(err, &rx, buf->data, buf->dsize);
              regfree(&rx);
              mutt_error("%s", mutt_b2s(buf));
              break;
            }
            menu->redraw = REDRAW_FULL;
            i = 0;
          }
          else if (state.entrylen == 0)
          {
            mutt_error(_("No newsgroups match the mask"));
            break;
          }

          int rc = nntp_newsrc_parse(adata);
          if (rc < 0)
            break;

          for (; i < state.entrylen; i++)
          {
            struct FolderFile *ff = &state.entry[i];

            if ((op == OP_BROWSER_SUBSCRIBE) || (op == OP_BROWSER_UNSUBSCRIBE) ||
                (regexec(&rx, ff->name, 0, NULL, 0) == 0))
            {
              if ((op == OP_BROWSER_SUBSCRIBE) || (op == OP_SUBSCRIBE_PATTERN))
                mutt_newsgroup_subscribe(adata, ff->name);
              else
                mutt_newsgroup_unsubscribe(adata, ff->name);
            }
            if ((op == OP_BROWSER_SUBSCRIBE) || (op == OP_BROWSER_UNSUBSCRIBE))
            {
              if ((menu->current + 1) < menu->max)
                menu->current++;
              menu->redraw = REDRAW_MOTION_RESYNC;
              break;
            }
          }
          if (op == OP_SUBSCRIBE_PATTERN)
          {
            for (size_t j = 0; adata && (j < adata->groups_num); j++)
            {
              struct NntpMboxData *mdata = adata->groups_list[j];
              if (mdata && mdata->group && !mdata->subscribed)
              {
                if (regexec(&rx, mdata->group, 0, NULL, 0) == 0)
                {
                  mutt_newsgroup_subscribe(adata, mdata->group);
                  add_folder(menu, &state, mdata->group, NULL, NULL, NULL, mdata);
                }
              }
            }
            init_menu(&state, menu, title, sizeof(title), mailbox);
          }
          if (rc > 0)
            menu->redraw = REDRAW_FULL;
          nntp_newsrc_update(adata);
          nntp_clear_cache(adata);
          nntp_newsrc_close(adata);
          if ((op != OP_BROWSER_SUBSCRIBE) && (op != OP_BROWSER_UNSUBSCRIBE))
            regfree(&rx);
        }
#ifdef USE_IMAP
        else
#endif /* USE_IMAP && USE_NNTP */
#endif /* USE_NNTP */
#ifdef USE_IMAP
        {
          char tmp2[256];
          mutt_str_strfcpy(tmp2, state.entry[menu->current].name, sizeof(tmp2));
          mutt_expand_path(tmp2, sizeof(tmp2));
          imap_subscribe(tmp2, (op == OP_BROWSER_SUBSCRIBE));
        }
#endif /* USE_IMAP */
    }
  }

bail:
  mutt_buffer_pool_release(&OldLastDir);
  mutt_buffer_pool_release(&tmp);
  mutt_buffer_pool_release(&buf);
  mutt_buffer_pool_release(&prefix);

  if (menu)
  {
    mutt_menu_pop_current(menu);
    mutt_menu_free(&menu);
    dialog_pop();
    notify_observer_remove(NeoMutt->notify, mutt_dlg_browser_observer, dlg);
    mutt_window_free(&dlg);
  }

  goto_swapper[0] = '\0';
}

/**
 * mutt_select_file - Let the user select a file
 * @param[in]  file     Buffer for the result
 * @param[in]  filelen  Length of buffer
 * @param[in]  flags    Flags, see #SelectFileFlags
 * @param[out] files    Array of selected files
 * @param[out] numfiles Number of selected files
 */
void mutt_select_file(char *file, size_t filelen, SelectFileFlags flags,
                      char ***files, int *numfiles)
{
  struct Buffer *f_buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(f_buf, NONULL(file));
  mutt_buffer_select_file(f_buf, flags, files, numfiles);
  mutt_str_strfcpy(file, mutt_b2s(f_buf), filelen);

  mutt_buffer_pool_release(&f_buf);
}
