static const char rcsid[]="$Id$";
/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

#include "mutt.h"
#include "mutt_curses.h"
#include "rfc2047.h"
#include "keymap.h"
#include "mime.h"
#include "mailbox.h"
#include "copy.h"
#include "mx.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>

#ifdef _PGPPATH
#include "pgp.h"
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
    fclose (tmpfp);
    if (thepid != -1)
      mutt_wait_filter (thepid);
  }
}

/* compare two e-mail addresses and return 1 if they are equivalent */
static int mutt_addrcmp (ADDRESS *a, ADDRESS *b)
{
  if (!a->mailbox || !b->mailbox)
    return 0;
  if (strcasecmp (a->mailbox, b->mailbox))
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
static ADDRESS *mutt_remove_xrefs (ADDRESS *a, ADDRESS *b)
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
      if (mutt_is_mail_list (t))
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

  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), *a);
  if (mutt_get_field (field, buf, sizeof (buf), M_ALIAS) != 0)
    return (-1);
  rfc822_free_address (a);
  *a = mutt_expand_aliases (mutt_parse_adrlist (NULL, buf));
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
    char *p;

    buf[0] = 0;
    for (; uh; uh = uh->next)
    {
      if (strncasecmp ("subject:", uh->data, 8) == 0)
      {
	p = uh->data + 8;
	SKIPWS (p);
	strncpy (buf, p, sizeof (buf));
      }
    }
  }
  
  if (mutt_get_field ("Subject: ", buf, sizeof (buf), 0) != 0 ||
      (!buf[0] && query_quadoption (OPT_SUBJECT, _("No subject, abort?")) != 0))
  {
    mutt_message _("No subject, aborting.");
    return (-1);
  }
  safe_free ((void **) &en->subject);
  en->subject = safe_strdup (buf);

  return 0;
}

