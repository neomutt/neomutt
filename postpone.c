static char rcsid[]="$Id$";
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
#include "mutt_menu.h"
#include "rfc1524.h"
#include "mime.h"
#include "mailbox.h"
#include "mapping.h"

#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

static struct mapping_t PostponeHelp[] = {
  { N_("Exit"),  OP_EXIT },
  { N_("Del"),   OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Help"),  OP_HELP },
  { NULL }
};



#ifdef _PGPPATH
#include "pgp.h"
#endif /* _PGPPATH */



static short PostCount = 0;
static time_t LastModify = 0;
static CONTEXT *PostContext = NULL;

int mutt_num_postponed (void)
{
  struct stat st;
  CONTEXT ctx;

  if (!Postponed || stat (Postponed, &st) == -1)
  {
     PostCount = 0;
     LastModify = 0;
     return (0);
  } 
  else if (S_ISDIR (st.st_mode))
  {
    /* if we have a maildir mailbox, we need to stat the "new" dir */

    char buf[_POSIX_PATH_MAX];

    snprintf (buf, sizeof (buf), "%s/new", Postponed);
    if (access (buf, F_OK) == 0 && stat (buf, &st) == -1)
    {
      PostCount = 0;
      LastModify = 0;
      return 0;
    }
  }

  if (LastModify < st.st_mtime)
  {
    LastModify = st.st_mtime;

    if (access (Postponed, R_OK | F_OK) != 0)
      return (PostCount = 0);
    if (mx_open_mailbox (Postponed, M_NOSORT | M_QUIET, &ctx) == NULL)
      PostCount = 0;
    else
      PostCount = ctx.msgcount;
    mx_fastclose_mailbox (&ctx);
  }

  return (PostCount);
}

static void post_entry (char *s, size_t slen, MUTTMENU *menu, int entry)
{
  CONTEXT *ctx = (CONTEXT *) menu->data;

  _mutt_make_string (s, slen, NONULL (HdrFmt), ctx, ctx->hdrs[entry],
		     M_FORMAT_ARROWCURSOR);
}

static HEADER *select_msg (void)
{
  MUTTMENU *menu;
  int i, done=0, r=-1;
  char helpstr[SHORT_STRING];

  menu = mutt_new_menu ();
  menu->make_entry = post_entry;
  menu->menu = MENU_POST;
  menu->max = PostContext->msgcount;
  menu->title = _("Postponed Messages");
  menu->data = PostContext;
  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_POST, PostponeHelp);

  while (!done)
  {
    switch (i = mutt_menuLoop (menu))
    {
      case OP_DELETE:
      case OP_UNDELETE:
	mutt_set_flag (PostContext, PostContext->hdrs[menu->current], M_DELETE, (i == OP_DELETE) ? 1 : 0);
	PostCount = PostContext->msgcount - PostContext->deleted;
	if (option (OPTRESOLVE) && menu->current < menu->max - 1)
	{
	  menu->oldcurrent = menu->current;
	  menu->current++;
	  if (menu->current >= menu->top + menu->pagelen)
	  {
	    menu->top = menu->current;
	    menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	  }
	  else
	    menu->redraw |= REDRAW_MOTION_RESYNCH;
	}
	else
	  menu->redraw = REDRAW_CURRENT;
	break;

      case OP_GENERIC_SELECT_ENTRY:
	r = menu->current;
	done = 1;
	break;

      case OP_EXIT:
	done = 1;
	break;
    }
  }

  mutt_menuDestroy (&menu);
  return (r > -1 ? PostContext->hdrs[r] : NULL);
}

/* args:
 *      ctx	Context info, used when recalling a message to which
 *              we reply.
 *	hdr	envelope/attachment info for recalled message
 *	cur	if message was a reply, `cur' is set to the message which
 *		`hdr' is in reply to
 *
 * return vals:
 *	-1		error/no messages
 *	0		normal exit
 *	SENDREPLY	recalled message is a reply
 */
int mutt_get_postponed (CONTEXT *ctx, HEADER *hdr, HEADER **cur)
{
  HEADER *h;
  int code = SENDPOSTPONED;
  LIST *tmp;
  LIST *last = NULL;
  LIST *next;
  char *p;
  int opt_delete;

  if (!Postponed)
    return (-1);

  if ((PostContext = mx_open_mailbox (Postponed, M_NOSORT, NULL)) == NULL)
  {
    PostCount = 0;
    mutt_error _("No postponed messages.");
    return (-1);
  }
  
  if (! PostContext->msgcount)
  {
    PostCount = 0;
    mx_close_mailbox (PostContext);
    safe_free ((void **) &PostContext);
    mutt_error _("No postponed messages.");
    return (-1);
  }

  if (PostContext->msgcount == 1)
  {
    /* only one message, so just use that one. */
    h = PostContext->hdrs[0];
  }
  else if ((h = select_msg ()) == NULL)
  {
    mx_close_mailbox (PostContext);
    safe_free ((void **) &PostContext);
    return (-1);
  }

  if (mutt_edit_message (PostContext, hdr, h) < 0)
  {
      mx_fastclose_mailbox (PostContext);
      safe_free ((void **) &PostContext);
      return (-1);
  }

  /* finished with this message, so delete it. */
  mutt_set_flag (PostContext, h, M_DELETE, 1);

  /* update the count for the status display */
  PostCount = PostContext->msgcount - PostContext->deleted;

  /* avoid the "purge deleted messages" prompt */
  opt_delete = quadoption (OPT_DELETE);
  set_quadoption (OPT_DELETE, M_YES);
  mx_close_mailbox (PostContext);
  set_quadoption (OPT_DELETE, opt_delete);

  safe_free ((void **) &PostContext);

  for (tmp = hdr->env->userhdrs; tmp; )
  {
    if (strncasecmp ("X-Mutt-References:", tmp->data, 18) == 0)
    {
      if (ctx)
      {
	/* if a mailbox is currently open, look to see if the orignal message
	   the user attempted to reply to is in this mailbox */
	p = tmp->data + 18;
	SKIPWS (p);
	*cur = hash_find (ctx->id_hash, p);
      }

      /* Remove the X-Mutt-References: header field. */
      next = tmp->next;
      if (last)
	last->next = tmp->next;
      else
	hdr->env->userhdrs = tmp->next;
      tmp->next = NULL;
      mutt_free_list (&tmp);
      tmp = next;
      if (*cur)
	code |= SENDREPLY;
    }



#ifdef _PGPPATH
    else if (strncmp ("Pgp:", tmp->data, 4) == 0)
    {
      hdr->pgp = mutt_parse_pgp_hdr (tmp->data+4, 1);
       
      /* remove the pgp field */
      next = tmp->next;
      if (last)
	last->next = tmp->next;
      else
	hdr->env->userhdrs = tmp->next;
      tmp->next = NULL;
      mutt_free_list (&tmp);
      tmp = next;
    }
#endif /* _PGPPATH */



    else
    {
      last = tmp;
      tmp = tmp->next;
    }
  }
  return (code);
}



