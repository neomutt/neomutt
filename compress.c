/* Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mutt.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mx.h"
#include "compress.h"

/* Notes:
 * Any references to compressed files also apply to encrypted files.
 * ctx->path     == plaintext file
 * ctx->realpath == compressed file
 */

/**
 * struct COMPRESS_INFO - Private data for compress
 *
 * This object gets attached to the mailbox's CONTEXT.
 */
typedef struct
{
  const char *append;             /* append-hook command */
  const char *close;              /* close-hook  command */
  const char *open;               /* open-hook   command */
  off_t size;                     /* size of the compressed file */
  struct mx_ops *child_ops;       /* callbacks of de-compressed file */
  int locked;                     /* if realpath is locked */
  FILE *lockfp;                   /* fp used for locking */
} COMPRESS_INFO;


/**
 * lock_realpath - Try to lock the ctx->realpath
 * @ctx:  Mailbox to lock
 * @excl: Lock exclusively?
 *
 * Try to (exclusively) lock the mailbox.  If we succeed, then we mark the
 * mailbox as locked.  If we fail, but we didn't want exclusive rights, then
 * the mailbox will be marked readonly.
 *
 * Returns:
 *      1: Success (locked or readonly)
 *      0: Error (can't lock the file)
 */
static int
lock_realpath (CONTEXT *ctx, int excl)
{
  if (!ctx)
    return 0;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return 0;

  if (ci->locked)
    return 1;

  if (excl)
    ci->lockfp = fopen (ctx->realpath, "a");
  else
    ci->lockfp = fopen (ctx->realpath, "r");
  if (!ci->lockfp)
  {
    mutt_perror (ctx->realpath);
    return 0;
  }

  int r = mx_lock_file (ctx->realpath, fileno (ci->lockfp), excl, 1, 1);

  if (r == 0)
    ci->locked = 1;
  else if (excl == 0)
  {
    safe_fclose (&ci->lockfp);
    ctx->readonly = 1;
    return 1;
  }

  return (r == 0);
}

/**
 * unlock_realpath - Unlock the ctx->realpath
 * @ctx: Mailbox to unlock
 *
 * Unlock a mailbox previously locked by lock_mailbox().
 */
static void
unlock_realpath (CONTEXT *ctx)
{
  if (!ctx)
    return;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return;

  if (!ci->locked)
    return;

  mx_unlock_file (ctx->realpath, fileno (ci->lockfp), 1);

  ci->locked = 0;
  safe_fclose (&ci->lockfp);
}

/**
 * setup_paths - Set the mailbox paths
 * @ctx: Mailbox to modify
 *
 * Save the compressed filename in ctx->realpath.
 * Create a temporary filename and put its name in ctx->path.
 * The temporary file is created to prevent symlink attacks.
 *
 * Returns:
 *      0: Success
 *      -1: Error
 */
static int
setup_paths (CONTEXT *ctx)
{
  if (!ctx)
    return -1;

  char tmppath[_POSIX_PATH_MAX];
  FILE *tmpfp;

  /* Setup the right paths */
  FREE(&ctx->realpath);
  ctx->realpath = ctx->path;

  /* We will uncompress to /tmp */
  mutt_mktemp (tmppath, sizeof (tmppath));
  ctx->path = safe_strdup (tmppath);

  if ((tmpfp = safe_fopen (ctx->path, "w")) == NULL)
    return -1;

  safe_fclose (&tmpfp);
  return 0;
}

/**
 * get_size - Get the size of a file
 * @path: File to measure
 *
 * Returns:
 *      number: Size in bytes
 *      0:      On error
 */
static int
get_size (const char *path)
{
  if (!path)
    return 0;

  struct stat sb;
  if (stat (path, &sb) != 0)
    return 0;

  return sb.st_size;
}

/**
 * store_size - Save the size of the compressed file
 * @ctx: Mailbox
 *
 * Save the compressed file size in the compress_info struct.
 */
static void
store_size (const CONTEXT *ctx)
{
  if (!ctx)
    return;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return;

  ci->size = get_size (ctx->realpath);
}

