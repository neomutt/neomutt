/**
 * @file
 * Process a message for display in the pager
 *
 * @authors
 * Copyright (C) 2021 Richard Russon <rich@flatcap.org>
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
 * @page pager_message Process a message for display in the pager
 *
 * Process a message for display in the pager
 */

#include "config.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "attach/lib.h"
#include "index/lib.h"
#include "menu/lib.h"
#include "ncrypt/lib.h"
#include "pager/lib.h"
#include "question/lib.h"
#include "copy.h"
#include "format_flags.h"
#include "globals.h" // IWYU pragma: keep
#include "hdrline.h"
#include "hook.h"
#include "keymap.h"
#include "muttlib.h"
#include "mx.h"
#include "protos.h"
#ifdef USE_AUTOCRYPT
#include "autocrypt/lib.h"
#endif

static const char *ExtPagerProgress = N_("all");

/**
 * process_protected_headers - Get the protected header and update the index
 * @param m Mailbox
 * @param e Email to update
 */
static void process_protected_headers(struct Mailbox *m, struct Email *e)
{
  struct Envelope *prot_headers = NULL;
  regmatch_t pmatch[1];

  const bool c_crypt_protected_headers_read = cs_subset_bool(NeoMutt->sub, "crypt_protected_headers_read");
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
    const struct Regex *c_reply_regex = cs_subset_regex(NeoMutt->sub, "reply_regex");
    if (mutt_regex_capture(c_reply_regex, e->env->subject, 1, pmatch))
    {
      e->env->real_subj = e->env->subject + pmatch[0].rm_eo;
      if (e->env->real_subj[0] == '\0')
        e->env->real_subj = NULL;
    }
    else
    {
      e->env->real_subj = e->env->subject;
    }

    if (m->subj_hash)
      mutt_hash_insert(m->subj_hash, e->env->real_subj, e);

    mx_save_hcache(m, e);

    /* Also persist back to the message headers if this is set */
    const bool c_crypt_protected_headers_save = cs_subset_bool(NeoMutt->sub, "crypt_protected_headers_save");
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
    mutt_autocrypt_process_gossip_header(e, prot_headers);
  }
#endif
}

/**
 * email_to_file - Decrypt, decode and weed an Email into a file
 * @param msg      Raw Email
 * @param tempfile Temporary filename for result
 * @param m        Mailbox
 * @param e        Email to display
 * @param header   Header to prefix output (OPTIONAL)
 * @param wrap_len Width to wrap lines
 * @param cmflags  Message flags, e.g. #MUTT_CM_DECODE
 * @retval  0 Success
 * @retval -1 Error
 *
 * @note Flags may be added to @a cmflags
 */
static int email_to_file(struct Message *msg, struct Buffer *tempfile,
                         struct Mailbox *m, struct Email *e, const char *header,
                         int wrap_len, CopyMessageFlags *cmflags)
{
  int rc = 0;
  pid_t filterpid = -1;

  mutt_parse_mime_message(e, msg->fp);
  mutt_message_hook(m, e, MUTT_MESSAGE_HOOK);

  char columns[16] = { 0 };
  // win_pager might not be visible and have a size yet, so use win_index
  snprintf(columns, sizeof(columns), "%d", wrap_len);
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

      *cmflags |= MUTT_CM_VERIFY;
    }
    else if (e->security & SEC_SIGN)
    {
      /* find out whether or not the verify signature */
      /* L10N: Used for the $crypt_verify_sig prompt */
      const enum QuadOption c_crypt_verify_sig = cs_subset_quad(NeoMutt->sub, "crypt_verify_sig");
      if (query_quadoption(c_crypt_verify_sig, _("Verify signature?")) == MUTT_YES)
      {
        *cmflags |= MUTT_CM_VERIFY;
      }
    }
  }

  if (*cmflags & MUTT_CM_VERIFY || e->security & SEC_ENCRYPT)
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
  mutt_buffer_mktemp(tempfile);
  FILE *fp_out = mutt_file_fopen(mutt_buffer_string(tempfile), "w");
  if (!fp_out)
  {
    mutt_error(_("Could not create temporary file"));
    goto cleanup;
  }

  const char *const c_display_filter = cs_subset_string(NeoMutt->sub, "display_filter");
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

  if (header)
  {
    fputs(header, fp_out);
    fputs("\n\n", fp_out);
  }

  const bool c_weed = cs_subset_bool(NeoMutt->sub, "weed");
  CopyHeaderFlags chflags = (c_weed ? (CH_WEED | CH_REORDER) : CH_NO_FLAGS) |
                            CH_DECODE | CH_FROM | CH_DISPLAY;
#ifdef USE_NOTMUCH
  if (m->type == MUTT_NOTMUCH)
    chflags |= CH_VIRTUAL;
#endif
  rc = mutt_copy_message(fp_out, e, msg, *cmflags, chflags, wrap_len);

  if (((mutt_file_fclose(&fp_out) != 0) && (errno != EPIPE)) || (rc < 0))
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
    e->attr_color = NULL;

    /* Process protected headers and autocrypt gossip headers */
    process_protected_headers(m, e);
  }

cleanup:
  mutt_envlist_unset("COLUMNS");
  return rc;
}

/**
 * external_pager - Display a message in an external program
 * @param m       Mailbox
 * @param e       Email to display
 * @param command External command to run
 * @retval  0 Success
 * @retval -1 Error
 */