static void process_user_recips (ENVELOPE *env)
{
  LIST *uh = UserHeader;

  for (; uh; uh = uh->next)
  {
    if (strncasecmp ("to:", uh->data, 3) == 0)
      env->to = rfc822_parse_adrlist (env->to, uh->data + 3);
    else if (strncasecmp ("cc:", uh->data, 3) == 0)
      env->cc = rfc822_parse_adrlist (env->cc, uh->data + 3);
    else if (strncasecmp ("bcc:", uh->data, 4) == 0)
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
    if (strncasecmp ("from:", uh->data, 5) == 0)
    {
      /* User has specified a default From: address.  Remove default address */
      rfc822_free_address (&env->from);
      env->from = rfc822_parse_adrlist (env->from, uh->data + 5);
    }
    else if (strncasecmp ("reply-to:", uh->data, 9) == 0)
    {
      rfc822_free_address (&env->reply_to);
      env->reply_to = rfc822_parse_adrlist (env->reply_to, uh->data + 9);
    }
    else if (strncasecmp ("to:", uh->data, 3) != 0 &&
	     strncasecmp ("cc:", uh->data, 3) != 0 &&
	     strncasecmp ("bcc:", uh->data, 4) != 0 &&
	     strncasecmp ("subject:", uh->data, 8) != 0)
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

static int include_forward (CONTEXT *ctx, HEADER *cur, FILE *out)
{
  char buffer[STRING];
  int chflags = CH_DECODE, cmflags = 0;



#ifdef _PGPPATH
  if ((cur->pgp & PGPENCRYPT) && option (OPTFORWDECODE))
  {
    /* make sure we have the user's passphrase before proceeding... */
    pgp_valid_passphrase ();
  }
#endif /* _PGPPATH */



  fputs ("----- Forwarded message from ", out);
  buffer[0] = 0;
  rfc822_write_address (buffer, sizeof (buffer), cur->env->from);
  fputs (buffer, out);
  fputs (" -----\n\n", out);
  if (option (OPTFORWDECODE))
  {
    cmflags |= M_CM_DECODE;
    chflags |= CH_WEED;
  }
  if (option (OPTFORWQUOTE))
    cmflags |= M_CM_PREFIX;
  mutt_parse_mime_message (ctx, cur);
  mutt_copy_message (out, ctx, cur, cmflags, chflags);
  fputs ("\n----- End forwarded message -----\n", out);
  return 0;
}

static int include_reply (CONTEXT *ctx, HEADER *cur, FILE *out)
{
  char buffer[STRING];
  int flags = M_CM_PREFIX | M_CM_DECODE;



#ifdef _PGPPATH
  if (cur->pgp)
  {
    if (cur->pgp & PGPENCRYPT)
    {
      /* make sure we have the user's passphrase before proceeding... */
      pgp_valid_passphrase ();
    }
  }
#endif /* _PGPPATH */



  if (Attribution)
  {
    mutt_make_string (buffer, sizeof (buffer), Attribution, ctx, cur);
    fputs (buffer, out);
    fputc ('\n', out);
  }
  if (!option (OPTHEADER))
    flags |= M_CM_NOHEADER;
  mutt_parse_mime_message (ctx, cur);
  mutt_copy_message (out, ctx, cur, flags, CH_DECODE);
  if (PostIndentString)
  {
    mutt_make_string (buffer, sizeof (buffer), PostIndentString, ctx, cur);
    fputs (buffer, out);
    fputc ('\n', out);
  }
  return 0;
}

static int default_to (ADDRESS **to, ENVELOPE *env, int group)
{
  char prompt[STRING];
  int i = 0;

  if (group && env->mail_followup_to)
  {
    rfc822_append (to, env->mail_followup_to);
    return 0;
  }

  if (mutt_addr_is_user (env->from))
  {
    /* mail is from the user, assume replying to recipients */
    rfc822_append (to, env->to);
  }
  else if (env->reply_to)
  {
    if (option (OPTIGNORELISTREPLYTO) &&
	mutt_is_mail_list (env->reply_to) &&
	(mutt_addrsrc (env->reply_to, env->to) ||
	mutt_addrsrc (env->reply_to, env->cc)))
    {
      /* If the Reply-To: address is a mailing list, assume that it was
       * put there by the mailing list, and use the From: address
       */
      rfc822_append (to, env->from);
    }
    else if (!mutt_addrcmp (env->from, env->reply_to) &&
	     quadoption (OPT_REPLYTO) != M_YES)
    {
      /* There are quite a few mailing lists which set the Reply-To:
       * header field to the list address, which makes it quite impossible
       * to send a message to only the sender of the message.  This
       * provides a way to do that.
       */
      snprintf (prompt, sizeof (prompt), _("Reply to %s?"), env->reply_to->mailbox);
      if ((i = query_quadoption (OPT_REPLYTO, prompt)) == M_YES)
	rfc822_append (to, env->reply_to);
      else if (i == M_NO)
	rfc822_append (to, env->from);
      else
	return (-1); /* abort */
    }
    else
      rfc822_append (to, env->reply_to);
  }
  else
    rfc822_append (to, env->from);

  return (0);
}

static LIST *make_references(ENVELOPE *e)
{
  LIST *t, *l;
  
  l = mutt_copy_list(e->references);
  
  if(e->message_id)
  {
    t = mutt_new_list();
    t->data = safe_strdup(e->message_id);
    t->next = l;
    l = t;
  }
  
  return l;
}

static int fetch_recips (ENVELOPE *out, ENVELOPE *in, int flags)
{
  ADDRESS *tmp;
  if (flags & SENDLISTREPLY)
  {
    tmp = find_mailing_lists (in->to, in->cc);
    rfc822_append (&out->to, tmp);
    rfc822_free_address (&tmp);
  }
  else
  {
    if (default_to (&out->to, in, flags & SENDGROUPREPLY) == -1)
      return (-1); /* abort */

    if ((flags & SENDGROUPREPLY) && !in->mail_followup_to)
    {
      if(!mutt_addr_is_user(in->to))
	rfc822_append (&out->cc, in->to);
      
      rfc822_append (&out->cc, in->cc);
    }
  }
  return 0;
}

static int
envelope_defaults (ENVELOPE *env, CONTEXT *ctx, HEADER *cur, int flags)
{
  ENVELOPE *curenv = NULL;
  LIST *tmp;
  char buffer[STRING];
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
	if (h->tagged && fetch_recips (env, h->env, flags) == -1)
	  return -1;
      }
    }
    else if (fetch_recips (env, curenv, flags) == -1)
      return -1;

    if ((flags & SENDLISTREPLY) && !env->to)
    {
      mutt_error _("No mailing lists found!");
      return (-1);
    }

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

    if (curenv->real_subj)
    {
      env->subject = safe_malloc (strlen (curenv->real_subj) + 5);
      sprintf (env->subject, "Re: %s", curenv->real_subj);
    }
    else
      env->subject = safe_strdup ("Re: your mail");

    /* add the In-Reply-To field */
    if (InReplyTo)
    {
      strfcpy (buffer, "In-Reply-To: ", sizeof (buffer));
      mutt_make_string (buffer + 13, sizeof (buffer) - 13, InReplyTo, ctx, cur);
      tmp = env->userhdrs;
      while (tmp && tmp->next)
	tmp = tmp->next;
      if (tmp)
      {
	tmp->next = mutt_new_list ();
	tmp = tmp->next;
      }
      else
	tmp = env->userhdrs = mutt_new_list ();
      tmp->data = safe_strdup (buffer);
    }

    if(tag)
    {
      HEADER *h;
      LIST **p;

      env->references = NULL;
      p = &env->references;
      
      for(i = 0; i < ctx->vcount; i++)
      {
	while(*p) p = &(*p)->next;
	h = ctx->hdrs[ctx->v2r[i]];
	if(h->tagged)
	  *p = make_references(h->env);
      }
    }
    else
      env->references = make_references(curenv);

  }
  else if (flags & SENDFORWARD)
  {
    /* set the default subject for the message. */
    mutt_make_string (buffer, sizeof (buffer), NONULL(ForwFmt), ctx, cur);
    env->subject = safe_strdup (buffer);
  }

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

    if (i == M_YES)
    {
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
    if (query_quadoption (OPT_MIMEFWD, _("Forward MIME encapsulated?")))
    {
      BODY *last = msg->content;

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
    else
    {
      if (cur)
	include_forward (ctx, cur, tempfp);
      else
	for (i=0; i < ctx->vcount; i++)
	  if (ctx->hdrs[ctx->v2r[i]]->tagged)
	    include_forward (ctx, ctx->hdrs[ctx->v2r[i]], tempfp);
    }
  }



#ifdef _PGPPATH
  else if (flags & SENDKEY) 
  {
    BODY *tmp;
    if ((tmp = pgp_make_key_attachment (NULL)) == NULL)
      return -1;

    tmp->next = msg->content;
    msg->content = tmp;
  }
#endif



  return (0);
}

