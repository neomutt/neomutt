/**
 * @file
 * Send/reply with an attachment
 *
 * @authors
 * Copyright (C) 1999-2004 Thomas Roessler <roessler@does-not-exist.org>
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
#include "mutt.h"
#include "alias.h"
#include "attach.h"
#include "body.h"
#include "copy.h"
#include "envelope.h"
#include "globals.h"
#include "header.h"
#include "lib.h"
#include "mutt_curses.h"
#include "mutt_idna.h"
#include "options.h"
#include "protos.h"
#include "rfc822.h"
#include "state.h"

/**
 * check_msg - Are we working with an RFC822 message
 *
 * some helper functions to verify that we are exclusively operating on
 * message/rfc822 attachments
 */
static bool check_msg(struct Body *b, bool err)
{
  if (!mutt_is_message_type(b->type, b->subtype))
  {
    if (err)
      mutt_error(_("You may only bounce message/rfc822 parts."));
    return false;
  }
  return true;
}

static bool check_all_msg(struct AttachPtr **idx, short idxlen, struct Body *cur, bool err)
{
  if (cur && !check_msg(cur, err))
    return false;
  else if (!cur)
  {
    for (short i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
      {
        if (!check_msg(idx[i]->content, err))
          return false;
      }
    }
  }
  return true;
}


/**
 * check_can_decode - can we decode all tagged attachments?
 */
static short check_can_decode(struct AttachPtr **idx, short idxlen, struct Body *cur)
{
  if (cur)
    return mutt_can_decode(cur);

  for (short i = 0; i < idxlen; i++)
    if (idx[i]->content->tagged && !mutt_can_decode(idx[i]->content))
      return 0;

  return 1;
}

static short count_tagged(struct AttachPtr **idx, short idxlen)
{
  short count = 0;
  for (short i = 0; i < idxlen; i++)
    if (idx[i]->content->tagged)
      count++;

  return count;
}

/**
 * count_tagged_children - tagged children below a multipart/message attachment
 */
static short count_tagged_children(struct AttachPtr **idx, short idxlen, short i)
{
  short level = idx[i]->level;
  short count = 0;

  while ((++i < idxlen) && (level < idx[i]->level))
    if (idx[i]->content->tagged)
      count++;

  return count;
}


/**
 * mutt_attach_bounce - Bounce function, from the attachment menu
 */
void mutt_attach_bounce(FILE *fp, struct Header *hdr, struct AttachPtr **idx,
                        short idxlen, struct Body *cur)
{
  char prompt[STRING];
  char buf[HUGE_STRING];
  char *err = NULL;
  struct Address *adr = NULL;
  int ret = 0;
  int p = 0;

  if (!check_all_msg(idx, idxlen, cur, true))
    return;

  /* one or more messages? */
  p = (cur || count_tagged(idx, idxlen) == 1);

  /* RFC5322 mandates a From: header, so warn before bouncing
   * messages without one */
  if (cur)
  {
    if (!cur->hdr->env->from)
    {
      mutt_error(_("Warning: message contains no From: header"));
      mutt_sleep(2);
      mutt_clear_error();
    }
  }
  else
  {
    for (short i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
      {
        if (!idx[i]->content->hdr->env->from)
        {
          mutt_error(_("Warning: message contains no From: header"));
          mutt_sleep(2);
          mutt_clear_error();
          break;
        }
      }
    }
  }

  if (p)
    strfcpy(prompt, _("Bounce message to: "), sizeof(prompt));
  else
    strfcpy(prompt, _("Bounce tagged messages to: "), sizeof(prompt));

  buf[0] = '\0';
  if (mutt_get_field(prompt, buf, sizeof(buf), MUTT_ALIAS) || buf[0] == '\0')
    return;

  if (!(adr = rfc822_parse_adrlist(adr, buf)))
  {
    mutt_error(_("Error parsing address!"));
    return;
  }

  adr = mutt_expand_aliases(adr);

  if (mutt_addrlist_to_intl(adr, &err) < 0)
  {
    mutt_error(_("Bad IDN: '%s'"), err);
    FREE(&err);
    rfc822_free_address(&adr);
    return;
  }

  buf[0] = 0;
  rfc822_write_address(buf, sizeof(buf), adr, 1);

#define EXTRA_SPACE (15 + 7 + 2)
  /*
   * See commands.c.
   */
  snprintf(prompt, sizeof(prompt) - 4,
           (p ? _("Bounce message to %s") : _("Bounce messages to %s")), buf);

  if (mutt_strwidth(prompt) > MuttMessageWindow->cols - EXTRA_SPACE)
  {
    mutt_simple_format(prompt, sizeof(prompt) - 4, 0, MuttMessageWindow->cols - EXTRA_SPACE,
                       FMT_LEFT, 0, prompt, sizeof(prompt), 0);
    safe_strcat(prompt, sizeof(prompt), "...?");
  }
  else
    safe_strcat(prompt, sizeof(prompt), "?");

  if (query_quadoption(OPT_BOUNCE, prompt) != MUTT_YES)
  {
    rfc822_free_address(&adr);
    mutt_window_clearline(MuttMessageWindow, 0);
    mutt_message(p ? _("Message not bounced.") : _("Messages not bounced."));
    return;
  }

  mutt_window_clearline(MuttMessageWindow, 0);

  if (cur)
    ret = mutt_bounce_message(fp, cur->hdr, adr);
  else
  {
    for (short i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
        if (mutt_bounce_message(fp, idx[i]->content->hdr, adr))
          ret = 1;
    }
  }

  if (!ret)
    mutt_message(p ? _("Message bounced.") : _("Messages bounced."));
  else
    mutt_error(p ? _("Error bouncing message!") :
                   _("Error bouncing messages!"));
}


