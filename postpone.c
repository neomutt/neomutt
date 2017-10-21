/**
 * @file
 * Save/restore and GUI list postponed emails
 *
 * @authors
 * Copyright (C) 1996-2002,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002,2004 Thomas Roessler <roessler@does-not-exist.org>
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "lib/lib.h"
#include "mutt.h"
#include "body.h"
#include "context.h"
#include "envelope.h"
#include "format_flags.h"
#include "globals.h"
#include "header.h"
#include "keymap.h"
#include "mailbox.h"
#include "mime.h"
#include "mutt_menu.h"
#include "ncrypt/ncrypt.h"
#include "opcodes.h"
#include "options.h"
#include "parameter.h"
#include "protos.h"
#include "sort.h"
#include "state.h"
#include "thread.h"
#ifdef USE_IMAP
#include "imap/imap.h"
#endif

static const struct Mapping PostponeHelp[] = {
  { N_("Exit"), OP_EXIT },
  { N_("Del"), OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Help"), OP_HELP },
  { NULL, 0 },
};

static short PostCount = 0;
static struct Context *PostContext = NULL;
static short UpdateNumPostponed = 0;

/**
 * mutt_num_postponed - Return the number of postponed messages
 * @param force
 * * 0 Use a cached value if costly to get a fresh count (IMAP)
 * * 1 Force check
 * @retval n Number of postponed messages
 */
int mutt_num_postponed(int force)
{
  struct stat st;
  struct Context ctx;

  static time_t LastModify = 0;
  static char *OldPostponed = NULL;

  if (UpdateNumPostponed)
  {
    UpdateNumPostponed = 0;
    force = 1;
  }

  if (mutt_strcmp(Postponed, OldPostponed) != 0)
  {
    FREE(&OldPostponed);
    OldPostponed = safe_strdup(Postponed);
    LastModify = 0;
    force = 1;
  }

  if (!Postponed)
    return 0;

#ifdef USE_IMAP
  /* LastModify is useless for IMAP */
  if (mx_is_imap(Postponed))
  {
    if (force)
    {
      short newpc;

      newpc = imap_status(Postponed, 0);
      if (newpc >= 0)
      {
        PostCount = newpc;
        mutt_debug(3, "mutt_num_postponed: %d postponed IMAP messages found.\n", PostCount);
      }
      else
        mutt_debug(3, "mutt_num_postponed: using old IMAP postponed count.\n");
    }
    return PostCount;
  }
#endif

  if (stat(Postponed, &st) == -1)
  {
    PostCount = 0;
    LastModify = 0;
    return 0;
  }

  if (S_ISDIR(st.st_mode))
  {
    /* if we have a maildir mailbox, we need to stat the "new" dir */

    char buf[_POSIX_PATH_MAX];

    snprintf(buf, sizeof(buf), "%s/new", Postponed);
    if (access(buf, F_OK) == 0 && stat(buf, &st) == -1)
    {
      PostCount = 0;
      LastModify = 0;
      return 0;
    }
  }

  if (LastModify < st.st_mtime)
  {
#ifdef USE_NNTP
    int optnews = option(OPT_NEWS);
#endif
    LastModify = st.st_mtime;

    if (access(Postponed, R_OK | F_OK) != 0)
      return (PostCount = 0);
#ifdef USE_NNTP
    if (optnews)
      unset_option(OPT_NEWS);
#endif
    if (mx_open_mailbox(Postponed, MUTT_NOSORT | MUTT_QUIET, &ctx) == NULL)
      PostCount = 0;
    else
      PostCount = ctx.msgcount;
    mx_fastclose_mailbox(&ctx);
#ifdef USE_NNTP
    if (optnews)
      set_option(OPT_NEWS);
#endif
  }

  return PostCount;
}

void mutt_update_num_postponed(void)
{
  UpdateNumPostponed = 1;
}

static void post_entry(char *s, size_t slen, struct Menu *menu, int entry)
{
  struct Context *ctx = (struct Context *) menu->data;

  _mutt_make_string(s, slen, NONULL(IndexFormat), ctx, ctx->hdrs[entry], MUTT_FORMAT_ARROWCURSOR);
}

