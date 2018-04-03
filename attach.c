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

#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "mutt.h"
#include "attach.h"
#include "body.h"
#include "context.h"
#include "copy.h"
#include "filter.h"
#include "globals.h"
#include "header.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "pager.h"
#include "protos.h"
#include "rfc1524.h"
#include "state.h"

int mutt_get_tmp_attachment(struct Body *a)
{
  char type[STRING];
  char tempfile[_POSIX_PATH_MAX];
  FILE *fpin = NULL, *fpout = NULL;
  struct stat st;

  if (a->unlink)
    return 0;

  struct Rfc1524MailcapEntry *entry = rfc1524_new_entry();
  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  rfc1524_mailcap_lookup(a, type, entry, 0);
  rfc1524_expand_filename(entry->nametemplate, a->filename, tempfile, sizeof(tempfile));

  rfc1524_free_entry(&entry);

  if (stat(a->filename, &st) == -1)
    return -1;

  if ((fpin = fopen(a->filename, "r")) && (fpout = mutt_file_fopen(tempfile, "w")))
  {
    mutt_file_copy_stream(fpin, fpout);
    mutt_str_replace(&a->filename, tempfile);
    a->unlink = true;

    if (a->stamp >= st.st_mtime)
      mutt_stamp_attachment(a);
  }
  else
    mutt_perror(fpin ? tempfile : a->filename);

  if (fpin)
    mutt_file_fclose(&fpin);
  if (fpout)
    mutt_file_fclose(&fpout);

  return a->unlink ? 0 : -1;
}

/**
 * mutt_compose_attachment - Create an attachment
 * @retval 1 if require full screen redraw
 * @retval 0 otherwise
 */
