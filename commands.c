/**
 * @file
 * Manage where the email is piped to external commands
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2004,2006 Thomas Roessler <roessler@does-not-exist.org>
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
 * @page commands Manage where the email is piped to external commands
 *
 * Manage where the email is piped to external commands
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "commands.h"
#include "alias.h"
#include "context.h"
#include "copy.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "hdrline.h"
#include "hook.h"
#include "icommands.h"
#include "keymap.h"
#include "mutt_commands.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "mutt_menu.h"
#include "mutt_parse.h"
#include "muttlib.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "options.h"
#include "pager.h"
#include "protos.h"
#include "sendlib.h"
#include "sort.h"
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/mutt_notmuch.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/autocrypt.h"
#endif

/* These Config Variables are only used in commands.c */
unsigned char C_CryptVerifySig; ///< Config: Verify PGP or SMIME signatures
char *C_DisplayFilter; ///< Config: External command to pre-process an email before display
bool C_PipeDecode; ///< Config: Decode the message when piping it
char *C_PipeSep;   ///< Config: Separator to add between multiple piped messages
bool C_PipeSplit;  ///< Config: Run the pipe command on each message separately
bool C_PrintDecode; ///< Config: Decode message before printing it
bool C_PrintSplit;  ///< Config: Print multiple messages separately
bool C_PromptAfter; ///< Config: Pause after running an external pager

static const char *ExtPagerProgress = "all";

/** The folder the user last saved to.  Used by ci_save_message() */
static char LastSaveFolder[PATH_MAX] = "";

/**
 * process_protected_headers - Get the protected header and update the index
 * @param e Email to update
 */
static void process_protected_headers(struct Email *e)
{
  struct Envelope *prot_headers = NULL;
  regmatch_t pmatch[1];

#ifdef USE_AUTOCRYPT
  if (!C_CryptProtectedHeadersRead && !C_Autocrypt)
    return;
#else
  if (!C_CryptProtectedHeadersRead)
    return;
#endif

  /* Grab protected headers to update in the index */
  if (e->security & SEC_SIGN)
  {
    /* Don't update on a bad signature.
     *
     * This is a simplification.  It's possible the headers are in the
     * encrypted part of a nested encrypt/signed.  But properly handling that
     * case would require more complexity in the decryption handlers, which
     * I'm not sure is worth it. */
    if (!(e->security & SEC_GOODSIGN))
      return;

    if (mutt_is_multipart_signed(e->content) && e->content->parts)
    {
      prot_headers = e->content->parts->mime_headers;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(e->content))
    {
      prot_headers = e->content->mime_headers;
    }
  }
  if (!prot_headers && (e->security & SEC_ENCRYPT))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) &&
        (mutt_is_valid_multipart_pgp_encrypted(e->content) ||
         mutt_is_malformed_multipart_pgp_encrypted(e->content)))
    {
      prot_headers = e->content->mime_headers;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(e->content))
    {
      prot_headers = e->content->mime_headers;
    }
  }

  /* Update protected headers in the index and header cache. */
  if (C_CryptProtectedHeadersRead && prot_headers && prot_headers->subject &&
      mutt_str_strcmp(e->env->subject, prot_headers->subject))
  {
    if (Context->mailbox->subj_hash && e->env->real_subj)
      mutt_hash_delete(Context->mailbox->subj_hash, e->env->real_subj, e);

    mutt_str_replace(&e->env->subject, prot_headers->subject);
    FREE(&e->env->disp_subj);
    if (mutt_regex_capture(C_ReplyRegex, e->env->subject, 1, pmatch))
      e->env->real_subj = e->env->subject + pmatch[0].rm_eo;
    else
      e->env->real_subj = e->env->subject;

    if (Context->mailbox->subj_hash)
      mutt_hash_insert(Context->mailbox->subj_hash, e->env->real_subj, e);

    mx_save_hcache(Context->mailbox, e);

    /* Also persist back to the message headers if this is set */
    if (C_CryptProtectedHeadersSave)
    {
      e->env->changed |= MUTT_ENV_CHANGED_SUBJECT;
      e->changed = true;
      Context->mailbox->changed = true;
    }
  }

#ifdef USE_AUTOCRYPT
  if (C_Autocrypt && (e->security & SEC_ENCRYPT) && prot_headers && prot_headers->autocrypt_gossip)
  {
    mutt_autocrypt_process_gossip_header(e, prot_headers);
  }
#endif
}

