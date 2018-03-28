/**
 * @file
 * Compressed mbox local mailbox type
 *
 * @authors
 * Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
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

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "compress.h"
#include "context.h"
#include "format_flags.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mx.h"
#include "options.h"
#include "protos.h"

struct Header;

/* Notes:
 * Any references to compressed files also apply to encrypted files.
 * ctx->path     == plaintext file
 * ctx->realpath == compressed file
 */

/**
 * struct CompressInfo - Private data for compress
 *
 * This object gets attached to the mailbox's Context.
 */
struct CompressInfo
{
  const char *append;      /**< append-hook command */
  const char *close;       /**< close-hook  command */
  const char *open;        /**< open-hook   command */
  off_t size;              /**< size of the compressed file */
  struct MxOps *child_ops; /**< callbacks of de-compressed file */
  int locked;              /**< if realpath is locked */
  FILE *lockfp;            /**< fp used for locking */
};

/**
 * lock_realpath - Try to lock the ctx->realpath
 * @param ctx  Mailbox to lock
 * @param excl Lock exclusively?
 * @retval 1 Success (locked or readonly)
 * @retval 0 Error (can't lock the file)
 *
 * Try to (exclusively) lock the mailbox.  If we succeed, then we mark the
 * mailbox as locked.  If we fail, but we didn't want exclusive rights, then
 * the mailbox will be marked readonly.
 */
static int lock_realpath(struct Context *ctx, int excl)
{
  if (!ctx)
    return 0;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return 0;

  if (ci->locked)
    return 1;

  if (excl)
    ci->lockfp = fopen(ctx->realpath, "a");
  else
    ci->lockfp = fopen(ctx->realpath, "r");
  if (!ci->lockfp)
  {
    mutt_perror(ctx->realpath);
    return 0;
  }

  int r = mutt_file_lock(fileno(ci->lockfp), excl, 1);

  if (r == 0)
    ci->locked = 1;
  else if (excl == 0)
  {
    mutt_file_fclose(&ci->lockfp);
    ctx->readonly = true;
    return 1;
  }

  return (r == 0);
}

/**
 * unlock_realpath - Unlock the ctx->realpath
 * @param ctx Mailbox to unlock
 *
 * Unlock a mailbox previously locked by lock_mailbox().
 */
static void unlock_realpath(struct Context *ctx)
{
  if (!ctx)
    return;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return;

  if (!ci->locked)
    return;

  mutt_file_unlock(fileno(ci->lockfp));

  ci->locked = 0;
  mutt_file_fclose(&ci->lockfp);
}

/**
 * setup_paths - Set the mailbox paths
 * @param ctx Mailbox to modify
 * @retval  0 Success
 * @retval -1 Error
 *
 * Save the compressed filename in ctx->realpath.
 * Create a temporary filename and put its name in ctx->path.
 * The temporary file is created to prevent symlink attacks.
 */
static int setup_paths(struct Context *ctx)
{
  if (!ctx)
    return -1;

  char tmppath[_POSIX_PATH_MAX];

  /* Setup the right paths */
  FREE(&ctx->realpath);
  ctx->realpath = ctx->path;

  /* We will uncompress to /tmp */
  mutt_mktemp(tmppath, sizeof(tmppath));
  ctx->path = mutt_str_strdup(tmppath);

  FILE *tmpfp = mutt_file_fopen(ctx->path, "w");
  if (!tmpfp)
    return -1;

  mutt_file_fclose(&tmpfp);
  return 0;
}

/**
 * get_size - Get the size of a file
 * @param path File to measure
 * @retval n Size in bytes
 * @retval 0 On error
 */
static int get_size(const char *path)
{
  if (!path)
    return 0;

  struct stat sb;
  if (stat(path, &sb) != 0)
    return 0;

  return sb.st_size;
}

/**
 * store_size - Save the size of the compressed file
 * @param ctx Mailbox
 *
 * Save the compressed file size in the compress_info struct.
 */
static void store_size(const struct Context *ctx)
{
  if (!ctx)
    return;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return;

  ci->size = get_size(ctx->realpath);
}

/**
 * find_hook - Find a hook to match a path
 * @param type Type of hook, e.g. #MUTT_CLOSEHOOK
 * @param path Filename to test
 * @retval string Matching hook command
 * @retval NULL   No matches
 *
 * Each hook has a type and a pattern.
 * Find a command that matches the type and path supplied. e.g.
 *
 * User config:
 *      open-hook '\.gz$' "gzip -cd '%f' > '%t'"
 *
 * Call:
 *      find_hook (#MUTT_OPENHOOK, "myfile.gz");
 */
