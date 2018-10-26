/**
 * @file
 * Mailbox multiplexor
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2016-2018 Richard Russon <rich@flatcap.org>
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
#include "maildir/maildir.h"
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
unsigned char CatchupNewsgroup; ///< Config: (nntp) Mark all articles as read when leaving a newsgroup
bool KeepFlagged; ///< Config: Don't move flagged messages from Spoolfile to Mbox
short MboxType;   ///< Config: Default type for creating new mailboxes
unsigned char Move; ///< Config: Move emails from Spoolfile to Mbox when read
char *Trash;        ///< Config: Folder to put deleted emails

/**
 * mx_ops - All the Mailbox backends
 */
static const struct MxOps *mx_ops[] = {
/* These mailboxes can be recognised by their Url scheme */
#ifdef USE_IMAP
  &mx_imap_ops,
#endif
#ifdef USE_NOTMUCH
  &mx_notmuch_ops,
#endif
#ifdef USE_POP
  &mx_pop_ops,
#endif
#ifdef USE_NNTP
  &mx_nntp_ops,
#endif

  /* Local mailboxes */
  &mx_maildir_ops,
  &mx_mbox_ops,
  &mx_mh_ops,
  &mx_mmdf_ops,

/* If everything else fails... */
#ifdef USE_COMPRESSED
  &mx_comp_ops,
#endif
  NULL,
};

/**
 * mx_get_ops - Get mailbox operations
 * @param magic Mailbox magic number
 * @retval ptr  Mailbox function
 * @retval NULL Error
 */
const struct MxOps *mx_get_ops(int magic)
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
  return mutt_str_strcmp(Spoolfile, str) == 0;
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
 * @param ctx   Mailbox
 * @param flags Flags, e.g. #MUTT_READONLY
 * @retval  0 Success
 * @retval -1 Failure
 */
static int mx_open_mailbox_append(struct Context *ctx, int flags)
{
  struct stat sb;

  ctx->append = true;
  ctx->mailbox->magic = mx_path_probe(ctx->mailbox->path, NULL);
  if (ctx->mailbox->magic == MUTT_UNKNOWN)
  {
    if (flags & (MUTT_APPEND | MUTT_NEWFOLDER))
    {
      ctx->mailbox->magic = MUTT_MAILBOX_ERROR;
    }
    else
    {
      mutt_error(_("%s is not a mailbox"), ctx->mailbox->path);
      return -1;
    }
  }

  if (ctx->mailbox->magic == MUTT_MAILBOX_ERROR)
  {
    if (stat(ctx->mailbox->path, &sb) == -1)
    {
      if (errno == ENOENT)
      {
#ifdef USE_COMPRESSED
        if (mutt_comp_can_append(ctx->mailbox))
          ctx->mailbox->magic = MUTT_COMPRESSED;
        else
#endif
          ctx->mailbox->magic = MboxType;
        flags |= MUTT_APPENDNEW;
      }
      else
      {
        mutt_perror(ctx->mailbox->path);
        return -1;
      }
    }
    else
      return -1;
  }

  ctx->mailbox->mx_ops = mx_get_ops(ctx->mailbox->magic);
  if (!ctx->mailbox->mx_ops || !ctx->mailbox->mx_ops->mbox_open_append)
    return -1;

  return ctx->mailbox->mx_ops->mbox_open_append(ctx, flags);
}

/**
 * mx_mbox_open - Open a mailbox and parse it
 * @param m     Mailbox to open
 * @param path  Path to the mailbox
 * @param flags See below
 * @retval ptr  Mailbox context
 * @retval NULL Error
 *
 * flags:
 * * #MUTT_NOSORT   do not sort mailbox
 * * #MUTT_APPEND   open mailbox for appending
 * * #MUTT_READONLY open mailbox in read-only mode
 * * #MUTT_QUIET    only print error messages
 * * #MUTT_PEEK     revert atime where applicable
 */
struct Context *mx_mbox_open(struct Mailbox *m, const char *path, int flags)
{
  if (!path || !path[0])
    return NULL;

  struct Context *ctx = mutt_mem_calloc(1, sizeof(*ctx));
  ctx->mailbox = m;
  if (!ctx->mailbox)
  {
    ctx->mailbox = mailbox_new();
    ctx->mailbox->flags = MB_HIDDEN;
  }

#if 0
  if (!realpath(ctx->mailbox->path, ctx->mailbox->realpath))
  {
    mutt_str_strfcpy(ctx->mailbox->realpath, ctx->mailbox->path,
                     sizeof(ctx->mailbox->realpath));
  }
#endif

  ctx->msgnotreadyet = -1;
  ctx->collapsed = false;

  for (int i = 0; i < RIGHTSMAX; i++)
    mutt_bit_set(ctx->mailbox->rights, i);

  if (flags & MUTT_QUIET)
    ctx->mailbox->quiet = true;
  if (flags & MUTT_READONLY)
    ctx->mailbox->readonly = true;
  if (flags & MUTT_PEEK)
    ctx->peekonly = true;

  if (flags & (MUTT_APPEND | MUTT_NEWFOLDER))
  {
    if (mx_open_mailbox_append(ctx, flags) != 0)
    {
      mx_fastclose_mailbox(ctx);
      mutt_context_free(&ctx);
      return NULL;
    }
    return ctx;
  }

  ctx->mailbox->magic = mx_path_probe(path, NULL);
  ctx->mailbox->mx_ops = mx_get_ops(ctx->mailbox->magic);

