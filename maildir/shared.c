/**
 * @file
 * Maildir shared functions
 *
 * @authors
 * Copyright (C) 2024 Richard Russon <rich@flatcap.org>
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
 * @page maildir_shared Maildir shared functions
 *
 * Maildir shared functions
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "mutt/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "mutt.h"
#include "edata.h"
#include "mdata.h"

/**
 * maildir_umask - Create a umask from the mailbox directory
 * @param  m   Mailbox
 * @retval num Umask
 */
mode_t maildir_umask(struct Mailbox *m)
{
  struct MaildirMboxData *mdata = maildir_mdata_get(m);
  if (mdata && mdata->umask)
    return mdata->umask;

  struct stat st = { 0 };
  if (stat(mailbox_path(m), &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "stat failed on %s\n", mailbox_path(m));
    return 077;
  }

  return 0777 & ~st.st_mode;
}

/**
 * maildir_canon_filename - Generate the canonical filename for a Maildir folder
 * @param dest   Buffer for the result
 * @param src    Buffer containing source filename
 *
 * @note         maildir filename is defined as: \<base filename\>:2,\<flags\>
 *               but \<base filename\> may contain additional comma separated
 *               fields. Additionally, `:` may be replaced as the field
 *               delimiter by a user defined alternative.
 */
void maildir_canon_filename(struct Buffer *dest, const char *src)
{
  if (!dest || !src)
    return;

  const char *t = strrchr(src, '/');
  if (t)
    src = t + 1;

  buf_strcpy(dest, src);

  const char c_maildir_field_delimiter = *cc_maildir_field_delimiter();

  char searchable_bytes[8] = { 0 };
  snprintf(searchable_bytes, sizeof(searchable_bytes), ",%c", c_maildir_field_delimiter);
  char *u = strpbrk(dest->data, searchable_bytes);

  if (u)
  {
    *u = '\0';
    dest->dptr = u;
  }
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
    mutt_set_flag(m, e_old, MUTT_FLAG, e_new->flagged, true);
  if (e_old->replied != e_new->replied)
    mutt_set_flag(m, e_old, MUTT_REPLIED, e_new->replied, true);
  if (e_old->read != e_new->read)
    mutt_set_flag(m, e_old, MUTT_READ, e_new->read, true);
  if (e_old->old != e_new->old)
    mutt_set_flag(m, e_old, MUTT_OLD, e_new->old, true);

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

/**
 * maildir_merge_flags - Merge external flag changes into a locally modified Email
 * @param m     Mailbox
 * @param e_old Email with unsynced local changes
 * @param e_new Email parsed from the file currently on disk
 * @retval true  Flags were changed
 * @retval false No change
 *
 * Another program changed the file's flags while we had our own unsynced
 * changes.  Each flag that we did not change since we last saw the file takes
 * the on-disk value; a flag we did change keeps our value, so the next sync
 * writes our changes as a delta over theirs rather than replacing them.
 */
bool maildir_merge_flags(struct Mailbox *m, struct Email *e_old, struct Email *e_new)
{
  if (!m || !e_old || !e_new)
    return false;

  struct MaildirEmailData *edata = maildir_edata_get(e_old);
  if (!edata)
    return false;

  bool changed = false;

  if ((e_new->flagged != edata->disk_flagged) && (e_old->flagged == edata->disk_flagged))
  {
    mutt_set_flag(m, e_old, MUTT_FLAG, e_new->flagged, true);
    changed = true;
  }
  if ((e_new->replied != edata->disk_replied) && (e_old->replied == edata->disk_replied))
  {
    mutt_set_flag(m, e_old, MUTT_REPLIED, e_new->replied, true);
    changed = true;
  }
  if ((e_new->read != edata->disk_read) && (e_old->read == edata->disk_read))
  {
    mutt_set_flag(m, e_old, MUTT_READ, e_new->read, true);
    changed = true;
  }
  if ((e_new->old != edata->disk_old) && (e_old->old == edata->disk_old))
  {
    mutt_set_flag(m, e_old, MUTT_OLD, e_new->old, true);
    changed = true;
  }

  return changed;
}
