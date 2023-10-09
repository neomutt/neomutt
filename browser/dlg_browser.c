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
 * @page browser_dlg_browser File/Mailbox Browser Dialog
 *
 * The File/Mailbox Browser Dialog lets the user select from a list of files or
 * mailboxes.
 *
 * This is a @ref gui_simple
 *
 * ## Windows
 *
 * | Name           | Type           | See Also      |
 * | :------------- | :------------- | :------------ |
 * | Browser Dialog | WT_DLG_BROWSER | dlg_browser() |
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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "core/lib.h"
#include "conn/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "key/lib.h"
#include "menu/lib.h"
#include "format_flags.h"
#include "functions.h"
#include "globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "private_data.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#include "nntp/adata.h"
#include "nntp/mdata.h"
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

/// Browser: previous selected directory
struct Buffer LastDir = { 0 };
/// Browser: backup copy of the current directory
struct Buffer LastDirBackup = { 0 };

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
    buf_alloc(&LastDir, PATH_MAX);
    buf_alloc(&LastDirBackup, PATH_MAX);
    done = true;
  }
}

/**
 * mutt_browser_cleanup - Clean up working Buffers
 */
void mutt_browser_cleanup(void)
{
  buf_dealloc(&LastDir);
  buf_dealloc(&LastDirBackup);
}

/**
 * link_is_dir - Does this symlink point to a directory?
 * @param folder Folder
 * @param path   Link name
 * @retval true  Links to a directory
 * @retval false Otherwise
 */
bool link_is_dir(const char *folder, const char *path)
{
  struct stat st = { 0 };
  bool rc = false;

  struct Buffer *fullpath = buf_pool_get();
  buf_concat_path(fullpath, folder, path);

  if (stat(buf_string(fullpath), &st) == 0)
    rc = S_ISDIR(st.st_mode);

  buf_pool_release(&fullpath);

  return rc;
}

/**
 * folder_format_str - Format a string for the folder browser - Implements ::format_t - @ingroup expando_api
 *
 * | Expando | Description
 * | :------ | :-------------------------------------------------------
 * | \%a     | Alert: 1 if user is notified of new mail
 * | \%C     | Current file number
 * | \%d     | Date/time folder was last modified
 * | \%D     | Date/time folder was last modified using `$date_format.`
 * | \%F     | File permissions
 * | \%f     | Filename (with suffix `/`, `@` or `*`)
 * | \%g     | Group name (or numeric gid, if missing)
 * | \%i     | Description of the folder
 * | \%l     | Number of hard links
 * | \%m     | Number of messages in the mailbox
 * | \%N     | "N" if mailbox has new mail, " " (space) otherwise
 * | \%n     | Number of unread messages in the mailbox
 * | \%p     | Poll: 1 if Mailbox is checked for new mail
 * | \%s     | Size in bytes
 * | \%t     | `*` if the file is tagged, blank otherwise
 * | \%u     | Owner name (or numeric uid, if missing)
 * | \%[fmt] | Date folder was last modified using strftime(3)
 */