int mutt_compose_attachment(struct Body *a)
{
  char type[STRING];
  char command[STRING];
  char newfile[_POSIX_PATH_MAX] = "";
  struct Rfc1524MailcapEntry *entry = rfc1524_new_entry();
  bool unlink_newfile = false;
  int rc = 0;

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  if (rfc1524_mailcap_lookup(a, type, entry, MUTT_COMPOSE))
  {
    if (entry->composecommand || entry->composetypecommand)
    {
      if (entry->composetypecommand)
        mutt_str_strfcpy(command, entry->composetypecommand, sizeof(command));
      else
        mutt_str_strfcpy(command, entry->composecommand, sizeof(command));
      if (rfc1524_expand_filename(entry->nametemplate, a->filename, newfile, sizeof(newfile)))
      {
        mutt_debug(1, "oldfile: %s\t newfile: %s\n", a->filename, newfile);
        if (mutt_file_symlink(a->filename, newfile) == -1)
        {
          if (mutt_yesorno(_("Can't match nametemplate, continue?"), MUTT_YES) != MUTT_YES)
            goto bailout;
        }
        else
          unlink_newfile = true;
      }
      else
        mutt_str_strfcpy(newfile, a->filename, sizeof(newfile));

      if (rfc1524_expand_command(a, newfile, type, command, sizeof(command)))
      {
        /* For now, editing requires a file, no piping */
        mutt_error(_("Mailcap compose entry requires %%s"));
      }
      else
      {
        int r;

        mutt_endwin();
        r = mutt_system(command);
        if (r == -1)
          mutt_error(_("Error running \"%s\"!"), command);

        if (r != -1 && entry->composetypecommand)
        {
          struct Body *b = NULL;
          char tempfile[_POSIX_PATH_MAX];

          FILE *fp = mutt_file_fopen(a->filename, "r");
          if (!fp)
          {
            mutt_perror(_("Failure to open file to parse headers."));
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
            mutt_mktemp(tempfile, sizeof(tempfile));
            FILE *tfp = mutt_file_fopen(tempfile, "w");
            if (!tfp)
            {
              mutt_perror(_("Failure to open file to strip headers."));
              mutt_file_fclose(&fp);
              goto bailout;
            }
            mutt_file_copy_stream(fp, tfp);
            mutt_file_fclose(&fp);
            mutt_file_fclose(&tfp);
            mutt_file_unlink(a->filename);
            if (mutt_file_rename(tempfile, a->filename) != 0)
            {
              mutt_perror(_("Failure to rename file."));
              goto bailout;
            }
          }
        }
      }
    }
  }
  else
  {
    rfc1524_free_entry(&entry);
    mutt_message(_("No mailcap compose entry for %s, creating empty file."), type);
    return 1;
  }

  rc = 1;

bailout:

  if (unlink_newfile)
    unlink(newfile);

  rfc1524_free_entry(&entry);
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
  char type[STRING];
  char command[STRING];
  char newfile[_POSIX_PATH_MAX] = "";
  struct Rfc1524MailcapEntry *entry = rfc1524_new_entry();
  bool unlink_newfile = false;
  int rc = 0;

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  if (rfc1524_mailcap_lookup(a, type, entry, MUTT_EDIT))
  {
    if (entry->editcommand)
    {
      mutt_str_strfcpy(command, entry->editcommand, sizeof(command));
      if (rfc1524_expand_filename(entry->nametemplate, a->filename, newfile, sizeof(newfile)))
      {
        mutt_debug(1, "oldfile: %s\t newfile: %s\n", a->filename, newfile);
        if (mutt_file_symlink(a->filename, newfile) == -1)
        {
          if (mutt_yesorno(_("Can't match nametemplate, continue?"), MUTT_YES) != MUTT_YES)
            goto bailout;
        }
        else
          unlink_newfile = true;
      }
      else
        mutt_str_strfcpy(newfile, a->filename, sizeof(newfile));

      if (rfc1524_expand_command(a, newfile, type, command, sizeof(command)))
      {
        /* For now, editing requires a file, no piping */
        mutt_error(_("Mailcap Edit entry requires %%s"));
        goto bailout;
      }
      else
      {
        mutt_endwin();
        if (mutt_system(command) == -1)
        {
          mutt_error(_("Error running \"%s\"!"), command);
          goto bailout;
        }
      }
    }
  }
  else if (a->type == TYPETEXT)
  {
    /* On text, default to editor */
    mutt_edit_file(NONULL(Editor), a->filename);
  }
  else
  {
    rfc1524_free_entry(&entry);
    mutt_error(_("No mailcap edit entry for %s"), type);
    return 0;
  }

  rc = 1;

bailout:

  if (unlink_newfile)
    unlink(newfile);

  rfc1524_free_entry(&entry);
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
  struct ListNode *np;
  STAILQ_FOREACH(np, &MimeLookupList, entries)
  {
    const int i = mutt_str_strlen(np->data) - 1;
    if ((i > 0 && np->data[i - 1] == '/' && np->data[i] == '*' &&
         (mutt_str_strncasecmp(type, np->data, i) == 0)) ||
        (mutt_str_strcasecmp(type, np->data) == 0))
    {
      struct Body tmp = { 0 };
      int n;
      n = mutt_lookup_mime_type(&tmp, b->filename);
      if (n != TYPEOTHER)
      {
        snprintf(type, len, "%s/%s",
                 n == TYPEAUDIO ? "audio" :
                                  n == TYPEAPPLICATION ?
                                  "application" :
                                  n == TYPEIMAGE ?
                                  "image" :
                                  n == TYPEMESSAGE ?
                                  "message" :
                                  n == TYPEMODEL ?
                                  "model" :
                                  n == TYPEMULTIPART ?
                                  "multipart" :
                                  n == TYPETEXT ? "text" : n == TYPEVIDEO ? "video" : "other",
                 tmp.subtype);
        mutt_debug(1, "\"%s\" -> %s\n", b->filename, type);
      }
      if (tmp.subtype)
        FREE(&tmp.subtype);
      if (tmp.xtype)
        FREE(&tmp.xtype);
    }
  }
}