/**
 * mutt_display_message - Display a message in the pager
 * @param win_index Index Window
 * @param win_ibar  Index Bar Window
 * @param win_pager Pager Window
 * @param win_pbar  Pager Bar Window
 * @param m         Mailbox
 * @param e         Email to display
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_display_message(struct MuttWindow *win_index, struct MuttWindow *win_ibar,
                         struct MuttWindow *win_pager, struct MuttWindow *win_pbar,
                         struct Mailbox *m, struct Email *e)
{
  int rc = 0;
  bool builtin = false;
  CopyMessageFlags cmflags = MUTT_CM_DECODE | MUTT_CM_DISPLAY | MUTT_CM_CHARCONV;
  CopyHeaderFlags chflags;
  pid_t filterpid = -1;
  struct Buffer *tempfile = NULL;
  int res;

  mutt_parse_mime_message(m, e);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  char columns[16];
  snprintf(columns, sizeof(columns), "%d", win_pager->state.cols);
  mutt_envlist_set("COLUMNS", columns, true);

  /* see if crypto is needed for this message.  if so, we should exit curses */
  if ((WithCrypto != 0) && e->security)
  {
    if (e->security & SEC_ENCRYPT)
    {
      if (e->security & APPLICATION_SMIME)
        crypt_smime_getkeys(e->env);
      if (!crypt_valid_passphrase(e->security))
        goto cleanup;

      cmflags |= MUTT_CM_VERIFY;
    }
    else if (e->security & SEC_SIGN)
    {
      /* find out whether or not the verify signature */
      /* L10N: Used for the $crypt_verify_sig prompt */
      if (query_quadoption(C_CryptVerifySig, _("Verify signature?")) == MUTT_YES)
      {
        cmflags |= MUTT_CM_VERIFY;
      }
    }
  }

  if (cmflags & MUTT_CM_VERIFY || e->security & SEC_ENCRYPT)
  {
    if (e->security & APPLICATION_PGP)
    {
      if (!TAILQ_EMPTY(&e->env->from))
        crypt_pgp_invoke_getkeys(TAILQ_FIRST(&e->env->from));

      crypt_invoke_message(APPLICATION_PGP);
    }

    if (e->security & APPLICATION_SMIME)
      crypt_invoke_message(APPLICATION_SMIME);
  }

  FILE *fp_filter_out = NULL;
  tempfile = mutt_buffer_pool_get();
  mutt_buffer_mktemp(tempfile);
  FILE *fp_out = mutt_file_fopen(mutt_b2s(tempfile), "w");
  if (!fp_out)
  {
    mutt_error(_("Could not create temporary file"));
    goto cleanup;
  }

  if (C_DisplayFilter)
  {
    fp_filter_out = fp_out;
    fp_out = NULL;
    filterpid = mutt_create_filter_fd(C_DisplayFilter, &fp_out, NULL, NULL, -1,
                                      fileno(fp_filter_out), -1);
    if (filterpid < 0)
    {
      mutt_error(_("Can't create display filter"));
      mutt_file_fclose(&fp_filter_out);
      unlink(mutt_b2s(tempfile));
      goto cleanup;
    }
  }

  if (!C_Pager || (mutt_str_strcmp(C_Pager, "builtin") == 0))
    builtin = true;
  else
  {
    char buf[1024];
    struct HdrFormatInfo hfi;

    hfi.ctx = Context;
    hfi.mailbox = m;
    hfi.pager_progress = ExtPagerProgress;
    hfi.email = e;
    mutt_make_string_info(buf, sizeof(buf), win_pager->state.cols,
                          NONULL(C_PagerFormat), &hfi, MUTT_FORMAT_NO_FLAGS);
    fputs(buf, fp_out);
    fputs("\n\n", fp_out);
  }

  chflags = (C_Weed ? (CH_WEED | CH_REORDER) : CH_NO_FLAGS) | CH_DECODE | CH_FROM | CH_DISPLAY;
#ifdef USE_NOTMUCH
  if (m->magic == MUTT_NOTMUCH)
    chflags |= CH_VIRTUAL;
#endif
  res = mutt_copy_message(fp_out, m, e, cmflags, chflags, win_pager->state.cols);

  if (((mutt_file_fclose(&fp_out) != 0) && (errno != EPIPE)) || (res < 0))
  {
    mutt_error(_("Could not copy message"));
    if (fp_filter_out)
    {
      mutt_wait_filter(filterpid);
      mutt_file_fclose(&fp_filter_out);
    }
    mutt_file_unlink(mutt_b2s(tempfile));
    goto cleanup;
  }

  if (fp_filter_out && (mutt_wait_filter(filterpid) != 0))
    mutt_any_key_to_continue(NULL);

  mutt_file_fclose(&fp_filter_out); /* XXX - check result? */

  if (WithCrypto)
  {
    /* update crypto information for this message */
    e->security &= ~(SEC_GOODSIGN | SEC_BADSIGN);
    e->security |= crypt_query(e->content);

    /* Remove color cache for this message, in case there
     * are color patterns for both ~g and ~V */
    e->pair = 0;

    /* Process protected headers and autocrypt gossip headers */
    process_protected_headers(e);
  }

  if (builtin)
  {
    if ((WithCrypto != 0) && (e->security & APPLICATION_SMIME) && (cmflags & MUTT_CM_VERIFY))
    {
      if (e->security & SEC_GOODSIGN)
      {
        if (crypt_smime_verify_sender(m, e) == 0)
          mutt_message(_("S/MIME signature successfully verified"));
        else
          mutt_error(_("S/MIME certificate owner does not match sender"));
      }
      else if (e->security & SEC_PARTSIGN)
        mutt_message(_("Warning: Part of this message has not been signed"));
      else if (e->security & SEC_SIGN || e->security & SEC_BADSIGN)
        mutt_error(_("S/MIME signature could NOT be verified"));
    }

    if ((WithCrypto != 0) && (e->security & APPLICATION_PGP) && (cmflags & MUTT_CM_VERIFY))
    {
      if (e->security & SEC_GOODSIGN)
        mutt_message(_("PGP signature successfully verified"));
      else if (e->security & SEC_PARTSIGN)
        mutt_message(_("Warning: Part of this message has not been signed"));
      else if (e->security & SEC_SIGN)
        mutt_message(_("PGP signature could NOT be verified"));
    }

    struct Pager info = { 0 };
    /* Invoke the builtin pager */
    info.email = e;
    info.ctx = Context;
    info.win_ibar = win_ibar;
    info.win_index = win_index;
    info.win_pbar = win_pbar;
    info.win_pager = win_pager;
    rc = mutt_pager(NULL, mutt_b2s(tempfile), MUTT_PAGER_MESSAGE, &info);
  }
  else
  {
    mutt_endwin();

    struct Buffer *cmd = mutt_buffer_pool_get();
    mutt_buffer_printf(cmd, "%s %s", NONULL(C_Pager), mutt_b2s(tempfile));
    int r = mutt_system(mutt_b2s(cmd));
    if (r == -1)
      mutt_error(_("Error running \"%s\""), mutt_b2s(cmd));
    unlink(mutt_b2s(tempfile));
    mutt_buffer_pool_release(&cmd);

    if (!OptNoCurses)
      keypad(stdscr, true);
    if (r != -1)
      mutt_set_flag(m, e, MUTT_READ, true);
    if ((r != -1) && C_PromptAfter)
    {
      mutt_unget_event(mutt_any_key_to_continue(_("Command: ")), 0);
      rc = km_dokey(MENU_PAGER);
    }
    else
      rc = 0;
  }

