/**
 * @file
 * Handling of email attachments
 *
 * @authors
 * Copyright (C) 1996-2000,2002,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2004,2006 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page attach_mutt_attach Shared attachments functions
 *
 * Handling of email attachments
 */

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt_attach.h"
#include "lib.h"
#include "ncrypt/lib.h"
#include "pager/lib.h"
#include "question/lib.h"
#include "send/lib.h"
#include "attach.h"
#include "cid.h"
#include "copy.h"
#include "globals.h"
#include "handler.h"
#include "mailcap.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"
#include "rfc3676.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/**
 * mutt_get_tmp_attachment - Get a temporary copy of an attachment
 * @param b Attachment to copy
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_get_tmp_attachment(struct Body *b)
{
  char type[256] = { 0 };

  if (b->unlink)
    return 0;

  struct Buffer *tmpfile = buf_pool_get();
  struct MailcapEntry *entry = mailcap_entry_new();
  snprintf(type, sizeof(type), "%s/%s", TYPE(b), b->subtype);
  mailcap_lookup(b, type, sizeof(type), entry, MUTT_MC_NO_FLAGS);
  mailcap_expand_filename(entry->nametemplate, b->filename, tmpfile);

  mailcap_entry_free(&entry);

  FILE *fp_in = NULL, *fp_out = NULL;
  if ((fp_in = fopen(b->filename, "r")) &&
      (fp_out = mutt_file_fopen(buf_string(tmpfile), "w")))
  {
    mutt_file_copy_stream(fp_in, fp_out);
    mutt_str_replace(&b->filename, buf_string(tmpfile));
    b->unlink = true;

    struct stat st = { 0 };
    if ((fstat(fileno(fp_in), &st) == 0) && (b->stamp >= st.st_mtime))
    {
      mutt_stamp_attachment(b);
    }
  }
  else
  {
    mutt_perror("%s", fp_in ? buf_string(tmpfile) : b->filename);
  }

  mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);

  buf_pool_release(&tmpfile);

  return b->unlink ? 0 : -1;
}

/**
 * mutt_compose_attachment - Create an attachment
 * @param b Body of email
 * @retval 1 Require full screen redraw
 * @retval 0 Otherwise
 */
int mutt_compose_attachment(struct Body *b)
{
  char type[256] = { 0 };
  struct MailcapEntry *entry = mailcap_entry_new();
  bool unlink_newfile = false;
  int rc = 0;
  struct Buffer *cmd = buf_pool_get();
  struct Buffer *newfile = buf_pool_get();
  struct Buffer *tmpfile = buf_pool_get();

  snprintf(type, sizeof(type), "%s/%s", TYPE(b), b->subtype);
  if (mailcap_lookup(b, type, sizeof(type), entry, MUTT_MC_COMPOSE))
  {
    if (entry->composecommand || entry->composetypecommand)
    {
      if (entry->composetypecommand)
        buf_strcpy(cmd, entry->composetypecommand);
      else
        buf_strcpy(cmd, entry->composecommand);

      mailcap_expand_filename(entry->nametemplate, b->filename, newfile);
      mutt_debug(LL_DEBUG1, "oldfile: %s     newfile: %s\n", b->filename,
                 buf_string(newfile));
      if (mutt_file_symlink(b->filename, buf_string(newfile)) == -1)
      {
        if (query_yesorno(_("Can't match 'nametemplate', continue?"), MUTT_YES) != MUTT_YES)
          goto bailout;
        buf_strcpy(newfile, b->filename);
      }
      else
      {
        unlink_newfile = true;
      }

      if (mailcap_expand_command(b, buf_string(newfile), type, cmd))
      {
        /* For now, editing requires a file, no piping */
        mutt_error(_("Mailcap compose entry requires %%s"));
      }
      else
      {
        int r;

        mutt_endwin();
        r = mutt_system(buf_string(cmd));
        if (r == -1)
          mutt_error(_("Error running \"%s\""), buf_string(cmd));

        if ((r != -1) && entry->composetypecommand)
        {
          FILE *fp = mutt_file_fopen(b->filename, "r");
          if (!fp)
          {
            mutt_perror(_("Failure to open file to parse headers"));
            goto bailout;
          }

          struct Body *b_mime = mutt_read_mime_header(fp, 0);
          if (b_mime)
          {
            if (!TAILQ_EMPTY(&b_mime->parameter))
            {
              mutt_param_free(&b->parameter);
              b->parameter = b_mime->parameter;
              TAILQ_INIT(&b_mime->parameter);
            }
            if (b_mime->description)
            {
              FREE(&b->description);
              b->description = b_mime->description;
              b_mime->description = NULL;
            }
            if (b_mime->form_name)
            {
              FREE(&b->form_name);
              b->form_name = b_mime->form_name;
              b_mime->form_name = NULL;
            }

            /* Remove headers by copying out data to another file, then
             * copying the file back */
            const LOFF_T offset = b_mime->offset;
            mutt_body_free(&b_mime);
            if (!mutt_file_seek(fp, offset, SEEK_SET))
            {
              goto bailout;
            }

            buf_mktemp(tmpfile);
            FILE *fp_tmp = mutt_file_fopen(buf_string(tmpfile), "w");
            if (!fp_tmp)
            {
              mutt_perror(_("Failure to open file to strip headers"));
              mutt_file_fclose(&fp);
              goto bailout;
            }
            mutt_file_copy_stream(fp, fp_tmp);
            mutt_file_fclose(&fp);
            mutt_file_fclose(&fp_tmp);
            mutt_file_unlink(b->filename);
            if (mutt_file_rename(buf_string(tmpfile), b->filename) != 0)
            {
              mutt_perror(_("Failure to rename file"));
              goto bailout;
            }
          }
        }
      }
    }
  }
  else
  {
    mutt_message(_("No mailcap compose entry for %s, creating empty file"), type);
    rc = 1;
    goto bailout;
  }

  rc = 1;

bailout:

  if (unlink_newfile)
    unlink(buf_string(newfile));

  buf_pool_release(&cmd);
  buf_pool_release(&newfile);
  buf_pool_release(&tmpfile);

  mailcap_entry_free(&entry);
  return rc;
}

