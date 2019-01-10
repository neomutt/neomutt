/**
 * @file
 * Manage where the email is piped to external commands
 *
 * @authors
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-2004,2006 Thomas Roessler <roessler@does-not-exist.org>
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "config/lib.h"
#include "email/lib.h"
#include "conn/conn.h"
#include "mutt.h"
#include "alias.h"
#include "context.h"
#include "copy.h"
#include "curs_lib.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "hdrline.h"
#include "hook.h"
#include "icommands.h"
#include "keymap.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mutt_logging.h"
#include "mutt_menu.h"
#include "mutt_parse.h"
#include "mutt_window.h"
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

/* These Config Variables are only used in commands.c */
unsigned char CryptVerifySig; ///< Config: Verify PGP or SMIME signatures
char *DisplayFilter; ///< Config: External command to pre-process an email before display
bool PipeDecode;  ///< Config: Decode the message when piping it
char *PipeSep;    ///< Config: Separator to add between multiple piped messages
bool PipeSplit;   ///< Config: Run the pipe command on each message separately
bool PrintDecode; ///< Config: Decode message before printing it
bool PrintSplit;  ///< Config: Print multiple messages separately
bool PromptAfter; ///< Config: Pause after running an external pager

static const char *ExtPagerProgress = "all";

/** The folder the user last saved to.  Used by ci_save_message() */
static char LastSaveFolder[PATH_MAX] = "";

/**
 * update_protected_headers - Get the protected header and update the index
 * @param cur Email to update
 */
static void update_protected_headers(struct Email *cur)
{
  struct Envelope *prot_headers = NULL;
  regmatch_t pmatch[1];

  if (!CryptProtectedHeadersRead)
    return;

  /* Grab protected headers to update in the index */
  if (cur->security & SIGN)
  {
    /* Don't update on a bad signature.
     *
     * This is a simplification.  It's possible the headers are in the
     * encrypted part of a nested encrypt/signed.  But properly handling that
     * case would require more complexity in the decryption handlers, which
     * I'm not sure is worth it. */
    if (!(cur->security & GOODSIGN))
      return;

    if (mutt_is_multipart_signed(cur->content) && cur->content->parts)
    {
      prot_headers = cur->content->parts->mime_headers;
    }
    else if ((WithCrypto & APPLICATION_SMIME) && mutt_is_application_smime(cur->content))
    {
      prot_headers = cur->content->mime_headers;
    }
  }
  if (!prot_headers && (cur->security & ENCRYPT))
  {
    if ((WithCrypto & APPLICATION_PGP) &&
        (mutt_is_valid_multipart_pgp_encrypted(cur->content) ||
         mutt_is_malformed_multipart_pgp_encrypted(cur->content)))
    {
      prot_headers = cur->content->mime_headers;
    }
    else if ((WithCrypto & APPLICATION_SMIME) && mutt_is_application_smime(cur->content))
    {
      prot_headers = cur->content->mime_headers;
    }
  }

  /* Update protected headers in the index and header cache. */
  if (prot_headers && prot_headers->subject &&
      mutt_str_strcmp(cur->env->subject, prot_headers->subject))
  {
    if (Context->mailbox->subj_hash && cur->env->real_subj)
      mutt_hash_delete(Context->mailbox->subj_hash, cur->env->real_subj, cur);

    mutt_str_replace(&cur->env->subject, prot_headers->subject);
    FREE(&cur->env->disp_subj);
    if (regexec(ReplyRegex->regex, cur->env->subject, 1, pmatch, 0) == 0)
      cur->env->real_subj = cur->env->subject + pmatch[0].rm_eo;
    else
      cur->env->real_subj = cur->env->subject;

    if (Context->mailbox->subj_hash)
      mutt_hash_insert(Context->mailbox->subj_hash, cur->env->real_subj, cur);

    mx_save_hcache(Context->mailbox, cur);

    /* Also persist back to the message headers if this is set */
    if (CryptProtectedHeadersSave)
    {
      cur->env->changed |= MUTT_ENV_CHANGED_SUBJECT;
      cur->changed = 1;
      Context->mailbox->changed = 1;
    }
  }
}

