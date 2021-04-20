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
 * @page neo_commands Manage where the email is piped to external commands
 *
 * Manage where the email is piped to external commands
 */

#include "config.h"
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "commands.h"
#include "ncrypt/lib.h"
#include "pager/lib.h"
#include "send/lib.h"
#include "attachments.h"
#include "browser.h"
#include "context.h"
#include "copy.h"
#include "format_flags.h"
#include "hdrline.h"
#include "hook.h"
#include "icommands.h"
#include "init.h"
#include "keymap.h"
#include "mutt_commands.h"
#include "mutt_globals.h"
#include "mutt_logging.h"
#include "mutt_mailbox.h"
#include "muttlib.h"
#include "mx.h"
#include "options.h"
#include "progress.h"
#include "protos.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif
#ifdef USE_NOTMUCH
#include "notmuch/lib.h"
#endif
#ifdef ENABLE_NLS
#include <libintl.h>
#endif
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

static const char *ExtPagerProgress = "all";

/** The folder the user last saved to.  Used by ci_save_message() */
static struct Buffer LastSaveFolder = { 0 };

/**
 * mutt_commands_cleanup - Clean up commands globals
 */
void mutt_commands_cleanup(void)
{
  mutt_buffer_dealloc(&LastSaveFolder);
}

/**
 * process_protected_headers - Get the protected header and update the index
 * @param m Mailbox
 * @param e Email to update
 */
static void process_protected_headers(struct Mailbox *m, struct Email *e)
{
  struct Envelope *prot_headers = NULL;
  regmatch_t pmatch[1];

  const bool c_crypt_protected_headers_read =
      cs_subset_bool(NeoMutt->sub, "crypt_protected_headers_read");
#ifdef USE_AUTOCRYPT
  const bool c_autocrypt = cs_subset_bool(NeoMutt->sub, "autocrypt");
  if (!c_crypt_protected_headers_read && !c_autocrypt)
    return;
#else
  if (!c_crypt_protected_headers_read)
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

    if (mutt_is_multipart_signed(e->body) && e->body->parts)
    {
      prot_headers = e->body->parts->mime_headers;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(e->body))
    {
      prot_headers = e->body->mime_headers;
    }
  }
  if (!prot_headers && (e->security & SEC_ENCRYPT))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) &&
        (mutt_is_valid_multipart_pgp_encrypted(e->body) ||
         mutt_is_malformed_multipart_pgp_encrypted(e->body)))
    {
      prot_headers = e->body->mime_headers;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(e->body))
    {
      prot_headers = e->body->mime_headers;
    }
  }

  /* Update protected headers in the index and header cache. */
  if (c_crypt_protected_headers_read && prot_headers && prot_headers->subject &&
      !mutt_str_equal(e->env->subject, prot_headers->subject))
  {
    if (m->subj_hash && e->env->real_subj)
      mutt_hash_delete(m->subj_hash, e->env->real_subj, e);

    mutt_str_replace(&e->env->subject, prot_headers->subject);
    FREE(&e->env->disp_subj);
    const struct Regex *c_reply_regex =
        cs_subset_regex(NeoMutt->sub, "reply_regex");
    if (mutt_regex_capture(c_reply_regex, e->env->subject, 1, pmatch))
      e->env->real_subj = e->env->subject + pmatch[0].rm_eo;
    else
      e->env->real_subj = e->env->subject;

    if (m->subj_hash)
      mutt_hash_insert(m->subj_hash, e->env->real_subj, e);

    mx_save_hcache(m, e);

    /* Also persist back to the message headers if this is set */
    const bool c_crypt_protected_headers_save =
        cs_subset_bool(NeoMutt->sub, "crypt_protected_headers_save");
    if (c_crypt_protected_headers_save)
    {
      e->env->changed |= MUTT_ENV_CHANGED_SUBJECT;
      e->changed = true;
      m->changed = true;
    }
  }