/**
 * mutt_edit_attachment - Edit an attachment
 * @param b Email containing attachment
 * @retval true  Editor found
 * @retval false Editor not found
 *
 * Currently, this only works for send mode, as it assumes that the
 * Body->filename actually contains the information.  I'm not sure
 * we want to deal with editing attachments we've already received,
 * so this should be ok.
 *
 * Returning 0 is useful to tell the calling menu to redraw
 */
bool mutt_edit_attachment(struct Body *b)
{
  char type[256] = { 0 };
  struct MailcapEntry *entry = mailcap_entry_new();
  bool unlink_newfile = false;
  bool rc = false;
  struct Buffer *cmd = buf_pool_get();
  struct Buffer *newfile = buf_pool_get();

  snprintf(type, sizeof(type), "%s/%s", TYPE(b), b->subtype);
  if (mailcap_lookup(b, type, sizeof(type), entry, MUTT_MC_EDIT))
  {
    if (entry->editcommand)
    {
      buf_strcpy(cmd, entry->editcommand);
      mailcap_expand_filename(entry->nametemplate, b->filename, newfile);
      mutt_debug(LL_DEBUG1, "oldfile: %s     newfile: %s\n", b->filename,
                 buf_string(newfile));
      if (mutt_file_symlink(b->filename, buf_string(newfile)) == -1)
      {
        if (query_yesorno(_("Can't match 'nametemplate', continue?"), MUTT_YES) != MUTT_YES)
          goto bailout;
        buf_strcpy(newfile, b->filename);
      }
      else
      {
        unlink_newfile = true;
      }

      if (mailcap_expand_command(b, buf_string(newfile), type, cmd))
      {
        /* For now, editing requires a file, no piping */
        mutt_error(_("Mailcap Edit entry requires %%s"));
        goto bailout;
      }
      else
      {
        mutt_endwin();
        if (mutt_system(buf_string(cmd)) == -1)
        {
          mutt_error(_("Error running \"%s\""), buf_string(cmd));
          goto bailout;
        }
      }
    }
  }
  else if (b->type == TYPE_TEXT)
  {
    /* On text, default to editor */
    const char *const c_editor = cs_subset_string(NeoMutt->sub, "editor");
    mutt_edit_file(NONULL(c_editor), b->filename);
  }
  else
  {
    mutt_error(_("No mailcap edit entry for %s"), type);
    goto bailout;
  }

  rc = true;

bailout:

  if (unlink_newfile)
    unlink(buf_string(newfile));

  buf_pool_release(&cmd);
  buf_pool_release(&newfile);

  mailcap_entry_free(&entry);
  return rc;
}

/**
 * mutt_check_lookup_list - Update the mime type
 * @param b    Message attachment body
 * @param type Buffer with mime type of attachment in "type/subtype" format
 * @param len  Buffer length
 */
void mutt_check_lookup_list(struct Body *b, char *type, size_t len)
{
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, &MimeLookupList, entries)
  {
    const int i = mutt_str_len(np->data) - 1;
    if (((i > 0) && (np->data[i - 1] == '/') && (np->data[i] == '*') &&
         mutt_istrn_equal(type, np->data, i)) ||
        mutt_istr_equal(type, np->data))
    {
      struct Body tmp = { 0 };
      enum ContentType n;
      if ((n = mutt_lookup_mime_type(&tmp, b->filename)) != TYPE_OTHER ||
          (n = mutt_lookup_mime_type(&tmp, b->description)) != TYPE_OTHER)
      {
        snprintf(type, len, "%s/%s",
                 (n == TYPE_AUDIO)       ? "audio" :
                 (n == TYPE_APPLICATION) ? "application" :
                 (n == TYPE_IMAGE)       ? "image" :
                 (n == TYPE_MESSAGE)     ? "message" :
                 (n == TYPE_MODEL)       ? "model" :
                 (n == TYPE_MULTIPART)   ? "multipart" :
                 (n == TYPE_TEXT)        ? "text" :
                 (n == TYPE_VIDEO)       ? "video" :
                                           "other",
                 tmp.subtype);
        mutt_debug(LL_DEBUG1, "\"%s\" -> %s\n", b->filename, type);
      }
      FREE(&tmp.subtype);
      FREE(&tmp.xtype);
    }
  }
}

