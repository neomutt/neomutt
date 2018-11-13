/**
 * @file
 * Mbox local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2018 Richard Russon <rich@flatcap.org>
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
 */

#include "config.h"
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "mutt/mutt.h"
#include "email/lib.h"
#include "mutt.h"
#include "mbox.h"
#include "account.h"
#include "context.h"
#include "copy.h"
#include "globals.h"
#include "mailbox.h"
#include "mutt_header.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "mx.h"
#include "progress.h"
#include "protos.h"
#include "sort.h"

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
 * mbox_adata_free - Free data attached to the Mailbox
 * @param ptr Private mailbox data
 */
static void mbox_adata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

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
 * mbox_adata_get - Get the private data associated with a Mailbox
 * @param m Mailbox
 * @retval ptr Private data
 */
static struct MboxAccountData *mbox_adata_get(struct Mailbox *m)
{
  if (!m || (m->magic != MUTT_MBOX))
    return NULL;
  struct Account *a = m->account;
  if (!a)
    return NULL;
  return a->adata;
}

/**
 * init_mailbox - Add Mbox data to the Mailbox
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Error Bad format
 */
static int init_mailbox(struct Mailbox *m)
{
  if (!m || (m->magic != MUTT_MBOX))
    return -1;

  if (m->account && m->account->adata)
    return 0;

  m->account->adata = mbox_adata_new();
  m->account->free_adata = mbox_adata_free;
  return 0;
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

  int r = mutt_file_lock(fileno(adata->fp), excl, retry);
  if (r == 0)
    adata->locked = true;
  else if (retry && !excl)
  {
    m->readonly = true;
    return 0;
  }

  return r;
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
 * @param ctx Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 * @retval -2 Aborted
 */
static int mmdf_parse_mailbox(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  char buf[HUGE_STRING];
  char return_path[LONG_STRING];
  int count = 0, oldmsgcount = m->msg_count;
  int lines;
  time_t t;
  LOFF_T loc, tmploc;
  struct Email *e = NULL;
  struct stat sb;
  struct Progress progress;

  if (stat(m->path, &sb) == -1)
  {
    mutt_perror(m->path);
    return -1;
  }
  mutt_get_stat_timespec(&adata->atime, &sb, MUTT_STAT_ATIME);
  mutt_get_stat_timespec(&m->mtime, &sb, MUTT_STAT_MTIME);
  m->size = sb.st_size;

  buf[sizeof(buf) - 1] = '\0';

  if (!m->quiet)
  {
    char msgbuf[STRING];
    snprintf(msgbuf, sizeof(msgbuf), _("Reading %s..."), m->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, ReadInc, 0);
  }

  while (true)
  {
    if (!fgets(buf, sizeof(buf) - 1, adata->fp))
      break;

    if (SigInt == 1)
      break;

    if (mutt_str_strcmp(buf, MMDF_SEP) == 0)
    {
      loc = ftello(adata->fp);
      if (loc < 0)
        return -1;

      count++;
      if (!m->quiet)
        mutt_progress_update(&progress, count, (int) (loc / (m->size / 100 + 1)));

      if (m->msg_count == m->hdrmax)
        mx_alloc_memory(m);
      e = mutt_email_new();
      m->hdrs[m->msg_count] = e;
      e->offset = loc;
      e->index = m->msg_count;

      if (!fgets(buf, sizeof(buf) - 1, adata->fp))
      {
        /* TODO: memory leak??? */
        mutt_debug(1, "unexpected EOF\n");
        break;
      }

      return_path[0] = '\0';

      if (!is_from(buf, return_path, sizeof(return_path), &t))
      {
        if (fseeko(adata->fp, loc, SEEK_SET) != 0)
        {
          mutt_debug(1, "#1 fseek() failed\n");
          mutt_error(_("Mailbox is corrupt"));
          return -1;
        }
      }
      else
        e->received = t - mutt_date_local_tz(t);

      e->env = mutt_rfc822_read_header(adata->fp, e, false, false);

      loc = ftello(adata->fp);
      if (loc < 0)
        return -1;

      if (e->content->length > 0 && e->lines > 0)
      {
        tmploc = loc + e->content->length;

        if ((tmploc > 0) && (tmploc < m->size))
        {
          if (fseeko(adata->fp, tmploc, SEEK_SET) != 0 ||
              !fgets(buf, sizeof(buf) - 1, adata->fp) ||
              (mutt_str_strcmp(MMDF_SEP, buf) != 0))
          {
            if (fseeko(adata->fp, loc, SEEK_SET) != 0)
              mutt_debug(1, "#2 fseek() failed\n");
            e->content->length = -1;
          }
        }
        else
          e->content->length = -1;
      }
      else
        e->content->length = -1;

      if (e->content->length < 0)
      {
        lines = -1;
        do
        {
          loc = ftello(adata->fp);
          if (loc < 0)
            return -1;
          if (!fgets(buf, sizeof(buf) - 1, adata->fp))
            break;
          lines++;
        } while (mutt_str_strcmp(buf, MMDF_SEP) != 0);

        e->lines = lines;
        e->content->length = loc - e->content->offset;
      }

      if (!e->env->return_path && return_path[0])
        e->env->return_path = mutt_addr_parse_list(e->env->return_path, return_path);

      if (!e->env->from)
        e->env->from = mutt_addr_copy_list(e->env->return_path, false);

      m->msg_count++;
    }
    else
    {
      mutt_debug(1, "corrupt mailbox\n");
      mutt_error(_("Mailbox is corrupt"));
      return -1;
    }
  }

  if (m->msg_count > oldmsgcount)
    mx_update_context(ctx, m->msg_count - oldmsgcount);

  if (SigInt == 1)
  {
    SigInt = 0;
    return -2; /* action aborted */
  }

  return 0;
}

/**
 * mbox_parse_mailbox - Read a mailbox from disk
 * @param ctx Mailbox
 * @retval  0 Success
 * @retval -1 Error
 * @retval -2 Aborted
 *
 * Note that this function is also called when new mail is appended to the
 * currently open folder, and NOT just when the mailbox is initially read.
 *
 * NOTE: it is assumed that the mailbox being read has been locked before this
 * routine gets called.  Strange things could happen if it's not!
 */
static int mbox_parse_mailbox(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  struct stat sb;
  char buf[HUGE_STRING], return_path[STRING];
  struct Email *curhdr = NULL;
  time_t t;
  int count = 0, lines = 0;
  LOFF_T loc;
  struct Progress progress;

  /* Save information about the folder at the time we opened it. */
  if (stat(m->path, &sb) == -1)
  {
    mutt_perror(m->path);
    return -1;
  }

  m->size = sb.st_size;
  mutt_get_stat_timespec(&m->mtime, &sb, MUTT_STAT_MTIME);
  mutt_get_stat_timespec(&adata->atime, &sb, MUTT_STAT_ATIME);

  if (!m->readonly)
    m->readonly = access(m->path, W_OK) ? true : false;

  if (!m->quiet)
  {
    char msgbuf[STRING];
    snprintf(msgbuf, sizeof(msgbuf), _("Reading %s..."), m->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, ReadInc, 0);
  }

  if (!m->hdrs)
  {
    /* Allocate some memory to get started */
    m->hdrmax = m->msg_count;
    m->msg_count = 0;
    m->msg_unread = 0;
    m->vcount = 0;
    mx_alloc_memory(m);
  }

  loc = ftello(adata->fp);
  while ((fgets(buf, sizeof(buf), adata->fp)) && (SigInt != 1))
  {
    if (is_from(buf, return_path, sizeof(return_path), &t))
    {
      /* Save the Content-Length of the previous message */
      if (count > 0)
      {
        struct Email *e = m->hdrs[m->msg_count - 1];
        if (e->content->length < 0)
        {
          e->content->length = loc - e->content->offset - 1;
          if (e->content->length < 0)
            e->content->length = 0;
        }
        if (!e->lines)
          e->lines = lines ? lines - 1 : 0;
      }

      count++;

      if (!m->quiet)
      {
        mutt_progress_update(&progress, count,
                             (int) (ftello(adata->fp) / (m->size / 100 + 1)));
      }

      if (m->msg_count == m->hdrmax)
        mx_alloc_memory(m);

      m->hdrs[m->msg_count] = mutt_email_new();
      curhdr = m->hdrs[m->msg_count];
      curhdr->received = t - mutt_date_local_tz(t);
      curhdr->offset = loc;
      curhdr->index = m->msg_count;

      curhdr->env = mutt_rfc822_read_header(adata->fp, curhdr, false, false);

      /* if we know how long this message is, either just skip over the body,
       * or if we don't know how many lines there are, count them now (this will
       * save time by not having to search for the next message marker).
       */
      if (curhdr->content->length > 0)
      {
        LOFF_T tmploc;

        loc = ftello(adata->fp);

        /* The test below avoids a potential integer overflow if the
         * content-length is huge (thus necessarily invalid).
         */
        tmploc = (curhdr->content->length < m->size) ?
                     (loc + curhdr->content->length + 1) :
                     -1;

        if ((tmploc > 0) && (tmploc < m->size))
        {
          /* check to see if the content-length looks valid.  we expect to
           * to see a valid message separator at this point in the stream
           */
          if (fseeko(adata->fp, tmploc, SEEK_SET) != 0 ||
              !fgets(buf, sizeof(buf), adata->fp) ||
              !mutt_str_startswith(buf, "From ", CASE_MATCH))
          {
            mutt_debug(1, "bad content-length in message %d (cl=" OFF_T_FMT ")\n",
                       curhdr->index, curhdr->content->length);
            mutt_debug(1, "\tLINE: %s", buf);
            /* nope, return the previous position */
            if ((loc < 0) || (fseeko(adata->fp, loc, SEEK_SET) != 0))
            {
              mutt_debug(1, "#1 fseek() failed\n");
            }
            curhdr->content->length = -1;
          }
        }
        else if (tmploc != m->size)
        {
          /* content-length would put us past the end of the file, so it
           * must be wrong
           */
          curhdr->content->length = -1;
        }

        if (curhdr->content->length != -1)
        {
          /* good content-length.  check to see if we know how many lines
           * are in this message.
           */
          if (curhdr->lines == 0)
          {
            int cl = curhdr->content->length;

            /* count the number of lines in this message */
            if ((loc < 0) || (fseeko(adata->fp, loc, SEEK_SET) != 0))
              mutt_debug(1, "#2 fseek() failed\n");
            while (cl-- > 0)
            {
              if (fgetc(adata->fp) == '\n')
                curhdr->lines++;
            }
          }

          /* return to the offset of the next message separator */
          if (fseeko(adata->fp, tmploc, SEEK_SET) != 0)
            mutt_debug(1, "#3 fseek() failed\n");
        }
      }

      m->msg_count++;

      if (!curhdr->env->return_path && return_path[0])
      {
        curhdr->env->return_path =
            mutt_addr_parse_list(curhdr->env->return_path, return_path);
      }

      if (!curhdr->env->from)
        curhdr->env->from = mutt_addr_copy_list(curhdr->env->return_path, false);

      lines = 0;
    }
    else
      lines++;

    loc = ftello(adata->fp);
  }

  /* Only set the content-length of the previous message if we have read more
   * than one message during _this_ invocation.  If this routine is called
   * when new mail is received, we need to make sure not to clobber what
   * previously was the last message since the headers may be sorted.
   */
  if (count > 0)
  {
    struct Email *e = m->hdrs[m->msg_count - 1];
    if (e->content->length < 0)
    {
      e->content->length = ftello(adata->fp) - e->content->offset - 1;
      if (e->content->length < 0)
        e->content->length = 0;
    }

    if (!e->lines)
      e->lines = lines ? lines - 1 : 0;

    mx_update_context(ctx, count);
  }

  if (SigInt == 1)
  {
    SigInt = 0;
    return -2; /* action aborted */
  }

  return 0;
}

/**
 * reopen_mailbox - Close and reopen a mailbox
 * @param ctx        Mailbox
 * @param index_hint Current email
 * @retval >0 Success, e.g. #MUTT_REOPENED, #MUTT_NEW_MAIL
 * @retval -1 Error
 */
static int reopen_mailbox(struct Context *ctx, int *index_hint)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  bool (*cmp_headers)(const struct Email *, const struct Email *) = NULL;
  struct Email **old_hdrs = NULL;
  int old_msgcount;
  bool msg_mod = false;
  bool index_hint_set;
  int i, j;
  int rc = -1;

  /* silent operations */
  m->quiet = true;

  if (!m->quiet)
    mutt_message(_("Reopening mailbox..."));

  /* our heuristics require the old mailbox to be unsorted */
  if (Sort != SORT_ORDER)
  {
    short old_sort;

    old_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers(ctx, true);
    Sort = old_sort;
  }

  old_hdrs = NULL;
  old_msgcount = 0;

  /* simulate a close */
  mutt_hash_destroy(&m->id_hash);
  mutt_hash_destroy(&m->subj_hash);
  mutt_hash_destroy(&m->label_hash);
  mutt_clear_threads(ctx);
  FREE(&m->v2r);
  if (m->readonly)
  {
    for (i = 0; i < m->msg_count; i++)
      mutt_email_free(&(m->hdrs[i])); /* nothing to do! */
    FREE(&m->hdrs);
  }
  else
  {
    /* save the old headers */
    old_msgcount = m->msg_count;
    old_hdrs = m->hdrs;
    m->hdrs = NULL;
  }

  m->hdrmax = 0; /* force allocation of new headers */
  m->msg_count = 0;
  m->vcount = 0;
  ctx->vsize = 0;
  ctx->tagged = 0;
  m->msg_deleted = 0;
  m->msg_new = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->changed = false;
  m->id_hash = NULL;
  m->subj_hash = NULL;
  mutt_make_label_hash(m);

  switch (m->magic)
  {
    case MUTT_MBOX:
    case MUTT_MMDF:
      cmp_headers = mutt_email_cmp_strict;
      mutt_file_fclose(&adata->fp);
      adata->fp = mutt_file_fopen(m->path, "r");
      if (!adata->fp)
        rc = -1;
      else
        rc = ((m->magic == MUTT_MBOX) ? mbox_parse_mailbox : mmdf_parse_mailbox)(ctx);
      break;

    default:
      rc = -1;
      break;
  }

  if (rc == -1)
  {
    /* free the old headers */
    for (j = 0; j < old_msgcount; j++)
      mutt_email_free(&(old_hdrs[j]));
    FREE(&old_hdrs);

    m->quiet = false;
    return -1;
  }

  mutt_file_touch_atime(fileno(adata->fp));

  /* now try to recover the old flags */

  index_hint_set = (index_hint == NULL);

  if (!m->readonly)
  {
    for (i = 0; i < m->msg_count; i++)
    {
      bool found = false;

      /* some messages have been deleted, and new  messages have been
       * appended at the end; the heuristic is that old messages have then
       * "advanced" towards the beginning of the folder, so we begin the
       * search at index "i"
       */
      for (j = i; j < old_msgcount; j++)
      {
        if (!old_hdrs[j])
          continue;
        if (cmp_headers(m->hdrs[i], old_hdrs[j]))
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        for (j = 0; (j < i) && (j < old_msgcount); j++)
        {
          if (!old_hdrs[j])
            continue;
          if (cmp_headers(m->hdrs[i], old_hdrs[j]))
          {
            found = true;
            break;
          }
        }
      }

      if (found)
      {
        /* this is best done here */
        if (!index_hint_set && *index_hint == j)
          *index_hint = i;

        if (old_hdrs[j]->changed)
        {
          /* Only update the flags if the old header was changed;
           * otherwise, the header may have been modified externally,
           * and we don't want to lose _those_ changes
           */
          mutt_set_flag(ctx, m->hdrs[i], MUTT_FLAG, old_hdrs[j]->flagged);
          mutt_set_flag(ctx, m->hdrs[i], MUTT_REPLIED, old_hdrs[j]->replied);
          mutt_set_flag(ctx, m->hdrs[i], MUTT_OLD, old_hdrs[j]->old);
          mutt_set_flag(ctx, m->hdrs[i], MUTT_READ, old_hdrs[j]->read);
        }
        mutt_set_flag(ctx, m->hdrs[i], MUTT_DELETE, old_hdrs[j]->deleted);
        mutt_set_flag(ctx, m->hdrs[i], MUTT_PURGE, old_hdrs[j]->purge);
        mutt_set_flag(ctx, m->hdrs[i], MUTT_TAG, old_hdrs[j]->tagged);

        /* we don't need this header any more */
        mutt_email_free(&(old_hdrs[j]));
      }
    }

    /* free the remaining old headers */
    for (j = 0; j < old_msgcount; j++)
    {
      if (old_hdrs[j])
      {
        mutt_email_free(&(old_hdrs[j]));
        msg_mod = true;
      }
    }
    FREE(&old_hdrs);
  }

  m->quiet = false;

  return (m->changed || msg_mod) ? MUTT_REOPENED : MUTT_NEW_MAIL;
}

