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
#include "mutt_curses.h"
#include "keymap.h"
#include "rfc1524.h"
#include "mime.h"
#include "pager.h"
#include "mailbox.h"
#include "copy.h"
#include "mx.h"

#ifdef _PGPPATH
#include "pgp.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

int mutt_get_tmp_attachment (BODY *a)
{
  char type[STRING];
  char tempfile[_POSIX_PATH_MAX];
  rfc1524_entry *entry = rfc1524_new_entry();
  FILE *fpin = NULL, *fpout = NULL;
  struct stat st;
  
  if(a->unlink)
    return 0;

  snprintf(type, sizeof(type), "%s/%s", TYPE(a), a->subtype);
  rfc1524_mailcap_lookup(a, type, entry, 0);
  rfc1524_expand_filename(entry->nametemplate, a->filename, 
			  tempfile, sizeof(tempfile));
  
  rfc1524_free_entry(&entry);

  if(stat(a->filename, &st) == -1)
    return -1;
  
  if((fpin = fopen(a->filename, "r")) && (fpout = safe_fopen(tempfile, "w")))
  {
    mutt_copy_stream (fpin, fpout);
    FREE(&a->filename);
    a->filename = safe_strdup(tempfile);
    a->unlink = 1;

    if(a->stamp >= st.st_mtime)
      mutt_stamp_attachment(a);
  }
  else
    mutt_perror(fpin ? tempfile : a->filename);
  
  if(fpin)  fclose(fpin);
  if(fpout) fclose(fpout);
  
  return a->unlink ? 0 : -1;
}


/* return 1 if require full screen redraw, 0 otherwise */
int mutt_compose_attachment (BODY *a)
{
  char type[STRING];
  char command[STRING];
  char newfile[_POSIX_PATH_MAX] = "";
  rfc1524_entry *entry = rfc1524_new_entry ();
  short unlink_newfile = 0;
  int rc = 0;
  
  snprintf (type, sizeof (type), "%s/%s", TYPE (a), a->subtype);
  if (rfc1524_mailcap_lookup (a, type, entry, M_COMPOSE))
  {
    if (entry->composecommand || entry->composetypecommand)
    {

      if (entry->composetypecommand)
	strfcpy (command, entry->composetypecommand, sizeof (command));
      else 
	strfcpy (command, entry->composecommand, sizeof (command));
      if (rfc1524_expand_filename (entry->nametemplate,
				      a->filename, newfile, sizeof (newfile)))
      {
	dprint(1, (debugfile, "oldfile: %s\t newfile: %s\n",
				  a->filename, newfile));
	if (safe_symlink (a->filename, newfile) == -1)
	{
	  if (!mutt_yesorno (_("Can't match nametemplate, continue?"), 1))
	    goto bailout;
	}
	unlink_newfile = 1;
      }
      else
	strfcpy(newfile, a->filename, sizeof(newfile));
      
      if (rfc1524_expand_command (a, newfile, type,
				      command, sizeof (command)))
      {
	/* For now, editing requires a file, no piping */
	mutt_error _("Mailcap compose entry requires %%s");
      }
      else
      {
	int r;

	endwin ();
	if ((r = mutt_system (command)) == -1)
	  mutt_error (_("Error running \"%s\"!"), command);
	
	if (r != -1 && entry->composetypecommand)
	{
	  BODY *b;
	  FILE *fp, *tfp;
	  char tempfile[_POSIX_PATH_MAX];

	  if ((fp = safe_fopen (a->filename, "r")) == NULL)
	  {
	    mutt_perror _("Failure to open file to parse headers.");
	    goto bailout;
	  }

	  b = mutt_read_mime_header (fp, 0);
	  if (b)
	  {
	    if (b->parameter)
	    {
	      mutt_free_parameter (&a->parameter);
	      a->parameter = b->parameter;
	      b->parameter = NULL;
	    }
	    if (b->description) {
	      safe_free ((void **) &a->description);
	      a->description = b->description;
	      b->description = NULL;
	    }
	    if (b->form_name)
	    {
	      safe_free ((void **) &a->form_name);
	      a->form_name = b->form_name;
	      b->form_name = NULL;
	    }

	    /* Remove headers by copying out data to another file, then 
	     * copying the file back */
	    fseek (fp, b->offset, 0);
	    mutt_mktemp (tempfile);
	    if ((tfp = safe_fopen (tempfile, "w")) == NULL)
	    {
	      mutt_perror _("Failure to open file to strip headers.");
	      goto bailout;
	    }
	    mutt_copy_stream (fp, tfp);
	    fclose (fp);
	    fclose (tfp);
	    mutt_unlink (a->filename);  
	    mutt_rename_file (tempfile, a->filename); 

	    mutt_free_body (&b);
	  }
	}
      }
    }
  }
  else
  {
    rfc1524_free_entry (&entry);
    mutt_message (_("No mailcap compose entry for %s, creating empty file."),
		   type);
    return 1;
  }

  rc = 1;
  
  bailout:
  
  if(unlink_newfile)
    unlink(newfile);

  rfc1524_free_entry (&entry);
  return rc;
}

