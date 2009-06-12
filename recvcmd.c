/*
 * Copyright (C) 1999-2004 Thomas Roessler <roessler@does-not-exist.org>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301, USA. 
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "attach.h"
#include "mapping.h"
#include "copy.h"
#include "mutt_idna.h"

/* some helper functions to verify that we are exclusively operating
 * on message/rfc822 attachments
 */

static short check_msg (BODY * b, short err)
{
  if (!mutt_is_message_type (b->type, b->subtype))
  {
    if (err)
      mutt_error _("You may only bounce message/rfc822 parts.");
    return -1;
  }
  return 0;
}

static short check_all_msg (ATTACHPTR ** idx, short idxlen,
			    BODY * cur, short err)
{
  short i;

  if (cur && check_msg (cur, err) == -1)
    return -1;
  else if (!cur)
  {
    for (i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
      {
	if (check_msg (idx[i]->content, err) == -1)
	  return -1;
      }
    }
  }
  return 0;
}


/* can we decode all tagged attachments? */

static short check_can_decode (ATTACHPTR ** idx, short idxlen, 
			      BODY * cur)
{
  short i;

  if (cur)
    return mutt_can_decode (cur);

  for (i = 0; i < idxlen; i++)
    if (idx[i]->content->tagged && !mutt_can_decode (idx[i]->content))
      return 0;

  return 1;
}

static short count_tagged (ATTACHPTR **idx, short idxlen)
{
  short count = 0;
  short i;
  
  for (i = 0; i < idxlen; i++)
    if (idx[i]->content->tagged)
      count++;
  
  return count;
}

/* count the number of tagged children below a multipart or message
 * attachment.
 */

static short count_tagged_children (ATTACHPTR ** idx, 
				    short idxlen, short i)
{
  short level = idx[i]->level;
  short count = 0;

  while ((++i < idxlen) && (level < idx[i]->level))
    if (idx[i]->content->tagged)
      count++;

  return count;
}



/**
 **
 ** The bounce function, from the attachment menu
 **
 **/

void mutt_attach_bounce (FILE * fp, HEADER * hdr, 
	   ATTACHPTR ** idx, short idxlen, BODY * cur)
{
  short i;
  char prompt[STRING];
  char buf[HUGE_STRING];
  char *err = NULL;
  ADDRESS *adr = NULL;
  int ret = 0;
  int p   = 0;

  if (check_all_msg (idx, idxlen, cur, 1) == -1)
    return;

  /* one or more messages? */
  p = (cur || count_tagged (idx, idxlen) == 1);

  /* RfC 5322 mandates a From: header, so warn before bouncing
   * messages without one */
  if (cur)
  {
    if (!cur->hdr->env->from)
    {
      mutt_error _("Warning: message contains no From: header");
      mutt_sleep (2);
      mutt_clear_error ();
    }
  }
  else
  {
    for (i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
      {
	if (!idx[i]->content->hdr->env->from)
	{
	  mutt_error _("Warning: message contains no From: header");
	  mutt_sleep (2);
	  mutt_clear_error ();
	  break;
	}
      }
    }
  }

  if (p)
    strfcpy (prompt, _("Bounce message to: "), sizeof (prompt));
  else
    strfcpy (prompt, _("Bounce tagged messages to: "), sizeof (prompt));

  buf[0] = '\0';
  if (mutt_get_field (prompt, buf, sizeof (buf), M_ALIAS) 
      || buf[0] == '\0')
    return;

  if (!(adr = rfc822_parse_adrlist (adr, buf)))
  {
    mutt_error _("Error parsing address!");
    return;
  }

  adr = mutt_expand_aliases (adr);
  
  if (mutt_addrlist_to_idna (adr, &err) < 0)
  {
    mutt_error (_("Bad IDN: '%s'"), err);
    FREE (&err);
    rfc822_free_address (&adr);
    return;
  }
  
  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), adr, 1);