  if (ctx->mailbox->path[0] == '\0')
    mutt_str_strfcpy(ctx->mailbox->path, path, sizeof(ctx->mailbox->path));

  if ((ctx->mailbox->magic == MUTT_UNKNOWN) ||
      (ctx->mailbox->magic == MUTT_MAILBOX_ERROR) || !ctx->mailbox->mx_ops)
  {
    if (ctx->mailbox->magic == MUTT_MAILBOX_ERROR)
      mutt_perror(path);
    else if (ctx->mailbox->magic == MUTT_UNKNOWN || !ctx->mailbox->mx_ops)
      mutt_error(_("%s is not a mailbox"), path);

    mx_fastclose_mailbox(ctx);
    mutt_context_free(&ctx);
    return NULL;
  }

  if (!ctx->mailbox->account)
  {
    struct Account *a = account_create();
    a->type = ctx->mailbox->magic;
    TAILQ_INSERT_TAIL(&AllAccounts, a, entries);

    if (mx_ac_add(a, ctx->mailbox) < 0)
    {
      //error
      mailbox_free(&ctx->mailbox);
      return NULL;
    }
  }

  mutt_make_label_hash(ctx->mailbox);

  /* if the user has a `push' command in their .neomuttrc, or in a folder-hook,
   * it will cause the progress messages not to be displayed because
   * mutt_refresh() will think we are in the middle of a macro.  so set a
   * flag to indicate that we should really refresh the screen.
   */
  OptForceRefresh = true;

  if (!ctx->mailbox->quiet)
    mutt_message(_("Reading %s..."), ctx->mailbox->path);

  int rc = ctx->mailbox->mx_ops->mbox_open(ctx);

  if ((rc == 0) || (rc == -2))
  {
    if ((flags & MUTT_NOSORT) == 0)
    {
      /* avoid unnecessary work since the mailbox is completely unthreaded
         to begin with */
      OptSortSubthreads = false;
      OptNeedRescore = false;
      mutt_sort_headers(ctx, true);
    }
    if (!ctx->mailbox->quiet)
      mutt_clear_error();
    if (rc == -2)
      mutt_error(_("Reading from %s interrupted..."), ctx->mailbox->path);
  }
  else
  {
    mx_fastclose_mailbox(ctx);
    mutt_context_free(&ctx);
  }

  OptForceRefresh = false;
  return ctx;
}

/**
 * mx_fastclose_mailbox - free up memory associated with the mailbox context
 * @param ctx Mailbox
 */
void mx_fastclose_mailbox(struct Context *ctx)
{
  if (!ctx)
    return;

  /* never announce that a mailbox we've just left has new mail. #3290
   * TODO: really belongs in mx_mbox_close, but this is a nice hook point */
  if (!ctx->peekonly)
    mutt_mailbox_setnotified(ctx->mailbox->path);

  if (ctx->mailbox->mx_ops)
    ctx->mailbox->mx_ops->mbox_close(ctx);

  mutt_hash_destroy(&ctx->mailbox->subj_hash);
  mutt_hash_destroy(&ctx->mailbox->id_hash);
  mutt_hash_destroy(&ctx->mailbox->label_hash);
  if (ctx->mailbox->hdrs)
  {
    mutt_clear_threads(ctx);
    for (int i = 0; i < ctx->mailbox->msg_count; i++)
      mutt_email_free(&ctx->mailbox->hdrs[i]);
    FREE(&ctx->mailbox->hdrs);
  }
  FREE(&ctx->mailbox->v2r);
  FREE(&ctx->pattern);
  if (ctx->limit_pattern)
    mutt_pattern_free(&ctx->limit_pattern);
  memset(ctx, 0, sizeof(struct Context));
}

/**
 * sync_mailbox - save changes to disk
 * @param ctx        Mailbox
 * @param index_hint Current email
 * @retval  0 Success
 * @retval -1 Failure
 */
static int sync_mailbox(struct Context *ctx, int *index_hint)
{
  if (!ctx->mailbox->mx_ops || !ctx->mailbox->mx_ops->mbox_sync)
    return -1;

  if (!ctx->mailbox->quiet)
  {
    /* L10N: Displayed before/as a mailbox is being synced */
    mutt_message(_("Writing %s..."), ctx->mailbox->path);
  }

  int rc = ctx->mailbox->mx_ops->mbox_sync(ctx, index_hint);
  if ((rc != 0) && !ctx->mailbox->quiet)
  {
    /* L10N: Displayed if a mailbox sync fails */
    mutt_error(_("Unable to write %s"), ctx->mailbox->path);
  }

  return rc;
}

/**
 * trash_append - move deleted mails to the trash folder
 * @param ctx Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
static int trash_append(struct Context *ctx)
{
  int i;
  struct stat st, stc;
  int opt_confappend, rc;

  if (!Trash || !ctx->deleted || (ctx->mailbox->magic == MUTT_MAILDIR && MaildirTrash))
    return 0;

  int delmsgcount = 0;
  int first_del = -1;
  for (i = 0; i < ctx->mailbox->msg_count; i++)
  {
    if (ctx->mailbox->hdrs[i]->deleted && (!ctx->mailbox->hdrs[i]->purge))
    {
      if (first_del < 0)
        first_del = i;
      delmsgcount++;
    }
  }

  if (delmsgcount == 0)
    return 0; /* nothing to be done */

  /* avoid the "append messages" prompt */
  opt_confappend = Confirmappend;
  if (opt_confappend)
    Confirmappend = false;
  rc = mutt_save_confirm(Trash, &st);
  if (opt_confappend)
    Confirmappend = true;
  if (rc != 0)
  {
    /* L10N: Although we know the precise number of messages, we do not show it to the user.
       So feel free to use a "generic plural" as plural translation if your language has one.
     */
    mutt_error(ngettext("message not deleted", "messages not deleted", delmsgcount));
    return -1;
  }

  if (lstat(ctx->mailbox->path, &stc) == 0 && stc.st_ino == st.st_ino &&
      stc.st_dev == st.st_dev && stc.st_rdev == st.st_rdev)
  {
    return 0; /* we are in the trash folder: simple sync */
  }