/**
 * mbox_has_new - Does the mailbox have new mail
 * @param m Mailbox
 * @retval true if the mailbox has at least 1 new messages (not old)
 * @retval false otherwise
 */
static bool mbox_has_new(struct Mailbox *m)
{
  for (int i = 0; i < m->msg_count; i++)
    if (!m->hdrs[i]->deleted && !m->hdrs[i]->read && !m->hdrs[i]->old)
      return true;
  return false;
}

/**
 * fseek_last_message - Find the last message in the file
 * @param f File to search
 * @retval  0 Success
 * @retval -1 No message found
 */
static int fseek_last_message(FILE *f)
{
  LOFF_T pos;
  char buffer[BUFSIZ + 9] = { 0 }; /* 7 for "\n\nFrom " */
  size_t bytes_read;

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
      if (mutt_str_startswith(buffer + i, "\n\nFrom ", CASE_MATCH))
      { /* found it - go to the beginning of the From */
        fseeko(f, pos + i + 2, SEEK_SET);
        return 0;
      }
    }
    bytes_read = BUFSIZ;
  }

  /* here we are at the beginning of the file */
  if (mutt_str_startswith(buffer, "From ", CASE_MATCH))
  {
    fseek(f, 0, SEEK_SET);
    return 0;
  }

  return -1;
}

