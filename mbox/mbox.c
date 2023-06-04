/**
 * @file
 * Mbox local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page mbox_mbox Mbox local mailbox type
 *
 * Mbox local mailbox type
 *
 * This file contains code to parse 'mbox' and 'mmdf' style mailboxes.
 *
 * Implementation: #MxMboxOps
 * Implementation: #MxMmdfOps
 */

#include "config.h"
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "lib.h"
#include "progress/lib.h"
#include "copy.h"
#include "globals.h" // IWYU pragma: keep
#include "mutt_header.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"

/**
 * struct MUpdate - Store of new offsets, used by mutt_sync_mailbox()
 */
struct MUpdate
{
  bool valid;
  LOFF_T hdr;
  LOFF_T body;
  long lines;
  LOFF_T length;
};

/**
 * mbox_adata_free - Free the private Account data - Implements Account::adata_free()
 */
static void mbox_adata_free(void **ptr)
{
  struct MboxAccountData *m = *ptr;

  mutt_file_fclose(&m->fp);
  FREE(ptr);
}

/**
 * mbox_adata_new - Create a new MboxAccountData struct
 * @retval ptr New MboxAccountData
 */
static struct MboxAccountData *mbox_adata_new(void)
{
  return mutt_mem_calloc(1, sizeof(struct MboxAccountData));
}

/**
 * init_mailbox - Add Mbox data to the Mailbox
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Error Bad format
 */
static int init_mailbox(struct Mailbox *m)
{
  if (!m || !m->account)
    return -1;
  if ((m->type != MUTT_MBOX) && (m->type != MUTT_MMDF))
    return -1;
  if (m->account->adata)
    return 0;

  m->account->adata = mbox_adata_new();
  m->account->adata_free = mbox_adata_free;
  return 0;
}

/**
 * mbox_adata_get - Get the private data associated with a Mailbox
 * @param m Mailbox
 * @retval ptr Private data
 */
static struct MboxAccountData *mbox_adata_get(struct Mailbox *m)
{
  if (init_mailbox(m) == -1)
    return NULL;
  return m->account->adata;
}

/**
 * mbox_lock_mailbox - Lock a mailbox
 * @param m     Mailbox to lock
 * @param excl  Exclusive lock?
 * @param retry Should retry if unable to lock?
 * @retval  0 Success
 * @retval -1 Failure
 */
static int mbox_lock_mailbox(struct Mailbox *m, bool excl, bool retry)
{
  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  int rc = mutt_file_lock(fileno(adata->fp), excl, retry);
  if (rc == 0)
  {
    adata->locked = true;
  }
  else if (retry && !excl)
  {
    m->readonly = true;
    return 0;
  }

  return rc;
}

/**
 * mbox_unlock_mailbox - Unlock a mailbox
 * @param m Mailbox to unlock
 */
static void mbox_unlock_mailbox(struct Mailbox *m)
{
  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return;

  if (adata->locked)
  {
    fflush(adata->fp);

    mutt_file_unlock(fileno(adata->fp));
    adata->locked = false;
  }
}

/**
 * mmdf_parse_mailbox - Read a mailbox in MMDF format
 * @param m Mailbox
 * @retval enum #MxOpenReturns
 */
static enum MxOpenReturns mmdf_parse_mailbox(struct Mailbox *m)
{
  if (!m)
    return MX_OPEN_ERROR;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return MX_OPEN_ERROR;

  char buf[8192] = { 0 };
  char return_path[1024] = { 0 };
  int count = 0;
  int lines;
  time_t t = 0;
  LOFF_T loc, tmploc;
  struct Email *e = NULL;
  struct stat st = { 0 };
  struct Progress *progress = NULL;
  enum MxOpenReturns rc = MX_OPEN_ERROR;

  if (stat(mailbox_path(m), &st) == -1)
  {
    mutt_perror(mailbox_path(m));
    goto fail;
  }
  mutt_file_get_stat_timespec(&adata->atime, &st, MUTT_STAT_ATIME);
  mutt_file_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
  m->size = st.st_size;

  buf[sizeof(buf) - 1] = '\0';