#define extra_space (15+7+2)
  /*
   * See commands.c.
   */
  snprintf (prompt, sizeof (prompt) - 4, 
   (p ? _("Bounce message to %s") : _("Bounce messages to %s")), buf);
  
  if (mutt_strwidth (prompt) > COLS - extra_space)
  {
    mutt_format_string (prompt, sizeof (prompt) - 4,
			0, COLS-extra_space, FMT_LEFT, 0,
			prompt, sizeof (prompt), 0);
    safe_strcat (prompt, sizeof (prompt), "...?");
  }
  else
    safe_strcat (prompt, sizeof (prompt), "?");

  if (query_quadoption (OPT_BOUNCE, prompt) != M_YES)
  {
    rfc822_free_address (&adr);
    CLEARLINE (LINES - 1);
    mutt_message (p ? _("Message not bounced.") : _("Messages not bounced."));
    return;
  }
  
  CLEARLINE (LINES - 1);
  
  if (cur)
    ret = mutt_bounce_message (fp, cur->hdr, adr);
  else
  {
    for (i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
	if (mutt_bounce_message (fp, idx[i]->content->hdr, adr))
	  ret = 1;
    }
  }

  if (!ret)
    mutt_message (p ? _("Message bounced.") : _("Messages bounced."));
  else
    mutt_error (p ? _("Error bouncing message!") : _("Error bouncing messages!"));
}



/**
 **
 ** resend-message, from the attachment menu 
 **
 **
 **/

void mutt_attach_resend (FILE * fp, HEADER * hdr, ATTACHPTR ** idx, 
			 short idxlen, BODY * cur)
{
  short i;

  if (check_all_msg (idx, idxlen, cur, 1) == -1)
    return;

  if (cur)
    mutt_resend_message (fp, Context, cur->hdr);
  else
  {
    for (i = 0; i < idxlen; i++)
      if (idx[i]->content->tagged)
	mutt_resend_message (fp, Context, idx[i]->content->hdr);
  }
}


/**
 **
 ** forward-message, from the attachment menu 
 **
 **/
  
/* try to find a common parent message for the tagged attachments. */

static HEADER *find_common_parent (ATTACHPTR ** idx, short idxlen,
				   short nattach)
{
  short i;
  short nchildren;

  for (i = 0; i < idxlen; i++)
    if (idx[i]->content->tagged)
      break;
  
  while (--i >= 0)
  {
    if (mutt_is_message_type (idx[i]->content->type, idx[i]->content->subtype))
    {
      nchildren = count_tagged_children (idx, idxlen, i);
      if (nchildren == nattach)
	return idx[i]->content->hdr;
    }
  }

  return NULL;
}

/* 
 * check whether attachment #i is a parent of the attachment
 * pointed to by cur
 * 
 * Note: This and the calling procedure could be optimized quite a 
 * bit.  For now, it's not worth the effort.
 */

static int is_parent (short i, ATTACHPTR **idx, short idxlen, BODY *cur)
{
  short level = idx[i]->level;

  while ((++i < idxlen) && idx[i]->level > level)
  {
    if (idx[i]->content == cur)
      return 1;
  }

  return 0;
}

static HEADER *find_parent (ATTACHPTR **idx, short idxlen, BODY *cur, short nattach)
{
  short i;
  HEADER *parent = NULL;
  
  if (cur)
  {
    for (i = 0; i < idxlen; i++)
    {
      if (mutt_is_message_type (idx[i]->content->type, idx[i]->content->subtype) 
	  && is_parent (i, idx, idxlen, cur))
	parent = idx[i]->content->hdr;
      if (idx[i]->content == cur)
	break;
    }
  }
  else if (nattach)
    parent = find_common_parent (idx, idxlen, nattach);
  
  return parent;
}

static void include_header (int quote, FILE * ifp,
			    HEADER * hdr, FILE * ofp,
			    char *_prefix)
{
  int chflags = CH_DECODE;
  char prefix[SHORT_STRING];
  
  if (option (OPTWEED))
    chflags |= CH_WEED | CH_REORDER;

  if (quote)
  {
    if (_prefix)
      strfcpy (prefix, _prefix, sizeof (prefix));
    else if (!option (OPTTEXTFLOWED))
      _mutt_make_string (prefix, sizeof (prefix), NONULL (Prefix), 
			 Context, hdr, 0);
    else
      strfcpy (prefix, ">", sizeof (prefix));

    chflags |= CH_PREFIX;
  }
  
  mutt_copy_header (ifp, hdr, ofp, chflags, quote ? prefix : NULL);
}