/**
 * test_last_status_new - Is the last message new
 * @param f File to check
 * @retval true if the last message is new
 */
static bool test_last_status_new(FILE *f)
{
  struct Email *e = NULL;
  struct Envelope *tmp_envelope = NULL;
  bool result = false;

  if (fseek_last_message(f) == -1)
    return false;

  e = mutt_email_new();
  tmp_envelope = mutt_rfc822_read_header(f, e, false, false);
  if (!(e->read || e->old))
    result = true;

  mutt_env_free(&tmp_envelope);
  mutt_email_free(&e);

  return result;
}

/**
 * mbox_test_new_folder - Test if an mbox or mmdf mailbox has new mail
 * @param path Path to the mailbox
 * @retval bool true if the folder contains new mail
 */
bool mbox_test_new_folder(const char *path)
{
  FILE *f = NULL;
  bool rc = false;

  enum MailboxType magic = mx_path_probe(path, NULL);

  if ((magic != MUTT_MBOX) && (magic != MUTT_MMDF))
    return false;

  f = fopen(path, "rb");
  if (f)
  {
    rc = test_last_status_new(f);
    mutt_file_fclose(&f);
  }

  return rc;
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
  struct stat st2;

  if (!st)
  {
    if (stat(m->path, &st2) < 0)
      return;
    st = &st2;
  }

  utimebuf.actime = st->st_atime;
  utimebuf.modtime = st->st_mtime;

  /* When $mbox_check_recent is set, existing new mail is ignored, so do not
   * reset the atime to mtime-1 to signal new mail.
   */
  if (!MailCheckRecent && utimebuf.actime >= utimebuf.modtime && mbox_has_new(m))
  {
    utimebuf.actime = utimebuf.modtime - 1;
  }

  utime(m->path, &utimebuf);
}

