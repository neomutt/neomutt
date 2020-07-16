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
#include <locale.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "mx.h"
#include "context.h"
#include "copy.h"
#include "hook.h"
#include "keymap.h"
#include "mutt_globals.h"
#include "mutt_header.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "opcodes.h"
#include "options.h"
#include "protos.h"
#include "sort.h"
#include "maildir/lib.h"
#include "mbox/lib.h"
#ifdef USE_COMP_MBOX
#include "compmbox/lib.h"
#endif
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_POP
#include "pop/lib.h"
#endif
#ifdef USE_NNTP
#include "nntp/lib.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif

/* These Config Variables are only used in mx.c */
bool C_KeepFlagged; ///< Config: Don't move flagged messages from #C_Spoolfile to #C_Mbox
unsigned char C_MboxType; ///< Config: Default type for creating new mailboxes
unsigned char C_Move; ///< Config: Move emails from #C_Spoolfile to #C_Mbox when read
char *C_Trash;        ///< Config: Folder to put deleted emails

// clang-format off
static struct Mapping MboxTypeMap[] = {
  { "mbox",    MUTT_MBOX,    },
  { "MMDF",    MUTT_MMDF,    },
  { "MH",      MUTT_MH,      },
  { "Maildir", MUTT_MAILDIR, },
  { NULL,      0,            },
};
// clang-format on

struct EnumDef MboxTypeDef = {
  "mbox_type",
  4,
  (struct Mapping *) &MboxTypeMap,
};

/**
 * mx_ops - All the Mailbox backends
 */
const struct MxOps *mx_ops[] = {
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
#ifdef USE_COMP_MBOX
  &MxCompOps,
#endif
  NULL,
};

/**
 * mx_get_ops - Get mailbox operations
 * @param type Mailbox type
 * @retval ptr  Mailbox function
 * @retval NULL Error
 */
