/**
 * @file
 * Mailbox multiplexor
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2016-2018 Richard Russon <rich@flatcap.org>
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
 * @page mx Mailbox multiplexor
 *
 * Mailbox multiplexor
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "email/lib.h"
#include "mutt.h"
#include "mx.h"
#include "account.h"
#include "alias.h"
#include "context.h"
#include "copy.h"
#include "globals.h"
#include "hook.h"
#include "keymap.h"
#include "mailbox.h"
#include "maildir/lib.h"
#include "mbox/mbox.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_thread.h"
#include "muttlib.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "pattern.h"
#include "protos.h"
#include "score.h"
#include "sort.h"
#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif
#ifdef USE_COMPRESSED
#include "compress.h"
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_POP
#include "pop/pop.h"
#endif
#ifdef USE_NNTP
#include "nntp/nntp.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/mutt_notmuch.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/* These Config Variables are only used in mx.c */
unsigned char C_CatchupNewsgroup; ///< Config: (nntp) Mark all articles as read when leaving a newsgroup
bool C_KeepFlagged; ///< Config: Don't move flagged messages from #C_Spoolfile to #C_Mbox
short C_MboxType; ///< Config: Default type for creating new mailboxes
unsigned char C_Move; ///< Config: Move emails from #C_Spoolfile to #C_Mbox when read
char *C_Trash;        ///< Config: Folder to put deleted emails

/**
 * mx_ops - All the Mailbox backends
 */
static const struct MxOps *mx_ops[] = {
/* These mailboxes can be recognised by their Url scheme */
#ifdef USE_IMAP
  &MxImapOps,
#endif
#ifdef USE_NOTMUCH
  &MxNotmuchOps,
#endif
#ifdef USE_POP
  &MxPopOps,
#endif
#ifdef USE_NNTP
  &MxNntpOps,
#endif

  /* Local mailboxes */
  &MxMaildirOps,
  &MxMboxOps,
  &MxMhOps,
  &MxMmdfOps,

/* If everything else fails... */
#ifdef USE_COMPRESSED
  &MxCompOps,
#endif
  NULL,
};

/**
 * mx_get_ops - Get mailbox operations
 * @param magic Mailbox magic number
 * @retval ptr  Mailbox function
 * @retval NULL Error
 */
const struct MxOps *mx_get_ops(enum MailboxType magic)
{
  for (const struct MxOps **ops = mx_ops; *ops; ops++)
    if ((*ops)->magic == magic)
      return *ops;

  return NULL;
}

/**
 * mutt_is_spool - Is this the spoolfile?
 * @param str Name to check
 * @retval true It is the spoolfile
 */
static bool mutt_is_spool(const char *str)
{
  return mutt_str_strcmp(C_Spoolfile, str) == 0;
}

/**
 * mx_access - Wrapper for access, checks permissions on a given mailbox
 * @param path  Path of mailbox
 * @param flags Flags, e.g. W_OK
 * @retval  0 Success, allowed
 * @retval <0 Failure, not allowed
 *
 * We may be interested in using ACL-style flags at some point, currently we
 * use the normal access() flags.
 */
int mx_access(const char *path, int flags)
{
#ifdef USE_IMAP
  if (imap_path_probe(path, NULL) == MUTT_IMAP)
    return imap_access(path);
#endif

  return access(path, flags);
}

/**
 * mx_open_mailbox_append - Open a mailbox for appending
 * @param m     Mailbox
 * @param flags Flags, see #OpenMailboxFlags
 * @retval  0 Success
 * @retval -1 Failure
 */
static int mx_open_mailbox_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!m)
    return -1;

  struct stat sb;

  m->append = true;
  if ((m->magic == MUTT_UNKNOWN) || (m->magic == MUTT_MAILBOX_ERROR))
  {
    m->magic = mx_path_probe(mutt_b2s(m->pathbuf), NULL);

    if (m->magic == MUTT_UNKNOWN)
    {
      if (flags & (MUTT_APPEND | MUTT_NEWFOLDER))
      {
        m->magic = MUTT_MAILBOX_ERROR;
      }
      else
      {
        mutt_error(_("%s is not a mailbox"), mutt_b2s(m->pathbuf));
        return -1;
      }
    }

    if (m->magic == MUTT_MAILBOX_ERROR)
    {
      if (stat(mutt_b2s(m->pathbuf), &sb) == -1)
      {
        if (errno == ENOENT)
        {
#ifdef USE_COMPRESSED
          if (mutt_comp_can_append(m))
            m->magic = MUTT_COMPRESSED;
          else
#endif
            m->magic = C_MboxType;
          flags |= MUTT_APPENDNEW;
        }
        else
        {
          mutt_perror(mutt_b2s(m->pathbuf));
          return -1;
        }
      }
      else
        return -1;
    }

    m->mx_ops = mx_get_ops(m->magic);
  }

  if (!m->mx_ops || !m->mx_ops->mbox_open_append)
    return -1;

  int rc = m->mx_ops->mbox_open_append(m, flags);
  m->opened++;
  return rc;
}

/**
 * mx_mbox_open - Open a mailbox and parse it
 * @param m     Mailbox to open
 * @param flags Flags, see #OpenMailboxFlags
 * @retval ptr  Mailbox context
 * @retval NULL Error
 */
