/**
 * @file
 * Representation of a mailbox
 *
 * @authors
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2016-2017 Kevin J. McCarthy <kevin@8t8.us>
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

#include "config.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "buffy.h"
#include "context.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_window.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

static time_t BuffyTime = 0; /**< last time we started checking for mail */
static time_t BuffyStatsTime = 0; /**< last time we check performed mail_check_stats */
time_t BuffyDoneTime = 0; /**< last time we knew for sure how much mail there was. */
static short BuffyCount = 0;  /**< how many boxes with new mail */
static short BuffyNotify = 0; /**< # of unnotified new boxes */

/**
 * fseek_last_message - Find the last message in the file
 * @retval 0 on success
 * @retval -1 if no message found
 */
static int fseek_last_message(FILE *f)
{
  LOFF_T pos;
  char buffer[BUFSIZ + 9]; /* 7 for "\n\nFrom " */
  size_t bytes_read;

  memset(buffer, 0, sizeof(buffer));
  fseek(f, 0, SEEK_END);
  pos = ftello(f);

  /* Set `bytes_read' to the size of the last, probably partial, buffer;
   * 0 < `bytes_read' <= `BUFSIZ'.  */
  bytes_read = pos % BUFSIZ;
  if (bytes_read == 0)
    bytes_read = BUFSIZ;
  /* Make `pos' a multiple of `BUFSIZ' (0 if the file is short), so that all
   * reads will be on block boundaries, which might increase efficiency.  */
  while ((pos -= bytes_read) >= 0)
  {
    /* we save in the buffer at the end the first 7 chars from the last read */
    strncpy(buffer + BUFSIZ, buffer, 5 + 2); /* 2 == 2 * mutt_str_strlen(CRLF) */
    fseeko(f, pos, SEEK_SET);
    bytes_read = fread(buffer, sizeof(char), bytes_read, f);
    if (bytes_read == 0)
      return -1;
    /* 'i' is Index into `buffer' for scanning.  */
    for (int i = bytes_read; i >= 0; i--)
    {
      if (mutt_str_strncmp(buffer + i, "\n\nFrom ", mutt_str_strlen("\n\nFrom ")) == 0)
      { /* found it - go to the beginning of the From */
        fseeko(f, pos + i + 2, SEEK_SET);
        return 0;
      }
    }
    bytes_read = BUFSIZ;
  }

  /* here we are at the beginning of the file */
  if (mutt_str_strncmp("From ", buffer, 5) == 0)
  {
    fseek(f, 0, SEEK_SET);
    return 0;
  }

  return -1;
}

/**
 * test_last_status_new - Is the last message new
 * @retval 1 if the last message is new
 */
static int test_last_status_new(FILE *f)
{
  struct Header *hdr = NULL;
  struct Envelope *tmp_envelope = NULL;
  int result = 0;

  if (fseek_last_message(f) == -1)
    return 0;

  hdr = mutt_header_new();
  tmp_envelope = mutt_rfc822_read_header(f, hdr, 0, 0);
  if (!(hdr->read || hdr->old))
    result = 1;

  mutt_env_free(&tmp_envelope);
  mutt_header_free(&hdr);

  return result;
}

static int test_new_folder(const char *path)
{
  FILE *f = NULL;
  int rc = 0;
  int typ;

  typ = mx_get_magic(path);

  if (typ != MUTT_MBOX && typ != MUTT_MMDF)
    return 0;

  f = fopen(path, "rb");
  if (f)
  {
    rc = test_last_status_new(f);
    mutt_file_fclose(&f);
  }

  return rc;
}

static struct Buffy *buffy_new(const char *path)
{
  char rp[PATH_MAX] = "";

  struct Buffy *buffy = mutt_mem_calloc(1, sizeof(struct Buffy));
  mutt_str_strfcpy(buffy->path, path, sizeof(buffy->path));
  char *r = realpath(path, rp);
  mutt_str_strfcpy(buffy->realpath, r ? rp : path, sizeof(buffy->realpath));
  buffy->next = NULL;
  buffy->magic = 0;

  return buffy;
}