/* Attach all the body parts which can't be decoded. 
 * This code is shared by forwarding and replying. */

static BODY ** copy_problematic_attachments (FILE *fp,
					     BODY **last, 
					     ATTACHPTR **idx, 
					     short idxlen,
					     short force)
{
  short i;
  
  for (i = 0; i < idxlen; i++)
  {
    if (idx[i]->content->tagged && 
	(force || !mutt_can_decode (idx[i]->content)))
    {
      if (mutt_copy_body (fp, last, idx[i]->content) == -1)
	return NULL;		/* XXXXX - may lead to crashes */
      last = &((*last)->next);
    }
  }
  return last;
}

/* 
 * forward one or several MIME bodies 
 * (non-message types)
 */

static void attach_forward_bodies (FILE * fp, HEADER * hdr,
				   ATTACHPTR ** idx, short idxlen,
				   BODY * cur,
				   short nattach)
{
  short i;
  short mime_fwd_all = 0;
  short mime_fwd_any = 1;
  HEADER *parent = NULL;
  HEADER *tmphdr = NULL;
  BODY **last;
  char tmpbody[_POSIX_PATH_MAX];
  FILE *tmpfp = NULL;

  char prefix[STRING];

  int rc = 0;

  STATE st;

  /* 
   * First, find the parent message.
   * Note: This could be made an option by just
   * putting the following lines into an if block.
   */


  parent = find_parent (idx, idxlen, cur, nattach);
  
  if (parent == NULL)
    parent = hdr;


  tmphdr = mutt_new_header ();
  tmphdr->env = mutt_new_envelope ();
  mutt_make_forward_subject (tmphdr->env, Context, parent);

  mutt_mktemp (tmpbody);
  if ((tmpfp = safe_fopen (tmpbody, "w")) == NULL)
  {
    mutt_error (_("Can't open temporary file %s."), tmpbody);
    return;
  }

  mutt_forward_intro (tmpfp, parent);

  /* prepare the prefix here since we'll need it later. */

  if (option (OPTFORWQUOTE))
  {
    if (!option (OPTTEXTFLOWED))
      _mutt_make_string (prefix, sizeof (prefix), NONULL (Prefix), Context,
			 parent, 0);
    else
      strfcpy (prefix, ">", sizeof (prefix));
  }
    
  include_header (option (OPTFORWQUOTE), fp, parent,
		  tmpfp, prefix);


  /* 
   * Now, we have prepared the first part of the message body: The
   * original message's header. 
   *
   * The next part is more interesting: either include the message bodies,
   * or attach them.
   */

  if ((!cur || mutt_can_decode (cur)) &&
      (rc = query_quadoption (OPT_MIMEFWD, 
			      _("Forward as attachments?"))) == M_YES)
    mime_fwd_all = 1;
  else if (rc == -1)
    goto bail;

  /* 
   * shortcut MIMEFWDREST when there is only one attachment.  Is 
   * this intuitive?
   */

  if (!mime_fwd_all && !cur && (nattach > 1) 
      && !check_can_decode (idx, idxlen, cur))
  {
    if ((rc = query_quadoption (OPT_MIMEFWDREST,
_("Can't decode all tagged attachments.  MIME-forward the others?"))) == -1)
      goto bail;
    else if (rc == M_NO)
      mime_fwd_any = 0;
  }

  /* initialize a state structure */
  
  memset (&st, 0, sizeof (st));
  
  if (option (OPTFORWQUOTE))
    st.prefix = prefix;
  st.flags = M_CHARCONV;
  if (option (OPTWEED))
    st.flags |= M_WEED;
  st.fpin = fp;
  st.fpout = tmpfp;

  /* where do we append new MIME parts? */
  last = &tmphdr->content;

  if (cur)
  {
    /* single body case */

    if (!mime_fwd_all && mutt_can_decode (cur))
    {
      mutt_body_handler (cur, &st);
      state_putc ('\n', &st);
    }
    else
    {
      if (mutt_copy_body (fp, last, cur) == -1)
	goto bail;
      last = &((*last)->next);
    }
  }
  else
  {
    /* multiple body case */

    if (!mime_fwd_all)
    {
      for (i = 0; i < idxlen; i++)
      {
	if (idx[i]->content->tagged && mutt_can_decode (idx[i]->content))
	{
	  mutt_body_handler (idx[i]->content, &st);
	  state_putc ('\n', &st);
	}
      }
    }

    if (mime_fwd_any && 
	copy_problematic_attachments (fp, last, idx, idxlen, mime_fwd_all) == NULL)
      goto bail;
  }
  
  mutt_forward_trailer (tmpfp);
  
  safe_fclose (&tmpfp);
  tmpfp = NULL;

  /* now that we have the template, send it. */
  ci_send_message (0, tmphdr, tmpbody, NULL, parent);
  return;
  
  bail:
  
  if (tmpfp)
  {
    safe_fclose (&tmpfp);
    mutt_unlink (tmpbody);
  }

  mutt_free_header (&tmphdr);
}