/* 
 * Currently, this only works for send mode, as it assumes that the 
 * BODY->filename actually contains the information.  I'm not sure
 * we want to deal with editing attachments we've already received,
 * so this should be ok.
 *
 * Returns 1 if editor found, 0 if not (useful to tell calling menu to
 * redraw)
 */
int mutt_edit_attachment (BODY *a)
{
  char type[STRING];
  char command[STRING];
  char newfile[_POSIX_PATH_MAX] = "";
  rfc1524_entry *entry = rfc1524_new_entry ();
  short unlink_newfile = 0;
  int rc = 0;
  
  snprintf (type, sizeof (type), "%s/%s", TYPE (a), a->subtype);
  if (rfc1524_mailcap_lookup (a, type, entry, M_EDIT))
  {
    if (entry->editcommand)
    {

      strfcpy (command, entry->editcommand, sizeof (command));
      if (rfc1524_expand_filename (entry->nametemplate,
				      a->filename, newfile, sizeof (newfile)))
      {
	dprint(1, (debugfile, "oldfile: %s\t newfile: %s\n",
				  a->filename, newfile));
	if (safe_symlink (a->filename, newfile) == -1)
	{
	  if (!mutt_yesorno (_("Can't match nametemplate, continue?"), 1))
	    goto bailout;
	}
	unlink_newfile = 1;
      }
      else
	strfcpy(newfile, a->filename, sizeof(newfile));

      if (rfc1524_expand_command (a, newfile, type,
				      command, sizeof (command)))
      {
	/* For now, editing requires a file, no piping */
	mutt_error _("Mailcap Edit entry requires %%s");
      }
      else
      {
	endwin ();
	if (mutt_system (command) == -1)
	  mutt_error (_("Error running \"%s\"!"), command);
      }
    }
  }
  else if (a->type == TYPETEXT)
  {
    /* On text, default to editor */
    mutt_edit_file ((!Editor || mutt_strcmp ("builtin", Editor) == 0) ? 
		    NONULL(Visual) : NONULL(Editor), newfile);
  }
  else
  {
    rfc1524_free_entry (&entry);
    mutt_error (_("No mailcap edit entry for %s"),type);
    return 0;
  }

  rc = 1;
  
  bailout:
  
  if(unlink_newfile)
    unlink(newfile);
  
  rfc1524_free_entry (&entry);
  return rc;
}


/* for compatibility with metamail */
static int is_mmnoask (const char *buf)
{
  char tmp[LONG_STRING], *p, *q;
  int lng;

  if ((p = getenv ("MM_NOASK")) != NULL && *p)
  {
    if (mutt_strcmp (p, "1") == 0)
      return (1);

    strfcpy (tmp, p, sizeof (tmp));
    p = tmp;

    while ((p = strtok (p, ",")) != NULL)
    {
      if ((q = strrchr (p, '/')) != NULL)
      {
	if (*(q+1) == '*')
	{
	  if (mutt_strncasecmp (buf, p, q-p) == 0)
	    return (1);
	}
	else
	{
	  if (mutt_strcasecmp (buf, p) == 0)
	    return (1);
	}
      }
      else
      {
	lng = mutt_strlen (p);
	if (buf[lng] == '/' && mutt_strncasecmp (buf, p, lng) == 0)
	  return (1);
      }

      p = NULL;
    }
  }

  return (0);
}