#ifdef USE_AUTOCRYPT
  if (c_autocrypt && (e->security & SEC_ENCRYPT) && prot_headers && prot_headers->autocrypt_gossip)
  {
    mutt_autocrypt_process_gossip_header(m, e, prot_headers);
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

  struct Message *msg = mx_msg_open(m, e->msgno);
  mutt_parse_mime_message(m, e, msg);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  char columns[16];
  // win_pager might not be visible and have a size yet, so use win_index
  snprintf(columns, sizeof(columns), "%d", win_index->state.cols);
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
      const enum QuadOption c_crypt_verify_sig =
          cs_subset_quad(NeoMutt->sub, "crypt_verify_sig");
      if (query_quadoption(c_crypt_verify_sig, _("Verify signature?")) == MUTT_YES)
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
  FILE *fp_out = mutt_file_fopen(mutt_buffer_string(tempfile), "w");
  if (!fp_out)
  {
    mutt_error(_("Could not create temporary file"));
    goto cleanup;
  }

  const char *const c_display_filter =
      cs_subset_string(NeoMutt->sub, "display_filter");
  if (c_display_filter)
  {
    fp_filter_out = fp_out;
    fp_out = NULL;
    filterpid = filter_create_fd(c_display_filter, &fp_out, NULL, NULL, -1,
                                 fileno(fp_filter_out), -1);
    if (filterpid < 0)
    {
      mutt_error(_("Can't create display filter"));
      mutt_file_fclose(&fp_filter_out);
      unlink(mutt_buffer_string(tempfile));
      goto cleanup;
    }
  }

  const char *const c_pager = cs_subset_string(NeoMutt->sub, "pager");
  if (!c_pager || mutt_str_equal(c_pager, "builtin"))
    builtin = true;
  else
  {
    char buf[1024] = { 0 };
    const char *const c_pager_format =
        cs_subset_string(NeoMutt->sub, "pager_format");
    mutt_make_string(buf, sizeof(buf), win_index->state.cols, NONULL(c_pager_format),
                     m, Context ? Context->msg_in_pager : -1, e,
                     MUTT_FORMAT_NO_FLAGS, ExtPagerProgress);
    fputs(buf, fp_out);
    fputs("\n\n", fp_out);
  }

  const bool c_weed = cs_subset_bool(NeoMutt->sub, "weed");
  chflags = (c_weed ? (CH_WEED | CH_REORDER) : CH_NO_FLAGS) | CH_DECODE | CH_FROM | CH_DISPLAY;
#ifdef USE_NOTMUCH
  if (m->type == MUTT_NOTMUCH)
    chflags |= CH_VIRTUAL;
#endif
  res = mutt_copy_message(fp_out, m, e, msg, cmflags, chflags, win_index->state.cols);

  if (((mutt_file_fclose(&fp_out) != 0) && (errno != EPIPE)) || (res < 0))
  {
    mutt_error(_("Could not copy message"));
    if (fp_filter_out)
    {
      filter_wait(filterpid);
      mutt_file_fclose(&fp_filter_out);
    }
    mutt_file_unlink(mutt_buffer_string(tempfile));
    goto cleanup;
  }

  if (fp_filter_out && (filter_wait(filterpid) != 0))
    mutt_any_key_to_continue(NULL);

  mutt_file_fclose(&fp_filter_out); /* XXX - check result? */

  if (WithCrypto)
  {
    /* update crypto information for this message */
    e->security &= ~(SEC_GOODSIGN | SEC_BADSIGN);
    e->security |= crypt_query(e->body);

    /* Remove color cache for this message, in case there
     * are color patterns for both ~g and ~V */
    e->pair = 0;

    /* Process protected headers and autocrypt gossip headers */
    process_protected_headers(m, e);
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

    /* Invoke the builtin pager */
    struct PagerData pdata = { 0 };
    struct PagerView pview = { &pdata };

    pdata.email = e;
    pdata.msg = msg;
    pdata.ctx = Context;
    pdata.fname = mutt_buffer_string(tempfile);

    pview.mode = PAGER_MODE_EMAIL;
    pview.banner = NULL;
    pview.flags = MUTT_PAGER_MESSAGE | (e->body->nowrap ? MUTT_PAGER_NOWRAP : 0);
    pview.win_ibar = win_ibar;
    pview.win_index = win_index;
    pview.win_pbar = win_pbar;
    pview.win_pager = win_pager;
    rc = mutt_pager(&pview);
  }
  else
  {
    mutt_endwin();

    struct Buffer *cmd = mutt_buffer_pool_get();
    mutt_buffer_printf(cmd, "%s %s", NONULL(c_pager), mutt_buffer_string(tempfile));
    int r = mutt_system(mutt_buffer_string(cmd));
    if (r == -1)
      mutt_error(_("Error running \"%s\""), mutt_buffer_string(cmd));
    unlink(mutt_buffer_string(tempfile));
    mutt_buffer_pool_release(&cmd);

    if (!OptNoCurses)
      keypad(stdscr, true);
    if (r != -1)
      mutt_set_flag(m, e, MUTT_READ, true);
    const bool c_prompt_after = cs_subset_bool(NeoMutt->sub, "prompt_after");
    if ((r != -1) && c_prompt_after)
    {
      mutt_unget_event(mutt_any_key_to_continue(_("Command: ")), 0);
      rc = km_dokey(MENU_PAGER);
    }
    else
      rc = 0;
  }

cleanup:
  mx_msg_close(m, &msg);
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
    if (TAILQ_EMPTY(&en->email->env->from))
      mutt_error(_("Warning: message contains no From: header"));

    msg_count++;
  }

  if (msg_count == 1)
    mutt_str_copy(prompt, _("Bounce message to: "), sizeof(prompt));
  else
    mutt_str_copy(prompt, _("Bounce tagged messages to: "), sizeof(prompt));

  rc = mutt_get_field(prompt, buf, sizeof(buf), MUTT_ALIAS, false, NULL, NULL);
  if (rc || (buf[0] == '\0'))
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

  if (mutt_strwidth(scratch) > (MessageWindow->state.cols - EXTRA_SPACE))
  {
    mutt_simple_format(prompt, sizeof(prompt), 0, MessageWindow->state.cols - EXTRA_SPACE,
                       JUSTIFY_LEFT, 0, scratch, sizeof(scratch), false);
    mutt_str_cat(prompt, sizeof(prompt), "...?");
  }
  else
    mutt_str_copy(prompt, scratch, sizeof(prompt));

  const enum QuadOption c_bounce = cs_subset_quad(NeoMutt->sub, "bounce");
  if (query_quadoption(c_bounce, prompt) != MUTT_YES)
  {
    mutt_addrlist_clear(&al);
    mutt_window_clearline(MessageWindow, 0);
    mutt_message(ngettext("Message not bounced", "Messages not bounced", msg_count));
    return;
  }

  mutt_window_clearline(MessageWindow, 0);

  struct Message *msg = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    msg = mx_msg_open(m, en->email->msgno);
    if (!msg)
    {
      rc = -1;
      break;
    }

    rc = mutt_bounce_message(msg->fp, m, en->email, &al, NeoMutt->sub);
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

    const bool c_print_decode_weed =
        cs_subset_bool(NeoMutt->sub, "print_decode_weed");
    const bool c_pipe_decode_weed =
        cs_subset_bool(NeoMutt->sub, "pipe_decode_weed");
    if (print ? c_print_decode_weed : c_pipe_decode_weed)
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
 * @param msg    Message
 * @param fp     File to write to
 * @param decode If true, decode the message
 * @param print  If true, message is for printing
 */
