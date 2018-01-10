/**
 * @file
 * Mailbox multiplexor
 *
 * @authors
 * Copyright (C) 1996-2002,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2003 Thomas Roessler <roessler@does-not-exist.org>
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "mutt/mutt.h"
#include "email/email.h"
#include "mutt.h"
#include "mx.h"
#include "buffy.h"
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
unsigned char CatchupNewsgroup;
bool KeepFlagged;
unsigned char Move;
char *Trash;

/**
 * mx_get_ops - Get mailbox operations
 * @param magic Mailbox magic number
 * @retval ptr  Mailbox function
 * @retval NULL Error
 */
struct MxOps *mx_get_ops(int magic)
{
  switch (magic)
  {
#ifdef USE_IMAP
    case MUTT_IMAP:
      return &mx_imap_ops;
#endif
    case MUTT_MAILDIR:
      return &mx_maildir_ops;
    case MUTT_MBOX:
      return &mx_mbox_ops;
    case MUTT_MH:
      return &mx_mh_ops;
    case MUTT_MMDF:
      return &mx_mmdf_ops;
#ifdef USE_POP
    case MUTT_POP:
      return &mx_pop_ops;
#endif
#ifdef USE_COMPRESSED
    case MUTT_COMPRESSED:
      return &mx_comp_ops;
#endif
#ifdef USE_NNTP
    case MUTT_NNTP:
      return &mx_nntp_ops;
#endif
#ifdef USE_NOTMUCH
    case MUTT_NOTMUCH:
      return &mx_notmuch_ops;
#endif
    default:
      return NULL;
  }
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

#ifdef USE_IMAP

/**
 * mx_is_imap - Is this an IMAP mailbox
 * @param p Mailbox string to test
 * @retval true It is an IMAP mailbox
 */
bool mx_is_imap(const char *p)
{
  enum UrlScheme scheme;

  if (!p)
    return false;

  if (*p == '{')
    return true;

  scheme = url_check_scheme(p);
  if (scheme == U_IMAP || scheme == U_IMAPS)
    return true;

  return false;
}

#endif

#ifdef USE_POP
/**
 * mx_is_pop - Is this a POP mailbox
 * @param p Mailbox string to test
 * @retval true It is a POP mailbox
 */
bool mx_is_pop(const char *p)
{
  enum UrlScheme scheme;

  if (!p)
    return false;

  scheme = url_check_scheme(p);
  if (scheme == U_POP || scheme == U_POPS)
    return true;

  return false;
}
#endif

#ifdef USE_NNTP
/**
 * mx_is_nntp - Is this an NNTP mailbox
 * @param p Mailbox string to test
 * @retval true It is an NNTP mailbox
 */
bool mx_is_nntp(const char *p)
{
  enum UrlScheme scheme;

  if (!p)
    return false;

  scheme = url_check_scheme(p);
  if (scheme == U_NNTP || scheme == U_NNTPS)
    return true;

  return false;
}
#endif

#ifdef USE_NOTMUCH
/**
 * mx_is_notmuch - Is this a Notmuch mailbox
 * @param p Mailbox string to test
 * @retval true It is a Notmuch mailbox
 */
bool mx_is_notmuch(const char *p)
{
  enum UrlScheme scheme;

  if (!p)
    return false;

  scheme = url_check_scheme(p);
  if (scheme == U_NOTMUCH)
    return true;

  return false;
}
#endif

/**
 * mx_get_magic - Identify the type of mailbox
 * @param path Mailbox path to test
 * @retval -1 Error, can't identify mailbox
 * @retval >0 Success, e.g. #MUTT_IMAP
 */
int mx_get_magic(const char *path)
{
  struct stat st;
  int magic = 0;
  FILE *f = NULL;

#ifdef USE_IMAP
  if (mx_is_imap(path))
    return MUTT_IMAP;
#endif /* USE_IMAP */

#ifdef USE_POP
  if (mx_is_pop(path))
    return MUTT_POP;
#endif /* USE_POP */

#ifdef USE_NNTP
  if (mx_is_nntp(path))
    return MUTT_NNTP;
#endif /* USE_NNTP */

#ifdef USE_NOTMUCH
  if (mx_is_notmuch(path))
    return MUTT_NOTMUCH;
#endif

  if (stat(path, &st) == -1)
  {
    mutt_debug(1, "unable to stat %s: %s (errno %d).\n", path, strerror(errno), errno);
    return -1;
  }

  if (S_ISDIR(st.st_mode))
  {
    /* check for maildir-style mailbox */
    if (mx_is_maildir(path))
      return MUTT_MAILDIR;

    /* check for mh-style mailbox */
    if (mx_is_mh(path))
      return MUTT_MH;
  }
  else if (st.st_size == 0)
  {
    /* hard to tell what zero-length files are, so assume the default magic */
    if (MboxType == MUTT_MBOX || MboxType == MUTT_MMDF)
      return MboxType;
    else
      return MUTT_MBOX;
  }
  else if ((f = fopen(path, "r")) != NULL)
  {
    struct utimbuf times;
    int ch;

    /* Some mailbox creation tools erroneously append a blank line to
     * a file before appending a mail message.  This allows neomutt to
     * detect magic for and thus open those files. */
    while ((ch = fgetc(f)) != EOF)
    {
      if (ch != '\n' && ch != '\r')
      {
        ungetc(ch, f);
        break;
      }
    }

    char tmp[STRING];
    if (fgets(tmp, sizeof(tmp), f))
    {
      if (mutt_str_strncmp("From ", tmp, 5) == 0)
        magic = MUTT_MBOX;
      else if (mutt_str_strcmp(MMDF_SEP, tmp) == 0)
        magic = MUTT_MMDF;
    }
    mutt_file_fclose(&f);

    if (!CheckMboxSize)
    {
      /* need to restore the times here, the file was not really accessed,
       * only the type was accessed.  This is important, because detection
       * of "new mail" depends on those times set correctly.
       */
      times.actime = st.st_atime;
      times.modtime = st.st_mtime;
      utime(path, &times);
    }
  }
  else
  {
    mutt_debug(1, "unable to open file %s for reading.\n", path);
    return -1;
  }

#ifdef USE_COMPRESSED
  /* If there are no other matches, see if there are any
   * compress hooks that match */
  if ((magic == 0) && mutt_comp_can_read(path))
    return MUTT_COMPRESSED;
#endif
  return magic;
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
  if (mx_is_imap(path))
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
  ctx->magic = mx_get_magic(ctx->path);
  if (ctx->magic == 0)
  {
    mutt_error(_("%s is not a mailbox."), ctx->path);
    return -1;
  }

  if (ctx->magic < 0)
  {
    if (stat(ctx->path, &sb) == -1)
    {
      if (errno == ENOENT)
      {
#ifdef USE_COMPRESSED
        if (mutt_comp_can_append(ctx))
          ctx->magic = MUTT_COMPRESSED;
        else
#endif
          ctx->magic = MboxType;
        flags |= MUTT_APPENDNEW;
      }
      else
      {
        mutt_perror(ctx->path);
        return -1;
      }
    }
    else
      return -1;
  }

  ctx->mx_ops = mx_get_ops(ctx->magic);
  if (!ctx->mx_ops || !ctx->mx_ops->mbox_open_append)
    return -1;

  return ctx->mx_ops->mbox_open_append(ctx, flags);
}

/**
 * mx_mbox_open - Open a mailbox and parse it
 * @param path  Path to the mailbox
 * @param flags See below
 * @param pctx  Reuse this Context (if supplied)
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
struct Context *mx_mbox_open(const char *path, int flags, struct Context *pctx)
{
  struct Context *ctx = pctx;
  int rc;

  if (!path || !path[0])
    return NULL;

  if (!ctx)
    ctx = mutt_mem_malloc(sizeof(struct Context));
  memset(ctx, 0, sizeof(struct Context));

  ctx->path = mutt_str_strdup(path);
  if (!ctx->path)
  {
    if (!pctx)
      FREE(&ctx);
    return NULL;
  }
  ctx->realpath = realpath(ctx->path, NULL);
  if (!ctx->realpath)
    ctx->realpath = mutt_str_strdup(ctx->path);

  ctx->msgnotreadyet = -1;
  ctx->collapsed = false;

  for (rc = 0; rc < RIGHTSMAX; rc++)
    mutt_bit_set(ctx->rights, rc);

  if (flags & MUTT_QUIET)
    ctx->quiet = true;
  if (flags & MUTT_READONLY)
    ctx->readonly = true;
  if (flags & MUTT_PEEK)
    ctx->peekonly = true;

  if (flags & (MUTT_APPEND | MUTT_NEWFOLDER))
  {
    if (mx_open_mailbox_append(ctx, flags) != 0)
    {
      mx_fastclose_mailbox(ctx);
      if (!pctx)
        FREE(&ctx);
      return NULL;
    }
    return ctx;
  }

  ctx->magic = mx_get_magic(path);
  ctx->mx_ops = mx_get_ops(ctx->magic);

  if (ctx->magic <= 0 || !ctx->mx_ops)
  {
    if (ctx->magic == -1)
      mutt_perror(path);
    else if (ctx->magic == 0 || !ctx->mx_ops)
      mutt_error(_("%s is not a mailbox."), path);

    mx_fastclose_mailbox(ctx);
    if (!pctx)
      FREE(&ctx);
    return NULL;
  }

  mutt_make_label_hash(ctx);

  /* if the user has a `push' command in their .neomuttrc, or in a folder-hook,
   * it will cause the progress messages not to be displayed because
   * mutt_refresh() will think we are in the middle of a macro.  so set a
   * flag to indicate that we should really refresh the screen.
   */
  OptForceRefresh = true;