int mutt_is_autoview (BODY *b, const char *type)
{
  LIST *t = AutoViewList;
  char _type[SHORT_STRING];
  int i;

  if (!type)
  {
    snprintf (_type, sizeof (_type), "%s/%s", TYPE (b), b->subtype);
    type = _type;
  }

  if (mutt_needs_mailcap (b))
  {
    if (option (OPTIMPLICITAUTOVIEW))
      return 1;
    
    if (is_mmnoask (type))
      return 1;
  }

  for (; t; t = t->next)
  {
    i = mutt_strlen (t->data) - 1;
    if ((i > 0 && t->data[i-1] == '/' && t->data[i] == '*' && 
	  mutt_strncasecmp (type, t->data, i) == 0) ||
	  mutt_strcasecmp (type, t->data) == 0)
      return 1;
  }

  return 0;
}

/* returns -1 on error, 0 or the return code from mutt_do_pager() on success */
int mutt_view_attachment (FILE *fp, BODY *a, int flag)
{
  char tempfile[_POSIX_PATH_MAX] = "";
  char pagerfile[_POSIX_PATH_MAX] = "";
  int is_message;
  int use_mailcap;
  int use_pipe = 0;
  int use_pager = 1;
  char type[STRING];
  char command[STRING];
  char descrip[STRING];
  char *fname;
  rfc1524_entry *entry = NULL;
  int rc = -1;
  int unlink_tempfile = 0;
  
  is_message = mutt_is_message_type(a->type, a->subtype);
#ifdef _PGPPATH
  if (is_message && a->hdr && (a->hdr->pgp & PGPENCRYPT) && !pgp_valid_passphrase())
    return (rc);
#endif /* _PGPPATH */
  use_mailcap = (flag == M_MAILCAP ||
		(flag == M_REGULAR && mutt_needs_mailcap (a)));
  snprintf (type, sizeof (type), "%s/%s", TYPE (a), a->subtype);
  
  if (use_mailcap)
  {
    entry = rfc1524_new_entry (); 
    if (!rfc1524_mailcap_lookup (a, type, entry, 0))
    {
      if (flag == M_REGULAR)
      {
	/* fallback to view as text */
	rfc1524_free_entry (&entry);
	mutt_error _("No matching mailcap entry found.  Viewing as text.");
	flag = M_AS_TEXT;
	use_mailcap = 0;
      }
      else
	goto return_error;
    }
  }
  
  if (use_mailcap)
  {
    if (!entry->command)
    {
      mutt_error _("MIME type not defined.  Cannot view attachment.");
      goto return_error;
    }
    strfcpy (command, entry->command, sizeof (command));
    
    if (fp)
    {
      fname = safe_strdup (a->filename);
      mutt_sanitize_filename (fname, 1);
    }
    else
      fname = a->filename;

    if (rfc1524_expand_filename (entry->nametemplate, fname,
				 tempfile, sizeof (tempfile)))
    {
      if (fp == NULL && mutt_strcmp(tempfile, a->filename))
      {
	/* send case: the file is already there */
	if (safe_symlink (a->filename, tempfile) == -1)
	{
	  if (mutt_yesorno (_("Can't match nametemplate, continue?"), 1) == M_YES)
	    strfcpy (tempfile, a->filename, sizeof (tempfile));
	  else
	    goto return_error;
	}
	else
	  unlink_tempfile = 1;
      }
    }
    else if (fp == NULL) /* send case */
      strfcpy (tempfile, a->filename, sizeof (tempfile));
    
    if (fp)
    {
      /* recv case: we need to save the attachment to a file */
      FREE (&fname);
      if (mutt_save_attachment (fp, a, tempfile, 0, NULL) == -1)
	goto return_error;
    }

    use_pipe = rfc1524_expand_command (a, tempfile, type,
				       command, sizeof (command));
    use_pager = entry->copiousoutput;
  }
  
  if (use_pager)
  {
    if (fp && !use_mailcap && a->filename)
    {
      /* recv case */
      strfcpy (pagerfile, a->filename, sizeof (pagerfile));
      mutt_adv_mktemp (pagerfile, sizeof(pagerfile));
    }
    else
      mutt_mktemp (pagerfile);
  }
    
  if (use_mailcap)
  {
    pid_t thepid = 0;
    int tempfd = -1, pagerfd = -1;
    
    if (!use_pager)
      endwin ();

    if (use_pager || use_pipe)
    {
      if (use_pager && ((pagerfd = safe_open (pagerfile, O_CREAT | O_EXCL | O_WRONLY)) == -1))
      {
	mutt_perror ("open");
	goto return_error;
      }
      if (use_pipe && ((tempfd = open (tempfile, 0)) == -1))
      {
	if(pagerfd != -1)
	  close(pagerfd);
	mutt_perror ("open");
	goto return_error;
      }

      if ((thepid = mutt_create_filter_fd (command, NULL, NULL, NULL,
					   use_pipe ? tempfd : -1, use_pager ? pagerfd : -1, -1)) == -1)
      {
	if(pagerfd != -1)
	  close(pagerfd);
	
	if(tempfd != -1)
	  close(tempfd);

	mutt_error _("Cannot create filter");
	goto return_error;
      }

      if (use_pager)
      {
	if (a->description)
	  snprintf (descrip, sizeof (descrip),
		    "---Command: %-20.20s Description: %s",
		    command, a->description);
	else
	  snprintf (descrip, sizeof (descrip),
		    "---Command: %-30.30s Attachment: %s", command, type);
      }

      if ((mutt_wait_filter (thepid) || (entry->needsterminal &&
	  option (OPTWAITKEY))) && !use_pager)
	mutt_any_key_to_continue (NULL);
      
      close(tempfd);
      close(pagerfd);
      
    }
    else
    {
      /* interactive command */
      if (mutt_system (command) ||
	  (entry->needsterminal && option (OPTWAITKEY)))
	mutt_any_key_to_continue (NULL);
    }
  }
  else
  {
    /* Don't use mailcap; the attachment is viewed in the pager */

    if (flag == M_AS_TEXT)
    {
      /* just let me see the raw data */
      if (mutt_save_attachment (fp, a, pagerfile, 0, NULL))
	goto return_error;
    }
    else
    {
      /* Use built-in handler */
      set_option (OPTVIEWATTACH); /* disable the "use 'v' to view this part"
				   * message in case of error */
      if (mutt_decode_save_attachment (fp, a, pagerfile, 1, 0))
      {
	unset_option (OPTVIEWATTACH);
	goto return_error;
      }
      unset_option (OPTVIEWATTACH);
    }
    
    if (a->description)
      strfcpy (descrip, a->description, sizeof (descrip));
    else if (a->filename)
      snprintf (descrip, sizeof (descrip), "---Attachment: %s : %s",
	  a->filename, type);
    else
      snprintf (descrip, sizeof (descrip), "---Attachment: %s", type);
  }
  
  /* We only reach this point if there have been no errors */

  if (use_pager)
  {
    pager_t info;
    
    memset (&info, 0, sizeof (info));
    info.fp = fp;
    info.bdy = a;
    info.ctx = Context;
    rc = mutt_do_pager (descrip, pagerfile,
			is_message ? M_PAGER_MESSAGE : 0, &info);
  }
  else
    rc = 0;

  return_error:
  
  if (entry)
    rfc1524_free_entry (&entry);
  if (fp && tempfile[0])
    mutt_unlink (tempfile);
  else if (unlink_tempfile)
    unlink(tempfile);

  if (pagerfile[0])
    mutt_unlink (pagerfile);

  return rc;
}