cleanup:
  mutt_envlist_unset("COLUMNS");
  mutt_buffer_pool_release(&tempfile);
  return rc;
}

/**
 * ci_bounce_message - Bounce an email
 * @param m  Mailbox
 * @param el List of Emails to bounce
 */
void ci_bounce_message(struct Mailbox *m, struct EmailList *el)
{
  if (!m || !el || STAILQ_EMPTY(el))
    return;

  char prompt[8193];
  char scratch[8192];
  char buf[8192] = { 0 };
  struct AddressList al = TAILQ_HEAD_INITIALIZER(al);
  char *err = NULL;
  int rc;
  int msg_count = 0;

  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    /* RFC5322 mandates a From: header,
     * so warn before bouncing messages without one */
    if (!TAILQ_EMPTY(&en->email->env->from))
      mutt_error(_("Warning: message contains no From: header"));

    msg_count++;
  }

  if (msg_count == 1)
    mutt_str_strfcpy(prompt, _("Bounce message to: "), sizeof(prompt));
  else
    mutt_str_strfcpy(prompt, _("Bounce tagged messages to: "), sizeof(prompt));

  rc = mutt_get_field(prompt, buf, sizeof(buf), MUTT_ALIAS);
  if (rc || !buf[0])
    return;

  mutt_addrlist_parse2(&al, buf);
  if (TAILQ_EMPTY(&al))
  {
    mutt_error(_("Error parsing address"));
    return;
  }

  mutt_expand_aliases(&al);

  if (mutt_addrlist_to_intl(&al, &err) < 0)
  {
    mutt_error(_("Bad IDN: '%s'"), err);
    FREE(&err);
    mutt_addrlist_clear(&al);
    return;
  }

  buf[0] = '\0';
  mutt_addrlist_write(&al, buf, sizeof(buf), true);

#define EXTRA_SPACE (15 + 7 + 2)
  snprintf(scratch, sizeof(scratch),
           ngettext("Bounce message to %s?", "Bounce messages to %s?", msg_count), buf);

  if (mutt_strwidth(scratch) > MuttMessageWindow->state.cols - EXTRA_SPACE)
  {
    mutt_simple_format(prompt, sizeof(prompt), 0, MuttMessageWindow->state.cols - EXTRA_SPACE,
                       JUSTIFY_LEFT, 0, scratch, sizeof(scratch), false);
    mutt_str_strcat(prompt, sizeof(prompt), "...?");
  }
  else
    snprintf(prompt, sizeof(prompt), "%s", scratch);

  if (query_quadoption(C_Bounce, prompt) != MUTT_YES)
  {
    mutt_addrlist_clear(&al);
    mutt_window_clearline(MuttMessageWindow, 0);
    mutt_message(ngettext("Message not bounced", "Messages not bounced", msg_count));
    return;
  }

  mutt_window_clearline(MuttMessageWindow, 0);

  struct Message *msg = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    msg = mx_msg_open(m, en->email->msgno);
    if (!msg)
    {
      rc = -1;
      break;
    }

    rc = mutt_bounce_message(msg->fp, en->email, &al);
    mx_msg_close(m, &msg);

    if (rc < 0)
      break;
  }

  mutt_addrlist_clear(&al);
  /* If no error, or background, display message. */
  if ((rc == 0) || (rc == S_BKG))
    mutt_message(ngettext("Message bounced", "Messages bounced", msg_count));
}

/**
 * pipe_set_flags - Generate flags for copy header/message
 * @param[in]  decode  If true decode the message
 * @param[in]  print   If true, mark the message for printing
 * @param[out] cmflags Flags, see #CopyMessageFlags
 * @param[out] chflags Flags, see #CopyHeaderFlags
 */