/**
 * wait_interactive_filter - Wait after an interactive filter
 * @param pid Process id of the process to wait for
 * @retval num Exit status of the process identified by pid
 * @retval -1  Error
 *
 * This is used for filters that are actually interactive commands
 * with input piped in: e.g. in mutt_view_attachment(), a mailcap
 * entry without copiousoutput _and_ without a %s.
 *
 * For those cases, we treat it like a blocking system command, and
 * poll IMAP to keep connections open.
 */
static int wait_interactive_filter(pid_t pid)
{
  int rc;

#ifdef USE_IMAP
  rc = imap_wait_keep_alive(pid);
#else
  waitpid(pid, &rc, 0);
#endif
  mutt_sig_unblock_system(true);
  rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : -1;

  return rc;
}

/**
 * mutt_view_attachment - View an attachment
 * @param fp     Source file stream. Can be NULL
 * @param b      The message body containing the attachment
 * @param mode   How the attachment should be viewed, see #ViewAttachMode
 * @param e      Current Email. Can be NULL
 * @param actx   Attachment context
 * @param win    Window
 * @retval 0   The viewer is run and exited successfully
 * @retval -1  Error
 * @retval num Return value of mutt_do_pager() when it is used
 *
 * Display a message attachment using the viewer program configured in mailcap.
 * If there is no mailcap entry for a file type, view the image as text.
 * Viewer processes are opened and waited on synchronously so viewing an
 * attachment this way will block the main neomutt process until the viewer process
 * exits.
 */
int mutt_view_attachment(FILE *fp, struct Body *b, enum ViewAttachMode mode,
                         struct Email *e, struct AttachCtx *actx, struct MuttWindow *win)
{
  bool use_mailcap = false;
  bool use_pipe = false;
  bool use_pager = true;
  char type[256] = { 0 };
  char desc[256] = { 0 };
  char *fname = NULL;
  struct MailcapEntry *entry = NULL;
  int rc = -1;
  bool has_tempfile = false;
  bool unlink_pagerfile = false;

  bool is_message = mutt_is_message_type(b->type, b->subtype);
  if ((WithCrypto != 0) && is_message && b->email &&
      (b->email->security & SEC_ENCRYPT) && !crypt_valid_passphrase(b->email->security))
  {
    return rc;
  }

  struct Buffer *tmpfile = buf_pool_get();
  struct Buffer *pagerfile = buf_pool_get();
  struct Buffer *cmd = buf_pool_get();

  use_mailcap = ((mode == MUTT_VA_MAILCAP) ||
                 ((mode == MUTT_VA_REGULAR) && mutt_needs_mailcap(b)) ||
                 (mode == MUTT_VA_PAGER));
  snprintf(type, sizeof(type), "%s/%s", TYPE(b), b->subtype);

  char columns[16] = { 0 };
  snprintf(columns, sizeof(columns), "%d", win->state.cols);
  envlist_set(&EnvList, "COLUMNS", columns, true);

  if (use_mailcap)
  {
    entry = mailcap_entry_new();
    enum MailcapLookup mailcap_opt = (mode == MUTT_VA_PAGER) ? MUTT_MC_AUTOVIEW : MUTT_MC_NO_FLAGS;
    if (!mailcap_lookup(b, type, sizeof(type), entry, mailcap_opt))
    {
      if ((mode == MUTT_VA_REGULAR) || (mode == MUTT_VA_PAGER))
      {
        /* fallback to view as text */
        mailcap_entry_free(&entry);
        mutt_error(_("No matching mailcap entry found.  Viewing as text."));
        mode = MUTT_VA_AS_TEXT;
        use_mailcap = false;
      }
      else
      {
        goto return_error;
      }
    }
  }