/**
 * mutt_attach_resend - resend-message, from the attachment menu
 */
void mutt_attach_resend(FILE *fp, struct Header *hdr, struct AttachPtr **idx,
                        short idxlen, struct Body *cur)
{
  if (!check_all_msg(idx, idxlen, cur, true))
    return;

  if (cur)
    mutt_resend_message(fp, Context, cur->hdr);
  else
  {
    for (short i = 0; i < idxlen; i++)
      if (idx[i]->content->tagged)
        mutt_resend_message(fp, Context, idx[i]->content->hdr);
  }
}


/**
 ** forward-message, from the attachment menu
 **/

/**
 * find_common_parent - find a common parent message for the tagged attachments
 */
static struct Header *find_common_parent(struct AttachPtr **idx, short idxlen, short nattach)
{
  short i;
  short nchildren;

  for (i = 0; i < idxlen; i++)
    if (idx[i]->content->tagged)
      break;

  while (--i >= 0)
  {
    if (mutt_is_message_type(idx[i]->content->type, idx[i]->content->subtype))
    {
      nchildren = count_tagged_children(idx, idxlen, i);
      if (nchildren == nattach)
        return idx[i]->content->hdr;
    }
  }

  return NULL;
}

/**
 * is_parent - Check whether one attachment is the parent of another
 *
 * check whether attachment i is a parent of the attachment pointed to by cur
 *
 * Note: This and the calling procedure could be optimized quite a bit.
 * For now, it's not worth the effort.
 */
static bool is_parent(short i, struct AttachPtr **idx, short idxlen, struct Body *cur)
{
  short level = idx[i]->level;

  while ((++i < idxlen) && idx[i]->level > level)
  {
    if (idx[i]->content == cur)
      return true;
  }

  return false;
}

static struct Header *find_parent(struct AttachPtr **idx, short idxlen,
                                  struct Body *cur, short nattach)
{
  struct Header *parent = NULL;

  if (cur)
  {
    for (short i = 0; i < idxlen; i++)
    {
      if (mutt_is_message_type(idx[i]->content->type, idx[i]->content->subtype) &&
          is_parent(i, idx, idxlen, cur))
        parent = idx[i]->content->hdr;
      if (idx[i]->content == cur)
        break;
    }
  }
  else if (nattach)
    parent = find_common_parent(idx, idxlen, nattach);

  return parent;
}