static struct Header *select_msg(void)
{
  struct Menu *menu = NULL;
  int i, r = -1;
  bool done = false;
  char helpstr[LONG_STRING];
  short orig_sort;

  menu = mutt_new_menu(MENU_POST);
  menu->make_entry = post_entry;
  menu->max = PostContext->msgcount;
  menu->title = _("Postponed Messages");
  menu->data = PostContext;
  menu->help = mutt_compile_help(helpstr, sizeof(helpstr), MENU_POST, PostponeHelp);
  mutt_push_current_menu(menu);

  /* The postponed mailbox is setup to have sorting disabled, but the global
   * Sort variable may indicate something different.   Sorting has to be
   * disabled while the postpone menu is being displayed. */
  orig_sort = Sort;
  Sort = SORT_ORDER;

  while (!done)
  {
    switch (i = mutt_menu_loop(menu))
    {
      case OP_DELETE:
      case OP_UNDELETE:
        /* should deleted draft messages be saved in the trash folder? */
        mutt_set_flag(PostContext, PostContext->hdrs[menu->current],
                      MUTT_DELETE, (i == OP_DELETE) ? 1 : 0);
        PostCount = PostContext->msgcount - PostContext->deleted;
        if (option(OPT_RESOLVE) && menu->current < menu->max - 1)
        {
          menu->oldcurrent = menu->current;
          menu->current++;
          if (menu->current >= menu->top + menu->pagelen)
          {
            menu->top = menu->current;
            menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
          }
          else
            menu->redraw |= REDRAW_MOTION_RESYNCH;
        }
        else
          menu->redraw |= REDRAW_CURRENT;
        break;

      case OP_GENERIC_SELECT_ENTRY:
        r = menu->current;
        done = true;
        break;

      case OP_EXIT:
        done = true;
        break;
    }
  }

  Sort = orig_sort;
  mutt_pop_current_menu(menu);
  mutt_menu_destroy(&menu);
  return (r > -1 ? PostContext->hdrs[r] : NULL);
}

/**
 * mutt_get_postponed - Recall a postponed message
 * @param ctx     Context info, used when recalling a message to which we reply
 * @param hdr     envelope/attachment info for recalled message
 * @param cur     if message was a reply, `cur' is set to the message
 *                which `hdr' is in reply to
 * @param fcc     fcc for the recalled message
 * @param fcclen  max length of fcc
 * @retval -1         Error/no messages
 * @retval 0          Normal exit
 * @retval #SENDREPLY Recalled message is a reply
 */
int mutt_get_postponed(struct Context *ctx, struct Header *hdr,
                       struct Header **cur, char *fcc, size_t fcclen)
{
  struct Header *h = NULL;
  int code = SENDPOSTPONED;
  const char *p = NULL;
  int opt_delete;

  if (!Postponed)
    return -1;

  PostContext = mx_open_mailbox(Postponed, MUTT_NOSORT, NULL);
  if (!PostContext)
  {
    PostCount = 0;
    mutt_error(_("No postponed messages."));
    return -1;
  }

  if (!PostContext->msgcount)
  {
    PostCount = 0;
    mx_close_mailbox(PostContext, NULL);
    FREE(&PostContext);
    mutt_error(_("No postponed messages."));
    return -1;
  }

  if (PostContext->msgcount == 1)
  {
    /* only one message, so just use that one. */
    h = PostContext->hdrs[0];
  }
  else if ((h = select_msg()) == NULL)
  {
    mx_close_mailbox(PostContext, NULL);
    FREE(&PostContext);
    return -1;
  }

  if (mutt_prepare_template(NULL, PostContext, hdr, h, 0) < 0)
  {
    mx_fastclose_mailbox(PostContext);
    FREE(&PostContext);
    return -1;
  }

  /* finished with this message, so delete it. */
  mutt_set_flag(PostContext, h, MUTT_DELETE, 1);
  mutt_set_flag(PostContext, h, MUTT_PURGE, 1);

  /* update the count for the status display */
  PostCount = PostContext->msgcount - PostContext->deleted;

  /* avoid the "purge deleted messages" prompt */
  opt_delete = quadoption(OPT_DELETE);
  set_quadoption(OPT_DELETE, MUTT_YES);
  mx_close_mailbox(PostContext, NULL);
  set_quadoption(OPT_DELETE, opt_delete);