  if (use_mailcap)
  {
    if (!entry->command)
    {
      mutt_error(_("MIME type not defined.  Can't view attachment."));
      goto return_error;
    }
    buf_strcpy(cmd, entry->command);

    fname = mutt_str_dup(b->filename);
    /* In send mode(!fp), we allow slashes because those are part of
     * the tmpfile.  The path will be removed in expand_filename */
    mutt_file_sanitize_filename(fname, fp ? true : false);
    mailcap_expand_filename(entry->nametemplate, fname, tmpfile);
    FREE(&fname);

    if (mutt_save_attachment(fp, b, buf_string(tmpfile), 0, NULL) == -1)
      goto return_error;
    has_tempfile = true;

    mutt_rfc3676_space_unstuff_attachment(b, buf_string(tmpfile));

    /* check for multipart/related and save attachments with b Content-ID */
    if (mutt_str_equal(type, "text/html"))
    {
      struct Body *related_ancestor = NULL;
      if (actx->body_idx && (WithCrypto != 0) && (e->security & SEC_ENCRYPT))
        related_ancestor = attach_body_ancestor(actx->body_idx[0], b, "related");
      else
        related_ancestor = attach_body_ancestor(e->body, b, "related");
      if (related_ancestor)
      {
        struct CidMapList cid_map_list = STAILQ_HEAD_INITIALIZER(cid_map_list);
        mutt_debug(LL_DEBUG2, "viewing text/html attachment in multipart/related group\n");
        /* save attachments and build cid_map_list Content-ID to filename mapping list */
        cid_save_attachments(related_ancestor->parts, &cid_map_list);
        /* replace Content-IDs with filenames */
        cid_to_filename(tmpfile, &cid_map_list);
        /* empty Content-ID to filename mapping list */
        cid_map_list_clear(&cid_map_list);
      }
    }

    use_pipe = mailcap_expand_command(b, buf_string(tmpfile), type, cmd);
    use_pager = entry->copiousoutput;
  }

  if (use_pager)
  {
    if (fp && !use_mailcap && b->filename)
    {
      /* recv case */
      buf_strcpy(pagerfile, b->filename);
      mutt_adv_mktemp(pagerfile);
    }
    else
    {
      buf_mktemp(pagerfile);
    }
  }

  if (use_mailcap)
  {
    pid_t pid = 0;
    int fd_temp = -1, fd_pager = -1;

    if (!use_pager)
      mutt_endwin();

    const bool c_wait_key = cs_subset_bool(NeoMutt->sub, "wait_key");
    if (use_pager || use_pipe)
    {
      if (use_pager && ((fd_pager = mutt_file_open(buf_string(pagerfile),
                                                   O_CREAT | O_EXCL | O_WRONLY)) == -1))
      {
        mutt_perror("open");
        goto return_error;
      }
      unlink_pagerfile = true;

      if (use_pipe && ((fd_temp = open(buf_string(tmpfile), 0)) == -1))
      {
        if (fd_pager != -1)
          close(fd_pager);
        mutt_perror("open");
        goto return_error;
      }
      unlink_pagerfile = true;

      pid = filter_create_fd(buf_string(cmd), NULL, NULL, NULL, use_pipe ? fd_temp : -1,
                             use_pager ? fd_pager : -1, -1, EnvList);

      if (pid == -1)
      {
        if (fd_pager != -1)
          close(fd_pager);

        if (fd_temp != -1)
          close(fd_temp);

        mutt_error(_("Can't create filter"));
        goto return_error;
      }

      if (use_pager)
      {
        if (b->description)
        {
          snprintf(desc, sizeof(desc), _("---Command: %-20.20s Description: %s"),
                   buf_string(cmd), b->description);
        }
        else
        {
          snprintf(desc, sizeof(desc), _("---Command: %-30.30s Attachment: %s"),
                   buf_string(cmd), type);
        }
        filter_wait(pid);
      }
      else
      {
        if (wait_interactive_filter(pid) || (entry->needsterminal && c_wait_key))
          mutt_any_key_to_continue(NULL);
      }

      if (fd_temp != -1)
        close(fd_temp);
      if (fd_pager != -1)
        close(fd_pager);
    }
    else
    {
      /* interactive cmd */
      int rv = mutt_system(buf_string(cmd));
      if (rv == -1)
        mutt_debug(LL_DEBUG1, "Error running \"%s\"\n", cmd->data);

      if ((rv != 0) || (entry->needsterminal && c_wait_key))
        mutt_any_key_to_continue(NULL);
    }
  }
  else
  {
    /* Don't use mailcap; the attachment is viewed in the pager */

    if (mode == MUTT_VA_AS_TEXT)
    {
      /* just let me see the raw data */
      if (fp)
      {
        /* Viewing from a received message.
         *
         * Don't use mutt_save_attachment() because we want to perform charset
         * conversion since this will be displayed by the internal pager.  */
        struct State state = { 0 };

        state.fp_out = mutt_file_fopen(buf_string(pagerfile), "w");
        if (!state.fp_out)
        {
          mutt_debug(LL_DEBUG1, "mutt_file_fopen(%s) errno=%d %s\n",
                     buf_string(pagerfile), errno, strerror(errno));
          mutt_perror("%s", buf_string(pagerfile));
          goto return_error;
        }
        state.fp_in = fp;
        state.flags = STATE_CHARCONV;
        mutt_decode_attachment(b, &state);
        if (mutt_file_fclose(&state.fp_out) == EOF)
        {
          mutt_debug(LL_DEBUG1, "fclose(%s) errno=%d %s\n",
                     buf_string(pagerfile), errno, strerror(errno));
        }
      }
      else
      {
        /* in compose mode, just copy the file.  we can't use
         * mutt_decode_attachment() since it assumes the content-encoding has
         * already been applied */
        if (mutt_save_attachment(fp, b, buf_string(pagerfile), MUTT_SAVE_NO_FLAGS, NULL))
          goto return_error;
        unlink_pagerfile = true;
      }
      mutt_rfc3676_space_unstuff_attachment(b, buf_string(pagerfile));
    }
    else
    {
      StateFlags flags = STATE_DISPLAY | STATE_DISPLAY_ATTACH;
      const char *const c_pager = pager_get_pager(NeoMutt->sub);
      if (!c_pager)
        flags |= STATE_PAGER;

      /* Use built-in handler */
      if (mutt_decode_save_attachment(fp, b, buf_string(pagerfile), flags, MUTT_SAVE_NO_FLAGS))
      {
        goto return_error;
      }
      unlink_pagerfile = true;
    }

    if (b->description)
      mutt_str_copy(desc, b->description, sizeof(desc));
    else if (b->filename)
      snprintf(desc, sizeof(desc), _("---Attachment: %s: %s"), b->filename, type);
    else
      snprintf(desc, sizeof(desc), _("---Attachment: %s"), type);
  }