/* 
 * Forward one or several message-type attachments. This 
 * is different from the previous function
 * since we want to mimic the index menu's behaviour.
 *
 * Code reuse from ci_send_message is not possible here -
 * ci_send_message relies on a context structure to find messages,
 * while, on the attachment menu, messages are referenced through
 * the attachment index. 
 */

static void attach_forward_msgs (FILE * fp, HEADER * hdr, 
	       ATTACHPTR ** idx, short idxlen, BODY * cur)
{
  HEADER *curhdr = NULL;
  HEADER *tmphdr;
  short i;
  int rc;

  BODY **last;
  char tmpbody[_POSIX_PATH_MAX];
  FILE *tmpfp = NULL;

  int cmflags = 0;
  int chflags = CH_XMIT;
  
  if (cur)
    curhdr = cur->hdr;
  else
  {
    for (i = 0; i < idxlen; i++)
      if (idx[i]->content->tagged)
      {
	curhdr = idx[i]->content->hdr;
	break;
      }
  }

  tmphdr = mutt_new_header ();
  tmphdr->env = mutt_new_envelope ();
  mutt_make_forward_subject (tmphdr->env, Context, curhdr);


  tmpbody[0] = '\0';

  if ((rc = query_quadoption (OPT_MIMEFWD, 
		 _("Forward MIME encapsulated?"))) == M_NO)
  {
    
    /* no MIME encapsulation */
    
    mutt_mktemp (tmpbody);
    if (!(tmpfp = safe_fopen (tmpbody, "w")))
    {
      mutt_error (_("Can't create %s."), tmpbody);
      mutt_free_header (&tmphdr);
      return;
    }

    if (option (OPTFORWQUOTE))
    {
      chflags |= CH_PREFIX;
      cmflags |= M_CM_PREFIX;
    }

    if (option (OPTFORWDECODE))
    {
      cmflags |= M_CM_DECODE | M_CM_CHARCONV;
      if (option (OPTWEED))
      {
	chflags |= CH_WEED | CH_REORDER;
	cmflags |= M_CM_WEED;
      }
    }
    
    
    if (cur)
    {
      /* mutt_message_hook (cur->hdr, M_MESSAGEHOOK); */ 
      mutt_forward_intro (tmpfp, cur->hdr);
      _mutt_copy_message (tmpfp, fp, cur->hdr, cur->hdr->content, cmflags, chflags);
      mutt_forward_trailer (tmpfp);
    }
    else
    {
      for (i = 0; i < idxlen; i++)
      {
	if (idx[i]->content->tagged)
	{
	  /* mutt_message_hook (idx[i]->content->hdr, M_MESSAGEHOOK); */ 
	  mutt_forward_intro (tmpfp, idx[i]->content->hdr);
	  _mutt_copy_message (tmpfp, fp, idx[i]->content->hdr,
			      idx[i]->content->hdr->content, cmflags, chflags);
	  mutt_forward_trailer (tmpfp);
	}
      }
    }
    safe_fclose (&tmpfp);
  }
  else if (rc == M_YES)	/* do MIME encapsulation - we don't need to do much here */
  {
    last = &tmphdr->content;
    if (cur)
      mutt_copy_body (fp, last, cur);
    else
    {
      for (i = 0; i < idxlen; i++)
	if (idx[i]->content->tagged)
	{
	  mutt_copy_body (fp, last, idx[i]->content);
	  last = &((*last)->next);
	}
    }
  }
  else
    mutt_free_header (&tmphdr);

  ci_send_message (0, tmphdr, *tmpbody ? tmpbody : NULL, 
		   NULL, curhdr);

}