static void buffy_free(struct Buffy **mailbox)
{
  if (mailbox && *mailbox)
    FREE(&(*mailbox)->desc);
  FREE(mailbox);
}

/**
 * buffy_maildir_check_dir - Check for new mail / mail counts
 * @param mailbox     Mailbox to check
 * @param dir_name    Path to mailbox
 * @param check_new   if true, check for new mail
 * @param check_stats if true, count total, new, and flagged messages
 * @retval 1 if the dir has new mail
 *
 * Checks the specified maildir subdir (cur or new) for new mail or mail counts.
 */
static int buffy_maildir_check_dir(struct Buffy *mailbox, const char *dir_name,
                                   bool check_new, bool check_stats)
{
  char path[LONG_STRING];
  char msgpath[LONG_STRING];
  DIR *dirp = NULL;
  struct dirent *de = NULL;
  char *p = NULL;
  int rc = 0;
  struct stat sb;

  snprintf(path, sizeof(path), "%s/%s", mailbox->path, dir_name);

  /* when $mail_check_recent is set, if the new/ directory hasn't been modified since
   * the user last exited the mailbox, then we know there is no recent mail.
   */
  if (check_new && MailCheckRecent)
  {
    if (stat(path, &sb) == 0 && sb.st_mtime < mailbox->last_visited)
    {
      rc = 0;
      check_new = false;
    }
  }

  if (!(check_new || check_stats))
    return rc;

  dirp = opendir(path);
  if (!dirp)
  {
    mailbox->magic = 0;
    return 0;
  }

  while ((de = readdir(dirp)) != NULL)
  {
    if (*de->d_name == '.')
      continue;

    p = strstr(de->d_name, ":2,");
    if (p && strchr(p + 3, 'T'))
      continue;

    if (check_stats)
    {
      mailbox->msg_count++;
      if (p && strchr(p + 3, 'F'))
        mailbox->msg_flagged++;
    }
    if (!p || !strchr(p + 3, 'S'))
    {
      if (check_stats)
        mailbox->msg_unread++;
      if (check_new)
      {
        if (MailCheckRecent)
        {
          snprintf(msgpath, sizeof(msgpath), "%s/%s", path, de->d_name);
          /* ensure this message was received since leaving this mailbox */
          if (stat(msgpath, &sb) == 0 && (sb.st_ctime <= mailbox->last_visited))
            continue;
        }
        mailbox->new = true;
        rc = 1;
        check_new = false;
        if (!check_stats)
          break;
      }
    }
  }

  closedir(dirp);

  return rc;
}

/**
 * buffy_maildir_check - Check for new mail in a maildir mailbox
 * @param mailbox     Mailbox to check
 * @param check_stats if true, also count total, new, and flagged messages
 * @retval 1 if the mailbox has new mail
 */
static int buffy_maildir_check(struct Buffy *mailbox, bool check_stats)
{
  int rc = 1;
  bool check_new = true;

  if (check_stats)
  {
    mailbox->msg_count = 0;
    mailbox->msg_unread = 0;
    mailbox->msg_flagged = 0;
  }

  rc = buffy_maildir_check_dir(mailbox, "new", check_new, check_stats);

  check_new = !rc && MaildirCheckCur;
  if (check_new || check_stats)
    if (buffy_maildir_check_dir(mailbox, "cur", check_new, check_stats))
      rc = 1;

  return rc;
}

/**
 * buffy_mbox_check - Check for new mail for an mbox mailbox
 * @param mailbox     Mailbox to check
 * @param sb          stat(2) information about the mailbox
 * @param check_stats if true, also count total, new, and flagged messages
 * @retval 1 if the mailbox has new mail
 */