/**
 * mutt_view_attachment - View an attachment
 * @param fp     Source file stream. Can be NULL
 * @param a      The message body containing the attachment
 * @param flag   Option flag for how the attachment should be viewed
 * @param hdr    Message header for the current message. Can be NULL
 * @param actx   Attachment context
 * @retval 0  If the viewer is run and exited successfully
 * @retval -1 Error
 * @retval n  Return value of mutt_do_pager() when it is used
 *
 * flag can be one of: #MUTT_MAILCAP, #MUTT_REGULAR, #MUTT_AS_TEXT
 *
 * Display a message attachment using the viewer program configured in mailcap.
 * If there is no mailcap entry for a file type, view the image as text.
 * Viewer processes are opened and waited on synchronously so viewing an
 * attachment this way will block the main neomutt process until the viewer process
 * exits.
 *
 */
int mutt_view_attachment(FILE *fp, struct Body *a, int flag, struct Header *hdr,
                         struct AttachCtx *actx)
{
  char tempfile[_POSIX_PATH_MAX] = "";
  char pagerfile[_POSIX_PATH_MAX] = "";
  bool use_mailcap = false;
  bool use_pipe = false;
  bool use_pager = true;
  char type[STRING];
  char command[HUGE_STRING];
  char descrip[STRING];
  char *fname = NULL;
  struct Rfc1524MailcapEntry *entry = NULL;
  int rc = -1;
  bool unlink_tempfile = false;