  /* We only reach this point if there have been no errors */

  if (use_pager)
  {
    struct PagerData pdata = { 0 };
    struct PagerView pview = { &pdata };

    pdata.actx = actx;
    pdata.body = b;
    pdata.fname = buf_string(pagerfile);
    pdata.fp = fp;

    pview.banner = desc;
    pview.flags = MUTT_PAGER_ATTACHMENT |
                  (is_message ? MUTT_PAGER_MESSAGE : MUTT_PAGER_NO_FLAGS) |
                  ((use_mailcap && entry->xneomuttnowrap) ? MUTT_PAGER_NOWRAP :
                                                            MUTT_PAGER_NO_FLAGS);
    pview.mode = PAGER_MODE_ATTACH;

    rc = mutt_do_pager(&pview, e);

    buf_reset(pagerfile);
    unlink_pagerfile = false;
  }
  else
  {
    rc = 0;
  }

return_error:

  if (!entry || !entry->xneomuttkeep)
  {
    if ((fp && !buf_is_empty(tmpfile)) || has_tempfile)
    {
      /* add temporary file to TempAttachmentsList to be deleted on timeout hook */
      mutt_add_temp_attachment(buf_string(tmpfile));
    }
  }

  mailcap_entry_free(&entry);

  if (unlink_pagerfile)
    mutt_file_unlink(buf_string(pagerfile));

  buf_pool_release(&tmpfile);
  buf_pool_release(&pagerfile);
  buf_pool_release(&cmd);
  envlist_unset(&EnvList, "COLUMNS");

  return rc;
}

/**
 * mutt_pipe_attachment - Pipe an attachment to a command
 * @param fp      File to pipe into the command
 * @param b       Attachment
 * @param path    Path to command
 * @param outfile File to save output to
 * @retval 1 Success
 * @retval 0 Error
 */
int mutt_pipe_attachment(FILE *fp, struct Body *b, const char *path, const char *outfile)
{
  pid_t pid = 0;
  int out = -1, rc = 0;
  bool is_flowed = false;
  bool unlink_unstuff = false;
  FILE *fp_filter = NULL, *fp_unstuff = NULL, *fp_in = NULL;
  struct Buffer *unstuff_tempfile = NULL;

  if (outfile && *outfile)
  {
    out = mutt_file_open(outfile, O_CREAT | O_EXCL | O_WRONLY);
    if (out < 0)
    {
      mutt_perror("open");
      return 0;
    }
  }

  if (mutt_rfc3676_is_format_flowed(b))
  {
    is_flowed = true;
    unstuff_tempfile = buf_pool_get();
    buf_mktemp(unstuff_tempfile);
  }

  mutt_endwin();

  if (outfile && *outfile)
    pid = filter_create_fd(path, &fp_filter, NULL, NULL, -1, out, -1, EnvList);
  else
    pid = filter_create(path, &fp_filter, NULL, NULL, EnvList);
  if (pid < 0)
  {
    mutt_perror(_("Can't create filter"));
    goto bail;
  }

  /* recv case */
  if (fp)
  {
    struct State state = { 0 };

    /* perform charset conversion on text attachments when piping */
    state.flags = STATE_CHARCONV;

    if (is_flowed)
    {
      fp_unstuff = mutt_file_fopen(buf_string(unstuff_tempfile), "w");
      if (!fp_unstuff)
      {
        mutt_perror("mutt_file_fopen");
        goto bail;
      }
      unlink_unstuff = true;

      state.fp_in = fp;
      state.fp_out = fp_unstuff;
      mutt_decode_attachment(b, &state);
      mutt_file_fclose(&fp_unstuff);

      mutt_rfc3676_space_unstuff_attachment(b, buf_string(unstuff_tempfile));

      fp_unstuff = mutt_file_fopen(buf_string(unstuff_tempfile), "r");
      if (!fp_unstuff)
      {
        mutt_perror("mutt_file_fopen");
        goto bail;
      }
      mutt_file_copy_stream(fp_unstuff, fp_filter);
      mutt_file_fclose(&fp_unstuff);
    }
    else
    {
      state.fp_in = fp;
      state.fp_out = fp_filter;
      mutt_decode_attachment(b, &state);
    }
  }
  else
  {
    /* send case */
    const char *infile = NULL;

    if (is_flowed)
    {
      if (mutt_save_attachment(fp, b, buf_string(unstuff_tempfile),
                               MUTT_SAVE_NO_FLAGS, NULL) == -1)
      {
        goto bail;
      }
      unlink_unstuff = true;
      mutt_rfc3676_space_unstuff_attachment(b, buf_string(unstuff_tempfile));
      infile = buf_string(unstuff_tempfile);
    }
    else
    {
      infile = b->filename;
    }

    fp_in = fopen(infile, "r");
    if (!fp_in)
    {
      mutt_perror("fopen");
      goto bail;
    }

    mutt_file_copy_stream(fp_in, fp_filter);
    mutt_file_fclose(&fp_in);
  }

  mutt_file_fclose(&fp_filter);
  rc = 1;

bail:
  if (outfile && *outfile)
  {
    close(out);
    if (rc == 0)
      unlink(outfile);
    else if (is_flowed)
      mutt_rfc3676_space_stuff_attachment(NULL, outfile);
  }

  mutt_file_fclose(&fp_unstuff);
  mutt_file_fclose(&fp_filter);
  mutt_file_fclose(&fp_in);

  if (unlink_unstuff)
    mutt_file_unlink(buf_string(unstuff_tempfile));
  buf_pool_release(&unstuff_tempfile);

  /* check for error exit from child process */
  if ((pid > 0) && (filter_wait(pid) != 0))
    rc = 0;

  const bool c_wait_key = cs_subset_bool(NeoMutt->sub, "wait_key");
  if ((rc == 0) || c_wait_key)
    mutt_any_key_to_continue(NULL);
  return rc;
}