/**
 * find_hook - Find a hook to match a path
 * @type: Type of hook, e.g. MUTT_CLOSEHOOK
 * @path: Filename to test
 *
 * Each hook has a type and a pattern.
 * Find a command that matches the type and path supplied. e.g.
 *
 * User config:
 *      open-hook '\.gz$' "gzip -cd '%f' > '%t'"
 *
 * Call:
 *      find_hook (MUTT_OPENHOOK, "myfile.gz");
 *
 * Returns:
 *      string: Matching hook command
 *      NULL:   No matches
 */
static const char *
find_hook (int type, const char *path)
{
  if (!path)
    return NULL;

  const char *c = mutt_find_hook (type, path);
  if (!c || !*c)
    return NULL;

  return c;
}

/**
 * set_compress_info - Find the compress hooks for a mailbox
 * @ctx: Mailbox to examine
 *
 * When a mailbox is opened, we check if there are any matching hooks.
 *
 * Returns:
 *      COMPRESS_INFO: Hook info for the mailbox's path
 *      NULL:          On error
 */
static COMPRESS_INFO *
set_compress_info (CONTEXT *ctx)
{
  if (!ctx || !ctx->path)
    return NULL;

  if (ctx->compress_info)
    return ctx->compress_info;

  /* Open is compulsory */
  const char *o = find_hook (MUTT_OPENHOOK,   ctx->path);
  if (!o)
    return NULL;

  const char *c = find_hook (MUTT_CLOSEHOOK,  ctx->path);
  const char *a = find_hook (MUTT_APPENDHOOK, ctx->path);

  COMPRESS_INFO *ci = safe_calloc (1, sizeof (COMPRESS_INFO));
  ctx->compress_info = ci;

  ci->open   = safe_strdup (o);
  ci->close  = safe_strdup (c);
  ci->append = safe_strdup (a);

  return ci;
}

/**
 * mutt_free_compress_info - Frees the compress info members and structure.
 * @ctx: Mailbox to free compress_info for.
 */
void
mutt_free_compress_info (CONTEXT *ctx)
{
  COMPRESS_INFO *ci;

  if (!ctx || !ctx->compress_info)
    return;

  ci = ctx->compress_info;
  FREE (&ci->open);
  FREE (&ci->close);
  FREE (&ci->append);

  unlock_realpath (ctx);

  FREE (&ctx->compress_info);
}

/**
 * escape_path - Escapes single quotes in a path for a command string.
 * @src - the path to escape.
 *
 * Returns: a pointer to the escaped string.
 */
static char *
escape_path (char *src)
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
 * cb_format_str - Expand the filenames in the command string
 * @dest:        Buffer in which to save string
 * @destlen:     Buffer length
 * @col:         Starting column, UNUSED
 * @cols:        Number of screen columns, UNUSED
 * @op:          printf-like operator, e.g. 't'
 * @src:         printf-like format string
 * @fmt:         Field formatting string, UNUSED
 * @ifstring:    If condition is met, display this string, UNUSED
 * @elsestring:  Otherwise, display this string, UNUSED
 * @data:        Pointer to the mailbox CONTEXT
 * @flags:       Format flags, UNUSED
 *
 * cb_format_str is a callback function for mutt_FormatString.  It understands
 * two operators. '%f' : 'from' filename, '%t' : 'to' filename.
 *
 * Returns: src (unchanged)
 */
static const char *
cb_format_str (char *dest, size_t destlen, size_t col, int cols, char op, const char *src,
  const char *fmt, const char *ifstring, const char *elsestring,
  unsigned long data, format_flag flags)
{
  if (!dest || (data == 0))
    return src;

  CONTEXT *ctx = (CONTEXT *) data;

  switch (op)
  {
    case 'f':
      /* Compressed file */
      snprintf (dest, destlen, "%s", NONULL (escape_path (ctx->realpath)));
      break;
    case 't':
      /* Plaintext, temporary file */
      snprintf (dest, destlen, "%s", NONULL (escape_path (ctx->path)));
      break;
  }
  return src;
}

/**
 * expand_command_str - Expand placeholders in command string
 * @ctx:    Mailbox for paths
 * @buf:    Buffer to store the command
 * @buflen: Size of the buffer
 *
 * This function takes a hook command and expands the filename placeholders
 * within it.  The function calls mutt_FormatString() to do the replacement
 * which calls our callback function cb_format_str(). e.g.
 *
 * Template command:
 *      gzip -cd '%f' > '%t'
 *
 * Result:
 *      gzip -dc '~/mail/abc.gz' > '/tmp/xyz'
 */