void mutt_set_followup_to (ENVELOPE *e)
{
  ADDRESS *t = NULL;

  /* only generate the Mail-Followup-To if the user has requested it, and
     it hasn't already been set */
  if (option (OPTFOLLOWUPTO) && !e->mail_followup_to)
  {
    if (mutt_is_list_recipient (e->to) || mutt_is_list_recipient (e->cc))
    {
      /* i am a list recipient, so set the Mail-Followup-To: field so that
       * i don't end up getting multiple copies of responses to my mail
       */
      t = rfc822_append (&e->mail_followup_to, e->to);
      rfc822_append (&t, e->cc);
      /* the following is needed if $metoo is set, because the ->to and ->cc
	 may contain the user's private address(es) */
      e->mail_followup_to = remove_user (e->mail_followup_to, 0);
    }
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
    if (!tmp->personal)
      tmp->personal = safe_strdup (Realname);
  }
  return (tmp);
}

static ADDRESS *mutt_default_from (void)
{
  ADDRESS *adr = rfc822_new_address ();
  const char *fqdn = mutt_fqdn(1);
  
  /* don't set realname here, it will be set later */

  if (option (OPTUSEDOMAIN))
  {
    adr->mailbox = safe_malloc (strlen (NONULL(Username)) + strlen (NONULL(fqdn)) + 2);
    sprintf (adr->mailbox, "%s@%s", NONULL(Username), NONULL(fqdn));
  }
  else
    adr->mailbox = safe_strdup (NONULL(Username));
  return (adr);
}