/**
 * mbox_ac_find - Find a Account that matches a Mailbox path
 */
struct Account *mbox_ac_find(struct Account *a, const char *path)
{
  if (!a || (a->magic != MUTT_MBOX) || !path)
    return NULL;

  struct MailboxNode *np = STAILQ_FIRST(&a->mailboxes);
  if (!np)
    return NULL;

  if (mutt_str_strcmp(np->m->path, path) != 0)
    return NULL;

  return a;
}

/**
 * mbox_ac_add - Add a Mailbox to a Account
 */
int mbox_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m)
    return -1;

  if (m->magic != MUTT_MBOX)
    return -1;

  m->account = a;

  struct MailboxNode *np = mutt_mem_calloc(1, sizeof(*np));
  np->m = m;
  STAILQ_INSERT_TAIL(&a->mailboxes, np, entries);
  return 0;
}

/**
 * mbox_mbox_open - Implements MxOps::mbox_open()
 */
static int mbox_mbox_open(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  if (init_mailbox(m) != 0)
    return -1;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  adata->fp = fopen(m->path, "r");
  if (!adata->fp)
  {
    mutt_perror(m->path);
    return -1;
  }
  mutt_sig_block();
  if (mbox_lock_mailbox(m, false, true) == -1)
  {
    mutt_sig_unblock();
    return -1;
  }

  int rc;
  if (m->magic == MUTT_MBOX)
    rc = mbox_parse_mailbox(ctx);
  else if (m->magic == MUTT_MMDF)
    rc = mmdf_parse_mailbox(ctx);
  else
    rc = -1;
  mutt_file_touch_atime(fileno(adata->fp));

  mbox_unlock_mailbox(m);
  mutt_sig_unblock();
  return rc;
}

/**
 * mbox_mbox_open_append - Implements MxOps::mbox_open_append()
 */
static int mbox_mbox_open_append(struct Mailbox *m, int flags)
{
  if (!m)
    return -1;

  if (init_mailbox(m) != 0)
    return -1;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  adata->fp = mutt_file_fopen(m->path, (flags & MUTT_NEWFOLDER) ? "w" : "a");
  if (!adata->fp)
  {
    mutt_perror(m->path);
    return -1;
  }

  if (mbox_lock_mailbox(m, true, true) != false)
  {
    mutt_error(_("Couldn't lock %s"), m->path);
    mutt_file_fclose(&adata->fp);
    return -1;
  }

  fseek(adata->fp, 0, SEEK_END);

  return 0;
}