static void pipe_set_flags(bool decode, bool print, CopyMessageFlags *cmflags,
                           CopyHeaderFlags *chflags)
{
  if (decode)
  {
    *chflags |= CH_DECODE | CH_REORDER;
    *cmflags |= MUTT_CM_DECODE | MUTT_CM_CHARCONV;

    if (C_Weed)
    {
      *chflags |= CH_WEED;
      *cmflags |= MUTT_CM_WEED;
    }

    /* Just as with copy-decode, we need to update the mime fields to avoid
     * confusing programs that may process the email.  However, we don't want
     * to force those fields to appear in printouts. */
    if (!print)
      *chflags |= CH_MIME | CH_TXTPLAIN;
  }

  if (print)
    *cmflags |= MUTT_CM_PRINTING;
}

/**
 * pipe_msg - Pipe a message
 * @param m      Mailbox
 * @param e      Email to pipe
 * @param fp     File to write to
 * @param decode If true, decode the message
 * @param print  If true, message is for printing
 */
static void pipe_msg(struct Mailbox *m, struct Email *e, FILE *fp, bool decode, bool print)
{
  CopyMessageFlags cmflags = MUTT_CM_NO_FLAGS;
  CopyHeaderFlags chflags = CH_FROM;

  pipe_set_flags(decode, print, &cmflags, &chflags);

  if ((WithCrypto != 0) && decode && e->security & SEC_ENCRYPT)
  {
    if (!crypt_valid_passphrase(e->security))
      return;
    endwin();
  }

  if (decode)
    mutt_parse_mime_message(m, e);

  mutt_copy_message(fp, m, e, cmflags, chflags, 0);
}

/**
 * pipe_message - Pipe message to a command
 * @param m      Mailbox
 * @param el     List of Emails to pipe
 * @param cmd    Command to pipe to
 * @param decode Should the message be decrypted
 * @param print  True if this is a print job
 * @param split  Should a separator be sent between messages?
 * @param sep    Separator string
 * @retval  0 Success
 * @retval  1 Error
 *
 * The following code is shared between printing and piping.
 */
static int pipe_message(struct Mailbox *m, struct EmailList *el, char *cmd,
                        bool decode, bool print, bool split, const char *sep)
{
  if (!m || !el)
    return 1;

  struct EmailNode *en = STAILQ_FIRST(el);
  if (!en)
    return 1;

  int rc = 0;
  pid_t pid;
  FILE *fp_out = NULL;

  if (!STAILQ_NEXT(en, entries))
  {
    /* handle a single message */
    mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);

    if ((WithCrypto != 0) && decode)
    {
      mutt_parse_mime_message(m, en->email);
      if ((en->email->security & SEC_ENCRYPT) &&
          !crypt_valid_passphrase(en->email->security))
      {
        return 1;
      }
    }
    mutt_endwin();

    pid = mutt_create_filter(cmd, &fp_out, NULL, NULL);
    if (pid < 0)
    {
      mutt_perror(_("Can't create filter process"));
      return 1;
    }

    OptKeepQuiet = true;
    pipe_msg(m, en->email, fp_out, decode, print);
    mutt_file_fclose(&fp_out);
    rc = mutt_wait_filter(pid);
    OptKeepQuiet = false;
  }
  else
  {
    /* handle tagged messages */
    if ((WithCrypto != 0) && decode)
    {
      STAILQ_FOREACH(en, el, entries)
      {
        mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
        mutt_parse_mime_message(m, en->email);
        if ((en->email->security & SEC_ENCRYPT) &&
            !crypt_valid_passphrase(en->email->security))
        {
          return 1;
        }
      }
    }

    if (split)
    {
      STAILQ_FOREACH(en, el, entries)
      {
        mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
        mutt_endwin();
        pid = mutt_create_filter(cmd, &fp_out, NULL, NULL);
        if (pid < 0)
        {
          mutt_perror(_("Can't create filter process"));
          return 1;
        }
        OptKeepQuiet = true;
        pipe_msg(m, en->email, fp_out, decode, print);
        /* add the message separator */
        if (sep)
          fputs(sep, fp_out);
        mutt_file_fclose(&fp_out);
        if (mutt_wait_filter(pid) != 0)
          rc = 1;
        OptKeepQuiet = false;
      }
    }
    else
    {
      mutt_endwin();
      pid = mutt_create_filter(cmd, &fp_out, NULL, NULL);
      if (pid < 0)
      {
        mutt_perror(_("Can't create filter process"));
        return 1;
      }
      OptKeepQuiet = true;
      STAILQ_FOREACH(en, el, entries)
      {
        mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
        pipe_msg(m, en->email, fp_out, decode, print);
        /* add the message separator */
        if (sep)
          fputs(sep, fp_out);
      }
      mutt_file_fclose(&fp_out);
      if (mutt_wait_filter(pid) != 0)
        rc = 1;
      OptKeepQuiet = false;
    }
  }

  if ((rc != 0) || C_WaitKey)
    mutt_any_key_to_continue(NULL);
  return rc;
}

/**
 * mutt_pipe_message - Pipe a message
 * @param m  Mailbox
 * @param el List of Emails to pipe
 */
void mutt_pipe_message(struct Mailbox *m, struct EmailList *el)
{
  if (!m || !el)
    return;

  char buf[1024] = { 0 };

  if ((mutt_get_field(_("Pipe to command: "), buf, sizeof(buf), MUTT_CMD) != 0) ||
      (buf[0] == '\0'))
  {
    return;
  }

  mutt_expand_path(buf, sizeof(buf));
  pipe_message(m, el, buf, C_PipeDecode, false, C_PipeSplit, C_PipeSep);
}

/**
 * mutt_print_message - Print a message
 * @param m  Mailbox
 * @param el List of Emails to print
 */
