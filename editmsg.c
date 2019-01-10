/**
 * @file
 * Prepare an email to be edited
 *
 * @authors
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page editmsg Prepare an email to be edited
 *
 * Prepare an email to be edited
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "mutt.h"
#include "context.h"
#include "copy.h"
#include "curs_lib.h"
#include "globals.h"
#include "mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"

/**
 * ev_message - Edit an email or view it in an external editor
 * @param action Action to perform, e.g. #EVM_EDIT
 * @param m      Mailbox
 * @param e      Email
 * @retval  1 Message not modified
 * @retval  0 Message edited successfully
 * @retval -1 Error
 */
static int ev_message(enum EvMessage action, struct Mailbox *m, struct Email *e)
{
  char fname[PATH_MAX];
  char buf[STRING];
  int rc;
  struct stat sb;

  mutt_mktemp(fname, sizeof(fname));

  enum MailboxType omagic = MboxType;
  MboxType = MUTT_MBOX;
  struct Context *tmpctx = mx_mbox_open(NULL, fname, MUTT_NEWFOLDER);
  MboxType = omagic;

  if (!tmpctx)
  {
    mutt_error(_("could not create temporary folder: %s"), strerror(errno));
    return -1;
  }

  const int chflags =
      CH_NOLEN | ((m->magic == MUTT_MBOX || m->magic == MUTT_MMDF) ? 0 : CH_NOSTATUS);
  rc = mutt_append_message(tmpctx->mailbox, m, e, 0, chflags);
  int oerrno = errno;

  mx_mbox_close(&tmpctx);

  if (rc == -1)
  {
    mutt_error(_("could not write temporary mail folder: %s"), strerror(oerrno));
    goto bail;
  }

  rc = stat(fname, &sb);
  if (rc == -1)
  {
    mutt_error(_("Can't stat %s: %s"), fname, strerror(errno));
    goto bail;
  }

  /* The file the user is going to edit is not a real mbox, so we need to
   * truncate the last newline in the temp file, which is logically part of
   * the message separator, and not the body of the message.  If we fail to
   * remove it, the message will grow by one line each time the user edits
   * the message.
   */
  if ((sb.st_size != 0) && (truncate(fname, sb.st_size - 1) == -1))
  {
    mutt_error(_("could not truncate temporary mail folder: %s"), strerror(errno));
    goto bail;
  }

  if (action == EVM_VIEW)
  {
    /* remove write permissions */
    rc = mutt_file_chmod_rm_stat(fname, S_IWUSR | S_IWGRP | S_IWOTH, &sb);
    if (rc == -1)
    {
      mutt_debug(1, "Could not remove write permissions of %s: %s", fname, strerror(errno));
      /* Do not bail out here as we are checking afterwards if we should adopt
       * changes of the temporary file. */
    }
  }

  /* Do not reuse the stat sb here as it is outdated. */
  time_t mtime = mutt_file_decrease_mtime(fname, NULL);

  mutt_edit_file(NONULL(Editor), fname);

  rc = stat(fname, &sb);
  if (rc == -1)
  {
    mutt_error(_("Can't stat %s: %s"), fname, strerror(errno));
    goto bail;
  }

  if (sb.st_size == 0)
  {
    mutt_message(_("Message file is empty"));
    rc = 1;
    goto bail;
  }

  if ((action == EVM_EDIT) && (sb.st_mtime == mtime))
  {
    mutt_message(_("Message not modified"));
    rc = 1;
    goto bail;
  }

  if ((action == EVM_VIEW) && (sb.st_mtime != mtime))
  {
    mutt_message(_("Message of read-only mailbox modified! Ignoring changes."));
    rc = 1;
    goto bail;
  }

  if (action == EVM_VIEW)
  {
    /* stop processing here and skip right to the end */
    rc = 1;
    goto bail;
  }

  FILE *fp = fopen(fname, "r");
  if (!fp)
  {
    rc = -1;
    mutt_error(_("Can't open message file: %s"), strerror(errno));
    goto bail;
  }

  tmpctx = mx_mbox_open(m, NULL, MUTT_APPEND);
  if (!tmpctx)
  {
    rc = -1;
    /* L10N: %s is from strerror(errno) */
    mutt_error(_("Can't append to folder: %s"), strerror(errno));
    goto bail;
  }

  int of = 0;
  int cf = (((tmpctx->mailbox->magic == MUTT_MBOX) || (tmpctx->mailbox->magic == MUTT_MMDF)) ?
                0 :
                CH_NOSTATUS);

  if (fgets(buf, sizeof(buf), fp) && is_from(buf, NULL, 0, NULL))
  {
    if ((tmpctx->mailbox->magic == MUTT_MBOX) || (tmpctx->mailbox->magic == MUTT_MMDF))
      cf = CH_FROM | CH_FORCE_FROM;
  }
  else
    of = MUTT_ADD_FROM;

  /* XXX - we have to play games with the message flags to avoid
   * problematic behavior with maildir folders.  */

  bool o_read = e->read;
  bool o_old = e->old;
  e->read = false;
  e->old = false;
  struct Message *msg = mx_msg_open_new(tmpctx->mailbox, e, of);
  e->read = o_read;
  e->old = o_old;

  if (!msg)
  {
    mutt_error(_("Can't append to folder: %s"), strerror(errno));
    mx_mbox_close(&tmpctx);
    goto bail;
  }

  rc = mutt_copy_hdr(fp, msg->fp, 0, sb.st_size, CH_NOLEN | cf, NULL);
  if (rc == 0)
  {
    fputc('\n', msg->fp);
    mutt_file_copy_stream(fp, msg->fp);
  }

  rc = mx_msg_commit(tmpctx->mailbox, msg);
  mx_msg_close(tmpctx->mailbox, &msg);

  mx_mbox_close(&tmpctx);

bail:
  mutt_file_fclose(&fp);

  if (rc >= 0)
    unlink(fname);

  if (rc == 0)
  {
    mutt_set_flag(m, e, MUTT_DELETE, 1);
    mutt_set_flag(m, e, MUTT_PURGE, 1);
    mutt_set_flag(m, e, MUTT_READ, 1);

    if (DeleteUntag)
      mutt_set_flag(m, e, MUTT_TAG, 0);
  }
  else if (rc == -1)
    mutt_message(_("Error. Preserving temporary file: %s"), fname);

  return rc;
}

/**
 * mutt_ev_message - Edit or view a message
 * @param ctx    Mailbox Context
 * @param e      Email
 * @param action Action to perform, e.g. #EVM_EDIT
 * @retval 1  Message not modified
 * @retval 0  Message edited successfully
 * @retval -1 Error
 */
int mutt_ev_message(struct Context *ctx, struct Email *e, enum EvMessage action)
{
  if (e)
    return ev_message(action, ctx->mailbox, e);

  for (int i = 0; i < ctx->mailbox->msg_count; i++)
  {
    if (!message_is_tagged(ctx, i))
      continue;

    if (ev_message(action, ctx->mailbox, ctx->mailbox->emails[i]) == -1)
      return -1;
  }

  return 0;
}