static int send_message (HEADER *msg)
{  
  char tempfile[_POSIX_PATH_MAX];
  FILE *tempfp;
  int i;

  /* Write out the message in MIME form. */
  mutt_mktemp (tempfile);
  if ((tempfp = safe_fopen (tempfile, "w")) == NULL)
    return (-1);

  mutt_write_rfc822_header (tempfp, msg->env, msg->content, 0);
  fputc ('\n', tempfp); /* tie off the header. */

  if ((mutt_write_mime_body (msg->content, tempfp) == -1))
  {
    fclose(tempfp);
    unlink (tempfile);
    return (-1);
  }
  
  if (fclose (tempfp) != 0)
  {
    mutt_perror (tempfile);
    unlink (tempfile);
    return (-1);
  }

  i = mutt_invoke_sendmail (msg->env->to, msg->env->cc, msg->env->bcc,
		       tempfile, (msg->content->encoding == ENC8BIT));
  return (i ? -1 : 0);
}

/* rfc2047 encode the content-descriptions */
static void encode_descriptions (BODY *b)
{
  BODY *t;
  char tmp[LONG_STRING];

  for (t = b; t; t = t->next)
  {
    if (t->description)
    {
      rfc2047_encode_string (tmp, sizeof (tmp), (unsigned char *) t->description);
      safe_free ((void **) &t->description);
      t->description = safe_strdup (tmp);
    }
    if (t->parts)
      encode_descriptions (t->parts);
  }
}

void
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
#ifdef _PGPPATH
  BODY *save_content = NULL;
  char *pgpkeylist = NULL;
  /* save current value of "pgp_sign_as" */
  char *signas = NULL;
  char *signmic = NULL;
#endif

  if (!flags && quadoption (OPT_RECALL) != M_NO && mutt_num_postponed ())
  {
    /* If the user is composing a new message, check to see if there
     * are any postponed messages first.
     */
    if ((i = query_quadoption (OPT_RECALL, _("Recall postponed message?"))) == -1)
      return;

    if(i == M_YES)
      flags |= SENDPOSTPONED;
  }
  
  
#ifdef _PGPPATH
  if (flags & SENDPOSTPONED)
  {
    signas = safe_strdup(PgpSignAs);
    signmic = safe_strdup(PgpSignMicalg);
  }