#ifdef USE_IMAP
  if (Context->mailbox->magic == MUTT_IMAP && (imap_path_probe(Trash, NULL) == MUTT_IMAP))
  {
    if (imap_fast_trash(Context->mailbox, Trash) == 0)
      return 0;
  }
#endif

  struct Context *ctx_trash = mx_mbox_open(NULL, Trash, MUTT_APPEND);
  if (ctx_trash)
  {
    /* continue from initial scan above */
    for (i = first_del; i < ctx->mailbox->msg_count; i++)
    {
      if (ctx->mailbox->hdrs[i]->deleted && (!ctx->mailbox->hdrs[i]->purge))
      {
        if (mutt_append_message(ctx_trash, ctx, ctx->mailbox->hdrs[i], 0, 0) == -1)
        {
          mx_mbox_close(&ctx_trash, NULL);
          return -1;
        }
      }
    }

    mx_mbox_close(&ctx_trash, NULL);
  }
  else
  {
    mutt_error(_("Can't open trash folder"));
    return -1;
  }

  return 0;
}

/**
 * mx_mbox_close - Save changes and close mailbox
 * @param pctx       Mailbox
 * @param index_hint Current email
 * @retval  0 Success
 * @retval -1 Failure
 *
 * @note Context will be freed after it's closed
 */