/* returns 1 on success, 0 on error */
int mutt_pipe_attachment (FILE *fp, BODY *b, const char *path, char *outfile)
{
  pid_t thepid;
  int out = -1;

  if (outfile && *outfile)
    if ((out = safe_open (outfile, O_CREAT | O_EXCL | O_WRONLY)) < 0)
    {
      mutt_perror ("open");
      return 0;
    }

  endwin ();

  if (fp)
  {
    /* recv case */

    STATE s;

    memset (&s, 0, sizeof (STATE));

    if (outfile && *outfile)
      thepid = mutt_create_filter_fd (path, &s.fpout, NULL, NULL, -1, out, -1);
    else
      thepid = mutt_create_filter (path, &s.fpout, NULL, NULL);

    s.fpin = fp;
    mutt_decode_attachment (b, &s);
    fclose (s.fpout);
  }
  else
  {
    /* send case */

    FILE *ifp, *ofp;

    if ((ifp = fopen (b->filename, "r")) == NULL)
    {
      mutt_perror ("fopen");
      if (outfile && *outfile)
      {
	close (out);
	unlink (outfile);
      }
      return 0;
    }

    if (outfile && *outfile)
      thepid = mutt_create_filter_fd (path, &ofp, NULL, NULL, -1, out, -1);
    else
      thepid = mutt_create_filter (path, &ofp, NULL, NULL);

    mutt_copy_stream (ifp, ofp);
    fclose (ofp);
    fclose (ifp);
  }

  if (outfile && *outfile)
    close (out);

  if (mutt_wait_filter (thepid) != 0 || option (OPTWAITKEY))
    mutt_any_key_to_continue (NULL);
  return 1;
}