static void pipe_msg(struct Mailbox *m, struct Email *e, struct Message *msg,
                     FILE *fp, bool decode, bool print)
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

  const bool own_msg = !msg;
  if (own_msg)
  {
    msg = mx_msg_open(m, e->msgno);
  }

  if (decode)
  {
    mutt_parse_mime_message(m, e, msg);
  }

  mutt_copy_message(fp, m, e, msg, cmflags, chflags, 0);

  if (own_msg)
  {
    mx_msg_close(m, &msg);
  }
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
static int pipe_message(struct Mailbox *m, struct EmailList *el, const char *cmd,
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

    struct Message *msg = mx_msg_open(m, en->email->msgno);
    if (WithCrypto && decode)
    {
      mutt_parse_mime_message(m, en->email, msg);
      if ((en->email->security & SEC_ENCRYPT) &&
          !crypt_valid_passphrase(en->email->security))
      {
        mx_msg_close(m, &msg);
        return 1;
      }
    }
    mutt_endwin();

    pid = filter_create(cmd, &fp_out, NULL, NULL);
    if (pid < 0)
    {
      mutt_perror(_("Can't create filter process"));
      mx_msg_close(m, &msg);
      return 1;
    }

    OptKeepQuiet = true;
    pipe_msg(m, en->email, msg, fp_out, decode, print);
    mx_msg_close(m, &msg);
    mutt_file_fclose(&fp_out);
    rc = filter_wait(pid);
    OptKeepQuiet = false;
  }
  else
  {
    /* handle tagged messages */
    if ((WithCrypto != 0) && decode)
    {
      STAILQ_FOREACH(en, el, entries)
      {
        struct Message *msg = mx_msg_open(m, en->email->msgno);
        mutt_parse_mime_message(m, en->email, msg);
        mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
        mx_msg_close(m, &msg);
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
        pid = filter_create(cmd, &fp_out, NULL, NULL);
        if (pid < 0)
        {
          mutt_perror(_("Can't create filter process"));
          return 1;
        }
        OptKeepQuiet = true;
        pipe_msg(m, en->email, NULL, fp_out, decode, print);
        /* add the message separator */
        if (sep)
          fputs(sep, fp_out);
        mutt_file_fclose(&fp_out);
        if (filter_wait(pid) != 0)
          rc = 1;
        OptKeepQuiet = false;
      }
    }
    else
    {
      mutt_endwin();
      pid = filter_create(cmd, &fp_out, NULL, NULL);
      if (pid < 0)
      {
        mutt_perror(_("Can't create filter process"));
        return 1;
      }
      OptKeepQuiet = true;
      STAILQ_FOREACH(en, el, entries)
      {
        mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
        pipe_msg(m, en->email, NULL, fp_out, decode, print);
        /* add the message separator */
        if (sep)
          fputs(sep, fp_out);
      }
      mutt_file_fclose(&fp_out);
      if (filter_wait(pid) != 0)
        rc = 1;
      OptKeepQuiet = false;
    }
  }

  const bool c_wait_key = cs_subset_bool(NeoMutt->sub, "wait_key");
  if ((rc != 0) || c_wait_key)
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

  struct Buffer *buf = mutt_buffer_pool_get();

  if (mutt_buffer_get_field(_("Pipe to command: "), buf, MUTT_CMD, false, NULL,
                            NULL, NULL) != 0)
  {
    goto cleanup;
  }

  if (mutt_buffer_len(buf) == 0)
    goto cleanup;

  mutt_buffer_expand_path(buf);
  const bool c_pipe_decode = cs_subset_bool(NeoMutt->sub, "pipe_decode");
  const bool c_pipe_split = cs_subset_bool(NeoMutt->sub, "pipe_split");
  const char *const c_pipe_sep = cs_subset_string(NeoMutt->sub, "pipe_sep");
  pipe_message(m, el, mutt_buffer_string(buf), c_pipe_decode, false, c_pipe_split, c_pipe_sep);

