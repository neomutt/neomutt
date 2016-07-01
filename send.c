/*
 * Copyright (C) 1996-2002,2004,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "rfc2047.h"
#include "keymap.h"
#include "mime.h"
#include "mailbox.h"
#include "copy.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"
#include "url.h"
#include "rfc3676.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <utime.h>

#ifdef MIXMASTER
#include "remailer.h"
#endif


static void append_signature (FILE *f)
{
  FILE *tmpfp;
  pid_t thepid;

  if (Signature && (tmpfp = mutt_open_read (Signature, &thepid)))
  {
    if (option (OPTSIGDASHES))
      fputs ("\n-- \n", f);
    mutt_copy_stream (tmpfp, f);
    safe_fclose (&tmpfp);
    if (thepid != -1)
      mutt_wait_filter (thepid);
  }
}

/* compare two e-mail addresses and return 1 if they are equivalent */
static int mutt_addrcmp (ADDRESS *a, ADDRESS *b)
{
  if (!a->mailbox || !b->mailbox)
    return 0;
  if (ascii_strcasecmp (a->mailbox, b->mailbox))
    return 0;
  return 1;
}

/* search an e-mail address in a list */
static int mutt_addrsrc (ADDRESS *a, ADDRESS *lst)
{
  for (; lst; lst = lst->next)
  {
    if (mutt_addrcmp (a, lst))
      return (1);
  }
  return (0);
}

/* removes addresses from "b" which are contained in "a" */
ADDRESS *mutt_remove_xrefs (ADDRESS *a, ADDRESS *b)
{
  ADDRESS *top, *p, *prev = NULL;

  top = b;
  while (b)
  {
    for (p = a; p; p = p->next)
    {
      if (mutt_addrcmp (p, b))
	break;
    }
    if (p)
    {
      if (prev)
      {
	prev->next = b->next;
	b->next = NULL;
	rfc822_free_address (&b);
	b = prev;
      }
      else
      {
	top = top->next;
	b->next = NULL;
	rfc822_free_address (&b);
	b = top;
      }
    }
    else
    {
      prev = b;
      b = b->next;
    }
  }
  return top;
}

/* remove any address which matches the current user.  if `leave_only' is
 * nonzero, don't remove the user's address if it is the only one in the list
 */
static ADDRESS *remove_user (ADDRESS *a, int leave_only)
{
  ADDRESS *top = NULL, *last = NULL;

  while (a)
  {
    if (!mutt_addr_is_user (a))
    {
      if (top)
      {
        last->next = a;
        last = last->next;
      }
      else
        last = top = a;
      a = a->next;
      last->next = NULL;
    }
    else
    {
      ADDRESS *tmp = a;
      
      a = a->next;
      if (!leave_only || a || last)
      {
	tmp->next = NULL;
	rfc822_free_address (&tmp);
      }
      else
	last = top = tmp;
    }
  }
  return top;
}

static ADDRESS *find_mailing_lists (ADDRESS *t, ADDRESS *c)
{
  ADDRESS *top = NULL, *ptr = NULL;

  for (; t || c; t = c, c = NULL)
  {
    for (; t; t = t->next)
    {
      if (mutt_is_mail_list (t) && !t->group)
      {
	if (top)
	{
	  ptr->next = rfc822_cpy_adr_real (t);
	  ptr = ptr->next;
	}
	else
	  ptr = top = rfc822_cpy_adr_real (t);
      }
    }
  }
  return top;
}

static int edit_address (ADDRESS **a, /* const */ char *field)
{
  char buf[HUGE_STRING];
  char *err = NULL;
  int idna_ok = 0;
  
  do
  {
    buf[0] = 0;
    mutt_addrlist_to_local (*a);
    rfc822_write_address (buf, sizeof (buf), *a, 0);
    if (mutt_get_field (field, buf, sizeof (buf), MUTT_ALIAS) != 0)
      return (-1);
    rfc822_free_address (a);
    *a = mutt_expand_aliases (mutt_parse_adrlist (NULL, buf));
    if ((idna_ok = mutt_addrlist_to_intl (*a, &err)) != 0)
    {
      mutt_error (_("Error: '%s' is a bad IDN."), err);
      mutt_refresh ();
      mutt_sleep (2);
      FREE (&err);
    }
  } 
  while (idna_ok != 0);
  return 0;
}

static int edit_envelope (ENVELOPE *en)
{
  char buf[HUGE_STRING];
  LIST *uh = UserHeader;

  if (edit_address (&en->to, "To: ") == -1 || en->to == NULL)
    return (-1);
  if (option (OPTASKCC) && edit_address (&en->cc, "Cc: ") == -1)
    return (-1);
  if (option (OPTASKBCC) && edit_address (&en->bcc, "Bcc: ") == -1)
    return (-1);

  if (en->subject)
  {
    if (option (OPTFASTREPLY))
      return (0);
    else
      strfcpy (buf, en->subject, sizeof (buf));
  }
  else
  {
    const char *p;

    buf[0] = 0;
    for (; uh; uh = uh->next)
    {
      if (ascii_strncasecmp ("subject:", uh->data, 8) == 0)
      {
	p = skip_email_wsp(uh->data + 8);
	strncpy (buf, p, sizeof (buf));
      }
    }
  }
  
  if (mutt_get_field ("Subject: ", buf, sizeof (buf), 0) != 0 ||
      (!buf[0] && query_quadoption (OPT_SUBJECT, _("No subject, abort?")) != MUTT_NO))
  {
    mutt_message _("No subject, aborting.");
    return (-1);
  }
  mutt_str_replace (&en->subject, buf);

  return 0;
}

static void process_user_recips (ENVELOPE *env)
{
  LIST *uh = UserHeader;

  for (; uh; uh = uh->next)
  {
    if (ascii_strncasecmp ("to:", uh->data, 3) == 0)
      env->to = rfc822_parse_adrlist (env->to, uh->data + 3);
    else if (ascii_strncasecmp ("cc:", uh->data, 3) == 0)
      env->cc = rfc822_parse_adrlist (env->cc, uh->data + 3);
    else if (ascii_strncasecmp ("bcc:", uh->data, 4) == 0)
      env->bcc = rfc822_parse_adrlist (env->bcc, uh->data + 4);
  }
}

static void process_user_header (ENVELOPE *env)
{
  LIST *uh = UserHeader;
  LIST *last = env->userhdrs;

  if (last)
    while (last->next)
      last = last->next;

  for (; uh; uh = uh->next)
  {
    if (ascii_strncasecmp ("from:", uh->data, 5) == 0)
    {
      /* User has specified a default From: address.  Remove default address */
      rfc822_free_address (&env->from);
      env->from = rfc822_parse_adrlist (env->from, uh->data + 5);
    }
    else if (ascii_strncasecmp ("reply-to:", uh->data, 9) == 0)
    {
      rfc822_free_address (&env->reply_to);
      env->reply_to = rfc822_parse_adrlist (env->reply_to, uh->data + 9);
    }
    else if (ascii_strncasecmp ("message-id:", uh->data, 11) == 0)
    {
      char *tmp = mutt_extract_message_id (uh->data + 11, NULL);
      if (rfc822_valid_msgid (tmp) >= 0)
      {
	FREE(&env->message_id);
	env->message_id = tmp;
      } else
	FREE(&tmp);
    }
    else if (ascii_strncasecmp ("to:", uh->data, 3) != 0 &&
	     ascii_strncasecmp ("cc:", uh->data, 3) != 0 &&
	     ascii_strncasecmp ("bcc:", uh->data, 4) != 0 &&
	     ascii_strncasecmp ("subject:", uh->data, 8) != 0 &&
	     ascii_strncasecmp ("return-path:", uh->data, 12) != 0)
    {
      if (last)
      {
	last->next = mutt_new_list ();
	last = last->next;
      }
      else
	last = env->userhdrs = mutt_new_list ();
      last->data = safe_strdup (uh->data);
    }
  }
}