/**
 * mutt_display_message - Display a message in the pager
 * @param cur Header of current message
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_display_message(struct Email *cur)
{
  char tempfile[PATH_MAX], buf[LONG_STRING];
  int rc = 0;
  bool builtin = false;
  int cmflags = MUTT_CM_DECODE | MUTT_CM_DISPLAY | MUTT_CM_CHARCONV;
  int chflags;
  FILE *fpout = NULL;
  FILE *fpfilterout = NULL;
  pid_t filterpid = -1;
  int res;

  snprintf(buf, sizeof(buf), "%s/%s", TYPE(cur->content), cur->content->subtype);

  mutt_parse_mime_message(Context->mailbox, cur);
  mutt_message_hook(Context->mailbox, cur, MUTT_MESSAGE_HOOK);

  /* see if crypto is needed for this message.  if so, we should exit curses */
  if ((WithCrypto != 0) && cur->security)
  {
    if (cur->security & ENCRYPT)
    {
      if (cur->security & APPLICATION_SMIME)
        crypt_smime_getkeys(cur->env);
      if (!crypt_valid_passphrase(cur->security))
        return 0;

      cmflags |= MUTT_CM_VERIFY;
    }
    else if (cur->security & SIGN)
    {
      /* find out whether or not the verify signature */
      /* L10N: Used for the $crypt_verify_sig prompt */
      if (query_quadoption(CryptVerifySig, _("Verify signature?")) == MUTT_YES)
      {
        cmflags |= MUTT_CM_VERIFY;
      }
    }
  }

  if (cmflags & MUTT_CM_VERIFY || cur->security & ENCRYPT)
  {
    if (cur->security & APPLICATION_PGP)
    {
      if (cur->env->from)
        crypt_pgp_invoke_getkeys(cur->env->from);

      crypt_invoke_message(APPLICATION_PGP);
    }

    if (cur->security & APPLICATION_SMIME)
      crypt_invoke_message(APPLICATION_SMIME);
  }

  mutt_mktemp(tempfile, sizeof(tempfile));
  fpout = mutt_file_fopen(tempfile, "w");
  if (!fpout)
  {
    mutt_error(_("Could not create temporary file"));
    return 0;
  }

  if (DisplayFilter && *DisplayFilter)
  {
    fpfilterout = fpout;
    fpout = NULL;
    filterpid = mutt_create_filter_fd(DisplayFilter, &fpout, NULL, NULL, -1,
                                      fileno(fpfilterout), -1);
    if (filterpid < 0)
    {
      mutt_error(_("Cannot create display filter"));
      mutt_file_fclose(&fpfilterout);
      unlink(tempfile);
      return 0;
    }
  }

  if (!Pager || (mutt_str_strcmp(Pager, "builtin") == 0))
    builtin = true;
  else
  {
    struct HdrFormatInfo hfi;
    hfi.ctx = Context;
    hfi.pager_progress = ExtPagerProgress;
    hfi.email = cur;
    mutt_make_string_info(buf, sizeof(buf), MuttIndexWindow->cols,
                          NONULL(PagerFormat), &hfi, 0);
    fputs(buf, fpout);
    fputs("\n\n", fpout);
  }

  chflags = (Weed ? (CH_WEED | CH_REORDER) : 0) | CH_DECODE | CH_FROM | CH_DISPLAY;
#ifdef USE_NOTMUCH
  if (Context->mailbox->magic == MUTT_NOTMUCH)
    chflags |= CH_VIRTUAL;
