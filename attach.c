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

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

/* return 1 if require full screen redraw, 0 otherwise */
int mutt_compose_attachment (BODY *a)
{
  char type[STRING];
  char command[STRING];
  rfc1524_entry *entry = rfc1524_new_entry ();

  snprintf (type, sizeof (type), "%s/%s", TYPE (a->type), a->subtype);
  if (rfc1524_mailcap_lookup (a, type, entry, M_COMPOSE))
  {
    if (entry->composecommand || entry->composetypecommand)
    {
      char newfile[_POSIX_PATH_MAX] = "";

      if (entry->composetypecommand)
	strfcpy (command, entry->composetypecommand, sizeof (command));
      else 
	strfcpy (command, entry->composecommand, sizeof (command));
      if (rfc1524_expand_filename (entry->nametemplate,
				      a->filename, newfile, sizeof (newfile)))
      {
	dprint(1, (debugfile, "oldfile: %s\t newfile: %s\n",
				  a->filename, newfile));
	if (!mutt_rename_file (a->filename, newfile))
	{
	  if (!mutt_yesorno ("Can't match nametemplate, continue?", 1))
	    return 0;
	}
	else
	{
	  safe_free ((void **) &a->filename);
	  a->filename = safe_strdup (newfile);
	}
      }

      if (rfc1524_expand_command (a, a->filename, type,
				      command, sizeof (command)))
      {
	/* For now, editing requires a file, no piping */
	mutt_error ("Mailcap compose entry requires %%s");
      }
      else
      {
	endwin ();
	mutt_system (command);
	if (entry->composetypecommand)
	{
	  BODY *b;
	  FILE *fp, *tfp;
	  char tempfile[_POSIX_PATH_MAX];

	  if ((fp = safe_fopen (a->filename, "r")) == NULL)
	  {
	    mutt_perror ("Failure to open file to parse headers.");
	    return 0;
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
	      mutt_perror ("Failure to open file to strip headers.");
	      return 0;
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
    mutt_message ("No mailcap compose entry for %s, creating empty file.",type);
    return 1;
  }

  rfc1524_free_entry (&entry);
  return 1;
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
int mutt_edit_attachment (BODY *a, int opt)
{
  char type[STRING];
  char command[STRING];
  rfc1524_entry *entry = rfc1524_new_entry ();

  snprintf (type, sizeof (type), "%s/%s", TYPE (a->type), a->subtype);
  if (rfc1524_mailcap_lookup (a, type, entry, M_EDIT))
  {
    if (entry->editcommand)
    {
      char newfile[_POSIX_PATH_MAX] = "";

      strfcpy (command, entry->editcommand, sizeof (command));
      if (rfc1524_expand_filename (entry->nametemplate,
				      a->filename, newfile, sizeof (newfile)))
      {
	dprint(1, (debugfile, "oldfile: %s\t newfile: %s\n",
				  a->filename, newfile));
	if (mutt_rename_file (a->filename, newfile))
	{
	  if (!mutt_yesorno ("Can't match nametemplate, continue?", 1))
	    return 0;
	}
	else
	{
	  safe_free ((void **) &a->filename);
	  a->filename = safe_strdup (newfile);
	}
      }

      if (rfc1524_expand_command (a, a->filename, type,
				      command, sizeof (command)))
      {
	/* For now, editing requires a file, no piping */
	mutt_error ("Mailcap Edit entry requires %%s");
      }
      else
      {
	endwin ();
	mutt_system (command);
      }
    }
  }
  else if (a->type == TYPETEXT)
  {
    /* On text, default to editor */
    mutt_edit_file (strcmp ("builtin", Editor) == 0 ? Visual : Editor, 
		    a->filename);
  }
  else
  {
    rfc1524_free_entry (&entry);
    mutt_error ("No mailcap edit entry for %s",type);
    return 0;
  }

  rfc1524_free_entry (&entry);
  return 1;
}

int mutt_is_autoview (char *type)
{
  LIST *t = AutoViewList;
  int i;

  while (t)
  {
    i = strlen (t->data) - 1;
    if ((i > 0 && t->data[i-1] == '/' && t->data[i] == '*' && 
	  strncasecmp (type, t->data, i) == 0) ||
	  strcasecmp (type, t->data) == 0)
      return 1;
    t = t->next;
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
  rfc1524_entry *entry = NULL;
  int rc = -1;

  is_message = (a->type == TYPEMESSAGE && a->subtype &&
		  (!strcasecmp (a->subtype,"rfc822") ||
		   !strcasecmp (a->subtype, "news")));
  use_mailcap = (flag == M_MAILCAP ||
		(flag == M_REGULAR && mutt_needs_mailcap (a)));
  snprintf (type, sizeof (type), "%s/%s", TYPE (a->type), a->subtype);
  
  if (use_mailcap)
  {
    entry = rfc1524_new_entry (); 
    if (!rfc1524_mailcap_lookup (a, type, entry, 0))
    {
      if (flag == M_REGULAR)
      {
	/* fallback to view as text */
	rfc1524_free_entry (&entry);
	mutt_error ("No matching mailcap entry found.  Viewing as text.");
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
      mutt_error ("MIME type not defined.  Cannot view attachment.");
      goto return_error;
    }
    strfcpy (command, entry->command, sizeof (command));
    
    if (rfc1524_expand_filename (entry->nametemplate, a->filename,
				 tempfile, sizeof (tempfile)))
    {
      if (fp == NULL)
      {
	/* send case: the file is already there */
	if (mutt_rename_file (a->filename, tempfile))
	{
	  if (mutt_yesorno ("Can't match nametemplate, continue?", 1) == M_YES)
	    strfcpy (tempfile, a->filename, sizeof (tempfile));
	  else
	    goto return_error;
	}
	else
	{
	  safe_free ((void **) &a->filename);
	  a->filename = safe_strdup (tempfile);
	}
      }
    }
    else if (fp == NULL) /* send case */
      strfcpy (tempfile, a->filename, sizeof (tempfile));

    if (fp)
    {
      /* recv case: we need to save the attachment to a file */
      if (mutt_save_attachment (fp, a, tempfile, 0) == -1)
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
      mutt_adv_mktemp (pagerfile);
    }
    else
      mutt_mktemp (pagerfile);
  }
    
  if (use_mailcap)
  {
    pid_t thepid = 0;
    FILE *pagerfp = NULL;
    FILE *tempfp = NULL;
    FILE *filter_in;
    FILE *filter_out;

    if (!use_pager)
      endwin ();

    if (use_pager || use_pipe)
    {
      if (use_pager && ((pagerfp = safe_fopen (pagerfile, "w")) == NULL))
      {
	mutt_perror ("fopen");
	goto return_error;
      }
      if (use_pipe && ((tempfp = fopen (tempfile, "r")) == NULL))
      {
	if (pagerfp)
	  fclose (pagerfp);
	mutt_perror ("fopen");
	goto return_error;
      }

      if ((thepid = mutt_create_filter (command, use_pipe ? &filter_in : NULL,
					use_pager ? &filter_out : NULL, NULL)) == -1)
      {
	if (pagerfp)
	  fclose (pagerfp);
	if (tempfp)
	  fclose (tempfp);
	mutt_error ("Cannot create filter");
	goto return_error;
      }

      if (use_pipe)
      {
	mutt_copy_stream (tempfp, filter_in);
	fclose (tempfp);
	fclose (filter_in);
      }
      if (use_pager)
      {
	mutt_copy_stream (filter_out, pagerfp);
	fclose (filter_out);
	fclose (pagerfp);
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
      if (mutt_save_attachment (fp, a, pagerfile, 0))
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
    rc = mutt_do_pager (descrip, pagerfile, is_message, &info);
  }
  else
    rc = 0;

  return_error:
  
  if (entry)
    rfc1524_free_entry (&entry);
  if (fp && tempfile[0])
    mutt_unlink (tempfile);
  if (pagerfile[0])
    mutt_unlink (pagerfile);

  return rc;
}

/* returns 1 on success, 0 on error */
int mutt_pipe_attachment (FILE *fp, BODY *b, const char *path, char *outfile)
{
  STATE o;
  pid_t thepid;

  memset (&o, 0, sizeof (STATE));

  if (outfile && *outfile)
    if ((o.fpout = safe_fopen (outfile, "w")) == NULL)
    {
      mutt_perror ("fopen");
      return 0;
    }

  endwin ();

  if (fp)
  {
    /* recv case */

    STATE s;

    memset (&s, 0, sizeof (STATE));

    if (outfile && *outfile)
      thepid = mutt_create_filter (path, &s.fpout, &o.fpin, NULL);
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
      fclose (o.fpout);
      return 0;
    }

    if (outfile && *outfile)
      thepid = mutt_create_filter (path, &ofp, &o.fpin, NULL);
    else
      thepid = mutt_create_filter (path, &ofp, NULL, NULL);

    mutt_copy_stream (ifp, ofp);
    fclose (ofp);
    fclose (ifp);
  }

  if (outfile && *outfile)
  {
    mutt_copy_stream (o.fpin, o.fpout);
    fclose (o.fpin);
    fclose (o.fpout);
  }

  if (mutt_wait_filter (thepid) != 0 || option (OPTWAITKEY))
    mutt_any_key_to_continue (NULL);
  return 1;
}

/* returns 0 on success, -1 on error */
int mutt_save_attachment (FILE *fp, BODY *m, char *path, int flags)
{
  if (fp)
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
      mutt_error ("Write fault!");
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

  memset (&s, 0, sizeof (s));
  s.flags = displaying ? M_DISPLAY : 0;

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
    if (m->type == TYPEMESSAGE && m->subtype &&
		  (!strcasecmp (m->subtype,"rfc822") ||
		   !strcasecmp (m->subtype, "news")))
      m->parts = mutt_parse_messageRFC822 (s.fpin, m);
  }
  else
    s.fpin = fp;

  mutt_body_handler (m, &s);

  fclose (s.fpout);
  if (fp == NULL)
  {
    m->length = 0;
    m->encoding = saved_encoding;
    if (m->parts)
      mutt_free_body (&m->parts);
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

  snprintf (type, sizeof (type), "%s/%s", TYPE (a->type), a->subtype);

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
	/* only attempt file move in send mode */

	if (mutt_rename_file (a->filename, newfile))
	{
	  if (mutt_yesorno ("Can't match nametemplate, continue?", 1) != M_YES)
	  {
	    rfc1524_free_entry (&entry);
	    return 0;
	  }
	  strfcpy (newfile, a->filename, sizeof (newfile));
	}
	else
	{
	  safe_free ((void **)&a->filename);
	  a->filename = safe_strdup (newfile);
	}
      }
    }

    /* in recv mode, save file to newfile first */
    if (fp)
      mutt_save_attachment (fp, a, newfile, 0);

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

    rfc1524_free_entry (&entry);
    return (1);
  }

  if (!strcasecmp ("text/plain", a->subtype) ||
      !strcasecmp ("application/postscript", a->subtype))
    return (mutt_pipe_attachment (fp, a, PrintCmd, NULL));
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
	thepid = mutt_create_filter (PrintCmd, &fpout, NULL, NULL);
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
    mutt_error ("I don't know how to print that!");
    return 0;
  }
}