static void include_header(int quote, FILE *ifp, struct Header *hdr, FILE *ofp, char *_prefix)
{
  int chflags = CH_DECODE;
  char prefix[SHORT_STRING];

  if (option(OPTWEED))
    chflags |= CH_WEED | CH_REORDER;

  if (quote)
  {
    if (_prefix)
      strfcpy(prefix, _prefix, sizeof(prefix));
    else if (!option(OPTTEXTFLOWED))
      _mutt_make_string(prefix, sizeof(prefix), NONULL(Prefix), Context, hdr, 0);
    else
      strfcpy(prefix, ">", sizeof(prefix));

    chflags |= CH_PREFIX;
  }

  mutt_copy_header(ifp, hdr, ofp, chflags, quote ? prefix : NULL);
}

/**
 * copy_problematic_attachments - Attach the body parts which can't be decoded
 *
 * This code is shared by forwarding and replying.
 */
static struct Body **copy_problematic_attachments(FILE *fp, struct Body **last,
                                                  struct AttachPtr **idx,
                                                  short idxlen, short force)
{
  for (short i = 0; i < idxlen; i++)
  {
    if (idx[i]->content->tagged && (force || !mutt_can_decode(idx[i]->content)))
    {
      if (mutt_copy_body(fp, last, idx[i]->content) == -1)
        return NULL; /* XXXXX - may lead to crashes */
      last = &((*last)->next);
    }
  }
  return last;
}

/**
 * attach_forward_bodies - forward one or several MIME bodies
 *
 * (non-message types)
 */
static void attach_forward_bodies(FILE *fp, struct Header *hdr,
                                  struct AttachPtr **idx, short idxlen,
                                  struct Body *cur, short nattach, int flags)
{
  bool mime_fwd_all = false;
  bool mime_fwd_any = true;
  struct Header *parent = NULL;
  struct Header *tmphdr = NULL;
  struct Body **last = NULL;
  char tmpbody[_POSIX_PATH_MAX];
  FILE *tmpfp = NULL;

  char prefix[STRING];

  int rc = 0;

  struct State st;

  /*
   * First, find the parent message.
   * Note: This could be made an option by just
   * putting the following lines into an if block.
   */


  parent = find_parent(idx, idxlen, cur, nattach);
  if (!parent)
    parent = hdr;


  tmphdr = mutt_new_header();
  tmphdr->env = mutt_new_envelope();
  mutt_make_forward_subject(tmphdr->env, Context, parent);

  mutt_mktemp(tmpbody, sizeof(tmpbody));
  if ((tmpfp = safe_fopen(tmpbody, "w")) == NULL)
  {
    mutt_error(_("Can't open temporary file %s."), tmpbody);
    mutt_free_header(&tmphdr);
    return;
  }

  mutt_forward_intro(Context, parent, tmpfp);

  /* prepare the prefix here since we'll need it later. */

  if (option(OPTFORWQUOTE))
  {
    if (!option(OPTTEXTFLOWED))
      _mutt_make_string(prefix, sizeof(prefix), NONULL(Prefix), Context, parent, 0);
    else
      strfcpy(prefix, ">", sizeof(prefix));
  }

  include_header(option(OPTFORWQUOTE), fp, parent, tmpfp, prefix);


  /*
   * Now, we have prepared the first part of the message body: The
   * original message's header.
   *
   * The next part is more interesting: either include the message bodies,
   * or attach them.
   */

  if ((!cur || mutt_can_decode(cur)) &&
      (rc = query_quadoption(OPT_MIMEFWD, _("Forward as attachments?"))) == MUTT_YES)
    mime_fwd_all = true;
  else if (rc == -1)
    goto bail;

  /*
   * shortcut MIMEFWDREST when there is only one attachment.  Is
   * this intuitive?
   */

  if (!mime_fwd_all && !cur && (nattach > 1) && !check_can_decode(idx, idxlen, cur))
  {
    if ((rc = query_quadoption(OPT_MIMEFWDREST,
                               _("Can't decode all tagged attachments.  "
                                 "MIME-forward the others?"))) == MUTT_ABORT)
      goto bail;
    else if (rc == MUTT_NO)
      mime_fwd_any = false;
  }

  /* initialize a state structure */

  memset(&st, 0, sizeof(st));

  if (option(OPTFORWQUOTE))
    st.prefix = prefix;
  st.flags = MUTT_CHARCONV;
  if (option(OPTWEED))
    st.flags |= MUTT_WEED;
  st.fpin = fp;
  st.fpout = tmpfp;