/**
 * mbox_mbox_check - Implements MxOps::mbox_check()
 * @param[in]  ctx        Mailbox
 * @param[out] index_hint Keep track of current index selection
 * @retval #MUTT_REOPENED  Mailbox has been reopened
 * @retval #MUTT_NEW_MAIL  New mail has arrived
 * @retval #MUTT_LOCKED    Couldn't lock the file
 * @retval 0               No change
 * @retval -1              Error
 */
static int mbox_mbox_check(struct Context *ctx, int *index_hint)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  if (!adata->fp && (mbox_mbox_open(ctx) < 0))
    return -1;

  struct stat st;
  bool unlock = false;
  bool modified = false;

  if (stat(m->path, &st) == 0)
  {
    if ((mutt_stat_timespec_compare(&st, MUTT_STAT_MTIME, &m->mtime) == 0) &&
        st.st_size == m->size)
    {
      return 0;
    }

    if (st.st_size == m->size)
    {
      /* the file was touched, but it is still the same length, so just exit */
      mutt_get_stat_timespec(&m->mtime, &st, MUTT_STAT_MTIME);
      return 0;
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
           * parse it: we'll get it on the next pass
           */
          return MUTT_LOCKED;
        }
        unlock = 1;
      }

      /* Check to make sure that the only change to the mailbox is that
       * message(s) were appended to this file.  My heuristic is that we should
       * see the message separator at *exactly* what used to be the end of the
       * folder.
       */
      char buffer[LONG_STRING];
      if (fseeko(adata->fp, m->size, SEEK_SET) != 0)
        mutt_debug(1, "#1 fseek() failed\n");
      if (fgets(buffer, sizeof(buffer), adata->fp))
      {
        if ((m->magic == MUTT_MBOX &&
             mutt_str_startswith(buffer, "From ", CASE_MATCH)) ||
            (m->magic == MUTT_MMDF && (mutt_str_strcmp(buffer, MMDF_SEP) == 0)))
        {
          if (fseeko(adata->fp, m->size, SEEK_SET) != 0)
            mutt_debug(1, "#2 fseek() failed\n");
          if (m->magic == MUTT_MBOX)
            mbox_parse_mailbox(ctx);
          else
            mmdf_parse_mailbox(ctx);

          /* Only unlock the folder if it was locked inside of this routine.
           * It may have been locked elsewhere, like in
           * mutt_checkpoint_mailbox().
           */

          if (unlock)
          {
            mbox_unlock_mailbox(m);
            mutt_sig_unblock();
          }

          return MUTT_NEW_MAIL; /* signal that new mail arrived */
        }
        else
          modified = true;
      }
      else
      {
        mutt_debug(1, "fgets returned NULL.\n");
        modified = true;
      }
    }
    else
      modified = true;
  }

  if (modified)
  {
    if (reopen_mailbox(ctx, index_hint) != -1)
    {
      if (unlock)
      {
        mbox_unlock_mailbox(m);
        mutt_sig_unblock();
      }
      return MUTT_REOPENED;
    }
  }

  /* fatal error */

  mbox_unlock_mailbox(m);
  mx_fastclose_mailbox(ctx);
  mutt_sig_unblock();
  mutt_error(_("Mailbox was corrupted"));
  return -1;
}

/**
 * mbox_mbox_sync - Implements MxOps::mbox_sync()
 */