#endif
  res = mutt_copy_message_ctx(fpout, Context->mailbox, cur, cmflags, chflags);

  if ((mutt_file_fclose(&fpout) != 0 && errno != EPIPE) || res < 0)
  {
    mutt_error(_("Could not copy message"));
    if (fpfilterout)
    {
      mutt_wait_filter(filterpid);
      mutt_file_fclose(&fpfilterout);
    }
    mutt_file_unlink(tempfile);
    return 0;
  }

  if (fpfilterout && mutt_wait_filter(filterpid) != 0)
    mutt_any_key_to_continue(NULL);

  mutt_file_fclose(&fpfilterout); /* XXX - check result? */

  if (WithCrypto)
  {
    /* update crypto information for this message */
    cur->security &= ~(GOODSIGN | BADSIGN);
    cur->security |= crypt_query(cur->content);

    /* Remove color cache for this message, in case there
       are color patterns for both ~g and ~V */
    cur->pair = 0;

    /* Grab protected headers and update the header and index */
    update_protected_headers(cur);
  }

  if (builtin)
  {
    if ((WithCrypto != 0) && (cur->security & APPLICATION_SMIME) && (cmflags & MUTT_CM_VERIFY))
    {
      if (cur->security & GOODSIGN)
      {
        if (crypt_smime_verify_sender(cur) == 0)
          mutt_message(_("S/MIME signature successfully verified"));
        else
          mutt_error(_("S/MIME certificate owner does not match sender"));
      }
      else if (cur->security & PARTSIGN)
        mutt_message(_("Warning: Part of this message has not been signed"));
      else if (cur->security & SIGN || cur->security & BADSIGN)
        mutt_error(_("S/MIME signature could NOT be verified"));
    }

    if ((WithCrypto != 0) && (cur->security & APPLICATION_PGP) && (cmflags & MUTT_CM_VERIFY))
    {
      if (cur->security & GOODSIGN)
        mutt_message(_("PGP signature successfully verified"));
      else if (cur->security & PARTSIGN)
        mutt_message(_("Warning: Part of this message has not been signed"));
      else if (cur->security & SIGN)
        mutt_message(_("PGP signature could NOT be verified"));
    }

    struct Pager info = { 0 };
    /* Invoke the builtin pager */
    info.email = cur;
    info.ctx = Context;
    rc = mutt_pager(NULL, tempfile, MUTT_PAGER_MESSAGE, &info);
  }
  else
  {
    int r;

    char cmd[HUGE_STRING];
    mutt_endwin();
    snprintf(cmd, sizeof(cmd), "%s %s", NONULL(Pager), tempfile);
    r = mutt_system(cmd);
    if (r == -1)
      mutt_error(_("Error running \"%s\""), cmd);
    unlink(tempfile);
    if (!OptNoCurses)
      keypad(stdscr, true);
    if (r != -1)
      mutt_set_flag(Context->mailbox, cur, MUTT_READ, 1);
    if (r != -1 && PromptAfter)
    {
      mutt_unget_event(mutt_any_key_to_continue(_("Command: ")), 0);
      rc = km_dokey(MENU_PAGER);
    }
    else
      rc = 0;
  }

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

  char prompt[SHORT_STRING];
  char scratch[SHORT_STRING];
  char buf[HUGE_STRING] = { 0 };
  struct Address *addr = NULL;
  char *err = NULL;
  int rc;
  int msg_count = 0;

  struct EmailNode *en = NULL;
  STAILQ_FOREACH(en, el, entries)
  {
    /* RFC5322 mandates a From: header,
     * so warn before bouncing messages without one */
    if (!en->email->env->from)
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

  addr = mutt_addr_parse_list2(addr, buf);
  if (!addr)
  {
    mutt_error(_("Error parsing address"));
    return;
  }

  addr = mutt_expand_aliases(addr);

  if (mutt_addrlist_to_intl(addr, &err) < 0)
  {
    mutt_error(_("Bad IDN: '%s'"), err);
    FREE(&err);
    mutt_addr_free(&addr);
    return;
  }

  buf[0] = '\0';
  mutt_addr_write(buf, sizeof(buf), addr, true);

#define EXTRA_SPACE (15 + 7 + 2)
  snprintf(scratch, sizeof(scratch),
           ngettext("Bounce message to %s", "Bounce messages to %s", msg_count), buf);

  if (mutt_strwidth(prompt) > MuttMessageWindow->cols - EXTRA_SPACE)
  {
    mutt_simple_format(prompt, sizeof(prompt), 0, MuttMessageWindow->cols - EXTRA_SPACE,
                       FMT_LEFT, 0, scratch, sizeof(scratch), 0);
    mutt_str_strcat(prompt, sizeof(prompt), "...?");
  }
  else
    snprintf(prompt, sizeof(prompt), "%s?", scratch);

  if (query_quadoption(Bounce, prompt) != MUTT_YES)
  {
    mutt_addr_free(&addr);
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

    rc = mutt_bounce_message(msg->fp, en->email, addr);
    mx_msg_close(m, &msg);

    if (rc < 0)
      break;
  }

  mutt_addr_free(&addr);
  /* If no error, or background, display message. */
  if ((rc == 0) || (rc == S_BKG))
    mutt_message(ngettext("Message bounced", "Messages bounced", msg_count));
}

/**
 * pipe_set_flags - Generate flags for copy header/message
 * @param[in]  decode  If true decode the message
 * @param[in]  print   If true, mark the message for printing
 * @param[out] cmflags Copy message flags, e.g. MUTT_CM_DECODE
 * @param[out] chflags Copy header flags, e.g. CH_DECODE
 */