LIST *mutt_copy_list (LIST *p)
{
  LIST *t, *r=NULL, *l=NULL;

  for (; p; p = p->next)
  {
    t = (LIST *) safe_malloc (sizeof (LIST));
    t->data = safe_strdup (p->data);
    t->next = NULL;
    if (l)
    {
      r->next = t;
      r = r->next;
    }
    else
      l = r = t;
  }
  return (l);
}

void mutt_forward_intro (FILE *fp, HEADER *cur)
{
  char buffer[STRING];
  
  fputs ("----- Forwarded message from ", fp);
  buffer[0] = 0;
  rfc822_write_address (buffer, sizeof (buffer), cur->env->from, 1);
  fputs (buffer, fp);
  fputs (" -----\n\n", fp);
}

void mutt_forward_trailer (FILE *fp)
{
  fputs ("\n----- End forwarded message -----\n", fp);
}


static int include_forward (CONTEXT *ctx, HEADER *cur, FILE *out)
{
  int chflags = CH_DECODE, cmflags = 0;
  
  mutt_parse_mime_message (ctx, cur);
  mutt_message_hook (ctx, cur, MUTT_MESSAGEHOOK);

  if (WithCrypto && (cur->security & ENCRYPT) && option (OPTFORWDECODE))
  {
    /* make sure we have the user's passphrase before proceeding... */
    crypt_valid_passphrase (cur->security);
  }

  mutt_forward_intro (out, cur);

  if (option (OPTFORWDECODE))
  {
    cmflags |= MUTT_CM_DECODE | MUTT_CM_CHARCONV;
    if (option (OPTWEED))
    {
      chflags |= CH_WEED | CH_REORDER;
      cmflags |= MUTT_CM_WEED;
    }
  }
  if (option (OPTFORWQUOTE))
    cmflags |= MUTT_CM_PREFIX;

  /* wrapping headers for forwarding is considered a display
   * rather than send action */
  chflags |= CH_DISPLAY;

  mutt_copy_message (out, ctx, cur, cmflags, chflags);
  mutt_forward_trailer (out);
  return 0;
}

void mutt_make_attribution (CONTEXT *ctx, HEADER *cur, FILE *out)
{
  char buffer[LONG_STRING];
  if (Attribution)
  {
    mutt_make_string (buffer, sizeof (buffer), Attribution, ctx, cur);
    fputs (buffer, out);
    fputc ('\n', out);
  }
}

void mutt_make_post_indent (CONTEXT *ctx, HEADER *cur, FILE *out)
{
  char buffer[STRING];
  if (PostIndentString)
  {
    mutt_make_string (buffer, sizeof (buffer), PostIndentString, ctx, cur);
    fputs (buffer, out);
    fputc ('\n', out);
  }
}

static int include_reply (CONTEXT *ctx, HEADER *cur, FILE *out)
{
  int cmflags = MUTT_CM_PREFIX | MUTT_CM_DECODE | MUTT_CM_CHARCONV | MUTT_CM_REPLYING;
  int chflags = CH_DECODE;

  if (WithCrypto && (cur->security & ENCRYPT))
  {
    /* make sure we have the user's passphrase before proceeding... */
    crypt_valid_passphrase (cur->security);
  }

  mutt_parse_mime_message (ctx, cur);
  mutt_message_hook (ctx, cur, MUTT_MESSAGEHOOK);
  
  mutt_make_attribution (ctx, cur, out);
  
  if (!option (OPTHEADER))
    cmflags |= MUTT_CM_NOHEADER;
  if (option (OPTWEED))
  {
    chflags |= CH_WEED | CH_REORDER;
    cmflags |= MUTT_CM_WEED;
  }

  mutt_copy_message (out, ctx, cur, cmflags, chflags);

  mutt_make_post_indent (ctx, cur, out);
  
  return 0;
}

static int default_to (ADDRESS **to, ENVELOPE *env, int flags, int hmfupto)
{
  char prompt[STRING];

  if (flags && env->mail_followup_to && hmfupto == MUTT_YES) 
  {
    rfc822_append (to, env->mail_followup_to, 1);
    return 0;
  }

  /* Exit now if we're setting up the default Cc list for list-reply
   * (only set if Mail-Followup-To is present and honoured).
   */
  if (flags & SENDLISTREPLY)
    return 0;

  if (!option(OPTREPLYSELF) && mutt_addr_is_user (env->from))
  {
    /* mail is from the user, assume replying to recipients */
    rfc822_append (to, env->to, 1);
  }
  else if (env->reply_to)
  {
    if ((mutt_addrcmp (env->from, env->reply_to) && !env->reply_to->next) || 
	(option (OPTIGNORELISTREPLYTO) &&
	mutt_is_mail_list (env->reply_to) &&
	(mutt_addrsrc (env->reply_to, env->to) ||
	mutt_addrsrc (env->reply_to, env->cc))))
    {
      /* If the Reply-To: address is a mailing list, assume that it was
       * put there by the mailing list, and use the From: address
       * 
       * We also take the from header if our correspondent has a reply-to
       * header which is identical to the electronic mail address given
       * in his From header.
       * 
       */
      rfc822_append (to, env->from, 0);
    }
    else if (!(mutt_addrcmp (env->from, env->reply_to) && 
	       !env->reply_to->next) &&
	     quadoption (OPT_REPLYTO) != MUTT_YES)
    {
      /* There are quite a few mailing lists which set the Reply-To:
       * header field to the list address, which makes it quite impossible
       * to send a message to only the sender of the message.  This
       * provides a way to do that.
       */
      /* L10N:
         Asks whether the user respects the reply-to header.
         If she says no, mutt will reply to the from header's address instead. */
      snprintf (prompt, sizeof (prompt), _("Reply to %s%s?"),
		env->reply_to->mailbox, 
		env->reply_to->next?",...":"");
      switch (query_quadoption (OPT_REPLYTO, prompt))
      {
      case MUTT_YES:
	rfc822_append (to, env->reply_to, 0);
	break;

      case MUTT_NO:
	rfc822_append (to, env->from, 0);
	break;

      default:
	return (-1); /* abort */
      }
    }
    else
      rfc822_append (to, env->reply_to, 0);
  }
  else
    rfc822_append (to, env->from, 0);

  return (0);
}