struct Context *mx_mbox_open(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!m)
    return NULL;

  struct Context *ctx = mutt_mem_calloc(1, sizeof(*ctx));
  ctx->mailbox = m;

  if ((m->magic == MUTT_UNKNOWN) && (flags & (MUTT_NEWFOLDER | MUTT_APPEND)))
  {
    m->magic = C_MboxType;
    m->mx_ops = mx_get_ops(m->magic);
  }

  if (!m->account)
  {
    struct Account *a = mx_ac_find(m);
    bool new_account = false;
    if (!a)
    {
      a = account_new();
      a->magic = m->magic;
      new_account = true;
    }
    if (mx_ac_add(a, m) < 0)
    {
      ctx_free(&ctx);
      if (new_account)
      {
        FREE(&a);
      }
      return NULL;
    }
    if (new_account)
    {
      TAILQ_INSERT_TAIL(&AllAccounts, a, entries);
    }
  }

  ctx->msgnotreadyet = -1;
  ctx->collapsed = false;

  m->size = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->rights = MUTT_ACL_ALL;

  if (flags & MUTT_QUIET)
    m->quiet = true;
  if (flags & MUTT_READONLY)
    m->readonly = true;
  if (flags & MUTT_PEEK)
    m->peekonly = true;

  if (flags & (MUTT_APPEND | MUTT_NEWFOLDER))
  {
    if (mx_open_mailbox_append(ctx->mailbox, flags) != 0)
    {
      mx_fastclose_mailbox(m);
      ctx_free(&ctx);
      return NULL;
    }
    return ctx;
  }

  if (m->magic == MUTT_UNKNOWN)
  {
    m->magic = mx_path_probe(mutt_b2s(m->pathbuf), NULL);
    m->mx_ops = mx_get_ops(m->magic);
  }

  if ((m->magic == MUTT_UNKNOWN) || (m->magic == MUTT_MAILBOX_ERROR) || !m->mx_ops)
  {
    if (m->magic == MUTT_MAILBOX_ERROR)
      mutt_perror(mutt_b2s(m->pathbuf));
    else if ((m->magic == MUTT_UNKNOWN) || !m->mx_ops)
      mutt_error(_("%s is not a mailbox"), mutt_b2s(m->pathbuf));

    mx_fastclose_mailbox(m);
    ctx_free(&ctx);
    return NULL;
  }

  mutt_make_label_hash(m);

  /* if the user has a 'push' command in their .neomuttrc, or in a folder-hook,
   * it will cause the progress messages not to be displayed because
   * mutt_refresh() will think we are in the middle of a macro.  so set a
   * flag to indicate that we should really refresh the screen.  */
  OptForceRefresh = true;

  if (!m->quiet)
    mutt_message(_("Reading %s..."), mutt_b2s(m->pathbuf));

  int rc = m->mx_ops->mbox_open(ctx->mailbox);
  m->opened++;
  if (rc == 0)
    ctx_update(ctx);

  if ((rc == 0) || (rc == -2))
  {
    if ((flags & MUTT_NOSORT) == 0)
    {
      /* avoid unnecessary work since the mailbox is completely unthreaded
       * to begin with */
      OptSortSubthreads = false;
      OptNeedRescore = false;
      mutt_sort_headers(ctx, true);
    }
    if (!m->quiet)
      mutt_clear_error();
    if (rc == -2)
      mutt_error(_("Reading from %s interrupted..."), mutt_b2s(m->pathbuf));
  }
  else
  {
    mx_fastclose_mailbox(m);
    ctx_free(&ctx);
    return NULL;
  }

  OptForceRefresh = false;
  m->notify2 = ctx_mailbox_changed;
  m->ndata = ctx;

  return ctx;
}

/**
 * mx_fastclose_mailbox - free up memory associated with the Mailbox
 * @param m Mailbox
 */
void mx_fastclose_mailbox(struct Mailbox *m)
{
  if (!m)
    return;

  m->opened--;
  if (m->opened != 0)
    return;

  /* never announce that a mailbox we've just left has new mail. #3290
   * TODO: really belongs in mx_mbox_close, but this is a nice hook point */
  if (!m->peekonly)
    mutt_mailbox_set_notified(m);

  if (m->mx_ops)
    m->mx_ops->mbox_close(m);

  mutt_mailbox_changed(m, MBN_CLOSED);
  m->notify2 = NULL;

  mutt_hash_free(&m->subj_hash);
  mutt_hash_free(&m->id_hash);
  mutt_hash_free(&m->label_hash);

  if (m->emails)
  {
    for (int i = 0; i < m->msg_count; i++)
      mutt_email_free(&m->emails[i]);
    FREE(&m->emails);
  }
  FREE(&m->v2r);
}

/**
 * sync_mailbox - save changes to disk
 * @param m          Mailbox
 * @param index_hint Current email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int sync_mailbox(struct Mailbox *m, int *index_hint)
{
  if (!m || !m->mx_ops || !m->mx_ops->mbox_sync)
    return -1;

  if (!m->quiet)
  {
    /* L10N: Displayed before/as a mailbox is being synced */
    mutt_message(_("Writing %s..."), mutt_b2s(m->pathbuf));
  }

  int rc = m->mx_ops->mbox_sync(m, index_hint);
  if ((rc != 0) && !m->quiet)
  {
    /* L10N: Displayed if a mailbox sync fails */
    mutt_error(_("Unable to write %s"), mutt_b2s(m->pathbuf));
  }

  return rc;
}