static void pipe_set_flags(bool decode, bool print, int *cmflags, int *chflags)
{
  if (decode)
  {
    *cmflags |= MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    *chflags |= CH_DECODE | CH_REORDER;

    if (Weed)
    {
      *chflags |= CH_WEED;
      *cmflags |= MUTT_CM_WEED;
    }
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
  int cmflags = 0;
  int chflags = CH_FROM;

  pipe_set_flags(decode, print, &cmflags, &chflags);

  if ((WithCrypto != 0) && decode && e->security & ENCRYPT)
  {
    if (!crypt_valid_passphrase(e->security))
      return;
    endwin();
  }

  if (decode)
    mutt_parse_mime_message(m, e);

  mutt_copy_message_ctx(fp, m, e, cmflags, chflags);
}

/**
 * pipe_message - Pipe message to a command
 * @param m      Mailbox
 * @param e      Email
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
static int pipe_message(struct Mailbox *m, struct Email *e, char *cmd,
                        bool decode, bool print, bool split, const char *sep)
{
  if (!Context)
    return 1;

  int rc = 0;
  pid_t thepid;
  FILE *fpout = NULL;

  if (e)
  {
    mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

    if ((WithCrypto != 0) && decode)
    {
      mutt_parse_mime_message(m, e);
      if (e->security & ENCRYPT && !crypt_valid_passphrase(e->security))
        return 1;
    }
    mutt_endwin();

    thepid = mutt_create_filter(cmd, &fpout, NULL, NULL);
    if (thepid < 0)
    {
      mutt_perror(_("Can't create filter process"));
      return 1;
    }

    OptKeepQuiet = true;
    pipe_msg(m, e, fpout, decode, print);
    mutt_file_fclose(&fpout);
    rc = mutt_wait_filter(thepid);
    OptKeepQuiet = false;
  }
  else
  {
    /* handle tagged messages */
    if ((WithCrypto != 0) && decode)
    {
      for (int i = 0; i < m->msg_count; i++)
      {
        if (!message_is_tagged(Context, i))
          continue;

        mutt_message_hook(m, m->emails[i], MUTT_MESSAGE_HOOK);
        mutt_parse_mime_message(m, m->emails[i]);
        if (m->emails[i]->security & ENCRYPT &&
            !crypt_valid_passphrase(m->emails[i]->security))
        {
          return 1;
        }
      }
    }

    if (split)
    {
      for (int i = 0; i < m->msg_count; i++)
      {
        if (!message_is_tagged(Context, i))
          continue;

        mutt_message_hook(m, m->emails[i], MUTT_MESSAGE_HOOK);
        mutt_endwin();
        thepid = mutt_create_filter(cmd, &fpout, NULL, NULL);
        if (thepid < 0)
        {
          mutt_perror(_("Can't create filter process"));
          return 1;
        }
        OptKeepQuiet = true;
        pipe_msg(m, m->emails[i], fpout, decode, print);
        /* add the message separator */
        if (sep)
          fputs(sep, fpout);
        mutt_file_fclose(&fpout);
        if (mutt_wait_filter(thepid) != 0)
          rc = 1;
        OptKeepQuiet = false;
      }
    }
    else
    {
      mutt_endwin();
      thepid = mutt_create_filter(cmd, &fpout, NULL, NULL);
      if (thepid < 0)
      {
        mutt_perror(_("Can't create filter process"));
        return 1;
      }
      OptKeepQuiet = true;
      for (int i = 0; i < m->msg_count; i++)
      {
        if (!message_is_tagged(Context, i))
          continue;

        mutt_message_hook(m, m->emails[i], MUTT_MESSAGE_HOOK);
        pipe_msg(m, m->emails[i], fpout, decode, print);
        /* add the message separator */
        if (sep)
          fputs(sep, fpout);
      }
      mutt_file_fclose(&fpout);
      if (mutt_wait_filter(thepid) != 0)
        rc = 1;
      OptKeepQuiet = false;
    }
  }

  if (rc || WaitKey)
    mutt_any_key_to_continue(NULL);
  return rc;
}

/**
 * mutt_pipe_message - Pipe a message
 * @param m Mailbox
 * @param e Email to pipe
 */
void mutt_pipe_message(struct Mailbox *m, struct Email *e)
{
  char buffer[LONG_STRING];

  buffer[0] = '\0';
  if (mutt_get_field(_("Pipe to command: "), buffer, sizeof(buffer), MUTT_CMD) != 0 ||
      !buffer[0])
  {
    return;
  }

  mutt_expand_path(buffer, sizeof(buffer));
  pipe_message(m, e, buffer, PipeDecode, false, PipeSplit, PipeSep);
}

