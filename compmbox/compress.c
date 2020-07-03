/**
 * @file
 * Compressed mbox local mailbox type
 *
 * @authors
 * Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
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
 * @page compmbox_compress Compressed mbox local mailbox type
 *
 * Compressed mbox local mailbox type
 *
 * @note
 * Any references to compressed files also apply to encrypted files.
 * - mailbox->path     == plaintext file
 * - mailbox->realpath == compressed file
 */

#include "config.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "lib.h"
#include "format_flags.h"
#include "hook.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"

struct Email;

/**
 * lock_realpath - Try to lock the ctx->realpath
 * @param m    Mailbox to lock
 * @param excl Lock exclusively?
 * @retval true  Success (locked or readonly)
 * @retval false Error (can't lock the file)
 *
 * Try to (exclusively) lock the mailbox.  If we succeed, then we mark the
 * mailbox as locked.  If we fail, but we didn't want exclusive rights, then
 * the mailbox will be marked readonly.
 */
static bool lock_realpath(struct Mailbox *m, bool excl)
{
  if (!m || !m->compress_info)
    return false;

  struct CompressInfo *ci = m->compress_info;

  if (ci->locked)
    return true;

  if (excl)
    ci->fp_lock = fopen(m->realpath, "a");
  else
    ci->fp_lock = fopen(m->realpath, "r");
  if (!ci->fp_lock)
  {
    mutt_perror(m->realpath);
    return false;
  }

  int r = mutt_file_lock(fileno(ci->fp_lock), excl, true);
  if (r == 0)
    ci->locked = true;
  else if (excl)
  {
    mutt_file_fclose(&ci->fp_lock);
    m->readonly = true;
    return true;
  }

  return r == 0;
}

/**
 * unlock_realpath - Unlock the mailbox->realpath
 * @param m Mailbox to unlock
 *
 * Unlock a mailbox previously locked by lock_mailbox().
 */
static void unlock_realpath(struct Mailbox *m)
{
  if (!m || !m->compress_info)
    return;

  struct CompressInfo *ci = m->compress_info;

  if (!ci->locked)
    return;

  mutt_file_unlock(fileno(ci->fp_lock));

  ci->locked = false;
  mutt_file_fclose(&ci->fp_lock);
}

/**
 * setup_paths - Set the mailbox paths
 * @param m Mailbox to modify
 * @retval  0 Success
 * @retval -1 Error
 *
 * Save the compressed filename in mailbox->realpath.
 * Create a temporary filename and put its name in mailbox->path.
 * The temporary file is created to prevent symlink attacks.
 */
static int setup_paths(struct Mailbox *m)
{
  if (!m)
    return -1;

  /* Setup the right paths */
  mutt_str_replace(&m->realpath, mailbox_path(m));

  /* We will uncompress to /tmp */
  struct Buffer *buf = mutt_buffer_pool_get();
  mutt_buffer_mktemp(buf);
  mutt_buffer_copy(&m->pathbuf, buf);
  mutt_buffer_pool_release(&buf);

  FILE *fp = mutt_file_fopen(mailbox_path(m), "w");
  if (!fp)
    return -1;

  mutt_file_fclose(&fp);
  return 0;
}

/**
 * store_size - Save the size of the compressed file
 * @param m Mailbox
 *
 * Save the compressed file size in the compress_info struct.
 */
static void store_size(const struct Mailbox *m)
{
  if (!m || !m->compress_info)
    return;

  struct CompressInfo *ci = m->compress_info;

  ci->size = mutt_file_get_size(m->realpath);
}

/**
 * set_compress_info - Find the compress hooks for a mailbox
 * @param m Mailbox to examine
 * @retval ptr  CompressInfo Hook info for the mailbox's path
 * @retval NULL Error
 *
 * When a mailbox is opened, we check if there are any matching hooks.
 */
static struct CompressInfo *set_compress_info(struct Mailbox *m)
{
  if (!m)
    return NULL;