static void
expand_command_str (const CONTEXT *ctx, const char *cmd, char *buf, int buflen)
{
  if (!ctx || !cmd || !buf)
    return;

  mutt_FormatString (buf, buflen, 0, buflen, cmd, cb_format_str, (unsigned long) ctx, 0);
}

/**
 * execute_command - Run a system command
 * @ctx:         Mailbox to work with
 * @command:     Command string to execute
 * @progress:    Message to show the user
 *
 * Run the supplied command, taking care of all the Mutt requirements,
 * such as locking files and blocking signals.
 *
 * Returns:
 *      1: Success
 *      0: Failure
 */
static int
execute_command (CONTEXT *ctx, const char *command, const char *progress)
{
  int rc = 1;
  char sys_cmd[HUGE_STRING];

  if (!ctx || !command || !progress)
    return 0;

  if (!ctx->quiet)
    mutt_message (progress, ctx->realpath);

  mutt_block_signals();
  endwin();
  fflush (stdout);

  expand_command_str (ctx, command, sys_cmd, sizeof (sys_cmd));

  if (mutt_system (sys_cmd) != 0)
  {
    rc = 0;
    mutt_any_key_to_continue (NULL);
    mutt_error (_("Error running \"%s\"!"), sys_cmd);
  }

  mutt_unblock_signals();

  return rc;
}

/**
 * open_mailbox - Open a compressed mailbox
 * @ctx: Mailbox to open
 *
 * Set up a compressed mailbox to be read.
 * Decompress the mailbox and set up the paths and hooks needed.
 * Then determine the type of the mailbox so we can delegate the handling of
 * messages.
 */
static int
open_mailbox (CONTEXT *ctx)
{
  if (!ctx || (ctx->magic != MUTT_COMPRESSED))
    return -1;

  COMPRESS_INFO *ci = set_compress_info (ctx);
  if (!ci)
    return -1;

  /* If there's no close-hook, or the file isn't writable */
  if (!ci->close || (access (ctx->path, W_OK) != 0))
    ctx->readonly = 1;

  if (setup_paths (ctx) != 0)
    goto or_fail;
  store_size (ctx);

  if (!lock_realpath (ctx, 0))
  {
    mutt_error (_("Unable to lock mailbox!"));
    goto or_fail;
  }

  int rc = execute_command (ctx, ci->open, _("Decompressing %s"));
  if (rc == 0)
    goto or_fail;

  unlock_realpath (ctx);

  ctx->magic = mx_get_magic (ctx->path);
  if (ctx->magic == 0)
  {
    mutt_error (_("Can't identify the contents of the compressed file"));
    goto or_fail;
  }

  ci->child_ops = mx_get_ops (ctx->magic);
  if (!ci->child_ops)
  {
    mutt_error (_("Can't find mailbox ops for mailbox type %d"), ctx->magic);
    goto or_fail;
  }

  return ci->child_ops->open (ctx);

or_fail:
  /* remove the partial uncompressed file */
  remove (ctx->path);
  mutt_free_compress_info (ctx);
  return -1;
}

/**
 * open_append_mailbox - Open a compressed mailbox for appending
 * @ctx:   Mailbox to open
 * @flags: e.g. Does the file already exist?
 *
 * To append to a compressed mailbox we need an append-hook (or both open- and
 * close-hooks).
 *
 * Returns:
 *       0: Success
 *      -1: Failure
 */