static int buffy_mbox_check(struct Buffy *mailbox, struct stat *sb, bool check_stats)
{
  int rc = 0;
  int new_or_changed;
  struct Context ctx;

  if (CheckMboxSize)
    new_or_changed = sb->st_size > mailbox->size;
  else
    new_or_changed = sb->st_mtime > sb->st_atime ||
                     (mailbox->newly_created && sb->st_ctime == sb->st_mtime &&
                      sb->st_ctime == sb->st_atime);

  if (new_or_changed)
  {
    if (!MailCheckRecent || sb->st_mtime > mailbox->last_visited)
    {
      rc = 1;
      mailbox->new = true;
    }
  }
  else if (CheckMboxSize)
  {
    /* some other program has deleted mail from the folder */
    mailbox->size = (off_t) sb->st_size;
  }

  if (mailbox->newly_created && (sb->st_ctime != sb->st_mtime || sb->st_ctime != sb->st_atime))
    mailbox->newly_created = false;

  if (check_stats && (mailbox->stats_last_checked < sb->st_mtime))
  {
    if (mx_open_mailbox(mailbox->path, MUTT_READONLY | MUTT_QUIET | MUTT_NOSORT | MUTT_PEEK,
                        &ctx) != NULL)
    {
      mailbox->msg_count = ctx.msgcount;
      mailbox->msg_unread = ctx.unread;
      mailbox->msg_flagged = ctx.flagged;
      mailbox->stats_last_checked = ctx.mtime;
      mx_close_mailbox(&ctx, 0);
    }
  }

  return rc;
}

static void buffy_check(struct Buffy *tmp, struct stat *contex_sb, bool check_stats)
{
  struct stat sb;
#ifdef USE_SIDEBAR
  short orig_new;
  int orig_count, orig_unread, orig_flagged;
#endif

  memset(&sb, 0, sizeof (sb));

#ifdef USE_SIDEBAR
  orig_new = tmp->new;
  orig_count = tmp->msg_count;
  orig_unread = tmp->msg_unread;
  orig_flagged = tmp->msg_flagged;
#endif

  if (tmp->magic != MUTT_IMAP)
  {
    tmp->new = false;
#ifdef USE_POP
    if (mx_is_pop(tmp->path))
      tmp->magic = MUTT_POP;
    else
#endif
#ifdef USE_NNTP
        if ((tmp->magic == MUTT_NNTP) || mx_is_nntp(tmp->path))
      tmp->magic = MUTT_NNTP;
#endif
#ifdef USE_NOTMUCH
    if (mx_is_notmuch(tmp->path))
      tmp->magic = MUTT_NOTMUCH;
    else
#endif
        if (stat(tmp->path, &sb) != 0 || (S_ISREG(sb.st_mode) && sb.st_size == 0) ||
            (!tmp->magic && (tmp->magic = mx_get_magic(tmp->path)) <= 0))
    {
      /* if the mailbox still doesn't exist, set the newly created flag to be
       * ready for when it does. */
      tmp->newly_created = true;
      tmp->magic = 0;
      tmp->size = 0;
      return;
    }
  }

  /* check to see if the folder is the currently selected folder before polling */
  if (!Context || !Context->path ||
      ((tmp->magic == MUTT_IMAP ||
#ifdef USE_NNTP
        tmp->magic == MUTT_NNTP ||
#endif
#ifdef USE_NOTMUCH
        tmp->magic == MUTT_NOTMUCH ||
#endif
        tmp->magic == MUTT_POP) ?
           (mutt_str_strcmp(tmp->path, Context->path) != 0) :
           (sb.st_dev != contex_sb->st_dev || sb.st_ino != contex_sb->st_ino)))
  {
    switch (tmp->magic)
    {
      case MUTT_MBOX:
      case MUTT_MMDF:
        if (buffy_mbox_check(tmp, &sb, check_stats) > 0)
          BuffyCount++;
        break;

      case MUTT_MAILDIR:
        if (buffy_maildir_check(tmp, check_stats) > 0)
          BuffyCount++;
        break;

      case MUTT_MH:
        if (mh_buffy(tmp, check_stats))
          BuffyCount++;
        break;
#ifdef USE_NOTMUCH
      case MUTT_NOTMUCH:
        tmp->msg_count = 0;
        tmp->msg_unread = 0;
        tmp->msg_flagged = 0;
        nm_nonctx_get_count(tmp->path, &tmp->msg_count, &tmp->msg_unread);
        if (tmp->msg_unread > 0)
        {
          BuffyCount++;
          tmp->new = true;
        }
        break;
#endif
    }
  }
  else if (CheckMboxSize && Context && Context->path)
    tmp->size = (off_t) sb.st_size; /* update the size of current folder */

#ifdef USE_SIDEBAR
  if ((orig_new != tmp->new) || (orig_count != tmp->msg_count) ||
      (orig_unread != tmp->msg_unread) || (orig_flagged != tmp->msg_flagged))
  {
    mutt_menu_set_current_redraw(REDRAW_SIDEBAR);
  }
#endif

  if (!tmp->new)
    tmp->notified = false;
  else if (!tmp->notified)
    BuffyNotify++;
}