  if (m->compress_info)
    return m->compress_info;

  /* Open is compulsory */
  const char *o = mutt_find_hook(MUTT_OPEN_HOOK, mailbox_path(m));
  if (!o)
    return NULL;

  const char *c = mutt_find_hook(MUTT_CLOSE_HOOK, mailbox_path(m));
  const char *a = mutt_find_hook(MUTT_APPEND_HOOK, mailbox_path(m));

  struct CompressInfo *ci = mutt_mem_calloc(1, sizeof(struct CompressInfo));
  m->compress_info = ci;

  ci->cmd_open = mutt_str_dup(o);
  ci->cmd_close = mutt_str_dup(c);
  ci->cmd_append = mutt_str_dup(a);

  return ci;
}

/**
 * compress_info_free - Frees the compress info members and structure
 * @param m Mailbox to free compress_info for
 */
static void compress_info_free(struct Mailbox *m)
{
  if (!m || !m->compress_info)
    return;

  struct CompressInfo *ci = m->compress_info;
  FREE(&ci->cmd_open);
  FREE(&ci->cmd_close);
  FREE(&ci->cmd_append);

  unlock_realpath(m);

  FREE(&m->compress_info);
}

/**
 * compress_format_str - Expand the filenames in a command string - Implements ::format_t
 *
 * | Expando | Description
 * |:--------|:--------------------------------------------------------
 * | \%f     | Compressed file
 * | \%t     | Plaintext, temporary file
 */
static const char *compress_format_str(char *buf, size_t buflen, size_t col, int cols,
                                       char op, const char *src, const char *prec,
                                       const char *if_str, const char *else_str,
                                       intptr_t data, MuttFormatFlags flags)
{
  if (!buf || (data == 0))
    return src;

  struct Mailbox *m = (struct Mailbox *) data;

  /* NOTE the compressed file config vars expect %f and %t to be
   * surrounded by '' (unlike other NeoMutt config vars, which add the
   * outer quotes for the user).  This is why we use the
   * mutt_buffer_quote_filename() form with add_outer of false. */
  struct Buffer *quoted = mutt_buffer_pool_get();
  switch (op)
  {
    case 'f':
      /* Compressed file */
      mutt_buffer_quote_filename(quoted, m->realpath, false);
      snprintf(buf, buflen, "%s", mutt_b2s(quoted));
      break;
    case 't':
      /* Plaintext, temporary file */
      mutt_buffer_quote_filename(quoted, mailbox_path(m), false);
      snprintf(buf, buflen, "%s", mutt_b2s(quoted));
      break;
  }

  mutt_buffer_pool_release(&quoted);
  return src;
}

/**
 * expand_command_str - Expand placeholders in command string
 * @param m      Mailbox for paths
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
static void expand_command_str(const struct Mailbox *m, const char *cmd, char *buf, int buflen)
{
  if (!m || !cmd || !buf)
    return;

  mutt_expando_format(buf, buflen, 0, buflen, cmd, compress_format_str,
                      (intptr_t) m, MUTT_FORMAT_NO_FLAGS);
}

/**
 * execute_command - Run a system command
 * @param m        Mailbox to work with
 * @param command  Command string to execute
 * @param progress Message to show the user
 * @retval 1 Success
 * @retval 0 Failure
 *
 * Run the supplied command, taking care of all the NeoMutt requirements,
 * such as locking files and blocking signals.
 */
static int execute_command(struct Mailbox *m, const char *command, const char *progress)
{
  if (!m || !command || !progress)
    return 0;

  if (m->verbose)
    mutt_message(progress, m->realpath);

  int rc = 1;
  char sys_cmd[STR_COMMAND];

  mutt_sig_block();
  endwin();
  fflush(stdout);

  expand_command_str(m, command, sys_cmd, sizeof(sys_cmd));

  if (mutt_system(sys_cmd) != 0)
  {
    rc = 0;
    mutt_any_key_to_continue(NULL);
    mutt_error(_("Error running \"%s\""), sys_cmd);
  }

  mutt_sig_unblock();

  return rc;
}

