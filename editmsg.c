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
 * @page neo_editmsg Prepare an email to be edited
 *
 * Prepare an email to be edited
 */

#include "config.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "copy.h"
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
  char buf[256];
  int rc;
  FILE *fp = NULL;
  struct stat sb;
  bool old_append = m->append;

  struct Buffer *fname = mutt_buffer_pool_get();
  mutt_buffer_mktemp(fname);

  // Temporarily force $mbox_type to be MUTT_MBOX
  const unsigned char c_mbox_type = cs_subset_enum(NeoMutt->sub, "mbox_type");
  cs_subset_str_native_set(NeoMutt->sub, "mbox_type", MUTT_MBOX, NULL);

  struct Mailbox *m_fname = mx_path_resolve(mutt_buffer_string(fname));
  if (!mx_mbox_open(m_fname, MUTT_NEWFOLDER))
  {
    mutt_error(_("could not create temporary folder: %s"), strerror(errno));
    mutt_buffer_pool_release(&fname);
    mailbox_free(&m_fname);
    return -1;
  }

  cs_subset_str_native_set(NeoMutt->sub, "mbox_type", c_mbox_type, NULL);

  const CopyHeaderFlags chflags =
      CH_NOLEN | (((m->type == MUTT_MBOX) || (m->type == MUTT_MMDF)) ? CH_NO_FLAGS : CH_NOSTATUS);
  rc = mutt_append_message(m_fname, m, e, NULL, MUTT_CM_NO_FLAGS, chflags);
  int oerrno = errno;

  mx_mbox_close(&m_fname);

  if (rc == -1)
  {
    mutt_error(_("could not write temporary mail folder: %s"), strerror(oerrno));
    goto bail;
  }

  rc = stat(mutt_buffer_string(fname), &sb);
  if (rc == -1)
  {
    mutt_error(_("Can't stat %s: %s"), mutt_buffer_string(fname), strerror(errno));
    goto bail;
  }

  /* The file the user is going to edit is not a real mbox, so we need to
   * truncate the last newline in the temp file, which is logically part of
   * the message separator, and not the body of the message.  If we fail to
   * remove it, the message will grow by one line each time the user edits
   * the message.  */
  if ((sb.st_size != 0) && (truncate(mutt_buffer_string(fname), sb.st_size - 1) == -1))
  {
    rc = -1;
    mutt_error(_("could not truncate temporary mail folder: %s"), strerror(errno));
    goto bail;
  }

  if (action == EVM_VIEW)
  {
    /* remove write permissions */
    rc = mutt_file_chmod_rm_stat(mutt_buffer_string(fname),
                                 S_IWUSR | S_IWGRP | S_IWOTH, &sb);
    if (rc == -1)
    {
      mutt_debug(LL_DEBUG1, "Could not remove write permissions of %s: %s",
                 mutt_buffer_string(fname), strerror(errno));
      /* Do not bail out here as we are checking afterwards if we should adopt
       * changes of the temporary file. */
    }
  }

  /* re-stat after the truncate, to avoid false "modified" bugs */
  rc = stat(mutt_buffer_string(fname), &sb);
  if (rc == -1)
  {
    mutt_error(_("Can't stat %s: %s"), mutt_buffer_string(fname), strerror(errno));
    goto bail;
  }

  /* Do not reuse the stat sb here as it is outdated. */
  time_t mtime = mutt_file_decrease_mtime(mutt_buffer_string(fname), NULL);
  if (mtime == (time_t) -1)
  {
    rc = -1;
    mutt_perror(mutt_buffer_string(fname));
    goto bail;
  }

  const char *const c_editor = cs_subset_string(NeoMutt->sub, "editor");
  mutt_edit_file(NONULL(c_editor), mutt_buffer_string(fname));

  rc = stat(mutt_buffer_string(fname), &sb);
  if (rc == -1)
  {
    mutt_error(_("Can't stat %s: %s"), mutt_buffer_string(fname), strerror(errno));
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

  fp = fopen(mutt_buffer_string(fname), "r");
  if (!fp)
  {
    rc = -1;
    mutt_error(_("Can't open message file: %s"), strerror(errno));
    goto bail;
  }

  if (!mx_mbox_open(m, MUTT_APPEND | MUTT_QUIET))
  {
    rc = -1;
    /* L10N: %s is from strerror(errno) */
    mutt_error(_("Can't append to folder: %s"), strerror(errno));
    goto bail;
  }
  MsgOpenFlags of = MUTT_MSG_NO_FLAGS;
  CopyHeaderFlags cf =
      (((m->type == MUTT_MBOX) || (m->type == MUTT_MMDF)) ? CH_NO_FLAGS : CH_NOSTATUS);

  if (fgets(buf, sizeof(buf), fp) && is_from(buf, NULL, 0, NULL))
  {
    if ((m->type == MUTT_MBOX) || (m->type == MUTT_MMDF))
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
  struct Message *msg = mx_msg_open_new(m, e, of);
  e->read = o_read;
  e->old = o_old;

  if (!msg)
  {
    rc = -1;
    mutt_error(_("Can't append to folder: %s"), strerror(errno));
    mx_mbox_close(&m);
    goto bail;
  }

  rc = mutt_copy_hdr(fp, msg->fp, 0, sb.st_size, CH_NOLEN | cf, NULL, 0);
  if (rc == 0)
  {
    fputc('\n', msg->fp);
    mutt_file_copy_stream(fp, msg->fp);
  }

  rc = mx_msg_commit(m, msg);
  mx_msg_close(m, &msg);

  mx_mbox_close(&m);

bail:
  mutt_file_fclose(&fp);

  if (rc >= 0)
    unlink(mutt_buffer_string(fname));

  if (rc == 0)
  {
    mutt_set_flag(m, e, MUTT_DELETE, true);
    mutt_set_flag(m, e, MUTT_PURGE, true);
    mutt_set_flag(m, e, MUTT_READ, true);

    const bool c_delete_untag = cs_subset_bool(NeoMutt->sub, "delete_untag");
    if (c_delete_untag)
      mutt_set_flag(m, e, MUTT_TAG, false);
  }
  else if (rc == -1)
    mutt_message(_("Error. Preserving temporary file: %s"), mutt_buffer_string(fname));

  if (m)
    m->append = old_append;

  mutt_buffer_pool_release(&fname);
  return rc;
}

/**
 * mutt_ev_message - Edit or view a message
 * @param m      Mailbox
 * @param el     List of Emails
 * @param action Action to perform, e.g. #EVM_EDIT
 * @retval 1  Message not modified
 * @retval 0  Message edited successfully
 * @retval -1 Error
 */
int mutt_ev_message(struct Mailbox *m, struct EmailList *el, enum EvMessage action)
{
  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    if (ev_message(action, m, en->email) == -1)
      return -1;
  }

  return 0;
}