#endif /* _PGPPATH */

  if (msg)
  {
    msg->env->to = mutt_expand_aliases (msg->env->to);
    msg->env->cc = mutt_expand_aliases (msg->env->cc);
    msg->env->bcc = mutt_expand_aliases (msg->env->bcc);
  }
  else
  {
    msg = mutt_new_header ();

    if (flags == SENDEDITMSG)
    {
      if (mutt_prepare_edit_message(ctx, msg, cur) < 0)
	goto cleanup;
    }
    else if (flags == SENDPOSTPONED)
    {
      if ((flags = mutt_get_postponed (ctx, msg, &cur)) < 0)
	goto cleanup;
    }

    if (flags & (SENDPOSTPONED | SENDEDITMSG))
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

  if (! (flags & (SENDKEY | SENDPOSTPONED | SENDEDITMSG)))
  {
    pbody = mutt_new_body ();
    pbody->next = msg->content; /* don't kill command-line attachments */
    msg->content = pbody;
    
    msg->content->type = TYPETEXT;
    msg->content->subtype = safe_strdup ("plain");
    msg->content->unlink = 1;
    msg->content->use_disp = 0;
    
    if (!tempfile)
    {
      mutt_mktemp (buffer);
      tempfp = safe_fopen (buffer, "w+");
      msg->content->filename = safe_strdup (buffer);
    }
    else
    {
      tempfp = safe_fopen (tempfile, "a+");
      msg->content->filename = safe_strdup (tempfile);
    }

    if (!tempfp)
    {
      dprint(1,(debugfile, "newsend_message: can't create tempfile %s (errno=%d)\n", msg->content->filename, errno));
      mutt_perror (msg->content->filename);
      goto cleanup;
    }
  }

  /* this is handled here so that the user can match ~f in send-hook */
  if (cur && option (OPTREVNAME) && !(flags & (SENDPOSTPONED | SENDEDITMSG)))
  {
    /* we shouldn't have to worry about freeing `msg->env->from' before
     * setting it here since this code will only execute when doing some
     * sort of reply.  the pointer will only be set when using the -H command
     * line option */
    msg->env->from = set_reverse_name (cur->env);
  }

  if (!msg->env->from && option (OPTUSEFROM) && !(flags & (SENDEDITMSG|SENDPOSTPONED)))
    msg->env->from = mutt_default_from ();

  if (flags & SENDBATCH) 
  {
    mutt_copy_stream (stdin, tempfp);
    if (option (OPTHDRS))
    {
      process_user_recips (msg->env);
      process_user_header (msg->env);
    }
  }
  else if (! (flags & (SENDPOSTPONED | SENDEDITMSG)))
  {
    if ((flags & (SENDREPLY | SENDFORWARD)) &&
	envelope_defaults (msg->env, ctx, cur, flags) == -1)
      goto cleanup;

    if (option (OPTHDRS))
      process_user_recips (msg->env);

    if (! (flags & SENDMAILX) &&
	! (option (OPTAUTOEDIT) && option (OPTEDITHDRS)) &&
	! ((flags & SENDREPLY) && option (OPTFASTREPLY)))
    {
      if (edit_envelope (msg->env) == -1)
	goto cleanup;
    }

    /* the from address must be set here regardless of whether or not
     * $use_from is set so that the `~P' (from you) operator in send-hook
     * patterns will work.  if $use_from is unset, the from address is killed
     * after send-hooks are evaulated */

    if (!msg->env->from)
    {
      msg->env->from = mutt_default_from ();
      killfrom = 1;
    }

    /* change settings based upon recipients */
    
    /* this needs to be executed even for postponed messages - the user may
     * be chosing editor settings based upon a message's recipients.
     */
    
    mutt_send_hook (msg);

    if (killfrom)
    {
      rfc822_free_address (&msg->env->from);
      killfrom = 0;
    }

    /* don't handle user headers when editing or recalling
     * postponed messages: _this_ part of the hooks should
     * not be executed.
     */

    if (option (OPTHDRS) && !(flags &(SENDPOSTPONED|SENDEDITMSG)))
      process_user_header (msg->env);



#ifdef _PGPPATH
    if (! (flags & SENDMAILX))
    {
      if (option (OPTPGPAUTOSIGN))
	msg->pgp |= PGPSIGN;
      if (option (OPTPGPAUTOENCRYPT))
	msg->pgp |= PGPENCRYPT;
      if (option (OPTPGPREPLYENCRYPT) && cur && cur->pgp & PGPENCRYPT)
	msg->pgp |= PGPENCRYPT;
      if (option (OPTPGPREPLYSIGN) && cur && cur->pgp & PGPSIGN)
	msg->pgp |= PGPSIGN;
    }
#endif /* _PGPPATH */



    /* include replies/forwarded messages */
    if (generate_body (tempfp, msg, flags, ctx, cur) == -1)
      goto cleanup;

    if (! (flags & (SENDMAILX | SENDKEY)) && Editor && strcmp (Editor, "builtin") != 0)
      append_signature (tempfp);
  }
  /* wait until now to set the real name portion of our return address so
     that $realname can be set in a send-hook */
  if (msg->env->from && !msg->env->from->personal && !(flags & (SENDEDITMSG | SENDPOSTPONED)))
    msg->env->from->personal = safe_strdup (Realname);



#ifdef _PGPPATH
  
  if (! (flags & SENDKEY))
  {

#endif
    


    fclose (tempfp);
    tempfp = NULL;
    


#ifdef _PGPPATH
    
  }

#endif



  if (flags & SENDMAILX)
  {
    if (mutt_builtin_editor (msg->content->filename, msg, cur) == -1)
      goto cleanup;
  }
  else if (! (flags & SENDBATCH))
  {
    struct stat st;
    time_t mtime;

    stat (msg->content->filename, &st);
    mtime = st.st_mtime;

    mutt_update_encoding (msg->content);

    /* If the this isn't a text message, look for a mailcap edit command */
    if(! (flags & SENDKEY))
    {
      if (mutt_needs_mailcap (msg->content))
	mutt_edit_attachment (msg->content);
      else if (!Editor || strcmp ("builtin", Editor) == 0)
	mutt_builtin_editor (msg->content->filename, msg, cur);
      else if (option (OPTEDITHDRS))
	mutt_edit_headers (Editor, msg->content->filename, msg, fcc, sizeof (fcc));
      else
	mutt_edit_file (Editor, msg->content->filename);
    }

    if (! (flags & (SENDPOSTPONED | SENDEDITMSG | SENDFORWARD | SENDKEY)))
    {
      if (stat (msg->content->filename, &st) == 0)
      {
	/* if the file was not modified, bail out now */
	if (mtime == st.st_mtime &&
	    query_quadoption (OPT_ABORT, _("Abort unmodified message?")) == M_YES)
	{
	  mutt_message _("Aborted unmodified message.");
	  goto cleanup;
	}
      }
      else
	mutt_perror (msg->content->filename);
    }
  }

  /* specify a default fcc.  if we are in batchmode, only save a copy of
   * the message if the value of $copy is yes or ask-yes */

  if (!fcc[0] && (!(flags & SENDBATCH) || (quadoption (OPT_COPY) & 0x1)))
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

    i = mutt_compose_menu (msg, fcc, sizeof (fcc), cur);
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
      if (!Postponed || mutt_write_fcc (NONULL (Postponed), msg, (cur && (flags & SENDREPLY)) ? cur->env->message_id : NULL, 1) < 0)
      {
	msg->content = mutt_remove_multipart (msg->content);
	goto main_loop;
      }
      mutt_message _("Message postponed.");
      goto cleanup;
    }
  }

  if (!msg->env->to && !msg->env->cc && !msg->env->bcc)
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

  if (!msg->env->subject && ! (flags & SENDBATCH) &&
      (i = query_quadoption (OPT_SUBJECT, _("No subject, abort sending?"))) != M_NO)
  {
    /* if the abort is automatic, print an error message */
    if (quadoption (OPT_SUBJECT) == M_YES)
      mutt_error _("No subject specified.");
    goto main_loop;
  }

  if (msg->content->next)
    msg->content = mutt_make_multipart (msg->content);

  /* Ok, we need to do it this way instead of handling all fcc stuff in
   * one place in order to avoid going to main_loop with encoded "env"
   * in case of error.  Ugh.
   */