/**
 * mutt_comp_can_append - Can we append to this path?
 * @param m Mailbox
 * @retval true  Yes, we can append to the file
 * @retval false No, appending isn't possible
 *
 * To append to a file we can either use an 'append-hook' or a combination of
 * 'open-hook' and 'close-hook'.
 *
 * A match means it's our responsibility to append to the file.
 */
bool mutt_comp_can_append(struct Mailbox *m)
{
  if (!m)
    return false;

  /* If this succeeds, we know there's an open-hook */
  struct CompressInfo *ci = set_compress_info(m);
  if (!ci)
    return false;

  /* We have an open-hook, so to append we need an append-hook,
   * or a close-hook. */
  if (ci->cmd_append || ci->cmd_close)
    return true;

  mutt_error(_("Can't append without an append-hook or close-hook : %s"), mailbox_path(m));
  return false;
}

/**
 * mutt_comp_can_read - Can we read from this file?
 * @param path Pathname of file to be tested
 * @retval true  Yes, we can read the file
 * @retval false No, we can't read the file
 *
 * Search for an 'open-hook' with a regex that matches the path.
 *
 * A match means it's our responsibility to open the file.
 */
bool mutt_comp_can_read(const char *path)
{
  if (!path)
    return false;

  if (mutt_find_hook(MUTT_OPEN_HOOK, path))
    return true;

  return false;
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

  return strstr(cmd, "%f") && strstr(cmd, "%t");
}

/**
 * comp_ac_find - Find an Account that matches a Mailbox path - Implements MxOps::ac_find()
 */
static struct Account *comp_ac_find(struct Account *a, const char *path)
{
  return NULL;
}

/**
 * comp_ac_add - Add a Mailbox to an Account - Implements MxOps::ac_add()
 */
static int comp_ac_add(struct Account *a, struct Mailbox *m)
{
  if (!a || !m || (m->type != MUTT_COMPRESSED))
    return -1;
  return 0;
}

/**
 * comp_mbox_open - Open a Mailbox - Implements MxOps::mbox_open()
 *
 * Set up a compressed mailbox to be read.
 * Decompress the mailbox and set up the paths and hooks needed.
 * Then determine the type of the mailbox so we can delegate the handling of
 * messages.
 */
static int comp_mbox_open(struct Mailbox *m)
{
  if (!m || (m->type != MUTT_COMPRESSED))
    return -1;

  struct CompressInfo *ci = set_compress_info(m);
  if (!ci)
    return -1;

  int rc;

  /* If there's no close-hook, or the file isn't writable */
  if (!ci->cmd_close || (access(mailbox_path(m), W_OK) != 0))
    m->readonly = true;

  if (setup_paths(m) != 0)
    goto cmo_fail;
  store_size(m);

  if (!lock_realpath(m, false))
  {
    mutt_error(_("Unable to lock mailbox"));
    goto cmo_fail;
  }

  rc = execute_command(m, ci->cmd_open, _("Decompressing %s"));
  if (rc == 0)
    goto cmo_fail;

  unlock_realpath(m);

  m->type = mx_path_probe(mailbox_path(m));
  if (m->type == MUTT_UNKNOWN)
  {
    mutt_error(_("Can't identify the contents of the compressed file"));
    goto cmo_fail;
  }

  ci->child_ops = mx_get_ops(m->type);
  if (!ci->child_ops)
  {
    mutt_error(_("Can't find mailbox ops for mailbox type %d"), m->type);
    goto cmo_fail;
  }

  m->account->type = m->type;
  return ci->child_ops->mbox_open(m);

cmo_fail:
  /* remove the partial uncompressed file */
  remove(mailbox_path(m));
  compress_info_free(m);
  return -1;
}