int mutt_fetch_recips (ENVELOPE *out, ENVELOPE *in, int flags)
{
  char prompt[STRING];
  ADDRESS *tmp;
  int hmfupto = -1;

  if ((flags & (SENDLISTREPLY|SENDGROUPREPLY)) && in->mail_followup_to)
  {
    snprintf (prompt, sizeof (prompt), _("Follow-up to %s%s?"),
	      in->mail_followup_to->mailbox,
	      in->mail_followup_to->next ? ",..." : "");

    if ((hmfupto = query_quadoption (OPT_MFUPTO, prompt)) == -1)
      return -1;
  }

  if (flags & SENDLISTREPLY)
  {
    tmp = find_mailing_lists (in->to, in->cc);
    rfc822_append (&out->to, tmp, 0);
    rfc822_free_address (&tmp);

    if (in->mail_followup_to && hmfupto == MUTT_YES &&
        default_to (&out->cc, in, flags & SENDLISTREPLY, hmfupto) == -1)
      return (-1); /* abort */
  }
  else
  {
    if (default_to (&out->to, in, flags & SENDGROUPREPLY, hmfupto) == -1)
      return (-1); /* abort */

    if ((flags & SENDGROUPREPLY) && (!in->mail_followup_to || hmfupto != MUTT_YES))
    {
      /* if(!mutt_addr_is_user(in->to)) */
      rfc822_append (&out->cc, in->to, 1);
      rfc822_append (&out->cc, in->cc, 1);
    }
  }
  return 0;
}

LIST *mutt_make_references(ENVELOPE *e)
{
  LIST *t = NULL, *l = NULL;

  if (e->references)
    l = mutt_copy_list (e->references);
  else
    l = mutt_copy_list (e->in_reply_to);
  
  if (e->message_id)
  {
    t = mutt_new_list();
    t->data = safe_strdup(e->message_id);
    t->next = l;
    l = t;
  }
  
  return l;
}

void mutt_fix_reply_recipients (ENVELOPE *env)
{
  if (! option (OPTMETOO))
  {
    /* the order is important here.  do the CC: first so that if the
     * the user is the only recipient, it ends up on the TO: field
     */
    env->cc = remove_user (env->cc, (env->to == NULL));
    env->to = remove_user (env->to, (env->cc == NULL));
  }
  
  /* the CC field can get cluttered, especially with lists */
  env->to = mutt_remove_duplicates (env->to);
  env->cc = mutt_remove_duplicates (env->cc);
  env->cc = mutt_remove_xrefs (env->to, env->cc);
  
  if (env->cc && !env->to)
  {
    env->to = env->cc;
    env->cc = NULL;
  }
}

void mutt_make_forward_subject (ENVELOPE *env, CONTEXT *ctx, HEADER *cur)
{
  char buffer[STRING];

  /* set the default subject for the message. */
  mutt_make_string (buffer, sizeof (buffer), NONULL(ForwFmt), ctx, cur);
  mutt_str_replace (&env->subject, buffer);
}

void mutt_make_misc_reply_headers (ENVELOPE *env, CONTEXT *ctx,
				    HEADER *cur, ENVELOPE *curenv)
{
  /* This takes precedence over a subject that might have
   * been taken from a List-Post header.  Is that correct?
   */
  if (curenv->real_subj)
  {
    FREE (&env->subject);
    env->subject = safe_malloc (mutt_strlen (curenv->real_subj) + 5);
    sprintf (env->subject, "Re: %s", curenv->real_subj);	/* __SPRINTF_CHECKED__ */
  }
  else if (!env->subject)
    env->subject = safe_strdup ("Re: your mail");
}

void mutt_add_to_reference_headers (ENVELOPE *env, ENVELOPE *curenv, LIST ***pp, LIST ***qq)
{
  LIST **p = NULL, **q = NULL;

  if (pp) p = *pp;
  if (qq) q = *qq;
  
  if (!p) p = &env->references;
  if (!q) q = &env->in_reply_to;
  
  while (*p) p = &(*p)->next;
  while (*q) q = &(*q)->next;
  
  *p = mutt_make_references (curenv);
  
  if (curenv->message_id)
  {
    *q = mutt_new_list();
    (*q)->data = safe_strdup (curenv->message_id);
  }
  
  if (pp) *pp = p;
  if (qq) *qq = q;
  
}

static void 
mutt_make_reference_headers (ENVELOPE *curenv, ENVELOPE *env, CONTEXT *ctx)
{
  env->references = NULL;
  env->in_reply_to = NULL;
  
  if (!curenv)
  {
    HEADER *h;
    LIST **p = NULL, **q = NULL;
    int i;
    
    for(i = 0; i < ctx->vcount; i++)
    {
      h = ctx->hdrs[ctx->v2r[i]];
      if (h->tagged)
	mutt_add_to_reference_headers (env, h->env, &p, &q);
    }
  }
  else
    mutt_add_to_reference_headers (env, curenv, NULL, NULL);

  /* if there's more than entry in In-Reply-To (i.e. message has
     multiple parents), don't generate a References: header as it's
     discouraged by RfC2822, sect. 3.6.4 */
  if (ctx->tagged > 0 && env->in_reply_to && env->in_reply_to->next)
    mutt_free_list (&env->references);
}

static int
envelope_defaults (ENVELOPE *env, CONTEXT *ctx, HEADER *cur, int flags)
{
  ENVELOPE *curenv = NULL;
  int i = 0, tag = 0;

  if (!cur)
  {
    tag = 1;
    for (i = 0; i < ctx->vcount; i++)
      if (ctx->hdrs[ctx->v2r[i]]->tagged)
      {
	cur = ctx->hdrs[ctx->v2r[i]];
	curenv = cur->env;
	break;
      }

    if (!cur)
    {
      /* This could happen if the user tagged some messages and then did
       * a limit such that none of the tagged message are visible.
       */
      mutt_error _("No tagged messages are visible!");
      return (-1);
    }
  }
  else
    curenv = cur->env;

  if (flags & SENDREPLY)
  {
    if (tag)
    {
      HEADER *h;

      for (i = 0; i < ctx->vcount; i++)
      {
	h = ctx->hdrs[ctx->v2r[i]];
	if (h->tagged && mutt_fetch_recips (env, h->env, flags) == -1)
	  return -1;
      }
    }
    else if (mutt_fetch_recips (env, curenv, flags) == -1)
      return -1;

    if ((flags & SENDLISTREPLY) && !env->to)
    {
      mutt_error _("No mailing lists found!");
      return (-1);
    }

    mutt_make_misc_reply_headers (env, ctx, cur, curenv);
    mutt_make_reference_headers (tag ? NULL : curenv, env, ctx);
  }
  else if (flags & SENDFORWARD)
    mutt_make_forward_subject (env, ctx, cur);

  return (0);
}