const struct MxOps *mx_get_ops(enum MailboxType type)
{
  for (const struct MxOps **ops = mx_ops; *ops; ops++)
    if ((*ops)->type == type)
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
  if (mutt_str_equal(str, C_Spoolfile))
    return true;

  struct Url *ua = url_parse(str);
  struct Url *ub = url_parse(C_Spoolfile);

  const bool is_spool =
      ua && ub && (ua->scheme == ub->scheme) &&
      mutt_istr_equal(ua->host, ub->host) && mutt_istr_equal(ua->path, ub->path) &&
      (!ua->user || !ub->user || mutt_str_equal(ua->user, ub->user));

  url_free(&ua);
  url_free(&ub);
  return is_spool;
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
  if ((m->type == MUTT_UNKNOWN) || (m->type == MUTT_MAILBOX_ERROR))
  {
    m->type = mx_path_probe(mailbox_path(m));

    if (m->type == MUTT_UNKNOWN)
    {
      if (flags & (MUTT_APPEND | MUTT_NEWFOLDER))
      {
        m->type = MUTT_MAILBOX_ERROR;
      }
      else
      {
        mutt_error(_("%s is not a mailbox"), mailbox_path(m));
        return -1;
      }
    }

    if (m->type == MUTT_MAILBOX_ERROR)
    {
      if (stat(mailbox_path(m), &sb) == -1)
      {
        if (errno == ENOENT)
        {
#ifdef USE_COMP_MBOX
          if (mutt_comp_can_append(m))
            m->type = MUTT_COMPRESSED;
          else
#endif
            m->type = C_MboxType;
          flags |= MUTT_APPENDNEW;
        }
        else
        {
          mutt_perror(mailbox_path(m));
          return -1;
        }
      }
      else
        return -1;
    }

    m->mx_ops = mx_get_ops(m->type);
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

  struct Context *ctx = ctx_new();
  ctx->mailbox = m;

  struct EventContext ev_ctx = { ctx };
  notify_send(ctx->notify, NT_CONTEXT, NT_CONTEXT_OPEN, &ev_ctx);

  // If the Mailbox is closed, Context->mailbox must be set to NULL
  notify_observer_add(m->notify, ctx_mailbox_observer, ctx);

  if ((m->type == MUTT_UNKNOWN) && (flags & (MUTT_NEWFOLDER | MUTT_APPEND)))
  {
    m->type = C_MboxType;
    m->mx_ops = mx_get_ops(m->type);
  }

  const bool newly_linked_account = !m->account;
  if (newly_linked_account)
  {
    struct Account *a = mx_ac_find(m);
    bool new_account = false;
    if (!a)
    {
      a = account_new(NULL, NeoMutt->sub);
      a->type = m->type;
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
      neomutt_account_add(NeoMutt, a);
    }
  }

  ctx->msg_not_read_yet = -1;
  ctx->collapsed = false;

  m->verbose = !(flags & MUTT_QUIET);
  if (flags & MUTT_READONLY)
    m->readonly = true;
  m->peekonly = (flags & MUTT_PEEK);

  if (flags & (MUTT_APPEND | MUTT_NEWFOLDER))
  {
    if (mx_open_mailbox_append(ctx->mailbox, flags) != 0)
    {
      goto error;
    }
    return ctx;
  }

  if (m->opened > 0)
  {
    m->opened++;
    return ctx;
  }

  m->size = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->rights = MUTT_ACL_ALL;

  if (m->type == MUTT_UNKNOWN)
  {
    m->type = mx_path_probe(mailbox_path(m));
    m->mx_ops = mx_get_ops(m->type);
  }

  if ((m->type == MUTT_UNKNOWN) || (m->type == MUTT_MAILBOX_ERROR) || !m->mx_ops)
  {
    if (m->type == MUTT_MAILBOX_ERROR)
      mutt_perror(mailbox_path(m));
    else if ((m->type == MUTT_UNKNOWN) || !m->mx_ops)
      mutt_error(_("%s is not a mailbox"), mailbox_path(m));
    goto error;
  }

  mutt_make_label_hash(m);

  /* if the user has a 'push' command in their .neomuttrc, or in a folder-hook,
   * it will cause the progress messages not to be displayed because
   * mutt_refresh() will think we are in the middle of a macro.  so set a
   * flag to indicate that we should really refresh the screen.  */
  OptForceRefresh = true;

  if (m->verbose)
    mutt_message(_("Reading %s..."), mailbox_path(m));

  // Clear out any existing emails
  for (int i = 0; i < m->email_max; i++)
  {
    email_free(&m->emails[i]);
  }

  m->msg_count = 0;
  m->msg_unread = 0;
  m->msg_flagged = 0;
  m->msg_new = 0;
  m->msg_deleted = 0;
  m->msg_tagged = 0;
  m->vcount = 0;

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
    }
    if (m->verbose)
      mutt_clear_error();
    if (rc == -2)
    {
      mutt_error(_("Reading from %s interrupted..."), mailbox_path(m));
      mutt_sort_headers(ctx, true);
    }
  }
  else
  {
    goto error;
  }

  if (!m->peekonly)
    m->has_new = false;
  OptForceRefresh = false;

  return ctx;

error:
  mx_fastclose_mailbox(m);
  if (newly_linked_account)
    account_mailbox_remove(m->account, m);
  ctx_free(&ctx);
  return NULL;
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

  /* never announce that a mailbox we've just left has new mail.
   * TODO: really belongs in mx_mbox_close, but this is a nice hook point */
  if (!m->peekonly)
    mutt_mailbox_set_notified(m);

  if (m->mx_ops)
    m->mx_ops->mbox_close(m);

  mutt_hash_free(&m->subj_hash);
  mutt_hash_free(&m->id_hash);
  mutt_hash_free(&m->label_hash);

  if (m->emails)
  {
    for (int i = 0; i < m->msg_count; i++)
    {
      if (!m->emails[i])
        break;
      email_free(&m->emails[i]);
    }
  }
}