#ifdef _PGPPATH

int mutt_parse_pgp_hdr (char *p, int set_signas)
{
  int pgp = 0;
  char pgp_sign_as[LONG_STRING] = "\0", *q;
  char pgp_sign_micalg[LONG_STRING] = "\0";
   
  SKIPWS (p);
  for (; *p; p++)
  {    
     
    switch (*p)
    {
      case 'e':
      case 'E':
        pgp |= PGPENCRYPT;
        break;

      case 's':    
      case 'S':
        pgp |= PGPSIGN;
        q = pgp_sign_as;
      
        if (*(p+1) == '<')
        {
          for (p += 2; 
	       *p && *p != '>' && q < pgp_sign_as + sizeof (pgp_sign_as) - 1;
               *q++ = *p++)
	    ;

          if (*p!='>')
          {
            mutt_error _("Illegal PGP header");
            return 0;
          }
        }
       
        *q = '\0';
        break;

      case 'm':
      case 'M':
   	q = pgp_sign_micalg;
	
        if(*(p+1) == '<')
	{
	  for(p += 2; *p && *p != '>' && q < pgp_sign_micalg + sizeof(pgp_sign_micalg) - 1;
	      *q++ = *p++)
	    ;
	  
	  if(*p != '>')
	  {
	    mutt_error _("Illegal PGP header");
	    return 0;
	  }
	}

	*q = '\0';
	break;
	  
      default:
        mutt_error _("Illegal PGP header");
        return 0;
    }
     
  }
 
  if (set_signas || *pgp_sign_as)
  {
    safe_free((void **) &PgpSignAs);
    PgpSignAs = safe_strdup(pgp_sign_as);
  }

  /* the micalg field must not be empty */
  if (set_signas && *pgp_sign_micalg)
  {
    safe_free((void **) &PgpSignMicalg);
    PgpSignMicalg = safe_strdup(pgp_sign_micalg);
  }

  return pgp;
}
#endif /* _PGPPATH */



int mutt_edit_message (CONTEXT *ctx, HEADER *newhdr, HEADER *hdr)
{
  MESSAGE *msg = mx_open_message (ctx, hdr->msgno);
  char file[_POSIX_PATH_MAX];

  if (msg == NULL)
    return (-1);

  fseek (msg->fp, hdr->offset, 0);
  newhdr->env = mutt_read_rfc822_header (msg->fp, newhdr, 1);

  if (hdr->content->type == TYPEMESSAGE || hdr->content->type == TYPEMULTIPART)
  {
    BODY *b;

    fseek (msg->fp, hdr->content->offset, 0);

    if (hdr->content->type == TYPEMULTIPART)
    {
      hdr->content->parts = mutt_parse_multipart (msg->fp, 
	       mutt_get_parameter ("boundary", hdr->content->parameter),
	       hdr->content->offset + hdr->content->length,
	       strcasecmp ("digest", hdr->content->subtype) == 0);
    }
    else
      hdr->content->parts = mutt_parse_messageRFC822 (msg->fp, hdr->content);

    /* Now that we know what was in the other message, convert to the new
     * message.
     */
    newhdr->content = hdr->content->parts;
    b = hdr->content->parts;
    while (b != NULL)
    {
      file[0] = '\0';
      if (b->filename)
	strfcpy (file, b->filename, sizeof (file));
      else
	/* avoid Content-Disposition: header with temporary filename */
	b->use_disp = 0;
      mutt_adv_mktemp (file, sizeof(file));
      if (mutt_save_attachment (msg->fp, b, file, 0, NULL) == -1)
      {
	mutt_free_envelope (&newhdr->env);
	mutt_free_body (&newhdr->content);
	mx_close_message (&msg);
	return (-1);
      }
      safe_free ((void *) &b->filename);
      b->filename = safe_strdup (file);
      b->unlink = 1;
      mutt_free_body (&b->parts);
      b = b->next;
    }
    hdr->content->parts = NULL;
  }
  else
  {
    mutt_mktemp (file);
    if (mutt_save_attachment (msg->fp, hdr->content, file, 0, NULL) == -1)
    {
      mutt_free_envelope (&newhdr->env);
      mx_close_message (&msg);
      return (-1);
    }
    newhdr->content = mutt_make_file_attach (file);
    newhdr->content->use_disp = 0;	/* no content-disposition */
    newhdr->content->unlink = 1;	/* delete when we are done */
  }

  mx_close_message (&msg);
  return 0;
}