static int mbox_mbox_sync(struct Context *ctx, int *index_hint)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  char tempfile[PATH_MAX];
  char buf[32];
  int i, j, save_sort = SORT_ORDER;
  int rc = -1;
  int need_sort = 0; /* flag to resort mailbox if new mail arrives */
  int first = -1;    /* first message to be written */
  LOFF_T offset;     /* location in mailbox to write changed messages */
  struct stat statbuf;
  struct MUpdate *new_offset = NULL;
  struct MUpdate *old_offset = NULL;
  FILE *fp = NULL;
  struct Progress progress;
  char msgbuf[PATH_MAX + 64];
  struct Mailbox *tmp = NULL;

  /* sort message by their position in the mailbox on disk */
  if (Sort != SORT_ORDER)
  {
    save_sort = Sort;
    Sort = SORT_ORDER;
    mutt_sort_headers(ctx, false);
    Sort = save_sort;
    need_sort = 1;
  }

  /* need to open the file for writing in such a way that it does not truncate
   * the file, so use read-write mode.
   */
  adata->fp = freopen(m->path, "r+", adata->fp);
  if (!adata->fp)
  {
    mx_fastclose_mailbox(ctx);
    mutt_error(_("Fatal error!  Could not reopen mailbox!"));
    return -1;
  }

  mutt_sig_block();

  if (mbox_lock_mailbox(m, true, true) == -1)
  {
    mutt_sig_unblock();
    mutt_error(_("Unable to lock mailbox"));
    goto bail;
  }

  /* Check to make sure that the file hasn't changed on disk */
  i = mbox_mbox_check(ctx, index_hint);
  if ((i == MUTT_NEW_MAIL) || (i == MUTT_REOPENED))
  {
    /* new mail arrived, or mailbox reopened */
    rc = i;
    goto bail;
  }
  else if (i < 0)
  {
    /* fatal error */
    return -1;
  }

  /* Create a temporary file to write the new version of the mailbox in. */
  mutt_mktemp(tempfile, sizeof(tempfile));
  i = open(tempfile, O_WRONLY | O_EXCL | O_CREAT, 0600);
  if ((i == -1) || !(fp = fdopen(i, "w")))
  {
    if (-1 != i)
    {
      close(i);
      unlink(tempfile);
    }
    mutt_error(_("Could not create temporary file"));
    goto bail;
  }

  /* find the first deleted/changed message.  we save a lot of time by only
   * rewriting the mailbox from the point where it has actually changed.
   */
  for (i = 0; (i < m->msg_count) && !m->hdrs[i]->deleted &&
              !m->hdrs[i]->changed && !m->hdrs[i]->attach_del;
       i++)
  {
  }
  if (i == m->msg_count)
  {
    /* this means ctx->changed or m->msg_deleted was set, but no
     * messages were found to be changed or deleted.  This should
     * never happen, is we presume it is a bug in neomutt.
     */
    mutt_error(
        _("sync: mbox modified, but no modified messages (report this bug)"));
    mutt_debug(1, "no modified messages.\n");
    unlink(tempfile);
    goto bail;
  }

  /* save the index of the first changed/deleted message */
  first = i;
  /* where to start overwriting */
  offset = m->hdrs[i]->offset;

  /* the offset stored in the header does not include the MMDF_SEP, so make
   * sure we seek to the correct location
   */
  if (m->magic == MUTT_MMDF)
    offset -= (sizeof(MMDF_SEP) - 1);

  /* allocate space for the new offsets */
  new_offset = mutt_mem_calloc(m->msg_count - first, sizeof(struct MUpdate));
  old_offset = mutt_mem_calloc(m->msg_count - first, sizeof(struct MUpdate));

  if (!m->quiet)
  {
    snprintf(msgbuf, sizeof(msgbuf), _("Writing %s..."), m->path);
    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG, WriteInc, m->msg_count);
  }

  for (i = first, j = 0; i < m->msg_count; i++)
  {
    if (!m->quiet)
      mutt_progress_update(&progress, i, (int) (ftello(adata->fp) / (m->size / 100 + 1)));
    /* back up some information which is needed to restore offsets when
     * something fails.
     */

    old_offset[i - first].valid = true;
    old_offset[i - first].hdr = m->hdrs[i]->offset;
    old_offset[i - first].body = m->hdrs[i]->content->offset;
    old_offset[i - first].lines = m->hdrs[i]->lines;
    old_offset[i - first].length = m->hdrs[i]->content->length;

    if (!m->hdrs[i]->deleted)
    {
      j++;

      if (m->magic == MUTT_MMDF)
      {
        if (fputs(MMDF_SEP, fp) == EOF)
        {
          mutt_perror(tempfile);
          unlink(tempfile);
          goto bail;
        }
      }

      /* save the new offset for this message.  we add `offset' because the
       * temporary file only contains saved message which are located after
       * `offset' in the real mailbox
       */
      new_offset[i - first].hdr = ftello(fp) + offset;

      if (mutt_copy_message_ctx(fp, ctx, m->hdrs[i], MUTT_CM_UPDATE,
                                CH_FROM | CH_UPDATE | CH_UPDATE_LEN) != 0)
      {
        mutt_perror(tempfile);
        unlink(tempfile);
        goto bail;
      }

      /* Since messages could have been deleted, the offsets stored in memory
       * will be wrong, so update what we can, which is the offset of this
       * message, and the offset of the body.  If this is a multipart message,
       * we just flush the in memory cache so that the message will be reparsed
       * if the user accesses it later.
       */
      new_offset[i - first].body = ftello(fp) - m->hdrs[i]->content->length + offset;
      mutt_body_free(&m->hdrs[i]->content->parts);

      switch (m->magic)
      {
        case MUTT_MMDF:
          if (fputs(MMDF_SEP, fp) == EOF)
          {
            mutt_perror(tempfile);
            unlink(tempfile);
            goto bail;
          }
          break;
        default:
          if (fputs("\n", fp) == EOF)
          {
            mutt_perror(tempfile);
            unlink(tempfile);
            goto bail;
          }
      }
    }
  }

  if (fclose(fp) != 0)
  {
    fp = NULL;
    mutt_debug(1, "mutt_file_fclose (&) returned non-zero.\n");
    unlink(tempfile);
    mutt_perror(tempfile);
    goto bail;
  }
  fp = NULL;

  /* Save the state of this folder. */
  if (stat(m->path, &statbuf) == -1)
  {
    mutt_perror(m->path);
    unlink(tempfile);
    goto bail;
  }

  fp = fopen(tempfile, "r");
  if (!fp)
  {
    mutt_sig_unblock();
    mx_fastclose_mailbox(ctx);
    mutt_debug(1, "unable to reopen temp copy of mailbox!\n");
    mutt_perror(tempfile);
    FREE(&new_offset);
    FREE(&old_offset);
    return -1;
  }

  if (fseeko(adata->fp, offset, SEEK_SET) != 0 || /* seek the append location */
      /* do a sanity check to make sure the mailbox looks ok */
      !fgets(buf, sizeof(buf), adata->fp) ||
      (m->magic == MUTT_MBOX && !mutt_str_startswith(buf, "From ", CASE_MATCH)) ||
      (m->magic == MUTT_MMDF && (mutt_str_strcmp(MMDF_SEP, buf) != 0)))
  {
    mutt_debug(1, "message not in expected position.\n");
    mutt_debug(1, "\tLINE: %s\n", buf);
    i = -1;
  }
  else
  {
    if (fseeko(adata->fp, offset, SEEK_SET) != 0) /* return to proper offset */
    {
      i = -1;
      mutt_debug(1, "fseek() failed\n");
    }
    else
    {
      /* copy the temp mailbox back into place starting at the first
       * change/deleted message
       */
      if (!m->quiet)
        mutt_message(_("Committing changes..."));
      i = mutt_file_copy_stream(fp, adata->fp);

      if (ferror(adata->fp))
        i = -1;
    }
    if (i == 0)
    {
      m->size = ftello(adata->fp); /* update the mailbox->size of the mailbox */
      if ((m->size < 0) || (ftruncate(fileno(adata->fp), m->size) != 0))
      {
        i = -1;
        mutt_debug(1, "ftruncate() failed\n");
      }
    }
  }

  mutt_file_fclose(&fp);
  fp = NULL;
  mbox_unlock_mailbox(m);

  if (mutt_file_fclose(&adata->fp) != 0 || i == -1)
  {
    /* error occurred while writing the mailbox back, so keep the temp copy
     * around
     */

    char savefile[PATH_MAX];

    snprintf(savefile, sizeof(savefile), "%s/neomutt.%s-%s-%u", NONULL(Tmpdir),
             NONULL(Username), NONULL(ShortHostname), (unsigned int) getpid());
    rename(tempfile, savefile);
    mutt_sig_unblock();
    mx_fastclose_mailbox(ctx);
    mutt_pretty_mailbox(savefile, sizeof(savefile));
    mutt_error(_("Write failed!  Saved partial mailbox to %s"), savefile);
    FREE(&new_offset);
    FREE(&old_offset);
    return -1;
  }

  /* Restore the previous access/modification times */
  mbox_reset_atime(m, &statbuf);

  /* reopen the mailbox in read-only mode */
  adata->fp = fopen(m->path, "r");
  if (!adata->fp)
  {
    unlink(tempfile);
    mutt_sig_unblock();
    mx_fastclose_mailbox(ctx);
    mutt_error(_("Fatal error!  Could not reopen mailbox!"));
    FREE(&new_offset);
    FREE(&old_offset);
    return -1;
  }

  /* update the offsets of the rewritten messages */
  for (i = first, j = first; i < m->msg_count; i++)
  {
    if (!m->hdrs[i]->deleted)
    {
      m->hdrs[i]->offset = new_offset[i - first].hdr;
      m->hdrs[i]->content->hdr_offset = new_offset[i - first].hdr;
      m->hdrs[i]->content->offset = new_offset[i - first].body;
      m->hdrs[i]->index = j++;
    }
  }
  FREE(&new_offset);
  FREE(&old_offset);
  unlink(tempfile); /* remove partial copy of the mailbox */
  mutt_sig_unblock();

  if (CheckMboxSize)
  {
    tmp = mutt_find_mailbox(m->path);
    if (tmp && !tmp->has_new)
      mutt_update_mailbox(tmp);
  }

  return 0; /* signal success */

