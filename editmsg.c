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
#include "email/email.h"
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
 * edit_or_view_one_message - Edit an email or view it in an external editor
 * @param edit true if the message should be editable. If false, changes
 *            to the message (in the editor) will be ignored.
 * @param ctx Context
 * @param cur Header of email
 * @retval 1  Message not modified
 * @retval 0  Message edited successfully
 * @retval -1 Error
 */
static int edit_or_view_one_message(bool edit, struct Context *ctx, struct Header *cur)
{
  char tmp[PATH_MAX];
  char buf[STRING];
  int omagic;
  int oerrno;
  int rc;

  bool o_read;
  bool o_old;

  int of, cf;

  struct Context tmpctx;
  struct Message *msg = NULL;

  FILE *fp = NULL;

  struct stat sb;
  time_t mtime = 0;

  mutt_mktemp(tmp, sizeof(tmp));

  omagic = MboxType;
  MboxType = MUTT_MBOX;

  rc = (mx_mbox_open(tmp, MUTT_NEWFOLDER, &tmpctx) == NULL) ? -1 : 0;

  MboxType = omagic;

  if (rc == -1)
  {
    mutt_error(_("could not create temporary folder: %s"), strerror(errno));
    return -1;
  }

  rc = mutt_append_message(
      &tmpctx, ctx, cur, 0,
      CH_NOLEN | ((ctx->magic == MUTT_MBOX || ctx->magic == MUTT_MMDF) ? 0 : CH_NOSTATUS));
  oerrno = errno;

  mx_mbox_close(&tmpctx, NULL);

  if (rc == -1)
  {
    mutt_error(_("could not write temporary mail folder: %s"), strerror(oerrno));
    goto bail;
  }

  rc = stat(tmp, &sb);
  if (rc == -1)
  {
    mutt_error(_("Can't stat %s: %s"), tmp, strerror(errno));
    goto bail;
  }

  /* The file the user is going to edit is not a real mbox, so we need to
   * truncate the last newline in the temp file, which is logically part of
   * the message separator, and not the body of the message.  If we fail to
   * remove it, the message will grow by one line each time the user edits
   * the message.
   */
  if (sb.st_size != 0 && truncate(tmp, sb.st_size - 1) == -1)
  {
    mutt_error(_("could not truncate temporary mail folder: %s"), strerror(errno));
    goto bail;
  }

  if (!edit)
  {
    /* remove write permissions */
    rc = mutt_file_chmod_rm_stat(tmp, S_IWUSR | S_IWGRP | S_IWOTH, &sb);
    if (rc == -1)
    {
      mutt_debug(1, "Could not remove write permissions of %s: %s", tmp, strerror(errno));
      /* Do not bail out here as we are checking afterwards if we should adopt
       * changes of the temporary file. */
    }
  }

  /* Do not reuse the stat sb here as it is outdated. */
  mtime = mutt_file_decrease_mtime(tmp, NULL);

  mutt_edit_file(NONULL(Editor), tmp);

  rc = stat(tmp, &sb);
  if (rc == -1)
  {
    mutt_error(_("Can't stat %s: %s"), tmp, strerror(errno));
    goto bail;
  }

  if (sb.st_size == 0)
  {
    mutt_message(_("Message file is empty!"));
    rc = 1;
    goto bail;
  }

  if (edit && sb.st_mtime == mtime)
  {
    mutt_message(_("Message not modified!"));
    rc = 1;
    goto bail;
  }

  if (!edit && sb.st_mtime != mtime)
  {
    mutt_message(_("Message of read-only mailbox modified! Ignoring changes."));
    rc = 1;
    goto bail;
  }

  if (!edit)
  {
    /* stop processing here and skip right to the end */
    rc = 1;
    goto bail;
  }

  fp = fopen(tmp, "r");
  if (!fp)
  {
    rc = -1;
    mutt_error(_("Can't open message file: %s"), strerror(errno));
    goto bail;
  }

  if (mx_mbox_open(ctx->path, MUTT_APPEND, &tmpctx) == NULL)
  {
    rc = -1;
    /* L10N: %s is from strerror(errno) */
    mutt_error(_("Can't append to folder: %s"), strerror(errno));
    goto bail;
  }

  of = 0;
  cf = ((tmpctx.magic == MUTT_MBOX || tmpctx.magic == MUTT_MMDF) ? 0 : CH_NOSTATUS);

  if (fgets(buf, sizeof(buf), fp) && is_from(buf, NULL, 0, NULL))
  {
    if (tmpctx.magic == MUTT_MBOX || tmpctx.magic == MUTT_MMDF)
      cf = CH_FROM | CH_FORCE_FROM;
  }
  else
    of = MUTT_ADD_FROM;

  /* XXX - we have to play games with the message flags to avoid
   * problematic behavior with maildir folders.  */

  o_read = cur->read;
  o_old = cur->old;
  cur->read = cur->old = false;
  msg = mx_msg_open_new(&tmpctx, cur, of);
  cur->read = o_read;
  cur->old = o_old;

  if (!msg)
  {
    mutt_error(_("Can't append to folder: %s"), strerror(errno));
    mx_mbox_close(&tmpctx, NULL);
    goto bail;
  }

  rc = mutt_copy_hdr(fp, msg->fp, 0, sb.st_size, CH_NOLEN | cf, NULL);
  if (rc == 0)
  {
    fputc('\n', msg->fp);
    mutt_file_copy_stream(fp, msg->fp);
  }

  rc = mx_msg_commit(&tmpctx, msg);
  mx_msg_close(&tmpctx, &msg);

  mx_mbox_close(&tmpctx, NULL);

bail:
  if (fp)
    mutt_file_fclose(&fp);

  if (rc >= 0)
    unlink(tmp);

  if (rc == 0)
  {
    mutt_set_flag(Context, cur, MUTT_DELETE, 1);
    mutt_set_flag(Context, cur, MUTT_PURGE, 1);
    mutt_set_flag(Context, cur, MUTT_READ, 1);

    if (DeleteUntag)
      mutt_set_flag(Context, cur, MUTT_TAG, 0);
  }
  else if (rc == -1)
    mutt_message(_("Error. Preserving temporary file: %s"), tmp);

  return rc;
}