  bool is_message = mutt_is_message_type(a->type, a->subtype);
  if ((WithCrypto != 0) && is_message && a->hdr &&
      (a->hdr->security & ENCRYPT) && !crypt_valid_passphrase(a->hdr->security))
  {
    return rc;
  }
  use_mailcap =
      (flag == MUTT_MAILCAP || (flag == MUTT_REGULAR && mutt_needs_mailcap(a)));
  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);

  if (use_mailcap)
  {
    entry = rfc1524_new_entry();
    if (!rfc1524_mailcap_lookup(a, type, entry, 0))
    {
      if (flag == MUTT_REGULAR)
      {
        /* fallback to view as text */
        rfc1524_free_entry(&entry);
        mutt_error(_("No matching mailcap entry found.  Viewing as text."));
        flag = MUTT_AS_TEXT;
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
      mutt_error(_("MIME type not defined.  Cannot view attachment."));
      goto return_error;
    }
    mutt_str_strfcpy(command, entry->command, sizeof(command));

    if (fp)
    {
      fname = mutt_str_strdup(a->filename);
      mutt_file_sanitize_filename(fname, 1);
    }
    else
      fname = a->filename;

    if (rfc1524_expand_filename(entry->nametemplate, fname, tempfile, sizeof(tempfile)))
    {
      if (fp == NULL && (mutt_str_strcmp(tempfile, a->filename) != 0))
      {
        /* send case: the file is already there */
        if (mutt_file_symlink(a->filename, tempfile) == -1)
        {
          if (mutt_yesorno(_("Can't match nametemplate, continue?"), MUTT_YES) == MUTT_YES)
            mutt_str_strfcpy(tempfile, a->filename, sizeof(tempfile));
          else
            goto return_error;
        }
        else
          unlink_tempfile = true;
      }
    }
    else if (!fp) /* send case */
      mutt_str_strfcpy(tempfile, a->filename, sizeof(tempfile));

    if (fp)
    {
      /* recv case: we need to save the attachment to a file */
      FREE(&fname);
      if (mutt_save_attachment(fp, a, tempfile, 0, NULL) == -1)
        goto return_error;
      mutt_file_chmod(tempfile, S_IRUSR);
    }

    use_pipe = rfc1524_expand_command(a, tempfile, type, command, sizeof(command));
    use_pager = entry->copiousoutput;
  }

  if (use_pager)
  {
    if (fp && !use_mailcap && a->filename)
    {
      /* recv case */
      mutt_str_strfcpy(pagerfile, a->filename, sizeof(pagerfile));
      mutt_adv_mktemp(pagerfile, sizeof(pagerfile));
    }
    else
      mutt_mktemp(pagerfile, sizeof(pagerfile));
  }

  if (use_mailcap)
  {
    pid_t thepid = 0;
    int tempfd = -1, pagerfd = -1;

    if (!use_pager)
      mutt_endwin();

    if (use_pager || use_pipe)
    {
      if (use_pager &&
          ((pagerfd = mutt_file_open(pagerfile, O_CREAT | O_EXCL | O_WRONLY)) == -1))
      {
        mutt_perror("open");
        goto return_error;
      }
      if (use_pipe && ((tempfd = open(tempfile, 0)) == -1))
      {
        if (pagerfd != -1)
          close(pagerfd);
        mutt_perror("open");
        goto return_error;
      }

      thepid = mutt_create_filter_fd(command, NULL, NULL, NULL, use_pipe ? tempfd : -1,
                                     use_pager ? pagerfd : -1, -1);
      if (thepid == -1)
      {
        if (pagerfd != -1)
          close(pagerfd);

        if (tempfd != -1)
          close(tempfd);

        mutt_error(_("Can't create filter"));
        goto return_error;
      }

      if (use_pager)
      {
        if (a->description)
          snprintf(descrip, sizeof(descrip),
                   _("---Command: %-20.20s Description: %s"), command, a->description);
        else
          snprintf(descrip, sizeof(descrip),
                   _("---Command: %-30.30s Attachment: %s"), command, type);
      }

      if ((mutt_wait_filter(thepid) || (entry->needsterminal && WaitKey)) && !use_pager)
        mutt_any_key_to_continue(NULL);

      if (tempfd != -1)
        close(tempfd);
      if (pagerfd != -1)
        close(pagerfd);
    }
    else
    {
      /* interactive command */
      int rv = mutt_system(command);
      if (rv == -1)
        mutt_debug(1, "Error running \"%s\"!", command);

      if ((rv != 0) || (entry->needsterminal && WaitKey))
        mutt_any_key_to_continue(NULL);
    }
  }
  else
  {
    /* Don't use mailcap; the attachment is viewed in the pager */

    if (flag == MUTT_AS_TEXT)
    {
      /* just let me see the raw data */
      if (fp)
      {
        /* Viewing from a received message.
         *
         * Don't use mutt_save_attachment() because we want to perform charset
         * conversion since this will be displayed by the internal pager.
         */
        struct State decode_state;

        memset(&decode_state, 0, sizeof(decode_state));
        decode_state.fpout = mutt_file_fopen(pagerfile, "w");
        if (!decode_state.fpout)
        {
          mutt_debug(1, "mutt_file_fopen(%s) errno=%d %s\n", pagerfile, errno,
                     strerror(errno));
          mutt_perror(pagerfile);
          goto return_error;
        }
        decode_state.fpin = fp;
        decode_state.flags = MUTT_CHARCONV;
        mutt_decode_attachment(a, &decode_state);
        if (fclose(decode_state.fpout) == EOF)
          mutt_debug(1, "fclose(%s) errno=%d %s\n", pagerfile, errno, strerror(errno));
      }
      else
      {
        /* in compose mode, just copy the file.  we can't use
         * mutt_decode_attachment() since it assumes the content-encoding has
         * already been applied
         */
        if (mutt_save_attachment(fp, a, pagerfile, 0, NULL))
          goto return_error;
      }
    }
    else
    {
      /* Use built-in handler */
      OPT_VIEW_ATTACH = true; /* disable the "use 'v' to view this part"
                                   * message in case of error */
      if (mutt_decode_save_attachment(fp, a, pagerfile, MUTT_DISPLAY, 0))
      {
        OPT_VIEW_ATTACH = false;
        goto return_error;
      }
      OPT_VIEW_ATTACH = false;
    }

    if (a->description)
      mutt_str_strfcpy(descrip, a->description, sizeof(descrip));
    else if (a->filename)
      snprintf(descrip, sizeof(descrip), _("---Attachment: %s: %s"), a->filename, type);
    else
      snprintf(descrip, sizeof(descrip), _("---Attachment: %s"), type);
  }

  /* We only reach this point if there have been no errors */

  if (use_pager)
  {
    struct Pager info;

    memset(&info, 0, sizeof(info));
    info.fp = fp;
    info.bdy = a;
    info.ctx = Context;
    info.actx = actx;
    info.hdr = hdr;

    rc = mutt_do_pager(descrip, pagerfile,
                       MUTT_PAGER_ATTACHMENT | (is_message ? MUTT_PAGER_MESSAGE : 0),
                       &info);
    *pagerfile = '\0';
  }
  else
    rc = 0;

return_error:

  if (entry)
    rfc1524_free_entry(&entry);
  if (fp && tempfile[0])
  {
    /* Restore write permission so mutt_file_unlink can open the file for writing */
    mutt_file_chmod_add(tempfile, S_IWUSR);
    mutt_file_unlink(tempfile);
  }
  else if (unlink_tempfile)
    unlink(tempfile);

  if (pagerfile[0])
    mutt_file_unlink(pagerfile);

  return rc;
}