/**
 * trash_append - move deleted mails to the trash folder
 * @param m Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
static int trash_append(struct Mailbox *m)
{
  if (!m)
    return -1;

  struct stat st, stc;
  int opt_confappend, rc;

  if (!C_Trash || (m->msg_deleted == 0) || ((m->magic == MUTT_MAILDIR) && C_MaildirTrash))
  {
    return 0;
  }

  int delmsgcount = 0;
  int first_del = -1;
  for (int i = 0; i < m->msg_count; i++)
  {
    if (m->emails[i]->deleted && (!m->emails[i]->purge))
    {
      if (first_del < 0)
        first_del = i;
      delmsgcount++;
    }
  }

  if (delmsgcount == 0)
    return 0; /* nothing to be done */

  /* avoid the "append messages" prompt */
  opt_confappend = C_Confirmappend;
  if (opt_confappend)
    C_Confirmappend = false;
  rc = mutt_save_confirm(C_Trash, &st);
  if (opt_confappend)
    C_Confirmappend = true;
  if (rc != 0)
  {
    /* L10N: Although we know the precise number of messages, we do not show it to the user.
       So feel free to use a "generic plural" as plural translation if your language has one. */
    mutt_error(ngettext("message not deleted", "messages not deleted", delmsgcount));
    return -1;
  }

  if ((lstat(mutt_b2s(m->pathbuf), &stc) == 0) && (stc.st_ino == st.st_ino) &&
      (stc.st_dev == st.st_dev) && (stc.st_rdev == st.st_rdev))
  {
    return 0; /* we are in the trash folder: simple sync */
  }

#ifdef USE_IMAP
  if ((m->magic == MUTT_IMAP) && (imap_path_probe(C_Trash, NULL) == MUTT_IMAP))
  {
    if (imap_fast_trash(m, C_Trash) == 0)
      return 0;
  }
#endif

  struct Mailbox *m_trash = mx_path_resolve(C_Trash);
  struct Context *ctx_trash = mx_mbox_open(m_trash, MUTT_OPEN_NO_FLAGS);
  if (ctx_trash)
  {
    bool old_append = m_trash->append;
    m_trash->append = true;

    /* continue from initial scan above */
    for (int i = first_del; i < m->msg_count; i++)
    {
      if (m->emails[i]->deleted && (!m->emails[i]->purge))
      {
        if (mutt_append_message(ctx_trash->mailbox, m, m->emails[i],
                                MUTT_CM_NO_FLAGS, CH_NO_FLAGS) == -1)
        {
          m_trash->append = old_append;
          mx_mbox_close(&ctx_trash);
          return -1;
        }
      }
    }

    m_trash->append = old_append;
    mx_mbox_close(&ctx_trash);
  }
  else
  {
    mutt_error(_("Can't open trash folder"));
    mailbox_free(&m_trash);
    return -1;
  }

  return 0;
}

/**
 * mx_mbox_close - Save changes and close mailbox
 * @param[out] ptr Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 *
 * @note Context will be freed after it's closed
 */