  if (!ctx->quiet)
    mutt_message(_("Reading %s..."), ctx->path);

  rc = ctx->mx_ops->mbox_open(ctx);

  if ((rc == 0) || (rc == -2))
  {
    if ((flags & MUTT_NOSORT) == 0)
    {
      /* avoid unnecessary work since the mailbox is completely unthreaded
         to begin with */
      OptSortSubthreads = false;
      OptNeedRescore = false;
      mutt_sort_headers(ctx, 1);
    }
    if (!ctx->quiet)
      mutt_clear_error();
    if (rc == -2)
      mutt_error(_("Reading from %s interrupted..."), ctx->path);
  }
  else
  {
    mx_fastclose_mailbox(ctx);
    if (!pctx)
      FREE(&ctx);
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
  struct utimbuf ut;

  if (!ctx)
    return;

  /* fix up the times so buffy won't get confused */
  if (ctx->peekonly && ctx->path && (ctx->mtime > ctx->atime))
  {
    ut.actime = ctx->atime;
    ut.modtime = ctx->mtime;
    utime(ctx->path, &ut);
  }

  /* never announce that a mailbox we've just left has new mail. #3290
   * TODO: really belongs in mx_mbox_close, but this is a nice hook point */
  if (!ctx->peekonly)
    mutt_buffy_setnotified(ctx->path);

  if (ctx->mx_ops)
    ctx->mx_ops->mbox_close(ctx);

  if (ctx->subj_hash)
    mutt_hash_destroy(&ctx->subj_hash);
  if (ctx->id_hash)
    mutt_hash_destroy(&ctx->id_hash);
  mutt_hash_destroy(&ctx->label_hash);
  mutt_clear_threads(ctx);
  for (int i = 0; i < ctx->msgcount; i++)
    mutt_header_free(&ctx->hdrs[i]);
  FREE(&ctx->hdrs);
  FREE(&ctx->v2r);
  FREE(&ctx->path);
  FREE(&ctx->realpath);
  FREE(&ctx->pattern);
  if (ctx->limit_pattern)
    mutt_pattern_free(&ctx->limit_pattern);
  mutt_file_fclose(&ctx->fp);
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
  if (!ctx->mx_ops || !ctx->mx_ops->mbox_sync)
    return -1;

  if (!ctx->quiet)
    mutt_message(_("Writing %s..."), ctx->path);

  return ctx->mx_ops->mbox_sync(ctx, index_hint);
}

/**
 * trash_append - move deleted mails to the trash folder
 * @param ctx Mailbox
 * @retval  0 Success
 * @retval -1 Failure
 */
static int trash_append(struct Context *ctx)
{
  struct Context ctx_trash;
  int i;
  struct stat st, stc;
  int opt_confappend, rc;

  if (!Trash || !ctx->deleted || (ctx->magic == MUTT_MAILDIR && MaildirTrash))
    return 0;

  int delmsgcount = 0;
  int first_del = -1;
  for (i = 0; i < ctx->msgcount; i++)
  {
    if (ctx->hdrs[i]->deleted && (!ctx->hdrs[i]->purge))
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

  if (lstat(ctx->path, &stc) == 0 && stc.st_ino == st.st_ino &&
      stc.st_dev == st.st_dev && stc.st_rdev == st.st_rdev)
  {
    return 0; /* we are in the trash folder: simple sync */
  }

#ifdef USE_IMAP
  if (Context->magic == MUTT_IMAP && mx_is_imap(Trash))
  {
    if (imap_fast_trash(Context, Trash) == 0)
      return 0;
  }
#endif

  if (mx_mbox_open(Trash, MUTT_APPEND, &ctx_trash) != NULL)
  {
    /* continue from initial scan above */
    for (i = first_del; i < ctx->msgcount; i++)
    {
      if (ctx->hdrs[i]->deleted && (!ctx->hdrs[i]->purge))
      {
        if (mutt_append_message(&ctx_trash, ctx, ctx->hdrs[i], 0, 0) == -1)
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
 * mx_mbox_close - save changes and close mailbox
 * @param ctx        Mailbox
 * @param index_hint Current email
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_mbox_close(struct Context *ctx, int *index_hint)
{
  int i, move_messages = 0, purge = 1, read_msgs = 0;
  struct Context f;
  char mbox[PATH_MAX];
  char buf[PATH_MAX + 64];

  if (!ctx)
    return 0;

  ctx->closing = true;

  if (ctx->readonly || ctx->dontwrite || ctx->append)
  {
    mx_fastclose_mailbox(ctx);
    return 0;
  }

#ifdef USE_NNTP
  if (ctx->unread && ctx->magic == MUTT_NNTP)
  {
    struct NntpData *nntp_data = ctx->data;

    if (nntp_data && nntp_data->nserv && nntp_data->group)
    {
      int rc = query_quadoption(CatchupNewsgroup, _("Mark all articles read?"));
      if (rc == MUTT_ABORT)
      {
        ctx->closing = false;
        return -1;
      }
      else if (rc == MUTT_YES)
        mutt_newsgroup_catchup(nntp_data->nserv, nntp_data->group);
    }
  }
#endif

  for (i = 0; i < ctx->msgcount; i++)
  {
    if (!ctx->hdrs[i]->deleted && ctx->hdrs[i]->read && !(ctx->hdrs[i]->flagged && KeepFlagged))
    {
      read_msgs++;
    }
  }

#ifdef USE_NNTP
  /* don't need to move articles from newsgroup */
  if (ctx->magic == MUTT_NNTP)
    read_msgs = 0;
#endif

  if (read_msgs && Move != MUTT_NO)
  {
    int is_spool;
    char *p = mutt_find_hook(MUTT_MBOX_HOOK, ctx->path);
    if (p)
    {
      is_spool = 1;
      mutt_str_strfcpy(mbox, p, sizeof(mbox));
    }
    else
    {
      mutt_str_strfcpy(mbox, NONULL(Mbox), sizeof(mbox));
      is_spool = mutt_is_spool(ctx->path) && !mutt_is_spool(mbox);
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
        ctx->closing = false;
        return -1;
      }
    }
  }

  /* There is no point in asking whether or not to purge if we are
   * just marking messages as "trash".
   */
  if (ctx->deleted && !(ctx->magic == MUTT_MAILDIR && MaildirTrash))
  {
    snprintf(buf, sizeof(buf),
             ngettext("Purge %d deleted message?", "Purge %d deleted messages?", ctx->deleted),
             ctx->deleted);
    purge = query_quadoption(Delete, buf);
    if (purge == MUTT_ABORT)
    {
      ctx->closing = false;
      return -1;
    }
  }

  if (MarkOld)
  {
    for (i = 0; i < ctx->msgcount; i++)
    {
      if (!ctx->hdrs[i]->deleted && !ctx->hdrs[i]->old && !ctx->hdrs[i]->read)
        mutt_set_flag(ctx, ctx->hdrs[i], MUTT_OLD, 1);
    }
  }

  if (move_messages)
  {
    if (!ctx->quiet)
      mutt_message(_("Moving read messages to %s..."), mbox);

#ifdef USE_IMAP
    /* try to use server-side copy first */
    i = 1;

    if (ctx->magic == MUTT_IMAP && mx_is_imap(mbox))
    {
      /* tag messages for moving, and clear old tags, if any */
      for (i = 0; i < ctx->msgcount; i++)
      {
        if (ctx->hdrs[i]->read && !ctx->hdrs[i]->deleted &&
            !(ctx->hdrs[i]->flagged && KeepFlagged))
        {
          ctx->hdrs[i]->tagged = true;
        }
        else
        {
          ctx->hdrs[i]->tagged = false;
        }
      }

      i = imap_copy_messages(ctx, NULL, mbox, true);
    }

    if (i == 0) /* success */
      mutt_clear_error();
    else if (i == -1) /* horrible error, bail */
    {
      ctx->closing = false;
      return -1;
    }
    else /* use regular append-copy mode */
#endif
    {
      if (mx_mbox_open(mbox, MUTT_APPEND, &f) == NULL)
      {
        ctx->closing = false;
        return -1;
      }

      for (i = 0; i < ctx->msgcount; i++)
      {
        if (ctx->hdrs[i]->read && !ctx->hdrs[i]->deleted &&
            !(ctx->hdrs[i]->flagged && KeepFlagged))
        {
          if (mutt_append_message(&f, ctx, ctx->hdrs[i], 0, CH_UPDATE_LEN) == 0)
          {
            mutt_set_flag(ctx, ctx->hdrs[i], MUTT_DELETE, 1);
            mutt_set_flag(ctx, ctx->hdrs[i], MUTT_PURGE, 1);
          }
          else
          {
            mx_mbox_close(&f, NULL);
            ctx->closing = false;
            return -1;
          }
        }
      }

      mx_mbox_close(&f, NULL);
    }
  }
  else if (!ctx->changed && ctx->deleted == 0)
  {
    if (!ctx->quiet)
      mutt_message(_("Mailbox is unchanged."));
    if (ctx->magic == MUTT_MBOX || ctx->magic == MUTT_MMDF)
      mbox_reset_atime(ctx, NULL);
    mx_fastclose_mailbox(ctx);
    return 0;
  }

  /* copy mails to the trash before expunging */
  if (purge && ctx->deleted && (mutt_str_strcmp(ctx->path, Trash) != 0))
  {
    if (trash_append(ctx) != 0)
    {
      ctx->closing = false;
      return -1;
    }
  }

#ifdef USE_IMAP
  /* allow IMAP to preserve the deleted flag across sessions */
  if (ctx->magic == MUTT_IMAP)
  {
    int check = imap_sync_mailbox(ctx, purge);
    if (check != 0)
    {
      ctx->closing = false;
      return check;
    }
  }
  else
#endif
  {
    if (!purge)
    {
      for (i = 0; i < ctx->msgcount; i++)
      {
        ctx->hdrs[i]->deleted = false;
        ctx->hdrs[i]->purge = false;
      }
      ctx->deleted = 0;
    }

    if (ctx->changed || ctx->deleted)
    {
      int check = sync_mailbox(ctx, index_hint);
      if (check != 0)
      {
        ctx->closing = false;
        return check;
      }
    }
  }

  if (!ctx->quiet)
  {
    if (move_messages)
    {
      mutt_message(_("%d kept, %d moved, %d deleted."),
                   ctx->msgcount - ctx->deleted, read_msgs, ctx->deleted);
    }
    else
      mutt_message(_("%d kept, %d deleted."), ctx->msgcount - ctx->deleted, ctx->deleted);
  }

  if (ctx->msgcount == ctx->deleted && (ctx->magic == MUTT_MMDF || ctx->magic == MUTT_MBOX) &&
      !mutt_is_spool(ctx->path) && !SaveEmpty)
  {
    mutt_file_unlink_empty(ctx->path);
  }

#ifdef USE_SIDEBAR
  if (purge && ctx->deleted)
  {
    int orig_msgcount = ctx->msgcount;

    for (i = 0; i < ctx->msgcount; i++)
    {
      if (ctx->hdrs[i]->deleted && !ctx->hdrs[i]->read)
        ctx->unread--;
      if (ctx->hdrs[i]->deleted && ctx->hdrs[i]->flagged)
        ctx->flagged--;
    }
    ctx->msgcount -= ctx->deleted;
    mutt_sb_set_buffystats(ctx);
    ctx->msgcount = orig_msgcount;
  }
#endif

  mx_fastclose_mailbox(ctx);

  return 0;
}

/**
 * mx_update_tables - Update a Context structure's internal tables
 * @param ctx        Mailbox
 * @param committing Commit the changes?
 */
void mx_update_tables(struct Context *ctx, bool committing)
{
  int i, j;

  /* update memory to reflect the new state of the mailbox */
  ctx->vcount = 0;
  ctx->vsize = 0;
  ctx->tagged = 0;
  ctx->deleted = 0;
  ctx->new = 0;
  ctx->unread = 0;
  ctx->changed = false;
  ctx->flagged = 0;
  for (i = 0, j = 0; i < ctx->msgcount; i++)
  {
    if (!ctx->hdrs[i]->quasi_deleted &&
        ((committing && (!ctx->hdrs[i]->deleted || (ctx->magic == MUTT_MAILDIR && MaildirTrash))) ||
         (!committing && ctx->hdrs[i]->active)))
    {
      if (i != j)
      {
        ctx->hdrs[j] = ctx->hdrs[i];
        ctx->hdrs[i] = NULL;
      }
      ctx->hdrs[j]->msgno = j;
      if (ctx->hdrs[j]->virtual != -1)
      {
        ctx->v2r[ctx->vcount] = j;
        ctx->hdrs[j]->virtual = ctx->vcount++;
        struct Body *b = ctx->hdrs[j]->content;
        ctx->vsize += b->length + b->offset - b->hdr_offset;
      }

      if (committing)
        ctx->hdrs[j]->changed = false;
      else if (ctx->hdrs[j]->changed)
        ctx->changed = true;

      if (!committing || (ctx->magic == MUTT_MAILDIR && MaildirTrash))
      {
        if (ctx->hdrs[j]->deleted)
          ctx->deleted++;
      }

      if (ctx->hdrs[j]->tagged)
        ctx->tagged++;
      if (ctx->hdrs[j]->flagged)
        ctx->flagged++;
      if (!ctx->hdrs[j]->read)
      {
        ctx->unread++;
        if (!ctx->hdrs[j]->old)
          ctx->new ++;
      }

      j++;
    }
    else
    {
      if (ctx->magic == MUTT_MH || ctx->magic == MUTT_MAILDIR)
      {
        ctx->size -= (ctx->hdrs[i]->content->length + ctx->hdrs[i]->content->offset -
                      ctx->hdrs[i]->content->hdr_offset);
      }
      /* remove message from the hash tables */
      if (ctx->subj_hash && ctx->hdrs[i]->env->real_subj)
        mutt_hash_delete(ctx->subj_hash, ctx->hdrs[i]->env->real_subj, ctx->hdrs[i]);
      if (ctx->id_hash && ctx->hdrs[i]->env->message_id)
        mutt_hash_delete(ctx->id_hash, ctx->hdrs[i]->env->message_id, ctx->hdrs[i]);
      mutt_label_hash_remove(ctx, ctx->hdrs[i]);
      /* The path mx_mbox_check() -> imap_check_mailbox() ->
       *          imap_expunge_mailbox() -> mx_update_tables()
       * can occur before a call to mx_mbox_sync(), resulting in
       * last_tag being stale if it's not reset here.
       */
      if (ctx->last_tag == ctx->hdrs[i])
        ctx->last_tag = NULL;
      mutt_header_free(&ctx->hdrs[i]);
    }
  }
  ctx->msgcount = j;
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
      mutt_str_strfcpy(tmp, _("Use 'toggle-write' to re-enable write!"), sizeof(tmp));

    mutt_error(_("Mailbox is marked unwritable. %s"), tmp);
    return -1;
  }
  else if (ctx->readonly)
  {
    mutt_error(_("Mailbox is read-only."));
    return -1;
  }

  if (!ctx->changed && !ctx->deleted)
  {
    if (!ctx->quiet)
      mutt_message(_("Mailbox is unchanged."));
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
      if (!ctx->changed)
        return 0; /* nothing to do! */
      /* let IMAP servers hold on to D flags */
      if (ctx->magic != MUTT_IMAP)
      {
        for (int i = 0; i < ctx->msgcount; i++)
        {
          ctx->hdrs[i]->deleted = false;
          ctx->hdrs[i]->purge = false;
        }
        ctx->deleted = 0;
      }
    }
    else if (ctx->last_tag && ctx->last_tag->deleted)
      ctx->last_tag = NULL; /* reset last tagged msg now useless */
  }

  /* really only for IMAP - imap_sync_mailbox results in a call to
   * mx_update_tables, so ctx->deleted is 0 when it comes back */
  msgcount = ctx->msgcount;
  deleted = ctx->deleted;

  if (purge && ctx->deleted && (mutt_str_strcmp(ctx->path, Trash) != 0))
  {
    if (trash_append(ctx) != 0)
      return -1;
  }

#ifdef USE_IMAP
  if (ctx->magic == MUTT_IMAP)
    rc = imap_sync_mailbox(ctx, purge);
  else
#endif
    rc = sync_mailbox(ctx, index_hint);
  if (rc == 0)
  {
#ifdef USE_IMAP
    if (ctx->magic == MUTT_IMAP && !purge)
    {
      if (!ctx->quiet)
        mutt_message(_("Mailbox checkpointed."));
    }
    else
#endif
    {
      if (!ctx->quiet)
        mutt_message(_("%d kept, %d deleted."), msgcount - deleted, deleted);
    }

    mutt_sleep(0);

    if (ctx->msgcount == ctx->deleted && (ctx->magic == MUTT_MBOX || ctx->magic == MUTT_MMDF) &&
        !mutt_is_spool(ctx->path) && !SaveEmpty)
    {
      unlink(ctx->path);
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
    if (purge || (ctx->magic != MUTT_MAILDIR && ctx->magic != MUTT_MH))
    {
      /* IMAP does this automatically after handling EXPUNGE */
      if (ctx->magic != MUTT_IMAP)
      {
        mx_update_tables(ctx, true);
        mutt_sort_headers(ctx, 1); /* rethread from scratch */
      }
    }
  }

  return rc;
}

/**
 * mx_msg_open_new - Open a new message
 * @param ctx   Destination mailbox
 * @param hdr   Message being copied (required for maildir support, because the filename depends on the message flags)
 * @param flags Flags, e.g. #MUTT_SET_DRAFT
 * @retval ptr New Message
 */
struct Message *mx_msg_open_new(struct Context *ctx, struct Header *hdr, int flags)
{
  struct Address *p = NULL;
  struct Message *msg = NULL;

  if (!ctx->mx_ops || !ctx->mx_ops->msg_open_new)
  {
    mutt_debug(1, "function unimplemented for mailbox type %d.\n", ctx->magic);
    return NULL;
  }

  msg = mutt_mem_calloc(1, sizeof(struct Message));
  msg->write = true;

  if (hdr)
  {
    msg->flags.flagged = hdr->flagged;
    msg->flags.replied = hdr->replied;
    msg->flags.read = hdr->read;
    msg->flags.draft = (flags & MUTT_SET_DRAFT) ? true : false;
    msg->received = hdr->received;
  }

  if (msg->received == 0)
    time(&msg->received);

  if (ctx->mx_ops->msg_open_new(ctx, msg, hdr) == 0)
  {
    if (ctx->magic == MUTT_MMDF)
      fputs(MMDF_SEP, msg->fp);

    if ((ctx->magic == MUTT_MBOX || ctx->magic == MUTT_MMDF) && flags & MUTT_ADD_FROM)
    {
      if (hdr)
      {
        if (hdr->env->return_path)
          p = hdr->env->return_path;
        else if (hdr->env->sender)
          p = hdr->env->sender;
        else
          p = hdr->env->from;
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
  if (!ctx || !ctx->mx_ops)
  {
    mutt_debug(1, "null or invalid context.\n");
    return -1;
  }

  return ctx->mx_ops->mbox_check(ctx, index_hint);
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

  if (!ctx->mx_ops || !ctx->mx_ops->msg_open)
  {
    mutt_debug(1, "function not implemented for mailbox type %d.\n", ctx->magic);
    return NULL;
  }

  msg = mutt_mem_calloc(1, sizeof(struct Message));
  if (ctx->mx_ops->msg_open(ctx, msg, msgno))
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
  if (!ctx->mx_ops || !ctx->mx_ops->msg_commit)
    return -1;

  if (!(msg->write && ctx->append))
  {
    mutt_debug(1, "msg->write = %d, ctx->append = %d\n", msg->write, ctx->append);
    return -1;
  }

  return ctx->mx_ops->msg_commit(ctx, msg);
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

  if (ctx->mx_ops && ctx->mx_ops->msg_close)
    r = ctx->mx_ops->msg_close(ctx, *msg);

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
 * @param ctx Mailbox
 */
void mx_alloc_memory(struct Context *ctx)
{
  size_t s = MAX(sizeof(struct Header *), sizeof(int));

  if ((ctx->hdrmax + 25) * s < ctx->hdrmax * s)
  {
    mutt_error(_("Integer overflow -- can't allocate memory."));
    mutt_exit(1);
  }

  if (ctx->hdrs)
  {
    mutt_mem_realloc(&ctx->hdrs, sizeof(struct Header *) * (ctx->hdrmax += 25));
    mutt_mem_realloc(&ctx->v2r, sizeof(int) * ctx->hdrmax);
  }
  else
  {
    ctx->hdrs = mutt_mem_calloc((ctx->hdrmax += 25), sizeof(struct Header *));
    ctx->v2r = mutt_mem_calloc(ctx->hdrmax, sizeof(int));
  }
  for (int i = ctx->msgcount; i < ctx->hdrmax; i++)
  {
    ctx->hdrs[i] = NULL;
    ctx->v2r[i] = -1;
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
  struct Header *h = NULL;
  for (int msgno = ctx->msgcount - new_messages; msgno < ctx->msgcount; msgno++)
  {
    h = ctx->hdrs[msgno];

    if (WithCrypto)
    {
      /* NOTE: this _must_ be done before the check for mailcap! */
      h->security = crypt_query(h->content);
    }

    if (!ctx->pattern)
    {
      ctx->v2r[ctx->vcount] = msgno;
      h->virtual = ctx->vcount++;
    }
    else
      h->virtual = -1;
    h->msgno = msgno;

    if (h->env->supersedes)
    {
      struct Header *h2 = NULL;

      if (!ctx->id_hash)
        ctx->id_hash = mutt_make_id_hash(ctx);

      h2 = mutt_hash_find(ctx->id_hash, h->env->supersedes);
      if (h2)
      {
        h2->superseded = true;
        if (Score)
          mutt_score_message(ctx, h2, true);
      }
    }

    /* add this message to the hash tables */
    if (ctx->id_hash && h->env->message_id)
      mutt_hash_insert(ctx->id_hash, h->env->message_id, h);
    if (ctx->subj_hash && h->env->real_subj)
      mutt_hash_insert(ctx->subj_hash, h->env->real_subj, h);
    mutt_label_hash_add(ctx, h);

    if (Score)
      mutt_score_message(ctx, h, false);

    if (h->changed)
      ctx->changed = true;
    if (h->flagged)
      ctx->flagged++;
    if (h->deleted)
      ctx->deleted++;
    if (!h->read)
    {
      ctx->unread++;
      if (!h->old)
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
  switch (mx_get_magic(path))
  {
    case MUTT_MBOX:
    case MUTT_MMDF:
      return mutt_file_check_empty(path);
    case MUTT_MH:
      return mh_check_empty(path);
    case MUTT_MAILDIR:
      return maildir_check_empty(path);
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
  if (ctx->mx_ops->tags_edit)
    return ctx->mx_ops->tags_edit(ctx, tags, buf, buflen);

  mutt_message(_("Folder doesn't support tagging, aborting."));
  return -1;
}

/**
 * mx_tags_commit - save tags to the mailbox
 * @param ctx  Mailbox
 * @param hdr  Email Header
 * @param tags Tags to save
 * @retval  0 Success
 * @retval -1 Failure
 */
int mx_tags_commit(struct Context *ctx, struct Header *hdr, char *tags)
{
  if (ctx->mx_ops->tags_commit)
    return ctx->mx_ops->tags_commit(ctx, hdr, tags);

  mutt_message(_("Folder doesn't support tagging, aborting."));
  return -1;
}

/**
 * mx_tags_is_supported - return true if mailbox support tagging
 * @param ctx Mailbox
 * @retval true Tagging is supported
 */
bool mx_tags_is_supported(struct Context *ctx)
{
  return ctx->mx_ops->tags_commit && ctx->mx_ops->tags_edit;
}