int mx_mbox_close(struct Context **pctx, int *index_hint)
{
  if (!pctx || !*pctx)
    return 0;

  struct Context *ctx = *pctx;
  int i, move_messages = 0, purge = 1, read_msgs = 0;
  char mbox[PATH_MAX];
  char buf[PATH_MAX + 64];

  ctx->mailbox->closing = true;

  if (ctx->mailbox->readonly || ctx->dontwrite || ctx->append)
  {
    mx_fastclose_mailbox(ctx);
    FREE(pctx);
    return 0;
  }

#ifdef USE_NNTP
  if (ctx->mailbox->msg_unread && ctx->mailbox->magic == MUTT_NNTP)
  {
    struct NntpMboxData *mdata = ctx->mailbox->mdata;

    if (mdata && mdata->adata && mdata->group)
    {
      int rc = query_quadoption(CatchupNewsgroup, _("Mark all articles read?"));
      if (rc == MUTT_ABORT)
      {
        ctx->mailbox->closing = false;
        return -1;
      }
      else if (rc == MUTT_YES)
        mutt_newsgroup_catchup(Context, mdata->adata, mdata->group);
    }
  }
#endif

  for (i = 0; i < ctx->mailbox->msg_count; i++)
  {
    if (!ctx->mailbox->hdrs[i])
      break;
    if (!ctx->mailbox->hdrs[i]->deleted && ctx->mailbox->hdrs[i]->read &&
        !(ctx->mailbox->hdrs[i]->flagged && KeepFlagged))
    {
      read_msgs++;
    }
  }

#ifdef USE_NNTP
  /* don't need to move articles from newsgroup */
  if (ctx->mailbox->magic == MUTT_NNTP)
    read_msgs = 0;
#endif

  if (read_msgs && Move != MUTT_NO)
  {
    int is_spool;
    char *p = mutt_find_hook(MUTT_MBOX_HOOK, ctx->mailbox->path);
    if (p)
    {
      is_spool = 1;
      mutt_str_strfcpy(mbox, p, sizeof(mbox));
    }
    else
    {
      mutt_str_strfcpy(mbox, Mbox, sizeof(mbox));
      is_spool = mutt_is_spool(ctx->mailbox->path) && !mutt_is_spool(mbox);
    }

    if (is_spool && *mbox)
    {
      mutt_expand_path(mbox, sizeof(mbox));
      snprintf(buf, sizeof(buf),
               /* L10N: The first argument is the number of read messages to be
                  moved, the second argument is the target mailbox. */
               ngettext("Move %d read message to %s?", "Move %d read messages to %s?", read_msgs),
               read_msgs, mbox);
      move_messages = query_quadoption(Move, buf);
      if (move_messages == MUTT_ABORT)
      {
        ctx->mailbox->closing = false;
        return -1;
      }
    }
  }

  /* There is no point in asking whether or not to purge if we are
   * just marking messages as "trash".
   */
  if (ctx->deleted && !(ctx->mailbox->magic == MUTT_MAILDIR && MaildirTrash))
  {
    snprintf(buf, sizeof(buf),
             ngettext("Purge %d deleted message?", "Purge %d deleted messages?", ctx->deleted),
             ctx->deleted);
    purge = query_quadoption(Delete, buf);
    if (purge == MUTT_ABORT)
    {
      ctx->mailbox->closing = false;
      return -1;
    }
  }

  if (MarkOld)
  {
    for (i = 0; i < ctx->mailbox->msg_count; i++)
    {
      if (!ctx->mailbox->hdrs[i]->deleted && !ctx->mailbox->hdrs[i]->old &&
          !ctx->mailbox->hdrs[i]->read)
        mutt_set_flag(ctx, ctx->mailbox->hdrs[i], MUTT_OLD, 1);
    }
  }

  if (move_messages)
  {
    if (!ctx->mailbox->quiet)
      mutt_message(_("Moving read messages to %s..."), mbox);

#ifdef USE_IMAP
    /* try to use server-side copy first */
    i = 1;

    if ((ctx->mailbox->magic == MUTT_IMAP) && (imap_path_probe(mbox, NULL) == MUTT_IMAP))
    {
      /* tag messages for moving, and clear old tags, if any */
      for (i = 0; i < ctx->mailbox->msg_count; i++)
      {
        if (ctx->mailbox->hdrs[i]->read && !ctx->mailbox->hdrs[i]->deleted &&
            !(ctx->mailbox->hdrs[i]->flagged && KeepFlagged))
        {
          ctx->mailbox->hdrs[i]->tagged = true;
        }
        else
        {
          ctx->mailbox->hdrs[i]->tagged = false;
        }
      }

      i = imap_copy_messages(ctx, NULL, mbox, true);
    }

    if (i == 0) /* success */
      mutt_clear_error();
    else if (i == -1) /* horrible error, bail */
    {
      ctx->mailbox->closing = false;
      return -1;
    }
    else /* use regular append-copy mode */
#endif
    {
      struct Context *f = mx_mbox_open(NULL, mbox, MUTT_APPEND);
      if (!f)
      {
        ctx->mailbox->closing = false;
        return -1;
      }

      for (i = 0; i < ctx->mailbox->msg_count; i++)
      {
        if (ctx->mailbox->hdrs[i]->read && !ctx->mailbox->hdrs[i]->deleted &&
            !(ctx->mailbox->hdrs[i]->flagged && KeepFlagged))
        {
          if (mutt_append_message(f, ctx, ctx->mailbox->hdrs[i], 0, CH_UPDATE_LEN) == 0)
          {
            mutt_set_flag(ctx, ctx->mailbox->hdrs[i], MUTT_DELETE, 1);
            mutt_set_flag(ctx, ctx->mailbox->hdrs[i], MUTT_PURGE, 1);
          }
          else
          {
            mx_mbox_close(&f, NULL);
            ctx->mailbox->closing = false;
            return -1;
          }
        }
      }

      mx_mbox_close(&f, NULL);
    }
  }
  else if (!ctx->mailbox->changed && ctx->deleted == 0)
  {
    if (!ctx->mailbox->quiet)
      mutt_message(_("Mailbox is unchanged"));
    if (ctx->mailbox->magic == MUTT_MBOX || ctx->mailbox->magic == MUTT_MMDF)
      mbox_reset_atime(ctx->mailbox, NULL);
    mx_fastclose_mailbox(ctx);
    FREE(pctx);
    return 0;
  }

  /* copy mails to the trash before expunging */
  if (purge && ctx->deleted && (mutt_str_strcmp(ctx->mailbox->path, Trash) != 0))
  {
    if (trash_append(ctx) != 0)
    {
      ctx->mailbox->closing = false;
      return -1;
    }
  }

#ifdef USE_IMAP
  /* allow IMAP to preserve the deleted flag across sessions */
  if (ctx->mailbox->magic == MUTT_IMAP)
  {
    int check = imap_sync_mailbox(ctx, (purge != MUTT_NO));
    if (check != 0)
    {
      ctx->mailbox->closing = false;
      return check;
    }
  }
  else
#endif
  {
    if (!purge)
    {
      for (i = 0; i < ctx->mailbox->msg_count; i++)
      {
        ctx->mailbox->hdrs[i]->deleted = false;
        ctx->mailbox->hdrs[i]->purge = false;
      }
      ctx->deleted = 0;
    }

    if (ctx->mailbox->changed || ctx->deleted)
    {
      int check = sync_mailbox(ctx, index_hint);
      if (check != 0)
      {
        ctx->mailbox->closing = false;
        return check;
      }
    }
  }

  if (!ctx->mailbox->quiet)
  {
    if (move_messages)
    {
      mutt_message(_("%d kept, %d moved, %d deleted"),
                   ctx->mailbox->msg_count - ctx->deleted, read_msgs, ctx->deleted);
    }
    else
      mutt_message(_("%d kept, %d deleted"),
                   ctx->mailbox->msg_count - ctx->deleted, ctx->deleted);
  }

  if (ctx->mailbox->msg_count == ctx->deleted &&
      (ctx->mailbox->magic == MUTT_MMDF || ctx->mailbox->magic == MUTT_MBOX) &&
      !mutt_is_spool(ctx->mailbox->path) && !SaveEmpty)
  {
    mutt_file_unlink_empty(ctx->mailbox->path);
  }

#ifdef USE_SIDEBAR
  if (purge && ctx->deleted)
  {
    int orig_msgcount = ctx->mailbox->msg_count;

    for (i = 0; i < ctx->mailbox->msg_count; i++)
    {
      if (ctx->mailbox->hdrs[i]->deleted && !ctx->mailbox->hdrs[i]->read)
        ctx->mailbox->msg_unread--;
      if (ctx->mailbox->hdrs[i]->deleted && ctx->mailbox->hdrs[i]->flagged)
        ctx->mailbox->msg_flagged--;
    }
    ctx->mailbox->msg_count = orig_msgcount;
  }
#endif

  mx_fastclose_mailbox(ctx);
  FREE(pctx);

  return 0;
}

/**
 * mx_update_tables - Update a Context structure's internal tables
 * @param ctx        Mailbox
 * @param committing Commit the changes?
 */
