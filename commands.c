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
#include "mutt_menu.h"
#include "mime.h"
#include "sort.h"
#include "mailbox.h"
#include "copy.h"
#include "mx.h"
#include "pager.h"

#ifdef BUFFY_SIZE
#include "buffy.h"
#endif



#ifdef _PGPPATH
#include "pgp.h"
#endif











#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

extern char *ReleaseDate;

/* The folder the user last saved to.  Used by ci_save_message() */
static char LastSaveFolder[_POSIX_PATH_MAX] = "";

/* for compatibility with metamail */
static int is_mmnoask (const char *buf)
{
  char tmp[LONG_STRING], *p, *q;
  int lng;

  if ((p = getenv ("MM_NOASK")) != NULL && *p)
  {
    if (strcmp (p, "1") == 0)
      return (1);

    strfcpy (tmp, p, sizeof (tmp));
    p = tmp;

    while ((p = strtok (p, ",")) != NULL)
    {
      if ((q = strrchr (p, '/')) != NULL)
      {
	if (*(q+1) == '*')
	{
	  if (strncasecmp (buf, p, q-p) == 0)
	    return (1);
	}
	else
	{
	  if (strcasecmp (buf, p) == 0)
	    return (1);
	}
      }
      else
      {
	lng = strlen (p);
	if (buf[lng] == '/' && strncasecmp (buf, p, lng) == 0)
	  return (1);
      }

      p = NULL;
    }
  }

  return (0);
}

int mutt_display_message (HEADER *cur, const char *attach_msg_status)
{
  char tempfile[_POSIX_PATH_MAX], buf[LONG_STRING];
  int rc = 0, builtin = 0;
  int cmflags = M_CM_DECODE | M_CM_DISPLAY;
  FILE *fpout;

  snprintf (buf, sizeof (buf), "%s/%s", TYPE (cur->content),
	    cur->content->subtype);

  if (cur->mailcap && !mutt_is_autoview (buf))
  {
    if (is_mmnoask (buf))
      rc = M_YES;
    else
      rc = query_quadoption (OPT_USEMAILCAP, "Display message using mailcap?");
    if (rc < 0)
      return 0;
    else if (rc == M_YES)
    {
      MESSAGE *msg;

      if ((msg = mx_open_message (Context, cur->msgno)) != NULL)
      {
	mutt_view_attachment (msg->fp, cur->content, M_REGULAR);
	mx_close_message (&msg);
	mutt_set_flag (Context, cur, M_READ, 1);
      }
      return 0;
    }
  }

  mutt_parse_mime_message (Context, cur);



#ifdef _PGPPATH
  /* see if PGP is needed for this message.  if so, we should exit curses */
  if (cur->pgp)
  {
    if (cur->pgp & PGPENCRYPT)
    {
      if (!pgp_valid_passphrase ())
	return 0;

      cmflags |= M_CM_VERIFY;
      mutt_message ("Invoking PGP...");
    }
    else if (cur->pgp & PGPSIGN)
    {
      /* find out whether or not the verify signature */
      if (query_quadoption (OPT_VERIFYSIG, "Verify PGP signature?") == M_YES)
      {
	cmflags |= M_CM_VERIFY;
	mutt_message ("Invoking PGP...");
      }
    }
  }
#endif







  mutt_mktemp (tempfile);
  if ((fpout = safe_fopen (tempfile, "w")) == NULL)
  {
    mutt_error ("Could not create temporary file!");
    return (0);
  }

  if (!Pager || strcmp (Pager, "builtin") == 0)
    builtin = 1;
  else
  {
    mutt_make_string (buf, sizeof (buf), NONULL(PagerFmt), Context, cur);
    fputs (buf, fpout);
    fputs ("\n\n", fpout);
  }

  if (mutt_copy_message (fpout, Context, cur, cmflags,
			 (option (OPTWEED) ? (CH_WEED | CH_REORDER) : 0) | CH_DECODE | CH_FROM) == -1)
  {
    fclose (fpout);
    unlink (tempfile);
    return 0;
  }

  if (fclose (fpout) != 0 && errno != EPIPE)
  {
    mutt_perror ("fclose");
    mutt_unlink (tempfile);
    return (0);
  }

  if (builtin)
  {
    pager_t info;
    
    /* Invoke the builtin pager */
    memset (&info, 0, sizeof (pager_t));
    info.hdr = cur;
    info.ctx = Context;
    rc = mutt_pager (NULL, tempfile, 1, &info, attach_msg_status);
  }
  else
  {
    endwin ();
    snprintf (buf, sizeof (buf), "%s %s", NONULL(Pager), tempfile);
    mutt_system (buf);
    unlink (tempfile);
    keypad (stdscr, TRUE);
    mutt_set_flag (Context, cur, M_READ, 1);
    if (option (OPTPROMPTAFTER))
    {
      mutt_ungetch (mutt_any_key_to_continue ("Command: "));
      rc = km_dokey (MENU_PAGER);
    }
    else
      rc = 0;
  }

  return rc;
}