int mx_mbox_close(struct Context **ptr)
{
  if (!ptr || !*ptr)
    return 0;

  struct Context *ctx = *ptr;
  if (!ctx || !ctx->mailbox)
    return -1;

  struct Mailbox *m = ctx->mailbox;

  int i, read_msgs = 0;
  enum QuadOption move_messages = MUTT_NO;
  enum QuadOption purge = MUTT_YES;
  char mbox[PATH_MAX];
  char buf[PATH_MAX + 64];

  if (m->readonly || m->dontwrite || m->append)
  {
    mx_fastclose_mailbox(m);
    FREE(ptr);
    return 0;
  }

#ifdef USE_NNTP
  if ((m->msg_unread != 0) && (m->magic == MUTT_NNTP))
  {
    struct NntpMboxData *mdata = m->mdata;

    if (mdata && mdata->adata && mdata->group)
    {
      enum QuadOption ans =
          query_quadoption(C_CatchupNewsgroup, _("Mark all articles read?"));
      if (ans == MUTT_ABORT)
        return -1;
      else if (ans == MUTT_YES)
        mutt_newsgroup_catchup(Context->mailbox, mdata->adata, mdata->group);
    }
  }
#endif

  for (i = 0; i < m->msg_count; i++)
  {
    if (!m->emails[i])
      break;
    if (!m->emails[i]->deleted && m->emails[i]->read && !(m->emails[i]->flagged && C_KeepFlagged))
    {
      read_msgs++;
    }
  }

#ifdef USE_NNTP
  /* don't need to move articles from newsgroup */
  if (m->magic == MUTT_NNTP)
    read_msgs = 0;
#endif

  if ((read_msgs != 0) && (C_Move != MUTT_NO))
  {
    bool is_spool;
    char *p = mutt_find_hook(MUTT_MBOX_HOOK, mutt_b2s(m->pathbuf));
    if (p)
    {
      is_spool = true;
      mutt_str_strfcpy(mbox, p, sizeof(mbox));
    }
    else
    {
      mutt_str_strfcpy(mbox, C_Mbox, sizeof(mbox));
      is_spool = mutt_is_spool(mutt_b2s(m->pathbuf)) && !mutt_is_spool(mbox);
    }

    if (is_spool && (mbox[0] != '\0'))
    {
      mutt_expand_path(mbox, sizeof(mbox));
      snprintf(buf, sizeof(buf),
               /* L10N: The first argument is the number of read messages to be
                  moved, the second argument is the target mailbox. */
               ngettext("Move %d read message to %s?", "Move %d read messages to %s?", read_msgs),
               read_msgs, mbox);
      move_messages = query_quadoption(C_Move, buf);
      if (move_messages == MUTT_ABORT)
        return -1;
    }
  }

  /* There is no point in asking whether or not to purge if we are
   * just marking messages as "trash".  */
  if ((m->msg_deleted != 0) && !((m->magic == MUTT_MAILDIR) && C_MaildirTrash))
  {
    snprintf(buf, sizeof(buf),
             ngettext("Purge %d deleted message?", "Purge %d deleted messages?", m->msg_deleted),
             m->msg_deleted);
    purge = query_quadoption(C_Delete, buf);
    if (purge == MUTT_ABORT)
      return -1;
  }

  if (C_MarkOld)
  {
    for (i = 0; i < m->msg_count; i++)
    {
      if (!m->emails[i]->deleted && !m->emails[i]->old && !m->emails[i]->read)
        mutt_set_flag(m, m->emails[i], MUTT_OLD, true);
    }
  }

  if (move_messages)
  {
    if (!m->quiet)
      mutt_message(_("Moving read messages to %s..."), mbox);

#ifdef USE_IMAP
    /* try to use server-side copy first */
    i = 1;

    if ((m->magic == MUTT_IMAP) && (imap_path_probe(mbox, NULL) == MUTT_IMAP))
    {
      /* tag messages for moving, and clear old tags, if any */
      for (i = 0; i < m->msg_count; i++)
      {
        if (m->emails[i]->read && !m->emails[i]->deleted &&
            !(m->emails[i]->flagged && C_KeepFlagged))
        {
          m->emails[i]->tagged = true;
        }
        else
        {
          m->emails[i]->tagged = false;
        }
      }

      i = imap_copy_messages(ctx->mailbox, NULL, mbox, true);
    }

    if (i == 0) /* success */
      mutt_clear_error();
    else if (i == -1) /* horrible error, bail */
      return -1;
    else /* use regular append-copy mode */
#endif
    {
      struct Mailbox *m_read = mx_path_resolve(mbox);
      struct Context *ctx_read = mx_mbox_open(m_read, MUTT_APPEND);
      if (!ctx_read)
      {
        mailbox_free(&m_read);
        return -1;
      }

      for (i = 0; i < m->msg_count; i++)
      {
        if (m->emails[i]->read && !m->emails[i]->deleted &&
            !(m->emails[i]->flagged && C_KeepFlagged))
        {
          if (mutt_append_message(ctx_read->mailbox, ctx->mailbox, m->emails[i],
                                  MUTT_CM_NO_FLAGS, CH_UPDATE_LEN) == 0)
          {
            mutt_set_flag(m, m->emails[i], MUTT_DELETE, true);
            mutt_set_flag(m, m->emails[i], MUTT_PURGE, true);
          }
          else
          {
            mx_mbox_close(&ctx_read);
            return -1;
          }
        }
      }

      mx_mbox_close(&ctx_read);
    }
  }
  else if (!m->changed && (m->msg_deleted == 0))
  {
    if (!m->quiet)
      mutt_message(_("Mailbox is unchanged"));
    if ((m->magic == MUTT_MBOX) || (m->magic == MUTT_MMDF))
      mbox_reset_atime(m, NULL);
    mx_fastclose_mailbox(m);
    FREE(ptr);
    return 0;
  }

  /* copy mails to the trash before expunging */
  if (purge && (m->msg_deleted != 0) &&
      (mutt_str_strcmp(mutt_b2s(m->pathbuf), C_Trash) != 0))
  {
    if (trash_append(ctx->mailbox) != 0)
      return -1;
  }

#ifdef USE_IMAP
  /* allow IMAP to preserve the deleted flag across sessions */
  if (m->magic == MUTT_IMAP)
  {
    int check = imap_sync_mailbox(ctx->mailbox, (purge != MUTT_NO), true);
    if (check != 0)
      return check;
  }
  else
#endif
  {
    if (purge == MUTT_NO)
    {
      for (i = 0; i < m->msg_count; i++)
      {
        m->emails[i]->deleted = false;
        m->emails[i]->purge = false;
      }
      m->msg_deleted = 0;
    }

    if (m->changed || (m->msg_deleted != 0))
    {
      int check = sync_mailbox(ctx->mailbox, NULL);
      if (check != 0)
        return check;
    }
  }

  if (!m->quiet)
  {
    if (move_messages)
    {
      mutt_message(_("%d kept, %d moved, %d deleted"),
                   m->msg_count - m->msg_deleted, read_msgs, m->msg_deleted);
    }
    else
      mutt_message(_("%d kept, %d deleted"), m->msg_count - m->msg_deleted, m->msg_deleted);
  }

  if ((m->msg_count == m->msg_deleted) &&
      ((m->magic == MUTT_MMDF) || (m->magic == MUTT_MBOX)) &&
      !mutt_is_spool(mutt_b2s(m->pathbuf)) && !C_SaveEmpty)
  {
    mutt_file_unlink_empty(mutt_b2s(m->pathbuf));
  }

#ifdef USE_SIDEBAR
  if ((purge == MUTT_YES) && (m->msg_deleted != 0))
  {
    for (i = 0; i < m->msg_count; i++)
    {
      if (m->emails[i]->deleted && !m->emails[i]->read)
      {
        m->msg_unread--;
        if (!m->emails[i]->old)
          m->msg_new--;
      }
      if (m->emails[i]->deleted && m->emails[i]->flagged)
        m->msg_flagged--;
    }
  }
#endif

  mx_fastclose_mailbox(m);
  FREE(ptr);

  return 0;
}