cleanup:
  mutt_buffer_pool_release(&buf);
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

  const enum QuadOption c_print = cs_subset_quad(NeoMutt->sub, "print");
  const char *const c_print_command =
      cs_subset_string(NeoMutt->sub, "print_command");
  if (c_print && !c_print_command)
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

  if (query_quadoption(c_print, (msg_count == 1) ?
                                    _("Print message?") :
                                    _("Print tagged messages?")) != MUTT_YES)
  {
    return;
  }

  const bool c_print_decode = cs_subset_bool(NeoMutt->sub, "print_decode");
  const bool c_print_split = cs_subset_bool(NeoMutt->sub, "print_split");
  if (pipe_message(m, el, c_print_command, c_print_decode, true, c_print_split, "\f") == 0)
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
bool mutt_select_sort(bool reverse)
{
  enum SortType sort = SORT_DATE;

  switch (mutt_multi_choice(reverse ?
                                /* L10N: The highlighted letters must match the "Sort" options */
                                _("Rev-Sort "
                                  "(d)ate,(f)rm,(r)ecv,(s)ubj,t(o),(t)hread,(u)"
                                  "nsort,si(z)e,s(c)ore,s(p)am,(l)abel?") :
                                /* L10N: The highlighted letters must match the "Rev-Sort" options */
                                _("Sort "
                                  "(d)ate,(f)rm,(r)ecv,(s)ubj,t(o),(t)hread,(u)"
                                  "nsort,si(z)e,s(c)ore,s(p)am,(l)abel?"),
                            /* L10N: These must match the highlighted letters from "Sort" and "Rev-Sort" */
                            _("dfrsotuzcpl")))
  {
    case -1: /* abort - don't resort */
      return -1;

    case 1: /* (d)ate */
      sort = SORT_DATE;
      break;

    case 2: /* (f)rm */
      sort = SORT_FROM;
      break;

    case 3: /* (r)ecv */
      sort = SORT_RECEIVED;
      break;

    case 4: /* (s)ubj */
      sort = SORT_SUBJECT;
      break;

    case 5: /* t(o) */
      sort = SORT_TO;
      break;

    case 6: /* (t)hread */
      sort = SORT_THREADS;
      break;

    case 7: /* (u)nsort */
      sort = SORT_ORDER;
      break;

    case 8: /* si(z)e */
      sort = SORT_SIZE;
      break;

    case 9: /* s(c)ore */
      sort = SORT_SCORE;
      break;

    case 10: /* s(p)am */
      sort = SORT_SPAM;
      break;

    case 11: /* (l)abel */
      sort = SORT_LABEL;
      break;
  }
  if (reverse)
    sort |= SORT_REVERSE;

  int rc = cs_subset_str_native_set(NeoMutt->sub, "sort", sort, NULL);

  return ((CSR_RESULT(rc) == CSR_SUCCESS) && !(rc & CSR_SUC_NO_CHANGE));
}

