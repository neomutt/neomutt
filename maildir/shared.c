/**
 * @file
 * Maildir/MH local mailbox type
 *
 * @authors
 * Copyright (C) 1996-2002,2007,2009 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2005 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2010,2013 Michael R. Elkins <me@mutt.org>
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
 * @page maildir_shared Maildir/MH local mailbox type
 *
 * Maildir/MH local mailbox type
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <sys/stat.h>
#include "private.h"
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "lib.h"
#include "edata.h"
#include "mdata.h"
#include "mdemail.h"
#include "mx.h"
#include "protos.h"
#include "sequence.h"

/**
 * mh_umask - Create a umask from the mailbox directory
 * @param  m   Mailbox
 * @retval num Umask
 */
mode_t mh_umask(struct Mailbox *m)
{
  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (mdata && mdata->mh_umask)
    return mdata->mh_umask;

  struct stat st;
  if (stat(mailbox_path(m), &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "stat failed on %s\n", mailbox_path(m));
    return 077;
  }

  return 0777 & ~st.st_mode;
}

/**
 * maildir_move_to_mailbox - Copy the Maildir list to the Mailbox
 * @param[in]  m   Mailbox
 * @param[out] mda Maildir array to copy, then free
 * @retval num Number of new emails
 * @retval 0   Error
 */
int maildir_move_to_mailbox(struct Mailbox *m, struct MdEmailArray *mda)
{
  if (!m)
    return 0;

  int oldmsgcount = m->msg_count;

  struct MdEmail *md = NULL;
  struct MdEmail **mdp = NULL;
  ARRAY_FOREACH(mdp, mda)
  {
    md = *mdp;
    mutt_debug(LL_DEBUG2, "Considering %s\n", NONULL(md->canon_fname));
    if (!md->email)
      continue;

    mutt_debug(LL_DEBUG2, "Adding header structure. Flags: %s%s%s%s%s\n",
               md->email->flagged ? "f" : "", md->email->deleted ? "D" : "",
               md->email->replied ? "r" : "", md->email->old ? "O" : "",
               md->email->read ? "R" : "");
    if (m->msg_count == m->email_max)
      mx_alloc_memory(m);

    m->emails[m->msg_count] = md->email;
    m->emails[m->msg_count]->index = m->msg_count;
    mailbox_size_add(m, md->email);

    md->email = NULL;
    m->msg_count++;
  }

  int num = 0;
  if (m->msg_count > oldmsgcount)
    num = m->msg_count - oldmsgcount;

  maildirarray_clear(mda);
  return num;
}

/**
 * maildir_update_flags - Update the mailbox flags
 * @param m     Mailbox
 * @param e_old Old Email
 * @param e_new New Email
 * @retval true  The flags changed
 * @retval false Otherwise
 */
bool maildir_update_flags(struct Mailbox *m, struct Email *e_old, struct Email *e_new)
{
  if (!m)
    return false;

  /* save the global state here so we can reset it at the
   * end of list block if required.  */
  bool context_changed = m->changed;

  /* user didn't modify this message.  alter the flags to match the
   * current state on disk.  This may not actually do
   * anything. mutt_set_flag() will just ignore the call if the status
   * bits are already properly set, but it is still faster not to pass
   * through it */
  if (e_old->flagged != e_new->flagged)
    mutt_set_flag(m, e_old, MUTT_FLAG, e_new->flagged);
  if (e_old->replied != e_new->replied)
    mutt_set_flag(m, e_old, MUTT_REPLIED, e_new->replied);
  if (e_old->read != e_new->read)
    mutt_set_flag(m, e_old, MUTT_READ, e_new->read);
  if (e_old->old != e_new->old)
    mutt_set_flag(m, e_old, MUTT_OLD, e_new->old);

  /* mutt_set_flag() will set this, but we don't need to
   * sync the changes we made because we just updated the
   * context to match the current on-disk state of the
   * message.  */
  bool header_changed = e_old->changed;
  e_old->changed = false;

  /* if the mailbox was not modified before we made these
   * changes, unset the changed flag since nothing needs to
   * be synchronized.  */
  if (!context_changed)
    m->changed = false;

  return header_changed;
}