  FREE(&PostContext);

  struct ListNode *np, *tmp;
  STAILQ_FOREACH_SAFE(np, &hdr->env->userhdrs, entries, tmp)
  {
    if (mutt_strncasecmp("X-Mutt-References:", np->data, 18) == 0)
    {
      if (ctx)
      {
        /* if a mailbox is currently open, look to see if the original message
           the user attempted to reply to is in this mailbox */
        p = skip_email_wsp(np->data + 18);
        if (!ctx->id_hash)
          ctx->id_hash = mutt_make_id_hash(ctx);
        *cur = hash_find(ctx->id_hash, p);
      }
      if (*cur)
        code |= SENDREPLY;
    }
    else if (mutt_strncasecmp("X-Mutt-Fcc:", np->data, 11) == 0)
    {
      p = skip_email_wsp(np->data + 11);
      strfcpy(fcc, p, fcclen);
      mutt_pretty_mailbox(fcc, fcclen);

      /* note that x-mutt-fcc was present.  we do this because we want to add a
      * default fcc if the header was missing, but preserve the request of the
      * user to not make a copy if the header field is present, but empty.
      * see http://dev.mutt.org/trac/ticket/3653
      */
      code |= SENDPOSTPONEDFCC;
    }
    else if ((WithCrypto & APPLICATION_PGP) &&
             ((mutt_strncmp("Pgp:", np->data, 4) == 0) /* this is generated
                                                        * by old neomutt versions
                                                        */
              || (mutt_strncmp("X-Mutt-PGP:", np->data, 11) == 0)))
    {
      hdr->security = mutt_parse_crypt_hdr(strchr(np->data, ':') + 1, 1, APPLICATION_PGP);
      hdr->security |= APPLICATION_PGP;
    }
    else if ((WithCrypto & APPLICATION_SMIME) &&
             (mutt_strncmp("X-Mutt-SMIME:", np->data, 13) == 0))
    {
      hdr->security = mutt_parse_crypt_hdr(strchr(np->data, ':') + 1, 1, APPLICATION_SMIME);
      hdr->security |= APPLICATION_SMIME;
    }

#ifdef MIXMASTER
    else if (mutt_strncmp("X-Mutt-Mix:", np->data, 11) == 0)
    {
      char *t = NULL;
      mutt_list_free(&hdr->chain);

      t = strtok(np->data + 11, " \t\n");
      while (t)
      {
        mutt_list_insert_tail(&hdr->chain, safe_strdup(t));
        t = strtok(NULL, " \t\n");
      }
    }
#endif

    else
    {
      // skip header removal
      continue;
    }

    // remove the header
    STAILQ_REMOVE(&hdr->env->userhdrs, np, ListNode, entries);
    FREE(&np->data);
    FREE(&np);
  }

  if (option(OPT_CRYPT_OPPORTUNISTIC_ENCRYPT))
    crypt_opportunistic_encrypt(hdr);

  return code;
}