/**
 * mutt_print_message - Print a message
 * @param m Mailbox
 * @param e Email to print
 */
void mutt_print_message(struct Mailbox *m, struct Email *e)
{
  int msgcount; // for L10N with ngettext
  if (e)
    msgcount = 1;
  else if (Context)
  {
    msgcount = 0; // count the precise number of messages.
    for (int i = 0; i < m->msg_count; i++)
      if (message_is_tagged(Context, i))
        msgcount++;
  }
  else
    msgcount = 0;

  if (Print && (!PrintCommand || !*PrintCommand))
  {
    mutt_message(_("No printing command has been defined"));
    return;
  }

  if (query_quadoption(Print, e ? _("Print message?") : _("Print tagged messages?")) != MUTT_YES)
  {
    return;
  }

  if (pipe_message(m, e, PrintCommand, PrintDecode, true, PrintSplit, "\f") == 0)
    mutt_message(ngettext("Message printed", "Messages printed", msgcount));
  else
  {
    mutt_message(ngettext("Message could not be printed",
                          "Messages could not be printed", msgcount));
  }
}

/**
 * mutt_select_sort - Ask the user for a sort method
 * @param reverse If true make it a reverse sort
 * @retval num Sort type, e.g. #SORT_DATE
 */
int mutt_select_sort(int reverse)
{
  int method = Sort; /* save the current method in case of abort */
  int new_sort = -1;

  switch (mutt_multi_choice(reverse ?
                                /* L10N: The highlighted letters must match the "Sort" options */
                                _("Rev-Sort "
                                  "(d)ate/(f)rm/(r)ecv/(s)ubj/t(o)/(t)hread/"
                                  "(u)nsort/si(z)e/s(c)ore/s(p)am/(l)abel?: ") :
                                /* L10N: The highlighted letters must match the "Rev-Sort" options */
                                _("Sort "
                                  "(d)ate/(f)rm/(r)ecv/(s)ubj/t(o)/(t)hread/"
                                  "(u)nsort/si(z)e/s(c)ore/s(p)am/(l)abel?: "),
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

  cs_str_native_set(Config, "sort", new_sort, NULL);
  return (Sort != method) ? 0 : -1; /* no need to resort if it's the same */
}

/**
 * mutt_shell_escape - invoke a command in a subshell
 */
void mutt_shell_escape(void)
{
  char buf[LONG_STRING];

  buf[0] = '\0';
  if (mutt_get_field(_("Shell command: "), buf, sizeof(buf), MUTT_CMD) != 0)
    return;

  if (!buf[0] && Shell)
    mutt_str_strfcpy(buf, Shell, sizeof(buf));
  if (!buf[0])
    return;

  mutt_window_clearline(MuttMessageWindow, 0);
  mutt_endwin();
  fflush(stdout);
  int rc = mutt_system(buf);
  if (rc == -1)
    mutt_debug(1, "Error running \"%s\"!", buf);

  if ((rc != 0) || WaitKey)
    mutt_any_key_to_continue(NULL);
  mutt_mailbox_check(MUTT_MAILBOX_CHECK_FORCE);
}

/**
 * mutt_enter_command - enter a neomutt command
 */
void mutt_enter_command(void)
{
  char buffer[LONG_STRING] = { 0 };

  /* if enter is pressed after : with no command, just return */
  if (mutt_get_field(":", buffer, sizeof(buffer), MUTT_COMMAND) != 0 || !buffer[0])
    return;

  struct Buffer *err = mutt_buffer_alloc(STRING);
  struct Buffer *token = mutt_buffer_alloc(STRING);

  /* check if buffer is a valid icommand, else fall back quietly to parse_rc_lines */
  enum CommandResult rc = mutt_parse_icommand(buffer, err);
  if (!mutt_buffer_is_empty(err))
  {
    /* since errbuf could potentially contain printf() sequences in it,
     * we must call mutt_error() in this fashion so that vsprintf()
     * doesn't expect more arguments that we passed */
    if (rc == MUTT_CMD_ERROR)
      mutt_error("%s", err->data);
    else
      mutt_warning("%s", err->data);
  }
  else if (rc != MUTT_CMD_SUCCESS)
  {
    rc = mutt_parse_rc_line(buffer, token, err);
    if (!mutt_buffer_is_empty(err))
    {
      if (rc == MUTT_CMD_SUCCESS) /* command succeeded with message */
        mutt_message("%s", err->data);
      else /* error executing command */
        mutt_error("%s", err->data);
    }
  }
  /* else successful command */

  mutt_buffer_free(&token);
  mutt_buffer_free(&err);
}

/**
 * mutt_display_address - Display the address of a message
 * @param env Envelope containing address
 */
void mutt_display_address(struct Envelope *env)
{
  const char *pfx = NULL;
  char buf[SHORT_STRING];

  struct Address *addr = mutt_get_address(env, &pfx);
  if (!addr)
    return;

  /* Note: We don't convert IDNA to local representation this time.
   * That is intentional, so the user has an opportunity to copy &
   * paste the on-the-wire form of the address to other, IDN-unable
   * software.
   */

  buf[0] = '\0';
  mutt_addr_write(buf, sizeof(buf), addr, false);
  mutt_message("%s: %s", pfx, buf);
}

/**
 * set_copy_flags - Set the flags for a message copy
 * @param[in]  e       Email
 * @param[in]  decode  If true, decode the message
 * @param[in]  decrypt If true, decrypt the message
 * @param[out] cmflags Copy message flags, e.g. MUTT_CM_DECODE
 * @param[out] chflags Copy header flags, e.g. CH_DECODE
 */
static void set_copy_flags(struct Email *e, bool decode, bool decrypt,
                           int *cmflags, int *chflags)
{
  *cmflags = 0;
  *chflags = CH_UPDATE_LEN;

  if ((WithCrypto != 0) && !decode && decrypt && (e->security & ENCRYPT))
  {
    if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_multipart_encrypted(e->content))
    {
      *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
      *cmflags = MUTT_CM_DECODE_PGP;
    }
    else if (((WithCrypto & APPLICATION_PGP) != 0) &&
             mutt_is_application_pgp(e->content) & ENCRYPT)
    {
      decode = 1;
    }
    else if (((WithCrypto & APPLICATION_SMIME) != 0) &&
             mutt_is_application_smime(e->content) & ENCRYPT)
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

      if (Weed)
      {
        *chflags |= CH_WEED; /* and respect $weed. */
        *cmflags |= MUTT_CM_WEED;
      }
    }
  }
}