/**
 * edit_or_view_message - Edit an email or view it in an external editor
 * @param edit true: Edit the email; false: view the email
 * @param ctx  Mailbox Context
 * @param hdr  Email Header
 * @retval 1  Message not modified
 * @retval 0  Message edited successfully
 * @retval -1 Error
 */
int edit_or_view_message(bool edit, struct Context *ctx, struct Header *hdr)
{
  if (hdr)
    return edit_or_view_one_message(edit, ctx, hdr);

  for (int i = 0; i < ctx->msgcount; i++)
  {
    if (!message_is_tagged(ctx, i))
      continue;

    if (edit_or_view_one_message(edit, ctx, ctx->hdrs[i]) == -1)
      return -1;
  }

  return 0;
}

/**
 * mutt_edit_message - Edit a message
 * @param ctx Mailbox Context
 * @param hdr Email Header
 * @retval 1  Message not modified
 * @retval 0  Message edited successfully
 * @retval -1 Error
 */
int mutt_edit_message(struct Context *ctx, struct Header *hdr)
{
  return edit_or_view_message(true, ctx, hdr); /* true means edit */
}

/**
 * mutt_view_message - Edit a message
 * @param ctx Mailbox Context
 * @param hdr Email Header
 * @retval 1  Message not modified
 * @retval 0  Message edited successfully
 * @retval -1 Error
 */
int mutt_view_message(struct Context *ctx, struct Header *hdr)
{
  return edit_or_view_message(false, ctx, hdr); /* false means only view */
}