/**
 * mx_mbox_sync - Save changes to mailbox
 * @param[in]  m          Mailbox
 * @param[out] index_hint Currently selected Email
 * @retval  0 Success
 * @retval -1 Error
 */
int mx_mbox_sync(struct Mailbox *m, int *index_hint)
{
  if (!m)
    return -1;

  int rc;
  int purge = 1;
  int msgcount, deleted;

  if (m->dontwrite)
  {
    char buf[256], tmp[256];
    if (km_expand_key(buf, sizeof(buf), km_find_func(MENU_MAIN, OP_TOGGLE_WRITE)))
      snprintf(tmp, sizeof(tmp), _(" Press '%s' to toggle write"), buf);
    else
      mutt_str_strfcpy(tmp, _("Use 'toggle-write' to re-enable write"), sizeof(tmp));

    mutt_error(_("Mailbox is marked unwritable. %s"), tmp);
    return -1;
  }
  else if (m->readonly)
  {
    mutt_error(_("Mailbox is read-only"));
    return -1;
  }

  if (!m->changed && (m->msg_deleted == 0))
  {
    if (!m->quiet)
      mutt_message(_("Mailbox is unchanged"));
    return 0;
  }

  if (m->msg_deleted != 0)
  {
    char buf[128];

    snprintf(buf, sizeof(buf),
             ngettext("Purge %d deleted message?", "Purge %d deleted messages?", m->msg_deleted),
             m->msg_deleted);
    purge = query_quadoption(C_Delete, buf);
    if (purge == MUTT_ABORT)
      return -1;
    else if (purge == MUTT_NO)
    {
      if (!m->changed)
        return 0; /* nothing to do! */
      /* let IMAP servers hold on to D flags */
      if (m->magic != MUTT_IMAP)
      {
        for (int i = 0; i < m->msg_count; i++)
        {
          m->emails[i]->deleted = false;
          m->emails[i]->purge = false;
        }
        m->msg_deleted = 0;
      }
    }
    mutt_mailbox_changed(m, MBN_UNTAG);
  }

  /* really only for IMAP - imap_sync_mailbox results in a call to
   * ctx_update_tables, so m->msg_deleted is 0 when it comes back */
  msgcount = m->msg_count;
  deleted = m->msg_deleted;

  if (purge && (m->msg_deleted != 0) &&
      (mutt_str_strcmp(mutt_b2s(m->pathbuf), C_Trash) != 0))
  {
    if (trash_append(m) != 0)
      return -1;
  }

#ifdef USE_IMAP
  if (m->magic == MUTT_IMAP)
    rc = imap_sync_mailbox(m, purge, false);
  else
#endif
    rc = sync_mailbox(m, index_hint);
  if (rc == 0)
  {
#ifdef USE_IMAP
    if ((m->magic == MUTT_IMAP) && !purge)
    {
      if (!m->quiet)
        mutt_message(_("Mailbox checkpointed"));
    }
    else
#endif
    {
      if (!m->quiet)
        mutt_message(_("%d kept, %d deleted"), msgcount - deleted, deleted);
    }

    mutt_sleep(0);

    if ((m->msg_count == m->msg_deleted) &&
        ((m->magic == MUTT_MBOX) || (m->magic == MUTT_MMDF)) &&
        !mutt_is_spool(mutt_b2s(m->pathbuf)) && !C_SaveEmpty)
    {
      unlink(mutt_b2s(m->pathbuf));
      mx_fastclose_mailbox(m);
      return 0;
    }

    /* if we haven't deleted any messages, we don't need to resort */
    /* ... except for certain folder formats which need "unsorted"
     * sort order in order to synchronize folders.
     *
     * MH and maildir are safe.  mbox-style seems to need re-sorting,
     * at least with the new threading code.  */
    if (purge || ((m->magic != MUTT_MAILDIR) && (m->magic != MUTT_MH)))
    {
      /* IMAP does this automatically after handling EXPUNGE */
      if (m->magic != MUTT_IMAP)
      {
        mutt_mailbox_changed(m, MBN_UPDATE);
        mutt_mailbox_changed(m, MBN_RESORT);
      }
    }
  }

  return rc;
}

/**
 * mx_msg_open_new - Open a new message
 * @param m     Destination mailbox
 * @param e     Message being copied (required for maildir support, because the filename depends on the message flags)
 * @param flags Flags, see #MsgOpenFlags
 * @retval ptr New Message
 */
struct Message *mx_msg_open_new(struct Mailbox *m, struct Email *e, MsgOpenFlags flags)
{
  if (!m)
    return NULL;

  struct Address *p = NULL;
  struct Message *msg = NULL;

  if (!m->mx_ops || !m->mx_ops->msg_open_new)
  {
    mutt_debug(LL_DEBUG1, "function unimplemented for mailbox type %d\n", m->magic);
    return NULL;
  }

  msg = mutt_mem_calloc(1, sizeof(struct Message));
  msg->write = true;

  if (e)
  {
    msg->flags.flagged = e->flagged;
    msg->flags.replied = e->replied;
    msg->flags.read = e->read;
    msg->flags.draft = (flags & MUTT_SET_DRAFT);
    msg->received = e->received;
  }

  if (msg->received == 0)
    time(&msg->received);