static const char *folder_format_str(char *buf, size_t buflen, size_t col, int cols,
                                     char op, const char *src, const char *prec,
                                     const char *if_str, const char *else_str,
                                     intptr_t data, MuttFormatFlags flags)
{
  char fn[128] = { 0 };
  char fmt[128] = { 0 };
  struct Folder *folder = (struct Folder *) data;
  bool optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'a':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, folder->ff->notify_user);
      }
      else
      {
        if (folder->ff->notify_user == 0)
          optional = false;
      }
      break;

    case 'C':
      snprintf(fmt, sizeof(fmt), "%%%sd", prec);
      snprintf(buf, buflen, fmt, folder->num + 1);
      break;

    case 'd':
    case 'D':
      if (folder->ff->local)
      {
        bool use_c_locale = false;

        const char *t_fmt = NULL;
        if (op == 'D')
        {
          const char *const c_date_format = cs_subset_string(NeoMutt->sub, "date_format");
          t_fmt = NONULL(c_date_format);
          if (*t_fmt == '!')
          {
            t_fmt++;
            use_c_locale = true;
          }
        }
        else
        {
          static const time_t one_year = 31536000;
          t_fmt = ((mutt_date_now() - folder->ff->mtime) < one_year) ? "%b %d %H:%M" : "%b %d  %Y";
        }

        char date[128] = { 0 };
        if (use_c_locale)
        {
          mutt_date_localtime_format_locale(date, sizeof(date), t_fmt,
                                            folder->ff->mtime, NeoMutt->time_c_locale);
        }
        else
        {
          mutt_date_localtime_format(date, sizeof(date), t_fmt, folder->ff->mtime);
        }

        mutt_format_s(buf, buflen, prec, date);
      }
      else
      {
        mutt_format_s(buf, buflen, prec, "");
      }
      break;

    case '[':
      if (folder->ff->local)
      {
        char buf2[128] = { 0 };
        bool use_c_locale = false;
        struct tm tm = { 0 };

        char *p = buf;

        const char *cp = src;
        if (*cp == '!')
        {
          use_c_locale = true;
          cp++;
        }

        size_t len = buflen - 1;
        while ((len > 0) && (*cp != ']'))
        {
          if (*cp == '%')
          {
            cp++;
            if (len >= 2)
            {
              *p++ = '%';
              *p++ = *cp;
              len -= 2;
            }
            else
            {
              break; /* not enough space */
            }
            cp++;
          }
          else
          {
            *p++ = *cp++;
            len--;
          }
        }
        *p = '\0';

        tm = mutt_date_localtime(folder->ff->mtime);

        if (use_c_locale)
          strftime_l(buf2, sizeof(buf2), buf, &tm, NeoMutt->time_c_locale);
        else
          strftime(buf2, sizeof(buf2), buf, &tm);

        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, buf2);
        if (len > 0)
          src = cp + 1;
      }
      else
      {
        mutt_format_s(buf, buflen, prec, "");
      }
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
        char permission[11] = { 0 };
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
        char permission[11] = { 0 };
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
        {
          mutt_format_s(buf, buflen, prec, gr->gr_name);
        }
        else
        {
          snprintf(fmt, sizeof(fmt), "%%%sld", prec);
          snprintf(buf, buflen, fmt, folder->ff->gid);
        }
      }
      else
      {
        mutt_format_s(buf, buflen, prec, "");
      }
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
      {
        mutt_format_s(buf, buflen, prec, "");
      }
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
        {
          mutt_format_s(buf, buflen, prec, "");
        }
      }
      else if (folder->ff->msg_count == 0)
      {
        optional = false;
      }
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
        {
          mutt_format_s(buf, buflen, prec, "");
        }
      }
      else if (folder->ff->msg_unread == 0)
      {
        optional = false;
      }
      break;

    case 'p':
      if (!optional)
      {
        snprintf(fmt, sizeof(fmt), "%%%sd", prec);
        snprintf(buf, buflen, fmt, folder->ff->poll_new_mail);
      }
      else
      {
        if (folder->ff->poll_new_mail == 0)
          optional = false;
      }
      break;

    case 's':
      if (folder->ff->local)
      {
        mutt_str_pretty_size(fn, sizeof(fn), folder->ff->size);
        snprintf(fmt, sizeof(fmt), "%%%ss", prec);
        snprintf(buf, buflen, fmt, fn);
      }
      else
      {
        mutt_format_s(buf, buflen, prec, "");
      }
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
        {
          mutt_format_s(buf, buflen, prec, pw->pw_name);
        }
        else
        {
          snprintf(fmt, sizeof(fmt), "%%%sld", prec);
          snprintf(buf, buflen, fmt, folder->ff->uid);
        }
      }
      else
      {
        mutt_format_s(buf, buflen, prec, "");
      }
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
 * browser_add_folder - Add a folder to the browser list
 * @param menu  Menu to use
 * @param state Browser state
 * @param name  Name of folder
 * @param desc  Description of folder
 * @param st    stat info for the folder
 * @param m     Mailbox
 * @param data  Data to associate with the folder
 */