/**
 * comp_mbox_open_append - Open a Mailbox for appending - Implements MxOps::mbox_open_append()
 *
 * flags may also contain #MUTT_NEWFOLDER
 *
 * To append to a compressed mailbox we need an append-hook (or both open- and
 * close-hooks).
 */
static int comp_mbox_open_append(struct Mailbox *m, OpenMailboxFlags flags)
{
  if (!m)
    return -1;

  /* If this succeeds, we know there's an open-hook */
  struct CompressInfo *ci = set_compress_info(m);
  if (!ci)
    return -1;

  /* To append we need an append-hook or a close-hook */
  if (!ci->cmd_append && !ci->cmd_close)
  {
    mutt_error(_("Can't append without an append-hook or close-hook : %s"),
               mailbox_path(m));
    goto cmoa_fail1;
  }

  if (setup_paths(m) != 0)
    goto cmoa_fail2;

  /* Lock the realpath for the duration of the append.
   * It will be unlocked in the close */
  if (!lock_realpath(m, true))
  {
    mutt_error(_("Unable to lock mailbox"));
    goto cmoa_fail2;
  }

  /* Open the existing mailbox, unless we are appending */
  if (!ci->cmd_append && (mutt_file_get_size(m->realpath) > 0))
  {
    int rc = execute_command(m, ci->cmd_open, _("Decompressing %s"));
    if (rc == 0)
    {
      mutt_error(_("Compress command failed: %s"), ci->cmd_open);
      goto cmoa_fail2;
    }
    m->type = mx_path_probe(mailbox_path(m));
  }
  else
    m->type = C_MboxType;

  /* We can only deal with mbox and mmdf mailboxes */
  if ((m->type != MUTT_MBOX) && (m->type != MUTT_MMDF))
  {
    mutt_error(_("Unsupported mailbox type for appending"));
    goto cmoa_fail2;
  }

  ci->child_ops = mx_get_ops(m->type);
  if (!ci->child_ops)
  {
    mutt_error(_("Can't find mailbox ops for mailbox type %d"), m->type);
    goto cmoa_fail2;
  }

  if (ci->child_ops->mbox_open_append(m, flags) != 0)
    goto cmoa_fail2;

  return 0;

cmoa_fail2:
  /* remove the partial uncompressed file */
  remove(mailbox_path(m));
cmoa_fail1:
  /* Free the compress_info to prevent close from trying to recompress */
  compress_info_free(m);

  return -1;
}

/**
 * comp_mbox_check - Check for new mail - Implements MxOps::mbox_check()
 * @param m          Mailbox
 * @retval 0              Mailbox OK
 * @retval #MUTT_REOPENED The mailbox was closed and reopened
 * @retval -1             Mailbox bad
 *
 * If the compressed file changes in size but the mailbox hasn't been changed
 * in NeoMutt, then we can close and reopen the mailbox.
 *
 * If the mailbox has been changed in NeoMutt, warn the user.
 *
 * The return codes are picked to match mx_mbox_check().
 */
static int comp_mbox_check(struct Mailbox *m)
{
  if (!m || !m->compress_info)
    return -1;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  int size = mutt_file_get_size(m->realpath);
  if (size == ci->size)
    return 0;

  if (!lock_realpath(m, false))
  {
    mutt_error(_("Unable to lock mailbox"));
    return -1;
  }

  int rc = execute_command(m, ci->cmd_open, _("Decompressing %s"));
  store_size(m);
  unlock_realpath(m);
  if (rc == 0)
    return -1;

  return ops->mbox_check(m);
}

/**
 * comp_mbox_sync - Save changes to the Mailbox - Implements MxOps::mbox_sync()
 *
 * Changes in NeoMutt only affect the tmp file.
 * Calling comp_mbox_sync() will commit them to the compressed file.
 */