static int
generate_body (FILE *tempfp,	/* stream for outgoing message */
	       HEADER *msg,	/* header for outgoing message */
	       int flags,	/* compose mode */
	       CONTEXT *ctx,	/* current mailbox */
	       HEADER *cur)	/* current message */
{
  int i;
  HEADER *h;
  BODY *tmp;

  if (flags & SENDREPLY)
  {
    if ((i = query_quadoption (OPT_INCLUDE, _("Include message in reply?"))) == -1)
      return (-1);

    if (i == MUTT_YES)
    {
      mutt_message _("Including quoted message...");
      if (!cur)
      {
	for (i = 0; i < ctx->vcount; i++)
	{
	  h = ctx->hdrs[ctx->v2r[i]];
	  if (h->tagged)
	  {
	    if (include_reply (ctx, h, tempfp) == -1)
	    {
	      mutt_error _("Could not include all requested messages!");
	      return (-1);
	    }
	    fputc ('\n', tempfp);
	  }
	}
      }
      else
	include_reply (ctx, cur, tempfp);

    }
  }
  else if (flags & SENDFORWARD)
  {
    if ((i = query_quadoption (OPT_MIMEFWD, _("Forward as attachment?"))) == MUTT_YES)
    {
      BODY *last = msg->content;

      mutt_message _("Preparing forwarded message...");
      
      while (last && last->next)
	last = last->next;

      if (cur)
      {
	tmp = mutt_make_message_attach (ctx, cur, 0);
	if (last)
	  last->next = tmp;
	else
	  msg->content = tmp;
      }
      else
      {
	for (i = 0; i < ctx->vcount; i++)
	{
	  if (ctx->hdrs[ctx->v2r[i]]->tagged)
	  {
	    tmp = mutt_make_message_attach (ctx, ctx->hdrs[ctx->v2r[i]], 0);
	    if (last)
	    {
	      last->next = tmp;
	      last = tmp;
	    }
	    else
	      last = msg->content = tmp;
	  }
	}
      }
    }
    else if (i != -1)
    {
      if (cur)
	include_forward (ctx, cur, tempfp);
      else
	for (i=0; i < ctx->vcount; i++)
	  if (ctx->hdrs[ctx->v2r[i]]->tagged)
	    include_forward (ctx, ctx->hdrs[ctx->v2r[i]], tempfp);
    }
    else if (i == -1)
      return -1;
  }
  /* if (WithCrypto && (flags & SENDKEY)) */
  else if ((WithCrypto & APPLICATION_PGP) && (flags & SENDKEY)) 
  {
    BODY *tmp;

    if ((WithCrypto & APPLICATION_PGP)
        && (tmp = crypt_pgp_make_key_attachment (NULL)) == NULL)
      return -1;

    tmp->next = msg->content;
    msg->content = tmp;
  }

  mutt_clear_error ();

  return (0);
}

void mutt_set_followup_to (ENVELOPE *e)
{
  ADDRESS *t = NULL;
  ADDRESS *from;

  /* 
   * Only generate the Mail-Followup-To if the user has requested it, and
   * it hasn't already been set
   */

  if (option (OPTFOLLOWUPTO) && !e->mail_followup_to)
  {
    if (mutt_is_list_cc (0, e->to, e->cc))
    {
      /* 
       * this message goes to known mailing lists, so create a proper
       * mail-followup-to header
       */

      t = rfc822_append (&e->mail_followup_to, e->to, 0);
      rfc822_append (&t, e->cc, 1);
    }

    /* remove ourselves from the mail-followup-to header */
    e->mail_followup_to = remove_user (e->mail_followup_to, 0);

    /*
     * If we are not subscribed to any of the lists in question,
     * re-add ourselves to the mail-followup-to header.  The 
     * mail-followup-to header generated is a no-op with group-reply,
     * but makes sure list-reply has the desired effect.
     */

    if (e->mail_followup_to && !mutt_is_list_recipient (0, e->to, e->cc))
    {
      if (e->reply_to)
	from = rfc822_cpy_adr (e->reply_to, 0);
      else if (e->from)
	from = rfc822_cpy_adr (e->from, 0);
      else
	from = mutt_default_from ();
      
      if (from)
      {
	/* Normally, this loop will not even be entered. */
	for (t = from; t && t->next; t = t->next)
	  ;
	
	t->next = e->mail_followup_to; 	/* t cannot be NULL at this point. */
	e->mail_followup_to = from;
      }
    }
    
    e->mail_followup_to = mutt_remove_duplicates (e->mail_followup_to);
    
  }
}


/* look through the recipients of the message we are replying to, and if
   we find an address that matches $alternates, we use that as the default
   from field */
static ADDRESS *set_reverse_name (ENVELOPE *env)
{
  ADDRESS *tmp;

  for (tmp = env->to; tmp; tmp = tmp->next)
  {
    if (mutt_addr_is_user (tmp))
      break;
  }
  if (!tmp)
  {
    for (tmp = env->cc; tmp; tmp = tmp->next)
    {
      if (mutt_addr_is_user (tmp))
	break;
    }
  }
  if (!tmp && mutt_addr_is_user (env->from))
    tmp = env->from;
  if (tmp)
  {
    tmp = rfc822_cpy_adr_real (tmp);
    /* when $reverse_realname is not set, clear the personal name so that it
     * may be set vi a reply- or send-hook.
     */
    if (!option (OPTREVREAL))
      FREE (&tmp->personal);
  }
  return (tmp);
}

ADDRESS *mutt_default_from (void)
{
  ADDRESS *adr;
  const char *fqdn = mutt_fqdn(1);

  /* 
   * Note: We let $from override $realname here.  Is this the right
   * thing to do? 
   */

  if (From)
    adr = rfc822_cpy_adr_real (From);
  else if (option (OPTUSEDOMAIN))
  {
    adr = rfc822_new_address ();
    adr->mailbox = safe_malloc (mutt_strlen (Username) + mutt_strlen (fqdn) + 2);
    sprintf (adr->mailbox, "%s@%s", NONULL(Username), NONULL(fqdn));	/* __SPRINTF_CHECKED__ */
  }
  else
  {
    adr = rfc822_new_address ();
    adr->mailbox = safe_strdup (NONULL(Username));
  }
  
  return (adr);
}

static int send_message (HEADER *msg)
{  
  char tempfile[_POSIX_PATH_MAX];
  FILE *tempfp;
  int i;
#ifdef USE_SMTP
  short old_write_bcc;
#endif
  
  /* Write out the message in MIME form. */
  mutt_mktemp (tempfile, sizeof (tempfile));
  if ((tempfp = safe_fopen (tempfile, "w")) == NULL)
    return (-1);

#ifdef USE_SMTP
  old_write_bcc = option (OPTWRITEBCC);
  if (SmtpUrl)
    unset_option (OPTWRITEBCC);
#endif
#ifdef MIXMASTER
  mutt_write_rfc822_header (tempfp, msg->env, msg->content, 0, msg->chain ? 1 : 0);
#endif
#ifndef MIXMASTER
  mutt_write_rfc822_header (tempfp, msg->env, msg->content, 0, 0);
#endif
#ifdef USE_SMTP
  if (old_write_bcc)
    set_option (OPTWRITEBCC);
#endif
  
  fputc ('\n', tempfp); /* tie off the header. */

  if ((mutt_write_mime_body (msg->content, tempfp) == -1))
  {
    safe_fclose (&tempfp);
    unlink (tempfile);
    return (-1);
  }
  
  if (fclose (tempfp) != 0)
  {
    mutt_perror (tempfile);
    unlink (tempfile);
    return (-1);
  }

#ifdef MIXMASTER
  if (msg->chain)
    return mix_send_message (msg->chain, tempfile);
#endif

#if USE_SMTP
  if (SmtpUrl)
      return mutt_smtp_send (msg->env->from, msg->env->to, msg->env->cc,
                             msg->env->bcc, tempfile,
                             (msg->content->encoding == ENC8BIT));
#endif /* USE_SMTP */

  i = mutt_invoke_sendmail (msg->env->from, msg->env->to, msg->env->cc, 
			    msg->env->bcc, tempfile,
                            (msg->content->encoding == ENC8BIT));
  return (i);
}

/* rfc2047 encode the content-descriptions */
void mutt_encode_descriptions (BODY *b, short recurse)
{
  BODY *t;

  for (t = b; t; t = t->next)
  {
    if (t->description)
    {
      rfc2047_encode_string (&t->description);
    }
    if (recurse && t->parts)
      mutt_encode_descriptions (t->parts, recurse);
  }
}

/* rfc2047 decode them in case of an error */
static void decode_descriptions (BODY *b)
{
  BODY *t;
  
  for (t = b; t; t = t->next)
  {
    if (t->description)
    {
      rfc2047_decode (&t->description);
    }
    if (t->parts)
      decode_descriptions (t->parts);
  }
}