void browser_add_folder(const struct Menu *menu, struct BrowserState *state,
                        const char *name, const char *desc,
                        const struct stat *st, struct Mailbox *m, void *data)
{
  if ((!menu || state->is_mailbox_list) && m && !m->visible)
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
  {
    ff.local = false;
  }

  if (m)
  {
    ff.has_mailbox = true;
    ff.gen = m->gen;
    ff.has_new_mail = m->has_new;
    ff.msg_count = m->msg_count;
    ff.msg_unread = m->msg_unread;
    ff.notify_user = m->notify_user;
    ff.poll_new_mail = m->poll_new_mail;
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
void init_state(struct BrowserState *state, struct Menu *menu)
{
  ARRAY_INIT(&state->entry);
  ARRAY_RESERVE(&state->entry, 256);
#ifdef USE_IMAP
  state->imap_browse = false;
#endif
  if (menu)
  {
    menu->mdata = &state->entry;
    menu->mdata_free = NULL; // Menu doesn't own the data
  }
}

/**
 * examine_directory - Get list of all files/newsgroups with mask
 * @param m       Mailbox
 * @param menu    Current Menu
 * @param state   State of browser
 * @param dirname Directory
 * @param prefix  Files/newsgroups must match this prefix
 * @retval  0 Success
 * @retval -1 Error
 */
int examine_directory(struct Mailbox *m, struct Menu *menu, struct BrowserState *state,
                      const char *dirname, const char *prefix)
{
  int rc = -1;
  struct Buffer *buf = buf_pool_get();
#ifdef USE_NNTP
  if (OptNews)
  {
    struct NntpAccountData *adata = CurrentNewsSrv;

    init_state(state, menu);

    const struct Regex *c_mask = cs_subset_regex(NeoMutt->sub, "mask");
    for (unsigned int i = 0; i < adata->groups_num; i++)
    {
      struct NntpMboxData *mdata = adata->groups_list[i];
      if (!mdata)
        continue;
      if (prefix && *prefix && !mutt_str_startswith(mdata->group, prefix))
        continue;
      if (!mutt_regex_match(c_mask, mdata->group))
      {
        continue;
      }
      browser_add_folder(menu, state, mdata->group, NULL, NULL, NULL, mdata);
    }
  }
  else
#endif /* USE_NNTP */
  {
    struct stat st = { 0 };
    DIR *dir = NULL;
    struct dirent *de = NULL;

    while (stat(dirname, &st) == -1)
    {
      if (errno == ENOENT)
      {
        /* The last used directory is deleted, try to use the parent dir. */
        char *c = strrchr(dirname, '/');

        if (c && (c > dirname))
        {
          *c = '\0';
          continue;
        }
      }
      mutt_perror("%s", dirname);
      goto ed_out;
    }

    if (!S_ISDIR(st.st_mode))
    {
      mutt_error(_("%s is not a directory"), dirname);
      goto ed_out;
    }

    if (m)
      mutt_mailbox_check(m, MUTT_MAILBOX_CHECK_NO_FLAGS);

    dir = mutt_file_opendir(dirname, MUTT_OPENDIR_NONE);
    if (!dir)
    {
      mutt_perror("%s", dirname);
      goto ed_out;
    }

    init_state(state, menu);

    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);

    const struct Regex *c_mask = cs_subset_regex(NeoMutt->sub, "mask");
    while ((de = readdir(dir)))
    {
      if (mutt_str_equal(de->d_name, "."))
        continue; /* we don't need . */

      if (prefix && *prefix && !mutt_str_startswith(de->d_name, prefix))
      {
        continue;
      }
      if (!mutt_regex_match(c_mask, de->d_name))
      {
        continue;
      }

      buf_concat_path(buf, dirname, de->d_name);
      if (lstat(buf_string(buf), &st) == -1)
        continue;

      /* No size for directories or symlinks */
      if (S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
        st.st_size = 0;
      else if (!S_ISREG(st.st_mode))
        continue;

      struct MailboxNode *np = NULL;
      STAILQ_FOREACH(np, &ml, entries)
      {
        if (mutt_str_equal(buf_string(buf), mailbox_path(np->mailbox)))
          break;
      }

      if (np && m && m->poll_new_mail && mutt_str_equal(np->mailbox->realpath, m->realpath))
      {
        np->mailbox->msg_count = m->msg_count;
        np->mailbox->msg_unread = m->msg_unread;
      }
      browser_add_folder(menu, state, de->d_name, NULL, &st, np ? np->mailbox : NULL, NULL);
    }
    neomutt_mailboxlist_clear(&ml);
    closedir(dir);
  }
  browser_sort(state);
  rc = 0;
ed_out:
  buf_pool_release(&buf);
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
int examine_mailboxes(struct Mailbox *m, struct Menu *menu, struct BrowserState *state)
{
  struct stat st = { 0 };
  struct Buffer *md = NULL;
  struct Buffer *mailbox = NULL;

#ifdef USE_NNTP
  if (OptNews)
  {
    struct NntpAccountData *adata = CurrentNewsSrv;

    init_state(state, menu);

    const bool c_show_only_unread = cs_subset_bool(NeoMutt->sub, "show_only_unread");
    for (unsigned int i = 0; i < adata->groups_num; i++)
    {
      struct NntpMboxData *mdata = adata->groups_list[i];
      if (mdata && (mdata->has_new_mail ||
                    (mdata->subscribed && (mdata->unread || !c_show_only_unread))))
      {
        browser_add_folder(menu, state, mdata->group, NULL, NULL, NULL, mdata);
      }
    }
  }
  else
#endif
  {
    init_state(state, menu);

    if (TAILQ_EMPTY(&NeoMutt->accounts))
      return -1;
    mailbox = buf_pool_get();
    md = buf_pool_get();

    mutt_mailbox_check(m, MUTT_MAILBOX_CHECK_NO_FLAGS);

    struct MailboxList ml = STAILQ_HEAD_INITIALIZER(ml);
    neomutt_mailboxlist_get_all(&ml, NeoMutt, MUTT_MAILBOX_ANY);
    struct MailboxNode *np = NULL;
    const bool c_browser_abbreviate_mailboxes = cs_subset_bool(NeoMutt->sub, "browser_abbreviate_mailboxes");

    STAILQ_FOREACH(np, &ml, entries)
    {
      if (!np->mailbox)
        continue;

      if (m && m->poll_new_mail && mutt_str_equal(np->mailbox->realpath, m->realpath))
      {
        np->mailbox->msg_count = m->msg_count;
        np->mailbox->msg_unread = m->msg_unread;
      }

      buf_strcpy(mailbox, mailbox_path(np->mailbox));
      if (c_browser_abbreviate_mailboxes)
        buf_pretty_mailbox(mailbox);

      switch (np->mailbox->type)
      {
        case MUTT_IMAP:
        case MUTT_POP:
          browser_add_folder(menu, state, buf_string(mailbox),
                             np->mailbox->name, NULL, np->mailbox, NULL);
          continue;
        case MUTT_NOTMUCH:
        case MUTT_NNTP:
          browser_add_folder(menu, state, mailbox_path(np->mailbox),
                             np->mailbox->name, NULL, np->mailbox, NULL);
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

        buf_printf(md, "%s/new", mailbox_path(np->mailbox));
        if (stat(buf_string(md), &st) < 0)
          st.st_mtime = 0;
        buf_printf(md, "%s/cur", mailbox_path(np->mailbox));
        if (stat(buf_string(md), &st2) < 0)
          st2.st_mtime = 0;
        if (st2.st_mtime > st.st_mtime)
          st.st_mtime = st2.st_mtime;
      }

      browser_add_folder(menu, state, buf_string(mailbox), np->mailbox->name,
                         &st, np->mailbox, NULL);
    }
    neomutt_mailboxlist_clear(&ml);
  }
  browser_sort(state);

  buf_pool_release(&mailbox);
  buf_pool_release(&md);
  return 0;
}

/**
 * select_file_search - Menu search callback for matching files - Implements Menu::search() - @ingroup menu_search
 */
static int select_file_search(struct Menu *menu, regex_t *rx, int line)
{
  struct BrowserEntryArray *entry = menu->mdata;
#ifdef USE_NNTP
  if (OptNews)
    return regexec(rx, ARRAY_GET(entry, line)->desc, 0, NULL, 0);
#endif
  struct FolderFile *ff = ARRAY_GET(entry, line);
  char *search_on = ff->desc ? ff->desc : ff->name;

  return regexec(rx, search_on, 0, NULL, 0);
}

/**
 * folder_make_entry - Format a Folder for the Menu - Implements Menu::make_entry() - @ingroup menu_make_entry
 *
 * @sa $folder_format, $group_index_format, $mailbox_folder_format, folder_format_str()
 */
static void folder_make_entry(struct Menu *menu, char *buf, size_t buflen, int line)
{
  struct BrowserState *bstate = menu->mdata;
  struct BrowserEntryArray *entry = &bstate->entry;
  struct Folder folder = {
    .ff = ARRAY_GET(entry, line),
    .num = line,
  };

#ifdef USE_NNTP
  if (OptNews)
  {
    const char *const c_group_index_format = cs_subset_string(NeoMutt->sub, "group_index_format");
    mutt_expando_format(buf, buflen, 0, menu->win->state.cols,
                        NONULL(c_group_index_format), group_index_format_str,
                        (intptr_t) &folder, MUTT_FORMAT_ARROWCURSOR);
  }
  else
#endif
      if (bstate->is_mailbox_list)
  {
    const char *const c_mailbox_folder_format = cs_subset_string(NeoMutt->sub, "mailbox_folder_format");
    mutt_expando_format(buf, buflen, 0, menu->win->state.cols,
                        NONULL(c_mailbox_folder_format), folder_format_str,
                        (intptr_t) &folder, MUTT_FORMAT_ARROWCURSOR);
  }
  else
  {
    const char *const c_folder_format = cs_subset_string(NeoMutt->sub, "folder_format");
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
void browser_highlight_default(struct BrowserState *state, struct Menu *menu)
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
void init_menu(struct BrowserState *state, struct Menu *menu, struct Mailbox *m,
               struct MuttWindow *sbar)
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

  menu->num_tagged = 0;

#ifdef USE_NNTP
  if (OptNews)
  {
    if (state->is_mailbox_list)
    {
      snprintf(title, sizeof(title), _("Subscribed newsgroups"));
    }
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
      snprintf(title, sizeof(title), _("Mailboxes [%d]"),
               mutt_mailbox_check(m, MUTT_MAILBOX_CHECK_NO_FLAGS));
    }
    else
    {
      struct Buffer *path = buf_pool_get();
      buf_copy(path, &LastDir);
      buf_pretty_mailbox(path);
      const struct Regex *c_mask = cs_subset_regex(NeoMutt->sub, "mask");
#ifdef USE_IMAP
      const bool c_imap_list_subscribed = cs_subset_bool(NeoMutt->sub, "imap_list_subscribed");
      if (state->imap_browse && c_imap_list_subscribed)
      {
        snprintf(title, sizeof(title), _("Subscribed [%s], File mask: %s"),
                 buf_string(path), NONULL(c_mask ? c_mask->pattern : NULL));
      }
      else
#endif
      {
        snprintf(title, sizeof(title), _("Directory [%s], File mask: %s"),
                 buf_string(path), NONULL(c_mask ? c_mask->pattern : NULL));
      }
      buf_pool_release(&path);
    }
  }
  sbar_set_title(sbar, title);

  /* Browser tracking feature.
   * The goal is to highlight the good directory if LastDir is the parent dir
   * of LastDirBackup (this occurs mostly when one hit "../"). It should also work
   * properly when the user is in examine_mailboxes-mode.  */
  if (mutt_str_startswith(buf_string(&LastDirBackup), buf_string(&LastDir)))
  {
    char target_dir[PATH_MAX] = { 0 };

#ifdef USE_IMAP
    /* Check what kind of dir LastDirBackup is. */
    if (imap_path_probe(buf_string(&LastDirBackup), NULL) == MUTT_IMAP)
    {
      mutt_str_copy(target_dir, buf_string(&LastDirBackup), sizeof(target_dir));
      imap_clean_path(target_dir, sizeof(target_dir));
    }
    else
#endif
      mutt_str_copy(target_dir, strrchr(buf_string(&LastDirBackup), '/') + 1,
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
  {
    browser_highlight_default(state, menu);
  }

  menu_queue_redraw(menu, MENU_REDRAW_FULL);
}

/**
 * file_tag - Tag an entry in the menu - Implements Menu::tag() - @ingroup menu_tag
 */
static int file_tag(struct Menu *menu, int sel, int act)
{
  struct BrowserEntryArray *entry = menu->mdata;
  struct FolderFile *ff = ARRAY_GET(entry, sel);
  if (S_ISDIR(ff->mode) ||
      (S_ISLNK(ff->mode) && link_is_dir(buf_string(&LastDir), ff->name)))
  {
    mutt_error(_("Can't attach a directory"));
    return 0;
  }

  bool ot = ff->tagged;
  ff->tagged = ((act >= 0) ? act : !ff->tagged);

  return ff->tagged - ot;
}

/**
 * browser_config_observer - Notification that a Config Variable has changed - Implements ::observer_t - @ingroup observer_api
 */
static int browser_config_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_CONFIG)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct EventConfig *ev_c = nc->event_data;

  struct BrowserPrivateData *priv = nc->global_data;
  struct Menu *menu = priv->menu;

  if (mutt_str_equal(ev_c->name, "browser_sort_dirs_first"))
  {
    struct BrowserState *state = menu->mdata;
    browser_sort(state);
    browser_highlight_default(state, menu);
  }
  else if (!mutt_str_equal(ev_c->name, "browser_abbreviate_mailboxes") &&
           !mutt_str_equal(ev_c->name, "date_format") &&
           !mutt_str_equal(ev_c->name, "folder") &&
           !mutt_str_equal(ev_c->name, "folder_format") &&
           !mutt_str_equal(ev_c->name, "group_index_format") &&
           !mutt_str_equal(ev_c->name, "mailbox_folder_format") &&
           !mutt_str_equal(ev_c->name, "sort_browser"))
  {
    return 0;
  }

  menu_queue_redraw(menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "config done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * browser_mailbox_observer - Notification that a Mailbox has changed - Implements ::observer_t - @ingroup observer_api
 *
 * Find the matching Mailbox and update its details.
 */
static int browser_mailbox_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_MAILBOX)
    return 0;
  if (nc->event_subtype == NT_MAILBOX_DELETE)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;

  struct BrowserPrivateData *priv = nc->global_data;

  struct BrowserState *state = &priv->state;
  if (state->is_mailbox_list)
  {
    struct EventMailbox *ev_m = nc->event_data;
    struct Mailbox *m = ev_m->mailbox;
    struct FolderFile *ff = NULL;
    ARRAY_FOREACH(ff, &state->entry)
    {
      if (ff->gen != m->gen)
        continue;

      ff->has_new_mail = m->has_new;
      ff->msg_count = m->msg_count;
      ff->msg_unread = m->msg_unread;
      ff->notify_user = m->notify_user;
      ff->poll_new_mail = m->poll_new_mail;
      mutt_str_replace(&ff->desc, m->name);
      break;
    }
  }

  menu_queue_redraw(priv->menu, MENU_REDRAW_FULL);
  mutt_debug(LL_DEBUG5, "mailbox done, request WA_RECALC, MENU_REDRAW_FULL\n");

  return 0;
}

/**
 * browser_window_observer - Notification that a Window has changed - Implements ::observer_t - @ingroup observer_api
 *
 * This function is triggered by changes to the windows.
 *
 * - Delete (this window): clean up the resources held by the Help Bar
 */
static int browser_window_observer(struct NotifyCallback *nc)
{
  if (nc->event_type != NT_WINDOW)
    return 0;
  if (!nc->global_data || !nc->event_data)
    return -1;
  if (nc->event_subtype != NT_WINDOW_DELETE)
    return 0;

  struct BrowserPrivateData *priv = nc->global_data;
  struct MuttWindow *win_menu = priv->menu->win;

  struct EventWindow *ev_w = nc->event_data;
  if (ev_w->win != win_menu)
    return 0;

  notify_observer_remove(NeoMutt->sub->notify, browser_config_observer, priv);
  notify_observer_remove(win_menu->notify, browser_window_observer, priv);
  notify_observer_remove(NeoMutt->notify, browser_mailbox_observer, priv);

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

  buf_strcpy(&LastDirBackup, f);

  /* Method that will fetch the parent path depending on the type of the path. */
  char buf[PATH_MAX] = { 0 };
  mutt_get_parent_path(buf_string(&LastDirBackup), buf, sizeof(buf));
  buf_strcpy(&LastDir, buf);
}

/**
 * dlg_browser - Let the user select a file - @ingroup gui_dlg
 * @param[in]  file     Buffer for the result
 * @param[in]  flags    Flags, see #SelectFileFlags
 * @param[in]  m        Mailbox
 * @param[out] files    Array of selected files
 * @param[out] numfiles Number of selected files
 *
 * The Select File Dialog is a file browser.
 * It allows the user to select a file or directory to use.
 */
void dlg_browser(struct Buffer *file, SelectFileFlags flags, struct Mailbox *m,
                 char ***files, int *numfiles)
{
  struct BrowserPrivateData *priv = browser_private_data_new();
  priv->file = file;
  priv->mailbox = m;
  priv->files = files;
  priv->numfiles = numfiles;
  struct MuttWindow *dlg = NULL;

  priv->multiple = (flags & MUTT_SEL_MULTI);
  priv->folder = (flags & MUTT_SEL_FOLDER);
  priv->state.is_mailbox_list = (flags & MUTT_SEL_MAILBOX) && priv->folder;
  priv->last_selected_mailbox = -1;

  init_lastdir();

#ifdef USE_NNTP
  if (OptNews)
  {
    if (buf_is_empty(file))
    {
      struct NntpAccountData *adata = CurrentNewsSrv;

      /* default state for news reader mode is browse subscribed newsgroups */
      priv->state.is_mailbox_list = false;
      for (size_t i = 0; i < adata->groups_num; i++)
      {
        struct NntpMboxData *mdata = adata->groups_list[i];
        if (mdata && mdata->subscribed)
        {
          priv->state.is_mailbox_list = true;
          break;
        }
      }
    }
    else
    {
      buf_copy(priv->prefix, file);
    }
  }
  else
#endif
      if (!buf_is_empty(file))
  {
    buf_expand_path(file);
#ifdef USE_IMAP
    if (imap_path_probe(buf_string(file), NULL) == MUTT_IMAP)
    {
      init_state(&priv->state, NULL);
      priv->state.imap_browse = true;
      if (imap_browse(buf_string(file), &priv->state) == 0)
      {
        buf_strcpy(&LastDir, priv->state.folder);
        browser_sort(&priv->state);
      }
    }
    else
    {
#endif
      int i;
      for (i = buf_len(file) - 1; (i > 0) && ((buf_string(file))[i] != '/'); i--)
      {
        ; // do nothing
      }

      if (i > 0)
      {
        if ((buf_string(file))[0] == '/')
        {
          buf_strcpy_n(&LastDir, buf_string(file), i);
        }
        else
        {
          mutt_path_getcwd(&LastDir);
          buf_addch(&LastDir, '/');
          buf_addstr_n(&LastDir, buf_string(file), i);
        }
      }
      else
      {
        if ((buf_string(file))[0] == '/')
          buf_strcpy(&LastDir, "/");
        else
          mutt_path_getcwd(&LastDir);
      }

      if ((i <= 0) && (buf_string(file)[0] != '/'))
        buf_copy(priv->prefix, file);
      else
        buf_strcpy(priv->prefix, buf_string(file) + i + 1);
      priv->kill_prefix = true;
#ifdef USE_IMAP
    }
#endif
  }
  else
  {
    if (priv->folder)
    {
      /* Whether we use the tracking feature of the browser depends
       * on which sort method we chose to use. This variable is defined
       * only to help readability of the code.  */
      bool browser_track = false;

      const enum SortType c_sort_browser = cs_subset_sort(NeoMutt->sub, "sort_browser");
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
        if (buf_is_empty(&LastDir))
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
              const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
              const char *const c_spool_file = cs_subset_string(NeoMutt->sub, "spool_file");
              if (c_folder)
                buf_strcpy(&LastDir, c_folder);
              else if (c_spool_file)
                mutt_browser_select_dir(c_spool_file);
              break;
            }
            default:
              mutt_browser_select_dir(CurrentFolder);
              break;
          }
        }
        else if (!mutt_str_equal(CurrentFolder, buf_string(&LastDirBackup)))
        {
          mutt_browser_select_dir(CurrentFolder);
        }
      }

      /* When browser tracking feature is disabled, clear LastDirBackup */
      if (!browser_track)
        buf_reset(&LastDirBackup);
    }
    else
    {
      mutt_path_getcwd(&LastDir);
    }

#ifdef USE_IMAP
    if (!priv->state.is_mailbox_list &&
        (imap_path_probe(buf_string(&LastDir), NULL) == MUTT_IMAP))
    {
      init_state(&priv->state, NULL);
      priv->state.imap_browse = true;
      imap_browse(buf_string(&LastDir), &priv->state);
      browser_sort(&priv->state);
    }
    else
#endif
    {
      size_t i = buf_len(&LastDir);
      while ((i > 0) && (buf_string(&LastDir)[--i] == '/'))
        LastDir.data[i] = '\0';
      buf_fix_dptr(&LastDir);
      if (buf_is_empty(&LastDir))
        mutt_path_getcwd(&LastDir);
    }
  }

  buf_reset(file);

  const struct Mapping *help_data = NULL;