  if (m->verbose)
  {
    char msg[PATH_MAX] = { 0 };
    snprintf(msg, sizeof(msg), _("Reading %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_READ, 0);
  }

  while (true)
  {
    if (!fgets(buf, sizeof(buf) - 1, adata->fp))
      break;

    if (SigInt)
      break;

    if (mutt_str_equal(buf, MMDF_SEP))
    {
      loc = ftello(adata->fp);
      if (loc < 0)
        goto fail;

      count++;
      if (m->verbose)
        progress_update(progress, count, (int) (loc / (m->size / 100 + 1)));

      mx_alloc_memory(m, m->msg_count);
      e = email_new();
      m->emails[m->msg_count] = e;
      e->offset = loc;
      e->index = m->msg_count;

      if (!fgets(buf, sizeof(buf) - 1, adata->fp))
      {
        /* TODO: memory leak??? */
        mutt_debug(LL_DEBUG1, "unexpected EOF\n");
        break;
      }

      return_path[0] = '\0';

      if (!is_from(buf, return_path, sizeof(return_path), &t))
      {
        if (!mutt_file_seek(adata->fp, loc, SEEK_SET))
        {
          mutt_error(_("Mailbox is corrupt"));
          goto fail;
        }
      }
      else
      {
        e->received = t - mutt_date_local_tz(t);
      }

      e->env = mutt_rfc822_read_header(adata->fp, e, false, false);

      loc = ftello(adata->fp);
      if (loc < 0)
        goto fail;

      if ((e->body->length > 0) && (e->lines > 0))
      {
        tmploc = loc + e->body->length;

        if ((tmploc > 0) && (tmploc < m->size))
        {
          if (!mutt_file_seek(adata->fp, tmploc, SEEK_SET) ||
              !fgets(buf, sizeof(buf) - 1, adata->fp) || !mutt_str_equal(MMDF_SEP, buf))
          {
            (void) mutt_file_seek(adata->fp, loc, SEEK_SET);
            e->body->length = -1;
          }
        }
        else
        {
          e->body->length = -1;
        }
      }
      else
      {
        e->body->length = -1;
      }

      if (e->body->length < 0)
      {
        lines = -1;
        do
        {
          loc = ftello(adata->fp);
          if (loc < 0)
            goto fail;
          if (!fgets(buf, sizeof(buf) - 1, adata->fp))
            break;
          lines++;
        } while (!mutt_str_equal(buf, MMDF_SEP));

        e->lines = lines;
        e->body->length = loc - e->body->offset;
      }

      if (TAILQ_EMPTY(&e->env->return_path) && return_path[0])
        mutt_addrlist_parse(&e->env->return_path, return_path);

      if (TAILQ_EMPTY(&e->env->from))
        mutt_addrlist_copy(&e->env->from, &e->env->return_path, false);

      m->msg_count++;
    }
    else
    {
      mutt_debug(LL_DEBUG1, "corrupt mailbox\n");
      mutt_error(_("Mailbox is corrupt"));
      goto fail;
    }
  }

  if (SigInt)
  {
    SigInt = false;
    rc = MX_OPEN_ABORT; /* action aborted */
    goto fail;
  }

  rc = MX_OPEN_OK;
fail:
  progress_free(&progress);
  return rc;
}

/**
 * mbox_parse_mailbox - Read a mailbox from disk
 * @param m Mailbox
 * @retval enum #MxOpenReturns
 *
 * Note that this function is also called when new mail is appended to the
 * currently open folder, and NOT just when the mailbox is initially read.
 *
 * @note It is assumed that the mailbox being read has been locked before this
 *       routine gets called.  Strange things could happen if it's not!
 */
static enum MxOpenReturns mbox_parse_mailbox(struct Mailbox *m)
{
  if (!m)
    return MX_OPEN_ERROR;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return MX_OPEN_ERROR;

  struct stat st = { 0 };
  char buf[8192], return_path[256];
  struct Email *e_cur = NULL;
  time_t t = 0;
  int count = 0, lines = 0;
  LOFF_T loc;
  struct Progress *progress = NULL;
  enum MxOpenReturns rc = MX_OPEN_ERROR;

  /* Save information about the folder at the time we opened it. */
  if (stat(mailbox_path(m), &st) == -1)
  {
    mutt_perror(mailbox_path(m));
    goto fail;
  }

  m->size = st.st_size;
  mutt_file_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
  mutt_file_get_stat_timespec(&adata->atime, &st, MUTT_STAT_ATIME);

  if (!m->readonly)
    m->readonly = access(mailbox_path(m), W_OK) ? true : false;

  if (m->verbose)
  {
    char msg[PATH_MAX] = { 0 };
    snprintf(msg, sizeof(msg), _("Reading %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_READ, 0);
  }

  loc = ftello(adata->fp);
  while ((fgets(buf, sizeof(buf), adata->fp)) && !SigInt)
  {
    if (is_from(buf, return_path, sizeof(return_path), &t))
    {
      /* Save the Content-Length of the previous message */
      if (count > 0)
      {
        struct Email *e = m->emails[m->msg_count - 1];
        if (e->body->length < 0)
        {
          e->body->length = loc - e->body->offset - 1;
          if (e->body->length < 0)
            e->body->length = 0;
        }
        if (e->lines == 0)
          e->lines = lines ? lines - 1 : 0;
      }

      count++;

      if (m->verbose)
      {
        progress_update(progress, count, (int) (ftello(adata->fp) / (m->size / 100 + 1)));
      }

      mx_alloc_memory(m, m->msg_count);

      m->emails[m->msg_count] = email_new();
      e_cur = m->emails[m->msg_count];
      e_cur->received = t - mutt_date_local_tz(t);
      e_cur->offset = loc;
      e_cur->index = m->msg_count;

      e_cur->env = mutt_rfc822_read_header(adata->fp, e_cur, false, false);

      /* if we know how long this message is, either just skip over the body,
       * or if we don't know how many lines there are, count them now (this will
       * save time by not having to search for the next message marker).  */
      if (e_cur->body->length > 0)
      {
        LOFF_T tmploc;

        loc = ftello(adata->fp);

        /* The test below avoids a potential integer overflow if the
         * content-length is huge (thus necessarily invalid).  */
        tmploc = (e_cur->body->length < m->size) ? (loc + e_cur->body->length + 1) : -1;

        if ((tmploc > 0) && (tmploc < m->size))
        {
          /* check to see if the content-length looks valid.  we expect to
           * to see a valid message separator at this point in the stream */
          if (!mutt_file_seek(adata->fp, tmploc, SEEK_SET) ||
              !fgets(buf, sizeof(buf), adata->fp) || !mutt_str_startswith(buf, "From "))
          {
            mutt_debug(LL_DEBUG1, "bad content-length in message %d (cl=" OFF_T_FMT ")\n",
                       e_cur->index, e_cur->body->length);
            mutt_debug(LL_DEBUG1, "    LINE: %s", buf);
            /* nope, return the previous position */
            (void) mutt_file_seek(adata->fp, loc, SEEK_SET);
            e_cur->body->length = -1;
          }
        }
        else if (tmploc != m->size)
        {
          /* content-length would put us past the end of the file, so it
           * must be wrong */
          e_cur->body->length = -1;
        }

        if (e_cur->body->length != -1)
        {
          /* good content-length.  check to see if we know how many lines
           * are in this message.  */
          if (e_cur->lines == 0)
          {
            int cl = e_cur->body->length;

            /* count the number of lines in this message */
            (void) mutt_file_seek(adata->fp, loc, SEEK_SET);
            while (cl-- > 0)
            {
              if (fgetc(adata->fp) == '\n')
                e_cur->lines++;
            }
          }

          /* return to the offset of the next message separator */
          (void) mutt_file_seek(adata->fp, tmploc, SEEK_SET);
        }
      }

      m->msg_count++;

      if (TAILQ_EMPTY(&e_cur->env->return_path) && return_path[0])
      {
        mutt_addrlist_parse(&e_cur->env->return_path, return_path);
      }

      if (TAILQ_EMPTY(&e_cur->env->from))
        mutt_addrlist_copy(&e_cur->env->from, &e_cur->env->return_path, false);

      lines = 0;
    }
    else
    {
      lines++;
    }

    loc = ftello(adata->fp);
  }

  /* Only set the content-length of the previous message if we have read more
   * than one message during _this_ invocation.  If this routine is called
   * when new mail is received, we need to make sure not to clobber what
   * previously was the last message since the headers may be sorted.  */
  if (count > 0)
  {
    struct Email *e = m->emails[m->msg_count - 1];
    if (e->body->length < 0)
    {
      e->body->length = ftello(adata->fp) - e->body->offset - 1;
      if (e->body->length < 0)
        e->body->length = 0;
    }

    if (e->lines == 0)
      e->lines = lines ? lines - 1 : 0;
  }

  if (SigInt)
  {
    SigInt = false;
    rc = MX_OPEN_ABORT;
    goto fail; /* action aborted */
  }

  rc = MX_OPEN_OK;
fail:
  progress_free(&progress);
  return rc;
}

/**
 * reopen_mailbox - Close and reopen a mailbox
 * @param m          Mailbox
 * @retval >0 Success, e.g. #MX_STATUS_REOPENED, #MX_STATUS_NEW_MAIL
 * @retval -1 Error
 */
static int reopen_mailbox(struct Mailbox *m)
{
  if (!m)
    return -1;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  bool (*cmp_headers)(const struct Email *, const struct Email *) = NULL;
  struct Email **e_old = NULL;
  int old_msg_count;
  bool msg_mod = false;
  int rc = -1;

  /* silent operations */
  m->verbose = false;

  /* our heuristics require the old mailbox to be unsorted */
  const enum SortType c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  if (c_sort != SORT_ORDER)
  {
    cs_subset_str_native_set(NeoMutt->sub, "sort", SORT_ORDER, NULL);
    mailbox_changed(m, NT_MAILBOX_RESORT);
    cs_subset_str_native_set(NeoMutt->sub, "sort", c_sort, NULL);
  }

  e_old = NULL;
  old_msg_count = 0;

  /* simulate a close */
  mutt_hash_free(&m->id_hash);
  mutt_hash_free(&m->subj_hash);
  mutt_hash_free(&m->label_hash);
  FREE(&m->v2r);
  if (m->readonly)
  {
    for (int i = 0; i < m->msg_count; i++)
      email_free(&(m->emails[i])); /* nothing to do! */
    FREE(&m->emails);
  }
  else
  {
    /* save the old headers */
    old_msg_count = m->msg_count;
    e_old = m->emails;
    m->emails = NULL;
  }

  m->email_max = 0; /* force allocation of new headers */
  m->msg_count = 0;
  m->vcount = 0;
  m->msg_tagged = 0;
  m->msg_deleted = 0;
  m->msg_new = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->changed = false;
  m->id_hash = NULL;
  m->subj_hash = NULL;
  mutt_make_label_hash(m);

  switch (m->type)
  {
    case MUTT_MBOX:
    case MUTT_MMDF:
      cmp_headers = email_cmp_strict;
      mutt_file_fclose(&adata->fp);
      adata->fp = mutt_file_fopen(mailbox_path(m), "r");
      if (!adata->fp)
        rc = -1;
      else if (m->type == MUTT_MBOX)
        rc = mbox_parse_mailbox(m);
      else
        rc = mmdf_parse_mailbox(m);
      break;

    default:
      rc = -1;
      break;
  }

  if (rc == -1)
  {
    /* free the old headers */
    for (int i = 0; i < old_msg_count; i++)
      email_free(&(e_old[i]));
    FREE(&e_old);

    m->verbose = true;
    return -1;
  }

  mutt_file_touch_atime(fileno(adata->fp));

  /* now try to recover the old flags */

  if (!m->readonly)
  {
    for (int i = 0; i < m->msg_count; i++)
    {
      bool found = false;

      /* some messages have been deleted, and new  messages have been
       * appended at the end; the heuristic is that old messages have then
       * "advanced" towards the beginning of the folder, so we begin the
       * search at index "i" */
      int j;
      for (j = i; j < old_msg_count; j++)
      {
        if (!e_old[j])
          continue;
        if (cmp_headers(m->emails[i], e_old[j]))
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        for (j = 0; (j < i) && (j < old_msg_count); j++)
        {
          if (!e_old[j])
            continue;
          if (cmp_headers(m->emails[i], e_old[j]))
          {
            found = true;
            break;
          }
        }
      }

      if (found)
      {
        m->changed = true;
        if (e_old[j]->changed)
        {
          /* Only update the flags if the old header was changed;
           * otherwise, the header may have been modified externally,
           * and we don't want to lose _those_ changes */
          mutt_set_flag(m, m->emails[i], MUTT_FLAG, e_old[j]->flagged, true);
          mutt_set_flag(m, m->emails[i], MUTT_REPLIED, e_old[j]->replied, true);
          mutt_set_flag(m, m->emails[i], MUTT_OLD, e_old[j]->old, true);
          mutt_set_flag(m, m->emails[i], MUTT_READ, e_old[j]->read, true);
        }
        mutt_set_flag(m, m->emails[i], MUTT_DELETE, e_old[j]->deleted, true);
        mutt_set_flag(m, m->emails[i], MUTT_PURGE, e_old[j]->purge, true);
        mutt_set_flag(m, m->emails[i], MUTT_TAG, e_old[j]->tagged, true);

        /* we don't need this header any more */
        email_free(&(e_old[j]));
      }
    }

    /* free the remaining old emails */
    for (int j = 0; j < old_msg_count; j++)
    {
      if (e_old[j])
      {
        email_free(&(e_old[j]));
        msg_mod = true;
      }
    }
    FREE(&e_old);
  }

  mailbox_changed(m, NT_MAILBOX_UPDATE);
  m->verbose = true;

  return (m->changed || msg_mod) ? MX_STATUS_REOPENED : MX_STATUS_NEW_MAIL;
}

/**
 * mbox_has_new - Does the mailbox have new mail
 * @param m Mailbox
 * @retval true The mailbox has at least 1 new messages (not old)
 * @retval false otherwise
 */
static bool mbox_has_new(struct Mailbox *m)
{
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;
    if (!e->deleted && !e->read && !e->old)
      return true;
  }
  return false;
}

/**
 * mbox_reset_atime - Reset the access time on the mailbox file
 * @param m  Mailbox
 * @param st Timestamp
 *
 * if mailbox has at least 1 new message, sets mtime > atime of mailbox so
 * mailbox check reports new mail
 */
void mbox_reset_atime(struct Mailbox *m, struct stat *st)
{
  struct utimbuf utimebuf;
  struct stat st2 = { 0 };

  if (!st)
  {
    if (stat(mailbox_path(m), &st2) < 0)
      return;
    st = &st2;
  }

  utimebuf.actime = st->st_atime;
  utimebuf.modtime = st->st_mtime;

  /* When $mbox_check_recent is set, existing new mail is ignored, so do not
   * reset the atime to mtime-1 to signal new mail.  */
  const bool c_mail_check_recent = cs_subset_bool(NeoMutt->sub, "mail_check_recent");
  if (!c_mail_check_recent && (utimebuf.actime >= utimebuf.modtime) && mbox_has_new(m))
  {
    utimebuf.actime = utimebuf.modtime - 1;
  }

  utime(mailbox_path(m), &utimebuf);
}

/**
 * mbox_ac_owns_path - Check whether an Account owns a Mailbox path - Implements MxOps::ac_owns_path() - @ingroup mx_ac_owns_path
 */
static bool mbox_ac_owns_path(struct Account *a, const char *path)
{
  if ((a->type != MUTT_MBOX) && (a->type != MUTT_MMDF))
    return false;

  struct MailboxNode *np = STAILQ_FIRST(&a->mailboxes);
  if (!np)
    return false;

  return mutt_str_equal(mailbox_path(np->mailbox), path);
}

/**
 * mbox_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add() - @ingroup mx_ac_add
 */
static bool mbox_ac_add(struct Account *a, struct Mailbox *m)
{
  return true;
}

/**
 * mbox_open_readwrite - Open an mbox read-write
 * @param m Mailbox
 * @retval ptr FILE handle
 *
 * This function ensures that the FILE and readonly flag are changed atomically.
 */
static FILE *mbox_open_readwrite(struct Mailbox *m)
{
  FILE *fp = fopen(mailbox_path(m), "r+");
  if (fp)
    m->readonly = false;
  return fp;
}

/**
 * mbox_open_readonly - Open an mbox read-only
 * @param m Mailbox
 * @retval ptr FILE handle
 *
 * This function ensures that the FILE and readonly flag are changed atomically.
 */
static FILE *mbox_open_readonly(struct Mailbox *m)
{
  FILE *fp = fopen(mailbox_path(m), "r");
  if (fp)
    m->readonly = true;
  return fp;
}

/**
 * mbox_mbox_open - Open a Mailbox - Implements MxOps::mbox_open() - @ingroup mx_mbox_open
 */
static enum MxOpenReturns mbox_mbox_open(struct Mailbox *m)
{
  if (init_mailbox(m) != 0)
    return MX_OPEN_ERROR;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return MX_OPEN_ERROR;

  adata->fp = m->readonly ? NULL : mbox_open_readwrite(m);
  if (!adata->fp)
  {
    adata->fp = mbox_open_readonly(m);
  }
  if (!adata->fp)
  {
    mutt_perror(mailbox_path(m));
    return MX_OPEN_ERROR;
  }

  mutt_sig_block();
  if (mbox_lock_mailbox(m, false, true) == -1)
  {
    mutt_sig_unblock();
    return MX_OPEN_ERROR;
  }

  m->has_new = true;
  enum MxOpenReturns rc = MX_OPEN_ERROR;
  if (m->type == MUTT_MBOX)
    rc = mbox_parse_mailbox(m);
  else if (m->type == MUTT_MMDF)
    rc = mmdf_parse_mailbox(m);
  else
    rc = MX_OPEN_ERROR;

  if (!mbox_has_new(m))
    m->has_new = false;
  clearerr(adata->fp); // Clear the EOF flag
  mutt_file_touch_atime(fileno(adata->fp));

  mbox_unlock_mailbox(m);
  mutt_sig_unblock();
  return rc;
}

/**
 * mbox_mbox_open_append - Open a Mailbox for appending - Implements MxOps::mbox_open_append() - @ingroup mx_mbox_open_append
 */
static bool mbox_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (init_mailbox(m) != 0)
    return false;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return false;

  if (!adata->fp)
  {
    // create dir recursively
    char *tmp_path = mutt_path_dirname(mailbox_path(m));
    if (mutt_file_mkdir(tmp_path, S_IRWXU) == -1)
    {
      mutt_perror(mailbox_path(m));
      FREE(&tmp_path);
      return false;
    }
    FREE(&tmp_path);

    adata->fp = mutt_file_fopen(mailbox_path(m), (flags & MUTT_NEWFOLDER) ? "w+" : "a+");
    if (!adata->fp)
    {
      mutt_perror(mailbox_path(m));
      return false;
    }

    if (mbox_lock_mailbox(m, true, true) != false)
    {
      mutt_error(_("Couldn't lock %s"), mailbox_path(m));
      mutt_file_fclose(&adata->fp);
      return false;
    }
  }

  if (!mutt_file_seek(adata->fp, 0, SEEK_END))
  {
    mutt_file_fclose(&adata->fp);
    return false;
  }

  return true;
}

/**
 * mbox_mbox_check - Check for new mail - Implements MxOps::mbox_check() - @ingroup mx_mbox_check
 * @param[in]  m Mailbox
 * @retval #MX_STATUS_REOPENED  Mailbox has been reopened
 * @retval #MX_STATUS_NEW_MAIL  New mail has arrived
 * @retval #MX_STATUS_LOCKED    Couldn't lock the file
 */
static enum MxStatus mbox_mbox_check(struct Mailbox *m)
{
  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return MX_STATUS_ERROR;

  if (!adata->fp)
  {
    if (mbox_mbox_open(m) != MX_OPEN_OK)
      return MX_STATUS_ERROR;
    mailbox_changed(m, NT_MAILBOX_INVALID);
  }
  if (!adata->fp)
    return MX_STATUS_ERROR;

  struct stat st = { 0 };
  bool unlock = false;
  bool modified = false;

  if (stat(mailbox_path(m), &st) == 0)
  {
    if ((mutt_file_stat_timespec_compare(&st, MUTT_STAT_MTIME, &m->mtime) == 0) &&
        (st.st_size == m->size))
    {
      return MX_STATUS_OK;
    }

    if (st.st_size == m->size)
    {
      /* the file was touched, but it is still the same length, so just exit */
      mutt_file_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
      return MX_STATUS_OK;
    }

    if (st.st_size > m->size)
    {
      /* lock the file if it isn't already */
      if (!adata->locked)
      {
        mutt_sig_block();
        if (mbox_lock_mailbox(m, false, false) == -1)
        {
          mutt_sig_unblock();
          /* we couldn't lock the mailbox, but nothing serious happened:
           * probably the new mail arrived: no reason to wait till we can
           * parse it: we'll get it on the next pass */
          return MX_STATUS_LOCKED;
        }
        unlock = 1;
      }

      /* Check to make sure that the only change to the mailbox is that
       * message(s) were appended to this file.  My heuristic is that we should
       * see the message separator at *exactly* what used to be the end of the
       * folder.  */
      char buf[1024] = { 0 };
      if (!mutt_file_seek(adata->fp, m->size, SEEK_SET))
      {
        goto error;
      }
      if (fgets(buf, sizeof(buf), adata->fp))
      {
        if (((m->type == MUTT_MBOX) && mutt_str_startswith(buf, "From ")) ||
            ((m->type == MUTT_MMDF) && mutt_str_equal(buf, MMDF_SEP)))
        {
          if (!mutt_file_seek(adata->fp, m->size, SEEK_SET))
          {
            goto error;
          }

          int old_msg_count = m->msg_count;
          if (m->type == MUTT_MBOX)
            mbox_parse_mailbox(m);
          else
            mmdf_parse_mailbox(m);

          if (m->msg_count > old_msg_count)
            mailbox_changed(m, NT_MAILBOX_INVALID);

          /* Only unlock the folder if it was locked inside of this routine.
           * It may have been locked elsewhere, like in
           * mutt_checkpoint_mailbox().  */
          if (unlock)
          {
            mbox_unlock_mailbox(m);
            mutt_sig_unblock();
          }

          return MX_STATUS_NEW_MAIL; /* signal that new mail arrived */
        }
        else
        {
          modified = true;
        }
      }
      else
      {
        mutt_debug(LL_DEBUG1, "fgets returned NULL\n");
        modified = true;
      }
    }
    else
    {
      modified = true;
    }
  }

  if (modified)
  {
    if (reopen_mailbox(m) != -1)
    {
      mailbox_changed(m, NT_MAILBOX_INVALID);
      if (unlock)
      {
        mbox_unlock_mailbox(m);
        mutt_sig_unblock();
      }
      return MX_STATUS_REOPENED;
    }
  }

  /* fatal error */

error:
  mbox_unlock_mailbox(m);
  mx_fastclose_mailbox(m, false);
  mutt_sig_unblock();
  mutt_error(_("Mailbox was corrupted"));
  return MX_STATUS_ERROR;
}

/**
 * mbox_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync() - @ingroup mx_mbox_sync
 */
static enum MxStatus mbox_mbox_sync(struct Mailbox *m)
{
  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return MX_STATUS_ERROR;

  struct Buffer *tempfile = NULL;
  char buf[32] = { 0 };
  int j;
  bool unlink_tempfile = false;
  bool need_sort = false; /* flag to resort mailbox if new mail arrives */
  int first = -1;         /* first message to be written */
  LOFF_T offset;          /* location in mailbox to write changed messages */
  struct stat st = { 0 };
  struct MUpdate *new_offset = NULL;
  struct MUpdate *old_offset = NULL;
  FILE *fp = NULL;
  struct Progress *progress = NULL;
  enum MxStatus rc = MX_STATUS_ERROR;

  /* sort message by their position in the mailbox on disk */
  const enum SortType c_sort = cs_subset_sort(NeoMutt->sub, "sort");
  const unsigned char c_use_threads = cs_subset_enum(NeoMutt->sub, "use_threads");
  if (c_sort != SORT_ORDER)
  {
    cs_subset_str_native_set(NeoMutt->sub, "sort", SORT_ORDER, NULL);
    cs_subset_str_native_set(NeoMutt->sub, "use_threads", UT_FLAT, NULL);
    mailbox_changed(m, NT_MAILBOX_RESORT);
    cs_subset_str_native_set(NeoMutt->sub, "sort", c_sort, NULL);
    cs_subset_str_native_set(NeoMutt->sub, "use_threads", c_use_threads, NULL);
    need_sort = true;
  }

  /* need to open the file for writing in such a way that it does not truncate
   * the file, so use read-write mode.  */
  adata->fp = freopen(mailbox_path(m), "r+", adata->fp);
  if (!adata->fp)
  {
    mx_fastclose_mailbox(m, false);
    mutt_error(_("Fatal error!  Could not reopen mailbox!"));
    goto fatal;
  }

  mutt_sig_block();

  if (mbox_lock_mailbox(m, true, true) == -1)
  {
    mutt_sig_unblock();
    mutt_error(_("Unable to lock mailbox"));
    goto bail;
  }

  /* Check to make sure that the file hasn't changed on disk */
  enum MxStatus check = mbox_mbox_check(m);
  if ((check == MX_STATUS_NEW_MAIL) || (check == MX_STATUS_REOPENED))
  {
    /* new mail arrived, or mailbox reopened */
    rc = check;
    goto bail;
  }
  else if (check < 0)
  {
    goto fatal;
  }

  /* Create a temporary file to write the new version of the mailbox in. */
  tempfile = buf_pool_get();
  buf_mktemp(tempfile);
  int fd = open(buf_string(tempfile), O_WRONLY | O_EXCL | O_CREAT, 0600);
  if ((fd == -1) || !(fp = fdopen(fd, "w")))
  {
    if (fd != -1)
    {
      close(fd);
      unlink_tempfile = true;
    }
    mutt_error(_("Could not create temporary file"));
    goto bail;
  }
  unlink_tempfile = true;

  /* find the first deleted/changed message.  we save a lot of time by only
   * rewriting the mailbox from the point where it has actually changed.  */
  int i = 0;
  for (; (i < m->msg_count) && !m->emails[i]->deleted &&
         !m->emails[i]->changed && !m->emails[i]->attach_del;
       i++)
  {
  }
  if (i == m->msg_count)
  {
    /* this means m->changed or m->msg_deleted was set, but no
     * messages were found to be changed or deleted.  This should
     * never happen, is we presume it is a bug in neomutt.  */
    mutt_error(_("sync: mbox modified, but no modified messages (report this bug)"));
    mutt_debug(LL_DEBUG1, "no modified messages\n");
    goto bail;
  }

  /* save the index of the first changed/deleted message */
  first = i;
  /* where to start overwriting */
  offset = m->emails[i]->offset;

  /* the offset stored in the header does not include the MMDF_SEP, so make
   * sure we seek to the correct location */
  if (m->type == MUTT_MMDF)
    offset -= (sizeof(MMDF_SEP) - 1);

  /* allocate space for the new offsets */
  new_offset = mutt_mem_calloc(m->msg_count - first, sizeof(struct MUpdate));
  old_offset = mutt_mem_calloc(m->msg_count - first, sizeof(struct MUpdate));

  if (m->verbose)
  {
    char msg[PATH_MAX] = { 0 };
    snprintf(msg, sizeof(msg), _("Writing %s..."), mailbox_path(m));
    progress = progress_new(msg, MUTT_PROGRESS_WRITE, m->msg_count);
  }

  for (i = first, j = 0; i < m->msg_count; i++)
  {
    if (m->verbose)
      progress_update(progress, i, i / (m->msg_count / 100 + 1));
    /* back up some information which is needed to restore offsets when
     * something fails.  */

    old_offset[i - first].valid = true;
    old_offset[i - first].hdr = m->emails[i]->offset;
    old_offset[i - first].body = m->emails[i]->body->offset;
    old_offset[i - first].lines = m->emails[i]->lines;
    old_offset[i - first].length = m->emails[i]->body->length;

    if (!m->emails[i]->deleted)
    {
      j++;

      if (m->type == MUTT_MMDF)
      {
        if (fputs(MMDF_SEP, fp) == EOF)
        {
          mutt_perror(buf_string(tempfile));
          goto bail;
        }
      }

      /* save the new offset for this message.  we add 'offset' because the
       * temporary file only contains saved message which are located after
       * 'offset' in the real mailbox */
      new_offset[i - first].hdr = ftello(fp) + offset;

      struct Message *msg = mx_msg_open(m, m->emails[i]);
      const int rc2 = mutt_copy_message(fp, m->emails[i], msg, MUTT_CM_UPDATE,
                                        CH_FROM | CH_UPDATE | CH_UPDATE_LEN, 0);
      mx_msg_close(m, &msg);
      if (rc2 != 0)
      {
        mutt_perror(buf_string(tempfile));
        goto bail;
      }

      /* Since messages could have been deleted, the offsets stored in memory
       * will be wrong, so update what we can, which is the offset of this
       * message, and the offset of the body.  If this is a multipart message,
       * we just flush the in memory cache so that the message will be reparsed
       * if the user accesses it later.  */
      new_offset[i - first].body = ftello(fp) - m->emails[i]->body->length + offset;
      mutt_body_free(&m->emails[i]->body->parts);

      if (m->type == MUTT_MMDF)
      {
        if (fputs(MMDF_SEP, fp) == EOF)
        {
          mutt_perror(buf_string(tempfile));
          goto bail;
        }
      }
      else
      {
        if (fputs("\n", fp) == EOF)
        {
          mutt_perror(buf_string(tempfile));
          goto bail;
        }
      }
    }
  }

  if (mutt_file_fclose(&fp) != 0)
  {
    mutt_debug(LL_DEBUG1, "mutt_file_fclose (&) returned non-zero\n");
    mutt_perror(buf_string(tempfile));
    goto bail;
  }

  /* Save the state of this folder. */
  if (stat(mailbox_path(m), &st) == -1)
  {
    mutt_perror(mailbox_path(m));
    goto bail;
  }

  unlink_tempfile = false;

  fp = fopen(buf_string(tempfile), "r");
  if (!fp)
  {
    mutt_sig_unblock();
    mx_fastclose_mailbox(m, false);
    mutt_debug(LL_DEBUG1, "unable to reopen temp copy of mailbox!\n");
    mutt_perror(buf_string(tempfile));
    FREE(&new_offset);
    FREE(&old_offset);
    goto fatal;
  }

  if (!mutt_file_seek(adata->fp, offset, SEEK_SET) || /* seek the append location */
      /* do a sanity check to make sure the mailbox looks ok */
      !fgets(buf, sizeof(buf), adata->fp) ||
      ((m->type == MUTT_MBOX) && !mutt_str_startswith(buf, "From ")) ||
      ((m->type == MUTT_MMDF) && !mutt_str_equal(MMDF_SEP, buf)))
  {
    mutt_debug(LL_DEBUG1, "message not in expected position\n");
    mutt_debug(LL_DEBUG1, "    LINE: %s\n", buf);
    i = -1;
  }
  else
  {
    if (!mutt_file_seek(adata->fp, offset, SEEK_SET)) /* return to proper offset */
    {
      i = -1;
    }
    else
    {
      /* copy the temp mailbox back into place starting at the first
       * change/deleted message */
      if (m->verbose)
        mutt_message(_("Committing changes..."));
      i = mutt_file_copy_stream(fp, adata->fp);

      if (ferror(adata->fp))
        i = -1;
    }
    if (i >= 0)
    {
      m->size = ftello(adata->fp); /* update the mailbox->size of the mailbox */
      if ((m->size < 0) || (ftruncate(fileno(adata->fp), m->size) != 0))
      {
        i = -1;
        mutt_debug(LL_DEBUG1, "ftruncate() failed\n");
      }
    }
  }

  mutt_file_fclose(&fp);
  fp = NULL;
  mbox_unlock_mailbox(m);

  if ((mutt_file_fclose(&adata->fp) != 0) || (i == -1))
  {
    /* error occurred while writing the mailbox back, so keep the temp copy around */

    struct Buffer *savefile = buf_pool_get();

    const char *const c_tmp_dir = cs_subset_path(NeoMutt->sub, "tmp_dir");
    buf_printf(savefile, "%s/neomutt.%s-%s-%u", NONULL(c_tmp_dir),
               NONULL(Username), NONULL(ShortHostname), (unsigned int) getpid());
    rename(buf_string(tempfile), buf_string(savefile));
    mutt_sig_unblock();
    mx_fastclose_mailbox(m, false);
    buf_pretty_mailbox(savefile);
    mutt_error(_("Write failed!  Saved partial mailbox to %s"), buf_string(savefile));
    buf_pool_release(&savefile);
    FREE(&new_offset);
    FREE(&old_offset);
    goto fatal;
  }

  /* Restore the previous access/modification times */
  mbox_reset_atime(m, &st);

  /* reopen the mailbox in read-only mode */
  adata->fp = mbox_open_readwrite(m);
  if (!adata->fp)
  {
    adata->fp = mbox_open_readonly(m);
  }
  if (!adata->fp)
  {
    unlink(buf_string(tempfile));
    mutt_sig_unblock();
    mx_fastclose_mailbox(m, false);
    mutt_error(_("Fatal error!  Could not reopen mailbox!"));
    FREE(&new_offset);
    FREE(&old_offset);
    goto fatal;
  }

  /* update the offsets of the rewritten messages */
  for (i = first, j = first; i < m->msg_count; i++)
  {
    if (!m->emails[i]->deleted)
    {
      m->emails[i]->offset = new_offset[i - first].hdr;
      m->emails[i]->body->hdr_offset = new_offset[i - first].hdr;
      m->emails[i]->body->offset = new_offset[i - first].body;
      m->emails[i]->index = j++;
    }
  }
  FREE(&new_offset);
  FREE(&old_offset);
  unlink(buf_string(tempfile)); /* remove partial copy of the mailbox */
  buf_pool_release(&tempfile);
  mutt_sig_unblock();

  const bool c_check_mbox_size = cs_subset_bool(NeoMutt->sub, "check_mbox_size");
  if (c_check_mbox_size)
  {
    struct Mailbox *m_tmp = mailbox_find(mailbox_path(m));
    if (m_tmp && !m_tmp->has_new)
      mailbox_update(m_tmp);
  }

  progress_free(&progress);
  return 0; /* signal success */

bail: /* Come here in case of disaster */

  mutt_file_fclose(&fp);

  if (tempfile && unlink_tempfile)
    unlink(buf_string(tempfile));

  /* restore offsets, as far as they are valid */
  if ((first >= 0) && old_offset)
  {
    for (i = first; (i < m->msg_count) && old_offset[i - first].valid; i++)
    {
      m->emails[i]->offset = old_offset[i - first].hdr;
      m->emails[i]->body->hdr_offset = old_offset[i - first].hdr;
      m->emails[i]->body->offset = old_offset[i - first].body;
      m->emails[i]->lines = old_offset[i - first].lines;
      m->emails[i]->body->length = old_offset[i - first].length;
    }
  }

  /* this is ok to call even if we haven't locked anything */
  mbox_unlock_mailbox(m);

  mutt_sig_unblock();
  FREE(&new_offset);
  FREE(&old_offset);

  adata->fp = freopen(mailbox_path(m), "r", adata->fp);
  if (!adata->fp)
  {
    mutt_error(_("Could not reopen mailbox"));
    mx_fastclose_mailbox(m, false);
    goto fatal;
  }

  mailbox_changed(m, NT_MAILBOX_UPDATE);
  if (need_sort)
  {
    /* if the mailbox was reopened, the thread tree will be invalid so make
     * sure to start threading from scratch.  */
    mailbox_changed(m, NT_MAILBOX_RESORT);
  }

fatal:
  buf_pool_release(&tempfile);
  progress_free(&progress);
  return rc;
}

/**
 * mbox_mbox_close - Close a Mailbox - Implements MxOps::mbox_close() - @ingroup mx_mbox_close
 */
static enum MxStatus mbox_mbox_close(struct Mailbox *m)
{
  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return MX_STATUS_ERROR;

  if (!adata->fp)
    return MX_STATUS_OK;

  if (adata->append)
  {
    mutt_file_unlock(fileno(adata->fp));
    mutt_sig_unblock();
  }

  mutt_file_fclose(&adata->fp);

  /* fix up the times so mailbox won't get confused */
  if (m->peekonly && !buf_is_empty(&m->pathbuf) &&
      (mutt_file_timespec_compare(&m->mtime, &adata->atime) > 0))
  {
#ifdef HAVE_UTIMENSAT
    struct timespec ts[2];
    ts[0] = adata->atime;
    ts[1] = m->mtime;
    utimensat(AT_FDCWD, m->path, ts, 0);
#else
    struct utimbuf ut;
    ut.actime = adata->atime.tv_sec;
    ut.modtime = m->mtime.tv_sec;
    utime(mailbox_path(m), &ut);
#endif
  }

  return MX_STATUS_OK;
}

/**
 * mbox_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open() - @ingroup mx_msg_open
 */
static bool mbox_msg_open(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return false;

  msg->fp = mutt_file_fopen(mailbox_path(m), "r");
  if (!msg->fp)
    return false;

  return true;
}

/**
 * mbox_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new() - @ingroup mx_msg_open_new
 */
static bool mbox_msg_open_new(struct Mailbox *m, struct Message *msg, const struct Email *e)
{
  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return false;

  msg->fp = adata->fp;
  return true;
}

/**
 * mbox_msg_commit - Save changes to an email - Implements MxOps::msg_commit() - @ingroup mx_msg_commit
 */
static int mbox_msg_commit(struct Mailbox *m, struct Message *msg)
{
  if (fputc('\n', msg->fp) == EOF)
    return -1;

  if ((fflush(msg->fp) == EOF) || (fsync(fileno(msg->fp)) == -1))
  {
    mutt_perror(_("Can't write message"));
    return -1;
  }

  return 0;
}

/**
 * mbox_msg_close - Close an email - Implements MxOps::msg_close() - @ingroup mx_msg_close
 */
static int mbox_msg_close(struct Mailbox *m, struct Message *msg)
{
  if (msg->write)
    msg->fp = NULL;
  else
    mutt_file_fclose(&msg->fp);

  return 0;
}

/**
 * mbox_msg_padding_size - Bytes of padding between messages - Implements MxOps::msg_padding_size() - @ingroup mx_msg_padding_size
 * @param m Mailbox
 * @retval 1 Always
 */
static int mbox_msg_padding_size(struct Mailbox *m)
{
  return 1;
}

/**
 * mbox_path_probe - Is this an mbox Mailbox? - Implements MxOps::path_probe() - @ingroup mx_path_probe
 */
enum MailboxType mbox_path_probe(const char *path, const struct stat *st)
{
  if (!st)
    return MUTT_UNKNOWN;

  if (S_ISDIR(st->st_mode))
    return MUTT_UNKNOWN;

  if (st->st_size == 0)
    return MUTT_MBOX;

  FILE *fp = fopen(path, "r");
  if (!fp)
    return MUTT_UNKNOWN;

  int ch;
  while ((ch = fgetc(fp)) != EOF)
  {
    /* Some mailbox creation tools erroneously append a blank line to
     * a file before appending a mail message.  This allows neomutt to
     * detect type for and thus open those files. */
    if ((ch != '\n') && (ch != '\r'))
    {
      ungetc(ch, fp);
      break;
    }
  }

  enum MailboxType type = MUTT_UNKNOWN;
  char tmp[256] = { 0 };
  if (fgets(tmp, sizeof(tmp), fp))
  {
    if (mutt_str_startswith(tmp, "From "))
      type = MUTT_MBOX;
    else if (mutt_str_equal(tmp, MMDF_SEP))
      type = MUTT_MMDF;
  }
  mutt_file_fclose(&fp);

  const bool c_check_mbox_size = cs_subset_bool(NeoMutt->sub, "check_mbox_size");
  if (!c_check_mbox_size)
  {
    /* need to restore the times here, the file was not really accessed,
     * only the type was accessed.  This is important, because detection
     * of "new mail" depends on those times set correctly.  */
#ifdef HAVE_UTIMENSAT
    struct timespec ts[2];
    mutt_file_get_stat_timespec(&ts[0], &st, MUTT_STAT_ATIME);
    mutt_file_get_stat_timespec(&ts[1], &st, MUTT_STAT_MTIME);
    utimensat(AT_FDCWD, path, ts, 0);
#else
    struct utimbuf times;
    times.actime = st->st_atime;
    times.modtime = st->st_mtime;
    utime(path, &times);
#endif
  }

  return type;
}

/**
 * mbox_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon() - @ingroup mx_path_canon
 */
static int mbox_path_canon(char *buf, size_t buflen)
{
  mutt_path_canon(buf, buflen, HomeDir, false);
  return 0;
}

/**
 * mbox_path_pretty - Abbreviate a Mailbox path - Implements MxOps::path_pretty() - @ingroup mx_path_pretty
 */
static int mbox_path_pretty(char *buf, size_t buflen, const char *folder)
{
  if (mutt_path_abbr_folder(buf, folder))
    return 0;

  if (mutt_path_pretty(buf, buflen, HomeDir, false))
    return 0;

  return -1;
}

/**
 * mbox_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent() - @ingroup mx_path_parent
 */
static int mbox_path_parent(char *buf, size_t buflen)
{
  if (mutt_path_parent(buf))
    return 0;

  if (buf[0] == '~')
    mutt_path_canon(buf, buflen, HomeDir, false);

  if (mutt_path_parent(buf))
    return 0;

  return -1;
}

/**
 * mbox_path_is_empty - Is the mailbox empty - Implements MxOps::path_is_empty() - @ingroup mx_path_is_empty
 */
static int mbox_path_is_empty(const char *path)
{
  return mutt_file_check_empty(path);
}

/**
 * mmdf_msg_commit - Save changes to an email - Implements MxOps::msg_commit() - @ingroup mx_msg_commit
 */
static int mmdf_msg_commit(struct Mailbox *m, struct Message *msg)
{
  if (fputs(MMDF_SEP, msg->fp) == EOF)
    return -1;

  if ((fflush(msg->fp) == EOF) || (fsync(fileno(msg->fp)) == -1))
  {
    mutt_perror(_("Can't write message"));
    return -1;
  }

  return 0;
}

/**
 * mmdf_msg_padding_size - Bytes of padding between messages - Implements MxOps::msg_padding_size() - @ingroup mx_msg_padding_size
 * @param m Mailbox
 * @retval 10 Always
 */
static int mmdf_msg_padding_size(struct Mailbox *m)
{
  return 10;
}

/**
 * mbox_mbox_check_stats - Check the Mailbox statistics - Implements MxOps::mbox_check_stats() - @ingroup mx_mbox_check_stats
 */
static enum MxStatus mbox_mbox_check_stats(struct Mailbox *m, uint8_t flags)
{
  struct stat st = { 0 };
  if (stat(mailbox_path(m), &st) != 0)
    return MX_STATUS_ERROR;

  bool new_or_changed;

  const bool c_check_mbox_size = cs_subset_bool(NeoMutt->sub, "check_mbox_size");
  if (c_check_mbox_size)
  {
    new_or_changed = (st.st_size > m->size);
  }
  else
  {
    new_or_changed =
        (mutt_file_stat_compare(&st, MUTT_STAT_MTIME, &st, MUTT_STAT_ATIME) > 0) ||
        (m->newly_created &&
         (mutt_file_stat_compare(&st, MUTT_STAT_CTIME, &st, MUTT_STAT_MTIME) == 0) &&
         (mutt_file_stat_compare(&st, MUTT_STAT_CTIME, &st, MUTT_STAT_ATIME) == 0));
  }

  if (new_or_changed)
  {
    const bool c_mail_check_recent = cs_subset_bool(NeoMutt->sub, "mail_check_recent");
    if (!c_mail_check_recent ||
        (mutt_file_stat_timespec_compare(&st, MUTT_STAT_MTIME, &m->last_visited) > 0))
    {
      m->has_new = true;
    }
  }
  else if (c_check_mbox_size)
  {
    /* some other program has deleted mail from the folder */
    m->size = (off_t) st.st_size;
  }

  if (m->newly_created && ((st.st_ctime != st.st_mtime) || (st.st_ctime != st.st_atime)))
    m->newly_created = false;

  if (flags & (MUTT_MAILBOX_CHECK_FORCE | MUTT_MAILBOX_CHECK_FORCE_STATS))
  {
    struct MboxAccountData *adata = mbox_adata_get(m);
    if (adata && mutt_file_stat_timespec_compare(&st, MUTT_STAT_MTIME,
                                                 &adata->stats_last_checked) > 0)
    {
      bool old_peek = m->peekonly;
      mx_mbox_open(m, MUTT_QUIET | MUTT_NOSORT | MUTT_PEEK);
      mx_mbox_close(m);
      m->peekonly = old_peek;
      adata->stats_last_checked.tv_sec = mutt_date_now();
    }
  }

  if (m->has_new || m->msg_new)
    return MX_STATUS_NEW_MAIL;
  return MX_STATUS_OK;
}

/**
 * MxMboxOps - Mbox Mailbox - Implements ::MxOps - @ingroup mx_api
 */
const struct MxOps MxMboxOps = {
  // clang-format off
  .type            = MUTT_MBOX,
  .name             = "mbox",
  .is_local         = true,
  .ac_owns_path     = mbox_ac_owns_path,
  .ac_add           = mbox_ac_add,
  .mbox_open        = mbox_mbox_open,
  .mbox_open_append = mbox_mbox_open_append,
  .mbox_check       = mbox_mbox_check,
  .mbox_check_stats = mbox_mbox_check_stats,
  .mbox_sync        = mbox_mbox_sync,
  .mbox_close       = mbox_mbox_close,
  .msg_open         = mbox_msg_open,
  .msg_open_new     = mbox_msg_open_new,
  .msg_commit       = mbox_msg_commit,
  .msg_close        = mbox_msg_close,
  .msg_padding_size = mbox_msg_padding_size,
  .msg_save_hcache  = NULL,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = mbox_path_probe,
  .path_canon       = mbox_path_canon,
  .path_pretty      = mbox_path_pretty,
  .path_parent      = mbox_path_parent,
  .path_is_empty    = mbox_path_is_empty,
  // clang-format on
};

/**
 * MxMmdfOps - MMDF Mailbox - Implements ::MxOps - @ingroup mx_api
 */
const struct MxOps MxMmdfOps = {
  // clang-format off
  .type            = MUTT_MMDF,
  .name             = "mmdf",
  .is_local         = true,
  .ac_owns_path     = mbox_ac_owns_path,
  .ac_add           = mbox_ac_add,
  .mbox_open        = mbox_mbox_open,
  .mbox_open_append = mbox_mbox_open_append,
  .mbox_check       = mbox_mbox_check,
  .mbox_check_stats = mbox_mbox_check_stats,
  .mbox_sync        = mbox_mbox_sync,
  .mbox_close       = mbox_mbox_close,
  .msg_open         = mbox_msg_open,
  .msg_open_new     = mbox_msg_open_new,
  .msg_commit       = mmdf_msg_commit,
  .msg_close        = mbox_msg_close,
  .msg_padding_size = mmdf_msg_padding_size,
  .msg_save_hcache  = NULL,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = mbox_path_probe,
  .path_canon       = mbox_path_canon,
  .path_pretty      = mbox_path_pretty,
  .path_parent      = mbox_path_parent,
  .path_is_empty    = mbox_path_is_empty,
  // clang-format on
};
