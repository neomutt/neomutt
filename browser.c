/**
 * @file
 * File/Mailbox Browser Dialog
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2020 R Primus <rprimus@gmail.com>
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
 * @page neo_browser File/Mailbox Browser Dialog
 *
 * ## Overview
 *
 * The File/Mailbox Browser Dialog lets the user select from a list of files or
 * mailboxes.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name           | Type           | See Also                  |
 * | :------------- | :------------- | :------------------------ |
 * | Browser Dialog | WT_DLG_BROWSER | mutt_buffer_select_file() |
 *
 * **Parent**
 * - @ref gui_dialog
 *
 * **Children**
 * - See: @ref gui_simple
 *
 * ## Data
 * - #Menu
 * - #Menu::mdata
 * - #BrowserState
 *
 * The @ref gui_simple holds a Menu.  The Browser Dialog stores its data
 * (#BrowserState) in Menu::mdata.
 *
 * ## Events
 *
 * Once constructed, it is controlled by the following events:
 *
 * | Event Type            | Handler                     |
 * | :-------------------- | :-------------------------- |
 * | #NT_CONFIG            | browser_config_observer() |
 * | #NT_WINDOW            | browser_window_observer() |
 *
 * The Browser Dialog doesn't have any specific colours, so it doesn't need to
 * support #NT_COLOR.
 *
 * The Browser Dialog does not implement MuttWindow::recalc() or MuttWindow::repaint().
 *
 * Some other events are handled by the @ref gui_simple.
 */

#include "config.h"
#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "browser.h"
#include "attach/lib.h"
#include "menu/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "format_flags.h"
#include "mutt_globals.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "opcodes.h"
#include "options.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/adata.h" // IWYU pragma: keep
#include "nntp/mdata.h" // IWYU pragma: keep
#endif

/// Help Bar for the File/Dir/Mailbox browser dialog
static const struct Mapping FolderHelp[] = {
  // clang-format off
  { N_("Exit"),  OP_EXIT },
  { N_("Chdir"), OP_CHANGE_DIRECTORY },
  { N_("Goto"),  OP_BROWSER_GOTO_FOLDER },
  { N_("Mask"),  OP_ENTER_MASK },
  { N_("Help"),  OP_HELP },
  { NULL, 0 },
  // clang-format on
};

#ifdef USE_NNTP
/// Help Bar for the NNTP Mailbox browser dialog
static const struct Mapping FolderNewsHelp[] = {
  // clang-format off
  { N_("Exit"),        OP_EXIT },
  { N_("List"),        OP_TOGGLE_MAILBOXES },
  { N_("Subscribe"),   OP_BROWSER_SUBSCRIBE },
  { N_("Unsubscribe"), OP_BROWSER_UNSUBSCRIBE },
  { N_("Catchup"),     OP_CATCHUP },
  { N_("Mask"),        OP_ENTER_MASK },
  { N_("Help"),        OP_HELP },
  { NULL, 0 },
  // clang-format on
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
  struct FolderFile *ff = NULL;
  ARRAY_FOREACH(ff, &state->entry)
  {
    FREE(&ff->name);
    FREE(&ff->desc);
  }
  ARRAY_FREE(&state->entry);

#ifdef USE_IMAP
  FREE(&state->folder);
#endif
}

/**
 * browser_compare_subject - Compare the subject of two browser entries - Implements ::sort_t
 */