/**
 * sync_mailbox - save changes to disk
 * @param m          Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
static int sync_mailbox(struct Mailbox *m)
{
  if (!m || !m->mx_ops || !m->mx_ops->mbox_sync)
    return -1;

  if (m->verbose)
  {
    /* L10N: Displayed before/as a mailbox is being synced */
    mutt_message(_("Writing %s..."), mailbox_path(m));
  }

  int rc = m->mx_ops->mbox_sync(m);
  if (rc != 0)
  {
    mutt_debug(LL_DEBUG2, "mbox_sync returned: %d\n", rc);
    if ((rc < 0) && m->verbose)
    {
      /* L10N: Displayed if a mailbox sync fails */
      mutt_error(_("Unable to write %s"), mailbox_path(m));
    }
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

  if (!C_Trash || (m->msg_deleted == 0) || ((m->type == MUTT_MAILDIR) && C_MaildirTrash))
  {
    return 0;
  }

  int delmsgcount = 0;
  int first_del = -1;
  for (int i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    if (e->deleted && !e->purge)
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

  if ((lstat(mailbox_path(m), &stc) == 0) && (stc.st_ino == st.st_ino) &&
      (stc.st_dev == st.st_dev) && (stc.st_rdev == st.st_rdev))
  {
    return 0; /* we are in the trash folder: simple sync */
  }

#ifdef USE_IMAP
  if ((m->type == MUTT_IMAP) && (imap_path_probe(C_Trash, NULL) == MUTT_IMAP))
  {
    if (imap_fast_trash(m, C_Trash) == 0)
      return 0;
  }
#endif

  struct Mailbox *m_trash = mx_path_resolve(C_Trash);
  const bool old_append = m_trash->append;
  struct Context *ctx_trash = mx_mbox_open(m_trash, MUTT_APPEND);
  if (ctx_trash)
  {
    /* continue from initial scan above */
    for (int i = first_del; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;

      if (e->deleted && !e->purge)
      {
        if (mutt_append_message(ctx_trash->mailbox, m, e, MUTT_CM_NO_FLAGS, CH_NO_FLAGS) == -1)
        {
          mx_mbox_close(&ctx_trash);
          m_trash->append = old_append;
          return -1;
        }
      }
    }

    mx_mbox_close(&ctx_trash);
    m_trash->append = old_append;
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
 * @retval #MUTT_REOPENED  mailbox has been externally modified
 * @retval #MUTT_NEW_MAIL  new mail has arrived
 * @retval  0 Success
 * @retval -1 Failure
 *
 * @note The flag retvals come from a call to a backend sync function
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

  if (C_MailCheckRecent && !m->peekonly)
    m->has_new = false;

  if (m->readonly || m->dontwrite || m->append || m->peekonly)
  {
    mx_fastclose_mailbox(m);
    ctx_free(ptr);
    return 0;
  }

  int i, read_msgs = 0;
  int rc = -1;
  enum QuadOption move_messages = MUTT_NO;
  enum QuadOption purge = MUTT_YES;
  struct Buffer *mbox = NULL;
  struct Buffer *buf = mutt_buffer_pool_get();

#ifdef USE_NNTP
  if ((m->msg_unread != 0) && (m->type == MUTT_NNTP))
  {
    struct NntpMboxData *mdata = m->mdata;

    if (mdata && mdata->adata && mdata->group)
    {
      enum QuadOption ans =
          query_quadoption(C_CatchupNewsgroup, _("Mark all articles read?"));
      if (ans == MUTT_ABORT)
        goto cleanup;
      if (ans == MUTT_YES)
        mutt_newsgroup_catchup(m, mdata->adata, mdata->group);
    }
  }
#endif

  for (i = 0; i < m->msg_count; i++)
  {
    struct Email *e = m->emails[i];
    if (!e)
      break;

    if (!e->deleted && e->read && !(e->flagged && C_KeepFlagged))
      read_msgs++;
  }

#ifdef USE_NNTP
  /* don't need to move articles from newsgroup */
  if (m->type == MUTT_NNTP)
    read_msgs = 0;
#endif

  if ((read_msgs != 0) && (C_Move != MUTT_NO))
  {
    bool is_spool;
    mbox = mutt_buffer_pool_get();

    char *p = mutt_find_hook(MUTT_MBOX_HOOK, mailbox_path(m));
    if (p)
    {
      is_spool = true;
      mutt_buffer_strcpy(mbox, p);
    }
    else
    {
      mutt_buffer_strcpy(mbox, C_Mbox);
      is_spool = mutt_is_spool(mailbox_path(m)) && !mutt_is_spool(mutt_b2s(mbox));
    }

    if (is_spool && !mutt_buffer_is_empty(mbox))
    {
      mutt_buffer_expand_path(mbox);
      mutt_buffer_printf(buf,
                         /* L10N: The first argument is the number of read messages to be
                            moved, the second argument is the target mailbox. */
                         ngettext("Move %d read message to %s?",
                                  "Move %d read messages to %s?", read_msgs),
                         read_msgs, mutt_b2s(mbox));
      move_messages = query_quadoption(C_Move, mutt_b2s(buf));
      if (move_messages == MUTT_ABORT)
        goto cleanup;
    }
  }

  /* There is no point in asking whether or not to purge if we are
   * just marking messages as "trash".  */
  if ((m->msg_deleted != 0) && !((m->type == MUTT_MAILDIR) && C_MaildirTrash))
  {
    mutt_buffer_printf(buf,
                       ngettext("Purge %d deleted message?",
                                "Purge %d deleted messages?", m->msg_deleted),
                       m->msg_deleted);
    purge = query_quadoption(C_Delete, mutt_b2s(buf));
    if (purge == MUTT_ABORT)
      goto cleanup;
  }

  if (C_MarkOld && !m->peekonly)
  {
    for (i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      if (!e->deleted && !e->old && !e->read)
        mutt_set_flag(m, e, MUTT_OLD, true);
    }
  }

  if (move_messages)
  {
    if (m->verbose)
      mutt_message(_("Moving read messages to %s..."), mutt_b2s(mbox));

#ifdef USE_IMAP
    /* try to use server-side copy first */
    i = 1;

    if ((m->type == MUTT_IMAP) && (imap_path_probe(mutt_b2s(mbox), NULL) == MUTT_IMAP))
    {
      /* add messages for moving, and clear old tags, if any */
      struct EmailList el = STAILQ_HEAD_INITIALIZER(el);
      for (i = 0; i < m->msg_count; i++)
      {
        struct Email *e = m->emails[i];
        if (!e)
          break;

        if (e->read && !e->deleted && !(e->flagged && C_KeepFlagged))
        {
          e->tagged = true;
          emaillist_add_email(&el, e);
        }
        else
          e->tagged = false;
      }

      i = imap_copy_messages(ctx->mailbox, &el, mutt_b2s(mbox), true);
      emaillist_clear(&el);
    }

    if (i == 0) /* success */
      mutt_clear_error();
    else if (i == -1) /* horrible error, bail */
      goto cleanup;
    else /* use regular append-copy mode */
#endif
    {
      struct Mailbox *m_read = mx_path_resolve(mutt_b2s(mbox));
      struct Context *ctx_read = mx_mbox_open(m_read, MUTT_APPEND);
      if (!ctx_read)
      {
        mailbox_free(&m_read);
        goto cleanup;
      }

      for (i = 0; i < m->msg_count; i++)
      {
        struct Email *e = m->emails[i];
        if (!e)
          break;
        if (e->read && !e->deleted && !(e->flagged && C_KeepFlagged))
        {
          if (mutt_append_message(ctx_read->mailbox, ctx->mailbox, e,
                                  MUTT_CM_NO_FLAGS, CH_UPDATE_LEN) == 0)
          {
            mutt_set_flag(m, e, MUTT_DELETE, true);
            mutt_set_flag(m, e, MUTT_PURGE, true);
          }
          else
          {
            mx_mbox_close(&ctx_read);
            goto cleanup;
          }
        }
      }

      mx_mbox_close(&ctx_read);
    }
  }
  else if (!m->changed && (m->msg_deleted == 0))
  {
    if (m->verbose)
      mutt_message(_("Mailbox is unchanged"));
    if ((m->type == MUTT_MBOX) || (m->type == MUTT_MMDF))
      mbox_reset_atime(m, NULL);
    mx_fastclose_mailbox(m);
    ctx_free(ptr);
    rc = 0;
    goto cleanup;
  }

  /* copy mails to the trash before expunging */
  const struct Mailbox *m_trash = mx_mbox_find(m->account, C_Trash);
  if (purge && (m->msg_deleted != 0) && (m != m_trash))
  {
    if (trash_append(ctx->mailbox) != 0)
      goto cleanup;
  }

#ifdef USE_IMAP
  /* allow IMAP to preserve the deleted flag across sessions */
  if (m->type == MUTT_IMAP)
  {
    int check = imap_sync_mailbox(ctx->mailbox, (purge != MUTT_NO), true);
    if (check < 0)
    {
      rc = check;
      goto cleanup;
    }
  }
  else
#endif
  {
    if (purge == MUTT_NO)
    {
      for (i = 0; i < m->msg_count; i++)
      {
        struct Email *e = m->emails[i];
        if (!e)
          break;

        e->deleted = false;
        e->purge = false;
      }
      m->msg_deleted = 0;
    }

    if (m->changed || (m->msg_deleted != 0))
    {
      int check = sync_mailbox(ctx->mailbox);
      if (check != 0)
      {
        rc = check;
        goto cleanup;
      }
    }
  }

  if (m->verbose)
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
      ((m->type == MUTT_MMDF) || (m->type == MUTT_MBOX)) &&
      !mutt_is_spool(mailbox_path(m)) && !C_SaveEmpty)
  {
    mutt_file_unlink_empty(mailbox_path(m));
  }

#ifdef USE_SIDEBAR
  if ((purge == MUTT_YES) && (m->msg_deleted != 0))
  {
    for (i = 0; i < m->msg_count; i++)
    {
      struct Email *e = m->emails[i];
      if (!e)
        break;
      if (e->deleted && !e->read)
      {
        m->msg_unread--;
        if (!e->old)
          m->msg_new--;
      }
      if (e->deleted && e->flagged)
        m->msg_flagged--;
    }
  }
#endif

  mx_fastclose_mailbox(m);
  ctx_free(ptr);

  rc = 0;

cleanup:
  mutt_buffer_pool_release(&mbox);
  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * mx_mbox_sync - Save changes to mailbox
 * @param[in]  m          Mailbox
 * @retval #MUTT_REOPENED  mailbox has been externally modified
 * @retval #MUTT_NEW_MAIL  new mail has arrived
 * @retval  0 Success
 * @retval -1 Error
 *
 * @note The flag retvals come from a call to a backend sync function
 */
int mx_mbox_sync(struct Mailbox *m)
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
      mutt_str_copy(tmp, _("Use 'toggle-write' to re-enable write"), sizeof(tmp));

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
    if (m->verbose)
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
    if (purge == MUTT_NO)
    {
      if (!m->changed)
        return 0; /* nothing to do! */
      /* let IMAP servers hold on to D flags */
      if (m->type != MUTT_IMAP)
      {
        for (int i = 0; i < m->msg_count; i++)
        {
          struct Email *e = m->emails[i];
          if (!e)
            break;
          e->deleted = false;
          e->purge = false;
        }
        m->msg_deleted = 0;
      }
    }
    mailbox_changed(m, NT_MAILBOX_UNTAG);
  }

  /* really only for IMAP - imap_sync_mailbox results in a call to
   * ctx_update_tables, so m->msg_deleted is 0 when it comes back */
  msgcount = m->msg_count;
  deleted = m->msg_deleted;

  const struct Mailbox *m_trash = mx_mbox_find(m->account, C_Trash);
  if (purge && (m->msg_deleted != 0) && (m != m_trash))
  {
    if (trash_append(m) != 0)
      return -1;
  }

#ifdef USE_IMAP
  if (m->type == MUTT_IMAP)
    rc = imap_sync_mailbox(m, purge, false);
  else
#endif
    rc = sync_mailbox(m);
  if (rc >= 0)
  {
#ifdef USE_IMAP
    if ((m->type == MUTT_IMAP) && !purge)
    {
      if (m->verbose)
        mutt_message(_("Mailbox checkpointed"));
    }
    else
#endif
    {
      if (m->verbose)
        mutt_message(_("%d kept, %d deleted"), msgcount - deleted, deleted);
    }

    mutt_sleep(0);

    if ((m->msg_count == m->msg_deleted) &&
        ((m->type == MUTT_MBOX) || (m->type == MUTT_MMDF)) &&
        !mutt_is_spool(mailbox_path(m)) && !C_SaveEmpty)
    {
      unlink(mailbox_path(m));
      mx_fastclose_mailbox(m);
      return 0;
    }

    /* if we haven't deleted any messages, we don't need to resort */
    /* ... except for certain folder formats which need "unsorted"
     * sort order in order to synchronize folders.
     *
     * MH and maildir are safe.  mbox-style seems to need re-sorting,
     * at least with the new threading code.  */
    if (purge || ((m->type != MUTT_MAILDIR) && (m->type != MUTT_MH)))
    {
      /* IMAP does this automatically after handling EXPUNGE */
      if (m->type != MUTT_IMAP)
      {
        mailbox_changed(m, NT_MAILBOX_UPDATE);
        mailbox_changed(m, NT_MAILBOX_RESORT);
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
    mutt_debug(LL_DEBUG1, "function unimplemented for mailbox type %d\n", m->type);
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
    msg->received = mutt_date_epoch();

  if (m->mx_ops->msg_open_new(m, msg, e) == 0)
  {
    if (m->type == MUTT_MMDF)
      fputs(MMDF_SEP, msg->fp);

    if (((m->type == MUTT_MBOX) || (m->type == MUTT_MMDF)) && (flags & MUTT_ADD_FROM))
    {
      if (e)
      {
        p = TAILQ_FIRST(&e->env->return_path);
        if (!p)
          p = TAILQ_FIRST(&e->env->sender);
        if (!p)
          p = TAILQ_FIRST(&e->env->from);
      }

      // Force a 'C' locale for the date, so that day/month names are in English
      locale_t loc = newlocale(LC_TIME_MASK, "C", 0);
      char buf[64] = { 0 };
      struct tm tm = mutt_date_localtime(msg->received);
      strftime_l(buf, sizeof(buf), "%a %b %e %H:%M:%S %Y", &tm, loc);
      freelocale(loc);
      fprintf(msg->fp, "From %s %s\n", p ? p->mailbox : NONULL(Username), buf);
    }
  }
  else
    FREE(&msg);

  return msg;
}

/**
 * mx_mbox_check - Check for new mail - Wrapper for MxOps::mbox_check()
 * @param m          Mailbox
 * @retval >0 Success, e.g. #MUTT_NEW_MAIL
 * @retval  0 Success, no change
 * @retval -1 Failure
 */
int mx_mbox_check(struct Mailbox *m)
{
  if (!m || !m->mx_ops)
    return -1;

  int rc = m->mx_ops->mbox_check(m);
  if ((rc == MUTT_NEW_MAIL) || (rc == MUTT_REOPENED))
    mailbox_changed(m, NT_MAILBOX_INVALID);

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
    mutt_debug(LL_DEBUG1, "function not implemented for mailbox type %d\n", m->type);
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
  for (int i = m->email_max - 25; i < m->email_max; i++)
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
  switch (mx_path_probe(path))
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
      if (rc == 0)
        return 1;
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
 * @param path Path to examine
 * @retval num Type, e.g. #MUTT_IMAP
 */
enum MailboxType mx_path_probe(const char *path)
{
  if (!path)
    return MUTT_UNKNOWN;

  enum MailboxType rc;

  // First, search the non-local Mailbox types (is_local == false)
  for (const struct MxOps **ops = mx_ops; *ops; ops++)
  {
    if ((*ops)->is_local)
      continue;
    rc = (*ops)->path_probe(path, NULL);
    if (rc != MUTT_UNKNOWN)
      return rc;
  }

  struct stat st = { 0 };
  if (stat(path, &st) != 0)
  {
    mutt_debug(LL_DEBUG1, "unable to stat %s: %s (errno %d)\n", path, strerror(errno), errno);
    return MUTT_UNKNOWN;
  }

  if (S_ISFIFO(st.st_mode))
  {
    mutt_error(_("Can't open %s: it is a pipe"), path);
    return MUTT_UNKNOWN;
  }

  // Next, search the local Mailbox types (is_local == true)
  for (const struct MxOps **ops = mx_ops; *ops; ops++)
  {
    if (!(*ops)->is_local)
      continue;
    rc = (*ops)->path_probe(path, &st);
    if (rc != MUTT_UNKNOWN)
      return rc;
  }

  return rc;
}

/**
 * mx_path_canon - Canonicalise a mailbox path - Wrapper for MxOps::path_canon()
 */
int mx_path_canon(char *buf, size_t buflen, const char *folder, enum MailboxType *type)
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
      size_t folder_len = mutt_str_len(folder);
      if ((folder_len > 0) && (folder[folder_len - 1] != '/'))
      {
        buf[0] = '/';
        mutt_str_inline_replace(buf, buflen, 0, folder);
      }
      else
      {
        mutt_str_inline_replace(buf, buflen, 1, folder);
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
      struct AddressList *al = alias_lookup(buf + 1);
      if (!al || TAILQ_EMPTY(al))
        break;

      struct Email *e = email_new();
      e->env = mutt_env_new();
      mutt_addrlist_copy(&e->env->from, al, false);
      mutt_addrlist_copy(&e->env->to, al, false);
      mutt_default_save(buf, buflen, e);
      email_free(&e);
      break;
    }
    else
    {
      break;
    }
  }

  // if (!folder) //XXX - use inherited version, or pass NULL to backend?
  //   return -1;

  enum MailboxType type2 = mx_path_probe(buf);
  if (type)
    *type = type2;
  const struct MxOps *ops = mx_get_ops(type2);
  if (!ops || !ops->path_canon)
    return -1;

  if (ops->path_canon(buf, buflen) < 0)
  {
    mutt_path_canon(buf, buflen, HomeDir, true);
  }

  return 0;
}

/**
 * mx_path_canon2 - Canonicalise the path to realpath
 * @param m      Mailbox
 * @param folder Path to canonicalise
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_path_canon2(struct Mailbox *m, const char *folder)
{
  if (!m)
    return -1;

  char buf[PATH_MAX];

  if (m->realpath)
    mutt_str_copy(buf, m->realpath, sizeof(buf));
  else
    mutt_str_copy(buf, mailbox_path(m), sizeof(buf));

  int rc = mx_path_canon(buf, sizeof(buf), folder, &m->type);

  mutt_str_replace(&m->realpath, buf);

  if (rc >= 0)
  {
    m->mx_ops = mx_get_ops(m->type);
    mutt_buffer_strcpy(&m->pathbuf, m->realpath);
  }

  return rc;
}

/**
 * mx_path_pretty - Abbreviate a mailbox path - Wrapper for MxOps::path_pretty()
 */
int mx_path_pretty(char *buf, size_t buflen, const char *folder)
{
  enum MailboxType type = mx_path_probe(buf);
  const struct MxOps *ops = mx_get_ops(type);
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
 * mx_path_parent - Find the parent of a mailbox path - Wrapper for MxOps::path_parent()
 */
int mx_path_parent(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  return 0;
}

/**
 * mx_msg_padding_size - Bytes of padding between messages - Wrapper for MxOps::msg_padding_size()
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
 * mx_ac_find - Find the Account owning a Mailbox
 * @param m Mailbox
 * @retval ptr  Account
 * @retval NULL None found
 */
struct Account *mx_ac_find(struct Mailbox *m)
{
  if (!m || !m->mx_ops)
    return NULL;

  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
    if (np->type != m->type)
      continue;

    if (m->mx_ops->ac_find(np, m->realpath))
      return np;
  }

  return NULL;
}

/**
 * mx_mbox_find - Find a Mailbox on an Account
 * @param a    Account to search
 * @param path Path to find
 * @retval ptr Mailbox
 */
struct Mailbox *mx_mbox_find(struct Account *a, const char *path)
{
  if (!a || !path)
    return NULL;

  struct MailboxNode *np = NULL;
  struct Url *url_p = NULL;
  struct Url *url_a = NULL;

  const bool use_url = (a->type == MUTT_IMAP);
  if (use_url)
  {
    url_p = url_parse(path);
    if (!url_p)
      goto done;
  }

  STAILQ_FOREACH(np, &a->mailboxes, entries)
  {
    if (!use_url)
    {
      if (mutt_str_equal(np->mailbox->realpath, path))
        return np->mailbox;
      continue;
    }

    url_free(&url_a);
    url_a = url_parse(np->mailbox->realpath);
    if (!url_a)
      continue;

    if (!mutt_istr_equal(url_a->host, url_p->host))
      continue;
    if (url_p->user && !mutt_istr_equal(url_a->user, url_p->user))
      continue;
    if (a->type == MUTT_IMAP)
    {
      if (imap_mxcmp(url_a->path, url_p->path) == 0)
        break;
    }
    else
    {
      if (mutt_str_equal(url_a->path, url_p->path))
        break;
    }
  }

done:
  url_free(&url_p);
  url_free(&url_a);

  if (!np)
    return NULL;
  return np->mailbox;
}

/**
 * mx_mbox_find2 - Find a Mailbox on an Account
 * @param path Path to find
 * @retval ptr  Mailbox
 * @retval NULL No match
 */
struct Mailbox *mx_mbox_find2(const char *path)
{
  if (!path)
    return NULL;

  char buf[PATH_MAX];
  mutt_str_copy(buf, path, sizeof(buf));
  mx_path_canon(buf, sizeof(buf), C_Folder, NULL);

  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
    struct Mailbox *m = mx_mbox_find(np, buf);
    if (m)
      return m;
  }

  return NULL;
}

/**
 * mx_path_resolve - Get a Mailbox for a path
 * @param path Mailbox path
 * @retval ptr Mailbox
 *
 * If there isn't a Mailbox for the path, one will be created.
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
  mutt_buffer_strcpy(&m->pathbuf, path);
  mx_path_canon2(m, C_Folder);

  return m;
}

/**
 * mx_mbox_find_by_name_ac - Find a Mailbox with given name under an Account
 * @param a    Account to search
 * @param name Name to find
 * @retval ptr Mailbox
 */
static struct Mailbox *mx_mbox_find_by_name_ac(struct Account *a, const char *name)
{
  if (!a || !name)
    return NULL;

  struct MailboxNode *np = NULL;

  STAILQ_FOREACH(np, &a->mailboxes, entries)
  {
    if (mutt_str_equal(np->mailbox->name, name))
      return np->mailbox;
  }

  return NULL;
}

/**
 * mx_mbox_find_by_name - Find a Mailbox with given name
 * @param name Name to search
 * @retval ptr Mailbox
 */
static struct Mailbox *mx_mbox_find_by_name(const char *name)
{
  if (!name)
    return NULL;

  struct Account *np = NULL;
  TAILQ_FOREACH(np, &NeoMutt->accounts, entries)
  {
    struct Mailbox *m = mx_mbox_find_by_name_ac(np, name);
    if (m)
      return m;
  }

  return NULL;
}

/**
 * mx_resolve - Get a Mailbox from either a path or name
 * @param path_or_name Mailbox path or name
 * @retval ptr         Mailbox
 *
 * Order of resolving:
 *  1. Name
 *  2. Path
 */
struct Mailbox *mx_resolve(const char *path_or_name)
{
  if (!path_or_name)
    return NULL;

  // Order is name first because you can create a Mailbox from
  // a path, but can't from a name. So fallback behavior creates
  // a new Mailbox for us.
  struct Mailbox *m = mx_mbox_find_by_name(path_or_name);
  if (!m)
    m = mx_path_resolve(path_or_name);

  return m;
}

/**
 * mx_ac_add - Add a Mailbox to an Account - Wrapper for MxOps::ac_add()
 */
int mx_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m || !m->mx_ops || !m->mx_ops->ac_add)
    return -1;

  if (m->mx_ops->ac_add(a, m) < 0)
    return -1;

  account_mailbox_add(a, m);
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

  struct Account *a = m->account;
  account_mailbox_remove(m->account, m);
  mailbox_free(&m);
  if (STAILQ_EMPTY(&a->mailboxes))
  {
    neomutt_account_remove(NeoMutt, a);
  }
  return 0;
}

/**
 * mx_mbox_check_stats - Check the statistics for a mailbox - Wrapper for MxOps::mbox_check_stats()
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