#ifdef _PGPPATH
  if (msg->pgp)
  {
    if (pgp_get_keys (msg, &pgpkeylist) == -1)
      goto main_loop;

    /* save the decrypted attachments */
    save_content = msg->content;

    if (pgp_protect (msg, pgpkeylist) == -1)
    {
      if (msg->content->parts)
      {
	/* remove the toplevel multipart structure */
	pbody = msg->content;
	msg->content = msg->content->parts;
	pbody->parts = NULL;
	mutt_free_body (&pbody);
      }
      if (pgpkeylist)
	FREE (&pgpkeylist);
      goto main_loop;
    }
  }
#endif /* _PGPPATH */

  if (flags & SENDEDITMSG)
  {
   int really_send = mutt_yesorno (_("Message edited. Really send?"), 1);
   if (really_send != M_YES)
     goto main_loop;
  }

  if (!option (OPTNOCURSES) && !(flags & SENDMAILX))
    mutt_message _("Sending message...");

  mutt_prepare_envelope (msg->env);
  encode_descriptions (msg->content);

  /* save a copy of the message, if necessary. */
  mutt_expand_path (fcc, sizeof (fcc));
  if (*fcc && strcmp ("/dev/null", fcc) != 0)
  {
    BODY *tmpbody = msg->content;
#ifdef _PGPPATH
    BODY *save_sig = NULL;
    BODY *save_parts = NULL;
#endif /* _PGPPATH */

    /* check to see if the user wants copies of all attachments */
    if (!option (OPTFCCATTACH) && msg->content->type == TYPEMULTIPART)
    {
#ifdef _PGPPATH
      if (strcmp (msg->content->subtype, "encrypted") == 0 ||
	  strcmp (msg->content->subtype, "signed") == 0)
      {
	if (save_content->type == TYPEMULTIPART)
	{
	  if (!(msg->pgp & PGPENCRYPT) && (msg->pgp & PGPSIGN))
	  {
	    /* save initial signature and attachments */
	    save_sig = msg->content->parts->next;
	    save_parts = msg->content->parts->parts->next;
	  }

	  /* this means writing only the main part */
	  msg->content = save_content->parts;

	  if (pgp_protect (msg, pgpkeylist) == -1)
	  {
	    /* we can't do much about it at this point, so
	     * fallback to saving the whole thing to fcc
	     */
	    msg->content = tmpbody;
	    save_sig = NULL;
	    goto full_fcc;
	  }

	  if (msg->pgp & PGPENCRYPT)
	  {
	    /* not released in pgp_encrypt_message() */
	    mutt_free_body (&save_content->parts);
	    /* make sure we release the right thing later */
	    save_content->parts = msg->content;

	    encode_descriptions (msg->content);
	  }
	  else
	    save_content = msg->content;
	}
      }
      else
#endif /* _PGPPATH */
	msg->content = msg->content->parts;
    }

#ifdef _PGPPATH
full_fcc:
#endif /* _PGPPATH */
    if (msg->content)
      mutt_write_fcc (fcc, msg, NULL, 0);
    msg->content = tmpbody;

#ifdef _PGPPATH
    if (save_sig)
    {
      /* cleanup the second signature structures */
      mutt_free_body (&save_content->parts->next);
      save_content->parts = NULL;
      mutt_free_body (&save_content);

      /* restore old signature and attachments */
      msg->content->parts->next = save_sig;
      msg->content->parts->parts->next = save_parts;
    }
#endif /* _PGPPATH */
  }