void ci_bounce_message (HEADER *h, int *redraw)
{
  char prompt[SHORT_STRING];
  char buf[HUGE_STRING] = { 0 };
  ADDRESS *adr = NULL;
  int rc;

  snprintf (prompt, sizeof(prompt), "Bounce %smessage%s to: ",
	    h ? "" : "tagged ", h ? "" : "s");
  rc = mutt_get_field (prompt, buf, sizeof (buf), M_ALIAS);

  if (option (OPTNEEDREDRAW))
  {
    unset_option (OPTNEEDREDRAW);
    *redraw = REDRAW_FULL;
  }

  if (rc || !buf[0])
    return;

  if (!(adr = rfc822_parse_adrlist (adr, buf)))
  {
    mutt_error ("Error parsing address!");
    return;
  }

  adr = mutt_expand_aliases (adr);

  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), adr);

  snprintf (prompt, (COLS > sizeof(prompt) ? sizeof(prompt) : COLS) - 13, 
            "Bounce message%s to %s", (h ? "" : "s"), buf);
  strcat(prompt, "...?");
  if (mutt_yesorno (prompt, 1) != 1)
  {
    rfc822_free_address (&adr);
    CLEARLINE (LINES-1);
    return;
  }

  mutt_bounce_message (h, adr);
  rfc822_free_address (&adr);
  mutt_message ("Message%s bounced.", h ? "" : "s");
}

void mutt_pipe_message_to_state (HEADER *h, STATE *s)
{
  if (option (OPTPIPEDECODE))
    mutt_parse_mime_message (Context, h);
  mutt_copy_message (s->fpout, Context, h,
		     option (OPTPIPEDECODE) ? M_CM_DECODE : 0,
		     option (OPTPIPEDECODE) ? CH_FROM | CH_WEED | CH_DECODE : CH_FROM);
}