void mutt_print_message(struct Mailbox *m, struct EmailList *el)
{
  if (!m || !el)
    return;

  if (C_Print && !C_PrintCommand)
  {
    mutt_message(_("No printing command has been defined"));
    return;
  }

  int msg_count = 0;
  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    msg_count++;
  }

  if (query_quadoption(C_Print, (msg_count == 1) ?
                                    _("Print message?") :
                                    _("Print tagged messages?")) != MUTT_YES)
  {
    return;
  }

  if (pipe_message(m, el, C_PrintCommand, C_PrintDecode, true, C_PrintSplit, "\f") == 0)
    mutt_message(ngettext("Message printed", "Messages printed", msg_count));
  else
  {
    mutt_message(ngettext("Message could not be printed",
                          "Messages could not be printed", msg_count));
  }
}

/**
 * mutt_select_sort - Ask the user for a sort method
 * @param reverse If true make it a reverse sort
 * @retval num Sort type, see #SortType
 */
int mutt_select_sort(bool reverse)
{
  enum SortType method = C_Sort; /* save the current method in case of abort */
  enum SortType new_sort = C_Sort;

  switch (mutt_multi_choice(reverse ?
                                /* L10N: The highlighted letters must match the "Sort" options */
                                _("Rev-Sort "
                                  "(d)ate/(f)rm/(r)ecv/(s)ubj/t(o)/(t)hread/"
                                  "(u)nsort/si(z)e/s(c)ore/s(p)am/(l)abel?") :
                                /* L10N: The highlighted letters must match the "Rev-Sort" options */
                                _("Sort "
                                  "(d)ate/(f)rm/(r)ecv/(s)ubj/t(o)/(t)hread/"
                                  "(u)nsort/si(z)e/s(c)ore/s(p)am/(l)abel?"),
                            /* L10N: These must match the highlighted letters from "Sort" and "Rev-Sort" */
                            _("dfrsotuzcpl")))
  {
    case -1: /* abort - don't resort */
      return -1;

    case 1: /* (d)ate */
      new_sort = SORT_DATE;
      break;

    case 2: /* (f)rm */
      new_sort = SORT_FROM;
      break;

    case 3: /* (r)ecv */
      new_sort = SORT_RECEIVED;
      break;

    case 4: /* (s)ubj */
      new_sort = SORT_SUBJECT;
      break;

    case 5: /* t(o) */
      new_sort = SORT_TO;
      break;

    case 6: /* (t)hread */
      new_sort = SORT_THREADS;
      break;

    case 7: /* (u)nsort */
      new_sort = SORT_ORDER;
      break;

    case 8: /* si(z)e */
      new_sort = SORT_SIZE;
      break;

    case 9: /* s(c)ore */
      new_sort = SORT_SCORE;
      break;

    case 10: /* s(p)am */
      new_sort = SORT_SPAM;
      break;

    case 11: /* (l)abel */
      new_sort = SORT_LABEL;
      break;
  }
  if (reverse)
    new_sort |= SORT_REVERSE;

  cs_str_native_set(NeoMutt->sub->cs, "sort", new_sort, NULL);
  return (C_Sort != method) ? 0 : -1; /* no need to resort if it's the same */
}

/**
 * mutt_shell_escape - invoke a command in a subshell
 */
void mutt_shell_escape(void)
{
  char buf[1024];

  buf[0] = '\0';
  if (mutt_get_field(_("Shell command: "), buf, sizeof(buf), MUTT_CMD) != 0)
    return;

  if ((buf[0] == '\0') && C_Shell)
    mutt_str_strfcpy(buf, C_Shell, sizeof(buf));
  if (buf[0] == '\0')
    return;

  mutt_window_clearline(MuttMessageWindow, 0);
  mutt_endwin();
  fflush(stdout);
  int rc = mutt_system(buf);
  if (rc == -1)
    mutt_debug(LL_DEBUG1, "Error running \"%s\"", buf);

  if ((rc != 0) || C_WaitKey)
    mutt_any_key_to_continue(NULL);
  mutt_mailbox_check(Context->mailbox, MUTT_MAILBOX_CHECK_FORCE);
}

/**
 * mutt_enter_command - enter a neomutt command
 */
void mutt_enter_command(void)
{
  char buf[1024] = { 0 };

  /* if enter is pressed after : with no command, just return */
  if ((mutt_get_field(":", buf, sizeof(buf), MUTT_COMMAND) != 0) || !buf[0])
    return;

  struct Buffer err = mutt_buffer_make(256);
  struct Buffer token = mutt_buffer_make(256);

  /* check if buf is a valid icommand, else fall back quietly to parse_rc_lines */
  enum CommandResult rc = mutt_parse_icommand(buf, &err);
  if (!mutt_buffer_is_empty(&err))
  {
    /* since errbuf could potentially contain printf() sequences in it,
     * we must call mutt_error() in this fashion so that vsprintf()
     * doesn't expect more arguments that we passed */
    if (rc == MUTT_CMD_ERROR)
      mutt_error("%s", err.data);
    else
      mutt_warning("%s", err.data);
  }
  else if (rc != MUTT_CMD_SUCCESS)
  {
    rc = mutt_parse_rc_line(buf, &token, &err);
    if (!mutt_buffer_is_empty(&err))
    {
      if (rc == MUTT_CMD_SUCCESS) /* command succeeded with message */
        mutt_message("%s", err.data);
      else if (rc == MUTT_CMD_ERROR)
        mutt_error("%s", err.data);
      else if (rc == MUTT_CMD_WARNING)
        mutt_warning("%s", err.data);
    }
  }
  /* else successful command */

  mutt_buffer_dealloc(&token);
  mutt_buffer_dealloc(&err);
}