#ifdef USE_NNTP
  if (OptNews)
    help_data = FolderNewsHelp;
  else
#endif
    help_data = FolderHelp;

  dlg = simple_dialog_new(MENU_FOLDER, WT_DLG_BROWSER, help_data);

  priv->menu = dlg->wdata;
  dlg->wdata = priv;
  priv->menu->make_entry = folder_make_entry;
  priv->menu->search = select_file_search;
  if (priv->multiple)
    priv->menu->tag = file_tag;

  priv->sbar = window_find_child(dlg, WT_STATUS_BAR);
  priv->win_browser = window_find_child(dlg, WT_MENU);

  struct MuttWindow *win_menu = priv->menu->win;

  // NT_COLOR is handled by the SimpleDialog
  notify_observer_add(NeoMutt->sub->notify, NT_CONFIG, browser_config_observer, priv);
  notify_observer_add(win_menu->notify, NT_WINDOW, browser_window_observer, priv);
  notify_observer_add(NeoMutt->notify, NT_MAILBOX, browser_mailbox_observer, priv);

  struct MuttWindow *old_focus = window_set_focus(priv->menu->win);

  if (priv->state.is_mailbox_list)
  {
    examine_mailboxes(m, NULL, &priv->state);
  }
  else
#ifdef USE_IMAP
      if (!priv->state.imap_browse)