static void fix_end_of_file (const char *data)
{
  FILE *fp;
  int c;
  
  if ((fp = safe_fopen (data, "a+")) == NULL)
    return;
  fseek (fp,-1,SEEK_END);
  if ((c = fgetc(fp)) != '\n')
    fputc ('\n', fp);
  safe_fclose (&fp);
}

int mutt_resend_message (FILE *fp, CONTEXT *ctx, HEADER *cur)
{
  HEADER *msg = mutt_new_header ();
  
  if (mutt_prepare_template (fp, ctx, msg, cur, 1) < 0)
    return -1;

  if (WithCrypto)
  {
    /* mutt_prepare_template doesn't always flip on an application bit.
     * so fix that here */
    if (!(msg->security & (APPLICATION_SMIME | APPLICATION_PGP)))
    {
      if ((WithCrypto & APPLICATION_SMIME) && option (OPTSMIMEISDEFAULT))
        msg->security |= APPLICATION_SMIME;
      else if (WithCrypto & APPLICATION_PGP)
        msg->security |= APPLICATION_PGP;
      else
        msg->security |= APPLICATION_SMIME;
    }

    if (option (OPTCRYPTOPPORTUNISTICENCRYPT))
    {
      msg->security |= OPPENCRYPT;
      crypt_opportunistic_encrypt(msg);
    }
  }

  return ci_send_message (SENDRESEND, msg, NULL, ctx, cur);
}

static int is_reply (HEADER *reply, HEADER *orig)
{
  return mutt_find_list (orig->env->references, reply->env->message_id) ||
         mutt_find_list (orig->env->in_reply_to, reply->env->message_id);
}

static int has_recips (ADDRESS *a)
{
  int c = 0;

  for ( ; a; a = a->next)
  {
    if (!a->mailbox || a->group)
      continue;
    c++;
  }
  return c;
}

/*
 * Returns 0 if the message was successfully sent
 *        -1 if the message was aborted or an error occurred
 *         1 if the message was postponed
 */