void mx_update_tables(struct Context *ctx, bool committing)
{
  if (!ctx)
    return;

  int i, j, padding;

  /* update memory to reflect the new state of the mailbox */
  ctx->mailbox->vcount = 0;
  ctx->vsize = 0;
  ctx->tagged = 0;
  ctx->deleted = 0;
  ctx->new = 0;
  ctx->mailbox->msg_unread = 0;
  ctx->mailbox->changed = false;
  ctx->mailbox->msg_flagged = 0;
  padding = mx_msg_padding_size(ctx);
  for (i = 0, j = 0; i < ctx->mailbox->msg_count; i++)
  {
    if (!ctx->mailbox->hdrs[i]->quasi_deleted &&
        ((committing && (!ctx->mailbox->hdrs[i]->deleted ||
                         (ctx->mailbox->magic == MUTT_MAILDIR && MaildirTrash))) ||
         (!committing && ctx->mailbox->hdrs[i]->active)))
    {
      if (i != j)
      {
        ctx->mailbox->hdrs[j] = ctx->mailbox->hdrs[i];
        ctx->mailbox->hdrs[i] = NULL;
      }
      ctx->mailbox->hdrs[j]->msgno = j;
      if (ctx->mailbox->hdrs[j]->virtual != -1)
      {
        ctx->mailbox->v2r[ctx->mailbox->vcount] = j;
        ctx->mailbox->hdrs[j]->virtual = ctx->mailbox->vcount++;
        struct Body *b = ctx->mailbox->hdrs[j]->content;
        ctx->vsize += b->length + b->offset - b->hdr_offset + padding;
      }

      if (committing)
        ctx->mailbox->hdrs[j]->changed = false;
      else if (ctx->mailbox->hdrs[j]->changed)
        ctx->mailbox->changed = true;

      if (!committing || (ctx->mailbox->magic == MUTT_MAILDIR && MaildirTrash))
      {
        if (ctx->mailbox->hdrs[j]->deleted)
          ctx->deleted++;
      }

      if (ctx->mailbox->hdrs[j]->tagged)
        ctx->tagged++;
      if (ctx->mailbox->hdrs[j]->flagged)
        ctx->mailbox->msg_flagged++;
      if (!ctx->mailbox->hdrs[j]->read)
      {
        ctx->mailbox->msg_unread++;
        if (!ctx->mailbox->hdrs[j]->old)
          ctx->new ++;
      }

      j++;
    }
    else
    {
      if (ctx->mailbox->magic == MUTT_MH || ctx->mailbox->magic == MUTT_MAILDIR)
      {
        ctx->mailbox->size -= (ctx->mailbox->hdrs[i]->content->length +
                               ctx->mailbox->hdrs[i]->content->offset -
                               ctx->mailbox->hdrs[i]->content->hdr_offset);
      }
      /* remove message from the hash tables */
      if (ctx->mailbox->subj_hash && ctx->mailbox->hdrs[i]->env->real_subj)
        mutt_hash_delete(ctx->mailbox->subj_hash, ctx->mailbox->hdrs[i]->env->real_subj,
                         ctx->mailbox->hdrs[i]);
      if (ctx->mailbox->id_hash && ctx->mailbox->hdrs[i]->env->message_id)
        mutt_hash_delete(ctx->mailbox->id_hash, ctx->mailbox->hdrs[i]->env->message_id,
                         ctx->mailbox->hdrs[i]);
      mutt_label_hash_remove(ctx->mailbox, ctx->mailbox->hdrs[i]);
      /* The path mx_mbox_check() -> imap_check_mailbox() ->
       *          imap_expunge_mailbox() -> mx_update_tables()
       * can occur before a call to mx_mbox_sync(), resulting in
       * last_tag being stale if it's not reset here.
       */
      if (ctx->last_tag == ctx->mailbox->hdrs[i])
        ctx->last_tag = NULL;
      mutt_email_free(&ctx->mailbox->hdrs[i]);
    }
  }
  ctx->mailbox->msg_count = j;
}

/**
 * mx_mbox_sync - Save changes to mailbox
 * @param[in]  ctx        Context
 * @param[out] index_hint Currently selected mailbox
 * @retval  0 Success
 * @retval -1 Error
 */