/**
 * mutt_pipe_attachment - Pipe an attachment to a command
 * @retval 1 on success
 * @retval 0 on error
 */
int mutt_pipe_attachment(FILE *fp, struct Body *b, const char *path, char *outfile)
{
  pid_t thepid;
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

    struct State s;

    memset(&s, 0, sizeof(struct State));
    /* perform charset conversion on text attachments when piping */
    s.flags = MUTT_CHARCONV;

    if (outfile && *outfile)
      thepid = mutt_create_filter_fd(path, &s.fpout, NULL, NULL, -1, out, -1);
    else
      thepid = mutt_create_filter(path, &s.fpout, NULL, NULL);

    if (thepid < 0)
    {
      mutt_perror(_("Can't create filter"));
      goto bail;
    }

    s.fpin = fp;
    mutt_decode_attachment(b, &s);
    mutt_file_fclose(&s.fpout);
  }
  else
  {
    /* send case */

    FILE *ofp = NULL;

    FILE *ifp = fopen(b->filename, "r");
    if (!ifp)
    {
      mutt_perror("fopen");
      if (outfile && *outfile)
      {
        close(out);
        unlink(outfile);
      }
      return 0;
    }

    if (outfile && *outfile)
      thepid = mutt_create_filter_fd(path, &ofp, NULL, NULL, -1, out, -1);
    else
      thepid = mutt_create_filter(path, &ofp, NULL, NULL);

    if (thepid < 0)
    {
      mutt_perror(_("Can't create filter"));
      mutt_file_fclose(&ifp);
      goto bail;
    }

    mutt_file_copy_stream(ifp, ofp);
    mutt_file_fclose(&ofp);
    mutt_file_fclose(&ifp);
  }

  rc = 1;

bail:

  if (outfile && *outfile)
    close(out);

  /*
   * check for error exit from child process
   */
  if (mutt_wait_filter(thepid) != 0)
    rc = 0;

  if (rc == 0 || WaitKey)
    mutt_any_key_to_continue(NULL);
  return rc;
}

static FILE *save_attachment_open(char *path, int flags)
{
  if (flags == MUTT_SAVE_APPEND)
    return fopen(path, "a");
  if (flags == MUTT_SAVE_OVERWRITE)
    return fopen(path, "w");

  return mutt_file_fopen(path, "w");
}