  /* where do we append new MIME parts? */
  last = &tmphdr->content;

  if (cur)
  {
    /* single body case */

    if (!mime_fwd_all && mutt_can_decode(cur))
    {
      mutt_body_handler(cur, &st);
      state_putc('\n', &st);
    }
    else
    {
      if (mutt_copy_body(fp, last, cur) == -1)
        goto bail;
      last = &((*last)->next);
    }
  }
  else
  {
    /* multiple body case */

    if (!mime_fwd_all)
    {
      for (int i = 0; i < idxlen; i++)
      {
        if (idx[i]->content->tagged && mutt_can_decode(idx[i]->content))
        {
          mutt_body_handler(idx[i]->content, &st);
          state_putc('\n', &st);
        }
      }
    }

    if (mime_fwd_any &&
        copy_problematic_attachments(fp, last, idx, idxlen, mime_fwd_all) == NULL)
      goto bail;
  }

  mutt_forward_trailer(Context, parent, tmpfp);

  safe_fclose(&tmpfp);
  tmpfp = NULL;

  /* now that we have the template, send it. */
  ci_send_message(flags, tmphdr, tmpbody, NULL, parent);
  return;

bail:

  if (tmpfp)
  {
    safe_fclose(&tmpfp);
    mutt_unlink(tmpbody);
  }

  mutt_free_header(&tmphdr);
}


/**
 * attach_forward_msgs - Forward one or several message-type attachments
 *
 * This is different from the previous function since we want to mimic the
 * index menu's behavior.
 *
 * Code reuse from ci_send_message is not possible here - ci_send_message
 * relies on a context structure to find messages, while, on the attachment
 * menu, messages are referenced through the attachment index.
 */
static void attach_forward_msgs(FILE *fp, struct Header *hdr, struct AttachPtr **idx,
                                short idxlen, struct Body *cur, int flags)
{
  struct Header *curhdr = NULL;
  struct Header *tmphdr = NULL;
  int rc;

  struct Body **last = NULL;
  char tmpbody[_POSIX_PATH_MAX];
  FILE *tmpfp = NULL;

  int cmflags = 0;
  int chflags = CH_XMIT;

  if (cur)
    curhdr = cur->hdr;
  else
  {
    for (short i = 0; i < idxlen; i++)
      if (idx[i]->content->tagged)
      {
        curhdr = idx[i]->content->hdr;
        break;
      }
  }

  tmphdr = mutt_new_header();
  tmphdr->env = mutt_new_envelope();
  mutt_make_forward_subject(tmphdr->env, Context, curhdr);


  tmpbody[0] = '\0';

  if ((rc = query_quadoption(OPT_MIMEFWD, _("Forward MIME encapsulated?"))) == MUTT_NO)
  {
    /* no MIME encapsulation */

    mutt_mktemp(tmpbody, sizeof(tmpbody));
    if (!(tmpfp = safe_fopen(tmpbody, "w")))
    {
      mutt_error(_("Can't create %s."), tmpbody);
      mutt_free_header(&tmphdr);
      return;
    }

    if (option(OPTFORWQUOTE))
    {
      chflags |= CH_PREFIX;
      cmflags |= MUTT_CM_PREFIX;
    }

    if (option(OPTFORWDECODE))
    {
      cmflags |= MUTT_CM_DECODE | MUTT_CM_CHARCONV;
      if (option(OPTWEED))
      {
        chflags |= CH_WEED | CH_REORDER;
        cmflags |= MUTT_CM_WEED;
      }
    }


    if (cur)
    {
      mutt_forward_intro(Context, cur->hdr, tmpfp);
      _mutt_copy_message(tmpfp, fp, cur->hdr, cur->hdr->content, cmflags, chflags);
      mutt_forward_trailer(Context, cur->hdr, tmpfp);
    }
    else
    {
      for (short i = 0; i < idxlen; i++)
      {
        if (idx[i]->content->tagged)
        {
          mutt_forward_intro(Context, idx[i]->content->hdr, tmpfp);
          _mutt_copy_message(tmpfp, fp, idx[i]->content->hdr,
                             idx[i]->content->hdr->content, cmflags, chflags);
          mutt_forward_trailer(Context, idx[i]->content->hdr, tmpfp);
        }
      }
    }
    safe_fclose(&tmpfp);
  }
  else if (rc == MUTT_YES) /* do MIME encapsulation - we don't need to do much here */
  {
    last = &tmphdr->content;
    if (cur)
      mutt_copy_body(fp, last, cur);
    else
    {
      for (short i = 0; i < idxlen; i++)
        if (idx[i]->content->tagged)
        {
          mutt_copy_body(fp, last, idx[i]->content);
          last = &((*last)->next);
        }
    }
  }
  else
    mutt_free_header(&tmphdr);

  ci_send_message(flags, tmphdr, *tmpbody ? tmpbody : NULL, NULL, curhdr);
}