int mx_mbox_sync(struct Context *ctx, int *index_hint)
{
  int rc;
  int purge = 1;
  int msgcount, deleted;

  if (ctx->dontwrite)
  {
    char buf[STRING], tmp[STRING];
    if (km_expand_key(buf, sizeof(buf), km_find_func(MENU_MAIN, OP_TOGGLE_WRITE)))
      snprintf(tmp, sizeof(tmp), _(" Press '%s' to toggle write"), buf);
    else
      mutt_str_strfcpy(tmp, _("Use 'toggle-write' to re-enable write"), sizeof(tmp));

    mutt_error(_("Mailbox is marked unwritable. %s"), tmp);
    return -1;
  }
  else if (ctx->mailbox->readonly)
  {
    mutt_error(_("Mailbox is read-only"));
    return -1;
  }

  if (!ctx->mailbox->changed && !ctx->deleted)
  {
    if (!ctx->mailbox->quiet)
      mutt_message(_("Mailbox is unchanged"));
    return 0;
  }

  if (ctx->deleted)
  {
    char buf[SHORT_STRING];

    snprintf(buf, sizeof(buf),
             ngettext("Purge %d deleted message?", "Purge %d deleted messages?", ctx->deleted),
             ctx->deleted);
    purge = query_quadoption(Delete, buf);
    if (purge == MUTT_ABORT)
      return -1;
    else if (purge == MUTT_NO)
    {
      if (!ctx->mailbox->changed)
        return 0; /* nothing to do! */
      /* let IMAP servers hold on to D flags */
      if (ctx->mailbox->magic != MUTT_IMAP)
      {
        for (int i = 0; i < ctx->mailbox->msg_count; i++)
        {
          ctx->mailbox->hdrs[i]->deleted = false;
          ctx->mailbox->hdrs[i]->purge = false;
        }
        ctx->deleted = 0;
      }
    }
    else if (ctx->last_tag && ctx->last_tag->deleted)
      ctx->last_tag = NULL; /* reset last tagged msg now useless */
  }

  /* really only for IMAP - imap_sync_mailbox results in a call to
   * mx_update_tables, so ctx->deleted is 0 when it comes back */
  msgcount = ctx->mailbox->msg_count;
  deleted = ctx->deleted;

  if (purge && ctx->deleted && (mutt_str_strcmp(ctx->mailbox->path, Trash) != 0))
  {
    if (trash_append(ctx) != 0)
      return -1;
  }

#ifdef USE_IMAP
  if (ctx->mailbox->magic == MUTT_IMAP)
    rc = imap_sync_mailbox(ctx, purge);
  else
#endif
    rc = sync_mailbox(ctx, index_hint);
  if (rc == 0)
  {
#ifdef USE_IMAP
    if (ctx->mailbox->magic == MUTT_IMAP && !purge)
    {
      if (!ctx->mailbox->quiet)
        mutt_message(_("Mailbox checkpointed"));
    }
    else
#endif
    {
      if (!ctx->mailbox->quiet)
        mutt_message(_("%d kept, %d deleted"), msgcount - deleted, deleted);
    }

    mutt_sleep(0);

    if (ctx->mailbox->msg_count == ctx->deleted &&
        (ctx->mailbox->magic == MUTT_MBOX || ctx->mailbox->magic == MUTT_MMDF) &&
        !mutt_is_spool(ctx->mailbox->path) && !SaveEmpty)
    {
      unlink(ctx->mailbox->path);
      mx_fastclose_mailbox(ctx);
      return 0;
    }

    /* if we haven't deleted any messages, we don't need to resort */
    /* ... except for certain folder formats which need "unsorted"
     * sort order in order to synchronize folders.
     *
     * MH and maildir are safe.  mbox-style seems to need re-sorting,
     * at least with the new threading code.
     */
    if (purge || (ctx->mailbox->magic != MUTT_MAILDIR && ctx->mailbox->magic != MUTT_MH))
    {
      /* IMAP does this automatically after handling EXPUNGE */
      if (ctx->mailbox->magic != MUTT_IMAP)
      {
        mx_update_tables(ctx, true);
        mutt_sort_headers(ctx, true); /* rethread from scratch */
      }
    }
  }

  return rc;
}

/**
 * mx_msg_open_new - Open a new message
 * @param ctx   Destination mailbox
 * @param e   Message being copied (required for maildir support, because the filename depends on the message flags)
 * @param flags Flags, e.g. #MUTT_SET_DRAFT
 * @retval ptr New Message
 */
struct Message *mx_msg_open_new(struct Context *ctx, struct Email *e, int flags)
{
  struct Address *p = NULL;
  struct Message *msg = NULL;

  if (!ctx->mailbox->mx_ops || !ctx->mailbox->mx_ops->msg_open_new)
  {
    mutt_debug(1, "function unimplemented for mailbox type %d.\n", ctx->mailbox->magic);
    return NULL;
  }

  msg = mutt_mem_calloc(1, sizeof(struct Message));
  msg->write = true;

  if (e)
  {
    msg->flags.flagged = e->flagged;
    msg->flags.replied = e->replied;
    msg->flags.read = e->read;
    msg->flags.draft = (flags & MUTT_SET_DRAFT) ? true : false;
    msg->received = e->received;
  }

  if (msg->received == 0)
    time(&msg->received);