/**
 * mutt_shell_escape - invoke a command in a subshell
 * @retval true A command was invoked (no matter what its result)
 * @retval false No command was invoked
 */
bool mutt_shell_escape(void)
{
  char buf[1024];

  buf[0] = '\0';
  if (mutt_get_field(_("Shell command: "), buf, sizeof(buf), MUTT_CMD, false, NULL, NULL) != 0)
  {
    return false;
  }

  const char *const c_shell = cs_subset_string(NeoMutt->sub, "shell");
  if ((buf[0] == '\0') && c_shell)
    mutt_str_copy(buf, c_shell, sizeof(buf));
  if (buf[0] == '\0')
  {
    return false;
  }

  mutt_window_clearline(MessageWindow, 0);
  mutt_endwin();
  fflush(stdout);
  int rc = mutt_system(buf);
  if (rc == -1)
    mutt_debug(LL_DEBUG1, "Error running \"%s\"", buf);

  const bool c_wait_key = cs_subset_bool(NeoMutt->sub, "wait_key");
  if ((rc != 0) || c_wait_key)
    mutt_any_key_to_continue(NULL);

  return true;
}

/**
 * mutt_enter_command - enter a neomutt command
 */
void mutt_enter_command(void)
{
  char buf[1024] = { 0 };

  window_set_focus(MessageWindow);
  window_redraw(RootWindow, true);
  /* if enter is pressed after : with no command, just return */
  if ((mutt_get_field(":", buf, sizeof(buf), MUTT_COMMAND, false, NULL, NULL) != 0) ||
      (buf[0] == '\0'))
  {
    return;
  }

  struct Buffer err = mutt_buffer_make(256);

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
    rc = mutt_parse_rc_line(buf, &err);
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
 * @param[in]  e               Email
 * @param[in]  transform_opt   Transformation, e.g. #TRANSFORM_DECRYPT
 * @param[out] cmflags         Flags, see #CopyMessageFlags
 * @param[out] chflags         Flags, see #CopyHeaderFlags
 */
static void set_copy_flags(struct Email *e, enum MessageTransformOpt transform_opt,
                           CopyMessageFlags *cmflags, CopyHeaderFlags *chflags)
{
  *cmflags = MUTT_CM_NO_FLAGS;
  *chflags = CH_UPDATE_LEN;

  const bool need_decrypt =
      (transform_opt == TRANSFORM_DECRYPT) && (e->security & SEC_ENCRYPT);
  const bool want_pgp = (WithCrypto & APPLICATION_PGP);
  const bool want_smime = (WithCrypto & APPLICATION_SMIME);
  const bool is_pgp = mutt_is_application_pgp(e->body) & SEC_ENCRYPT;
  const bool is_smime = mutt_is_application_smime(e->body) & SEC_ENCRYPT;

  if (need_decrypt && want_pgp && mutt_is_multipart_encrypted(e->body))
  {
    *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
    *cmflags = MUTT_CM_DECODE_PGP;
  }
  else if (need_decrypt && want_pgp && is_pgp)
  {
    *chflags = CH_XMIT | CH_MIME | CH_TXTPLAIN;
    *cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
  }
  else if (need_decrypt && want_smime && is_smime)
  {
    *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
    *cmflags = MUTT_CM_DECODE_SMIME;
  }
  else if (transform_opt == TRANSFORM_DECODE)
  {
    *chflags = CH_XMIT | CH_MIME | CH_TXTPLAIN | CH_DECODE; // then decode RFC2047
    *cmflags = MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    const bool c_copy_decode_weed =
        cs_subset_bool(NeoMutt->sub, "copy_decode_weed");
    if (c_copy_decode_weed)
    {
      *chflags |= CH_WEED; // and respect $weed
      *cmflags |= MUTT_CM_WEED;
    }
  }
}

/**
 * mutt_save_message_ctx - Save a message to a given mailbox
 * @param m_src            Mailbox to copy from
 * @param e                Email
 * @param save_opt         Copy or move, e.g. #SAVE_MOVE
 * @param transform_opt    Transformation, e.g. #TRANSFORM_DECRYPT
 * @param m_dst            Mailbox to save to
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_save_message_ctx(struct Mailbox *m_src, struct Email *e, enum MessageSaveOpt save_opt,
                          enum MessageTransformOpt transform_opt, struct Mailbox *m_dst)
{
  CopyMessageFlags cmflags = MUTT_CM_NO_FLAGS;
  CopyHeaderFlags chflags = CH_NO_FLAGS;
  int rc;

  set_copy_flags(e, transform_opt, &cmflags, &chflags);

  struct Message *msg = mx_msg_open(m_src, e->msgno);
  if (transform_opt != TRANSFORM_NONE)
  {
    mutt_parse_mime_message(m_src, e, msg);
  }

  rc = mutt_append_message(m_dst, m_src, e, msg, cmflags, chflags);
  mx_msg_close(m_src, &msg);
  if (rc != 0)
    return rc;

  if (save_opt == SAVE_MOVE)
  {
    mutt_set_flag(m_src, e, MUTT_DELETE, true);
    mutt_set_flag(m_src, e, MUTT_PURGE, true);
    const bool c_delete_untag = cs_subset_bool(NeoMutt->sub, "delete_untag");
    if (c_delete_untag)
      mutt_set_flag(m_src, e, MUTT_TAG, false);
  }

  return 0;
}

/**
 * mutt_save_message - Save an email
 * @param m                Mailbox
 * @param el               List of Emails to save
 * @param save_opt         Copy or move, e.g. #SAVE_MOVE
 * @param transform_opt    Transformation, e.g. #TRANSFORM_DECRYPT
 * @retval  0 Copy/save was successful
 * @retval -1 Error/abort
 */
int mutt_save_message(struct Mailbox *m, struct EmailList *el,
                      enum MessageSaveOpt save_opt, enum MessageTransformOpt transform_opt)
{
  if (!el || STAILQ_EMPTY(el))
    return -1;

  int rc = -1;
  int tagged_progress_count = 0;
  unsigned int msg_count = 0;

  struct Buffer *buf = mutt_buffer_pool_get();
  struct Progress progress;
  struct stat st;
  struct EmailNode *en = NULL;

  STAILQ_FOREACH(en, el, entries)
  {
    msg_count++;
  }
  en = STAILQ_FIRST(el);

  const SecurityFlags security_flags = WithCrypto ? en->email->security : SEC_NO_FLAGS;
  const bool is_passphrase_needed = security_flags & SEC_ENCRYPT;

  const char *prompt = NULL;
  const char *progress_msg = NULL;

  // Set prompt and progress_msg
  switch (save_opt)
  {
    case SAVE_COPY:
      // L10N: Progress meter message when copying tagged messages
      progress_msg = (msg_count > 1) ? _("Copying tagged messages...") : NULL;
      switch (transform_opt)
      {
        case TRANSFORM_NONE:
          prompt = (msg_count > 1) ? _("Copy tagged to mailbox") : _("Copy to mailbox");
          break;
        case TRANSFORM_DECRYPT:
          prompt = (msg_count > 1) ? _("Decrypt-copy tagged to mailbox") :
                                     _("Decrypt-copy to mailbox");
          break;
        case TRANSFORM_DECODE:
          prompt = (msg_count > 1) ? _("Decode-copy tagged to mailbox") :
                                     _("Decode-copy to mailbox");
          break;
      }
      break;

    case SAVE_MOVE:
      // L10N: Progress meter message when saving tagged messages
      progress_msg = (msg_count > 1) ? _("Saving tagged messages...") : NULL;
      switch (transform_opt)
      {
        case TRANSFORM_NONE:
          prompt = (msg_count > 1) ? _("Save tagged to mailbox") : _("Save to mailbox");
          break;
        case TRANSFORM_DECRYPT:
          prompt = (msg_count > 1) ? _("Decrypt-save tagged to mailbox") :
                                     _("Decrypt-save to mailbox");
          break;
        case TRANSFORM_DECODE:
          prompt = (msg_count > 1) ? _("Decode-save tagged to mailbox") :
                                     _("Decode-save to mailbox");
          break;
      }
      break;
  }

  mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
  mutt_default_save(buf->data, buf->dsize, en->email);
  mutt_buffer_fix_dptr(buf);
  mutt_buffer_pretty_mailbox(buf);

  if (mutt_buffer_enter_fname(prompt, buf, false, NULL, false, NULL, NULL,
                              MUTT_SEL_NO_FLAGS) == -1)
    goto cleanup;

  size_t pathlen = mutt_buffer_len(buf);
  if (pathlen == 0)
    goto cleanup;

  /* Trim any trailing '/' */
  if (buf->data[pathlen - 1] == '/')
    buf->data[pathlen - 1] = '\0';

  /* This is an undocumented feature of ELM pointed out to me by Felix von
   * Leitner <leitner@prz.fu-berlin.de> */
  if (mutt_buffer_len(&LastSaveFolder) == 0)
    mutt_buffer_alloc(&LastSaveFolder, PATH_MAX);
  if (mutt_str_equal(mutt_buffer_string(buf), "."))
    mutt_buffer_copy(buf, &LastSaveFolder);
  else
    mutt_buffer_strcpy(&LastSaveFolder, mutt_buffer_string(buf));

  mutt_buffer_expand_path(buf);

  /* check to make sure that this file is really the one the user wants */
  if (mutt_save_confirm(mutt_buffer_string(buf), &st) != 0)
    goto cleanup;

  if (is_passphrase_needed && (transform_opt != TRANSFORM_NONE) &&
      !crypt_valid_passphrase(security_flags))
  {
    rc = -1;
    goto errcleanup;
  }

  mutt_message(_("Copying to %s..."), mutt_buffer_string(buf));

#ifdef USE_IMAP
  enum MailboxType mailbox_type = imap_path_probe(mutt_buffer_string(buf), NULL);
  if ((m->type == MUTT_IMAP) && (transform_opt == TRANSFORM_NONE) && (mailbox_type == MUTT_IMAP))
  {
    rc = imap_copy_messages(m, el, mutt_buffer_string(buf), save_opt);
    switch (rc)
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
        goto errcleanup;
    }
  }
#endif

  mutt_file_resolve_symlink(buf);
  struct Mailbox *m_save = mx_path_resolve(mutt_buffer_string(buf));
  bool old_append = m_save->append;
  OpenMailboxFlags mbox_flags = MUTT_NEWFOLDER;
  /* Display a tagged message progress counter, rather than (for
   * IMAP) a per-message progress counter */
  if (msg_count > 1)
    mbox_flags |= MUTT_QUIET;
  if (!mx_mbox_open(m_save, mbox_flags))
  {
    rc = -1;
    mailbox_free(&m_save);
    goto errcleanup;
  }
  m_save->append = true;

#ifdef USE_COMP_MBOX
  /* If we're saving to a compressed mailbox, the stats won't be updated
   * until the next open.  Until then, improvise. */
  struct Mailbox *m_comp = NULL;
  if (m_save->compress_info)
  {
    m_comp = mailbox_find(m_save->realpath);
  }
  /* We probably haven't been opened yet */
  if (m_comp && (m_comp->msg_count == 0))
    m_comp = NULL;
#endif
  if (msg_count == 1)
  {
    rc = mutt_save_message_ctx(m, en->email, save_opt, transform_opt, m_save);
    if (rc != 0)
    {
      mx_mbox_close(m_save);
      m_save->append = old_append;
      goto errcleanup;
    }
#ifdef USE_COMP_MBOX
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
    mutt_progress_init(&progress, progress_msg, MUTT_PROGRESS_WRITE, msg_count);

#ifdef USE_NOTMUCH
    if (m->type == MUTT_NOTMUCH)
      nm_db_longrun_init(m, true);
#endif
    STAILQ_FOREACH(en, el, entries)
    {
      mutt_progress_update(&progress, ++tagged_progress_count, -1);
      mutt_message_hook(m, en->email, MUTT_MESSAGE_HOOK);
      rc = mutt_save_message_ctx(m, en->email, save_opt, transform_opt, m_save);
      if (rc != 0)
        break;
#ifdef USE_COMP_MBOX
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
    if (m->type == MUTT_NOTMUCH)
      nm_db_longrun_done(m);
#endif
    if (rc != 0)
    {
      mx_mbox_close(m_save);
      m_save->append = old_append;
      goto errcleanup;
    }
  }

  const bool need_mailbox_cleanup =
      ((m_save->type == MUTT_MBOX) || (m_save->type == MUTT_MMDF));

  mx_mbox_close(m_save);
  m_save->append = old_append;

  if (need_mailbox_cleanup)
    mutt_mailbox_cleanup(mutt_buffer_string(buf), &st);

  mutt_clear_error();
  rc = 0;

errcleanup:
  if (rc != 0)
  {
    switch (save_opt)
    {
      case SAVE_MOVE:
        if (msg_count > 1)
        {
          // L10N: Message when an index tagged save operation fails for some reason
          mutt_error(_("Error saving tagged messages"));
        }
        else
        {
          // L10N: Message when an index/pager save operation fails for some reason
          mutt_error(_("Error saving message"));
        }
        break;
      case SAVE_COPY:
        if (msg_count > 1)
        {
          // L10N: Message when an index tagged copy operation fails for some reason
          mutt_error(_("Error copying tagged messages"));
        }
        else
        {
          // L10N: Message when an index/pager copy operation fails for some reason
          mutt_error(_("Error copying message"));
        }
        break;
    }
  }

cleanup:
  mutt_buffer_pool_release(&buf);
  return rc;
}

/**
 * mutt_edit_content_type - Edit the content type of an attachment
 * @param e  Email
 * @param b  Attachment
 * @param fp File handle to the attachment
 * @retval true A Any change is made
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
  mutt_str_copy(charset, cp, sizeof(charset));

  snprintf(buf, sizeof(buf), "%s/%s", TYPE(b), b->subtype);
  mutt_str_copy(obuf, buf, sizeof(obuf));
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

  if ((mutt_get_field("Content-Type: ", buf, sizeof(buf), MUTT_COMP_NO_FLAGS,
                      false, NULL, NULL) != 0) ||
      (buf[0] == '\0'))
  {
    return false;
  }

  /* clean up previous junk */
  mutt_param_free(&b->parameter);
  FREE(&b->subtype);

  mutt_parse_content_type(buf, b);

  snprintf(tmp, sizeof(tmp), "%s/%s", TYPE(b), NONULL(b->subtype));
  type_changed = !mutt_istr_equal(tmp, obuf);
  charset_changed =
      !mutt_istr_equal(charset, mutt_param_get(&b->parameter, "charset"));

  /* if in send mode, check for conversion - current setting is default. */

  if (!e && (b->type == TYPE_TEXT) && charset_changed)
  {
    snprintf(tmp, sizeof(tmp), _("Convert to %s upon sending?"),
             mutt_param_get(&b->parameter, "charset"));
    enum QuadOption ans = mutt_yesorno(tmp, b->noconv ? MUTT_NO : MUTT_YES);
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
    b->email->body = NULL;
    email_free(&b->email);
  }

  if (fp && !b->parts && (is_multipart(b) || mutt_is_message_type(b->type, b->subtype)))
  {
    structure_changed = true;
    mutt_parse_part(fp, b);
  }

  if ((WithCrypto != 0) && e)
  {
    if (e->body == b)
      e->security = SEC_NO_FLAGS;

    e->security |= crypt_query(b);
  }

  return structure_changed | type_changed;
}