/**
 * buffy_get - fetch buffy object for given path, if present
 */
static struct Buffy *buffy_get(const char *path)
{
  struct Buffy *cur = NULL;
  char *epath = NULL;

  if (!path)
    return NULL;

  epath = mutt_str_strdup(path);
  mutt_expand_path(epath, mutt_str_strlen(epath));

  for (cur = Incoming; cur; cur = cur->next)
  {
    /* must be done late because e.g. IMAP delimiter may change */
    mutt_expand_path(cur->path, sizeof(cur->path));
    if (mutt_str_strcmp(cur->path, path) == 0)
    {
      FREE(&epath);
      return cur;
    }
  }

  FREE(&epath);
  return NULL;
}

void mutt_buffy_cleanup(const char *buf, struct stat *st)
{
  struct utimbuf ut;

  if (CheckMboxSize)
  {
    struct Buffy *b = mutt_find_mailbox(buf);
    if (b && !b->new)
      mutt_update_mailbox(b);
  }
  else
  {
    /* fix up the times so buffy won't get confused */
    if (st->st_mtime > st->st_atime)
    {
      ut.actime = st->st_atime;
      ut.modtime = time(NULL);
      utime(buf, &ut);
    }
    else
      utime(buf, NULL);
  }
}

struct Buffy *mutt_find_mailbox(const char *path)
{
  struct stat sb;
  struct stat tmp_sb;

  if (stat(path, &sb) != 0)
    return NULL;

  for (struct Buffy *b = Incoming; b; b = b->next)
  {
    if (stat(b->path, &tmp_sb) == 0 && sb.st_dev == tmp_sb.st_dev &&
        sb.st_ino == tmp_sb.st_ino)
    {
      return b;
    }
  }
  return NULL;
}

void mutt_update_mailbox(struct Buffy *b)
{
  struct stat sb;

  if (!b)
    return;

  if (stat(b->path, &sb) == 0)
    b->size = (off_t) sb.st_size;
  else
    b->size = 0;
  return;
}

int mutt_parse_mailboxes(struct Buffer *path, struct Buffer *s,
                         unsigned long data, struct Buffer *err)
{
  struct Buffy **b = NULL;
  char buf[_POSIX_PATH_MAX];
  struct stat sb;
  char f1[PATH_MAX];
  char *p = NULL;

  while (MoreArgs(s))
  {
    char *desc = NULL;

    if (data & MUTT_NAMED)
    {
      mutt_extract_token(path, s, 0);
      if (path->data && *path->data)
        desc = mutt_str_strdup(path->data);
      else
        continue;
    }

    mutt_extract_token(path, s, 0);
#ifdef USE_NOTMUCH
    if (mx_is_notmuch(path->data))
      nm_normalize_uri(buf, path->data, sizeof(buf));
    else
#endif
      mutt_str_strfcpy(buf, path->data, sizeof(buf));

    mutt_expand_path(buf, sizeof(buf));

    /* Skip empty tokens. */
    if (!*buf)
    {
      FREE(&desc);
      continue;
    }

    /* avoid duplicates */
    p = realpath(buf, f1);
    for (b = &Incoming; *b; b = &((*b)->next))
    {
      if (mutt_str_strcmp(p ? p : buf, (*b)->realpath) == 0)
      {
        mutt_debug(3, "mailbox '%s' already registered as '%s'\n", buf, (*b)->path);
        break;
      }
    }

    if (*b)
    {
      FREE(&desc);
      continue;
    }

    *b = buffy_new(buf);

    (*b)->new = false;
    (*b)->notified = true;
    (*b)->newly_created = false;
    (*b)->desc = desc;
#ifdef USE_NOTMUCH
    if (mx_is_notmuch((*b)->path))
    {
      (*b)->magic = MUTT_NOTMUCH;
      (*b)->size = 0;
    }
    else
#endif
    {
      /* for check_mbox_size, it is important that if the folder is new (tested by
       * reading it), the size is set to 0 so that later when we check we see
       * that it increased. without check_mbox_size we probably don't care.
       */
      if (CheckMboxSize && stat((*b)->path, &sb) == 0 && !test_new_folder((*b)->path))
      {
        /* some systems out there don't have an off_t type */
        (*b)->size = (off_t) sb.st_size;
      }
      else
        (*b)->size = 0;
    }

#ifdef USE_SIDEBAR
    mutt_sb_notify_mailbox(*b, 1);
#endif
  }
  return 0;
}