int mutt_parse_crypt_hdr(const char *p, int set_empty_signas, int crypt_app)
{
  char smime_cryptalg[LONG_STRING] = "\0";
  char sign_as[LONG_STRING] = "\0", *q = NULL;
  int flags = 0;

  if (!WithCrypto)
    return 0;

  p = skip_email_wsp(p);
  for (; *p; p++)
  {
    switch (*p)
    {
      case 'e':
      case 'E':
        flags |= ENCRYPT;
        break;

      case 'o':
      case 'O':
        flags |= OPPENCRYPT;
        break;

      case 's':
      case 'S':
        flags |= SIGN;
        q = sign_as;

        if (*(p + 1) == '<')
        {
          for (p += 2; *p && *p != '>' && q < sign_as + sizeof(sign_as) - 1; *q++ = *p++)
            ;

          if (*p != '>')
          {
            mutt_error(_("Illegal crypto header"));
            return 0;
          }
        }

        *q = '\0';
        break;

      /* This used to be the micalg parameter.
       *
       * It's no longer needed, so we just skip the parameter in order
       * to be able to recall old messages.
       */
      case 'm':
      case 'M':
        if (*(p + 1) == '<')
        {
          for (p += 2; *p && *p != '>'; p++)
            ;
          if (*p != '>')
          {
            mutt_error(_("Illegal crypto header"));
            return 0;
          }
        }

        break;

      case 'c':
      case 'C':
        q = smime_cryptalg;

        if (*(p + 1) == '<')
        {
          for (p += 2; *p && *p != '>' && q < smime_cryptalg + sizeof(smime_cryptalg) - 1;
               *q++ = *p++)
            ;

          if (*p != '>')
          {
            mutt_error(_("Illegal S/MIME header"));
            return 0;
          }
        }

        *q = '\0';
        break;

      case 'i':
      case 'I':
        flags |= INLINE;
        break;

      default:
        mutt_error(_("Illegal crypto header"));
        return 0;
    }
  }

  /* the cryptalg field must not be empty */
  if ((WithCrypto & APPLICATION_SMIME) && *smime_cryptalg)
    mutt_str_replace(&SmimeEncryptWith, smime_cryptalg);

  /* Set {Smime,Pgp}SignAs, if desired. */

  if ((WithCrypto & APPLICATION_PGP) && (crypt_app == APPLICATION_PGP) &&
      (flags & SIGN) && (set_empty_signas || *sign_as))
    mutt_str_replace(&PgpSignAs, sign_as);

  if ((WithCrypto & APPLICATION_SMIME) && (crypt_app == APPLICATION_SMIME) &&
      (flags & SIGN) && (set_empty_signas || *sign_as))
    mutt_str_replace(&SmimeDefaultKey, sign_as);

  return flags;
}

/**
 * mutt_prepare_template - Prepare a message template
 * @param fp      If not NULL, file containing the template
 * @param ctx     If fp is NULL, the context containing the header with the template
 * @param newhdr  The template is read into this Header
 * @param hdr     The message to recall/resend
 * @param resend  Set if resending (as opposed to recalling a postponed msg).
 *                Resent messages enable header weeding, and also
 *                discard any existing Message-ID and Mail-Followup-To.
 * @retval 0 on success
 * @retval -1 on error
 */
int mutt_prepare_template(FILE *fp, struct Context *ctx, struct Header *newhdr,
                          struct Header *hdr, short resend)
{
  struct Message *msg = NULL;
  char file[_POSIX_PATH_MAX];
  struct Body *b = NULL;
  FILE *bfp = NULL;
  int rv = -1;
  struct State s;
  int sec_type;

  memset(&s, 0, sizeof(s));

  if (!fp && (msg = mx_open_message(ctx, hdr->msgno)) == NULL)
    return -1;

  if (!fp)
    fp = msg->fp;

  bfp = fp;

  /* parse the message header and MIME structure */

  fseeko(fp, hdr->offset, SEEK_SET);
  newhdr->offset = hdr->offset;
  /* enable header weeding for resent messages */
  newhdr->env = mutt_read_rfc822_header(fp, newhdr, 1, resend);
  newhdr->content->length = hdr->content->length;
  mutt_parse_part(fp, newhdr->content);

  /* If resending a message, don't keep message_id or mail_followup_to.
   * Otherwise, we are resuming a postponed message, and want to keep those
   * headers if they exist.
   */
  if (resend)
  {
    FREE(&newhdr->env->message_id);
    FREE(&newhdr->env->mail_followup_to);
  }

  /* decrypt pgp/mime encoded messages */

  if ((WithCrypto & APPLICATION_PGP) &&
      (sec_type = mutt_is_multipart_encrypted(newhdr->content)))
  {
    newhdr->security |= sec_type;
    if (!crypt_valid_passphrase(sec_type))
      goto err;

    mutt_message(_("Decrypting message..."));
    if ((crypt_pgp_decrypt_mime(fp, &bfp, newhdr->content, &b) == -1) || b == NULL)
    {
    err:
      mx_close_message(ctx, &msg);
      mutt_free_envelope(&newhdr->env);
      mutt_free_body(&newhdr->content);
      mutt_error(_("Decryption failed."));
      return -1;
    }

    mutt_free_body(&newhdr->content);
    newhdr->content = b;

    mutt_clear_error();
  }

  /*
   * remove a potential multipart/signed layer - useful when
   * resending messages
   */