static int comp_mbox_sync(struct Mailbox *m)
{
  if (!m || !m->compress_info)
    return -1;

  struct CompressInfo *ci = m->compress_info;

  if (!ci->cmd_close)
  {
    mutt_error(_("Can't sync a compressed file without a close-hook"));
    return -1;
  }

  const struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  if (!lock_realpath(m, true))
  {
    mutt_error(_("Unable to lock mailbox"));
    return -1;
  }

  int rc = comp_mbox_check(m);
  if (rc != 0)
    goto sync_cleanup;

  rc = ops->mbox_sync(m);
  if (rc != 0)
    goto sync_cleanup;

  rc = execute_command(m, ci->cmd_close, _("Compressing %s"));
  if (rc == 0)
  {
    rc = -1;
    goto sync_cleanup;
  }

  rc = 0;

sync_cleanup:
  store_size(m);
  unlock_realpath(m);
  return rc;
}

/**
 * comp_mbox_close - Close a Mailbox - Implements MxOps::mbox_close()
 *
 * If the mailbox has been changed then re-compress the tmp file.
 * Then delete the tmp file.
 */
static int comp_mbox_close(struct Mailbox *m)
{
  if (!m || !m->compress_info)
    return -1;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops)
  {
    compress_info_free(m);
    return -1;
  }

  ops->mbox_close(m);

  /* sync has already been called, so we only need to delete some files */
  if (m->append)
  {
    const char *append = NULL;
    const char *msg = NULL;

    /* The file exists and we can append */
    if ((access(m->realpath, F_OK) == 0) && ci->cmd_append)
    {
      append = ci->cmd_append;
      msg = _("Compressed-appending to %s...");
    }
    else
    {
      append = ci->cmd_close;
      msg = _("Compressing %s");
    }

    int rc = execute_command(m, append, msg);
    if (rc == 0)
    {
      mutt_any_key_to_continue(NULL);
      mutt_error(_("Error. Preserving temporary file: %s"), mailbox_path(m));
    }
    else
      remove(mailbox_path(m));

    unlock_realpath(m);
  }
  else
  {
    /* If the file was removed, remove the compressed folder too */
    if ((access(mailbox_path(m), F_OK) != 0) && !C_SaveEmpty)
    {
      remove(m->realpath);
    }
    else
    {
      remove(mailbox_path(m));
    }
  }

  compress_info_free(m);

  return 0;
}

/**
 * comp_msg_open - Open an email message in a Mailbox - Implements MxOps::msg_open()
 */
static int comp_msg_open(struct Mailbox *m, struct Message *msg, int msgno)
{
  if (!m || !m->compress_info)
    return -1;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->msg_open(m, msg, msgno);
}

/**
 * comp_msg_open_new - Open a new message in a Mailbox - Implements MxOps::msg_open_new()
 */
static int comp_msg_open_new(struct Mailbox *m, struct Message *msg, struct Email *e)
{
  if (!m || !m->compress_info)
    return -1;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->msg_open_new(m, msg, e);
}

/**
 * comp_msg_commit - Save changes to an email - Implements MxOps::msg_commit()
 */
static int comp_msg_commit(struct Mailbox *m, struct Message *msg)
{
  if (!m || !m->compress_info)
    return -1;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->msg_commit(m, msg);
}

/**
 * comp_msg_close - Close an email - Implements MxOps::msg_close()
 */
static int comp_msg_close(struct Mailbox *m, struct Message *msg)
{
  if (!m || !m->compress_info)
    return -1;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops)
    return -1;

  /* Delegate */
  return ops->msg_close(m, msg);
}

/**
 * comp_msg_padding_size - Bytes of padding between messages - Implements MxOps::msg_padding_size()
 */
static int comp_msg_padding_size(struct Mailbox *m)
{
  if (!m || !m->compress_info)
    return 0;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops || !ops->msg_padding_size)
    return 0;

  return ops->msg_padding_size(m);
}

/**
 * comp_msg_save_hcache - Save message to the header cache - Implements MxOps::msg_save_hcache()
 */