void mutt_attach_forward (FILE * fp, HEADER * hdr, 
			  ATTACHPTR ** idx, short idxlen, BODY * cur)
{
  short nattach;
  

  if (check_all_msg (idx, idxlen, cur, 0) == 0)
    attach_forward_msgs (fp, hdr, idx, idxlen, cur);
  else
  {
    nattach = count_tagged (idx, idxlen);
    attach_forward_bodies (fp, hdr, idx, idxlen, cur, nattach);
  }
}



/**
 ** 
 ** the various reply functions, from the attachment menu
 **
 **
 **/

/* Create the envelope defaults for a reply.
 *
 * This function can be invoked in two ways.
 * 
 * Either, parent is NULL.  In this case, all tagged bodies are of a message type,
 * and the header information is fetched from them.
 * 
 * Or, parent is non-NULL.  In this case, cur is the common parent of all the
 * tagged attachments.
 * 
 * Note that this code is horribly similar to envelope_defaults () from send.c.
 */
  
static int
attach_reply_envelope_defaults (ENVELOPE *env, ATTACHPTR **idx, short idxlen,
				HEADER *parent, int flags)
{
  ENVELOPE *curenv = NULL;
  HEADER *curhdr = NULL;
  short i;
  
  if (!parent)
  {
    for (i = 0; i < idxlen; i++)
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

  if (curenv == NULL  ||  curhdr == NULL)
  {
    mutt_error _("Can't find any tagged messages.");
    return -1;
  }

  if (parent)
  {
    if (mutt_fetch_recips (env, curenv, flags) == -1)
      return -1;
  }
  else
  {
    for (i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged
	  && mutt_fetch_recips (env, idx[i]->content->hdr->env, flags) == -1)
	return -1;
    }
  }
  
  if ((flags & SENDLISTREPLY) && !env->to)
  {
    mutt_error _("No mailing lists found!");
    return (-1);
  }
  
  mutt_fix_reply_recipients (env);
  mutt_make_misc_reply_headers (env, Context, curhdr, curenv);

  if (parent)
    mutt_add_to_reference_headers (env, curenv, NULL, NULL);
  else
  {
    LIST **p = NULL, **q = NULL;
    
    for (i = 0; i < idxlen; i++)
    {
      if (idx[i]->content->tagged)
	mutt_add_to_reference_headers (env, idx[i]->content->hdr->env, &p, &q);
    }
  }
  
  return 0;
}


/*  This is _very_ similar to send.c's include_reply(). */

static void attach_include_reply (FILE *fp, FILE *tmpfp, HEADER *cur, int flags)
{
  int cmflags = M_CM_PREFIX | M_CM_DECODE | M_CM_CHARCONV;
  int chflags = CH_DECODE;

  /* mutt_message_hook (cur, M_MESSAGEHOOK); */ 
  
  mutt_make_attribution (Context, cur, tmpfp);
  
  if (!option (OPTHEADER))
    cmflags |= M_CM_NOHEADER;
  if (option (OPTWEED))
  {
    chflags |= CH_WEED;
    cmflags |= M_CM_WEED;
  }

  _mutt_copy_message (tmpfp, fp, cur, cur->content, cmflags, chflags);
  mutt_make_post_indent (Context, cur, tmpfp);
}
  