/**
 * save_attachment_open - Open a file to write an attachment to
 * @param path Path to file to open
 * @param opt  Save option, see #SaveAttach
 * @retval ptr File handle to attachment file
 */
static FILE *save_attachment_open(const char *path, enum SaveAttach opt)
{
  if (opt == MUTT_SAVE_APPEND)
    return fopen(path, "a");
  if (opt == MUTT_SAVE_OVERWRITE)
    return fopen(path, "w");

  return mutt_file_fopen(path, "w");
}

/**
 * mutt_save_attachment - Save an attachment
 * @param fp   Source file stream. Can be NULL
 * @param b    Email Body
 * @param path Where to save the attachment
 * @param opt  Save option, see #SaveAttach
 * @param e    Current Email. Can be NULL
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_save_attachment(FILE *fp, struct Body *b, const char *path,
                         enum SaveAttach opt, struct Email *e)
{
  if (!b)
    return -1;

  if (fp)
  {
    /* recv mode */

    if (e && b->email && (b->encoding != ENC_BASE64) &&
        (b->encoding != ENC_QUOTED_PRINTABLE) && mutt_is_message_type(b->type, b->subtype))
    {
      /* message type attachments are written to mail folders. */

      char buf[8192] = { 0 };
      struct Message *msg = NULL;
      CopyHeaderFlags chflags = CH_NO_FLAGS;
      int rc = -1;

      struct Email *e_new = b->email;
      e_new->msgno = e->msgno; /* required for MH/maildir */
      e_new->read = true;

      if (!mutt_file_seek(fp, b->offset, SEEK_SET))
        return -1;
      if (!fgets(buf, sizeof(buf), fp))
        return -1;
      struct Mailbox *m_att = mx_path_resolve(path);
      if (!mx_mbox_open(m_att, MUTT_APPEND | MUTT_QUIET))
      {
        mailbox_free(&m_att);
        return -1;
      }
      msg = mx_msg_open_new(m_att, e_new,
                            is_from(buf, NULL, 0, NULL) ? MUTT_MSG_NO_FLAGS : MUTT_ADD_FROM);
      if (!msg)
      {
        mx_mbox_close(m_att);
        return -1;
      }
      if ((m_att->type == MUTT_MBOX) || (m_att->type == MUTT_MMDF))
        chflags = CH_FROM | CH_UPDATE_LEN;
      chflags |= ((m_att->type == MUTT_MAILDIR) ? CH_NOSTATUS : CH_UPDATE);
      if ((mutt_copy_message_fp(msg->fp, fp, e_new, MUTT_CM_NO_FLAGS, chflags, 0) == 0) &&
          (mx_msg_commit(m_att, msg) == 0))
      {
        rc = 0;
      }
      else
      {
        rc = -1;
      }

      mx_msg_close(m_att, &msg);
      mx_mbox_close(m_att);
      return rc;
    }
    else
    {
      /* In recv mode, extract from folder and decode */

      struct State state = { 0 };

      state.fp_out = save_attachment_open(path, opt);
      if (!state.fp_out)
      {
        mutt_perror("fopen");
        return -1;
      }
      if (!mutt_file_seek((state.fp_in = fp), b->offset, SEEK_SET))
      {
        mutt_file_fclose(&state.fp_out);
        return -1;
      }
      mutt_decode_attachment(b, &state);

      if (mutt_file_fsync_close(&state.fp_out) != 0)
      {
        mutt_perror("fclose");
        return -1;
      }
    }
  }
  else
  {
    if (!b->filename)
      return -1;

    /* In send mode, just copy file */

    FILE *fp_old = fopen(b->filename, "r");
    if (!fp_old)
    {
      mutt_perror("fopen");
      return -1;
    }

    FILE *fp_new = save_attachment_open(path, opt);
    if (!fp_new)
    {
      mutt_perror("fopen");
      mutt_file_fclose(&fp_old);
      return -1;
    }

    if (mutt_file_copy_stream(fp_old, fp_new) == -1)
    {
      mutt_error(_("Write fault"));
      mutt_file_fclose(&fp_old);
      mutt_file_fclose(&fp_new);
      return -1;
    }
    mutt_file_fclose(&fp_old);
    if (mutt_file_fsync_close(&fp_new) != 0)
    {
      mutt_error(_("Write fault"));
      return -1;
    }
  }

  return 0;
}