  if (m->mx_ops->msg_open_new(m, msg, e) == 0)
  {
    if (m->magic == MUTT_MMDF)
      fputs(MMDF_SEP, msg->fp);

    if (((m->magic == MUTT_MBOX) || (m->magic == MUTT_MMDF)) && flags & MUTT_ADD_FROM)
    {
      if (e)
      {
        p = TAILQ_FIRST(&e->env->return_path);
        if (!p)
          p = TAILQ_FIRST(&e->env->sender);
        if (!p)
          p = TAILQ_FIRST(&e->env->from);
      }

      fprintf(msg->fp, "From %s %s", p ? p->mailbox : NONULL(Username),
              ctime(&msg->received));
    }
  }
  else
    FREE(&msg);

  return msg;
}

/**
 * mx_mbox_check - Check for new mail - Wrapper for MxOps::mbox_check()
 * @param m          Mailbox
 * @param index_hint Current email
 * @retval >0 Success, e.g. #MUTT_NEW_MAIL
 * @retval  0 Success, no change
 * @retval -1 Failure
 */
int mx_mbox_check(struct Mailbox *m, int *index_hint)
{
  if (!m || !m->mx_ops)
    return -1;

  int rc = m->mx_ops->mbox_check(m, index_hint);
  if ((rc == MUTT_NEW_MAIL) || (rc == MUTT_REOPENED))
    mutt_mailbox_changed(m, MBN_INVALID);

  return rc;
}

/**
 * mx_msg_open - return a stream pointer for a message
 * @param m   Mailbox
 * @param msgno Message number
 * @retval ptr  Message
 * @retval NULL Error
 */
struct Message *mx_msg_open(struct Mailbox *m, int msgno)
{
  if (!m)
    return NULL;

  struct Message *msg = NULL;

  if (!m->mx_ops || !m->mx_ops->msg_open)
  {
    mutt_debug(LL_DEBUG1, "function not implemented for mailbox type %d\n", m->magic);
    return NULL;
  }

  msg = mutt_mem_calloc(1, sizeof(struct Message));
  if (m->mx_ops->msg_open(m, msg, msgno) < 0)
    FREE(&msg);

  return msg;
}

/**
 * mx_msg_commit - Commit a message to a folder - Wrapper for MxOps::msg_commit()
 * @param m   Mailbox
 * @param msg Message to commit
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_msg_commit(struct Mailbox *m, struct Message *msg)
{
  if (!m || !m->mx_ops || !m->mx_ops->msg_commit)
    return -1;

  if (!(msg->write && m->append))
  {
    mutt_debug(LL_DEBUG1, "msg->write = %d, m->append = %d\n", msg->write, m->append);
    return -1;
  }

  return m->mx_ops->msg_commit(m, msg);
}

/**
 * mx_msg_close - Close a message
 * @param[in]  m   Mailbox
 * @param[out] msg Message to close
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_msg_close(struct Mailbox *m, struct Message **msg)
{
  if (!m || !msg || !*msg)
    return 0;

  int rc = 0;

  if (m->mx_ops && m->mx_ops->msg_close)
    rc = m->mx_ops->msg_close(m, *msg);

  if ((*msg)->path)
  {
    mutt_debug(LL_DEBUG1, "unlinking %s\n", (*msg)->path);
    unlink((*msg)->path);
    FREE(&(*msg)->path);
  }

  FREE(&(*msg)->committed_path);
  FREE(msg);
  return rc;
}

/**
 * mx_alloc_memory - Create storage for the emails
 * @param m Mailbox
 */
void mx_alloc_memory(struct Mailbox *m)
{
  size_t s = MAX(sizeof(struct Email *), sizeof(int));

  if ((m->email_max + 25) * s < m->email_max * s)
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  m->email_max += 25;
  if (m->emails)
  {
    mutt_mem_realloc(&m->emails, sizeof(struct Email *) * m->email_max);
    mutt_mem_realloc(&m->v2r, sizeof(int) * m->email_max);
  }
  else
  {
    m->emails = mutt_mem_calloc(m->email_max, sizeof(struct Email *));
    m->v2r = mutt_mem_calloc(m->email_max, sizeof(int));
  }
  for (int i = m->msg_count; i < m->email_max; i++)
  {
    m->emails[i] = NULL;
    m->v2r[i] = -1;
  }
}

/**
 * mx_check_empty - Is the mailbox empty
 * @param path Mailbox to check
 * @retval 1 Mailbox is empty
 * @retval 0 Mailbox contains mail
 * @retval -1 Error
 */
int mx_check_empty(const char *path)
{
  switch (mx_path_probe(path, NULL))
  {
    case MUTT_MBOX:
    case MUTT_MMDF:
      return mutt_file_check_empty(path);
    case MUTT_MH:
      return mh_check_empty(path);
    case MUTT_MAILDIR:
      return maildir_check_empty(path);
#ifdef USE_IMAP
    case MUTT_IMAP:
    {
      int rc = imap_path_status(path, false);
      if (rc < 0)
        return -1;
      else if (rc == 0)
        return 1;
      else
        return 0;
    }
#endif
    default:
      errno = EINVAL;
      return -1;
  }
  /* not reached */
}

/**
 * mx_tags_edit - start the tag editor of the mailbox
 * @param m      Mailbox
 * @param tags   Existing tags
 * @param buf    Buffer for the results
 * @param buflen Length of the buffer
 * @retval -1 Error
 * @retval 0  No valid user input
 * @retval 1  Buffer set
 */
int mx_tags_edit(struct Mailbox *m, const char *tags, char *buf, size_t buflen)
{
  if (!m)
    return -1;

  if (m->mx_ops->tags_edit)
    return m->mx_ops->tags_edit(m, tags, buf, buflen);

  mutt_message(_("Folder doesn't support tagging, aborting"));
  return -1;
}

