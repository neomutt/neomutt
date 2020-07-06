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
 * @page mutt_attach Handling of email attachments
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
#include "context.h"
#include "copy.h"
#include "handler.h"
#include "mailcap.h"
#include "mutt_globals.h"
#include "muttlib.h"
#include "mx.h"
#include "options.h"
#include "pager.h"
#include "protos.h"
#include "rfc3676.h"
#include "state.h"
#include "ncrypt/lib.h"
#include "send/lib.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/**
 * mutt_get_tmp_attachment - Get a temporary copy of an attachment
 * @param a Attachment to copy
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_get_tmp_attachment(struct Body *a)
{
  char type[256];
  struct stat st;

  if (a->unlink)
    return 0;

  struct Buffer *tmpfile = mutt_buffer_pool_get();
  struct MailcapEntry *entry = mailcap_entry_new();
  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  mailcap_lookup(a, type, sizeof(type), entry, MUTT_MC_NO_FLAGS);
  mailcap_expand_filename(entry->nametemplate, a->filename, tmpfile);

  mailcap_entry_free(&entry);

  if (stat(a->filename, &st) == -1)
  {
    mutt_buffer_pool_release(&tmpfile);
    return -1;
  }

  FILE *fp_in = NULL, *fp_out = NULL;
  if ((fp_in = fopen(a->filename, "r")) &&
      (fp_out = mutt_file_fopen(mutt_b2s(tmpfile), "w")))
  {
    mutt_file_copy_stream(fp_in, fp_out);
    mutt_str_replace(&a->filename, mutt_b2s(tmpfile));
    a->unlink = true;

    if (a->stamp >= st.st_mtime)
      mutt_stamp_attachment(a);
  }
  else
    mutt_perror(fp_in ? mutt_b2s(tmpfile) : a->filename);

  mutt_file_fclose(&fp_in);
  mutt_file_fclose(&fp_out);

  mutt_buffer_pool_release(&tmpfile);

  return a->unlink ? 0 : -1;
}

/**
 * mutt_compose_attachment - Create an attachment
 * @param a Body of email
 * @retval 1 if require full screen redraw
 * @retval 0 otherwise
 */