int
ci_send_message (int flags,		/* send mode */
		 HEADER *msg,		/* template to use for new message */
		 char *tempfile,	/* file specified by -i or -H */
		 CONTEXT *ctx,		/* current mailbox */
		 HEADER *cur)		/* current message */
{
  char buffer[LONG_STRING];
  char fcc[_POSIX_PATH_MAX] = ""; /* where to copy this message */
  FILE *tempfp = NULL;
  BODY *pbody;
  int i, killfrom = 0;
  int fcc_error = 0;
  int free_clear_content = 0;

  BODY *save_content = NULL;
  BODY *clear_content = NULL;
  char *pgpkeylist = NULL;
  /* save current value of "pgp_sign_as"  and "smime_default_key" */
  char *pgp_signas = NULL;
  char *smime_default_key = NULL;
  char *tag = NULL, *err = NULL;
  char *ctype;

  int rv = -1;
  
  if (!flags && !msg && quadoption (OPT_RECALL) != MUTT_NO &&
      mutt_num_postponed (1))
  {
    /* If the user is composing a new message, check to see if there
     * are any postponed messages first.
     */
    if ((i = query_quadoption (OPT_RECALL, _("Recall postponed message?"))) == -1)
      return rv;

    if(i == MUTT_YES)
      flags |= SENDPOSTPONED;
  }
  
  
  if (flags & SENDPOSTPONED)
  {
    if (WithCrypto & APPLICATION_PGP)
      pgp_signas = safe_strdup(PgpSignAs);
    if (WithCrypto & APPLICATION_SMIME)
      smime_default_key = safe_strdup(SmimeDefaultKey);
  }

  /* Delay expansion of aliases until absolutely necessary--shouldn't
   * be necessary unless we are prompting the user or about to execute a
   * send-hook.
   */

  if (!msg)
  {
    msg = mutt_new_header ();

    if (flags == SENDPOSTPONED)
    {
      if ((flags = mutt_get_postponed (ctx, msg, &cur, fcc, sizeof (fcc))) < 0)
	goto cleanup;
    }

    if (flags & (SENDPOSTPONED|SENDRESEND))
    {
      if ((tempfp = safe_fopen (msg->content->filename, "a+")) == NULL)
      {
	mutt_perror (msg->content->filename);
	goto cleanup;
      }
    }

    if (!msg->env)
      msg->env = mutt_new_envelope ();
  }

  /* Parse and use an eventual list-post header */
  if ((flags & SENDLISTREPLY) 
      && cur && cur->env && cur->env->list_post) 
  {
    /* Use any list-post header as a template */
    url_parse_mailto (msg->env, NULL, cur->env->list_post);
    /* We don't let them set the sender's address. */
    rfc822_free_address (&msg->env->from);
  }
  
  if (! (flags & (SENDKEY | SENDPOSTPONED | SENDRESEND)))
  {
    /* When SENDDRAFTFILE is set, the caller has already
     * created the "parent" body structure.
     */
    if (! (flags & SENDDRAFTFILE))
    {
      pbody = mutt_new_body ();
      pbody->next = msg->content; /* don't kill command-line attachments */
      msg->content = pbody;

      if (!(ctype = safe_strdup (ContentType)))
        ctype = safe_strdup ("text/plain");
      mutt_parse_content_type (ctype, msg->content);
      FREE (&ctype);
      msg->content->unlink = 1;
      msg->content->use_disp = 0;
      msg->content->disposition = DISPINLINE;

      if (!tempfile)
      {
        mutt_mktemp (buffer, sizeof (buffer));
        tempfp = safe_fopen (buffer, "w+");
        msg->content->filename = safe_strdup (buffer);
      }
      else
      {
        tempfp = safe_fopen (tempfile, "a+");
        msg->content->filename = safe_strdup (tempfile);
      }
    }
    else
      tempfp = safe_fopen (msg->content->filename, "a+");

    if (!tempfp)
    {
      dprint(1,(debugfile, "newsend_message: can't create tempfile %s (errno=%d)\n", msg->content->filename, errno));
      mutt_perror (msg->content->filename);
      goto cleanup;
    }
  }

  /* this is handled here so that the user can match ~f in send-hook */
  if (cur && option (OPTREVNAME) && !(flags & (SENDPOSTPONED|SENDRESEND)))
  {
    /* we shouldn't have to worry about freeing `msg->env->from' before
     * setting it here since this code will only execute when doing some
     * sort of reply.  the pointer will only be set when using the -H command
     * line option.
     *
     * We shouldn't have to worry about alias expansion here since we are
     * either replying to a real or postponed message, therefore no aliases
     * should exist since the user has not had the opportunity to add
     * addresses to the list.  We just have to ensure the postponed messages
     * have their aliases expanded.
     */

    msg->env->from = set_reverse_name (cur->env);
  }

  if (! (flags & (SENDPOSTPONED|SENDRESEND)) &&
      ! ((flags & SENDDRAFTFILE) && option (OPTRESUMEDRAFTFILES)))
  {
    if ((flags & (SENDREPLY | SENDFORWARD)) && ctx &&
	envelope_defaults (msg->env, ctx, cur, flags) == -1)
      goto cleanup;

    if (option (OPTHDRS))
      process_user_recips (msg->env);

    /* Expand aliases and remove duplicates/crossrefs */
    mutt_expand_aliases_env (msg->env);
    
    if (flags & SENDREPLY)
      mutt_fix_reply_recipients (msg->env);

    if (! (flags & (SENDMAILX|SENDBATCH)) &&
	! (option (OPTAUTOEDIT) && option (OPTEDITHDRS)) &&
	! ((flags & SENDREPLY) && option (OPTFASTREPLY)))
    {
      if (edit_envelope (msg->env) == -1)
	goto cleanup;
    }

    /* the from address must be set here regardless of whether or not
     * $use_from is set so that the `~P' (from you) operator in send-hook
     * patterns will work.  if $use_from is unset, the from address is killed
     * after send-hooks are evaluated */

    if (!msg->env->from)
    {
      msg->env->from = mutt_default_from ();
      killfrom = 1;
    }

    if ((flags & SENDREPLY) && cur)
    {
      /* change setting based upon message we are replying to */
      mutt_message_hook (ctx, cur, MUTT_REPLYHOOK);

      /*
       * set the replied flag for the message we are generating so that the
       * user can use ~Q in a send-hook to know when reply-hook's are also
       * being used.
       */
      msg->replied = 1;
    }

    /* change settings based upon recipients */
    
    mutt_message_hook (NULL, msg, MUTT_SENDHOOK);

    /*
     * Unset the replied flag from the message we are composing since it is
     * no longer required.  This is done here because the FCC'd copy of
     * this message was erroneously get the 'R'eplied flag when stored in
     * a maildir-style mailbox.
     */
    msg->replied = 0;

    if (! (flags & SENDKEY))
    {
      if (option (OPTTEXTFLOWED) && msg->content->type == TYPETEXT && !ascii_strcasecmp (msg->content->subtype, "plain"))
        mutt_set_parameter ("format", "flowed", &msg->content->parameter);
    }

    /* $use_from and/or $from might have changed in a send-hook */
    if (killfrom)
    {
      rfc822_free_address (&msg->env->from);
      if (option (OPTUSEFROM) && !(flags & (SENDPOSTPONED|SENDRESEND)))
	msg->env->from = mutt_default_from ();
      killfrom = 0;
    }

    if (option (OPTHDRS))
      process_user_header (msg->env);

    if (flags & SENDBATCH)
       mutt_copy_stream (stdin, tempfp);

    if (option (OPTSIGONTOP) && ! (flags & (SENDMAILX|SENDKEY|SENDBATCH))
	&& Editor && mutt_strcmp (Editor, "builtin") != 0)
      append_signature (tempfp);

    /* include replies/forwarded messages, unless we are given a template */
    if (!tempfile && (ctx || !(flags & (SENDREPLY|SENDFORWARD)))
	&& generate_body (tempfp, msg, flags, ctx, cur) == -1)
      goto cleanup;

    if (!option (OPTSIGONTOP) && ! (flags & (SENDMAILX|SENDKEY|SENDBATCH))
	&& Editor && mutt_strcmp (Editor, "builtin") != 0)
      append_signature (tempfp);
  }
  
  /* 
   * This hook is even called for postponed messages, and can, e.g., be
   * used for setting the editor, the sendmail path, or the
   * envelope sender.
   */
  mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);

  /* wait until now to set the real name portion of our return address so
     that $realname can be set in a send-hook */
  if (msg->env->from && !msg->env->from->personal
      && !(flags & (SENDRESEND|SENDPOSTPONED)))
    msg->env->from->personal = safe_strdup (Realname);

  if (!((WithCrypto & APPLICATION_PGP) && (flags & SENDKEY)))
    safe_fclose (&tempfp);

  if (flags & SENDMAILX)
  {
    if (mutt_builtin_editor (msg->content->filename, msg, cur) == -1)
      goto cleanup;
  }
  else if (! (flags & SENDBATCH))
  {
    struct stat st;
    time_t mtime = mutt_decrease_mtime (msg->content->filename, NULL);

    mutt_update_encoding (msg->content);

    /*
     * Select whether or not the user's editor should be called now.  We
     * don't want to do this when:
     * 1) we are sending a key/cert
     * 2) we are forwarding a message and the user doesn't want to edit it.
     *    This is controlled by the quadoption $forward_edit.  However, if
     *    both $edit_headers and $autoedit are set, we want to ignore the
     *    setting of $forward_edit because the user probably needs to add the
     *    recipients.
     */
    if (! (flags & SENDKEY) &&
	((flags & SENDFORWARD) == 0 ||
	 (option (OPTEDITHDRS) && option (OPTAUTOEDIT)) ||
	 query_quadoption (OPT_FORWEDIT, _("Edit forwarded message?")) == MUTT_YES))
    {
      /* If the this isn't a text message, look for a mailcap edit command */
      if (mutt_needs_mailcap (msg->content))
      {
	if (!mutt_edit_attachment (msg->content))
          goto cleanup;
      }
      else if (!Editor || mutt_strcmp ("builtin", Editor) == 0)
	mutt_builtin_editor (msg->content->filename, msg, cur);
      else if (option (OPTEDITHDRS))
      {
	mutt_env_to_local (msg->env);
	mutt_edit_headers (Editor, msg->content->filename, msg, fcc, sizeof (fcc));
	mutt_env_to_intl (msg->env, NULL, NULL);
      }
      else
      {
	mutt_edit_file (Editor, msg->content->filename);
	if (stat (msg->content->filename, &st) == 0)
	{
	  if (mtime != st.st_mtime)
	    fix_end_of_file (msg->content->filename);
	}
	else
	  mutt_perror (msg->content->filename);
      }
      
      /* If using format=flowed, perform space stuffing.  Avoid stuffing when
       * recalling a postponed message where the stuffing was already
       * performed.  If it has already been performed, the format=flowed
       * parameter will be present.
       */
      if (option (OPTTEXTFLOWED) && msg->content->type == TYPETEXT && !ascii_strcasecmp("plain", msg->content->subtype))
      {
	char *p = mutt_get_parameter("format", msg->content->parameter);
	if (ascii_strcasecmp("flowed", NONULL(p)))
	  rfc3676_space_stuff (msg);
      }

      mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
    }

    if (! (flags & (SENDPOSTPONED | SENDFORWARD | SENDKEY | SENDRESEND | SENDDRAFTFILE)))
    {
      if (stat (msg->content->filename, &st) == 0)
      {
	/* if the file was not modified, bail out now */
	if (mtime == st.st_mtime && !msg->content->next &&
	    query_quadoption (OPT_ABORT, _("Abort unmodified message?")) == MUTT_YES)
	{
	  mutt_message _("Aborted unmodified message.");
	  goto cleanup;
	}
      }
      else
	mutt_perror (msg->content->filename);
    }
  }

  /* 
   * Set the message security unless:
   * 1) crypto support is not enabled (WithCrypto==0)
   * 2) pgp: header field was present during message editing with $edit_headers (msg->security != 0)
   * 3) we are resending a message
   * 4) we are recalling a postponed message (don't override the user's saved settings)
   * 5) we are in mailx mode
   * 6) we are in batch mode
   *
   * This is done after allowing the user to edit the message so that security
   * settings can be configured with send2-hook and $edit_headers.
   */
  if (WithCrypto && (msg->security == 0) && !(flags & (SENDBATCH | SENDMAILX | SENDPOSTPONED | SENDRESEND)))
  {
    if (option (OPTCRYPTAUTOSIGN))
      msg->security |= SIGN;
    if (option (OPTCRYPTAUTOENCRYPT))
      msg->security |= ENCRYPT;
    if (option (OPTCRYPTREPLYENCRYPT) && cur && (cur->security & ENCRYPT))
      msg->security |= ENCRYPT;
    if (option (OPTCRYPTREPLYSIGN) && cur && (cur->security & SIGN))
      msg->security |= SIGN;
    if (option (OPTCRYPTREPLYSIGNENCRYPTED) && cur && (cur->security & ENCRYPT))
      msg->security |= SIGN;
    if ((WithCrypto & APPLICATION_PGP) &&
        ((msg->security & (ENCRYPT | SIGN)) || option (OPTCRYPTOPPORTUNISTICENCRYPT)))
    {
      if (option (OPTPGPAUTOINLINE))
	msg->security |= INLINE;
      if (option (OPTPGPREPLYINLINE) && cur && (cur->security & INLINE))
	msg->security |= INLINE;
    }

    if (msg->security || option (OPTCRYPTOPPORTUNISTICENCRYPT))
    {
      /* 
       * When replying / forwarding, use the original message's
       * crypto system.  According to the documentation,
       * smime_is_default should be disregarded here.
       * 
       * Problem: At least with forwarding, this doesn't really
       * make much sense. Should we have an option to completely
       * disable individual mechanisms at run-time?
       */
      if (cur)
      {
	if ((WithCrypto & APPLICATION_PGP) && option (OPTCRYPTAUTOPGP) 
	    && (cur->security & APPLICATION_PGP))
	  msg->security |= APPLICATION_PGP;
	else if ((WithCrypto & APPLICATION_SMIME) && option (OPTCRYPTAUTOSMIME)
	    && (cur->security & APPLICATION_SMIME))
	  msg->security |= APPLICATION_SMIME;
      }

      /*
       * No crypto mechanism selected? Use availability + smime_is_default
       * for the decision. 
       */
      if (!(msg->security & (APPLICATION_SMIME | APPLICATION_PGP)))
      {
	if ((WithCrypto & APPLICATION_SMIME) && option (OPTCRYPTAUTOSMIME) 
	    && option (OPTSMIMEISDEFAULT))
	  msg->security |= APPLICATION_SMIME;
	else if ((WithCrypto & APPLICATION_PGP) && option (OPTCRYPTAUTOPGP))
	  msg->security |= APPLICATION_PGP;
	else if ((WithCrypto & APPLICATION_SMIME) && option (OPTCRYPTAUTOSMIME))
	  msg->security |= APPLICATION_SMIME;
      }
    }

    /* opportunistic encrypt relys on SMIME or PGP already being selected */
    if (option (OPTCRYPTOPPORTUNISTICENCRYPT))
    {
      /* If something has already enabled encryption, e.g. OPTCRYPTAUTOENCRYPT
       * or OPTCRYPTREPLYENCRYPT, then don't enable opportunistic encrypt for
       * the message.
       */
      if (! (msg->security & ENCRYPT))
      {
        msg->security |= OPPENCRYPT;
        crypt_opportunistic_encrypt(msg);
      }
    }

    /* No permissible mechanisms found.  Don't sign or encrypt. */
    if (!(msg->security & (APPLICATION_SMIME|APPLICATION_PGP)))
      msg->security = 0;
  }

  /* specify a default fcc.  if we are in batchmode, only save a copy of
   * the message if the value of $copy is yes or ask-yes */

  if (!fcc[0] && !(flags & (SENDPOSTPONEDFCC)) && (!(flags & SENDBATCH) || (quadoption (OPT_COPY) & 0x1)))
  {
    /* set the default FCC */
    if (!msg->env->from)
    {
      msg->env->from = mutt_default_from ();
      killfrom = 1; /* no need to check $use_from because if the user specified
		       a from address it would have already been set by now */
    }
    mutt_select_fcc (fcc, sizeof (fcc), msg);
    if (killfrom)
    {
      rfc822_free_address (&msg->env->from);
      killfrom = 0;
    }
  }

  
  mutt_update_encoding (msg->content);

  if (! (flags & (SENDMAILX | SENDBATCH)))
  {
main_loop:

    fcc_error = 0; /* reset value since we may have failed before */
    mutt_pretty_mailbox (fcc, sizeof (fcc));
    i = mutt_compose_menu (msg, fcc, sizeof (fcc), cur,
                           (flags & SENDNOFREEHEADER ? MUTT_COMPOSE_NOFREEHEADER : 0));
    if (i == -1)
    {
      /* abort */
      mutt_message _("Mail not sent.");
      goto cleanup;
    }
    else if (i == 1)
    {
      /* postpone the message until later. */
      if (msg->content->next)
	msg->content = mutt_make_multipart (msg->content);

      if (WithCrypto && option (OPTPOSTPONEENCRYPT) && PostponeEncryptAs
          && (msg->security & ENCRYPT))
      {
        int is_signed = msg->security & SIGN;
        if (is_signed)
          msg->security &= ~SIGN;

        pgpkeylist = safe_strdup (PostponeEncryptAs);
        if (mutt_protect (msg, pgpkeylist) == -1)
        {
          if (is_signed)
            msg->security |= SIGN;
          FREE (&pgpkeylist);
          msg->content = mutt_remove_multipart (msg->content);
          goto main_loop;
        }

        if (is_signed)
          msg->security |= SIGN;
        FREE (&pgpkeylist);
      }

      /*
       * make sure the message is written to the right part of a maildir 
       * postponed folder.
       */
      msg->read = 0; msg->old = 0;

      mutt_encode_descriptions (msg->content, 1);
      mutt_prepare_envelope (msg->env, 0);
      mutt_env_to_intl (msg->env, NULL, NULL);	/* Handle bad IDNAs the next time. */

      if (!Postponed || mutt_write_fcc (NONULL (Postponed), msg, (cur && (flags & SENDREPLY)) ? cur->env->message_id : NULL, 1, fcc) < 0)
      {
	msg->content = mutt_remove_multipart (msg->content);
	decode_descriptions (msg->content);
	mutt_unprepare_envelope (msg->env);
	goto main_loop;
      }
      mutt_update_num_postponed ();
      mutt_message _("Message postponed.");
      rv = 1;
      goto cleanup;
    }
  }

  if (!has_recips (msg->env->to) && !has_recips (msg->env->cc) &&
      !has_recips (msg->env->bcc))
  {
    if (! (flags & SENDBATCH))
    {
      mutt_error _("No recipients are specified!");
      goto main_loop;
    }
    else
    {
      puts _("No recipients were specified.");
      goto cleanup;
    }
  }

  if (mutt_env_to_intl (msg->env, &tag, &err))
  {
    mutt_error (_("Bad IDN in \"%s\": '%s'"), tag, err);
    FREE (&err);
    if (!(flags & SENDBATCH))
      goto main_loop;
    else 
      goto cleanup;
  }
  
  if (!msg->env->subject && ! (flags & SENDBATCH) &&
      (i = query_quadoption (OPT_SUBJECT, _("No subject, abort sending?"))) != MUTT_NO)
  {
    /* if the abort is automatic, print an error message */
    if (quadoption (OPT_SUBJECT) == MUTT_YES)
      mutt_error _("No subject specified.");
    goto main_loop;
  }

  if (msg->content->next)
    msg->content = mutt_make_multipart (msg->content);

  /* 
   * Ok, we need to do it this way instead of handling all fcc stuff in
   * one place in order to avoid going to main_loop with encoded "env"
   * in case of error.  Ugh.
   */

  mutt_encode_descriptions (msg->content, 1);
  
  /*
   * Make sure that clear_content and free_clear_content are
   * properly initialized -- we may visit this particular place in
   * the code multiple times, including after a failed call to
   * mutt_protect().
   */
  
  clear_content = NULL;
  free_clear_content = 0;
  
  if (WithCrypto)
  {
    if (msg->security & (ENCRYPT | SIGN))
    {
      /* save the decrypted attachments */
      clear_content = msg->content;
  
      if ((crypt_get_keys (msg, &pgpkeylist, 0) == -1) ||
          mutt_protect (msg, pgpkeylist) == -1)
      {
        msg->content = mutt_remove_multipart (msg->content);
        
	FREE (&pgpkeylist);
        
        decode_descriptions (msg->content);
        goto main_loop;
      }
      mutt_encode_descriptions (msg->content, 0);
    }
  
    /* 
     * at this point, msg->content is one of the following three things:
     * - multipart/signed.  In this case, clear_content is a child.
     * - multipart/encrypted.  In this case, clear_content exists
     *   independently
     * - application/pgp.  In this case, clear_content exists independently.
     * - something else.  In this case, it's the same as clear_content.
     */
  
    /* This is ugly -- lack of "reporting back" from mutt_protect(). */
    
    if (clear_content && (msg->content != clear_content)
        && (msg->content->parts != clear_content))
      free_clear_content = 1;
  }

  if (!option (OPTNOCURSES) && !(flags & SENDMAILX))
    mutt_message _("Sending message...");

  mutt_prepare_envelope (msg->env, 1);

  /* save a copy of the message, if necessary. */

  mutt_expand_path (fcc, sizeof (fcc));

  
  /* Don't save a copy when we are in batch-mode, and the FCC
   * folder is on an IMAP server: This would involve possibly lots
   * of user interaction, which is not available in batch mode. 
   * 
   * Note: A patch to fix the problems with the use of IMAP servers
   * from non-curses mode is available from Brendan Cully.  However, 
   * I'd like to think a bit more about this before including it.
   */