/**
 * mx_tags_commit - Save tags to the Mailbox - Wrapper for MxOps::tags_commit()
 * @param m    Mailbox
 * @param e    Email
 * @param tags Tags to save
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_tags_commit(struct Mailbox *m, struct Email *e, char *tags)
{
  if (!m)
    return -1;

  if (m->mx_ops->tags_commit)
    return m->mx_ops->tags_commit(m, e, tags);

  mutt_message(_("Folder doesn't support tagging, aborting"));
  return -1;
}

/**
 * mx_tags_is_supported - return true if mailbox support tagging
 * @param m Mailbox
 * @retval true Tagging is supported
 */
bool mx_tags_is_supported(struct Mailbox *m)
{
  return m && m->mx_ops->tags_commit && m->mx_ops->tags_edit;
}

/**
 * mx_path_probe - Find a mailbox that understands a path
 * @param[in]  path  Path to examine
 * @param[out] st    stat buffer (OPTIONAL, for local mailboxes)
 * @retval num Type, e.g. #MUTT_IMAP
 */
enum MailboxType mx_path_probe(const char *path, struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  static const struct MxOps *no_stat[] = {
#ifdef USE_IMAP
    &MxImapOps,
#endif
#ifdef USE_NOTMUCH
    &MxNotmuchOps,
#endif
#ifdef USE_POP
    &MxPopOps,
#endif
#ifdef USE_NNTP
    &MxNntpOps,
#endif
  };

  static const struct MxOps *with_stat[] = {
    &MxMaildirOps, &MxMboxOps, &MxMhOps, &MxMmdfOps,
#ifdef USE_COMPRESSED
    &MxCompOps,
#endif
  };

  enum MailboxType rc;

  for (size_t i = 0; i < mutt_array_size(no_stat); i++)
  {
    rc = no_stat[i]->path_probe(path, NULL);
    if (rc != MUTT_UNKNOWN)
      return rc;
  }

  struct stat st2 = { 0 };
  if (!st)
    st = &st2;

  if (stat(path, st) != 0)
  {
    mutt_debug(LL_DEBUG1, "unable to stat %s: %s (errno %d)\n", path, strerror(errno), errno);
    return MUTT_UNKNOWN;
  }

  for (size_t i = 0; i < mutt_array_size(with_stat); i++)
  {
    rc = with_stat[i]->path_probe(path, st);
    if (rc != MUTT_UNKNOWN)
      return rc;
  }

  return rc;
}

/**
 * mx_path_canon - Canonicalise a mailbox path - Wrapper for MxOps::path_canon
 */
int mx_path_canon(char *buf, size_t buflen, const char *folder, enum MailboxType *magic)
{
  if (!buf)
    return -1;

  for (size_t i = 0; i < 3; i++)
  {
    /* Look for !! ! - < > or ^ followed by / or NUL */
    if ((buf[0] == '!') && (buf[1] == '!'))
    {
      if (((buf[2] == '/') || (buf[2] == '\0')))
      {
        mutt_str_inline_replace(buf, buflen, 2, LastFolder);
      }
    }
    else if ((buf[0] == '+') || (buf[0] == '='))
    {
      size_t folder_len = mutt_str_strlen(C_Folder);
      if ((folder_len > 0) && (C_Folder[folder_len - 1] != '/'))
      {
        buf[0] = '/';
        mutt_str_inline_replace(buf, buflen, 0, C_Folder);
      }
      else
      {
        mutt_str_inline_replace(buf, buflen, 1, C_Folder);
      }
    }
    else if ((buf[1] == '/') || (buf[1] == '\0'))
    {
      if (buf[0] == '!')
      {
        mutt_str_inline_replace(buf, buflen, 1, C_Spoolfile);
      }
      else if (buf[0] == '-')
      {
        mutt_str_inline_replace(buf, buflen, 1, LastFolder);
      }
      else if (buf[0] == '<')
      {
        mutt_str_inline_replace(buf, buflen, 1, C_Record);
      }
      else if (buf[0] == '>')
      {
        mutt_str_inline_replace(buf, buflen, 1, C_Mbox);
      }
      else if (buf[0] == '^')
      {
        mutt_str_inline_replace(buf, buflen, 1, CurrentFolder);
      }
      else if (buf[0] == '~')
      {
        mutt_str_inline_replace(buf, buflen, 1, HomeDir);
      }
    }
    else if (buf[0] == '@')
    {
      /* elm compatibility, @ expands alias to user name */
      struct AddressList *al = mutt_alias_lookup(buf + 1);
      if (TAILQ_EMPTY(al))
        break;

      struct Email *e = mutt_email_new();
      e->env = mutt_env_new();
      mutt_addrlist_copy(&e->env->from, al, false);
      mutt_addrlist_copy(&e->env->to, al, false);
      mutt_default_save(buf, buflen, e);
      mutt_email_free(&e);
      break;
    }
    else
    {
      break;
    }
  }

  // if (!folder) //XXX - use inherited version, or pass NULL to backend?
  //   return -1;

  enum MailboxType magic2 = mx_path_probe(buf, NULL);
  if (magic)
    *magic = magic2;
  const struct MxOps *ops = mx_get_ops(magic2);
  if (!ops || !ops->path_canon)
    return -1;

  if (ops->path_canon(buf, buflen) < 0)
  {
    mutt_path_canon(buf, buflen, HomeDir);
  }

  return 0;
}