static const char *find_hook(int type, const char *path)
{
  if (!path)
    return NULL;

  const char *c = mutt_find_hook(type, path);
  if (!c || !*c)
    return NULL;

  return c;
}

/**
 * set_compress_info - Find the compress hooks for a mailbox
 * @param ctx Mailbox to examine
 * @retval ptr  CompressInfo Hook info for the mailbox's path
 * @retval NULL On error
 *
 * When a mailbox is opened, we check if there are any matching hooks.
 */
static struct CompressInfo *set_compress_info(struct Context *ctx)
{
  if (!ctx || !ctx->path)
    return NULL;

  if (ctx->compress_info)
    return ctx->compress_info;

  /* Open is compulsory */
  const char *o = find_hook(MUTT_OPENHOOK, ctx->path);
  if (!o)
    return NULL;

  const char *c = find_hook(MUTT_CLOSEHOOK, ctx->path);
  const char *a = find_hook(MUTT_APPENDHOOK, ctx->path);

  struct CompressInfo *ci = mutt_mem_calloc(1, sizeof(struct CompressInfo));
  ctx->compress_info = ci;

  ci->open = mutt_str_strdup(o);
  ci->close = mutt_str_strdup(c);
  ci->append = mutt_str_strdup(a);

  return ci;
}

/**
 * free_compress_info - Frees the compress info members and structure
 * @param ctx Mailbox to free compress_info for
 */
static void free_compress_info(struct Context *ctx)
{
  struct CompressInfo *ci = NULL;

  if (!ctx || !ctx->compress_info)
    return;

  ci = ctx->compress_info;
  FREE(&ci->open);
  FREE(&ci->close);
  FREE(&ci->append);

  unlock_realpath(ctx);

  FREE(&ctx->compress_info);
}

/**
 * escape_path - Escapes single quotes in a path for a command string
 * @param src the path to escape
 * @retval ptr The escaped string
 */
static char *escape_path(char *src)
{
  static char dest[HUGE_STRING];
  char *destp = dest;
  int destsize = 0;

  if (!src)
    return NULL;

  while (*src && (destsize < sizeof(dest) - 1))
  {
    if (*src != '\'')
    {
      *destp++ = *src++;
      destsize++;
    }
    else
    {
      /* convert ' into '\'' */
      if (destsize + 4 < sizeof(dest))
      {
        *destp++ = *src++;
        *destp++ = '\\';
        *destp++ = '\'';
        *destp++ = '\'';
        destsize += 4;
      }
      else
        break;
    }
  }
  *destp = '\0';

  return dest;
}

/**
 * compress_format_str - Expand the filenames in a command string
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  op       printf-like operator, e.g. 't'
 * @param[in]  src      printf-like format string
 * @param[in]  prec     Field precision, e.g. "-3.4"
 * @param[in]  if_str   If condition is met, display this string
 * @param[in]  else_str Otherwise, display this string
 * @param[in]  data     Pointer to the mailbox Context
 * @param[in]  flags    Format flags
 * @retval src (unchanged)
 *
 * compress_format_str() is a callback function for mutt_expando_format().
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%f     | Compressed file
 * | \%t     | Plaintext, temporary file
 */
static const char *compress_format_str(char *buf, size_t buflen, size_t col, int cols,
                                       char op, const char *src, const char *prec,
                                       const char *if_str, const char *else_str,
                                       unsigned long data, enum FormatFlag flags)
{
  if (!buf || (data == 0))
    return src;

  struct Context *ctx = (struct Context *) data;

  switch (op)
  {
    case 'f':
      /* Compressed file */
      snprintf(buf, buflen, "%s", NONULL(escape_path(ctx->realpath)));
      break;
    case 't':
      /* Plaintext, temporary file */
      snprintf(buf, buflen, "%s", NONULL(escape_path(ctx->path)));
      break;
  }
  return src;
}