static int browser_compare_subject(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  /* inbox should be sorted ahead of its siblings */
  int r = mutt_inbox_cmp(pa->name, pb->name);
  if (r == 0)
    r = mutt_str_coll(pa->name, pb->name);
  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_order - Compare the order of creation of two browser entries - Implements ::sort_t
 *
 * @note This only affects browsing mailboxes and is a no-op for folders.
 */
static int browser_compare_order(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return ((c_sort_browser & SORT_REVERSE) ? -1 : 1) * (pa->gen - pb->gen);
}

/**
 * browser_compare_desc - Compare the descriptions of two browser entries - Implements ::sort_t
 */
static int browser_compare_desc(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = mutt_str_coll(pa->desc, pb->desc);

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_date - Compare the date of two browser entries - Implements ::sort_t
 */
static int browser_compare_date(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = pa->mtime - pb->mtime;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_size - Compare the size of two browser entries - Implements ::sort_t
 */
static int browser_compare_size(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  int r = pa->size - pb->size;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_count - Compare the message count of two browser entries - Implements ::sort_t
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

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare_count_new - Compare the new count of two browser entries - Implements ::sort_t
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

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  return (c_sort_browser & SORT_REVERSE) ? -r : r;
}

/**
 * browser_compare - Sort the items in the browser - Implements ::sort_t
 *
 * Wild compare function that calls the others. It's useful because it provides
 * a way to tell "../" is always on the top of the list, independently of the
 * sort method.
 */
static int browser_compare(const void *a, const void *b)
{
  const struct FolderFile *pa = (const struct FolderFile *) a;
  const struct FolderFile *pb = (const struct FolderFile *) b;

  if ((mutt_str_coll(pa->desc, "../") == 0) || (mutt_str_coll(pa->desc, "..") == 0))
    return -1;
  if ((mutt_str_coll(pb->desc, "../") == 0) || (mutt_str_coll(pb->desc, "..") == 0))
    return 1;

  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  switch (c_sort_browser & SORT_MASK)
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
      return browser_compare_subject(a, b);
    default:
    case SORT_ORDER:
      return browser_compare_order(a, b);
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
  const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
  switch (c_sort_browser & SORT_MASK)
  {
#ifdef USE_NNTP
    case SORT_SIZE:
    case SORT_DATE:
      if (OptNews)
        return;
#endif
    default:
      break;
  }

  ARRAY_SORT(&state->entry, browser_compare);
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
  struct stat st = { 0 };
  bool retval = false;

  struct Buffer *fullpath = mutt_buffer_pool_get();
  mutt_buffer_concat_path(fullpath, folder, path);

  if (stat(mutt_buffer_string(fullpath), &st) == 0)
    retval = S_ISDIR(st.st_mode);

  mutt_buffer_pool_release(&fullpath);

  return retval;
}

/**
 * folder_format_str - Format a string for the folder browser - Implements ::format_t - @ingroup expando_api
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
                                     intptr_t data, MuttFormatFlags flags)
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

        const char *t_fmt = NULL;
        if (op == 'D')
        {
          const char *const c_date_format =
              cs_subset_string(NeoMutt->sub, "date_format");
          t_fmt = NONULL(c_date_format);
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
                 ((folder->ff->mode & S_ISUID) != 0) ? 's' :
                 ((folder->ff->mode & S_IXUSR) != 0) ? 'x' :
                                                       '-',
                 ((folder->ff->mode & S_IRGRP) != 0) ? 'r' : '-',
                 ((folder->ff->mode & S_IWGRP) != 0) ? 'w' : '-',
                 ((folder->ff->mode & S_ISGID) != 0) ? 's' :
                 ((folder->ff->mode & S_IXGRP) != 0) ? 'x' :
                                                       '-',
                 ((folder->ff->mode & S_IROTH) != 0) ? 'r' : '-',
                 ((folder->ff->mode & S_IWOTH) != 0) ? 'w' : '-',
                 ((folder->ff->mode & S_ISVTX) != 0) ? 't' :
                 ((folder->ff->mode & S_IXOTH) != 0) ? 'x' :
                                                       '-');
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
  {
    mutt_expando_format(buf, buflen, col, cols, if_str, folder_format_str, data,
                        MUTT_FORMAT_NO_FLAGS);
  }
  else if (flags & MUTT_FORMAT_OPTIONAL)
  {
    mutt_expando_format(buf, buflen, col, cols, else_str, folder_format_str,
                        data, MUTT_FORMAT_NO_FLAGS);
  }

  /* We return the format string, unchanged */
  return src;
}

/**
 * add_folder - Add a folder to the browser list
 * @param menu  Menu to use
 * @param state Browser state
 * @param name  Name of folder
 * @param desc  Description of folder
 * @param st    stat info for the folder
 * @param m     Mailbox
 * @param data  Data to associate with the folder
 */
static void add_folder(struct Menu *menu, struct BrowserState *state,
                       const char *name, const char *desc,
                       const struct stat *st, struct Mailbox *m, void *data)
{
  if ((!menu || state->is_mailbox_list) && m && (m->flags & MB_HIDDEN))
  {
    return;
  }

  struct FolderFile ff = { 0 };

  if (st)
  {
    ff.mode = st->st_mode;
    ff.mtime = st->st_mtime;
    ff.size = st->st_size;
    ff.gid = st->st_gid;
    ff.uid = st->st_uid;
    ff.nlink = st->st_nlink;
    ff.local = true;
  }
  else
    ff.local = false;

  if (m)
  {
    ff.has_mailbox = true;
    ff.gen = m->gen;
    ff.has_new_mail = m->has_new;
    ff.msg_count = m->msg_count;
    ff.msg_unread = m->msg_unread;
  }

  ff.name = mutt_str_dup(name);
  ff.desc = mutt_str_dup(desc ? desc : name);
#ifdef USE_IMAP
  ff.imap = false;
#endif
#ifdef USE_NNTP
  if (OptNews)
    ff.nd = data;
#endif

  ARRAY_ADD(&state->entry, ff);
}

/**
 * init_state - Initialise a browser state
 * @param state BrowserState to initialise
 * @param menu  Current menu
 */
static void init_state(struct BrowserState *state, struct Menu *menu)
{
  ARRAY_INIT(&state->entry);
  ARRAY_RESERVE(&state->entry, 256);
#ifdef USE_IMAP
  state->imap_browse = false;
#endif
  if (menu)
    menu->mdata = &state->entry;
}

/**
 * examine_directory - Get list of all files/newsgroups with mask
 * @param m      Mailbox
 * @param menu   Current Menu
 * @param state  State of browser
 * @param d      Directory
 * @param prefix Files/newsgroups must match this prefix
 * @retval  0 Success
 * @retval -1 Error
 */
static int examine_directory(struct Mailbox *m, struct Menu *menu,
                             struct BrowserState *state, const char *d, const char *prefix)
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
      if (prefix && *prefix && !mutt_str_startswith(mdata->group, prefix))
        continue;
      const struct Regex *c_mask = cs_subset_regex(NeoMutt->sub, "mask");
      if (!mutt_regex_match(c_mask, mdata->group))
      {
        continue;
      }
      add_folder(menu, state, mdata->group, NULL, NULL, NULL, mdata);
    }
  }
  else