/**
 * mutt_save_attachment - Save an attachment
 * @param fp    Source file stream. Can be NULL
 * @param m     Email Body
 * @param path  Where to save the attachment
 * @param flags Flags, e.g. #MUTT_SAVE_APPEND
 * @param hdr   Message header for the current message. Can be NULL
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_save_attachment(FILE *fp, struct Body *m, char *path, int flags, struct Header *hdr)
{
  if (!m)
    return -1;

  if (fp)
  {
    /* recv mode */

    if (hdr && m->hdr && m->encoding != ENCBASE64 && m->encoding != ENCQUOTEDPRINTABLE &&
        mutt_is_message_type(m->type, m->subtype))
    {
      /* message type attachments are written to mail folders. */

      char buf[HUGE_STRING];
      struct Context ctx;
      struct Message *msg = NULL;
      int chflags = 0;
      int r = -1;

      struct Header *hn = m->hdr;
      hn->msgno = hdr->msgno; /* required for MH/maildir */
      hn->read = true;

      if (fseeko(fp, m->offset, SEEK_SET) < 0)
        return -1;
      if (fgets(buf, sizeof(buf), fp) == NULL)
        return -1;
      if (mx_open_mailbox(path, MUTT_APPEND | MUTT_QUIET, &ctx) == NULL)
        return -1;
      msg = mx_open_new_message(&ctx, hn, is_from(buf, NULL, 0, NULL) ? 0 : MUTT_ADD_FROM);
      if (!msg)
      {
        mx_close_mailbox(&ctx, NULL);
        return -1;
      }
      if (ctx.magic == MUTT_MBOX || ctx.magic == MUTT_MMDF)
        chflags = CH_FROM | CH_UPDATE_LEN;
      chflags |= (ctx.magic == MUTT_MAILDIR ? CH_NOSTATUS : CH_UPDATE);
      if (mutt_copy_message_fp(msg->fp, fp, hn, 0, chflags) == 0 &&
          mx_commit_message(msg, &ctx) == 0)
      {
        r = 0;
      }
      else
      {
        r = -1;
      }

      mx_close_message(&ctx, &msg);
      mx_close_mailbox(&ctx, NULL);
      return r;
    }
    else
    {
      /* In recv mode, extract from folder and decode */

      struct State s;

      memset(&s, 0, sizeof(s));
      s.fpout = save_attachment_open(path, flags);
      if (!s.fpout)
      {
        mutt_perror("fopen");
        return -1;
      }
      fseeko((s.fpin = fp), m->offset, SEEK_SET);
      mutt_decode_attachment(m, &s);

      if (mutt_file_fsync_close(&s.fpout) != 0)
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

    FILE *ofp = fopen(m->filename, "r");
    if (!ofp)
    {
      mutt_perror("fopen");
      return -1;
    }

    FILE *nfp = save_attachment_open(path, flags);
    if (!nfp)
    {
      mutt_perror("fopen");
      mutt_file_fclose(&ofp);
      return -1;
    }

    if (mutt_file_copy_stream(ofp, nfp) == -1)
    {
      mutt_error(_("Write fault!"));
      mutt_file_fclose(&ofp);
      mutt_file_fclose(&nfp);
      return -1;
    }
    mutt_file_fclose(&ofp);
    if (mutt_file_fsync_close(&nfp) != 0)
    {
      mutt_error(_("Write fault!"));
      return -1;
    }
  }

  return 0;
}

/**
 * mutt_decode_save_attachment - Decode, then save an attachment
 * @retval 0 on success
 * @retval -1 on error
 */
int mutt_decode_save_attachment(FILE *fp, struct Body *m, char *path, int displaying, int flags)
{
  struct State s;
  unsigned int saved_encoding = 0;
  struct Body *saved_parts = NULL;
  struct Header *saved_hdr = NULL;
  int rc = 0;

  memset(&s, 0, sizeof(s));
  s.flags = displaying;

  if (flags == MUTT_SAVE_APPEND)
    s.fpout = fopen(path, "a");
  else if (flags == MUTT_SAVE_OVERWRITE)
    s.fpout = fopen(path, "w");
  else
    s.fpout = mutt_file_fopen(path, "w");

  if (!s.fpout)
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
      mutt_file_fclose(&s.fpout);
      return -1;
    }

    s.fpin = fopen(m->filename, "r");
    if (!s.fpin)
    {
      mutt_perror("fopen");
      return -1;
    }

    saved_encoding = m->encoding;
    if (!is_multipart(m))
      m->encoding = ENC8BIT;

    m->length = st.st_size;
    m->offset = 0;
    saved_parts = m->parts;
    saved_hdr = m->hdr;
    mutt_parse_part(s.fpin, m);

    if (m->noconv || is_multipart(m))
      s.flags |= MUTT_CHARCONV;
  }
  else
  {
    s.fpin = fp;
    s.flags |= MUTT_CHARCONV;
  }

  mutt_body_handler(m, &s);

  if (mutt_file_fsync_close(&s.fpout) != 0)
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
      mutt_header_free(&m->hdr);
      m->parts = saved_parts;
      m->hdr = saved_hdr;
    }
    mutt_file_fclose(&s.fpin);
  }

  return rc;
}