bail: /* Come here in case of disaster */

  mutt_file_fclose(&fp);

  /* restore offsets, as far as they are valid */
  if (first >= 0 && old_offset)
  {
    for (i = first; (i < m->msg_count) && old_offset[i - first].valid; i++)
    {
      m->hdrs[i]->offset = old_offset[i - first].hdr;
      m->hdrs[i]->content->hdr_offset = old_offset[i - first].hdr;
      m->hdrs[i]->content->offset = old_offset[i - first].body;
      m->hdrs[i]->lines = old_offset[i - first].lines;
      m->hdrs[i]->content->length = old_offset[i - first].length;
    }
  }

  /* this is ok to call even if we haven't locked anything */
  mbox_unlock_mailbox(m);

  mutt_sig_unblock();
  FREE(&new_offset);
  FREE(&old_offset);

  adata->fp = freopen(m->path, "r", adata->fp);
  if (!adata->fp)
  {
    mutt_error(_("Could not reopen mailbox"));
    mx_fastclose_mailbox(ctx);
    return -1;
  }

  if (need_sort)
  {
    /* if the mailbox was reopened, the thread tree will be invalid so make
     * sure to start threading from scratch.  */
    mutt_sort_headers(ctx, (need_sort == MUTT_REOPENED));
  }

  return rc;
}

/**
 * mbox_mbox_close - Implements MxOps::mbox_close()
 */
static int mbox_mbox_close(struct Context *ctx)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  if (!adata->fp)
    return 0;

  if (adata->append)
  {
    mutt_file_unlock(fileno(adata->fp));
    mutt_sig_unblock();
  }

  mutt_file_fclose(&adata->fp);

  /* fix up the times so mailbox won't get confused */
  if (ctx->peekonly && (m->path[0] != '\0') &&
      (mutt_timespec_compare(&m->mtime, &adata->atime) > 0))
  {
#ifdef HAVE_UTIMENSAT
    struct timespec ts[2];
    ts[0] = adata->atime;
    ts[1] = m->mtime;
    utimensat(0, m->path, ts, 0);
#else
    struct utimbuf ut;
    ut.actime = adata->atime.tv_sec;
    ut.modtime = m->mtime.tv_sec;
    utime(m->path, &ut);
#endif
  }

  return 0;
}

/**
 * mbox_msg_open - Implements MxOps::msg_open()
 */
static int mbox_msg_open(struct Context *ctx, struct Message *msg, int msgno)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  msg->fp = adata->fp;

  return 0;
}

/**
 * mbox_msg_open_new - Implements MxOps::msg_open_new()
 */
static int mbox_msg_open_new(struct Context *ctx, struct Message *msg, struct Email *e)
{
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  struct MboxAccountData *adata = mbox_adata_get(m);
  if (!adata)
    return -1;

  msg->fp = adata->fp;
  return 0;
}

/**
 * mbox_msg_commit - Implements MxOps::msg_commit()
 */