int external_pager(struct Mailbox *m, struct Email *e, const char *command)
{
  struct Message *msg = mx_msg_open(m, e->msgno);
  if (!msg)
    return -1;

  char buf[1024] = { 0 };
  const char *const c_pager_format = cs_subset_string(NeoMutt->sub, "pager_format");
  const int screen_width = RootWindow->state.cols;
  mutt_make_string(buf, sizeof(buf), screen_width, NONULL(c_pager_format), m,
                   -1, e, MUTT_FORMAT_NO_FLAGS, _(ExtPagerProgress));

  struct Buffer *tempfile = mutt_buffer_pool_get();

  CopyMessageFlags cmflags = MUTT_CM_DECODE | MUTT_CM_DISPLAY | MUTT_CM_CHARCONV;
  int rc = email_to_file(msg, tempfile, m, e, buf, screen_width, &cmflags);
  if (rc < 0)
    goto cleanup;

  mutt_endwin();

  struct Buffer *cmd = mutt_buffer_pool_get();
  mutt_buffer_printf(cmd, "%s %s", command, mutt_buffer_string(tempfile));
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
    mutt_unget_ch(mutt_any_key_to_continue(_("Command: ")));
    rc = km_dokey(MENU_PAGER);
  }
  else
    rc = 0;

cleanup:
  mx_msg_close(m, &msg);
  mutt_buffer_pool_release(&tempfile);
  return rc;
}

/**
 * notify_crypto - Notify the user about the crypto status of the Email
 * @param e       Email to display
 * @param msg     Raw Email
 * @param cmflags Message flags, e.g. #MUTT_CM_DECODE
 */
static void notify_crypto(struct Email *e, struct Message *msg, CopyMessageFlags cmflags)
{
  if ((WithCrypto != 0) && (e->security & APPLICATION_SMIME) && (cmflags & MUTT_CM_VERIFY))
  {
    if (e->security & SEC_GOODSIGN)
    {
      if (crypt_smime_verify_sender(e, msg) == 0)
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
}

/**
 * squash_index_panel - Shrink or hide the Index Panel
 * @param m      Mailbox
 * @param win_index Index Window
 * @param win_pager Pager Window
 */
static void squash_index_panel(struct Mailbox *m, struct MuttWindow *win_index,
                               struct MuttWindow *win_pager)
{
  const short c_pager_index_lines = cs_subset_number(NeoMutt->sub, "pager_index_lines");

  const int index_space = MIN(c_pager_index_lines, m->vcount);
  if (index_space > 0)
  {
    win_index->size = MUTT_WIN_SIZE_FIXED;
    win_index->req_rows = index_space;
    win_index->parent->size = MUTT_WIN_SIZE_MINIMISE;
  }
  window_set_visible(win_index->parent, (index_space > 0));

  window_set_visible(win_pager->parent, true);

  struct MuttWindow *dlg = dialog_find(win_index);
  mutt_window_reflow(dlg);

  // Force the menu to reframe itself
  struct Menu *menu = win_index->wdata;
  menu_set_index(menu, menu_get_index(menu));
}

/**
 * expand_index_panel - Restore the Index Panel
 * @param win_index Index Window
 * @param win_pager Pager Window
 */
static void expand_index_panel(struct MuttWindow *win_index, struct MuttWindow *win_pager)
{
  win_index->size = MUTT_WIN_SIZE_MAXIMISE;
  win_index->req_rows = MUTT_WIN_SIZE_UNLIMITED;
  win_index->parent->size = MUTT_WIN_SIZE_MAXIMISE;
  win_index->parent->req_rows = MUTT_WIN_SIZE_UNLIMITED;
  window_set_visible(win_index->parent, true);

  window_set_visible(win_pager->parent, false);

  struct MuttWindow *dlg = dialog_find(win_index);
  mutt_window_reflow(dlg);
}

/**
 * mutt_display_message - Display a message in the pager
 * @param win_index Index Window
 * @param shared    Shared Index data
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_display_message(struct MuttWindow *win_index, struct IndexSharedData *shared)
{
  struct MuttWindow *dlg = dialog_find(win_index);
  struct MuttWindow *win_pager = window_find_child(dlg, WT_CUSTOM);
  struct MuttWindow *win_pbar = window_find_child(dlg, WT_STATUS_BAR);
  struct Buffer *tempfile = mutt_buffer_pool_get();
  struct Message *msg = NULL;

  squash_index_panel(shared->mailbox, win_index, win_pager);

  int rc = PAGER_LOOP_QUIT;
  do
  {
    msg = mx_msg_open(shared->mailbox, shared->email->msgno);
    if (!msg)
      break;

    CopyMessageFlags cmflags = MUTT_CM_DECODE | MUTT_CM_DISPLAY | MUTT_CM_CHARCONV;

    mutt_buffer_reset(tempfile);
    // win_pager might not be visible and have a size yet, so use win_index
    rc = email_to_file(msg, tempfile, shared->mailbox, shared->email, NULL,
                       win_index->state.cols, &cmflags);
    if (rc < 0)
      break;

    notify_crypto(shared->email, msg, cmflags);

    /* Invoke the builtin pager */
    struct PagerData pdata = { 0 };
    struct PagerView pview = { &pdata };

    pdata.fp = msg->fp;
    pdata.fname = mutt_buffer_string(tempfile);

    pview.mode = PAGER_MODE_EMAIL;
    pview.banner = NULL;
    pview.flags = MUTT_PAGER_MESSAGE |
                  (shared->email->body->nowrap ? MUTT_PAGER_NOWRAP : 0);
    pview.win_index = win_index;
    pview.win_pbar = win_pbar;
    pview.win_pager = win_pager;

    rc = mutt_pager(&pview);
    mx_msg_close(shared->mailbox, &msg);
  } while (rc == PAGER_LOOP_RELOAD);

  expand_index_panel(win_index, win_pager);

  mx_msg_close(shared->mailbox, &msg);
  mutt_buffer_pool_release(&tempfile);
  return rc;
}