/**
 * mutt_print_attachment - Print out an attachment
 *
 * Ok, the difference between send and receive:
 * recv: Body->filename is a suggested name, and Context|Header points
 *       to the attachment in mailbox which is encoded
 * send: Body->filename points to the un-encoded file which contains the
 *       attachment
 */
int mutt_print_attachment(FILE *fp, struct Body *a)
{
  char newfile[_POSIX_PATH_MAX] = "";
  char type[STRING];
  pid_t thepid;
  FILE *ifp = NULL, *fpout = NULL;
  bool unlink_newfile = false;

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);

  if (rfc1524_mailcap_lookup(a, type, NULL, MUTT_PRINT))
  {
    char command[_POSIX_PATH_MAX + STRING];
    int piped = false;

    mutt_debug(2, "Using mailcap...\n");

    struct Rfc1524MailcapEntry *entry = rfc1524_new_entry();
    rfc1524_mailcap_lookup(a, type, entry, MUTT_PRINT);
    if (rfc1524_expand_filename(entry->nametemplate, a->filename, newfile, sizeof(newfile)))
    {
      if (!fp)
      {
        if (mutt_file_symlink(a->filename, newfile) == -1)
        {
          if (mutt_yesorno(_("Can't match nametemplate, continue?"), MUTT_YES) != MUTT_YES)
          {
            rfc1524_free_entry(&entry);
            return 0;
          }
          mutt_str_strfcpy(newfile, a->filename, sizeof(newfile));
        }
        else
          unlink_newfile = true;
      }
    }

    /* in recv mode, save file to newfile first */
    if (fp && (mutt_save_attachment(fp, a, newfile, 0, NULL) != 0))
      return 0;

    mutt_str_strfcpy(command, entry->printcommand, sizeof(command));
    piped = rfc1524_expand_command(a, newfile, type, command, sizeof(command));

    mutt_endwin();

    /* interactive program */
    if (piped)
    {
      ifp = fopen(newfile, "r");
      if (!ifp)
      {
        mutt_perror("fopen");
        rfc1524_free_entry(&entry);
        return 0;
      }

      thepid = mutt_create_filter(command, &fpout, NULL, NULL);
      if (thepid < 0)
      {
        mutt_perror(_("Can't create filter"));
        rfc1524_free_entry(&entry);
        mutt_file_fclose(&ifp);
        return 0;
      }
      mutt_file_copy_stream(ifp, fpout);
      mutt_file_fclose(&fpout);
      mutt_file_fclose(&ifp);
      if (mutt_wait_filter(thepid) || WaitKey)
        mutt_any_key_to_continue(NULL);
    }
    else
    {
      int rc = mutt_system(command);
      if (rc == -1)
        mutt_debug(1, "Error running \"%s\"!", command);

      if ((rc != 0) || WaitKey)
        mutt_any_key_to_continue(NULL);
    }

    if (fp)
      mutt_file_unlink(newfile);
    else if (unlink_newfile)
      unlink(newfile);

    rfc1524_free_entry(&entry);
    return 1;
  }

  if ((mutt_str_strcasecmp("text/plain", type) == 0) ||
      (mutt_str_strcasecmp("application/postscript", type) == 0))
  {
    return (mutt_pipe_attachment(fp, a, NONULL(PrintCommand), NULL));
  }
  else if (mutt_can_decode(a))
  {
    /* decode and print */

    int rc = 0;

    ifp = NULL;
    fpout = NULL;

    mutt_mktemp(newfile, sizeof(newfile));
    if (mutt_decode_save_attachment(fp, a, newfile, MUTT_PRINTING, 0) == 0)
    {
      mutt_debug(2, "successfully decoded %s type attachment to %s\n", type, newfile);

      ifp = fopen(newfile, "r");
      if (!ifp)
      {
        mutt_perror("fopen");
        goto bail0;
      }

      mutt_debug(2, "successfully opened %s read-only\n", newfile);

      mutt_endwin();
      thepid = mutt_create_filter(NONULL(PrintCommand), &fpout, NULL, NULL);
      if (thepid < 0)
      {
        mutt_perror(_("Can't create filter"));
        goto bail0;
      }

      mutt_debug(2, "Filter created.\n");

      mutt_file_copy_stream(ifp, fpout);

      mutt_file_fclose(&fpout);
      mutt_file_fclose(&ifp);

      if (mutt_wait_filter(thepid) != 0 || WaitKey)
        mutt_any_key_to_continue(NULL);
      rc = 1;
    }
  bail0:
    mutt_file_fclose(&ifp);
    mutt_file_fclose(&fpout);
    mutt_file_unlink(newfile);
    return rc;
  }
  else
  {
    mutt_error(_("I don't know how to print that!"));
    return 0;
  }
}