/**
 * mutt_decode_save_attachment - Decode, then save an attachment
 * @param fp         File to read from (OPTIONAL)
 * @param b          Attachment
 * @param path       Path to save the Attachment to
 * @param flags      Flags, e.g. #STATE_DISPLAY
 * @param opt        Save option, see #SaveAttach
 * @retval 0  Success
 * @retval -1 Error
 */
int mutt_decode_save_attachment(FILE *fp, struct Body *b, const char *path,
                                StateFlags flags, enum SaveAttach opt)
{
  struct State state = { 0 };
  unsigned int saved_encoding = 0;
  struct Body *saved_parts = NULL;
  struct Email *e_saved = NULL;
  int rc = 0;

  state.flags = flags;

  if (opt == MUTT_SAVE_APPEND)
    state.fp_out = fopen(path, "a");
  else if (opt == MUTT_SAVE_OVERWRITE)
    state.fp_out = fopen(path, "w");
  else
    state.fp_out = mutt_file_fopen(path, "w");

  if (!state.fp_out)
  {
    mutt_perror("fopen");
    return -1;
  }

  if (fp)
  {
    state.fp_in = fp;
    state.flags |= STATE_CHARCONV;
  }
  else
  {
    /* When called from the compose menu, the attachment isn't parsed,
     * so we need to do it here. */
    state.fp_in = fopen(b->filename, "r");
    if (!state.fp_in)
    {
      mutt_perror("fopen");
      mutt_file_fclose(&state.fp_out);
      return -1;
    }

    struct stat st = { 0 };
    if (fstat(fileno(state.fp_in), &st) == -1)
    {
      mutt_perror("stat");
      mutt_file_fclose(&state.fp_in);
      mutt_file_fclose(&state.fp_out);
      return -1;
    }

    saved_encoding = b->encoding;
    if (!is_multipart(b))
      b->encoding = ENC_8BIT;

    b->length = st.st_size;
    b->offset = 0;
    saved_parts = b->parts;
    e_saved = b->email;
    mutt_parse_part(state.fp_in, b);

    if (b->noconv || is_multipart(b))
      state.flags |= STATE_CHARCONV;
  }

  mutt_body_handler(b, &state);

  if (mutt_file_fsync_close(&state.fp_out) != 0)
  {
    mutt_perror("fclose");
    rc = -1;
  }
  if (!fp)
  {
    b->length = 0;
    b->encoding = saved_encoding;
    if (saved_parts)
    {
      email_free(&b->email);
      b->parts = saved_parts;
      b->email = e_saved;
    }
    mutt_file_fclose(&state.fp_in);
  }

  return rc;
}

/**
 * mutt_print_attachment - Print out an attachment
 * @param fp File to write to
 * @param b  Attachment
 * @retval 1 Success
 * @retval 0 Error
 *
 * Ok, the difference between send and receive:
 * recv: Body->filename is a suggested name, and Mailbox|Email points
 *       to the attachment in mailbox which is encoded
 * send: Body->filename points to the un-encoded file which contains the
 *       attachment
 */