/**
 * mutt_display_address - Display the address of a message
 * @param env Envelope containing address
 */
void mutt_display_address(struct Envelope *env)
{
  const char *pfx = NULL;
  char buf[128];

  struct AddressList *al = mutt_get_address(env, &pfx);
  if (!al)
    return;

  /* Note: We don't convert IDNA to local representation this time.
   * That is intentional, so the user has an opportunity to copy &
   * paste the on-the-wire form of the address to other, IDN-unable
   * software.  */
  buf[0] = '\0';
  mutt_addrlist_write(al, buf, sizeof(buf), false);
  mutt_message("%s: %s", pfx, buf);
}

/**
 * set_copy_flags - Set the flags for a message copy
 * @param[in]  e       Email
 * @param[in]  decode  If true, decode the message
 * @param[in]  decrypt If true, decrypt the message
 * @param[out] cmflags Flags, see #CopyMessageFlags
 * @param[out] chflags Flags, see #CopyHeaderFlags
 */
static void set_copy_flags(struct Email *e, bool decode, bool decrypt,
                           CopyMessageFlags *cmflags, CopyHeaderFlags *chflags)
{
  *cmflags = MUTT_CM_NO_FLAGS;
  *chflags = CH_UPDATE_LEN;

  if ((WithCrypto != 0) && !decode && decrypt && (e->security & SEC_ENCRYPT))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_multipart_encrypted(e->content))
    {
      *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
      *cmflags = MUTT_CM_DECODE_PGP;
    }
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             mutt_is_application_pgp(e->content) & SEC_ENCRYPT)
    {
      decode = 1;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) &&
             mutt_is_application_smime(e->content) & SEC_ENCRYPT)
    {
      *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
      *cmflags = MUTT_CM_DECODE_SMIME;
    }
  }

  if (decode)
  {
    *chflags = CH_XMIT | CH_MIME | CH_TXTPLAIN;
    *cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;

    if (!decrypt) /* If decode doesn't kick in for decrypt, */
    {
      *chflags |= CH_DECODE; /* then decode RFC2047 headers, */

      if (C_Weed)
      {
        *chflags |= CH_WEED; /* and respect $weed. */
        *cmflags |= MUTT_CM_WEED;
      }
    }
  }
}

/**
 * mutt_save_message_ctx - Save a message to a given mailbox
 * @param e                Email
 * @param delete_original  If true, delete the original
 * @param decode           If true, decode the message
 * @param decrypt          If true, decrypt the message
 * @param m                Mailbox to save to
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_save_message_ctx(struct Email *e, bool delete_original, bool decode,
                          bool decrypt, struct Mailbox *m)
{
  CopyMessageFlags cmflags = MUTT_CM_NO_FLAGS;
  CopyHeaderFlags chflags = CH_NO_FLAGS;
  int rc;

  set_copy_flags(e, decode, decrypt, &cmflags, &chflags);

  if (decode || decrypt)
    mutt_parse_mime_message(Context->mailbox, e);

  rc = mutt_append_message(m, Context->mailbox, e, cmflags, chflags);
  if (rc != 0)
    return rc;

  if (delete_original)
  {
    mutt_set_flag(Context->mailbox, e, MUTT_DELETE, true);
    mutt_set_flag(Context->mailbox, e, MUTT_PURGE, true);
    if (C_DeleteUntag)
      mutt_set_flag(Context->mailbox, e, MUTT_TAG, false);
  }

  return 0;
}

/**
 * mutt_save_message - Save an email
 * @param m                Mailbox
 * @param el               List of Emails to save
 * @param delete_original  If true, delete the original (save)
 * @param decode           If true, decode the message
 * @param decrypt          If true, decrypt the message
 * @retval  0 Copy/save was successful
 * @retval -1 Error/abort
 */