#ifdef _PGPPATH
  if (msg->pgp & PGPENCRYPT)
  {
    /* cleanup structures from the first encryption */
    mutt_free_body (&save_content);
    FREE (&pgpkeylist);
  }
#endif /* _PGPPATH */

  if (send_message (msg) == -1)
  {
    msg->content = mutt_remove_multipart (msg->content);
    goto main_loop;
  }

  if (!option (OPTNOCURSES) && ! (flags & SENDMAILX))
    mutt_message _("Mail sent.");

  if (flags & SENDREPLY)
  {
    if (cur)
      mutt_set_flag (ctx, cur, M_REPLIED, 1);
    else if (!(flags & SENDPOSTPONED) && ctx && ctx->tagged)
    {
      for (i = 0; i < ctx->vcount; i++)
	if (ctx->hdrs[ctx->v2r[i]]->tagged)
	  mutt_set_flag (ctx, ctx->hdrs[ctx->v2r[i]], M_REPLIED, 1);
    }
  }

cleanup:



#ifdef _PGPPATH
  if (flags & SENDPOSTPONED)
  {
    
    if(signas)
    {
      safe_free((void **) &PgpSignAs);
      PgpSignAs = signas;
    }
    
    if(signmic)
    {
      safe_free((void **) &PgpSignMicalg);
      PgpSignMicalg = signmic;
    }
  }
#endif /* _PGPPATH */
   
  if (tempfp)
    fclose (tempfp);
  mutt_free_header (&msg);

}