/**
 * expand_command_str - Expand placeholders in command string
 * @param ctx    Mailbox for paths
 * @param cmd    Template command to be expanded
 * @param buf    Buffer to store the command
 * @param buflen Size of the buffer
 *
 * This function takes a hook command and expands the filename placeholders
 * within it.  The function calls mutt_expando_format() to do the replacement
 * which calls our callback function compress_format_str(). e.g.
 *
 * Template command:
 *      gzip -cd '%f' > '%t'
 *
 * Result:
 *      gzip -dc '~/mail/abc.gz' > '/tmp/xyz'
 */
static void expand_command_str(const struct Context *ctx, const char *cmd, char *buf, int buflen)
{
  if (!ctx || !cmd || !buf)
    return;

  mutt_expando_format(buf, buflen, 0, buflen, cmd, compress_format_str,
                      (unsigned long) ctx, 0);
}

/**
 * execute_command - Run a system command
 * @param ctx         Mailbox to work with
 * @param command     Command string to execute
 * @param progress    Message to show the user
 * @retval 1 Success
 * @retval 0 Failure
 *
 * Run the supplied command, taking care of all the NeoMutt requirements,
 * such as locking files and blocking signals.
 */
static int execute_command(struct Context *ctx, const char *command, const char *progress)
{
  int rc = 1;
  char sys_cmd[HUGE_STRING];

  if (!ctx || !command || !progress)
    return 0;

  if (!ctx->quiet)
    mutt_message(progress, ctx->realpath);

  mutt_sig_block();
  endwin();
  fflush(stdout);

  expand_command_str(ctx, command, sys_cmd, sizeof(sys_cmd));

  if (mutt_system(sys_cmd) != 0)
  {
    rc = 0;
    mutt_any_key_to_continue(NULL);
    mutt_error(_("Error running \"%s\"!"), sys_cmd);
  }

  mutt_sig_unblock();

  return rc;
}

/**
 * comp_open_mailbox - Open a compressed mailbox
 * @param ctx Mailbox to open
 *
 * Set up a compressed mailbox to be read.
 * Decompress the mailbox and set up the paths and hooks needed.
 * Then determine the type of the mailbox so we can delegate the handling of
 * messages.
 */
static int comp_open_mailbox(struct Context *ctx)
{
  if (!ctx || (ctx->magic != MUTT_COMPRESSED))
    return -1;

  struct CompressInfo *ci = set_compress_info(ctx);
  if (!ci)
    return -1;

  /* If there's no close-hook, or the file isn't writable */
  if (!ci->close || (access(ctx->path, W_OK) != 0))
    ctx->readonly = true;

  if (setup_paths(ctx) != 0)
    goto or_fail;
  store_size(ctx);

  if (!lock_realpath(ctx, 0))
  {
    mutt_error(_("Unable to lock mailbox!"));
    goto or_fail;
  }

  int rc = execute_command(ctx, ci->open, _("Decompressing %s"));
  if (rc == 0)
    goto or_fail;

  unlock_realpath(ctx);

  ctx->magic = mx_get_magic(ctx->path);
  if (ctx->magic == 0)
  {
    mutt_error(_("Can't identify the contents of the compressed file"));
    goto or_fail;
  }

  ci->child_ops = mx_get_ops(ctx->magic);
  if (!ci->child_ops)
  {
    mutt_error(_("Can't find mailbox ops for mailbox type %d"), ctx->magic);
    goto or_fail;
  }

  return ci->child_ops->open(ctx);

or_fail:
  /* remove the partial uncompressed file */
  remove(ctx->path);
  free_compress_info(ctx);
  return -1;
}

/**
 * comp_open_append_mailbox - Open a compressed mailbox for appending
 * @param ctx   Mailbox to open
 * @param flags e.g. Does the file already exist?
 * @retval  0 Success
 * @retval -1 Failure
 *
 * To append to a compressed mailbox we need an append-hook (or both open- and
 * close-hooks).
 */