#ifdef USE_IMAP
  if ((flags & SENDBATCH) && fcc[0] && mx_is_imap (fcc))
    fcc[0] = '\0';
#endif

  if (*fcc && mutt_strcmp ("/dev/null", fcc) != 0)
  {
    BODY *tmpbody = msg->content;
    BODY *save_sig = NULL;
    BODY *save_parts = NULL;

    if (WithCrypto && (msg->security & (ENCRYPT | SIGN)) && option (OPTFCCCLEAR))
      msg->content = clear_content;

    /* check to see if the user wants copies of all attachments */
    if (query_quadoption (OPT_FCCATTACH, _("Save attachments in Fcc?")) != MUTT_YES &&
	msg->content->type == TYPEMULTIPART)
    {
      if (WithCrypto
          && (msg->security & (ENCRYPT | SIGN))
          && (mutt_strcmp (msg->content->subtype, "encrypted") == 0 ||
              mutt_strcmp (msg->content->subtype, "signed") == 0))
      {
	if (clear_content->type == TYPEMULTIPART)
	{
	  if(!(msg->security & ENCRYPT) && (msg->security & SIGN))
	  {
	    /* save initial signature and attachments */
	    save_sig = msg->content->parts->next;
	    save_parts = clear_content->parts->next;
	  }

	  /* this means writing only the main part */
	  msg->content = clear_content->parts;

	  if (mutt_protect (msg, pgpkeylist) == -1)
	  {
	    /* we can't do much about it at this point, so
	     * fallback to saving the whole thing to fcc
	     */
	    msg->content = tmpbody;
	    save_sig = NULL;
	    goto full_fcc;
	  }

	  save_content = msg->content;
	}
      }
      else
	msg->content = msg->content->parts;
    }

full_fcc:
    if (msg->content)
    {
      /* update received time so that when storing to a mbox-style folder
       * the From_ line contains the current time instead of when the
       * message was first postponed.
       */
      msg->received = time (NULL);
      if (mutt_write_fcc (fcc, msg, NULL, 0, NULL) == -1)
      {
	/*
	 * Error writing FCC, we should abort sending.
	 */
	fcc_error = 1;
      }
    }

    msg->content = tmpbody;

    if (WithCrypto && save_sig)
    {
      /* cleanup the second signature structures */
      if (save_content->parts)
      {
	mutt_free_body (&save_content->parts->next);
	save_content->parts = NULL;
      }
      mutt_free_body (&save_content);

      /* restore old signature and attachments */
      msg->content->parts->next = save_sig;
      msg->content->parts->parts->next = save_parts;
    }
    else if (WithCrypto && save_content)
    {
      /* destroy the new encrypted body. */
      mutt_free_body (&save_content);
    }

  }


  /*
   * Don't attempt to send the message if the FCC failed.  Just pretend
   * the send failed as well so we give the user a chance to fix the
   * error.
   */
  if (fcc_error || (i = send_message (msg)) < 0)
  {
    if (!(flags & SENDBATCH))
    {
      if (!WithCrypto)
        ;
      else if ((msg->security & ENCRYPT) || 
               ((msg->security & SIGN)
                && msg->content->type == TYPEAPPLICATION))
      {
	mutt_free_body (&msg->content); /* destroy PGP data */
	msg->content = clear_content;	/* restore clear text. */
      }
      else if ((msg->security & SIGN) && msg->content->type == TYPEMULTIPART)
      {
	mutt_free_body (&msg->content->parts->next);	     /* destroy sig */
	msg->content = mutt_remove_multipart (msg->content); 
      }

      msg->content = mutt_remove_multipart (msg->content);
      decode_descriptions (msg->content);
      mutt_unprepare_envelope (msg->env);
      goto main_loop;
    }
    else
    {
      puts _("Could not send the message.");
      goto cleanup;
    }
  }
  else if (!option (OPTNOCURSES) && ! (flags & SENDMAILX))
    mutt_message (i == 0 ? _("Mail sent.") : _("Sending in background."));

  if (WithCrypto && (msg->security & ENCRYPT))
    FREE (&pgpkeylist);
  
  if (WithCrypto && free_clear_content)
    mutt_free_body (&clear_content);

  /* set 'replied' flag only if the user didn't change/remove
     In-Reply-To: and References: headers during edit */
  if (flags & SENDREPLY)
  {
    if (cur && ctx)
      mutt_set_flag (ctx, cur, MUTT_REPLIED, is_reply (cur, msg));
    else if (!(flags & SENDPOSTPONED) && ctx && ctx->tagged)
    {
      for (i = 0; i < ctx->vcount; i++)
	if (ctx->hdrs[ctx->v2r[i]]->tagged)
	  mutt_set_flag (ctx, ctx->hdrs[ctx->v2r[i]], MUTT_REPLIED,
			 is_reply (ctx->hdrs[ctx->v2r[i]], msg));
    }
  }


  rv = 0;
  
cleanup:

  if (flags & SENDPOSTPONED)
  {
    if (WithCrypto & APPLICATION_PGP)
    {
      FREE (&PgpSignAs);
      PgpSignAs = pgp_signas;
    }
    if (WithCrypto & APPLICATION_SMIME)
    {
      FREE (&SmimeDefaultKey);
      SmimeDefaultKey = smime_default_key;
    }
  }
   
  safe_fclose (&tempfp);
  if (! (flags & SENDNOFREEHEADER))
    mutt_free_header (&msg);
  
  return rv;
}

/* vim: set sw=2: */