int mutt_pipe_message (HEADER *h)
{
  STATE s;
  char buffer[LONG_STRING];
  int i, rc = 0; 
  pid_t thepid;

  buffer[0] = 0;
  if (mutt_get_field ("Pipe to command: ", buffer, sizeof (buffer), M_CMD) != 0 ||
      !buffer[0])
    return 0;
  mutt_expand_path (buffer, sizeof (buffer));

  memset (&s, 0, sizeof (s));

  endwin ();
  if (h)
  {



#ifdef _PGPPATH
    if (option (OPTPIPEDECODE))
    {
      mutt_parse_mime_message (Context, h);
      if(h->pgp & PGPENCRYPT && !pgp_valid_passphrase())
	return 1;
    }
    endwin ();
#endif



    thepid = mutt_create_filter (buffer, &s.fpout, NULL, NULL);
    mutt_pipe_message_to_state (h, &s);
    fclose (s.fpout);
    rc = mutt_wait_filter (thepid);
  }
  else
  { /* handle tagged messages */



#ifdef _PGPPATH

    if(option(OPTPIPEDECODE))
    {
      for (i = 0; i < Context->vcount; i++)
	if(Context->hdrs[Context->v2r[i]]->tagged)
	{
	  mutt_parse_mime_message(Context, Context->hdrs[Context->v2r[i]]);
	  if (Context->hdrs[Context->v2r[i]]->pgp & PGPENCRYPT &&
	      !pgp_valid_passphrase())
	    return 1;
	}
    }
#endif
    


    if (option (OPTPIPESPLIT))
    {
      for (i = 0; i < Context->vcount; i++)
      {
        if (Context->hdrs[Context->v2r[i]]->tagged)
        {
	  endwin ();
	  thepid = mutt_create_filter (buffer, &(s.fpout), NULL, NULL);
          mutt_pipe_message_to_state (Context->hdrs[Context->v2r[i]], &s);
          /* add the message separator */
          if (PipeSep)
	    state_puts (PipeSep, &s);
	  fclose (s.fpout);
	  if (mutt_wait_filter (thepid) != 0)
	    rc = 1;
        }
      }
    }
    else
    {
      endwin ();
      thepid = mutt_create_filter (buffer, &(s.fpout), NULL, NULL);
      for (i = 0; i < Context->vcount; i++)
      {
        if (Context->hdrs[Context->v2r[i]]->tagged)
        {
          mutt_pipe_message_to_state (Context->hdrs[Context->v2r[i]], &s);
          /* add the message separator */
          if (PipeSep)
	    state_puts (PipeSep, &s);
        }
      }
      fclose (s.fpout);
      if (mutt_wait_filter (thepid) != 0)
	rc = 1;
    }
  }

  if (rc || option (OPTWAITKEY))
    mutt_any_key_to_continue (NULL);
  return 1;
}

int mutt_select_sort (int reverse)
{
  int method = Sort; /* save the current method in case of abort */
  int ch;

  Sort = 0;
  while (!Sort)
  {
    mvprintw (LINES - 1, 0,
"%sSort (d)ate/(f)rm/(r)ecv/(s)ubj/t(o)/(t)hread/(u)nsort/si(z)e/s(c)ore?: ",
	      reverse ? "Rev-" : "");
    ch = mutt_getch ();
    if (ch == ERR || CI_is_return (ch))
    {
      Sort = method;
      CLEARLINE (LINES-1);
      return (-1);
    }
    switch (ch)
    {
      case 'c':
	Sort = SORT_SCORE;
	break;
      case 'd':
	Sort = SORT_DATE;
	break;
      case 'f':
	Sort = SORT_FROM;
	break;
      case 'o':
	Sort = SORT_TO;
	break;
      case 'r':
	Sort = SORT_RECEIVED;
	break;
      case 's':
	Sort = SORT_SUBJECT;
	break;
      case 't':
	Sort = SORT_THREADS;
	break;
      case 'u':
	Sort = SORT_ORDER;
	break;
      case 'z':
	Sort = SORT_SIZE;
	break;
      default:
	BEEP ();
	break;
    }
  }
  CLEARLINE (LINES-1);
  if (reverse)
    Sort |= SORT_REVERSE;

  return (Sort != method ? 0 : -1); /* no need to resort if it's the same */
}

/* invoke a command in a subshell */
void mutt_shell_escape (void)
{
  char buf[LONG_STRING];

  buf[0] = 0;
  if (mutt_get_field ("Shell command: ", buf, sizeof (buf), M_CMD) == 0)
  {
    if (!buf[0] && Shell)
      strfcpy (buf, Shell, sizeof (buf));
    if(buf[0])
    {
      CLEARLINE (LINES-1);
      endwin ();
      fflush (stdout);
      if (mutt_system (buf) != 0 || option (OPTWAITKEY))
	mutt_any_key_to_continue (NULL);
    }
  }
}