  if (ctx->mailbox->mx_ops->msg_open_new(ctx, msg, e) == 0)
  {
    if (ctx->mailbox->magic == MUTT_MMDF)
      fputs(MMDF_SEP, msg->fp);

    if ((ctx->mailbox->magic == MUTT_MBOX || ctx->mailbox->magic == MUTT_MMDF) &&
        flags & MUTT_ADD_FROM)
    {
      if (e)
      {
        if (e->env->return_path)
          p = e->env->return_path;
        else if (e->env->sender)
          p = e->env->sender;
        else
          p = e->env->from;
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
 * mx_mbox_check - check for new mail
 * @param ctx        Mailbox
 * @param index_hint Current email
 * @retval >0 Success, e.g. #MUTT_NEW_MAIL
 * @retval  0 Success, no change
 * @retval -1 Failure
 */
int mx_mbox_check(struct Context *ctx, int *index_hint)
{
  if (!ctx || !ctx->mailbox->mx_ops)
  {
    mutt_debug(1, "null or invalid context.\n");
    return -1;
  }

  return ctx->mailbox->mx_ops->mbox_check(ctx, index_hint);
}

/**
 * mx_msg_open - return a stream pointer for a message
 * @param ctx   Mailbox
 * @param msgno Message number
 * @retval ptr  Message
 * @retval NULL Error
 */
struct Message *mx_msg_open(struct Context *ctx, int msgno)
{
  struct Message *msg = NULL;

  if (!ctx->mailbox->mx_ops || !ctx->mailbox->mx_ops->msg_open)
  {
    mutt_debug(1, "function not implemented for mailbox type %d.\n",
               ctx->mailbox->magic);
    return NULL;
  }

  msg = mutt_mem_calloc(1, sizeof(struct Message));
  if (ctx->mailbox->mx_ops->msg_open(ctx, msg, msgno))
    FREE(&msg);

  return msg;
}

/**
 * mx_msg_commit - commit a message to a folder
 * @param msg Message to commit
 * @param ctx Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_msg_commit(struct Context *ctx, struct Message *msg)
{
  if (!ctx->mailbox->mx_ops || !ctx->mailbox->mx_ops->msg_commit)
    return -1;

  if (!(msg->write && ctx->append))
  {
    mutt_debug(1, "msg->write = %d, ctx->append = %d\n", msg->write, ctx->append);
    return -1;
  }

  return ctx->mailbox->mx_ops->msg_commit(ctx, msg);
}

/**
 * mx_msg_close - Close a message
 * @param ctx Mailbox
 * @param msg Message to close
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_msg_close(struct Context *ctx, struct Message **msg)
{
  if (!ctx || !msg || !*msg)
    return 0;
  int r = 0;

  if (ctx->mailbox->mx_ops && ctx->mailbox->mx_ops->msg_close)
    r = ctx->mailbox->mx_ops->msg_close(ctx, *msg);

  if ((*msg)->path)
  {
    mutt_debug(1, "unlinking %s\n", (*msg)->path);
    unlink((*msg)->path);
    FREE(&(*msg)->path);
  }

  FREE(&(*msg)->committed_path);
  FREE(msg);
  return r;
}

/**
 * mx_alloc_memory - Create storage for the emails
 * @param m Mailbox
 */
void mx_alloc_memory(struct Mailbox *m)
{
  size_t s = MAX(sizeof(struct Email *), sizeof(int));

  if ((m->hdrmax + 25) * s < m->hdrmax * s)
  {
    mutt_error(_("Out of memory"));
    mutt_exit(1);
  }

  if (m->hdrs)
  {
    mutt_mem_realloc(&m->hdrs, sizeof(struct Email *) * (m->hdrmax += 25));
    mutt_mem_realloc(&m->v2r, sizeof(int) * m->hdrmax);
  }
  else
  {
    m->hdrs = mutt_mem_calloc((m->hdrmax += 25), sizeof(struct Email *));
    m->v2r = mutt_mem_calloc(m->hdrmax, sizeof(int));
  }
  for (int i = m->msg_count; i < m->hdrmax; i++)
  {
    m->hdrs[i] = NULL;
    m->v2r[i] = -1;
  }
}

/**
 * mx_update_context - Update the Context's message counts
 * @param ctx          Mailbox
 * @param new_messages Number of new messages
 *
 * this routine is called to update the counts in the context structure for the
 * last message header parsed.
 */
void mx_update_context(struct Context *ctx, int new_messages)
{
  struct Email *e = NULL;
  for (int msgno = ctx->mailbox->msg_count - new_messages;
       msgno < ctx->mailbox->msg_count; msgno++)
  {
    e = ctx->mailbox->hdrs[msgno];

    if (WithCrypto)
    {
      /* NOTE: this _must_ be done before the check for mailcap! */
      e->security = crypt_query(e->content);
    }

    if (!ctx->pattern)
    {
      ctx->mailbox->v2r[ctx->mailbox->vcount] = msgno;
      e->virtual = ctx->mailbox->vcount++;
    }
    else
      e->virtual = -1;
    e->msgno = msgno;

    if (e->env->supersedes)
    {
      struct Email *e2 = NULL;

      if (!ctx->mailbox->id_hash)
        ctx->mailbox->id_hash = mutt_make_id_hash(ctx->mailbox);

      e2 = mutt_hash_find(ctx->mailbox->id_hash, e->env->supersedes);
      if (e2)
      {
        e2->superseded = true;
        if (Score)
          mutt_score_message(ctx, e2, true);
      }
    }

    /* add this message to the hash tables */
    if (ctx->mailbox->id_hash && e->env->message_id)
      mutt_hash_insert(ctx->mailbox->id_hash, e->env->message_id, e);
    if (ctx->mailbox->subj_hash && e->env->real_subj)
      mutt_hash_insert(ctx->mailbox->subj_hash, e->env->real_subj, e);
    mutt_label_hash_add(ctx->mailbox, e);

    if (Score)
      mutt_score_message(ctx, e, false);

    if (e->changed)
      ctx->mailbox->changed = true;
    if (e->flagged)
      ctx->mailbox->msg_flagged++;
    if (e->deleted)
      ctx->deleted++;
    if (!e->read)
    {
      ctx->mailbox->msg_unread++;
      if (!e->old)
        ctx->new ++;
    }
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
      bool passive = ImapPassive;
      ImapPassive = false;
      int rv = imap_status(path, false);
      ImapPassive = passive;
      return (rv <= 0);
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
 * @param ctx    Mailbox
 * @param tags   Existing tags
 * @param buf    Buffer for the results
 * @param buflen Length of the buffer
 * @retval -1 Error
 * @retval 0  No valid user input
 * @retval 1  Buffer set
 */
int mx_tags_edit(struct Context *ctx, const char *tags, char *buf, size_t buflen)
{
  if (ctx->mailbox->mx_ops->tags_edit)
    return ctx->mailbox->mx_ops->tags_edit(ctx, tags, buf, buflen);

  mutt_message(_("Folder doesn't support tagging, aborting"));
  return -1;
}

/**
 * mx_tags_commit - save tags to the mailbox
 * @param ctx  Mailbox
 * @param e  Email Header
 * @param tags Tags to save
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_tags_commit(struct Context *ctx, struct Email *e, char *tags)
{
  if (ctx->mailbox->mx_ops->tags_commit)
    return ctx->mailbox->mx_ops->tags_commit(ctx, e, tags);

  mutt_message(_("Folder doesn't support tagging, aborting"));
  return -1;
}

/**
 * mx_tags_is_supported - return true if mailbox support tagging
 * @param ctx Mailbox
 * @retval true Tagging is supported
 */
bool mx_tags_is_supported(struct Context *ctx)
{
  return ctx->mailbox->mx_ops->tags_commit && ctx->mailbox->mx_ops->tags_edit;
}

/**
 * mx_path_probe - Find a mailbox that understands a path
 * @param[in]  path  Path to examine
 * @param[out] st    stat buffer (OPTIONAL, for local mailboxes)
 * @retval num Type, e.g. #MUTT_IMAP
 */
int mx_path_probe(const char *path, struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  static const struct MxOps *no_stat[] = {
#ifdef USE_IMAP
    &mx_imap_ops,
#endif
#ifdef USE_NOTMUCH
    &mx_notmuch_ops,
#endif
#ifdef USE_POP
    &mx_pop_ops,
#endif
#ifdef USE_NNTP
    &mx_nntp_ops,
#endif
  };

  static const struct MxOps *with_stat[] = {
    &mx_maildir_ops, &mx_mbox_ops, &mx_mh_ops, &mx_mmdf_ops,
#ifdef USE_COMPRESSED
    &mx_comp_ops,
#endif
  };

  int rc;

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
    mutt_debug(1, "unable to stat %s: %s (errno %d).\n", path, strerror(errno), errno);
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
int mx_path_canon(char *buf, size_t buflen, const char *folder, int *magic)
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
    else if ((buf[1] == '/') || (buf[1] == '\0'))
    {
      if (buf[0] == '!')
      {
        mutt_str_inline_replace(buf, buflen, 1, Spoolfile);
      }
      else if (buf[0] == '-')
      {
        mutt_str_inline_replace(buf, buflen, 1, LastFolder);
      }
      else if (buf[0] == '<')
      {
        mutt_str_inline_replace(buf, buflen, 1, Record);
      }
      else if (buf[0] == '>')
      {
        mutt_str_inline_replace(buf, buflen, 1, Mbox);
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
    else if ((buf[0] == '+') || (buf[0] == '='))
    {
      buf[0] = '/';
      mutt_str_inline_replace(buf, buflen, 0, Folder);
    }
    else if (buf[0] == '@')
    {
      /* elm compatibility, @ expands alias to user name */
      struct Address *alias = mutt_alias_lookup(buf + 1);
      if (!alias)
        break;

      struct Email *e = mutt_email_new();
      e->env = mutt_env_new();
      e->env->from = alias;
      e->env->to = alias;
      mutt_default_save(buf, buflen, e);
      e->env->from = NULL;
      e->env->to = NULL;
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

  int magic2 = mx_path_probe(buf, NULL);
  if (magic)
    *magic = magic2;
  const struct MxOps *ops = mx_get_ops(magic2);
  if (!ops || !ops->path_canon)
    return -1;

  if (ops->path_canon(buf, buflen, folder) < 0)
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

  if (m->realpath[0] == '\0')
    mutt_str_strfcpy(m->realpath, m->path, sizeof(m->realpath));

  int rc = mx_path_canon(m->realpath, sizeof(m->realpath), folder, &m->magic);
  if (rc >= 0)
  {
    m->mx_ops = mx_get_ops(m->magic);
    // temp?
    mutt_str_strfcpy(m->path, m->realpath, sizeof(m->path));
  }

  return rc;
}

/**
 * mx_path_pretty - Abbreviate a mailbox path - Wrapper for MxOps::path_pretty
 */
int mx_path_pretty(char *buf, size_t buflen, const char *folder)
{
  int magic = mx_path_probe(buf, NULL);
  const struct MxOps *ops = mx_get_ops(magic);
  if (!ops)
    return -1;

  if (!ops->path_canon)
    return -1;

  if (ops->path_canon(buf, buflen, folder) < 0)
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
 * @param ctx Mailbox
 * @retval num Number of bytes of padding
 *
 * mmdf and mbox add separators, which leads a small discrepancy when computing
 * vsize for a limited view.
 */
int mx_msg_padding_size(struct Context *ctx)
{
  if (!ctx->mailbox->mx_ops || !ctx->mailbox->mx_ops->msg_padding_size)
    return 0;

  return ctx->mailbox->mx_ops->msg_padding_size(ctx);
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
    if (np->type != m->magic)
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
struct Mailbox *mx_mbox_find(struct Account *a, struct Mailbox *m)
{
  if (!a || !m)
    return NULL;

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &a->mailboxes, entries)
  {
    if (mutt_str_strcmp(np->m->realpath, m->realpath) == 0)
      return np->m;
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
  mx_path_canon(buf, sizeof(buf), Folder, NULL);

  struct MailboxNode *np = NULL;
  STAILQ_FOREACH(np, &AllMailboxes, entries)
  {
    if (mutt_str_strcmp(np->m->realpath, buf) == 0)
      return np->m;
  }

  return NULL;
}

/**
 * mx_ac_add - Add a Mailbox to an Account - Wrapper for MxOps::ac_add
 */
int mx_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m || !m->mx_ops || !m->mx_ops->ac_add)
    return -1;

  return m->mx_ops->ac_add(a, m);
}