int mutt_compose_attachment(struct Body *a)
{
  char type[256];
  struct MailcapEntry *entry = mailcap_entry_new();
  bool unlink_newfile = false;
  int rc = 0;
  struct Buffer *cmd = mutt_buffer_pool_get();
  struct Buffer *newfile = mutt_buffer_pool_get();
  struct Buffer *tmpfile = mutt_buffer_pool_get();

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  if (mailcap_lookup(a, type, sizeof(type), entry, MUTT_MC_COMPOSE))
  {
    if (entry->composecommand || entry->composetypecommand)
    {
      if (entry->composetypecommand)
        mutt_buffer_strcpy(cmd, entry->composetypecommand);
      else
        mutt_buffer_strcpy(cmd, entry->composecommand);

      mailcap_expand_filename(entry->nametemplate, a->filename, newfile);
      mutt_debug(LL_DEBUG1, "oldfile: %s\t newfile: %s\n", a->filename, mutt_b2s(newfile));
      if (mutt_file_symlink(a->filename, mutt_b2s(newfile)) == -1)
      {
        if (mutt_yesorno(_("Can't match 'nametemplate', continue?"), MUTT_YES) != MUTT_YES)
          goto bailout;
        mutt_buffer_strcpy(newfile, a->filename);
      }
      else
        unlink_newfile = true;

      if (mailcap_expand_command(a, mutt_b2s(newfile), type, cmd))
      {
        /* For now, editing requires a file, no piping */
        mutt_error(_("Mailcap compose entry requires %%s"));
      }
      else
      {
        int r;

        mutt_endwin();
        r = mutt_system(mutt_b2s(cmd));
        if (r == -1)
          mutt_error(_("Error running \"%s\""), mutt_b2s(cmd));

        if ((r != -1) && entry->composetypecommand)
        {
          struct Body *b = NULL;

          FILE *fp = mutt_file_fopen(a->filename, "r");
          if (!fp)
          {
            mutt_perror(_("Failure to open file to parse headers"));
            goto bailout;
          }

          b = mutt_read_mime_header(fp, 0);
          if (b)
          {
            if (!TAILQ_EMPTY(&b->parameter))
            {
              mutt_param_free(&a->parameter);
              a->parameter = b->parameter;
              TAILQ_INIT(&b->parameter);
            }
            if (b->description)
            {
              FREE(&a->description);
              a->description = b->description;
              b->description = NULL;
            }
            if (b->form_name)
            {
              FREE(&a->form_name);
              a->form_name = b->form_name;
              b->form_name = NULL;
            }

            /* Remove headers by copying out data to another file, then
             * copying the file back */
            fseeko(fp, b->offset, SEEK_SET);
            mutt_body_free(&b);
            mutt_buffer_mktemp(tmpfile);
            FILE *fp_tmp = mutt_file_fopen(mutt_b2s(tmpfile), "w");
            if (!fp_tmp)
            {
              mutt_perror(_("Failure to open file to strip headers"));
              mutt_file_fclose(&fp);
              goto bailout;
            }
            mutt_file_copy_stream(fp, fp_tmp);
            mutt_file_fclose(&fp);
            mutt_file_fclose(&fp_tmp);
            mutt_file_unlink(a->filename);
            if (mutt_file_rename(mutt_b2s(tmpfile), a->filename) != 0)
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
    unlink(mutt_b2s(newfile));

  mutt_buffer_pool_release(&cmd);
  mutt_buffer_pool_release(&newfile);
  mutt_buffer_pool_release(&tmpfile);

  mailcap_entry_free(&entry);
  return rc;
}

/**
 * mutt_edit_attachment - Edit an attachment
 * @param a Email containing attachment
 * @retval 1 if editor found
 * @retval 0 if not
 *
 * Currently, this only works for send mode, as it assumes that the
 * Body->filename actually contains the information.  I'm not sure
 * we want to deal with editing attachments we've already received,
 * so this should be ok.
 *
 * Returning 0 is useful to tell the calling menu to redraw
 */
int mutt_edit_attachment(struct Body *a)
{
  char type[256];
  struct MailcapEntry *entry = mailcap_entry_new();
  bool unlink_newfile = false;
  int rc = 0;
  struct Buffer *cmd = mutt_buffer_pool_get();
  struct Buffer *newfile = mutt_buffer_pool_get();

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  if (mailcap_lookup(a, type, sizeof(type), entry, MUTT_MC_EDIT))
  {
    if (entry->editcommand)
    {
      mutt_buffer_strcpy(cmd, entry->editcommand);
      mailcap_expand_filename(entry->nametemplate, a->filename, newfile);
      mutt_debug(LL_DEBUG1, "oldfile: %s\t newfile: %s\n", a->filename, mutt_b2s(newfile));
      if (mutt_file_symlink(a->filename, mutt_b2s(newfile)) == -1)
      {
        if (mutt_yesorno(_("Can't match 'nametemplate', continue?"), MUTT_YES) != MUTT_YES)
          goto bailout;
        mutt_buffer_strcpy(newfile, a->filename);
      }
      else
        unlink_newfile = true;

      if (mailcap_expand_command(a, mutt_b2s(newfile), type, cmd))
      {
        /* For now, editing requires a file, no piping */
        mutt_error(_("Mailcap Edit entry requires %%s"));
        goto bailout;
      }
      else
      {
        mutt_endwin();
        if (mutt_system(mutt_b2s(cmd)) == -1)
        {
          mutt_error(_("Error running \"%s\""), mutt_b2s(cmd));
          goto bailout;
        }
      }
    }
  }
  else if (a->type == TYPE_TEXT)
  {
    /* On text, default to editor */
    mutt_edit_file(NONULL(C_Editor), a->filename);
  }
  else
  {
    mutt_error(_("No mailcap edit entry for %s"), type);
    rc = 0;
    goto bailout;
  }

  rc = 1;

bailout:

  if (unlink_newfile)
    unlink(mutt_b2s(newfile));

  mutt_buffer_pool_release(&cmd);
  mutt_buffer_pool_release(&newfile);

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
                 (n == TYPE_AUDIO) ?
                     "audio" :
                     (n == TYPE_APPLICATION) ?
                     "application" :
                     (n == TYPE_IMAGE) ?
                     "image" :
                     (n == TYPE_MESSAGE) ?
                     "message" :
                     (n == TYPE_MODEL) ?
                     "model" :
                     (n == TYPE_MULTIPART) ?
                     "multipart" :
                     (n == TYPE_TEXT) ? "text" : (n == TYPE_VIDEO) ? "video" : "other",
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
  rc = imap_wait_keepalive(pid);
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
 * @param a      The message body containing the attachment
 * @param mode   How the attachment should be viewed, see #ViewAttachMode
 * @param e      Current Email. Can be NULL
 * @param actx   Attachment context
 * @param win    Window
 * @retval 0   If the viewer is run and exited successfully
 * @retval -1  Error
 * @retval num Return value of mutt_do_pager() when it is used
 *
 * Display a message attachment using the viewer program configured in mailcap.
 * If there is no mailcap entry for a file type, view the image as text.
 * Viewer processes are opened and waited on synchronously so viewing an
 * attachment this way will block the main neomutt process until the viewer process
 * exits.
 */
int mutt_view_attachment(FILE *fp, struct Body *a, enum ViewAttachMode mode,
                         struct Email *e, struct AttachCtx *actx, struct MuttWindow *win)
{
  bool use_mailcap = false;
  bool use_pipe = false;
  bool use_pager = true;
  char type[256];
  char desc[256];
  char *fname = NULL;
  struct MailcapEntry *entry = NULL;
  int rc = -1;
  bool unlink_tempfile = false;
  bool unlink_pagerfile = false;

  bool is_message = mutt_is_message_type(a->type, a->subtype);
  if ((WithCrypto != 0) && is_message && a->email &&
      (a->email->security & SEC_ENCRYPT) && !crypt_valid_passphrase(a->email->security))
  {
    return rc;
  }

  struct Buffer *tmpfile = mutt_buffer_pool_get();
  struct Buffer *pagerfile = mutt_buffer_pool_get();
  struct Buffer *cmd = mutt_buffer_pool_get();

  use_mailcap =
      (mode == MUTT_VA_MAILCAP || (mode == MUTT_VA_REGULAR && mutt_needs_mailcap(a)));
  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);

  char columns[16];
  snprintf(columns, sizeof(columns), "%d", win->state.cols);
  mutt_envlist_set("COLUMNS", columns, true);

  if (use_mailcap)
  {
    entry = mailcap_entry_new();
    if (!mailcap_lookup(a, type, sizeof(type), entry, MUTT_MC_NO_FLAGS))
    {
      if (mode == MUTT_VA_REGULAR)
      {
        /* fallback to view as text */
        mailcap_entry_free(&entry);
        mutt_error(_("No matching mailcap entry found.  Viewing as text."));
        mode = MUTT_VA_AS_TEXT;
        use_mailcap = false;
      }
      else
        goto return_error;
    }
  }

  if (use_mailcap)
  {
    if (!entry->command)
    {
      mutt_error(_("MIME type not defined.  Can't view attachment."));
      goto return_error;
    }
    mutt_buffer_strcpy(cmd, entry->command);

    fname = mutt_str_dup(a->filename);
    /* In send mode(!fp), we allow slashes because those are part of
     * the tmpfile.  The path will be removed in expand_filename */
    mutt_file_sanitize_filename(fname, fp ? true : false);
    mailcap_expand_filename(entry->nametemplate, fname, tmpfile);
    FREE(&fname);

    if (mutt_save_attachment(fp, a, mutt_b2s(tmpfile), 0, NULL) == -1)
      goto return_error;
    unlink_tempfile = true;

    mutt_rfc3676_space_unstuff_attachment(a, mutt_b2s(tmpfile));

    use_pipe = mailcap_expand_command(a, mutt_b2s(tmpfile), type, cmd);
    use_pager = entry->copiousoutput;
  }

  if (use_pager)
  {
    if (fp && !use_mailcap && a->filename)
    {
      /* recv case */
      mutt_buffer_strcpy(pagerfile, a->filename);
      mutt_adv_mktemp(pagerfile);
    }
    else
      mutt_buffer_mktemp(pagerfile);
  }

  if (use_mailcap)
  {
    pid_t pid = 0;
    int fd_temp = -1, fd_pager = -1;

    if (!use_pager)
      mutt_endwin();

    if (use_pager || use_pipe)
    {
      if (use_pager && ((fd_pager = mutt_file_open(mutt_b2s(pagerfile),
                                                   O_CREAT | O_EXCL | O_WRONLY)) == -1))
      {
        mutt_perror("open");
        goto return_error;
      }
      unlink_pagerfile = true;

      if (use_pipe && ((fd_temp = open(mutt_b2s(tmpfile), 0)) == -1))
      {
        if (fd_pager != -1)
          close(fd_pager);
        mutt_perror("open");
        goto return_error;
      }
      unlink_pagerfile = true;

      pid = filter_create_fd(mutt_b2s(cmd), NULL, NULL, NULL,
                             use_pipe ? fd_temp : -1, use_pager ? fd_pager : -1, -1);

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
        if (a->description)
        {
          snprintf(desc, sizeof(desc), _("---Command: %-20.20s Description: %s"),
                   mutt_b2s(cmd), a->description);
        }
        else
        {
          snprintf(desc, sizeof(desc), _("---Command: %-30.30s Attachment: %s"),
                   mutt_b2s(cmd), type);
        }
        filter_wait(pid);
      }
      else
      {
        if (wait_interactive_filter(pid) || (entry->needsterminal && C_WaitKey))
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
      int rv = mutt_system(mutt_b2s(cmd));
      if (rv == -1)
        mutt_debug(LL_DEBUG1, "Error running \"%s\"", cmd->data);

      if ((rv != 0) || (entry->needsterminal && C_WaitKey))
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
        struct State decode_state = { 0 };

        decode_state.fp_out = mutt_file_fopen(mutt_b2s(pagerfile), "w");
        if (!decode_state.fp_out)
        {
          mutt_debug(LL_DEBUG1, "mutt_file_fopen(%s) errno=%d %s\n",
                     mutt_b2s(pagerfile), errno, strerror(errno));
          mutt_perror(mutt_b2s(pagerfile));
          goto return_error;
        }
        decode_state.fp_in = fp;
        decode_state.flags = MUTT_CHARCONV;
        mutt_decode_attachment(a, &decode_state);
        if (mutt_file_fclose(&decode_state.fp_out) == EOF)
        {
          mutt_debug(LL_DEBUG1, "fclose(%s) errno=%d %s\n", mutt_b2s(pagerfile),
                     errno, strerror(errno));
        }
      }
      else
      {
        /* in compose mode, just copy the file.  we can't use
         * mutt_decode_attachment() since it assumes the content-encoding has
         * already been applied */
        if (mutt_save_attachment(fp, a, mutt_b2s(pagerfile), MUTT_SAVE_NO_FLAGS, NULL))
          goto return_error;
        unlink_pagerfile = true;
      }
      mutt_rfc3676_space_unstuff_attachment(a, mutt_b2s(pagerfile));
    }
    else
    {
      /* Use built-in handler */
      OptViewAttach = true; /* disable the "use 'v' to view this part"
                             * message in case of error */
      if (mutt_decode_save_attachment(fp, a, mutt_b2s(pagerfile), MUTT_DISPLAY, MUTT_SAVE_NO_FLAGS))
      {
        OptViewAttach = false;
        goto return_error;
      }
      unlink_pagerfile = true;
      OptViewAttach = false;
    }

    if (a->description)
      mutt_str_copy(desc, a->description, sizeof(desc));
    else if (a->filename)
      snprintf(desc, sizeof(desc), _("---Attachment: %s: %s"), a->filename, type);
    else
      snprintf(desc, sizeof(desc), _("---Attachment: %s"), type);
  }

  /* We only reach this point if there have been no errors */

  if (use_pager)
  {
    struct Pager info = { 0 };
    info.fp = fp;
    info.body = a;
    info.ctx = Context;
    info.actx = actx;
    info.email = e;

    rc = mutt_do_pager(desc, mutt_b2s(pagerfile),
                       MUTT_PAGER_ATTACHMENT | (is_message ? MUTT_PAGER_MESSAGE : MUTT_PAGER_NO_FLAGS),
                       &info);
    mutt_buffer_reset(pagerfile);
    unlink_pagerfile = false;
  }
  else
    rc = 0;

return_error:

  if (!entry || !entry->xneomuttkeep)
  {
    if (fp && !mutt_buffer_is_empty(tmpfile))
    {
      /* add temporary file to TempAttachmentsList to be deleted on timeout hook */
      mutt_add_temp_attachment(mutt_b2s(tmpfile));
    }
    else if (unlink_tempfile)
    {
      unlink(mutt_b2s(tmpfile));
    }
  }

  mailcap_entry_free(&entry);

  if (unlink_pagerfile)
    mutt_file_unlink(mutt_b2s(pagerfile));

  mutt_buffer_pool_release(&tmpfile);
  mutt_buffer_pool_release(&pagerfile);
  mutt_buffer_pool_release(&cmd);
  mutt_envlist_unset("COLUMNS");

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
int mutt_pipe_attachment(FILE *fp, struct Body *b, const char *path, char *outfile)
{
  pid_t pid;
  int out = -1;
  int rc = 0;

  if (outfile && *outfile)
  {
    out = mutt_file_open(outfile, O_CREAT | O_EXCL | O_WRONLY);
    if (out < 0)
    {
      mutt_perror("open");
      return 0;
    }
  }

  mutt_endwin();

  if (fp)
  {
    /* recv case */

    struct State s = { 0 };

    /* perform charset conversion on text attachments when piping */
    s.flags = MUTT_CHARCONV;

    if (outfile && *outfile)
      pid = filter_create_fd(path, &s.fp_out, NULL, NULL, -1, out, -1);
    else
      pid = filter_create(path, &s.fp_out, NULL, NULL);

    if (pid < 0)
    {
      mutt_perror(_("Can't create filter"));
      goto bail;
    }

    s.fp_in = fp;
    mutt_decode_attachment(b, &s);
    mutt_file_fclose(&s.fp_out);
  }
  else
  {
    /* send case */
    FILE *fp_in = fopen(b->filename, "r");
    if (!fp_in)
    {
      mutt_perror("fopen");
      if (outfile && *outfile)
      {
        close(out);
        unlink(outfile);
      }
      return 0;
    }

    FILE *fp_out = NULL;
    if (outfile && *outfile)
      pid = filter_create_fd(path, &fp_out, NULL, NULL, -1, out, -1);
    else
      pid = filter_create(path, &fp_out, NULL, NULL);

    if (pid < 0)
    {
      mutt_perror(_("Can't create filter"));
      mutt_file_fclose(&fp_in);
      goto bail;
    }

    mutt_file_copy_stream(fp_in, fp_out);
    mutt_file_fclose(&fp_out);
    mutt_file_fclose(&fp_in);
  }

  rc = 1;

bail:

  if (outfile && *outfile)
    close(out);

  /* check for error exit from child process */
  if (filter_wait(pid) != 0)
    rc = 0;

  if ((rc == 0) || C_WaitKey)
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
 * @param m    Email Body
 * @param path Where to save the attachment
 * @param opt  Save option, see #SaveAttach
 * @param e    Current Email. Can be NULL
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_save_attachment(FILE *fp, struct Body *m, const char *path,
                         enum SaveAttach opt, struct Email *e)
{
  if (!m)
    return -1;

  if (fp)
  {
    /* recv mode */

    if (e && m->email && (m->encoding != ENC_BASE64) &&
        (m->encoding != ENC_QUOTED_PRINTABLE) && mutt_is_message_type(m->type, m->subtype))
    {
      /* message type attachments are written to mail folders. */

      char buf[8192];
      struct Message *msg = NULL;
      CopyHeaderFlags chflags = CH_NO_FLAGS;
      int rc = -1;

      struct Email *e_new = m->email;
      e_new->msgno = e->msgno; /* required for MH/maildir */
      e_new->read = true;

      if (fseeko(fp, m->offset, SEEK_SET) < 0)
        return -1;
      if (!fgets(buf, sizeof(buf), fp))
        return -1;
      struct Mailbox *m_att = mx_path_resolve(path);
      struct Context *ctx = mx_mbox_open(m_att, MUTT_APPEND | MUTT_QUIET);
      if (!ctx)
      {
        mailbox_free(&m_att);
        return -1;
      }
      msg = mx_msg_open_new(ctx->mailbox, e_new,
                            is_from(buf, NULL, 0, NULL) ? MUTT_MSG_NO_FLAGS : MUTT_ADD_FROM);
      if (!msg)
      {
        mx_mbox_close(&ctx);
        return -1;
      }
      if ((ctx->mailbox->type == MUTT_MBOX) || (ctx->mailbox->type == MUTT_MMDF))
        chflags = CH_FROM | CH_UPDATE_LEN;
      chflags |= ((ctx->mailbox->type == MUTT_MAILDIR) ? CH_NOSTATUS : CH_UPDATE);
      if ((mutt_copy_message_fp(msg->fp, fp, e_new, MUTT_CM_NO_FLAGS, chflags, 0) == 0) &&
          (mx_msg_commit(ctx->mailbox, msg) == 0))
      {
        rc = 0;
      }
      else
      {
        rc = -1;
      }

      mx_msg_close(ctx->mailbox, &msg);
      mx_mbox_close(&ctx);
      return rc;
    }
    else
    {
      /* In recv mode, extract from folder and decode */

      struct State s = { 0 };

      s.fp_out = save_attachment_open(path, opt);
      if (!s.fp_out)
      {
        mutt_perror("fopen");
        return -1;
      }
      fseeko((s.fp_in = fp), m->offset, SEEK_SET);
      mutt_decode_attachment(m, &s);

      if (mutt_file_fsync_close(&s.fp_out) != 0)
      {
        mutt_perror("fclose");
        return -1;
      }
    }
  }
  else
  {
    if (!m->filename)
      return -1;

    /* In send mode, just copy file */

    FILE *fp_old = fopen(m->filename, "r");
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
 * @param m          Attachment
 * @param path       Path to save the Attachment to
 * @param displaying Flags, e.g. #MUTT_DISPLAY
 * @param opt        Save option, see #SaveAttach
 * @retval 0  Success
 * @retval -1 Error
 */
int mutt_decode_save_attachment(FILE *fp, struct Body *m, const char *path,
                                int displaying, enum SaveAttach opt)
{
  struct State s = { 0 };
  unsigned int saved_encoding = 0;
  struct Body *saved_parts = NULL;
  struct Email *e_saved = NULL;
  int rc = 0;

  s.flags = displaying;

  if (opt == MUTT_SAVE_APPEND)
    s.fp_out = fopen(path, "a");
  else if (opt == MUTT_SAVE_OVERWRITE)
    s.fp_out = fopen(path, "w");
  else
    s.fp_out = mutt_file_fopen(path, "w");

  if (!s.fp_out)
  {
    mutt_perror("fopen");
    return -1;
  }

  if (!fp)
  {
    /* When called from the compose menu, the attachment isn't parsed,
     * so we need to do it here. */
    struct stat st;

    if (stat(m->filename, &st) == -1)
    {
      mutt_perror("stat");
      mutt_file_fclose(&s.fp_out);
      return -1;
    }

    s.fp_in = fopen(m->filename, "r");
    if (!s.fp_in)
    {
      mutt_perror("fopen");
      return -1;
    }

    saved_encoding = m->encoding;
    if (!is_multipart(m))
      m->encoding = ENC_8BIT;

    m->length = st.st_size;
    m->offset = 0;
    saved_parts = m->parts;
    e_saved = m->email;
    mutt_parse_part(s.fp_in, m);

    if (m->noconv || is_multipart(m))
      s.flags |= MUTT_CHARCONV;
  }
  else
  {
    s.fp_in = fp;
    s.flags |= MUTT_CHARCONV;
  }

  mutt_body_handler(m, &s);

  if (mutt_file_fsync_close(&s.fp_out) != 0)
  {
    mutt_perror("fclose");
    rc = -1;
  }
  if (!fp)
  {
    m->length = 0;
    m->encoding = saved_encoding;
    if (saved_parts)
    {
      email_free(&m->email);
      m->parts = saved_parts;
      m->email = e_saved;
    }
    mutt_file_fclose(&s.fp_in);
  }

  return rc;
}

/**
 * mutt_print_attachment - Print out an attachment
 * @param fp File to write to
 * @param a  Attachment
 * @retval 1 Success
 * @retval 0 Error
 *
 * Ok, the difference between send and receive:
 * recv: Body->filename is a suggested name, and Mailbox|Email points
 *       to the attachment in mailbox which is encoded
 * send: Body->filename points to the un-encoded file which contains the
 *       attachment
 */
int mutt_print_attachment(FILE *fp, struct Body *a)
{
  char type[256];
  pid_t pid;
  FILE *fp_in = NULL, *fp_out = NULL;
  bool unlink_newfile = false;
  struct Buffer *newfile = mutt_buffer_pool_get();
  struct Buffer *cmd = mutt_buffer_pool_get();

  int rc = 0;

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);

  if (mailcap_lookup(a, type, sizeof(type), NULL, MUTT_MC_PRINT))
  {
    int piped = false;

    mutt_debug(LL_DEBUG2, "Using mailcap\n");

    struct MailcapEntry *entry = mailcap_entry_new();
    mailcap_lookup(a, type, sizeof(type), entry, MUTT_MC_PRINT);
    mailcap_expand_filename(entry->nametemplate, a->filename, newfile);
    /* send mode: symlink from existing file to the newfile */
    if (!fp)
    {
      if (mutt_file_symlink(a->filename, mutt_b2s(newfile)) == -1)
      {
        if (mutt_yesorno(_("Can't match 'nametemplate', continue?"), MUTT_YES) != MUTT_YES)
          goto mailcap_cleanup;
        mutt_buffer_strcpy(newfile, a->filename);
      }
      else
        unlink_newfile = true;
    }
    /* in recv mode, save file to newfile first */
    else
    {
      if (mutt_save_attachment(fp, a, mutt_b2s(newfile), MUTT_SAVE_NO_FLAGS, NULL) == -1)
        goto mailcap_cleanup;
    }

    mutt_buffer_strcpy(cmd, entry->printcommand);
    piped = mailcap_expand_command(a, mutt_b2s(newfile), type, cmd);

    mutt_endwin();

    /* interactive program */
    if (piped)
    {
      fp_in = fopen(mutt_b2s(newfile), "r");
      if (!fp_in)
      {
        mutt_perror("fopen");
        mailcap_entry_free(&entry);
        goto mailcap_cleanup;
      }

      pid = filter_create(mutt_b2s(cmd), &fp_out, NULL, NULL);
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
      if (filter_wait(pid) || C_WaitKey)
        mutt_any_key_to_continue(NULL);
    }
    else
    {
      int rc2 = mutt_system(mutt_b2s(cmd));
      if (rc2 == -1)
        mutt_debug(LL_DEBUG1, "Error running \"%s\"", cmd->data);

      if ((rc2 != 0) || C_WaitKey)
        mutt_any_key_to_continue(NULL);
    }

    rc = 1;

  mailcap_cleanup:
    if (fp)
      mutt_file_unlink(mutt_b2s(newfile));
    else if (unlink_newfile)
      unlink(mutt_b2s(newfile));

    mailcap_entry_free(&entry);
    goto out;
  }

  if (mutt_istr_equal("text/plain", type) ||
      mutt_istr_equal("application/postscript", type))
  {
    rc = (mutt_pipe_attachment(fp, a, NONULL(C_PrintCommand), NULL));
    goto out;
  }
  else if (mutt_can_decode(a))
  {
    /* decode and print */

    fp_in = NULL;
    fp_out = NULL;

    mutt_buffer_mktemp(newfile);
    if (mutt_decode_save_attachment(fp, a, mutt_b2s(newfile), MUTT_PRINTING,
                                    MUTT_SAVE_NO_FLAGS) == 0)
    {
      mutt_debug(LL_DEBUG2, "successfully decoded %s type attachment to %s\n",
                 type, mutt_b2s(newfile));

      fp_in = fopen(mutt_b2s(newfile), "r");
      if (!fp_in)
      {
        mutt_perror("fopen");
        goto decode_cleanup;
      }

      mutt_debug(LL_DEBUG2, "successfully opened %s read-only\n", mutt_b2s(newfile));

      mutt_endwin();
      pid = filter_create(NONULL(C_PrintCommand), &fp_out, NULL, NULL);
      if (pid < 0)
      {
        mutt_perror(_("Can't create filter"));
        goto decode_cleanup;
      }

      mutt_debug(LL_DEBUG2, "Filter created\n");

      mutt_file_copy_stream(fp_in, fp_out);

      mutt_file_fclose(&fp_out);
      mutt_file_fclose(&fp_in);

      if ((filter_wait(pid) != 0) || C_WaitKey)
        mutt_any_key_to_continue(NULL);
      rc = 1;
    }
  decode_cleanup:
    mutt_file_fclose(&fp_in);
    mutt_file_fclose(&fp_out);
    mutt_file_unlink(mutt_b2s(newfile));
  }
  else
  {
    mutt_error(_("I don't know how to print that"));
    rc = 0;
  }

out:
  mutt_buffer_pool_release(&newfile);
  mutt_buffer_pool_release(&cmd);

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
 * mutt_unlink_temp_attachments - Delete all temporary attachments
 */
void mutt_unlink_temp_attachments(void)
{
  struct ListNode *np = NULL;

  STAILQ_FOREACH(np, &TempAttachmentsList, entries)
  {
    mutt_file_chmod_add(np->data, S_IWUSR);
    mutt_file_unlink(np->data);
  }

  mutt_list_free(&TempAttachmentsList);
}