int mutt_print_attachment(FILE *fp, struct Body *b)
{
  char type[256] = { 0 };
  pid_t pid;
  FILE *fp_in = NULL, *fp_out = NULL;
  bool unlink_newfile = false;
  struct Buffer *newfile = buf_pool_get();
  struct Buffer *cmd = buf_pool_get();

  int rc = 0;

  snprintf(type, sizeof(type), "%s/%s", TYPE(b), b->subtype);

  if (mailcap_lookup(b, type, sizeof(type), NULL, MUTT_MC_PRINT))
  {
    mutt_debug(LL_DEBUG2, "Using mailcap\n");

    struct MailcapEntry *entry = mailcap_entry_new();
    mailcap_lookup(b, type, sizeof(type), entry, MUTT_MC_PRINT);

    char *sanitized_fname = mutt_str_dup(b->filename);
    /* In send mode (!fp), we allow slashes because those are part of
     * the tempfile.  The path will be removed in expand_filename */
    mutt_file_sanitize_filename(sanitized_fname, fp ? true : false);
    mailcap_expand_filename(entry->nametemplate, sanitized_fname, newfile);
    FREE(&sanitized_fname);

    if (mutt_save_attachment(fp, b, buf_string(newfile), MUTT_SAVE_NO_FLAGS, NULL) == -1)
    {
      goto mailcap_cleanup;
    }
    unlink_newfile = 1;

    mutt_rfc3676_space_unstuff_attachment(b, buf_string(newfile));

    buf_strcpy(cmd, entry->printcommand);

    bool piped = mailcap_expand_command(b, buf_string(newfile), type, cmd);

    mutt_endwin();

    const bool c_wait_key = cs_subset_bool(NeoMutt->sub, "wait_key");
    /* interactive program */
    if (piped)
    {
      fp_in = fopen(buf_string(newfile), "r");
      if (!fp_in)
      {
        mutt_perror("fopen");
        mailcap_entry_free(&entry);
        goto mailcap_cleanup;
      }

      pid = filter_create(buf_string(cmd), &fp_out, NULL, NULL, EnvList);
      if (pid < 0)
      {
        mutt_perror(_("Can't create filter"));
        mailcap_entry_free(&entry);
        mutt_file_fclose(&fp_in);
        goto mailcap_cleanup;
      }
      mutt_file_copy_stream(fp_in, fp_out);
      mutt_file_fclose(&fp_out);
      mutt_file_fclose(&fp_in);
      if (filter_wait(pid) || c_wait_key)
        mutt_any_key_to_continue(NULL);
    }
    else
    {
      int rc2 = mutt_system(buf_string(cmd));
      if (rc2 == -1)
        mutt_debug(LL_DEBUG1, "Error running \"%s\"\n", cmd->data);

      if ((rc2 != 0) || c_wait_key)
        mutt_any_key_to_continue(NULL);
    }

    rc = 1;

  mailcap_cleanup:
    if (unlink_newfile)
      mutt_file_unlink(buf_string(newfile));

    mailcap_entry_free(&entry);
    goto out;
  }

  const char *const c_print_command = cs_subset_string(NeoMutt->sub, "print_command");
  if (mutt_istr_equal("text/plain", type) || mutt_istr_equal("application/postscript", type))
  {
    rc = (mutt_pipe_attachment(fp, b, NONULL(c_print_command), NULL));
    goto out;
  }
  else if (mutt_can_decode(b))
  {
    /* decode and print */

    fp_in = NULL;
    fp_out = NULL;

    buf_mktemp(newfile);
    if (mutt_decode_save_attachment(fp, b, buf_string(newfile), STATE_PRINTING,
                                    MUTT_SAVE_NO_FLAGS) == 0)
    {
      unlink_newfile = true;
      mutt_debug(LL_DEBUG2, "successfully decoded %s type attachment to %s\n",
                 type, buf_string(newfile));

      fp_in = fopen(buf_string(newfile), "r");
      if (!fp_in)
      {
        mutt_perror("fopen");
        goto decode_cleanup;
      }

      mutt_debug(LL_DEBUG2, "successfully opened %s read-only\n", buf_string(newfile));

      mutt_endwin();
      pid = filter_create(NONULL(c_print_command), &fp_out, NULL, NULL, EnvList);
      if (pid < 0)
      {
        mutt_perror(_("Can't create filter"));
        goto decode_cleanup;
      }

      mutt_debug(LL_DEBUG2, "Filter created\n");

      mutt_file_copy_stream(fp_in, fp_out);

      mutt_file_fclose(&fp_out);
      mutt_file_fclose(&fp_in);

      const bool c_wait_key = cs_subset_bool(NeoMutt->sub, "wait_key");
      if ((filter_wait(pid) != 0) || c_wait_key)
        mutt_any_key_to_continue(NULL);
      rc = 1;
    }
  decode_cleanup:
    mutt_file_fclose(&fp_in);
    mutt_file_fclose(&fp_out);
    if (unlink_newfile)
      mutt_file_unlink(buf_string(newfile));
  }
  else
  {
    mutt_error(_("I don't know how to print that"));
    rc = 0;
  }

out:
  buf_pool_release(&newfile);
  buf_pool_release(&cmd);

  return rc;
}

/**
 * mutt_add_temp_attachment - Add file to list of temporary attachments
 * @param filename filename with full path
 */
void mutt_add_temp_attachment(const char *filename)
{
  mutt_list_insert_tail(&TempAttachmentsList, mutt_str_dup(filename));
}

/**
 * mutt_temp_attachments_cleanup - Delete all temporary attachments
 */
void mutt_temp_attachments_cleanup(void)
{
  struct ListNode *np = NULL;

  STAILQ_FOREACH(np, &TempAttachmentsList, entries)
  {
    (void) mutt_file_chmod_add(np->data, S_IWUSR);
    mutt_file_unlink(np->data);
  }

  mutt_list_free(&TempAttachmentsList);
}