void mutt_actx_add_attach(struct AttachCtx *actx, struct AttachPtr *attach)
{
  if (actx->idxlen == actx->idxmax)
  {
    actx->idxmax += 5;
    mutt_mem_realloc(&actx->idx, sizeof(struct AttachPtr *) * actx->idxmax);
    mutt_mem_realloc(&actx->v2r, sizeof(short) * actx->idxmax);
    for (int i = actx->idxlen; i < actx->idxmax; i++)
      actx->idx[i] = NULL;
  }

  actx->idx[actx->idxlen++] = attach;
}

void mutt_actx_add_fp(struct AttachCtx *actx, FILE *new_fp)
{
  if (actx->fp_len == actx->fp_max)
  {
    actx->fp_max += 5;
    mutt_mem_realloc(&actx->fp_idx, sizeof(FILE *) * actx->fp_max);
    for (int i = actx->fp_len; i < actx->fp_max; i++)
      actx->fp_idx[i] = NULL;
  }

  actx->fp_idx[actx->fp_len++] = new_fp;
}

void mutt_actx_add_body(struct AttachCtx *actx, struct Body *new_body)
{
  if (actx->body_len == actx->body_max)
  {
    actx->body_max += 5;
    mutt_mem_realloc(&actx->body_idx, sizeof(struct Body *) * actx->body_max);
    for (int i = actx->body_len; i < actx->body_max; i++)
      actx->body_idx[i] = NULL;
  }

  actx->body_idx[actx->body_len++] = new_body;
}

void mutt_actx_free_entries(struct AttachCtx *actx)
{
  int i;

  for (i = 0; i < actx->idxlen; i++)
  {
    if (actx->idx[i]->content)
      actx->idx[i]->content->aptr = NULL;
    FREE(&actx->idx[i]->tree);
    FREE(&actx->idx[i]);
  }
  actx->idxlen = 0;
  actx->vcount = 0;

  for (i = 0; i < actx->fp_len; i++)
    mutt_file_fclose(&actx->fp_idx[i]);
  actx->fp_len = 0;

  for (i = 0; i < actx->body_len; i++)
    mutt_body_free(&actx->body_idx[i]);
  actx->body_len = 0;
}

void mutt_free_attach_context(struct AttachCtx **pactx)
{
  struct AttachCtx *actx = NULL;

  if (!pactx || !*pactx)
    return;

  actx = *pactx;
  mutt_actx_free_entries(actx);
  FREE(&actx->idx);
  FREE(&actx->v2r);
  FREE(&actx->fp_idx);
  FREE(&actx->body_idx);
  FREE(pactx);
}
