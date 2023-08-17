/**
 * @file
 * Maildir-specific Mailbox data
 *
 * @authors
 * Copyright (C) 2020-2023 Richard Russon <rich@flatcap.org>
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
 * @page maildir_mdata Maildir-specific Mailbox data
 *
 * Maildir-specific Mailbox data
 */

#include "config.h"
#include "mutt/lib.h"
#include "core/lib.h"
#include "mdata.h"
#ifdef USE_MONITOR
#include "monitor/lib.h"
#endif

struct Monitor;

/**
 * maildir_monitor - XXX
 */
void maildir_monitor(struct Monitor *mon, int wd, MonitorEvent me, void *wdata)
{
  // struct MaildirMboxData *mdata = wdata;
  mutt_debug(LL_DEBUG1, "QWQ maildir_monitor: wd %d\n", wd);
}

/**
 * maildir_mdata_free - Free the private Mailbox data - Implements Mailbox::mdata_free() - @ingroup mailbox_mdata_free
 */
void maildir_mdata_free(void **ptr)
{
  if (!ptr || !*ptr)
    return;

  struct MaildirMboxData *mdata = *ptr;

#ifdef USE_MONITOR
  monitor_remove_watch(NeoMutt->mon, mdata->wd_new);
#endif

  FREE(ptr);
}

/**
 * maildir_mdata_new - Create a new MaildirMboxData object
 * @param path Mailbox path
 * @retval ptr New MaildirMboxData struct
 */
struct MaildirMboxData *maildir_mdata_new(const char *path)
{
  struct MaildirMboxData *mdata = mutt_mem_calloc(1, sizeof(struct MaildirMboxData));
#ifdef USE_MONITOR
  struct Buffer *dir = buf_pool_get();
  buf_printf(dir, "%s/new", path);
  mdata->wd_new = monitor_watch_dir(NeoMutt->mon, buf_string(dir), maildir_monitor, mdata);
  buf_pool_release(&dir);
#endif
  return mdata;
}

/**
 * maildir_mdata_get - Get the private data for this Mailbox
 * @param m Mailbox
 * @retval ptr MaildirMboxData
 */
struct MaildirMboxData *maildir_mdata_get(struct Mailbox *m)
{
  if (m && (m->type == MUTT_MAILDIR))
    return m->mdata;

  return NULL;
}