void mutt_attach_forward(FILE *fp, struct Header *hdr, struct AttachPtr **idx,
                         short idxlen, struct Body *cur, int flags)
{
  short nattach;


  if (check_all_msg(idx, idxlen, cur, false))
    attach_forward_msgs(fp, hdr, idx, idxlen, cur, flags);
  else
  {
    nattach = count_tagged(idx, idxlen);
    attach_forward_bodies(fp, hdr, idx, idxlen, cur, nattach, flags);
  }
}


/**
 ** the various reply functions, from the attachment menu
 **/

/**
 * attach_reply_envelope_defaults - Create the envelope defaults for a reply
 *
 * This function can be invoked in two ways.
 *
 * Either, parent is NULL.  In this case, all tagged bodies are of a message type,
 * and the header information is fetched from them.
 *
 * Or, parent is non-NULL.  In this case, cur is the common parent of all the
 * tagged attachments.
 *
 * Note that this code is horribly similar to envelope_defaults() from send.c.
 */
static int attach_reply_envelope_defaults(struct Envelope *env, struct AttachPtr **idx,
                                          short idxlen, struct Header *parent, int flags)
{
  struct Envelope *curenv = NULL;
  struct Header *curhdr = NULL;

  if (!parent)
  {
    for (short i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
      {
        curhdr = idx[i]->content->hdr;
        curenv = curhdr->env;
        break;
      }
    }
  }
  else
  {
    curenv = parent->env;
    curhdr = parent;
  }

  if (curenv == NULL || curhdr == NULL)
  {
    mutt_error(_("Can't find any tagged messages."));
    return -1;
  }

#ifdef USE_NNTP
  if ((flags & SENDNEWS))
  {
    /* in case followup set Newsgroups: with Followup-To: if it present */
    if (!env->newsgroups && curenv &&
        (mutt_strcasecmp(curenv->followup_to, "poster") != 0))
      env->newsgroups = safe_strdup(curenv->followup_to);
  }
  else
#endif
  {
    if (parent)
    {
      if (mutt_fetch_recips(env, curenv, flags) == -1)
        return -1;
    }
    else
    {
      for (short i = 0; i < idxlen; i++)
      {
        if (idx[i]->content->tagged &&
            mutt_fetch_recips(env, idx[i]->content->hdr->env, flags) == -1)
          return -1;
      }
    }

    if ((flags & SENDLISTREPLY) && !env->to)
    {
      mutt_error(_("No mailing lists found!"));
      return -1;
    }

    mutt_fix_reply_recipients(env);
  }
  mutt_make_misc_reply_headers(env, Context, curhdr, curenv);

  if (parent)
    mutt_add_to_reference_headers(env, curenv, NULL, NULL);
  else
  {
    struct List **p = NULL, **q = NULL;

    for (short i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
        mutt_add_to_reference_headers(env, idx[i]->content->hdr->env, &p, &q);
    }
  }

  return 0;
}


/**
 * attach_include_reply - This is _very_ similar to send.c's include_reply()
 */
static void attach_include_reply(FILE *fp, FILE *tmpfp, struct Header *cur, int flags)
{
  int cmflags = MUTT_CM_PREFIX | MUTT_CM_DECODE | MUTT_CM_CHARCONV;
  int chflags = CH_DECODE;

  mutt_make_attribution(Context, cur, tmpfp);

  if (!option(OPTHEADER))
    cmflags |= MUTT_CM_NOHEADER;
  if (option(OPTWEED))
  {
    chflags |= CH_WEED;
    cmflags |= MUTT_CM_WEED;
  }

  _mutt_copy_message(tmpfp, fp, cur, cur->content, cmflags, chflags);
  mutt_make_post_indent(Context, cur, tmpfp);
}