static int
open_append_mailbox (CONTEXT *ctx, int flags)
{
  if (!ctx)
    return -1;

  /* If this succeeds, we know there's an open-hook */
  COMPRESS_INFO *ci = set_compress_info (ctx);
  if (!ci)
    return -1;

  /* To append we need an append-hook or a close-hook */
  if (!ci->append && !ci->close)
  {
    mutt_error (_("Cannot append without an append-hook or close-hook : %s"), ctx->path);
    goto oa_fail1;
  }

  if (setup_paths (ctx) != 0)
    goto oa_fail2;

  /* Lock the realpath for the duration of the append.
   * It will be unlocked in the close */
  if (!lock_realpath (ctx, 1))
  {
    mutt_error (_("Unable to lock mailbox!"));
    goto oa_fail2;
  }

  /* Open the existing mailbox, unless we are appending */
  if (!ci->append && (get_size (ctx->realpath) > 0))
  {
    int rc = execute_command (ctx, ci->open, _("Decompressing %s"));
    if (rc == 0)
    {
      mutt_error (_("Compress command failed: %s"), ci->open);
      goto oa_fail2;
    }
    ctx->magic = mx_get_magic (ctx->path);
  }
  else
    ctx->magic = DefaultMagic;

  /* We can only deal with mbox and mmdf mailboxes */
  if ((ctx->magic != MUTT_MBOX) && (ctx->magic != MUTT_MMDF))
  {
    mutt_error (_("Unsupported mailbox type for appending."));
    goto oa_fail2;
  }

  ci->child_ops = mx_get_ops (ctx->magic);
  if (!ci->child_ops)
  {
    mutt_error (_("Can't find mailbox ops for mailbox type %d"), ctx->magic);
    goto oa_fail2;
  }

  if (ci->child_ops->open_append (ctx, flags) != 0)
    goto oa_fail2;

  return 0;

oa_fail2:
  /* remove the partial uncompressed file */
  remove (ctx->path);
oa_fail1:
  /* Free the compress_info to prevent close from trying to recompress */
  mutt_free_compress_info (ctx);

  return -1;
}

/**
 * close_mailbox - Close a compressed mailbox
 * @ctx: Mailbox to close
 *
 * If the mailbox has been changed then re-compress the tmp file.
 * Then delete the tmp file.
 *
 * Returns:
 *       0: Success
 *      -1: Failure
 */
static int
close_mailbox (CONTEXT *ctx)
{
  if (!ctx)
    return -1;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct mx_ops *ops = ci->child_ops;
  if (!ops)
    return -1;

  ops->close (ctx);

  /* sync has already been called, so we only need to delete some files */
  if (!ctx->append)
  {
    /* If the file was removed, remove the compressed folder too */
    if ((access (ctx->path, F_OK) != 0) && !option (OPTSAVEEMPTY))
    {
      remove (ctx->realpath);
    }
    else
    {
      remove (ctx->path);
    }

    return 0;
  }

  const char *append;
  const char *msg;

  /* The file exists and we can append */
  if ((access (ctx->realpath, F_OK) == 0) && ci->append)
  {
    append = ci->append;
    msg = _("Compressed-appending to %s...");
  }
  else
  {
    append = ci->close;
    msg = _("Compressing %s...");
  }

  int rc = execute_command (ctx, append, msg);
  if (rc == 0)
  {
    mutt_any_key_to_continue (NULL);
    mutt_error (_("Error. Preserving temporary file: %s"), ctx->path);
  }
  else
    remove (ctx->path);

  unlock_realpath (ctx);

  return 0;
}

/**
 * check_mailbox - Check for changes in the compressed file
 * @ctx: Mailbox
 *
 * If the compressed file changes in size but the mailbox hasn't been changed
 * in Mutt, then we can close and reopen the mailbox.
 *
 * If the mailbox has been changed in Mutt, warn the user.
 *
 * The return codes are picked to match mx_check_mailbox().
 *
 * Returns:
 *      0:              Mailbox OK
 *      MUTT_REOPENED:  The mailbox was closed and reopened
 *      -1:             Mailbox bad
 */
static int
check_mailbox (CONTEXT *ctx, int *index_hint)
{
  if (!ctx)
    return -1;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct mx_ops *ops = ci->child_ops;
  if (!ops)
    return -1;

  int size = get_size (ctx->realpath);
  if (size == ci->size)
    return 0;

  if (!lock_realpath (ctx, 0))
  {
    mutt_error (_("Unable to lock mailbox!"));
    return -1;
  }

  int rc = execute_command (ctx, ci->open, _("Decompressing %s"));
  store_size (ctx);
  unlock_realpath (ctx);
  if (rc == 0)
    return -1;

  return ops->check (ctx, index_hint);
}


/**
 * open_message - Delegated to mbox handler
 */
static int
open_message (CONTEXT *ctx,  MESSAGE *msg, int msgno)
{
  if (!ctx)
    return -1;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct mx_ops *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->open_msg (ctx, msg, msgno);
}

/**
 * close_message - Delegated to mbox handler
 */
static int
close_message (CONTEXT *ctx, MESSAGE *msg)
{
  if (!ctx)
    return -1;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct mx_ops *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->close_msg (ctx, msg);
}

/**
 * commit_message - Delegated to mbox handler
 */