static int mbox_msg_commit(struct Context *ctx, struct Message *msg)
{
  if (!msg)
    return -1;

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
 * mbox_msg_close - Implements MxOps::msg_close()
 */
static int mbox_msg_close(struct Mailbox *m, struct Message *msg)
{
  if (!msg)
    return -1;

  msg->fp = NULL;

  return 0;
}

/**
 * mbox_msg_padding_size - Bytes of padding between messages - Implements MxOps::msg_padding_size()
 * @param ctx Mailbox
 * @retval 1 Always
 */
static int mbox_msg_padding_size(struct Context *ctx)
{
  return 1;
}

/**
 * mbox_path_probe - Is this an mbox mailbox? - Implements MxOps::path_probe()
 */
enum MailboxType mbox_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (!st || !S_ISREG(st->st_mode))
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
     * detect magic for and thus open those files. */
    if ((ch != '\n') && (ch != '\r'))
    {
      ungetc(ch, fp);
      break;
    }
  }

  enum MailboxType magic = MUTT_UNKNOWN;
  char tmp[STRING];
  if (fgets(tmp, sizeof(tmp), fp))
  {
    if (mutt_str_startswith(tmp, "From ", CASE_MATCH))
      magic = MUTT_MBOX;
    else if (mutt_str_strcmp(tmp, MMDF_SEP) == 0)
      magic = MUTT_MMDF;
  }
  mutt_file_fclose(&fp);

  if (!CheckMboxSize)
  {
    /* need to restore the times here, the file was not really accessed,
     * only the type was accessed.  This is important, because detection
     * of "new mail" depends on those times set correctly.
     */
#ifdef HAVE_UTIMENSAT
    struct timespec ts[2];
    mutt_get_stat_timespec(&ts[0], &st, MUTT_STAT_ATIME);
    mutt_get_stat_timespec(&ts[1], &st, MUTT_STAT_MTIME);
    utimensat(0, path, ts, 0);
#else
    struct utimbuf times;
    times.actime = st->st_atime;
    times.modtime = st->st_mtime;
    utime(path, &times);
#endif
  }

  return magic;
}

/**
 * mbox_path_canon - Canonicalise a mailbox path - Implements MxOps::path_canon()
 */
int mbox_path_canon(char *buf, size_t buflen, const char *folder)
{
  if (!buf)
    return -1;

  if ((buf[0] == '+') || (buf[0] == '='))
  {
    if (!folder)
      return -1;

    buf[0] = '/';
    mutt_str_inline_replace(buf, buflen, 0, folder);
  }

  mutt_path_canon(buf, buflen, HomeDir);
  return 0;
}

/**
 * mbox_path_pretty - Implements MxOps::path_pretty()
 */
int mbox_path_pretty(char *buf, size_t buflen, const char *folder)
{
  if (!buf)
    return -1;

  if (mutt_path_abbr_folder(buf, buflen, folder))
    return 0;

  if (mutt_path_pretty(buf, buflen, HomeDir))
    return 0;

  return -1;
}

/**
 * mbox_path_parent - Implements MxOps::path_parent()
 */
int mbox_path_parent(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  if (mutt_path_parent(buf, buflen))
    return 0;

  if (buf[0] == '~')
    mutt_path_canon(buf, buflen, HomeDir);

  if (mutt_path_parent(buf, buflen))
    return 0;

  return -1;
}

/**
 * mmdf_msg_commit - Implements MxOps::msg_commit()
 */
static int mmdf_msg_commit(struct Context *ctx, struct Message *msg)
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
 * mmdf_msg_padding_size - Bytes of padding between messages - Implements MxOps::msg_padding_size()
 * @param ctx Mailbox
 * @retval 10 Always
 */
static int mmdf_msg_padding_size(struct Context *ctx)
{
  return 10;
}

// clang-format off
/**
 * struct mx_mbox_ops - Mbox mailbox - Implements ::MxOps
 */
struct MxOps mx_mbox_ops = {
  .magic            = MUTT_MBOX,
  .name             = "mbox",
  .ac_find          = mbox_ac_find,
  .ac_add           = mbox_ac_add,
  .mbox_open        = mbox_mbox_open,
  .mbox_open_append = mbox_mbox_open_append,
  .mbox_check       = mbox_mbox_check,
  .mbox_sync        = mbox_mbox_sync,
  .mbox_close       = mbox_mbox_close,
  .msg_open         = mbox_msg_open,
  .msg_open_new     = mbox_msg_open_new,
  .msg_commit       = mbox_msg_commit,
  .msg_close        = mbox_msg_close,
  .msg_padding_size = mbox_msg_padding_size,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = mbox_path_probe,
  .path_canon       = mbox_path_canon,
  .path_pretty      = mbox_path_pretty,
  .path_parent      = mbox_path_parent,
};

/**
 * struct mx_mmdf_ops - MMDF mailbox - Implements ::MxOps
 */
struct MxOps mx_mmdf_ops = {
  .magic            = MUTT_MMDF,
  .name             = "mmdf",
  .ac_find          = mbox_ac_find,
  .ac_add           = mbox_ac_add,
  .mbox_open        = mbox_mbox_open,
  .mbox_open_append = mbox_mbox_open_append,
  .mbox_check       = mbox_mbox_check,
  .mbox_sync        = mbox_mbox_sync,
  .mbox_close       = mbox_mbox_close,
  .msg_open         = mbox_msg_open,
  .msg_open_new     = mbox_msg_open_new,
  .msg_commit       = mmdf_msg_commit,
  .msg_close        = mbox_msg_close,
  .msg_padding_size = mmdf_msg_padding_size,
  .tags_edit        = NULL,
  .tags_commit      = NULL,
  .path_probe       = mbox_path_probe,
  .path_canon       = mbox_path_canon,
  .path_pretty      = mbox_path_pretty,
  .path_parent      = mbox_path_parent,
};
// clang-format on