int mutt_parse_unmailboxes(struct Buffer *path, struct Buffer *s,
                           unsigned long data, struct Buffer *err)
{
  char buf[_POSIX_PATH_MAX];
  bool clear_all = false;

  while (!clear_all && MoreArgs(s))
  {
    mutt_extract_token(path, s, 0);

    if (mutt_str_strcmp(path->data, "*") == 0)
    {
      clear_all = true;
    }
    else
    {
#ifdef USE_NOTMUCH
      if (mx_is_notmuch(path->data))
      {
        nm_normalize_uri(buf, path->data, sizeof(buf));
      }
      else
#endif
      {
        mutt_str_strfcpy(buf, path->data, sizeof(buf));
        mutt_expand_path(buf, sizeof(buf));
      }
    }

    for (struct Buffy **b = &Incoming; *b;)
    {
      /* Decide whether to delete all normal mailboxes or all virtual */
      bool virt = (((*b)->magic == MUTT_NOTMUCH) && (data & MUTT_VIRTUAL));
      bool norm = (((*b)->magic != MUTT_NOTMUCH) && !(data & MUTT_VIRTUAL));
      bool clear_this = clear_all && (virt || norm);

      if (clear_this || (mutt_str_strcasecmp(buf, (*b)->path) == 0) ||
          (mutt_str_strcasecmp(buf, (*b)->desc) == 0))
      {
        struct Buffy *next = (*b)->next;
#ifdef USE_SIDEBAR
        mutt_sb_notify_mailbox(*b, 0);
#endif
        buffy_free(b);
        *b = next;
        continue;
      }

      b = &((*b)->next);
    }
  }
  return 0;
}

/**
 * mutt_buffy_check - Check all Incoming for new mail
 *
 * Check all Incoming for new mail and total/new/flagged messages
 * force: if true, ignore MailCheck and check for new mail anyway
 */
int mutt_buffy_check(bool force)
{
  struct stat contex_sb;
  time_t t;
  bool check_stats = false;
  contex_sb.st_dev = 0;
  contex_sb.st_ino = 0;

#ifdef USE_IMAP
  /* update postponed count as well, on force */
  if (force)
    mutt_update_num_postponed();
#endif

  /* fastest return if there are no mailboxes */
  if (!Incoming)
    return 0;

  t = time(NULL);
  if (!force && (t - BuffyTime < MailCheck))
    return BuffyCount;

  if (MailCheckStats && (t - BuffyStatsTime >= MailCheckStatsInterval))
  {
    check_stats = true;
    BuffyStatsTime = t;
  }

  BuffyTime = t;
  BuffyCount = 0;
  BuffyNotify = 0;

#ifdef USE_IMAP
  BuffyCount += imap_buffy_check(check_stats);
#endif

  /* check device ID and serial number instead of comparing paths */
  if (!Context || Context->magic == MUTT_IMAP || Context->magic == MUTT_POP
#ifdef USE_NNTP
      || Context->magic == MUTT_NNTP
#endif
      || stat(Context->path, &contex_sb) != 0)
  {
    contex_sb.st_dev = 0;
    contex_sb.st_ino = 0;
  }

  for (struct Buffy *b = Incoming; b; b = b->next)
    buffy_check(b, &contex_sb, check_stats);

  BuffyDoneTime = BuffyTime;
  return BuffyCount;
}