void mutt_attach_reply (FILE * fp, HEADER * hdr, 
			ATTACHPTR ** idx, short idxlen, BODY * cur, 
			int flags)
{
  short mime_reply_any = 0;
  
  short nattach = 0;
  HEADER *parent = NULL;
  HEADER *tmphdr = NULL;
  short i;

  STATE st;
  char tmpbody[_POSIX_PATH_MAX];
  FILE *tmpfp;
  
  char prefix[SHORT_STRING];
  int rc;
  
  if (check_all_msg (idx, idxlen, cur, 0) == -1)
  {
    nattach = count_tagged (idx, idxlen);
    if ((parent = find_parent (idx, idxlen, cur, nattach)) == NULL)
      parent = hdr;
  }

  if (nattach > 1 && !check_can_decode (idx, idxlen, cur))
  {
    if ((rc = query_quadoption (OPT_MIMEFWDREST,
      _("Can't decode all tagged attachments.  MIME-encapsulate the others?"))) == -1)
      return;
    else if (rc == M_YES)
      mime_reply_any = 1;
  }
  else if (nattach == 1)
    mime_reply_any = 1;

  tmphdr = mutt_new_header ();
  tmphdr->env = mutt_new_envelope ();

  if (attach_reply_envelope_defaults (tmphdr->env, idx, idxlen, 
				      parent ? parent : (cur ? cur->hdr : NULL), flags) == -1)
  {
    mutt_free_header (&tmphdr);
    return;
  }
  
  mutt_mktemp (tmpbody);
  if ((tmpfp = safe_fopen (tmpbody, "w")) == NULL)
  {
    mutt_error (_("Can't create %s."), tmpbody);
    mutt_free_header (&tmphdr);
    return;
  }

  if (!parent)
  {
    if (cur)
      attach_include_reply (fp, tmpfp, cur->hdr, flags);
    else
    {
      for (i = 0; i < idxlen; i++)
      {
	if (idx[i]->content->tagged)
	  attach_include_reply (fp, tmpfp, idx[i]->content->hdr, flags);
      }
    }
  }
  else
  {
    mutt_make_attribution (Context, parent, tmpfp);
    
    memset (&st, 0, sizeof (STATE));
    st.fpin = fp;
    st.fpout = tmpfp;

    if (!option (OPTTEXTFLOWED))
      _mutt_make_string (prefix, sizeof (prefix), NONULL (Prefix), 
			 Context, parent, 0);
    else
      strfcpy (prefix, ">", sizeof (prefix));

    st.prefix = prefix;
    st.flags  = M_CHARCONV;
    
    if (option (OPTWEED)) 
      st.flags |= M_WEED;

    if (option (OPTHEADER))
      include_header (1, fp, parent, tmpfp, prefix);

    if (cur)
    {
      if (mutt_can_decode (cur))
      {
	mutt_body_handler (cur, &st);
	state_putc ('\n', &st);
      }
      else
	mutt_copy_body (fp, &tmphdr->content, cur);
    }
    else
    {
      for (i = 0; i < idxlen; i++)
      {
	if (idx[i]->content->tagged && mutt_can_decode (idx[i]->content))
	{
	  mutt_body_handler (idx[i]->content, &st);
	  state_putc ('\n', &st);
	}
      }
    }

    mutt_make_post_indent (Context, parent, tmpfp);

    if (mime_reply_any && !cur && 
	copy_problematic_attachments (fp, &tmphdr->content, idx, idxlen, 0) == NULL)
    {
      mutt_free_header (&tmphdr);
      safe_fclose (&tmpfp);
      return;
    }
  }

  safe_fclose (&tmpfp);
  
  if (ci_send_message (flags, tmphdr, tmpbody, NULL, parent) == 0)
    mutt_set_flag (Context, hdr, M_REPLIED, 1);
}