void mutt_attach_reply(FILE *fp, struct Header *hdr, struct AttachPtr **idx,
                       short idxlen, struct Body *cur, int flags)
{
  bool mime_reply_any = false;

  short nattach = 0;
  struct Header *parent = NULL;
  struct Header *tmphdr = NULL;

  struct State st;
  char tmpbody[_POSIX_PATH_MAX];
  FILE *tmpfp = NULL;

  char prefix[SHORT_STRING];
  int rc;

#ifdef USE_NNTP
  if (flags & SENDNEWS)
    set_option(OPTNEWSSEND);
  else
    unset_option(OPTNEWSSEND);
#endif

  if (!check_all_msg(idx, idxlen, cur, false))
  {
    nattach = count_tagged(idx, idxlen);
    if ((parent = find_parent(idx, idxlen, cur, nattach)) == NULL)
      parent = hdr;
  }

  if (nattach > 1 && !check_can_decode(idx, idxlen, cur))
  {
    if ((rc = query_quadoption(OPT_MIMEFWDREST,
                               _("Can't decode all tagged attachments.  "
                                 "MIME-encapsulate the others?"))) == MUTT_ABORT)
      return;
    else if (rc == MUTT_YES)
      mime_reply_any = true;
  }
  else if (nattach == 1)
    mime_reply_any = true;

  tmphdr = mutt_new_header();
  tmphdr->env = mutt_new_envelope();

  if (attach_reply_envelope_defaults(tmphdr->env, idx, idxlen,
                                     parent ? parent : (cur ? cur->hdr : NULL), flags) == -1)
  {
    mutt_free_header(&tmphdr);
    return;
  }

  mutt_mktemp(tmpbody, sizeof(tmpbody));
  if ((tmpfp = safe_fopen(tmpbody, "w")) == NULL)
  {
    mutt_error(_("Can't create %s."), tmpbody);
    mutt_free_header(&tmphdr);
    return;
  }

  if (!parent)
  {
    if (cur)
      attach_include_reply(fp, tmpfp, cur->hdr, flags);
    else
    {
      for (short i = 0; i < idxlen; i++)
      {
        if (idx[i]->content->tagged)
          attach_include_reply(fp, tmpfp, idx[i]->content->hdr, flags);
      }
    }
  }
  else
  {
    mutt_make_attribution(Context, parent, tmpfp);

    memset(&st, 0, sizeof(struct State));
    st.fpin = fp;
    st.fpout = tmpfp;

    if (!option(OPTTEXTFLOWED))
      _mutt_make_string(prefix, sizeof(prefix), NONULL(Prefix), Context, parent, 0);
    else
      strfcpy(prefix, ">", sizeof(prefix));

    st.prefix = prefix;
    st.flags = MUTT_CHARCONV;

    if (option(OPTWEED))
      st.flags |= MUTT_WEED;

    if (option(OPTHEADER))
      include_header(1, fp, parent, tmpfp, prefix);

    if (cur)
    {
      if (mutt_can_decode(cur))
      {
        mutt_body_handler(cur, &st);
        state_putc('\n', &st);
      }
      else
        mutt_copy_body(fp, &tmphdr->content, cur);
    }
    else
    {
      for (short i = 0; i < idxlen; i++)
      {
        if (idx[i]->content->tagged && mutt_can_decode(idx[i]->content))
        {
          mutt_body_handler(idx[i]->content, &st);
          state_putc('\n', &st);
        }
      }
    }

    mutt_make_post_indent(Context, parent, tmpfp);

    if (mime_reply_any && !cur &&
        copy_problematic_attachments(fp, &tmphdr->content, idx, idxlen, 0) == NULL)
    {
      mutt_free_header(&tmphdr);
      safe_fclose(&tmpfp);
      return;
    }
  }

  safe_fclose(&tmpfp);

  if (ci_send_message(flags, tmphdr, tmpbody, NULL,
                      parent ? parent : (cur ? cur->hdr : NULL)) == 0)
    mutt_set_flag(Context, hdr, MUTT_REPLIED, 1);
}