int mutt_save_message(struct Mailbox *m, struct EmailList *el,
                      bool delete_original, bool decode, bool decrypt)
{
  if (!el || STAILQ_EMPTY(el))
    return -1;

  bool need_passphrase = false;
  int app = 0;
  int rc = -1;
  const char *prompt = NULL;
  struct stat st;
  struct EmailNode *en = STAILQ_FIRST(el);
  bool single = !STAILQ_NEXT(en, entries);

  struct Buffer *buf = mutt_buffer_pool_get();

  if (delete_original)
  {
    if (decode)
      prompt = single ? _("Decode-save to mailbox") : _("Decode-save tagged to mailbox");
    else if (decrypt)
      prompt = single ? _("Decrypt-save to mailbox") : _("Decrypt-save tagged to mailbox");
    else
      prompt = single ? _("Save to mailbox") : _("Save tagged to mailbox");
  }
  else
  {
    if (decode)
      prompt = single ? _("Decode-copy to mailbox") : _("Decode-copy tagged to mailbox");
    else if (decrypt)
      prompt = single ? _("Decrypt-copy to mailbox") : _("Decrypt-copy tagged to mailbox");
    else
      prompt = single ? _("Copy to mailbox") : _("Copy tagged to mailbox");
  }

  if (WithCrypto)
  {
    need_passphrase = (en->email->security & SEC_ENCRYPT);
    app = en->email->security;
  }
  mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
  mutt_default_save(buf->data, buf->dsize, en->email);
  mutt_buffer_fix_dptr(buf);

  mutt_buffer_pretty_mailbox(buf);
  if (mutt_buffer_enter_fname(prompt, buf, 0) == -1)
    goto cleanup;

  size_t pathlen = mutt_buffer_len(buf);
  if (pathlen == 0)
    goto cleanup;

  /* Trim any trailing '/' */
  if (buf->data[pathlen - 1] == '/')
    buf->data[pathlen - 1] = '\0';

  /* This is an undocumented feature of ELM pointed out to me by Felix von
   * Leitner <leitner@prz.fu-berlin.de> */
  if (mutt_str_strcmp(mutt_b2s(buf), ".") == 0)
    mutt_buffer_strcpy(buf, LastSaveFolder);
  else
    mutt_str_strfcpy(LastSaveFolder, mutt_b2s(buf), sizeof(LastSaveFolder));

  mutt_buffer_expand_path(buf);

  /* check to make sure that this file is really the one the user wants */
  if (mutt_save_confirm(mutt_b2s(buf), &st) != 0)
    goto cleanup;

  if ((WithCrypto != 0) && need_passphrase && (decode || decrypt) &&
      !crypt_valid_passphrase(app))
  {
    goto cleanup;
  }

  mutt_message(_("Copying to %s..."), mutt_b2s(buf));

#ifdef USE_IMAP
  if ((m->magic == MUTT_IMAP) && !(decode || decrypt) &&
      (imap_path_probe(mutt_b2s(buf), NULL) == MUTT_IMAP))
  {
    switch (imap_copy_messages(m, el, mutt_b2s(buf), delete_original))
    {
      /* success */
      case 0:
        mutt_clear_error();
        rc = 0;
        goto cleanup;
      /* non-fatal error: continue to fetch/append */
      case 1:
        break;
      /* fatal error, abort */
      case -1:
        goto cleanup;
    }
  }
#endif

  struct Mailbox *m_save = mx_path_resolve(mutt_b2s(buf));
  bool old_append = m_save->append;
  struct Context *ctx_save = mx_mbox_open(m_save, MUTT_NEWFOLDER);
  if (!ctx_save)
  {
    mailbox_free(&m_save);
    goto cleanup;
  }
  m_save->append = true;

#ifdef USE_COMPRESSED
  /* If we're saving to a compressed mailbox, the stats won't be updated
   * until the next open.  Until then, improvise. */
  struct Mailbox *m_comp = NULL;
  if (ctx_save->mailbox->compress_info)
  {
    m_comp = mailbox_find(ctx_save->mailbox->realpath);
  }
  /* We probably haven't been opened yet */
  if (m_comp && (m_comp->msg_count == 0))
    m_comp = NULL;
#endif
  if (single)
  {
    if (mutt_save_message_ctx(en->email, delete_original, decode, decrypt,
                              ctx_save->mailbox) != 0)
    {
      m_save->append = old_append;
      mx_mbox_close(&ctx_save);
      goto cleanup;
    }
#ifdef USE_COMPRESSED
    if (m_comp)
    {
      m_comp->msg_count++;
      if (!en->email->read)
      {
        m_comp->msg_unread++;
        if (!en->email->old)
          m_comp->msg_new++;
      }
      if (en->email->flagged)
        m_comp->msg_flagged++;
    }
#endif
  }
  else
  {
    rc = 0;

#ifdef USE_NOTMUCH
    if (m->magic == MUTT_NOTMUCH)
      nm_db_longrun_init(m, true);
#endif
    STAILQ_FOREACH(en, el, entries)
    {
      mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
      rc = mutt_save_message_ctx(en->email, delete_original, decode, decrypt,
                                 ctx_save->mailbox);
      if (rc != 0)
        break;
#ifdef USE_COMPRESSED
      if (m_comp)
      {
        struct Email *e2 = en->email;
        m_comp->msg_count++;
        if (!e2->read)
        {
          m_comp->msg_unread++;
          if (!e2->old)
            m_comp->msg_new++;
        }
        if (e2->flagged)
          m_comp->msg_flagged++;
      }
#endif
    }
#ifdef USE_NOTMUCH
    if (m->magic == MUTT_NOTMUCH)
      nm_db_longrun_done(m);
#endif
    if (rc != 0)
    {
      m_save->append = old_append;
      mx_mbox_close(&ctx_save);
      goto cleanup;
    }
  }

  const bool need_mailbox_cleanup = ((ctx_save->mailbox->magic == MUTT_MBOX) ||
                                     (ctx_save->mailbox->magic == MUTT_MMDF));

  m_save->append = old_append;
  mx_mbox_close(&ctx_save);

  if (need_mailbox_cleanup)
    mutt_mailbox_cleanup(mutt_b2s(buf), &st);

  mutt_clear_error();
  rc = 0;

cleanup:
  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * mutt_edit_content_type - Edit the content type of an attachment
 * @param e  Email
 * @param b  Attachment
 * @param fp File handle to the attachment
 * @retval bool true if a structural change is made
 *
 * recvattach requires the return code to know when to regenerate the actx.
 */
bool mutt_edit_content_type(struct Email *e, struct Body *b, FILE *fp)
{
  char buf[1024];
  char obuf[1024];
  char tmp[256];
  char charset[256];

  bool charset_changed = false;
  bool type_changed = false;
  bool structure_changed = false;

  char *cp = mutt_param_get(&b->parameter, "charset");
  mutt_str_strfcpy(charset, cp, sizeof(charset));

  snprintf(buf, sizeof(buf), "%s/%s", TYPE(b), b->subtype);
  mutt_str_strfcpy(obuf, buf, sizeof(obuf));
  if (!TAILQ_EMPTY(&b->parameter))
  {
    size_t l = strlen(buf);
    struct Parameter *np = NULL;
    TAILQ_FOREACH(np, &b->parameter, entries)
    {
      mutt_addr_cat(tmp, sizeof(tmp), np->value, MimeSpecials);
      l += snprintf(buf + l, sizeof(buf) - l, "; %s=%s", np->attribute, tmp);
      if (l >= sizeof(buf))
      {
        // L10N: e.g. "text/plain; charset=UTF-8; ..."
        mutt_error(_("Content type is too long"));
        return false;
      }
    }
  }

  if ((mutt_get_field("Content-Type: ", buf, sizeof(buf), 0) != 0) || (buf[0] == '\0'))
    return false;

  /* clean up previous junk */
  mutt_param_free(&b->parameter);
  FREE(&b->subtype);

  mutt_parse_content_type(buf, b);

  snprintf(tmp, sizeof(tmp), "%s/%s", TYPE(b), NONULL(b->subtype));
  type_changed = (mutt_str_strcasecmp(tmp, obuf) != 0);
  charset_changed =
      (mutt_str_strcasecmp(charset, mutt_param_get(&b->parameter, "charset")) != 0);

  /* if in send mode, check for conversion - current setting is default. */

  if (!e && (b->type == TYPE_TEXT) && charset_changed)
  {
    snprintf(tmp, sizeof(tmp), _("Convert to %s upon sending?"),
             mutt_param_get(&b->parameter, "charset"));
    int ans = mutt_yesorno(tmp, b->noconv ? MUTT_NO : MUTT_YES);
    if (ans != MUTT_ABORT)
      b->noconv = (ans == MUTT_NO);
  }

  /* inform the user */

  snprintf(tmp, sizeof(tmp), "%s/%s", TYPE(b), NONULL(b->subtype));
  if (type_changed)
    mutt_message(_("Content-Type changed to %s"), tmp);
  if ((b->type == TYPE_TEXT) && charset_changed)
  {
    if (type_changed)
      mutt_sleep(1);
    mutt_message(b->noconv ? _("Character set changed to %s; not converting") :
                             _("Character set changed to %s; converting"),
                 mutt_param_get(&b->parameter, "charset"));
  }

  b->force_charset |= charset_changed;

  if (!is_multipart(b) && b->parts)
  {
    structure_changed = true;
    mutt_body_free(&b->parts);
  }
  if (!mutt_is_message_type(b->type, b->subtype) && b->email)
  {
    structure_changed = true;
    b->email->content = NULL;
    email_free(&b->email);
  }

  if (fp && !b->parts && (is_multipart(b) || mutt_is_message_type(b->type, b->subtype)))
  {
    structure_changed = true;
    mutt_parse_part(fp, b);
  }

  if ((WithCrypto != 0) && e)
  {
    if (e->content == b)
      e->security = SEC_NO_FLAGS;

    e->security |= crypt_query(b);
  }

  return structure_changed;
}

/**
 * check_traditional_pgp - Check for an inline PGP content
 * @param[in]  e      Email to check
 * @param[out] redraw Flags if the screen needs redrawing, see #MuttRedrawFlags
 * @retval true If message contains inline PGP content
 */
static bool check_traditional_pgp(struct Email *e, MuttRedrawFlags *redraw)
{
  bool rc = false;

  e->security |= PGP_TRADITIONAL_CHECKED;

  mutt_parse_mime_message(Context->mailbox, e);
  struct Message *msg = mx_msg_open(Context->mailbox, e->msgno);
  if (!msg)
    return 0;
  if (crypt_pgp_check_traditional(msg->fp, e->content, false))
  {
    e->security = crypt_query(e->content);
    *redraw |= REDRAW_FULL;
    rc = true;
  }

  e->security |= PGP_TRADITIONAL_CHECKED;
  mx_msg_close(Context->mailbox, &msg);
  return rc;
}

/**
 * mutt_check_traditional_pgp - Check if a message has inline PGP content
 * @param[in]  el     List of Emails to check
 * @param[out] redraw Flags if the screen needs redrawing, see #MuttRedrawFlags
 * @retval true If message contains inline PGP content
 */
bool mutt_check_traditional_pgp(struct EmailList *el, MuttRedrawFlags *redraw)
{
  bool rc = false;
  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    if (!(en->email->security & PGP_TRADITIONAL_CHECKED))
      rc = check_traditional_pgp(en->email, redraw) || rc;
  }

  return rc;
}

/**
 * mutt_check_stats - Forcibly update mailbox stats
 */
void mutt_check_stats(void)
{
  mutt_mailbox_check(Context->mailbox, MUTT_MAILBOX_CHECK_FORCE | MUTT_MAILBOX_CHECK_FORCE_STATS);
}