int mutt_buffy_list(void)
{
  struct Buffy *b = NULL;
  char path[_POSIX_PATH_MAX];
  char buffylist[2 * STRING];
  size_t pos = 0;
  int first = 1;

  int have_unnotified = BuffyNotify;

  buffylist[0] = '\0';
  pos += strlen(strncat(buffylist, _("New mail in "), sizeof(buffylist) - 1 - pos));
  for (b = Incoming; b; b = b->next)
  {
    /* Is there new mail in this mailbox? */
    if (!b->new || (have_unnotified && b->notified))
      continue;

    mutt_str_strfcpy(path, b->path, sizeof(path));
    mutt_pretty_mailbox(path, sizeof(path));

    if (!first && (MuttMessageWindow->cols >= 7) &&
        (pos + strlen(path) >= (size_t) MuttMessageWindow->cols - 7))
    {
      break;
    }

    if (!first)
      pos += strlen(strncat(buffylist + pos, ", ", sizeof(buffylist) - 1 - pos));

    /* Prepend an asterisk to mailboxes not already notified */
    if (!b->notified)
    {
      /* pos += strlen (strncat(buffylist + pos, "*", sizeof(buffylist)-1-pos)); */
      b->notified = true;
      BuffyNotify--;
    }
    pos += strlen(strncat(buffylist + pos, path, sizeof(buffylist) - 1 - pos));
    first = 0;
  }
  if (!first && b)
  {
    strncat(buffylist + pos, ", ...", sizeof(buffylist) - 1 - pos);
  }
  if (!first)
  {
    mutt_message("%s", buffylist);
    return 1;
  }
  /* there were no mailboxes needing to be notified, so clean up since
   * BuffyNotify has somehow gotten out of sync
   */
  BuffyNotify = 0;
  return 0;
}

void mutt_buffy_setnotified(const char *path)
{
  struct Buffy *buffy = buffy_get(path);
  if (!buffy)
    return;

  buffy->notified = true;
  time(&buffy->last_visited);
}

int mutt_buffy_notify(void)
{
  if (mutt_buffy_check(false) && BuffyNotify)
  {
    return (mutt_buffy_list());
  }
  return 0;
}

/**
 * mutt_buffy - incoming folders completion routine
 * @param s    Buffer containing name of current mailbox
 * @param slen Buffer length
 *
 * Given a folder name, find the next incoming folder with new mail.
 */
void mutt_buffy(char *s, size_t slen)
{
  mutt_expand_path(s, slen);

  if (mutt_buffy_check(false))
  {
    int found = 0;
    for (int pass = 0; pass < 2; pass++)
    {
      for (struct Buffy *b = Incoming; b; b = b->next)
      {
        if (b->magic == MUTT_NOTMUCH) /* only match real mailboxes */
          continue;
        mutt_expand_path(b->path, sizeof(b->path));
        if ((found || pass) && b->new)
        {
          mutt_str_strfcpy(s, b->path, slen);
          mutt_pretty_mailbox(s, slen);
          return;
        }
        if (mutt_str_strcmp(s, b->path) == 0)
          found = 1;
      }
    }

    mutt_buffy_check(true); /* buffy was wrong - resync things */
  }

  /* no folders with new mail */
  *s = '\0';
}

#ifdef USE_NOTMUCH
void mutt_buffy_vfolder(char *s, size_t slen)
{
  if (mutt_buffy_check(false))
  {
    bool found = false;
    for (int pass = 0; pass < 2; pass++)
    {
      for (struct Buffy *b = Incoming; b; b = b->next)
      {
        if (b->magic != MUTT_NOTMUCH)
          continue;
        if ((found || pass) && b->new)
        {
          mutt_str_strfcpy(s, b->desc, slen);
          return;
        }
        if (mutt_str_strcmp(s, b->path) == 0)
          found = true;
      }
    }

    mutt_buffy_check(true); /* buffy was wrong - resync things */
  }

  /* no folders with new mail */
  *s = '\0';
}
#endif