/**
 * check_traditional_pgp - Check for an inline PGP content
 * @param m Mailbox
 * @param e Email to check
 * @retval true Message contains inline PGP content
 */
static bool check_traditional_pgp(struct Mailbox *m, struct Email *e)
{
  bool rc = false;

  e->security |= PGP_TRADITIONAL_CHECKED;

  struct Message *msg = mx_msg_open(m, e->msgno);
  if (msg)
  {
    mutt_parse_mime_message(m, e, msg);
    if (crypt_pgp_check_traditional(msg->fp, e->body, false))
    {
      e->security = crypt_query(e->body);
      rc = true;
    }

    e->security |= PGP_TRADITIONAL_CHECKED;
    mx_msg_close(m, &msg);
  }
  return rc;
}

/**
 * mutt_check_traditional_pgp - Check if a message has inline PGP content
 * @param m  Mailbox
 * @param el List of Emails to check
 * @retval true Message contains inline PGP content
 */
bool mutt_check_traditional_pgp(struct Mailbox *m, struct EmailList *el)
{
  bool rc = false;
  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    if (!(en->email->security & PGP_TRADITIONAL_CHECKED))
      rc = check_traditional_pgp(m, en->email) || rc;
  }

  return rc;
}

/**
 * mutt_check_stats - Forcibly update mailbox stats
 */
void mutt_check_stats(struct Mailbox *m)
{
  mutt_mailbox_check(m, MUTT_MAILBOX_CHECK_FORCE | MUTT_MAILBOX_CHECK_FORCE_STATS);
}