/**
 * mutt_save_message_ctx - Save a message to a given mailbox
 * @param e       Email
 * @param delete  If true, delete the original
 * @param decode  If true, decode the message
 * @param decrypt If true, decrypt the message
 * @param m       Mailbox to save to
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_save_message_ctx(struct Email *e, bool delete, bool decode,
                          bool decrypt, struct Mailbox *m)
{
  int cmflags, chflags;
  int rc;

  set_copy_flags(e, decode, decrypt, &cmflags, &chflags);

  if (decode || decrypt)
    mutt_parse_mime_message(Context->mailbox, e);

  rc = mutt_append_message(m, Context->mailbox, e, cmflags, chflags);
  if (rc != 0)
    return rc;

  if (delete)
  {
    mutt_set_flag(Context->mailbox, e, MUTT_DELETE, 1);
    mutt_set_flag(Context->mailbox, e, MUTT_PURGE, 1);
    if (DeleteUntag)
      mutt_set_flag(Context->mailbox, e, MUTT_TAG, 0);
  }

  return 0;
}

/**
 * mutt_save_message - Save an email
 * @param e       Email
 * @param delete  If true, delete the original (save)
 * @param decode  If true, decode the message
 * @param decrypt If true, decrypt the message
 * @retval  0 Copy/save was successful
 * @retval -1 Error/abort
 */