/**
 * mx_path_canon2 - XXX
 * canonicalise the path to realpath
 */
int mx_path_canon2(struct Mailbox *m, const char *folder)
{
  if (!m)
    return -1;

  char buf[PATH_MAX];

  if (m->realpath)
    mutt_str_strfcpy(buf, m->realpath, sizeof(buf));
  else
    mutt_str_strfcpy(buf, mutt_b2s(m->pathbuf), sizeof(buf));

  int rc = mx_path_canon(buf, sizeof(buf), folder, &m->magic);

  mutt_str_replace(&m->realpath, buf);

  if (rc >= 0)
  {
    m->mx_ops = mx_get_ops(m->magic);
    mutt_buffer_strcpy(m->pathbuf, m->realpath);
  }

  return rc;
}

/**
 * mx_path_pretty - Abbreviate a mailbox path - Wrapper for MxOps::path_pretty
 */
int mx_path_pretty(char *buf, size_t buflen, const char *folder)
{
  enum MailboxType magic = mx_path_probe(buf, NULL);
  const struct MxOps *ops = mx_get_ops(magic);
  if (!ops)
    return -1;

  if (!ops->path_canon)
    return -1;

  if (ops->path_canon(buf, buflen) < 0)
    return -1;

  if (!ops->path_pretty)
    return -1;

  if (ops->path_pretty(buf, buflen, folder) < 0)
    return -1;

  return 0;
}

/**
 * mx_path_parent - Find the parent of a mailbox path - Wrapper for MxOps::path_parent
 */
int mx_path_parent(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  return 0;
}

/**
 * mx_msg_padding_size - Bytes of padding between messages - Wrapper for MxOps::msg_padding_size
 * @param m Mailbox
 * @retval num Number of bytes of padding
 *
 * mmdf and mbox add separators, which leads a small discrepancy when computing
 * vsize for a limited view.
 */
int mx_msg_padding_size(struct Mailbox *m)
{
  if (!m || !m->mx_ops || !m->mx_ops->msg_padding_size)
    return 0;

  return m->mx_ops->msg_padding_size(m);
}

/**
 * mx_ac_find - XXX
 */
struct Account *mx_ac_find(struct Mailbox *m)
{
  if (!m || !m->mx_ops)
    return NULL;

  struct Account *np = NULL;
  TAILQ_FOREACH(np, &AllAccounts, entries)
  {
    if (np->magic != m->magic)
      continue;

    if (m->mx_ops->ac_find(np, m->realpath))
      return np;
  }

  return NULL;
}

/**
 * mx_mbox_find - XXX
 *
 * find a mailbox on an account
 */
struct Mailbox *mx_mbox_find(struct Account *a, const char *path)
{
  if (!a || !path)
    return NULL;

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &a->mailboxes, entries)
  {
    if (mutt_str_strcmp(np->mailbox->realpath, path) == 0)
      return np->mailbox;
  }

  return NULL;
}

/**
 * mx_mbox_find2 - XXX
 *
 * find a mailbox on an account
 */
struct Mailbox *mx_mbox_find2(const char *path)
{
  if (!path)
    return NULL;

  char buf[PATH_MAX];
  mutt_str_strfcpy(buf, path, sizeof(buf));
  mx_path_canon(buf, sizeof(buf), C_Folder, NULL);

  struct Account *np = NULL;
  TAILQ_FOREACH(np, &AllAccounts, entries)
  {
    struct Mailbox *m = mx_mbox_find(np, buf);
    if (m)
      return m;
  }

  return NULL;
}

/**
 * mx_path_resolve - XXX
 */
struct Mailbox *mx_path_resolve(const char *path)
{
  if (!path)
    return NULL;

  struct Mailbox *m = mx_mbox_find2(path);
  if (m)
    return m;

  m = mailbox_new();
  m->flags = MB_HIDDEN;
  mutt_buffer_strcpy(m->pathbuf, path);
  mx_path_canon2(m, C_Folder);

  return m;
}

/**
 * mx_ac_add - Add a Mailbox to an Account - Wrapper for MxOps::ac_add
 */
int mx_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m || !m->mx_ops || !m->mx_ops->ac_add)
    return -1;

  if (m->mx_ops->ac_add(a, m) < 0)
    return -1;

  m->account = a;
  struct MailboxNode *np = mutt_mem_calloc(1, sizeof(*np));
  np->mailbox = m;
  STAILQ_INSERT_TAIL(&a->mailboxes, np, entries);
  return 0;
}

/**
 * mx_ac_remove - Remove a Mailbox from an Account and delete Account if empty
 * @param m Mailbox to remove
 */
int mx_ac_remove(struct Mailbox *m)
{
  if (!m || !m->account)
    return -1;

  account_remove_mailbox(m->account, m);
  return 0;
}

/**
 * mx_mbox_check_stats - Check the statistics for a mailbox - Wrapper for MxOps::mbox_check_stats
 */
int mx_mbox_check_stats(struct Mailbox *m, int flags)
{
  if (!m)
    return -1;

  return m->mx_ops->mbox_check_stats(m, flags);
}

/**
 * mx_save_hcache - Save message to the header cache - Wrapper for MxOps::msg_save_hcache()
 * @param m Mailbox
 * @param e Email
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Write a single header out to the header cache.
 */
int mx_save_hcache(struct Mailbox *m, struct Email *e)
{
  if (!m->mx_ops || !m->mx_ops->msg_save_hcache)
    return 0;

  return m->mx_ops->msg_save_hcache(m, e);
}