/* enter a mutt command */
void mutt_enter_command (void)
{
  BUFFER err, token;
  char buffer[LONG_STRING], errbuf[SHORT_STRING];
  int r;
  int old_strictthreads = option (OPTSTRICTTHREADS);
  int old_sortre	= option (OPTSORTRE);

  buffer[0] = 0;
  if (mutt_get_field (":", buffer, sizeof (buffer), M_COMMAND) != 0 || !buffer[0])
    return;
  err.data = errbuf;
  err.dsize = sizeof (errbuf);
  memset (&token, 0, sizeof (token));
  r = mutt_parse_rc_line (buffer, &token, &err);
  FREE (&token.data);
  if (errbuf[0])
  {
    /* since errbuf could potentially contain printf() sequences in it,
       we must call mutt_error() in this fashion so that vsprintf()
       doesn't expect more arguments that we passed */
    if (r == 0)
      mutt_message ("%s", errbuf);
    else
      mutt_error ("%s", errbuf);
  }
  if (option (OPTSTRICTTHREADS) != old_strictthreads ||
      option (OPTSORTRE) != old_sortre)
    set_option (OPTNEEDRESORT);
}

void mutt_display_address (ADDRESS *adr)
{
  char buf[SHORT_STRING];

  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), adr);
  mutt_message ("%s", buf);
}

static void set_copy_flags(HEADER *hdr, int decode, int decrypt, int *cmflags, int *chflags)
{
  *cmflags = 0;
  *chflags = CH_UPDATE_LEN;
  
#ifdef _PGPPATH
  if(!decode && decrypt && (hdr->pgp & PGPENCRYPT))
  {
    if(mutt_is_multipart_encrypted(hdr->content))
    {
      *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
      *cmflags = M_CM_DECODE_PGP;
    }
    else if(mutt_is_application_pgp(hdr->content) & PGPENCRYPT)
      decode = 1;
  }
#endif

  if(decode)
  {
    *chflags = CH_MIME | CH_TXTPLAIN;
    *cmflags = M_CM_DECODE;
  }

}

static void _mutt_save_message (HEADER *h, CONTEXT *ctx, int delete, int decode, int decrypt)
{
  int cmflags, chflags;
  
  set_copy_flags(h, decode, decrypt, &cmflags, &chflags);

  if (decode  || decrypt)
    mutt_parse_mime_message (Context, h);

  if (mutt_append_message (ctx, Context, h, cmflags, chflags) == 0 && delete)
  {
    mutt_set_flag (Context, h, M_DELETE, 1);
    mutt_set_flag (Context, h, M_TAG, 0);
  }
}