int mutt_save_message(struct Email *e, bool delete, bool decode, bool decrypt)
{
  bool need_passphrase = false;
  int app = 0;
  char buf[PATH_MAX];
  const char *prompt = NULL;
  struct Context *savectx = NULL;
  struct stat st;

  if (delete)
  {
    if (decode)
      prompt = e ? _("Decode-save to mailbox") : _("Decode-save tagged to mailbox");
    else if (decrypt)
      prompt = e ? _("Decrypt-save to mailbox") : _("Decrypt-save tagged to mailbox");
    else
      prompt = e ? _("Save to mailbox") : _("Save tagged to mailbox");
  }
  else
  {
    if (decode)
      prompt = e ? _("Decode-copy to mailbox") : _("Decode-copy tagged to mailbox");
    else if (decrypt)
      prompt = e ? _("Decrypt-copy to mailbox") : _("Decrypt-copy tagged to mailbox");
    else
      prompt = e ? _("Copy to mailbox") : _("Copy tagged to mailbox");
  }

  if (e)
  {
    if (WithCrypto)
    {
      need_passphrase = (e->security & ENCRYPT);
      app = e->security;
    }
    mutt_message_hook(Context->mailbox, e, MUTT_MESSAGE_HOOK);
    mutt_default_save(buf, sizeof(buf), e);
  }
  else
  {
    /* look for the first tagged message */
    for (int i = 0; i < Context->mailbox->msg_count; i++)
    {
      if (message_is_tagged(Context, i))
      {
        e = Context->mailbox->emails[i];
        break;
      }
    }

    if (e)
    {
      mutt_message_hook(Context->mailbox, e, MUTT_MESSAGE_HOOK);
      mutt_default_save(buf, sizeof(buf), e);
      if (WithCrypto)
      {
        need_passphrase = (e->security & ENCRYPT);
        app = e->security;
      }
      e = NULL;
    }
  }

  mutt_pretty_mailbox(buf, sizeof(buf));
  if (mutt_enter_fname(prompt, buf, sizeof(buf), 0) == -1)
    return -1;

  size_t pathlen = strlen(buf);
  if (pathlen == 0)
    return -1;

  /* Trim any trailing '/' */
  if (buf[pathlen - 1] == '/')
    buf[pathlen - 1] = '\0';

  /* This is an undocumented feature of ELM pointed out to me by Felix von
   * Leitner <leitner@prz.fu-berlin.de>
   */
  if (mutt_str_strcmp(buf, ".") == 0)
    mutt_str_strfcpy(buf, LastSaveFolder, sizeof(buf));
  else
    mutt_str_strfcpy(LastSaveFolder, buf, sizeof(LastSaveFolder));

  mutt_expand_path(buf, sizeof(buf));

  /* check to make sure that this file is really the one the user wants */
  if (mutt_save_confirm(buf, &st) != 0)
    return -1;

  if ((WithCrypto != 0) && need_passphrase && (decode || decrypt) &&
      !crypt_valid_passphrase(app))
  {
    return -1;
  }

  mutt_message(_("Copying to %s..."), buf);

#ifdef USE_IMAP
  if ((Context->mailbox->magic == MUTT_IMAP) && !(decode || decrypt) &&
      (imap_path_probe(buf, NULL) == MUTT_IMAP))
  {
    switch (imap_copy_messages(Context->mailbox, e, buf, delete))
    {
      /* success */
      case 0:
        mutt_clear_error();
        return 0;
      /* non-fatal error: continue to fetch/append */
      case 1:
        break;
      /* fatal error, abort */
      case -1:
        return -1;
    }
  }
#endif

  savectx = mx_mbox_open(NULL, buf, MUTT_APPEND);
  if (savectx)
  {
#ifdef USE_COMPRESSED
    /* If we're saving to a compressed mailbox, the stats won't be updated
     * until the next open.  Until then, improvise. */
    struct Mailbox *cm = NULL;
    if (savectx->mailbox->compress_info)
    {
      cm = mutt_find_mailbox(savectx->mailbox->realpath);
    }
    /* We probably haven't been opened yet */
    if (cm && (cm->msg_count == 0))
      cm = NULL;
#endif
    if (e)
    {
      if (mutt_save_message_ctx(e, delete, decode, decrypt, savectx->mailbox) != 0)
      {
        mx_mbox_close(&savectx);
        return -1;
      }
#ifdef USE_COMPRESSED
      if (cm)
      {
        cm->msg_count++;
        if (!e->read)
        {
          cm->msg_unread++;
          if (!e->old)
            cm->msg_new++;
        }
        if (e->flagged)
          cm->msg_flagged++;
      }
#endif
    }
    else
    {
      int rc = 0;

#ifdef USE_NOTMUCH
      if (Context->mailbox->magic == MUTT_NOTMUCH)
        nm_db_longrun_init(Context->mailbox, true);
#endif
      for (int i = 0; i < Context->mailbox->msg_count; i++)
      {
        if (!message_is_tagged(Context, i))
          continue;

        mutt_message_hook(Context->mailbox, Context->mailbox->emails[i], MUTT_MESSAGE_HOOK);
        rc = mutt_save_message_ctx(Context->mailbox->emails[i], delete, decode,
                                   decrypt, savectx->mailbox);
        if (rc != 0)
          break;
#ifdef USE_COMPRESSED
        if (cm)
        {
          struct Email *e2 = Context->mailbox->emails[i];
          cm->msg_count++;
          if (!e2->read)
          {
            cm->msg_unread++;
            if (!e2->old)
              cm->msg_new++;
          }
          if (e2->flagged)
            cm->msg_flagged++;
        }
#endif
      }
#ifdef USE_NOTMUCH
      if (Context->mailbox->magic == MUTT_NOTMUCH)
        nm_db_longrun_done(Context->mailbox);
#endif
      if (rc != 0)
      {
        mx_mbox_close(&savectx);
        return -1;
      }
    }

    const bool need_mailbox_cleanup = ((savectx->mailbox->magic == MUTT_MBOX) ||
                                       (savectx->mailbox->magic == MUTT_MMDF));

    mx_mbox_close(&savectx);

    if (need_mailbox_cleanup)
      mutt_mailbox_cleanup(buf, &st);

    mutt_clear_error();
    return 0;
  }

  return -1;
}