static int comp_open_append_mailbox(struct Context *ctx, int flags)
{
  if (!ctx)
    return -1;

  /* If this succeeds, we know there's an open-hook */
  struct CompressInfo *ci = set_compress_info(ctx);
  if (!ci)
    return -1;

  /* To append we need an append-hook or a close-hook */
  if (!ci->append && !ci->close)
  {
    mutt_error(_("Cannot append without an append-hook or close-hook : %s"), ctx->path);
    goto oa_fail1;
  }

  if (setup_paths(ctx) != 0)
    goto oa_fail2;

  /* Lock the realpath for the duration of the append.
   * It will be unlocked in the close */
  if (!lock_realpath(ctx, 1))
  {
    mutt_error(_("Unable to lock mailbox!"));
    goto oa_fail2;
  }

  /* Open the existing mailbox, unless we are appending */
  if (!ci->append && (get_size(ctx->realpath) > 0))
  {
    int rc = execute_command(ctx, ci->open, _("Decompressing %s"));
    if (rc == 0)
    {
      mutt_error(_("Compress command failed: %s"), ci->open);
      goto oa_fail2;
    }
    ctx->magic = mx_get_magic(ctx->path);
  }
  else
    ctx->magic = MboxType;

  /* We can only deal with mbox and mmdf mailboxes */
  if ((ctx->magic != MUTT_MBOX) && (ctx->magic != MUTT_MMDF))
  {
    mutt_error(_("Unsupported mailbox type for appending."));
    goto oa_fail2;
  }

  ci->child_ops = mx_get_ops(ctx->magic);
  if (!ci->child_ops)
  {
    mutt_error(_("Can't find mailbox ops for mailbox type %d"), ctx->magic);
    goto oa_fail2;
  }

  if (ci->child_ops->open_append(ctx, flags) != 0)
    goto oa_fail2;

  return 0;

oa_fail2:
  /* remove the partial uncompressed file */
  remove(ctx->path);
oa_fail1:
  /* Free the compress_info to prevent close from trying to recompress */
  free_compress_info(ctx);

  return -1;
}

/**
 * comp_close_mailbox - Close a compressed mailbox
 * @param ctx Mailbox to close
 * @retval  0 Success
 * @retval -1 Failure
 *
 * If the mailbox has been changed then re-compress the tmp file.
 * Then delete the tmp file.
 */
static int comp_close_mailbox(struct Context *ctx)
{
  if (!ctx)
    return -1;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct MxOps *ops = ci->child_ops;
  if (!ops)
  {
    free_compress_info(ctx);
    return -1;
  }

  ops->close(ctx);

  /* sync has already been called, so we only need to delete some files */
  if (!ctx->append)
  {
    /* If the file was removed, remove the compressed folder too */
    if ((access(ctx->path, F_OK) != 0) && !SaveEmpty)
    {
      remove(ctx->realpath);
    }
    else
    {
      remove(ctx->path);
    }
  }
  else
  {
    const char *append = NULL;
    const char *msg = NULL;

    /* The file exists and we can append */
    if ((access(ctx->realpath, F_OK) == 0) && ci->append)
    {
      append = ci->append;
      msg = _("Compressed-appending to %s...");
    }
    else
    {
      append = ci->close;
      msg = _("Compressing %s...");
    }

    int rc = execute_command(ctx, append, msg);
    if (rc == 0)
    {
      mutt_any_key_to_continue(NULL);
      mutt_error(_("Error. Preserving temporary file: %s"), ctx->path);
    }
    else
      remove(ctx->path);

    unlock_realpath(ctx);
  }

  free_compress_info(ctx);

  return 0;
}

/**
 * comp_check_mailbox - Check for changes in the compressed file
 * @param ctx        Mailbox
 * @param index_hint Currently selected mailbox
 * @retval 0              Mailbox OK
 * @retval #MUTT_REOPENED The mailbox was closed and reopened
 * @retval -1             Mailbox bad
 *
 * If the compressed file changes in size but the mailbox hasn't been changed
 * in NeoMutt, then we can close and reopen the mailbox.
 *
 * If the mailbox has been changed in NeoMutt, warn the user.
 *
 * The return codes are picked to match mx_check_mailbox().
 */
static int comp_check_mailbox(struct Context *ctx, int *index_hint)
{
  if (!ctx)
    return -1;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  int size = get_size(ctx->realpath);
  if (size == ci->size)
    return 0;

  if (!lock_realpath(ctx, 0))
  {
    mutt_error(_("Unable to lock mailbox!"));
    return -1;
  }

  int rc = execute_command(ctx, ci->open, _("Decompressing %s"));
  store_size(ctx);
  unlock_realpath(ctx);
  if (rc == 0)
    return -1;

  return ops->check(ctx, index_hint);
}

/**
 * comp_open_message - Delegated to mbox handler
 */
static int comp_open_message(struct Context *ctx, struct Message *msg, int msgno)
{
  if (!ctx)
    return -1;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->open_msg(ctx, msg, msgno);
}

/**
 * comp_close_message - Delegated to mbox handler
 */