/* returns 0 if the copy/save was successful, or -1 on error/abort */
int mutt_save_message (HEADER *h, int delete, int decode, int decrypt, int *redraw)
{
  int i, need_buffy_cleanup, need_passphrase = 0;
  char prompt[SHORT_STRING], buf[_POSIX_PATH_MAX];
  CONTEXT ctx;
  struct stat st;
#ifdef BUFFY_SIZE
  BUFFY *tmp = NULL;
#else
  struct utimbuf ut;
#endif

  *redraw = 0;

  snprintf (prompt, sizeof (prompt), "%s%s to mailbox",
	    decode ? (delete ? "Decode-save" : "Decode-copy") :
	    (decrypt ? (delete ? "Decrypt-save" : "Decrypt-copy"):
	     (delete ? "Save" : "Copy")), h ? "" : " tagged");
  
  if (h)
  {
    need_passphrase = h->pgp & PGPENCRYPT;
    mutt_default_save (buf, sizeof (buf), h);
  }
  else
  {
    /* look for the first tagged message */

    for (i = 0; i < Context->vcount; i++)
    {
      if (Context->hdrs[Context->v2r[i]]->tagged)
      {
	h = Context->hdrs[Context->v2r[i]];
	break;
      }
    }

    if (h)
    {
      mutt_default_save (buf, sizeof (buf), h);
      need_passphrase |= h->pgp & PGPENCRYPT;
      h = NULL;
    }
  }

  mutt_pretty_mailbox (buf);
  if (mutt_enter_fname (prompt, buf, sizeof (buf), redraw, 0) == -1)
    return (-1);

  if (*redraw != REDRAW_FULL)
  {
    if (!h)
      *redraw = REDRAW_INDEX | REDRAW_STATUS;
    else
      *redraw = REDRAW_STATUS;
  }

  if (!buf[0])
    return (-1);
 
  /* This is an undocumented feature of ELM pointed out to me by Felix von
   * Leitner <leitner@prz.fu-berlin.de>
   */
  if (strcmp (buf, ".") == 0)
    strfcpy (buf, LastSaveFolder, sizeof (buf));
  else
    strfcpy (LastSaveFolder, buf, sizeof (LastSaveFolder));

  mutt_expand_path (buf, sizeof (buf));

  /* check to make sure that this file is really the one the user wants */
  if (!mutt_save_confirm (buf, &st))
  {
    CLEARLINE (LINES-1);
    return (-1);
  }

  if(need_passphrase && (decode || decrypt) && !pgp_valid_passphrase())
    return -1;
  
  mutt_message ("Copying to %s...", buf);
  
  if (mx_open_mailbox (buf, M_APPEND, &ctx) != NULL)
  {
    if (h)
      _mutt_save_message(h, &ctx, delete, decode, decrypt);
    else
    {
      for (i = 0; i < Context->vcount; i++)
      {
	if (Context->hdrs[Context->v2r[i]]->tagged)
	  _mutt_save_message(Context->hdrs[Context->v2r[i]],
			     &ctx, delete, decode, decrypt);
      }
    }

    need_buffy_cleanup = (ctx.magic == M_MBOX || ctx.magic == M_MMDF);

    mx_close_mailbox (&ctx);

    if (need_buffy_cleanup)
    {
#ifdef BUFFY_SIZE
      tmp = mutt_find_mailbox (buf);
      if (tmp && !tmp->new)
	mutt_update_mailbox (tmp);
#else
      /* fix up the times so buffy won't get confused */
      if (st.st_mtime > st.st_atime)
      {
	ut.actime = st.st_atime;
	ut.modtime = time (NULL);
	utime (buf, &ut); 
      }
      else
	utime (buf, NULL);
#endif
    }

    mutt_clear_error ();
  }

  return (0);
}

static void print_msg (FILE *fp, CONTEXT *ctx, HEADER *h)
{
  


#ifdef _PGPPATH
  if (h->pgp & PGPENCRYPT)
  {
    if (!pgp_valid_passphrase ())
      return;
    endwin ();
  }
#endif



  mutt_parse_mime_message (ctx, h);
  mutt_copy_message (fp, ctx, h, M_CM_DECODE, CH_WEED | CH_DECODE | CH_REORDER);
}

void mutt_print_message (HEADER *h)
{
  int i, count = 0;
  pid_t thepid;
  FILE *fp;

  if (query_quadoption (OPT_PRINT,
			h ? "Print message?" : "Print tagged messages?") != M_YES)
    return;
  endwin ();
  if ((thepid = mutt_create_filter (NONULL(PrintCmd), &fp, NULL, NULL)) == -1)
    return;
  if (h)
  {
    print_msg (fp, Context, h);
    count++;
  }
  else
  {
    for (i = 0 ; i < Context->vcount ; i++)
    {
      if (Context->hdrs[Context->v2r[i]]->tagged)
      {
	print_msg (fp, Context, Context->hdrs[Context->v2r[i]]);
	/* add a formfeed */
	fputc ('\f', fp);
	count++;
      }
    }
  }
  fclose (fp);
  if (mutt_wait_filter (thepid) || option (OPTWAITKEY))
    mutt_any_key_to_continue (NULL);
  mutt_message ("Message%s printed", (count > 1) ? "s" : "");
}

void mutt_version (void)
{
  mutt_message ("Mutt %s (%s)", VERSION, ReleaseDate);
}