/* returns 0 on success, -1 on error */
int mutt_save_attachment (FILE *fp, BODY *m, char *path, int flags, HEADER *hdr)
{
  if (fp)
  {
    
    /* recv mode */

    if(hdr &&
	m->hdr &&
	m->encoding != ENCBASE64 &&
	m->encoding != ENCQUOTEDPRINTABLE &&
	mutt_is_message_type(m->type, m->subtype))
    {
      /* message type attachments are written to mail folders. */

      char buf[HUGE_STRING];
      HEADER *hn;
      CONTEXT ctx;
      MESSAGE *msg;
      int chflags = 0;
      int r = -1;
      
      hn = m->hdr;
      hn->msgno = hdr->msgno; /* required for MH/maildir */
      hn->read = 1;

      fseek (fp, m->offset, 0);
      if (fgets (buf, sizeof (buf), fp) == NULL)
	return -1;
      if (mx_open_mailbox(path, M_APPEND | M_QUIET, &ctx) == NULL)
	return -1;
      if ((msg = mx_open_new_message (&ctx, hn, is_from (buf, NULL, 0) ? 0 : M_ADD_FROM)) == NULL)
      {
	mx_close_mailbox(&ctx);
	return -1;
      }
      if (ctx.magic == M_MBOX || ctx.magic == M_MMDF || ctx.magic == M_KENDRA)
	chflags = CH_FROM;
      chflags |= (ctx.magic == M_MAILDIR ? CH_NOSTATUS : CH_UPDATE);
      if ((r = _mutt_copy_message (msg->fp, fp, hn, hn->content, 0, chflags)) == 0)
	mutt_message _("Attachment saved.");
	
      mx_close_message (&msg);
      mx_close_mailbox(&ctx);
      return r;
    }
    else
    {
      /* In recv mode, extract from folder and decode */
      
      STATE s;
      
      memset (&s, 0, sizeof (s));
      if (flags == M_SAVE_APPEND)
	s.fpout = safe_fopen (path, "a");
      else
	s.fpout = fopen (path, "w");
      if (s.fpout == NULL)
      {
	mutt_perror ("fopen");
	return (-1);
      }
      fseek ((s.fpin = fp), m->offset, 0);
      mutt_decode_attachment (m, &s);
      
      if (fclose (s.fpout) != 0)
      {
	mutt_perror ("fclose");
	return (-1);
      }
    }
  }
  else
  {
    /* In send mode, just copy file */

    FILE *ofp, *nfp;

    if ((ofp = fopen (m->filename, "r")) == NULL)
    {
      mutt_perror ("fopen");
      return (-1);
    }

    if ((nfp = safe_fopen (path, "w")) == NULL)
    {
      mutt_perror ("fopen");
      fclose (ofp);
      return (-1);
    }

    if (mutt_copy_stream (ofp, nfp) == -1)
    {
      mutt_error _("Write fault!");
      fclose (ofp);
      fclose (nfp);
      return (-1);
    }
    fclose (ofp);
    fclose (nfp);
  }

  return 0;
}

/* returns 0 on success, -1 on error */
int mutt_decode_save_attachment (FILE *fp, BODY *m, char *path,
				 int displaying, int flags)
{
  STATE s;
  unsigned int saved_encoding = 0;
  BODY *saved_parts = NULL;
  HEADER *saved_hdr = NULL;

  memset (&s, 0, sizeof (s));
  s.flags = (displaying ? M_DISPLAY : 0);

  if (flags == M_SAVE_APPEND)
    s.fpout = safe_fopen (path, "a");
  else
    s.fpout = fopen (path, "w");
  if (s.fpout == NULL)
  {
    perror ("fopen");
    return (-1);
  }

  if (fp == NULL)
  {
    /* When called from the compose menu, the attachment isn't parsed,
     * so we need to do it here. */
    struct stat st;

    if (stat (m->filename, &st) == -1)
    {
      perror ("stat");
      fclose (s.fpout);
      return (-1);
    }

    if ((s.fpin = fopen (m->filename, "r")) == NULL)
    {
      perror ("fopen");
      return (-1);
    }

    saved_encoding = m->encoding;
    m->length = st.st_size;
    m->encoding = ENC8BIT;
    m->offset = 0;
    saved_parts = m->parts;
    saved_hdr = m->hdr;
    mutt_parse_part (s.fpin, m);

    /* display a readable version to the user */
    if (m->noconv)
      s.flags |= M_CHARCONV;
  }
  else
  {
    s.fpin = fp;
    s.flags |= M_CHARCONV;
  }

  mutt_body_handler (m, &s);

  fclose (s.fpout);
  if (fp == NULL)
  {
    m->length = 0;
    m->encoding = saved_encoding;
    if (saved_parts)
    {
      mutt_free_header (&m->hdr);
      m->parts = saved_parts;
      m->hdr = saved_hdr;
    }
    fclose (s.fpin);
  }

  return (0);
}