#endif /* USE_NNTP */
  {
    struct stat st = { 0 };
    DIR *dp = NULL;
    struct dirent *de = NULL;

    while (stat(d, &st) == -1)
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

    if (!S_ISDIR(st.st_mode))
    {
      mutt_error(_("%s is not a directory"), d);
      goto ed_out;
    }

    if (m)
      mutt_mailbox_check(m, 0);

    dp = opendir(d);
    if (!dp)
    {
      mutt_perror(d);
      goto ed_out;
    }

    init_state(state, menu);

    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
    while ((de = readdir(dp)))
    {
      if (mutt_str_equal(de->d_name, "."))
        continue; /* we don't need . */

      if (prefix && *prefix && !mutt_str_startswith(de->d_name, prefix))
      {
        continue;
      }
      const struct Regex *c_mask = cs_subset_regex(NeoMutt->sub, "mask");
      if (!mutt_regex_match(c_mask, de->d_name))
      {
        continue;
      }

      mutt_buffer_concat_path(buf, d, de->d_name);
      if (lstat(mutt_buffer_string(buf), &st) == -1)
        continue;

      /* No size for directories or symlinks */
      if (S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
        st.st_size = 0;
      else if (!S_ISREG(st.st_mode))
        continue;

      struct MailboxNode *np = NULL;
      STAILQ_FOREACH(np, &ml, entries)
      {
        if (mutt_str_equal(mutt_buffer_string(buf), mailbox_path(np->mailbox)))
          break;
      }

      if (np && m && mutt_str_equal(np->mailbox->realpath, m->realpath))
      {
        np->mailbox->msg_count = m->msg_count;
        np->mailbox->msg_unread = m->msg_unread;
      }
      add_folder(menu, state, de->d_name, NULL, &st, np ? np->mailbox : NULL, NULL);
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
 * @param m     Mailbox
 * @param menu  Current menu
 * @param state State of browser
 * @retval  0 Success
 * @retval -1 Error
 */
static int examine_mailboxes(struct Mailbox *m, struct Menu *menu, struct BrowserState *state)
{
  struct stat st = { 0 };
  struct Buffer *md = NULL;
  struct Buffer *mailbox = NULL;

#ifdef USE_NNTP
  if (OptNews)
  {
    struct NntpAccountData *adata = CurrentNewsSrv;

    init_state(state, menu);

    for (unsigned int i = 0; i < adata->groups_num; i++)
    {
      const bool c_show_only_unread =
          cs_subset_bool(NeoMutt->sub, "show_only_unread");
      struct NntpMboxData *mdata = adata->groups_list[i];
      if (mdata && (mdata->has_new_mail ||
                    (mdata->subscribed && (mdata->unread || !c_show_only_unread))))
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

    mutt_mailbox_check(m, 0);

    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
    struct MailboxNode *np = NULL;
    STAILQ_FOREACH(np, &ml, entries)
    {
      if (!np->mailbox)
        continue;

      if (m && mutt_str_equal(np->mailbox->realpath, m->realpath))
      {
        np->mailbox->msg_count = m->msg_count;
        np->mailbox->msg_unread = m->msg_unread;
      }

      mutt_buffer_strcpy(mailbox, mailbox_path(np->mailbox));
      const bool c_browser_abbreviate_mailboxes =
          cs_subset_bool(NeoMutt->sub, "browser_abbreviate_mailboxes");
      if (c_browser_abbreviate_mailboxes)
        mutt_buffer_pretty_mailbox(mailbox);

      switch (np->mailbox->type)
      {
        case MUTT_IMAP:
        case MUTT_POP:
          add_folder(menu, state, mutt_buffer_string(mailbox),
                     np->mailbox->name, NULL, np->mailbox, NULL);
          continue;
        case MUTT_NOTMUCH:
        case MUTT_NNTP:
          add_folder(menu, state, mailbox_path(np->mailbox), np->mailbox->name,
                     NULL, np->mailbox, NULL);
          continue;
        default: /* Continue */
          break;
      }

      if (lstat(mailbox_path(np->mailbox), &st) == -1)
        continue;

      if ((!S_ISREG(st.st_mode)) && (!S_ISDIR(st.st_mode)) && (!S_ISLNK(st.st_mode)))
        continue;

      if (np->mailbox->type == MUTT_MAILDIR)
      {
        struct stat st2 = { 0 };

        mutt_buffer_printf(md, "%s/new", mailbox_path(np->mailbox));
        if (stat(mutt_buffer_string(md), &st) < 0)
          st.st_mtime = 0;
        mutt_buffer_printf(md, "%s/cur", mailbox_path(np->mailbox));
        if (stat(mutt_buffer_string(md), &st2) < 0)
          st2.st_mtime = 0;
        if (st2.st_mtime > st.st_mtime)
          st.st_mtime = st2.st_mtime;
      }

      add_folder(menu, state, mutt_buffer_string(mailbox), np->mailbox->name,
                 &st, np->mailbox, NULL);
    }
    neomutt_mailboxlist_clear(&ml);
  }
  browser_sort(state);

  mutt_buffer_pool_release(&mailbox);
  mutt_buffer_pool_release(&md);
  return 0;
}

/**
 * select_file_search - Menu search callback for matching files - Implements Menu::search() - @ingroup menu_search
 */
static int select_file_search(struct Menu *menu, regex_t *rx, int line)
{
  struct BrowserStateEntry *entry = menu->mdata;
#ifdef USE_NNTP
  if (OptNews)
    return regexec(rx, ARRAY_GET(entry, line)->desc, 0, NULL, 0);
#endif
  struct FolderFile *ff = ARRAY_GET(entry, line);
  char *search_on = ff->desc ? ff->desc : ff->name;

  return regexec(rx, search_on, 0, NULL, 0);
}

/**
 * folder_make_entry - Format a menu item for the folder browser - Implements Menu::make_entry() - @ingroup menu_make_entry
 */
static void folder_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct BrowserStateEntry *entry = menu->mdata;
  struct Folder folder = {
    .ff = ARRAY_GET(entry, line),
    .num = line,
  };

#ifdef USE_NNTP
  if (OptNews)
  {
    const char *const c_group_index_format =
        cs_subset_string(NeoMutt->sub, "group_index_format");
    mutt_expando_format(buf, buflen, 0, menu->win->state.cols,
                        NONULL(c_group_index_format), group_index_format_str,
                        (intptr_t) &folder, MUTT_FORMAT_ARROWCURSOR);
  }
  else
#endif
  {
    const char *const c_folder_format =
        cs_subset_string(NeoMutt->sub, "folder_format");
    mutt_expando_format(buf, buflen, 0, menu->win->state.cols, NONULL(c_folder_format),
                        folder_format_str, (intptr_t) &folder, MUTT_FORMAT_ARROWCURSOR);
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
  if (!ARRAY_EMPTY(&state->entry) &&
      (mutt_str_equal(ARRAY_FIRST(&state->entry)->desc, "..") ||
       mutt_str_equal(ARRAY_FIRST(&state->entry)->desc, "../")))
  {
    /* Skip the first entry, unless there's only one entry. */
    menu_set_index(menu, (menu->max > 1));
  }
  else
  {
    menu_set_index(menu, 0);
  }
}

/**
 * init_menu - Set up a new menu
 * @param state    Browser state
 * @param menu     Current menu
 * @param m        Mailbox
 * @param sbar     Status bar
 */
static void init_menu(struct BrowserState *state, struct Menu *menu,
                      struct Mailbox *m, struct MuttWindow *sbar)
{
  char title[256] = { 0 };
  menu->max = ARRAY_SIZE(&state->entry);

  int index = menu_get_index(menu);
  if (index >= menu->max)
    menu_set_index(menu, menu->max - 1);
  if (index < 0)
    menu_set_index(menu, 0);
  if (menu->top > index)
    menu->top = 0;

  menu->tagged = 0;

#ifdef USE_NNTP
  if (OptNews)
  {
    if (state->is_mailbox_list)
      snprintf(title, sizeof(title), _("Subscribed newsgroups"));
    else
    {
      snprintf(title, sizeof(title), _("Newsgroups on server [%s]"),
               CurrentNewsSrv->conn->account.host);
    }
  }
  else
#endif
  {
    if (state->is_mailbox_list)
    {
      snprintf(title, sizeof(title), _("Mailboxes [%d]"), mutt_mailbox_check(m, 0));
    }
    else
    {
      struct Buffer *path = mutt_buffer_pool_get();
      mutt_buffer_copy(path, &LastDir);
      mutt_buffer_pretty_mailbox(path);
      const struct Regex *c_mask = cs_subset_regex(NeoMutt->sub, "mask");
#ifdef USE_IMAP
      const bool c_imap_list_subscribed =
          cs_subset_bool(NeoMutt->sub, "imap_list_subscribed");
      if (state->imap_browse && c_imap_list_subscribed)
      {
        snprintf(title, sizeof(title), _("Subscribed [%s], File mask: %s"),
                 mutt_buffer_string(path), NONULL(c_mask ? c_mask->pattern : NULL));
      }
      else
#endif
      {
        snprintf(title, sizeof(title), _("Directory [%s], File mask: %s"),
                 mutt_buffer_string(path), NONULL(c_mask ? c_mask->pattern : NULL));
      }
      mutt_buffer_pool_release(&path);
    }
  }
  sbar_set_title(sbar, title);

  /* Browser tracking feature.
   * The goal is to highlight the good directory if LastDir is the parent dir
   * of LastDirBackup (this occurs mostly when one hit "../"). It should also work
   * properly when the user is in examine_mailboxes-mode.  */
  if (mutt_str_startswith(mutt_buffer_string(&LastDirBackup), mutt_buffer_string(&LastDir)))
  {
    char target_dir[PATH_MAX] = { 0 };

#ifdef USE_IMAP
    /* Check what kind of dir LastDirBackup is. */
    if (imap_path_probe(mutt_buffer_string(&LastDirBackup), NULL) == MUTT_IMAP)
    {
      mutt_str_copy(target_dir, mutt_buffer_string(&LastDirBackup), sizeof(target_dir));
      imap_clean_path(target_dir, sizeof(target_dir));
    }
    else
#endif
      mutt_str_copy(target_dir, strrchr(mutt_buffer_string(&LastDirBackup), '/') + 1,
                    sizeof(target_dir));

    /* If we get here, it means that LastDir is the parent directory of
     * LastDirBackup.  I.e., we're returning from a subdirectory, and we want
     * to position the cursor on the directory we're returning from. */
    bool matched = false;
    struct FolderFile *ff = NULL;
    ARRAY_FOREACH(ff, &state->entry)
    {
      if (mutt_str_equal(ff->name, target_dir))
      {
        menu_set_index(menu, ARRAY_FOREACH_IDX);
        matched = true;
        break;
      }
    }
    if (!matched)
      browser_highlight_default(state, menu);
  }
  else
    browser_highlight_default(state, menu);

  menu_queue_redraw(menu, MENU_REDRAW_FULL);
}

/**
 * file_tag - Tag an entry in the menu - Implements Menu::tag() - @ingroup menu_tag
 */
static int file_tag(struct Menu *menu, int sel, int act)
{
  struct BrowserStateEntry *entry = menu->mdata;
  struct FolderFile *ff = ARRAY_GET(entry, sel);
  if (S_ISDIR(ff->mode) ||
      (S_ISLNK(ff->mode) && link_is_dir(mutt_buffer_string(&LastDir), ff->name)))
  {
    mutt_error(_("Can't attach a directory"));
    return 0;
  }

  bool ot = ff->tagged;
  ff->tagged = ((act >= 0) ? act : !ff->tagged);

  return ff->tagged - ot;
}

/**
 * browser_config_observer - Notification that a Config Variable has changed - Implements ::observer_t
 */
static int browser_config_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_CONFIG) || !nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  if (!mutt_str_equal(ev_c->name, "browser_abbreviate_mailboxes") &&
      !mutt_str_equal(ev_c->name, "date_format") && !mutt_str_equal(ev_c->name, "folder") &&
      !mutt_str_equal(ev_c->name, "folder_format") &&
      !mutt_str_equal(ev_c->name, "group_index_format") &&
      !mutt_str_equal(ev_c->name, "sort_browser"))
  {
    return 0;
  }

  struct Menu *menu = nc->global_data;
  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * browser_window_observer - Notification that a Window has changed - Implements ::observer_t
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int browser_window_observer(struct NotifyCallback *nc)
{
  if ((nc->event_type != NT_WINDOW) || !nc->global_data || !nc->event_data)
    return -1;

  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct MuttWindow *win_menu = nc->global_data;
  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  struct Menu *menu = win_menu->wdata;

  notify_observer_remove(NeoMutt->notify, browser_config_observer, menu);
  notify_observer_remove(win_menu->notify, browser_window_observer, win_menu);

  mutt_debug(LL_DEBUG5, "window delete done\n");
  return 0;
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
  mutt_get_parent_path(mutt_buffer_string(&LastDirBackup), buf, sizeof(buf));
  mutt_buffer_strcpy(&LastDir, buf);
}

/**
 * mutt_buffer_select_file - Let the user select a file
 * @param[in]  file     Buffer for the result
 * @param[in]  flags    Flags, see #SelectFileFlags
 * @param[in]  m        Mailbox
 * @param[out] files    Array of selected files
 * @param[out] numfiles Number of selected files
 */
void mutt_buffer_select_file(struct Buffer *file, SelectFileFlags flags,
                             struct Mailbox *m, char ***files, int *numfiles)
{
  struct BrowserState state = { { 0 } };
  struct Menu *menu = NULL;
  struct MuttWindow *dlg = NULL;
  bool kill_prefix = false;
  bool multiple = (flags & MUTT_SEL_MULTI);
  bool folder = (flags & MUTT_SEL_FOLDER);
  state.is_mailbox_list = (flags & MUTT_SEL_MAILBOX) && folder;

  /* Keeps in memory the directory we were in when hitting '='
   * to go directly to $folder (`$folder`) */
  char goto_swapper[PATH_MAX] = { 0 };

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
      state.is_mailbox_list = false;
      for (size_t i = 0; i < adata->groups_num; i++)
      {
        struct NntpMboxData *mdata = adata->groups_list[i];
        if (mdata && mdata->subscribed)
        {
          state.is_mailbox_list = true;
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
    if (imap_path_probe(mutt_buffer_string(file), NULL) == MUTT_IMAP)
    {
      init_state(&state, NULL);
      state.imap_browse = true;
      if (imap_browse(mutt_buffer_string(file), &state) == 0)
      {
        mutt_buffer_strcpy(&LastDir, state.folder);
        browser_sort(&state);
      }
    }
    else
    {
#endif
      int i;
      for (i = mutt_buffer_len(file) - 1;
           (i > 0) && ((mutt_buffer_string(file))[i] != '/'); i--)
      {
        ; // do nothing
      }

      if (i > 0)
      {
        if ((mutt_buffer_string(file))[0] == '/')
          mutt_buffer_strcpy_n(&LastDir, mutt_buffer_string(file), i);
        else
        {
          mutt_path_getcwd(&LastDir);
          mutt_buffer_addch(&LastDir, '/');
          mutt_buffer_addstr_n(&LastDir, mutt_buffer_string(file), i);
        }
      }
      else
      {
        if ((mutt_buffer_string(file))[0] == '/')
          mutt_buffer_strcpy(&LastDir, "/");
        else
          mutt_path_getcwd(&LastDir);
      }

      if ((i <= 0) && (mutt_buffer_string(file)[0] != '/'))
        mutt_buffer_copy(prefix, file);
      else
        mutt_buffer_strcpy(prefix, mutt_buffer_string(file) + i + 1);
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

      const short c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
      switch (c_sort_browser & SORT_MASK)
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
          switch (mx_path_probe(CurrentFolder))
          {
            case MUTT_IMAP:
            case MUTT_MAILDIR:
            case MUTT_MBOX:
            case MUTT_MH:
            case MUTT_MMDF:
            {
              const char *const c_folder =
                  cs_subset_string(NeoMutt->sub, "folder");
              const char *const c_spool_file = cs_subset_string(NeoMutt->sub, "spool_file");
              if (c_folder)
                mutt_buffer_strcpy(&LastDir, c_folder);
              else if (c_spool_file)
                mutt_browser_select_dir(c_spool_file);
              break;
            }
            default:
              mutt_browser_select_dir(CurrentFolder);
              break;
          }
        }
        else if (!mutt_str_equal(CurrentFolder, mutt_buffer_string(&LastDirBackup)))
        {
          mutt_browser_select_dir(CurrentFolder);
        }
      }

      /* When browser tracking feature is disabled, clear LastDirBackup */
      if (!browser_track)
        mutt_buffer_reset(&LastDirBackup);
    }

#ifdef USE_IMAP
    if (!state.is_mailbox_list &&
        (imap_path_probe(mutt_buffer_string(&LastDir), NULL) == MUTT_IMAP))
    {
      init_state(&state, NULL);
      state.imap_browse = true;
      imap_browse(mutt_buffer_string(&LastDir), &state);
      browser_sort(&state);
    }
    else
#endif
    {
      size_t i = mutt_buffer_len(&LastDir);
      while ((i > 0) && (mutt_buffer_string(&LastDir)[--i] == '/'))
        LastDir.data[i] = '\0';
      mutt_buffer_fix_dptr(&LastDir);
      if (mutt_buffer_is_empty(&LastDir))
        mutt_path_getcwd(&LastDir);
    }
  }

  mutt_buffer_reset(file);

  const struct Mapping *help_data = NULL;
#ifdef USE_NNTP
  if (OptNews)
    help_data = FolderNewsHelp;
  else
#endif
    help_data = FolderHelp;

  dlg = simple_dialog_new(MENU_FOLDER, WT_DLG_BROWSER, help_data);

  menu = dlg->wdata;
  menu->make_entry = folder_make_entry;
  menu->search = select_file_search;
  if (multiple)
    menu->tag = file_tag;

  struct MuttWindow *sbar = window_find_child(dlg, WT_STATUS_BAR);

  struct MuttWindow *win_menu = menu->win;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->notify, NT_CONFIG, browser_config_observer, menu);
  notify_observer_add(win_menu->notify, NT_WINDOW, browser_window_observer, win_menu);

  if (state.is_mailbox_list)
  {
    examine_mailboxes(m, NULL, &state);
  }
  else
#ifdef USE_IMAP
      if (!state.imap_browse)
#endif
  {
    // examine_directory() calls add_folder() which needs the menu
    if (examine_directory(m, menu, &state, mutt_buffer_string(&LastDir),
                          mutt_buffer_string(prefix)) == -1)
    {
      goto bail;
    }
  }

  init_menu(&state, menu, m, sbar);
  // only now do we have a valid state to attach
  menu->mdata = &state.entry;

  int last_selected_mailbox = -1;

  while (true)
  {
    if (state.is_mailbox_list && (last_selected_mailbox >= 0) &&
        (last_selected_mailbox < menu->max))
    {
      menu_set_index(menu, last_selected_mailbox);
    }
    int op = menu_loop(menu);
    if (op >= 0)
      mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", OpStrings[op][0], op);
    int index = menu_get_index(menu);
    struct FolderFile *ff = ARRAY_GET(&state.entry, index);
    switch (op)
    {
      case OP_DESCEND_DIRECTORY:
      case OP_GENERIC_SELECT_ENTRY:
      {
        if (ARRAY_EMPTY(&state.entry))
        {
          mutt_error(_("No files match the file mask"));
          break;
        }

        if (S_ISDIR(ff->mode) ||
            (S_ISLNK(ff->mode) && link_is_dir(mutt_buffer_string(&LastDir), ff->name))
#ifdef USE_IMAP
            || ff->inferiors
#endif
        )
        {
          /* make sure this isn't a MH or maildir mailbox */
          if (state.is_mailbox_list)
          {
            mutt_buffer_strcpy(buf, ff->name);
            mutt_buffer_expand_path(buf);
          }
#ifdef USE_IMAP
          else if (state.imap_browse)
          {
            mutt_buffer_strcpy(buf, ff->name);
          }
#endif
          else
          {
            mutt_buffer_concat_path(buf, mutt_buffer_string(&LastDir), ff->name);
          }

          enum MailboxType type = mx_path_probe(mutt_buffer_string(buf));
          if ((op == OP_DESCEND_DIRECTORY) || (type == MUTT_MAILBOX_ERROR) ||
              (type == MUTT_UNKNOWN)
#ifdef USE_IMAP
              || ff->inferiors
#endif
          )
          {
            /* save the old directory */
            mutt_buffer_copy(OldLastDir, &LastDir);

            if (mutt_str_equal(ff->name, ".."))
            {
              size_t lastdirlen = mutt_buffer_len(&LastDir);
              if ((lastdirlen > 1) &&
                  mutt_str_equal("..", mutt_buffer_string(&LastDir) + lastdirlen - 2))
              {
                mutt_buffer_addstr(&LastDir, "/..");
              }
              else
              {
                char *p = NULL;
                if (lastdirlen > 1)
                  p = strrchr(mutt_buffer_string(&LastDir) + 1, '/');

                if (p)
                {
                  *p = '\0';
                  mutt_buffer_fix_dptr(&LastDir);
                }
                else
                {
                  if (mutt_buffer_string(&LastDir)[0] == '/')
                    mutt_buffer_strcpy(&LastDir, "/");
                  else
                    mutt_buffer_addstr(&LastDir, "/..");
                }
              }
            }
            else if (state.is_mailbox_list)
            {
              mutt_buffer_strcpy(&LastDir, ff->name);
              mutt_buffer_expand_path(&LastDir);
            }
#ifdef USE_IMAP
            else if (state.imap_browse)
            {
              mutt_buffer_strcpy(&LastDir, ff->name);
              /* tack on delimiter here */

              /* special case "" needs no delimiter */
              struct Url *url = url_parse(ff->name);
              if (url && url->path && (ff->delim != '\0'))
              {
                mutt_buffer_addch(&LastDir, ff->delim);
              }
              url_free(&url);
            }
#endif
            else
            {
              mutt_buffer_concat_path(tmp, mutt_buffer_string(&LastDir), ff->name);
              mutt_buffer_copy(&LastDir, tmp);
            }

            destroy_state(&state);
            if (kill_prefix)
            {
              mutt_buffer_reset(prefix);
              kill_prefix = false;
            }
            state.is_mailbox_list = false;
#ifdef USE_IMAP
            if (state.imap_browse)
            {
              init_state(&state, NULL);
              state.imap_browse = true;
              imap_browse(mutt_buffer_string(&LastDir), &state);
              browser_sort(&state);
              menu->mdata = &state.entry;
            }
            else
#endif
            {
              if (examine_directory(m, menu, &state, mutt_buffer_string(&LastDir),
                                    mutt_buffer_string(prefix)) == -1)
              {
                /* try to restore the old values */
                mutt_buffer_copy(&LastDir, OldLastDir);
                if (examine_directory(m, menu, &state, mutt_buffer_string(&LastDir),
                                      mutt_buffer_string(prefix)) == -1)
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
            init_menu(&state, menu, m, sbar);
            goto_swapper[0] = '\0';
            break;
          }
        }
        else if (op == OP_DESCEND_DIRECTORY)
        {
          mutt_error(_("%s is not a directory"), ARRAY_GET(&state.entry, index)->name);
          break;
        }

        if (state.is_mailbox_list || OptNews) /* USE_NNTP */
        {
          mutt_buffer_strcpy(file, ff->name);
          mutt_buffer_expand_path(file);
        }
#ifdef USE_IMAP
        else if (state.imap_browse)
          mutt_buffer_strcpy(file, ff->name);
#endif
        else
        {
          mutt_buffer_concat_path(file, mutt_buffer_string(&LastDir), ff->name);
        }
      }
        /* fallthrough */

      case OP_EXIT:

        if (multiple)
        {
          char **tfiles = NULL;

          if (menu->tagged)
          {
            *numfiles = menu->tagged;
            tfiles = mutt_mem_calloc(*numfiles, sizeof(char *));
            size_t j = 0;
            ARRAY_FOREACH(ff, &state.entry)
            {
              if (ff->tagged)
              {
                mutt_buffer_concat_path(tmp, mutt_buffer_string(&LastDir), ff->name);
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
        if (!ARRAY_EMPTY(&state.entry))
          mutt_message("%s", ARRAY_GET(&state.entry, index)->name);
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

        if (imap_mailbox_create(mutt_buffer_string(&LastDir)) == 0)
        {
          /* TODO: find a way to detect if the new folder would appear in
           *   this window, and insert it without starting over. */
          destroy_state(&state);
          init_state(&state, NULL);
          state.imap_browse = true;
          imap_browse(mutt_buffer_string(&LastDir), &state);
          browser_sort(&state);
          menu->mdata = &state.entry;
          browser_highlight_default(&state, menu);
          init_menu(&state, menu, m, sbar);
        }
        /* else leave error on screen */
        break;

      case OP_RENAME_MAILBOX:
        if (!ff->imap)
          mutt_error(_("Rename is only supported for IMAP mailboxes"));
        else
        {
          if (imap_mailbox_rename(ff->name) >= 0)
          {
            destroy_state(&state);
            init_state(&state, NULL);
            state.imap_browse = true;
            imap_browse(mutt_buffer_string(&LastDir), &state);
            browser_sort(&state);
            menu->mdata = &state.entry;
            browser_highlight_default(&state, menu);
            init_menu(&state, menu, m, sbar);
          }
        }
        break;

      case OP_DELETE_MAILBOX:
        if (!ff->imap)
          mutt_error(_("Delete is only supported for IMAP mailboxes"));
        else
        {
          char msg[128];

          // TODO(sileht): It could be better to select INBOX instead. But I
          // don't want to manipulate Context/Mailboxes/mailbox->account here for now.
          // Let's just protect neomutt against crash for now. #1417
          if (mutt_str_equal(mailbox_path(m), ff->name))
          {
            mutt_error(_("Can't delete currently selected mailbox"));
            break;
          }

          snprintf(msg, sizeof(msg), _("Really delete mailbox \"%s\"?"), ff->name);
          if (mutt_yesorno(msg, MUTT_NO) == MUTT_YES)
          {
            if (imap_delete_mailbox(m, ff->name) == 0)
            {
              /* free the mailbox from the browser */
              FREE(&ff->name);
              FREE(&ff->desc);
              /* and move all other entries up */
              ARRAY_REMOVE(&state.entry, ff);
              mutt_message(_("Mailbox deleted"));
              init_menu(&state, menu, m, sbar);
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
          if ((len > 0) && (mutt_buffer_string(&LastDir)[len - 1] != '/'))
            mutt_buffer_addch(buf, '/');
        }

        if (op == OP_CHANGE_DIRECTORY)
        {
          /* buf comes from the buffer pool, so defaults to size 1024 */
          int ret = mutt_buffer_get_field(_("Chdir to: "), buf, MUTT_FILE,
                                          false, NULL, NULL, NULL);
          if ((ret != 0) && mutt_buffer_is_empty(buf))
            break;
        }
        else if (op == OP_GOTO_PARENT)
          mutt_get_parent_path(mutt_buffer_string(buf), buf->data, buf->dsize);

        if (!mutt_buffer_is_empty(buf))
        {
          state.is_mailbox_list = false;
          mutt_buffer_expand_path(buf);
#ifdef USE_IMAP
          if (imap_path_probe(mutt_buffer_string(buf), NULL) == MUTT_IMAP)
          {
            mutt_buffer_copy(&LastDir, buf);
            destroy_state(&state);
            init_state(&state, NULL);
            state.imap_browse = true;
            imap_browse(mutt_buffer_string(&LastDir), &state);
            browser_sort(&state);
            menu->mdata = &state.entry;
            browser_highlight_default(&state, menu);
            init_menu(&state, menu, m, sbar);
          }
          else
#endif
          {
            if (mutt_buffer_string(buf)[0] != '/')
            {
              /* in case dir is relative, make it relative to LastDir,
               * not current working dir */
              mutt_buffer_concat_path(tmp, mutt_buffer_string(&LastDir),
                                      mutt_buffer_string(buf));
              mutt_buffer_copy(buf, tmp);
            }
            /* Resolve path from <chdir>
             * Avoids buildup such as /a/b/../../c
             * Symlinks are always unraveled to keep code simple */
            if (mutt_path_realpath(buf->data) == 0)
              break;

            struct stat st = { 0 };
            if (stat(mutt_buffer_string(buf), &st) == 0)
            {
              if (S_ISDIR(st.st_mode))
              {
                destroy_state(&state);
                if (examine_directory(m, menu, &state, mutt_buffer_string(buf),
                                      mutt_buffer_string(prefix)) == 0)
                {
                  mutt_buffer_copy(&LastDir, buf);
                }
                else
                {
                  mutt_error(_("Error scanning directory"));
                  if (examine_directory(m, menu, &state, mutt_buffer_string(&LastDir),
                                        mutt_buffer_string(prefix)) == -1)
                  {
                    goto bail;
                  }
                }
                browser_highlight_default(&state, menu);
                init_menu(&state, menu, m, sbar);
              }
              else
                mutt_error(_("%s is not a directory"), mutt_buffer_string(buf));
            }
            else
              mutt_perror(mutt_buffer_string(buf));
          }
        }
        break;

      case OP_ENTER_MASK:
      {
        const struct Regex *c_mask = cs_subset_regex(NeoMutt->sub, "mask");
        mutt_buffer_strcpy(buf, c_mask ? c_mask->pattern : NULL);
        if (mutt_get_field(_("File Mask: "), buf->data, buf->dsize,
                           MUTT_COMP_NO_FLAGS, false, NULL, NULL) != 0)
        {
          break;
        }

        mutt_buffer_fix_dptr(buf);

        state.is_mailbox_list = false;
        /* assume that the user wants to see everything */
        if (mutt_buffer_is_empty(buf))
          mutt_buffer_strcpy(buf, ".");

        struct Buffer errmsg = mutt_buffer_make(256);
        int rc = cs_subset_str_string_set(NeoMutt->sub, "mask",
                                          mutt_buffer_string(buf), &errmsg);
        if (CSR_RESULT(rc) != CSR_SUCCESS)
        {
          if (!mutt_buffer_is_empty(&errmsg))
          {
            mutt_error("%s", mutt_buffer_string(&errmsg));
            mutt_buffer_dealloc(&errmsg);
          }
          break;
        }
        mutt_buffer_dealloc(&errmsg);

        destroy_state(&state);
#ifdef USE_IMAP
        if (state.imap_browse)
        {
          init_state(&state, NULL);
          state.imap_browse = true;
          imap_browse(mutt_buffer_string(&LastDir), &state);
          browser_sort(&state);
          menu->mdata = &state.entry;
          init_menu(&state, menu, m, sbar);
        }
        else
#endif
            if (examine_directory(m, menu, &state, mutt_buffer_string(&LastDir), NULL) == 0)
        {
          init_menu(&state, menu, m, sbar);
        }
        else
        {
          mutt_error(_("Error scanning directory"));
          goto bail;
        }
        kill_prefix = false;
        if (ARRAY_EMPTY(&state.entry))
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
            break;
        }
        if (resort)
        {
          sort |= reverse ? SORT_REVERSE : 0;
          cs_subset_str_native_set(NeoMutt->sub, "sort_browser", sort, NULL);
          browser_sort(&state);
          browser_highlight_default(&state, menu);
          menu_queue_redraw(menu, MENU_REDRAW_FULL);
        }
        else
        {
          cs_subset_str_native_set(NeoMutt->sub, "sort_browser", sort, NULL);
        }
        break;
      }

      case OP_TOGGLE_MAILBOXES:
      case OP_BROWSER_GOTO_FOLDER:
      case OP_CHECK_NEW:
        if (state.is_mailbox_list)
        {
          last_selected_mailbox = menu->current;
        }

        if (op == OP_TOGGLE_MAILBOXES)
        {
          state.is_mailbox_list = !state.is_mailbox_list;
        }

        if (op == OP_BROWSER_GOTO_FOLDER)
        {
          /* When in mailboxes mode, disables this feature */
          const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
          if (c_folder)
          {
            mutt_debug(LL_DEBUG3, "= hit! Folder: %s, LastDir: %s\n", c_folder,
                       mutt_buffer_string(&LastDir));
            if (goto_swapper[0] == '\0')
            {
              if (!mutt_str_equal(mutt_buffer_string(&LastDir), c_folder))
              {
                /* Stores into goto_swapper LastDir, and swaps to `$folder` */
                mutt_str_copy(goto_swapper, mutt_buffer_string(&LastDir),
                              sizeof(goto_swapper));
                mutt_buffer_copy(&LastDirBackup, &LastDir);
                mutt_buffer_strcpy(&LastDir, c_folder);
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

        if (state.is_mailbox_list)
        {
          examine_mailboxes(m, menu, &state);
        }
#ifdef USE_IMAP
        else if (imap_path_probe(mutt_buffer_string(&LastDir), NULL) == MUTT_IMAP)
        {
          init_state(&state, NULL);
          state.imap_browse = true;
          imap_browse(mutt_buffer_string(&LastDir), &state);
          browser_sort(&state);
          menu->mdata = &state.entry;
        }
#endif
        else if (examine_directory(m, menu, &state, mutt_buffer_string(&LastDir),
                                   mutt_buffer_string(prefix)) == -1)
        {
          goto bail;
        }
        init_menu(&state, menu, m, sbar);
        break;

      case OP_MAILBOX_LIST:
        mutt_mailbox_list();
        break;

      case OP_BROWSER_NEW_FILE:
        mutt_buffer_printf(buf, "%s/", mutt_buffer_string(&LastDir));
        /* buf comes from the buffer pool, so defaults to size 1024 */
        if (mutt_buffer_get_field(_("New file name: "), buf, MUTT_FILE, false,
                                  NULL, NULL, NULL) == 0)
        {
          mutt_buffer_copy(file, buf);
          destroy_state(&state);
          goto bail;
        }
        break;

      case OP_BROWSER_VIEW_FILE:
        if (ARRAY_EMPTY(&state.entry))
        {
          mutt_error(_("No files match the file mask"));
          break;
        }

#ifdef USE_IMAP
        if (ff->selectable)
        {
          mutt_buffer_strcpy(file, ff->name);
          destroy_state(&state);
          goto bail;
        }
        else
#endif
            if (S_ISDIR(ff->mode) ||
                (S_ISLNK(ff->mode) && link_is_dir(mutt_buffer_string(&LastDir), ff->name)))
        {
          mutt_error(_("Can't view a directory"));
          break;
        }
        else
        {
          char buf2[PATH_MAX];

          mutt_path_concat(buf2, mutt_buffer_string(&LastDir), ff->name, sizeof(buf2));
          struct Body *b = mutt_make_file_attach(buf2, NeoMutt->sub);
          if (b)
          {
            mutt_view_attachment(NULL, b, MUTT_VA_REGULAR, NULL, NULL, menu->win);
            mutt_body_free(&b);
            menu_queue_redraw(menu, MENU_REDRAW_FULL);
          }
          else
            mutt_error(_("Error trying to view file"));
        }
        break;

#ifdef USE_NNTP
      case OP_CATCHUP:
      case OP_UNCATCHUP:
      {
        if (!OptNews)
          break;

        struct NntpMboxData *mdata = NULL;

        int rc = nntp_newsrc_parse(CurrentNewsSrv);
        if (rc < 0)
          break;

        if (op == OP_CATCHUP)
          mdata = mutt_newsgroup_catchup(m, CurrentNewsSrv, ff->name);
        else
          mdata = mutt_newsgroup_uncatchup(m, CurrentNewsSrv, ff->name);

        if (mdata)
        {
          nntp_newsrc_update(CurrentNewsSrv);
          index = menu_get_index(menu) + 1;
          if (index < menu->max)
            menu_set_index(menu, index);
        }
        if (rc)
          menu_queue_redraw(menu, MENU_REDRAW_INDEX);
        nntp_newsrc_close(CurrentNewsSrv);
        break;
      }

      case OP_LOAD_ACTIVE:
      {
        if (!OptNews)
          break;

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
        if (state.is_mailbox_list)
        {
          examine_mailboxes(m, menu, &state);
        }
        else
        {
          if (examine_directory(m, menu, &state, NULL, NULL) == -1)
            break;
        }
        init_menu(&state, menu, m, sbar);
        break;
      }
#endif /* USE_NNTP */

#if defined(USE_IMAP) || defined(USE_NNTP)
      case OP_BROWSER_SUBSCRIBE:
      case OP_BROWSER_UNSUBSCRIBE:
#endif
#ifdef USE_NNTP
      case OP_SUBSCRIBE_PATTERN:
      case OP_UNSUBSCRIBE_PATTERN:
      {
        if (OptNews)
        {
          struct NntpAccountData *adata = CurrentNewsSrv;
          regex_t rx;
          memset(&rx, 0, sizeof(rx));
          char *s = buf->data;
          index = menu_get_index(menu);

          if ((op == OP_SUBSCRIBE_PATTERN) || (op == OP_UNSUBSCRIBE_PATTERN))
          {
            char tmp2[256];

            mutt_buffer_reset(buf);
            if (op == OP_SUBSCRIBE_PATTERN)
              snprintf(tmp2, sizeof(tmp2), _("Subscribe pattern: "));
            else
              snprintf(tmp2, sizeof(tmp2), _("Unsubscribe pattern: "));
            /* buf comes from the buffer pool, so defaults to size 1024 */
            if ((mutt_buffer_get_field(tmp2, buf, MUTT_PATTERN, false, NULL, NULL, NULL) != 0) ||
                mutt_buffer_is_empty(buf))
            {
              break;
            }

            int err = REG_COMP(&rx, s, REG_NOSUB);
            if (err != 0)
            {
              regerror(err, &rx, buf->data, buf->dsize);
              regfree(&rx);
              mutt_error("%s", mutt_buffer_string(buf));
              break;
            }
            menu_queue_redraw(menu, MENU_REDRAW_FULL);
            index = 0;
          }
          else if (ARRAY_EMPTY(&state.entry))
          {
            mutt_error(_("No newsgroups match the mask"));
            break;
          }

          int rc = nntp_newsrc_parse(adata);
          if (rc < 0)
            break;

          ARRAY_FOREACH_FROM(ff, &state.entry, index)
          {
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
              if ((index + 1) < menu->max)
                menu_set_index(menu, index + 1);
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
            init_menu(&state, menu, m, sbar);
          }
          if (rc > 0)
            menu_queue_redraw(menu, MENU_REDRAW_FULL);
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
          mutt_str_copy(tmp2, ff->name, sizeof(tmp2));
          mutt_expand_path(tmp2, sizeof(tmp2));
          imap_subscribe(tmp2, (op == OP_BROWSER_SUBSCRIBE));
        }
#endif /* USE_IMAP */
      }
    }
  }

bail:
  mutt_buffer_pool_release(&OldLastDir);
  mutt_buffer_pool_release(&tmp);
  mutt_buffer_pool_release(&buf);
  mutt_buffer_pool_release(&prefix);

  simple_dialog_free(&dlg);

  goto_swapper[0] = '\0';
}

/**
 * mutt_select_file - Let the user select a file
 * @param[in]  file     Buffer for the result
 * @param[in]  filelen  Length of buffer
 * @param[in]  flags    Flags, see #SelectFileFlags
 * @param[in]  m        Mailbox
 * @param[out] files    Array of selected files
 * @param[out] numfiles Number of selected files
 */
void mutt_select_file(char *file, size_t filelen, SelectFileFlags flags,
                      struct Mailbox *m, char ***files, int *numfiles)
{
  struct Buffer *f_buf = mutt_buffer_pool_get();

  mutt_buffer_strcpy(f_buf, NONULL(file));
  mutt_buffer_select_file(f_buf, flags, m, files, numfiles);
  mutt_str_copy(file, mutt_buffer_string(f_buf), filelen);

  mutt_buffer_pool_release(&f_buf);
}