  if (WithCrypto && mutt_is_multipart_signed(newhdr->content))
  {
    newhdr->security |= SIGN;
    if ((WithCrypto & APPLICATION_PGP) &&
        (mutt_strcasecmp(mutt_get_parameter("protocol", newhdr->content->parameter),
                         "application/pgp-signature") == 0))
      newhdr->security |= APPLICATION_PGP;
    else if ((WithCrypto & APPLICATION_SMIME))
      newhdr->security |= APPLICATION_SMIME;

    /* destroy the signature */
    mutt_free_body(&newhdr->content->parts->next);
    newhdr->content = mutt_remove_multipart(newhdr->content);
  }

  /*
   * We don't need no primary multipart.
   * Note: We _do_ preserve messages!
   *
   * XXX - we don't handle multipart/alternative in any
   * smart way when sending messages.  However, one may
   * consider this a feature.
   *
   */

  if (newhdr->content->type == TYPEMULTIPART)
    newhdr->content = mutt_remove_multipart(newhdr->content);

  s.fpin = bfp;

  /* create temporary files for all attachments */
  for (b = newhdr->content; b; b = b->next)
  {
    /* what follows is roughly a receive-mode variant of
     * mutt_get_tmp_attachment () from muttlib.c
     */

    file[0] = '\0';
    if (b->filename)
    {
      strfcpy(file, b->filename, sizeof(file));
      b->d_filename = safe_strdup(b->filename);
    }
    else
    {
      /* avoid Content-Disposition: header with temporary filename */
      b->use_disp = false;
    }

    /* set up state flags */

    s.flags = 0;

    if (b->type == TYPETEXT)
    {
      if (mutt_strcasecmp("yes", mutt_get_parameter("x-mutt-noconv", b->parameter)) == 0)
        b->noconv = true;
      else
      {
        s.flags |= MUTT_CHARCONV;
        b->noconv = false;
      }

      mutt_delete_parameter("x-mutt-noconv", &b->parameter);
    }

    mutt_adv_mktemp(file, sizeof(file));
    s.fpout = safe_fopen(file, "w");
    if (!s.fpout)
      goto bail;

    if ((WithCrypto & APPLICATION_PGP) &&
        ((sec_type = mutt_is_application_pgp(b)) & (ENCRYPT | SIGN)))
    {
      mutt_body_handler(b, &s);

      newhdr->security |= sec_type;

      b->type = TYPETEXT;
      mutt_str_replace(&b->subtype, "plain");
      mutt_delete_parameter("x-action", &b->parameter);
    }
    else if ((WithCrypto & APPLICATION_SMIME) &&
             ((sec_type = mutt_is_application_smime(b)) & (ENCRYPT | SIGN)))
    {
      mutt_body_handler(b, &s);

      newhdr->security |= sec_type;
      b->type = TYPETEXT;
      mutt_str_replace(&b->subtype, "plain");
    }
    else
      mutt_decode_attachment(b, &s);

    if (safe_fclose(&s.fpout) != 0)
      goto bail;

    mutt_str_replace(&b->filename, file);
    b->unlink = true;

    mutt_stamp_attachment(b);

    mutt_free_body(&b->parts);
    if (b->hdr)
      b->hdr->content = NULL; /* avoid dangling pointer */
  }

  /* Fix encryption flags. */

  /* No inline if multipart. */
  if (WithCrypto && (newhdr->security & INLINE) && newhdr->content->next)
    newhdr->security &= ~INLINE;

  /* Do we even support multiple mechanisms? */
  newhdr->security &= WithCrypto | ~(APPLICATION_PGP | APPLICATION_SMIME);

  /* Theoretically, both could be set. Take the one the user wants to set by default. */
  if ((newhdr->security & APPLICATION_PGP) && (newhdr->security & APPLICATION_SMIME))
  {
    if (option(OPT_SMIME_IS_DEFAULT))
      newhdr->security &= ~APPLICATION_PGP;
    else
      newhdr->security &= ~APPLICATION_SMIME;
  }

  rv = 0;

bail:

  /* that's it. */
  if (bfp != fp)
    safe_fclose(&bfp);
  if (msg)
    mx_close_message(ctx, &msg);

  if (rv == -1)
  {
    mutt_free_envelope(&newhdr->env);
    mutt_free_body(&newhdr->content);
  }

  return rv;
}