static int comp_msg_save_hcache(struct Mailbox *m, struct Email *e)
{
  if (!m || !m->compress_info)
    return 0;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops || !ops->msg_save_hcache)
    return 0;

  return ops->msg_save_hcache(m, e);
}

/**
 * comp_tags_edit - Prompt and validate new messages tags - Implements MxOps::tags_edit()
 */
static int comp_tags_edit(struct Mailbox *m, const char *tags, char *buf, size_t buflen)
{
  if (!m || !m->compress_info)
    return 0;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops || !ops->tags_edit)
    return 0;

  return ops->tags_edit(m, tags, buf, buflen);
}

/**
 * comp_tags_commit - Save the tags to a message - Implements MxOps::tags_commit()
 */
static int comp_tags_commit(struct Mailbox *m, struct Email *e, char *buf)
{
  if (!m || !m->compress_info)
    return 0;

  struct CompressInfo *ci = m->compress_info;

  const struct MxOps *ops = ci->child_ops;
  if (!ops || !ops->tags_commit)
    return 0;

  return ops->tags_commit(m, e, buf);
}

/**
 * comp_path_probe - Is this a compressed Mailbox? - Implements MxOps::path_probe()
 */
static enum MailboxType comp_path_probe(const char *path, const struct stat *st)
{
  if (!path)
    return MUTT_UNKNOWN;

  if (!st || !S_ISREG(st->st_mode))
    return MUTT_UNKNOWN;

  if (mutt_comp_can_read(path))
    return MUTT_COMPRESSED;

  return MUTT_UNKNOWN;
}

/**
 * comp_path_canon - Canonicalise a Mailbox path - Implements MxOps::path_canon()
 */
static int comp_path_canon(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  mutt_path_canon(buf, buflen, HomeDir, false);
  return 0;
}

/**
 * comp_path_pretty - Abbreviate a Mailbox path - Implements MxOps::path_pretty()
 */
static int comp_path_pretty(char *buf, size_t buflen, const char *folder)
{
  if (!buf)
    return -1;

  if (mutt_path_abbr_folder(buf, buflen, folder))
    return 0;

  if (mutt_path_pretty(buf, buflen, HomeDir, false))
    return 0;

  return -1;
}

/**
 * comp_path_parent - Find the parent of a Mailbox path - Implements MxOps::path_parent()
 */
static int comp_path_parent(char *buf, size_t buflen)
{
  if (!buf)
    return -1;

  if (mutt_path_parent(buf, buflen))
    return 0;

  if (buf[0] == '~')
    mutt_path_canon(buf, buflen, HomeDir, false);

  if (mutt_path_parent(buf, buflen))
    return 0;

  return -1;
}

// clang-format off
/**
 * MxCompOps - Compressed Mailbox - Implements ::MxOps
 *
 * Compress only uses open, close and check.
 * The message functions are delegated to mbox.
 */
struct MxOps MxCompOps = {
  .type            = MUTT_COMPRESSED,
  .name             = "compressed",
  .is_local         = true,
  .ac_find          = comp_ac_find,
  .ac_add           = comp_ac_add,
  .mbox_open        = comp_mbox_open,
  .mbox_open_append = comp_mbox_open_append,
  .mbox_check       = comp_mbox_check,
  .mbox_check_stats = NULL,
  .mbox_sync        = comp_mbox_sync,
  .mbox_close       = comp_mbox_close,
  .msg_open         = comp_msg_open,
  .msg_open_new     = comp_msg_open_new,
  .msg_commit       = comp_msg_commit,
  .msg_close        = comp_msg_close,
  .msg_padding_size = comp_msg_padding_size,
  .msg_save_hcache  = comp_msg_save_hcache,
  .tags_edit        = comp_tags_edit,
  .tags_commit      = comp_tags_commit,
  .path_probe       = comp_path_probe,
  .path_canon       = comp_path_canon,
  .path_pretty      = comp_path_pretty,
  .path_parent      = comp_path_parent,
};
// clang-format on