#endif
  {
    // examine_directory() calls browser_add_folder() which needs the menu
    if (examine_directory(m, priv->menu, &priv->state, buf_string(&LastDir),
                          buf_string(priv->prefix)) == -1)
    {
      goto bail;
    }
  }

  init_menu(&priv->state, priv->menu, m, priv->sbar);
  // only now do we have a valid priv->state to attach
  priv->menu->mdata = &priv->state;

  // ---------------------------------------------------------------------------
  // Event Loop
  int op = OP_NULL;
  do
  {
    menu_tagging_dispatcher(priv->menu->win, op);
    window_redraw(NULL);

    op = km_dokey(MENU_FOLDER, GETCH_NO_FLAGS);
    mutt_debug(LL_DEBUG1, "Got op %s (%d)\n", opcodes_get_name(op), op);
    if (op < 0)
      continue;
    if (op == OP_NULL)
    {
      km_error_key(MENU_FOLDER);
      continue;
    }
    mutt_clear_error();

    int rc = browser_function_dispatcher(priv->win_browser, op);

    if (rc == FR_UNKNOWN)
      rc = menu_function_dispatcher(priv->menu->win, op);
    if (rc == FR_UNKNOWN)
      rc = global_function_dispatcher(NULL, op);
  } while (!priv->done);
  // ---------------------------------------------------------------------------

bail:
  window_set_focus(old_focus);
  simple_dialog_free(&dlg);
  browser_private_data_free(&priv);
}