static int
commit_message (CONTEXT *ctx, MESSAGE *msg)
{
  if (!ctx)
    return -1;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct mx_ops *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->commit_msg (ctx, msg);
}

/**
 * open_new_message - Delegated to mbox handler
 */
static int
open_new_message (MESSAGE *msg, CONTEXT *ctx, HEADER *hdr)
{
  if (!ctx)
    return -1;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return -1;

  struct mx_ops *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->open_new_msg (msg, ctx, hdr);
}


/**
 * mutt_comp_can_append - Can we append to this path?
 * @path: pathname of file to be tested
 *
 * To append to a file we can either use an 'append-hook' or a combination of
 * 'open-hook' and 'close-hook'.
 *
 * A match means it's our responsibility to append to the file.
 *
 * Returns:
 *      1: Yes, we can append to the file
 *      0: No, appending isn't possible
 */
int
mutt_comp_can_append (CONTEXT *ctx)
{
  if (!ctx)
    return 0;

  /* If this succeeds, we know there's an open-hook */
  COMPRESS_INFO *ci = set_compress_info (ctx);
  if (!ci)
    return 0;

  /* We have an open-hook, so to append we need an append-hook,
   * or a close-hook. */
  if (ci->append || ci->close)
    return 1;

  mutt_error (_("Cannot append without an append-hook or close-hook : %s"), ctx->path);
  return 0;
}

/**
 * mutt_comp_can_read - Can we read from this file?
 * @path: Pathname of file to be tested
 *
 * Search for an 'open-hook' with a regex that matches the path.
 *
 * A match means it's our responsibility to open the file.
 *
 * Returns:
 *      1: Yes, we can read the file
 *      0: No, we cannot read the file
 */
int
mutt_comp_can_read (const char *path)
{
  if (!path)
    return 0;

  if (find_hook (MUTT_OPENHOOK, path))
    return 1;
  else
    return 0;
}

/**
 * sync_mailbox - Save changes to the compressed mailbox file
 * @ctx: Mailbox to sync
 *
 * Changes in Mutt only affect the tmp file.  Calling sync_mailbox()
 * will commit them to the compressed file.
 *
 * Returns:
 *       0: Success
 *      -1: Failure
 */
static int
sync_mailbox (CONTEXT *ctx, int *index_hint)
{
  if (!ctx)
    return -1;

  COMPRESS_INFO *ci = ctx->compress_info;
  if (!ci)
    return -1;

  if (!ci->close)
  {
    mutt_error (_("Can't sync a compressed file without a close-hook"));
    return -1;
  }

  struct mx_ops *ops = ci->child_ops;
  if (!ops)
    return -1;

  if (!lock_realpath (ctx, 1))
  {
    mutt_error (_("Unable to lock mailbox!"));
    return -1;
  }

  int rc = check_mailbox (ctx, index_hint);
  if (rc != 0)
    goto sync_cleanup;

  rc = ops->sync (ctx, index_hint);
  if (rc != 0)
    goto sync_cleanup;

  rc = execute_command (ctx, ci->close, _("Compressing %s"));
  if (rc == 0)
  {
    rc = -1;
    goto sync_cleanup;
  }

  rc = 0;

sync_cleanup:
  store_size (ctx);
  unlock_realpath (ctx);
  return rc;
}

/**
 * mutt_comp_valid_command - Is this command string allowed?
 * @cmd:  Command string
 *
 * A valid command string must have both "%f" (from file) and "%t" (to file).
 * We don't check if we can actually run the command.
 *
 * Returns:
 *      1: Valid command
 *      0: "%f" and/or "%t" is missing
 */
int
mutt_comp_valid_command (const char *cmd)
{
  if (!cmd)
    return 0;

  return (strstr (cmd, "%f") && strstr (cmd, "%t"));
}


/**
 * mx_comp_ops - Mailbox callback functions
 *
 * Compress only uses open, close and check.
 * The message functions are delegated to mbox.
 */
struct mx_ops mx_comp_ops =
{
  .open         = open_mailbox,
  .open_append  = open_append_mailbox,
  .close        = close_mailbox,
  .check        = check_mailbox,
  .sync         = sync_mailbox,
  .open_msg     = open_message,
  .close_msg    = close_message,
  .commit_msg   = commit_message,
  .open_new_msg = open_new_message
};