/**
 * mutt_edit_content_type - Edit the content type of an attachment
 * @param e  Email
 * @param b  Attachment
 * @param fp File handle to the attachment
 * @retval 1 A structural change is made
 * @retval 0 Otherwise
 *
 * recvattach requires the return code to know when to regenerate the actx.
 */
int mutt_edit_content_type(struct Email *e, struct Body *b, FILE *fp)
{
  char buf[LONG_STRING];
  char obuf[LONG_STRING];
  char tmp[STRING];
  char charset[STRING];

  short charset_changed = 0;
  short type_changed = 0;
  short structure_changed = 0;

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
    }
  }

  if (mutt_get_field("Content-Type: ", buf, sizeof(buf), 0) != 0 || buf[0] == 0)
    return 0;

  /* clean up previous junk */
  mutt_param_free(&b->parameter);
  FREE(&b->subtype);

  mutt_parse_content_type(buf, b);

  snprintf(tmp, sizeof(tmp), "%s/%s", TYPE(b), NONULL(b->subtype));
  type_changed = mutt_str_strcasecmp(tmp, obuf);
  charset_changed =
      mutt_str_strcasecmp(charset, mutt_param_get(&b->parameter, "charset"));

  /* if in send mode, check for conversion - current setting is default. */

  if (!e && b->type == TYPE_TEXT && charset_changed)
  {
    int r;
    snprintf(tmp, sizeof(tmp), _("Convert to %s upon sending?"),
             mutt_param_get(&b->parameter, "charset"));
    r = mutt_yesorno(tmp, !b->noconv);
    if (r != MUTT_ABORT)
      b->noconv = (r == MUTT_NO);
  }

  /* inform the user */

  snprintf(tmp, sizeof(tmp), "%s/%s", TYPE(b), NONULL(b->subtype));
  if (type_changed)
    mutt_message(_("Content-Type changed to %s"), tmp);
  if (b->type == TYPE_TEXT && charset_changed)
  {
    if (type_changed)
      mutt_sleep(1);
    mutt_message(b->noconv ? _("Character set changed to %s; not converting") :
                             _("Character set changed to %s; converting"),
                 mutt_param_get(&b->parameter, "charset"));
  }

  b->force_charset |= charset_changed ? 1 : 0;

  if (!is_multipart(b) && b->parts)
  {
    structure_changed = 1;
    mutt_body_free(&b->parts);
  }
  if (!mutt_is_message_type(b->type, b->subtype) && b->email)
  {
    structure_changed = 1;
    b->email->content = NULL;
    mutt_email_free(&b->email);
  }

  if (fp && !b->parts && (is_multipart(b) || mutt_is_message_type(b->type, b->subtype)))
  {
    structure_changed = 1;
    mutt_parse_part(fp, b);
  }

  if ((WithCrypto != 0) && e)
  {
    if (e->content == b)
      e->security = 0;

    e->security |= crypt_query(b);
  }

  return structure_changed;
}

/**
 * check_traditional_pgp - Check for an inline PGP content
 * @param[in]  e      Header of message to check
 * @param[out] redraw Set of #REDRAW_FULL if the screen may need redrawing
 * @retval true If message contains inline PGP content
 */
static bool check_traditional_pgp(struct Email *e, int *redraw)
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
 * @param[out] redraw Set of #REDRAW_FULL if the screen may need redrawing
 * @retval true If message contains inline PGP content
 */
bool mutt_check_traditional_pgp(struct EmailList *el, int *redraw)
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
  mutt_mailbox_check(MUTT_MAILBOX_CHECK_FORCE | MUTT_MAILBOX_CHECK_FORCE_STATS);
}