static int comp_close_message(struct Context *ctx, struct Message *msg)
{
  if (!ctx)
    return -1;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->close_msg(ctx, msg);
}

/**
 * comp_commit_message - Delegated to mbox handler
 */
static int comp_commit_message(struct Context *ctx, struct Message *msg)
{
  if (!ctx)
    return -1;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->commit_msg(ctx, msg);
}

/**
 * comp_open_new_message - Delegated to mbox handler
 */
static int comp_open_new_message(struct Message *msg, struct Context *ctx, struct Header *hdr)
{
  if (!ctx)
    return -1;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->open_new_msg(msg, ctx, hdr);
}

/**
 * mutt_comp_can_append - Can we append to this path?
 * @param ctx Mailbox
 * @retval true  Yes, we can append to the file
 * @retval false No, appending isn't possible
 *
 * To append to a file we can either use an 'append-hook' or a combination of
 * 'open-hook' and 'close-hook'.
 *
 * A match means it's our responsibility to append to the file.
 */
bool mutt_comp_can_append(struct Context *ctx)
{
  if (!ctx)
    return false;

  /* If this succeeds, we know there's an open-hook */
  struct CompressInfo *ci = set_compress_info(ctx);
  if (!ci)
    return false;

  /* We have an open-hook, so to append we need an append-hook,
   * or a close-hook. */
  if (ci->append || ci->close)
    return true;

  mutt_error(_("Cannot append without an append-hook or close-hook : %s"), ctx->path);
  return false;
}

/**
 * mutt_comp_can_read - Can we read from this file?
 * @param path Pathname of file to be tested
 * @retval true  Yes, we can read the file
 * @retval false No, we cannot read the file
 *
 * Search for an 'open-hook' with a regex that matches the path.
 *
 * A match means it's our responsibility to open the file.
 */
bool mutt_comp_can_read(const char *path)
{
  if (!path)
    return false;

  if (find_hook(MUTT_OPENHOOK, path))
    return true;
  else
    return false;
}

/**
 * comp_sync_mailbox - Save changes to the compressed mailbox file
 * @param ctx        Mailbox to sync
 * @param index_hint Currently selected mailbox
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Changes in NeoMutt only affect the tmp file.  Calling comp_sync_mailbox()
 * will commit them to the compressed file.
 */
static int comp_sync_mailbox(struct Context *ctx, int *index_hint)
{
  if (!ctx)
    return -1;

  struct CompressInfo *ci = ctx->compress_info;
  if (!ci)
    return -1;

  if (!ci->close)
  {
    mutt_error(_("Can't sync a compressed file without a close-hook"));
    return -1;
  }

  struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  if (!lock_realpath(ctx, 1))
  {
    mutt_error(_("Unable to lock mailbox!"));
    return -1;
  }

  int rc = comp_check_mailbox(ctx, index_hint);
  if (rc != 0)
    goto sync_cleanup;

  rc = ops->sync(ctx, index_hint);
  if (rc != 0)
    goto sync_cleanup;

  rc = execute_command(ctx, ci->close, _("Compressing %s"));
  if (rc == 0)
  {
    rc = -1;
    goto sync_cleanup;
  }

  rc = 0;

sync_cleanup:
  store_size(ctx);
  unlock_realpath(ctx);
  return rc;
}

/**
 * mutt_comp_valid_command - Is this command string allowed?
 * @param cmd  Command string
 * @retval 1 Valid command
 * @retval 0 "%f" and/or "%t" is missing
 *
 * A valid command string must have both "%f" (from file) and "%t" (to file).
 * We don't check if we can actually run the command.
 */
int mutt_comp_valid_command(const char *cmd)
{
  if (!cmd)
    return 0;

  return (strstr(cmd, "%f") && strstr(cmd, "%t"));
}

/**
 * mx_comp_ops - Mailbox callback functions
 *
 * Compress only uses open, close and check.
 * The message functions are delegated to mbox.
 */
struct MxOps mx_comp_ops = {
  .open = comp_open_mailbox,
  .open_append = comp_open_append_mailbox,
  .close = comp_close_mailbox,
  .check = comp_check_mailbox,
  .sync = comp_sync_mailbox,
  .open_msg = comp_open_message,
  .close_msg = comp_close_message,
  .commit_msg = comp_commit_message,
  .open_new_msg = comp_open_new_message,
};