/* Ok, the difference between send and receive:
 * recv: BODY->filename is a suggested name, and Context|HEADER points
 *       to the attachment in mailbox which is encooded
 * send: BODY->filename points to the un-encoded file which contains the 
 *       attachment
 */

int mutt_print_attachment (FILE *fp, BODY *a)
{
  char newfile[_POSIX_PATH_MAX] = "";
  char type[STRING];
  pid_t thepid;
  FILE *ifp, *fpout;
  short unlink_newfile = 0;
  
  snprintf (type, sizeof (type), "%s/%s", TYPE (a), a->subtype);

  if (rfc1524_mailcap_lookup (a, type, NULL, M_PRINT)) 
  {
    char command[_POSIX_PATH_MAX+STRING];
    rfc1524_entry *entry;
    int piped = FALSE;

    entry = rfc1524_new_entry ();
    rfc1524_mailcap_lookup (a, type, entry, M_PRINT);
    if (rfc1524_expand_filename (entry->nametemplate, a->filename,
						  newfile, sizeof (newfile)))
    {
      if (!fp)
      {
	if (safe_symlink(a->filename, newfile) == -1)
	{
	  if (mutt_yesorno (_("Can't match nametemplate, continue?"), 1) != M_YES)
	  {
	    rfc1524_free_entry (&entry);
	    return 0;
	  }
	  strfcpy (newfile, a->filename, sizeof (newfile));
	}
	else
	  unlink_newfile = 1;
      }
    }

    /* in recv mode, save file to newfile first */
    if (fp)
      mutt_save_attachment (fp, a, newfile, 0, NULL);

    strfcpy (command, entry->printcommand, sizeof (command));
    piped = rfc1524_expand_command (a, newfile, type, command, sizeof (command));

    endwin ();

    /* interactive program */
    if (piped)
    {
      if ((ifp = fopen (newfile, "r")) == NULL)
      {
	mutt_perror ("fopen");
	rfc1524_free_entry (&entry);
	return (0);
      }

      thepid = mutt_create_filter (command, &fpout, NULL, NULL);
      mutt_copy_stream (ifp, fpout);
      fclose (fpout);
      fclose (ifp);
      if (mutt_wait_filter (thepid) || option (OPTWAITKEY))
	mutt_any_key_to_continue (NULL);
    }
    else
    {
      if (mutt_system (command) || option (OPTWAITKEY))
	mutt_any_key_to_continue (NULL);
    }

    if (fp)
      mutt_unlink (newfile);
    else if (unlink_newfile)
      unlink(newfile);

    rfc1524_free_entry (&entry);
    return (1);
  }

  if (!mutt_strcasecmp ("text/plain", a->subtype) ||
      !mutt_strcasecmp ("application/postscript", a->subtype))
  {
    return (mutt_pipe_attachment (fp, a, NONULL(PrintCmd), NULL));
  }
  else if (mutt_can_decode (a))
  {
    /* decode and print */

    int rc = 0;

    mutt_mktemp (newfile);
    if (mutt_decode_save_attachment (fp, a, newfile, 0, 0) == 0)
    {
      if ((ifp = fopen (newfile, "r")) != NULL)
      {
	endwin ();
	thepid = mutt_create_filter (NONULL(PrintCmd), &fpout, NULL, NULL);
	mutt_copy_stream (ifp, fpout);
	fclose (ifp);
	fclose (fpout);
	if (mutt_wait_filter (thepid) != 0 || option (OPTWAITKEY))
	  mutt_any_key_to_continue (NULL);
	rc = 1;
      }
    }
    mutt_unlink (newfile);
    return rc;
  }
  else
  {
    mutt_error _("I don't know how to print that!");
    return 0;
  }
}
