/*
 * Copyright (C) 1996-2000 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2000-4,2006 Thomas Roessler <roessler@does-not-exist.org>
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
#include "mutt_menu.h"
#include "mime.h"
#include "sort.h"
#include "mailbox.h"
#include "copy.h"
#include "mx.h"
#include "pager.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef USE_IMAP
#include "imap.h"
#endif

#include "buffy.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

static const char *ExtPagerProgress = "all";

/* The folder the user last saved to.  Used by ci_save_message() */
static char LastSaveFolder[_POSIX_PATH_MAX] = "";

int mutt_display_message (HEADER *cur)
{
  char tempfile[_POSIX_PATH_MAX], buf[LONG_STRING];
  int rc = 0, builtin = 0;
  int cmflags = M_CM_DECODE | M_CM_DISPLAY | M_CM_CHARCONV;
  FILE *fpout = NULL;
  FILE *fpfilterout = NULL;
  pid_t filterpid = -1;
  int res;

  snprintf (buf, sizeof (buf), "%s/%s", TYPE (cur->content),
	    cur->content->subtype);

  mutt_parse_mime_message (Context, cur);
  mutt_message_hook (Context, cur, M_MESSAGEHOOK);

  /* see if crytpo is needed for this message.  if so, we should exit curses */
  if (WithCrypto && cur->security)
  {
    if (cur->security & ENCRYPT)
    {
      if (cur->security & APPLICATION_SMIME)
	crypt_smime_getkeys (cur->env);
      if(!crypt_valid_passphrase(cur->security))
	return 0;

      cmflags |= M_CM_VERIFY;
    }
    else if (cur->security & SIGN)
    {
      /* find out whether or not the verify signature */
      if (query_quadoption (OPT_VERIFYSIG, _("Verify PGP signature?")) == M_YES)
      {
	cmflags |= M_CM_VERIFY;
      }
    }
  }
  
  if (cmflags & M_CM_VERIFY || cur->security & ENCRYPT)
  {
    if (cur->security & APPLICATION_PGP)
    {
      if (cur->env->from)
        crypt_pgp_invoke_getkeys (cur->env->from);
      
      crypt_invoke_message (APPLICATION_PGP);
    }

    if (cur->security & APPLICATION_SMIME)
      crypt_invoke_message (APPLICATION_SMIME);
  }


  mutt_mktemp (tempfile);
  if ((fpout = safe_fopen (tempfile, "w")) == NULL)
  {
    mutt_error _("Could not create temporary file!");
    return (0);
  }

  if (DisplayFilter && *DisplayFilter) 
  {
    fpfilterout = fpout;
    fpout = NULL;
    /* mutt_endwin (NULL); */
    filterpid = mutt_create_filter_fd (DisplayFilter, &fpout, NULL, NULL,
				       -1, fileno(fpfilterout), -1);
    if (filterpid < 0)
    {
      mutt_error (_("Cannot create display filter"));
      safe_fclose (&fpfilterout);
      unlink (tempfile);
      return 0;
    }
  }

  if (!Pager || mutt_strcmp (Pager, "builtin") == 0)
    builtin = 1;
  else
  {
    struct hdr_format_info hfi;
    hfi.ctx = Context;
    hfi.pager_progress = ExtPagerProgress;
    hfi.hdr = cur;
    mutt_make_string_info (buf, sizeof (buf), NONULL(PagerFmt), &hfi, M_FORMAT_MAKEPRINT);
    fputs (buf, fpout);
    fputs ("\n\n", fpout);
  }

  res = mutt_copy_message (fpout, Context, cur, cmflags,
       	(option (OPTWEED) ? (CH_WEED | CH_REORDER) : 0) | CH_DECODE | CH_FROM | CH_DISPLAY);
  if ((safe_fclose (&fpout) != 0 && errno != EPIPE) || res < 0)
  {
    mutt_error (_("Could not copy message"));
    if (fpfilterout != NULL)
    {
      mutt_wait_filter (filterpid);
      safe_fclose (&fpfilterout);
    }
    mutt_unlink (tempfile);
    return 0;
  }

  if (fpfilterout != NULL && mutt_wait_filter (filterpid) != 0)
    mutt_any_key_to_continue (NULL);

  safe_fclose (&fpfilterout);	/* XXX - check result? */

  
  if (WithCrypto)
  {
    /* update crypto information for this message */
    cur->security &= ~(GOODSIGN|BADSIGN);
    cur->security |= crypt_query (cur->content);
  
    /* Remove color cache for this message, in case there
       are color patterns for both ~g and ~V */
    cur->pair = 0;
  }

  if (builtin)
  {
    pager_t info;

    if (WithCrypto 
        && (cur->security & APPLICATION_SMIME) && (cmflags & M_CM_VERIFY))
    {
      if (cur->security & GOODSIGN)
      {
	if (!crypt_smime_verify_sender(cur))
	  mutt_message ( _("S/MIME signature successfully verified."));
	else
	  mutt_error ( _("S/MIME certificate owner does not match sender."));
      }
      else if (cur->security & PARTSIGN)
	mutt_message (_("Warning: Part of this message has not been signed."));
      else if (cur->security & SIGN || cur->security & BADSIGN)
	mutt_error ( _("S/MIME signature could NOT be verified."));
    }

    if (WithCrypto 
        && (cur->security & APPLICATION_PGP) && (cmflags & M_CM_VERIFY))
    {
      if (cur->security & GOODSIGN)
	mutt_message (_("PGP signature successfully verified."));
      else if (cur->security & PARTSIGN)
	mutt_message (_("Warning: Part of this message has not been signed."));
      else if (cur->security & SIGN)
	mutt_message (_("PGP signature could NOT be verified."));
    }

    /* Invoke the builtin pager */
    memset (&info, 0, sizeof (pager_t));
    info.hdr = cur;
    info.ctx = Context;
    rc = mutt_pager (NULL, tempfile, M_PAGER_MESSAGE, &info);
  }
  else
  {
    int r;

    mutt_endwin (NULL);
    snprintf (buf, sizeof (buf), "%s %s", NONULL(Pager), tempfile);
    if ((r = mutt_system (buf)) == -1)
      mutt_error (_("Error running \"%s\"!"), buf);
    unlink (tempfile);
    keypad (stdscr, TRUE);
    if (r != -1)
      mutt_set_flag (Context, cur, M_READ, 1);
    if (r != -1 && option (OPTPROMPTAFTER))
    {
      mutt_ungetch (mutt_any_key_to_continue _("Command: "), 0);
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
  char scratch[SHORT_STRING];
  char buf[HUGE_STRING] = { 0 };
  ADDRESS *adr = NULL;
  char *err = NULL;
  int rc;

 /* RfC 5322 mandates a From: header, so warn before bouncing
  * messages without one */
  if (h)
  {
    if (!h->env->from)
    {
      mutt_error _("Warning: message has no From: header");
      mutt_sleep (2);
    }
  }
  else if (Context)
  {
    for (rc = 0; rc < Context->msgcount; rc++)
    {
      if (Context->hdrs[rc]->tagged && !Context->hdrs[rc]->env->from)
      {
	mutt_error ("Warning: message has no From: header");
	mutt_sleep (2);
	break;
      }
    }
  }

  if(h)
    strfcpy(prompt, _("Bounce message to: "), sizeof(prompt));
  else
    strfcpy(prompt, _("Bounce tagged messages to: "), sizeof(prompt));
  
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

#define extra_space (15 + 7 + 2)
  snprintf (scratch, sizeof (scratch),
           (h ? _("Bounce message to %s") : _("Bounce messages to %s")), buf);

  if (mutt_strwidth (prompt) > COLS - extra_space)
  {
    mutt_format_string (prompt, sizeof (prompt),
			0, COLS-extra_space, FMT_LEFT, 0,
			scratch, sizeof (scratch), 0);
    safe_strcat (prompt, sizeof (prompt), "...?");
  }
  else
    snprintf (prompt, sizeof (prompt), "%s?", scratch);

  if (query_quadoption (OPT_BOUNCE, prompt) != M_YES)
  {
    rfc822_free_address (&adr);
    CLEARLINE (LINES - 1);
    mutt_message (h ? _("Message not bounced.") : _("Messages not bounced."));
    return;
  }

  CLEARLINE (LINES - 1);
  
  rc = mutt_bounce_message (NULL, h, adr);
  rfc822_free_address (&adr);
  /* If no error, or background, display message. */
  if ((rc == 0) || (rc == S_BKG))
    mutt_message (h ? _("Message bounced.") : _("Messages bounced."));
}

static void pipe_set_flags (int decode, int print, int *cmflags, int *chflags)
{
  if (decode)
  {
    *cmflags |= M_CM_DECODE | M_CM_CHARCONV;
    *chflags |= CH_DECODE | CH_REORDER;
    
    if (option (OPTWEED))
    {
      *chflags |= CH_WEED;
      *cmflags |= M_CM_WEED;
    }
  }
  
  if (print)
    *cmflags |= M_CM_PRINTING;
  
}

static void pipe_msg (HEADER *h, FILE *fp, int decode, int print)
{
  int cmflags = 0;
  int chflags = CH_FROM;
  
  pipe_set_flags (decode, print, &cmflags, &chflags);

  if (WithCrypto && decode && h->security & ENCRYPT)
  {
    if(!crypt_valid_passphrase(h->security))
      return;
    endwin ();
  }

  if (decode)
    mutt_parse_mime_message (Context, h);

  mutt_copy_message (fp, Context, h, cmflags, chflags);
}


/* the following code is shared between printing and piping */

static int _mutt_pipe_message (HEADER *h, char *cmd,
			       int decode,
			       int print,
			       int split,
			       char *sep)
{
  
  int i, rc = 0;
  pid_t thepid;
  FILE *fpout;
  
/*   mutt_endwin (NULL); 

     is this really needed here ? 
     it makes the screen flicker on pgp and s/mime messages,
     before asking for a passphrase...
                                     Oliver Ehli */
  if (h)
  {

    mutt_message_hook (Context, h, M_MESSAGEHOOK);

    if (WithCrypto && decode)
    {
      mutt_parse_mime_message (Context, h);
      if(h->security & ENCRYPT && !crypt_valid_passphrase(h->security))
	return 1;
    }
    mutt_endwin (NULL);

    if ((thepid = mutt_create_filter (cmd, &fpout, NULL, NULL)) < 0)
    {
      mutt_perror _("Can't create filter process");
      return 1;
    }
      
    pipe_msg (h, fpout, decode, print);
    safe_fclose (&fpout);
    rc = mutt_wait_filter (thepid);
  }
  else
  { /* handle tagged messages */

    if (WithCrypto && decode)
    {
      for (i = 0; i < Context->vcount; i++)
	if(Context->hdrs[Context->v2r[i]]->tagged)
	{
	  mutt_message_hook (Context, Context->hdrs[Context->v2r[i]], M_MESSAGEHOOK);
	  mutt_parse_mime_message(Context, Context->hdrs[Context->v2r[i]]);
	  if (Context->hdrs[Context->v2r[i]]->security & ENCRYPT &&
	      !crypt_valid_passphrase(Context->hdrs[Context->v2r[i]]->security))
	    return 1;
	}
    }
    
    if (split)
    {
      for (i = 0; i < Context->vcount; i++)
      {
        if (Context->hdrs[Context->v2r[i]]->tagged)
        {
	  mutt_message_hook (Context, Context->hdrs[Context->v2r[i]], M_MESSAGEHOOK);
	  mutt_endwin (NULL);
	  if ((thepid = mutt_create_filter (cmd, &fpout, NULL, NULL)) < 0)
	  {
	    mutt_perror _("Can't create filter process");
	    return 1;
	  }
          pipe_msg (Context->hdrs[Context->v2r[i]], fpout, decode, print);
          /* add the message separator */
          if (sep)  fputs (sep, fpout);
	  safe_fclose (&fpout);
	  if (mutt_wait_filter (thepid) != 0)
	    rc = 1;
        }
      }
    }
    else
    {
      mutt_endwin (NULL);
      if ((thepid = mutt_create_filter (cmd, &fpout, NULL, NULL)) < 0)
      {
	mutt_perror _("Can't create filter process");
	return 1;
      }
      for (i = 0; i < Context->vcount; i++)
      {
        if (Context->hdrs[Context->v2r[i]]->tagged)
        {
	  mutt_message_hook (Context, Context->hdrs[Context->v2r[i]], M_MESSAGEHOOK);
          pipe_msg (Context->hdrs[Context->v2r[i]], fpout, decode, print);
          /* add the message separator */
          if (sep) fputs (sep, fpout);
        }
      }
      safe_fclose (&fpout);
      if (mutt_wait_filter (thepid) != 0)
	rc = 1;
    }
  }

  if (rc || option (OPTWAITKEY))
    mutt_any_key_to_continue (NULL);
  return rc;
}

void mutt_pipe_message (HEADER *h)
{
  char buffer[LONG_STRING];

  buffer[0] = 0;
  if (mutt_get_field (_("Pipe to command: "), buffer, sizeof (buffer), M_CMD)
      != 0 || !buffer[0])
    return;

  mutt_expand_path (buffer, sizeof (buffer));
  _mutt_pipe_message (h, buffer,
		      option (OPTPIPEDECODE),
		      0, 
		      option (OPTPIPESPLIT),
		      PipeSep);
}

void mutt_print_message (HEADER *h)
{

  if (quadoption (OPT_PRINT) && (!PrintCmd || !*PrintCmd))
  {
    mutt_message (_("No printing command has been defined."));
    return;
  }
  
  if (query_quadoption (OPT_PRINT,
			h ? _("Print message?") : _("Print tagged messages?"))
		  	!= M_YES)
    return;

  if (_mutt_pipe_message (h, PrintCmd,
			  option (OPTPRINTDECODE),
			  1,
			  option (OPTPRINTSPLIT),
			  "\f") == 0)
    mutt_message (h ? _("Message printed") : _("Messages printed"));
  else
    mutt_message (h ? _("Message could not be printed") :
		  _("Messages could not be printed"));
}


int mutt_select_sort (int reverse)
{
  int method = Sort; /* save the current method in case of abort */

  switch (mutt_multi_choice (reverse ?
			     _("Rev-Sort (d)ate/(f)rm/(r)ecv/(s)ubj/t(o)/(t)hread/(u)nsort/si(z)e/s(c)ore/s(p)am?: ") :
			     _("Sort (d)ate/(f)rm/(r)ecv/(s)ubj/t(o)/(t)hread/(u)nsort/si(z)e/s(c)ore/s(p)am?: "),
			     _("dfrsotuzcp")))
  {
  case -1: /* abort - don't resort */
    return -1;

  case 1: /* (d)ate */
    Sort = SORT_DATE;
    break;

  case 2: /* (f)rm */
    Sort = SORT_FROM;
    break;
  
  case 3: /* (r)ecv */
    Sort = SORT_RECEIVED;
    break;
  
  case 4: /* (s)ubj */
    Sort = SORT_SUBJECT;
    break;
  
  case 5: /* t(o) */
    Sort = SORT_TO;
    break;
  
  case 6: /* (t)hread */
    Sort = SORT_THREADS;
    break;
  
  case 7: /* (u)nsort */
    Sort = SORT_ORDER;
    break;
  
  case 8: /* si(z)e */
    Sort = SORT_SIZE;
    break;
  
  case 9: /* s(c)ore */ 
    Sort = SORT_SCORE;
    break;

  case 10: /* s(p)am */
    Sort = SORT_SPAM;
    break;
  }
  if (reverse)
    Sort |= SORT_REVERSE;

  return (Sort != method ? 0 : -1); /* no need to resort if it's the same */
}

/* invoke a command in a subshell */
void mutt_shell_escape (void)
{
  char buf[LONG_STRING];

  buf[0] = 0;
  if (mutt_get_field (_("Shell command: "), buf, sizeof (buf), M_CMD) == 0)
  {
    if (!buf[0] && Shell)
      strfcpy (buf, Shell, sizeof (buf));
    if(buf[0])
    {
      CLEARLINE (LINES-1);
      mutt_endwin (NULL);
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
  char buffer[LONG_STRING], errbuf[LONG_STRING];
  int r;

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
}

void mutt_display_address (ENVELOPE *env)
{
  char *pfx = NULL;
  char buf[SHORT_STRING];
  ADDRESS *adr = NULL;

  adr = mutt_get_address (env, &pfx);

  if (!adr) return;
  
  /* 
   * Note: We don't convert IDNA to local representation this time.
   * That is intentional, so the user has an opportunity to copy &
   * paste the on-the-wire form of the address to other, IDN-unable
   * software. 
   */
  
  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), adr, 0);
  mutt_message ("%s: %s", pfx, buf);
}

static void set_copy_flags (HEADER *hdr, int decode, int decrypt, int *cmflags, int *chflags)
{
  *cmflags = 0;
  *chflags = CH_UPDATE_LEN;
  
  if (WithCrypto && !decode && decrypt && (hdr->security & ENCRYPT))
  {
    if ((WithCrypto & APPLICATION_PGP)
        && mutt_is_multipart_encrypted(hdr->content))
    {
      *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
      *cmflags = M_CM_DECODE_PGP;
    }
    else if ((WithCrypto & APPLICATION_PGP)
              && mutt_is_application_pgp (hdr->content) & ENCRYPT)
      decode = 1;
    else if ((WithCrypto & APPLICATION_SMIME)
             && mutt_is_application_smime(hdr->content) & ENCRYPT)
    {
      *chflags = CH_NONEWLINE | CH_XMIT | CH_MIME;
      *cmflags = M_CM_DECODE_SMIME;
    }
  }

  if (decode)
  {
    *chflags = CH_XMIT | CH_MIME | CH_TXTPLAIN;
    *cmflags = M_CM_DECODE | M_CM_CHARCONV;

    if (!decrypt)	/* If decode doesn't kick in for decrypt, */
    {
      *chflags |= CH_DECODE;	/* then decode RFC 2047 headers, */

      if (option (OPTWEED))
      {
	*chflags |= CH_WEED;	/* and respect $weed. */
	*cmflags |= M_CM_WEED;
      }
    }
  }
}

int _mutt_save_message (HEADER *h, CONTEXT *ctx, int delete, int decode, int decrypt)
{
  int cmflags, chflags;
  int rc;
  
  set_copy_flags (h, decode, decrypt, &cmflags, &chflags);

  if (decode || decrypt)
    mutt_parse_mime_message (Context, h);

  if ((rc = mutt_append_message (ctx, Context, h, cmflags, chflags)) != 0)
    return rc;

  if (delete)
  {
    mutt_set_flag (Context, h, M_DELETE, 1);
    if (option (OPTDELETEUNTAG))
      mutt_set_flag (Context, h, M_TAG, 0);
  }
  
  return 0;
}

/* returns 0 if the copy/save was successful, or -1 on error/abort */
int mutt_save_message (HEADER *h, int delete, 
		       int decode, int decrypt, int *redraw)
{
  int i, need_buffy_cleanup;
  int need_passphrase = 0, app=0;
  char prompt[SHORT_STRING], buf[_POSIX_PATH_MAX];
  CONTEXT ctx;
  struct stat st;

  *redraw = 0;

  
  snprintf (prompt, sizeof (prompt),
	    decode  ? (delete ? _("Decode-save%s to mailbox") :
		       _("Decode-copy%s to mailbox")) :
	    (decrypt ? (delete ? _("Decrypt-save%s to mailbox") :
			_("Decrypt-copy%s to mailbox")) :
	     (delete ? _("Save%s to mailbox") : _("Copy%s to mailbox"))),
	    h ? "" : _(" tagged"));
  

  if (h)
  {
    if (WithCrypto)
    {
      need_passphrase = h->security & ENCRYPT;
      app = h->security;
    }
    mutt_message_hook (Context, h, M_MESSAGEHOOK);
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
      mutt_message_hook (Context, h, M_MESSAGEHOOK);
      mutt_default_save (buf, sizeof (buf), h);
      if (WithCrypto)
      {
        need_passphrase = h->security & ENCRYPT;
        app = h->security;
      }
      h = NULL;
    }
  }

  mutt_pretty_mailbox (buf, sizeof (buf));
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
  if (mutt_strcmp (buf, ".") == 0)
    strfcpy (buf, LastSaveFolder, sizeof (buf));
  else
    strfcpy (LastSaveFolder, buf, sizeof (LastSaveFolder));

  mutt_expand_path (buf, sizeof (buf));

  /* check to make sure that this file is really the one the user wants */
  if (mutt_save_confirm (buf, &st) != 0)
    return -1;

  if (WithCrypto && need_passphrase && (decode || decrypt)
      && !crypt_valid_passphrase(app))
    return -1;
  
  mutt_message (_("Copying to %s..."), buf);
  
#ifdef USE_IMAP
  if (Context->magic == M_IMAP && 
      !(decode || decrypt) && mx_is_imap (buf))
  {
    switch (imap_copy_messages (Context, h, buf, delete))
    {
      /* success */
      case 0: mutt_clear_error (); return 0;
      /* non-fatal error: fall through to fetch/append */
      case 1: break;
      /* fatal error, abort */
      case -1: return -1;
    }
  }
#endif

  if (mx_open_mailbox (buf, M_APPEND, &ctx) != NULL)
  {
    if (h)
    {
      if (_mutt_save_message(h, &ctx, delete, decode, decrypt) != 0)
      {
        mx_close_mailbox (&ctx, NULL);
        return -1;
      }
    }
    else
    {
      for (i = 0; i < Context->vcount; i++)
      {
	if (Context->hdrs[Context->v2r[i]]->tagged)
	{
	  mutt_message_hook (Context, Context->hdrs[Context->v2r[i]], M_MESSAGEHOOK);
	  if (_mutt_save_message(Context->hdrs[Context->v2r[i]],
			     &ctx, delete, decode, decrypt) != 0)
          {
            mx_close_mailbox (&ctx, NULL);
            return -1;
          }
	}
      }
    }

    need_buffy_cleanup = (ctx.magic == M_MBOX || ctx.magic == M_MMDF);

    mx_close_mailbox (&ctx, NULL);

    if (need_buffy_cleanup)
      mutt_buffy_cleanup (ctx.path, &st);

    mutt_clear_error ();
    return (0);
  }
  
  return -1;
}


void mutt_version (void)
{
  mutt_message ("Mutt %s (%s)", MUTT_VERSION, ReleaseDate);
}

void mutt_edit_content_type (HEADER *h, BODY *b, FILE *fp)
{
  char buf[LONG_STRING];
  char obuf[LONG_STRING];
  char tmp[STRING];
  PARAMETER *p;

  char charset[STRING];
  char *cp;

  short charset_changed = 0;
  short type_changed = 0;
  
  cp = mutt_get_parameter ("charset", b->parameter);
  strfcpy (charset, NONULL (cp), sizeof (charset));

  snprintf (buf, sizeof (buf), "%s/%s", TYPE (b), b->subtype);
  strfcpy (obuf, buf, sizeof (obuf));
  if (b->parameter)
  {
    size_t l;
    
    for (p = b->parameter; p; p = p->next)
    {
      l = strlen (buf);

      rfc822_cat (tmp, sizeof (tmp), p->value, MimeSpecials);
      snprintf (buf + l, sizeof (buf) - l, "; %s=%s", p->attribute, tmp);
    }
  }
  
  if (mutt_get_field ("Content-Type: ", buf, sizeof (buf), 0) != 0 ||
      buf[0] == 0)
    return;
  
  /* clean up previous junk */
  mutt_free_parameter (&b->parameter);
  FREE (&b->subtype);
  
  mutt_parse_content_type (buf, b);

  
  snprintf (tmp, sizeof (tmp), "%s/%s", TYPE (b), NONULL (b->subtype));
  type_changed = ascii_strcasecmp (tmp, obuf);
  charset_changed = ascii_strcasecmp (charset, mutt_get_parameter ("charset", b->parameter));

  /* if in send mode, check for conversion - current setting is default. */

  if (!h && b->type == TYPETEXT && charset_changed)
  {
    int r;
    snprintf (tmp, sizeof (tmp), _("Convert to %s upon sending?"),
	      mutt_get_parameter ("charset", b->parameter));
    if ((r = mutt_yesorno (tmp, !b->noconv)) != -1)
      b->noconv = (r == M_NO);
  }

  /* inform the user */
  
  snprintf (tmp, sizeof (tmp), "%s/%s", TYPE (b), NONULL (b->subtype));
  if (type_changed)
    mutt_message (_("Content-Type changed to %s."), tmp);
  if (b->type == TYPETEXT && charset_changed)
  {
    if (type_changed)
      mutt_sleep (1);
    mutt_message (_("Character set changed to %s; %s."),
		  mutt_get_parameter ("charset", b->parameter),
		  b->noconv ? _("not converting") : _("converting"));
  }

  b->force_charset |= charset_changed ? 1 : 0;

  if (!is_multipart (b) && b->parts)
    mutt_free_body (&b->parts);
  if (!mutt_is_message_type (b->type, b->subtype) && b->hdr)
  {
    b->hdr->content = NULL;
    mutt_free_header (&b->hdr);
  }

  if (fp && (is_multipart (b) || mutt_is_message_type (b->type, b->subtype)))
    mutt_parse_part (fp, b);
  
  if (WithCrypto && h)
  {
    if (h->content == b)
      h->security  = 0;

    h->security |= crypt_query (b);
  }
}


static int _mutt_check_traditional_pgp (HEADER *h, int *redraw)
{
  MESSAGE *msg;
  int rv = 0;
  
  h->security |= PGP_TRADITIONAL_CHECKED;
  
  mutt_parse_mime_message (Context, h);
  if ((msg = mx_open_message (Context, h->msgno)) == NULL)
    return 0;
  if (crypt_pgp_check_traditional (msg->fp, h->content, 0))
  {
    h->security = crypt_query (h->content);
    *redraw |= REDRAW_FULL;
    rv = 1;
  }
  
  h->security |= PGP_TRADITIONAL_CHECKED;
  mx_close_message (&msg);
  return rv;
}

int mutt_check_traditional_pgp (HEADER *h, int *redraw)
{
  int i;
  int rv = 0;
  if (h && !(h->security & PGP_TRADITIONAL_CHECKED))
    rv = _mutt_check_traditional_pgp (h, redraw);
  else
  {
    for (i = 0; i < Context->vcount; i++)
      if (Context->hdrs[Context->v2r[i]]->tagged && 
	  !(Context->hdrs[Context->v2r[i]]->security & PGP_TRADITIONAL_CHECKED))
	rv = _mutt_check_traditional_pgp (Context->hdrs[Context->v2r[i]], redraw)
	  || rv;
  }
  return rv;
}


