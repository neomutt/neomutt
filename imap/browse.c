/*
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999 Brendan Cully <brendan@kublai.com>
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

/* Mutt browser support routines */

#include <stdlib.h>
#include <ctype.h>

#include "mutt.h"
#include "imap.h"
#include "imap_private.h"
#include "imap_socket.h"

/* -- forward declarations -- */
static int add_list_result (CONNECTION *conn, const char *seq, const char *cmd,
  struct browser_state *state, short isparent);
static void imap_add_folder (char delim, char *folder, int noselect,
  int noinferiors, struct browser_state *state, short isparent);
static int compare_names(struct folder_file *a, struct folder_file *b);
static int get_namespace (IMAP_DATA *idata, char *nsbuf, int nsblen, 
  IMAP_NAMESPACE_INFO *nsi, int nsilen, int *nns);
static int verify_namespace (CONNECTION *conn, IMAP_NAMESPACE_INFO *nsi, 
  int nns);

int imap_init_browse (char *path, struct browser_state *state)
{
  CONNECTION *conn;
  IMAP_DATA *idata;
  char buf[LONG_STRING];
  char nsbuf[LONG_STRING];
  char mbox[LONG_STRING];
  char host[SHORT_STRING];
  int port;
  char list_cmd[5];
  char seq[16];
  char *ipath = NULL;
  IMAP_NAMESPACE_INFO nsi[16];
  int home_namespace = 0;
  int n;
  int i;
  int nsup;
  char ctmp;
  int nns;
  char *cur_folder;
  short showparents = 0;
  int noselect;
  int noinferiors;

  if (imap_parse_path (path, host, sizeof (host), &port, &ipath))
    return (-1);

  strfcpy (list_cmd, option (OPTIMAPLSUB) ? "LSUB" : "LIST", sizeof (list_cmd));

  conn = mutt_socket_select_connection (host, port, 0);
  idata = CONN_DATA;

  if (!idata || (idata->state == IMAP_DISCONNECTED))
  {
    if (!idata)
    {
      /* The current connection is a new connection */
      idata = safe_calloc (1, sizeof (IMAP_DATA));
      conn->data = idata;
      idata->conn = conn;
    }
    if (imap_open_connection (idata, conn))
      return (-1);
  }

  if (ipath[0] == '\0')
  {
    home_namespace = 1;
    mbox[0] = 0;		/* Do not replace "" with "INBOX" here */
    ipath = ImapHomeNamespace;
    nns = 0;
    if (mutt_bit_isset(idata->capabilities,NAMESPACE))
    {
      if (get_namespace (idata, nsbuf, sizeof (nsbuf), 
			 nsi, sizeof (nsi),  &nns) != 0)
	return (-1);
      if (verify_namespace (conn, nsi, nns) != 0)
	return (-1);
    }
    if (!ipath)		/* Any explicitly set imap_home_namespace wins */
    { 
      for (i = 0; i < nns; i++)
	if (nsi[i].listable &&
	    (nsi[i].type == IMAP_NS_PERSONAL || nsi[i].type == IMAP_NS_SHARED))
	{
	  ipath = nsi->prefix;
	  nsi->home_namespace = 1;
	  break;
	}
    }
  }

  mutt_message _("Contacted server, getting folder list...");

  if (ipath && ipath[0] != '\0')
  {
    imap_fix_path (idata, ipath, mbox, sizeof (mbox));
    n = mutt_strlen (mbox);

    dprint (3, (debugfile, "imap_init_browse: mbox: %s\n", mbox));

    /* if our target exists, has inferiors and isn't selectable, enter it if we
     * aren't already going to */
    if (mbox[n-1] != idata->delim)
    {
      imap_make_sequence (seq, sizeof (seq));
      snprintf (buf, sizeof (buf), "%s %s \"\" \"%s\"\r\n", seq, list_cmd,
        mbox);
      mutt_socket_write (conn, buf);
      do 
      {
        if (imap_parse_list_response(conn, buf, sizeof(buf), &cur_folder,
    	    &noselect, &noinferiors, &(idata->delim)) != 0)
          return -1;

        if (cur_folder)
        {
          imap_unquote_string (cur_folder);

          if (noselect && !noinferiors && cur_folder[0] &&
            (n = strlen (mbox)) < LONG_STRING-1)
          {
            mbox[n++] = idata->delim;
            mbox[n] = '\0';
          }
        }
      }
      while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
    }

    /* if we're descending a folder, mark it as current in browser_state */
    if (mbox[n-1] == idata->delim)
    {
      showparents = 1;
      imap_qualify_path (buf, sizeof (buf), host, port, mbox, NULL);
      state->folder = safe_strdup (buf);
      n--;
    }

    /* Find superiors to list */
    for (n--; n >= 0 && mbox[n] != idata->delim ; n--);
    if (n > 0)			/* "aaaa/bbbb/" -> "aaaa/" */
    {
      n++;
      ctmp = mbox[n];
      mbox[n] = '\0';
      /* List it to see if it can be selected */
      dprint (2, (debugfile, "imap_init_browse: listing %s\n", mbox));
      imap_make_sequence (seq, sizeof (seq));
      snprintf (buf, sizeof (buf), "%s %s \"\" \"%s\"\r\n", seq, 
        list_cmd, mbox);
      /* add this entry as a superior, if we aren't tab-completing */
      if (showparents && add_list_result (conn, seq, buf, state, 1))
          return -1;
      /* if our target isn't a folder, we are in our superior */
      if (!state->folder)
      {
        imap_qualify_path (buf, sizeof (buf), host, port, mbox, NULL);
        state->folder = safe_strdup (buf);
      }
      mbox[n] = ctmp;
    } 
    /* "/bbbb/" -> add  "/", "aaaa/" -> add "" */
    else
    {
      char relpath[2];
      /* folder may be "/" */
      snprintf (relpath, sizeof (relpath), "%c" , n < 0 ? '\0' : idata->delim);
      if (showparents)
        imap_add_folder (idata->delim, relpath, 1, 0, state, 1); 
      if (!state->folder)
      {
        imap_qualify_path (buf, sizeof (buf), host, port, relpath, NULL);
        state->folder = safe_strdup (buf);
      }
    }
  }

  /* no namespace, no folder: set folder to host only */
  if (!state->folder)
  {
    imap_qualify_path (buf, sizeof (buf), host, port, NULL, NULL);
    state->folder = safe_strdup (buf);
  }

  if (home_namespace && mbox[0] != '\0')
  {
    /* Listing the home namespace, so INBOX should be included. Home 
     * namespace is not "", so we have to list it explicitly. We ask the 
     * server to see if it has descendants. */
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s LIST \"\" \"INBOX\"\r\n", seq);
    if (add_list_result (conn, seq, buf, state, 0))
      return -1;
  }

  nsup = state->entrylen;

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s %s \"\" \"%s%%\"\r\n", seq, 
    list_cmd, mbox);
  if (add_list_result (conn, seq, buf, state, 0))
    return -1;

  qsort(&(state->entry[nsup]),state->entrylen-nsup,sizeof(state->entry[0]),
	(int (*)(const void*,const void*)) compare_names);
  if (home_namespace)
  {				/* List additional namespaces */
    for (i = 0; i < nns; i++)
      if (nsi[i].listable && !nsi[i].home_namespace)
	imap_add_folder(nsi[i].delim, nsi[i].prefix, nsi[i].noselect,
          nsi[i].noinferiors, state, 0);
  }

  mutt_clear_error ();
  return 0;
}

static int add_list_result (CONNECTION *conn, const char *seq, const char *cmd,
			    struct browser_state *state, short isparent)
{
  IMAP_DATA *idata = CONN_DATA;
  char buf[LONG_STRING];
  char host[SHORT_STRING];
  int port;
  char *curfolder;
  char *name;
  int noselect;
  int noinferiors;

  if (imap_parse_path (state->folder, host, sizeof (host), &port, &curfolder))
  {
    dprint (2, (debugfile,
      "add_list_result: current folder %s makes no sense\n", state->folder));
    return -1;
  }

  mutt_socket_write (conn, cmd);

  do 
  {
    if (imap_parse_list_response(conn, buf, sizeof(buf), &name,
        &noselect, &noinferiors, &(idata->delim)) != 0)
      return -1;

    if (name)
    {
      imap_unquote_string (name);
      /* prune current folder from output */
      if (isparent || strncmp (name, curfolder, strlen (name)))
        imap_add_folder (idata->delim, name, noselect, noinferiors, state,
          isparent);
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  return (0);
}

/* imap_add_folder: add a folder name to the browser list, formatting it as
 *   necessary. NOTE: check for duplicate folders removed, believed to be
 *   useless. Tell me if otherwise (brendan@kublai.com) */
static void imap_add_folder (char delim, char *folder, int noselect,
  int noinferiors, struct browser_state *state, short isparent)
{
  char tmp[LONG_STRING];
  char relpath[LONG_STRING];
  char host[SHORT_STRING];
  int port;
  char *curfolder;
  int flen = strlen (folder);

  if (imap_parse_path (state->folder, host, sizeof (host), &port, &curfolder))
    return;

  imap_unquote_string (folder);

  /* plus 2: folder may be selectable AND have inferiors */
  if (state->entrylen + 2 == state->entrymax)
  {
    safe_realloc ((void **) &state->entry,
	sizeof (struct folder_file) * (state->entrymax += 256));
  }

  /* render superiors as unix-standard ".." */
  if (isparent)
    strfcpy (relpath, "../", sizeof (relpath));
  /* strip current folder from target, to render a relative path */
  else if (!strncmp (curfolder, folder, strlen (curfolder)))
    strfcpy (relpath, folder + strlen (curfolder), sizeof (relpath));
  else
    strfcpy (relpath, folder, sizeof (relpath));

  /* apply filemask filter. This should really be done at menu setup rather
   * than at scan, since it's so expensive to scan. But that's big changes
   * to browser.c */
  if (!((regexec (Mask.rx, relpath, 0, NULL, 0) == 0) ^ Mask.not))
    return;

  if (!noselect)
  {
    imap_qualify_path (tmp, sizeof (tmp), host, port, folder, NULL);
    (state->entry)[state->entrylen].name = safe_strdup (tmp);

    (state->entry)[state->entrylen].desc = safe_strdup (relpath);

    (state->entry)[state->entrylen].notfolder = 0;
    (state->entrylen)++;
  }
  if (!noinferiors)
  {
    char trailing_delim[2];

    /* create trailing delimiter if necessary */
    trailing_delim[1] = '\0';
    trailing_delim[0] = (flen && folder[flen - 1] != delim) ? delim : '\0';

    imap_qualify_path (tmp, sizeof (tmp), host, port, folder, trailing_delim);
    (state->entry)[state->entrylen].name = safe_strdup (tmp);

    if (strlen (relpath) < sizeof (relpath) - 2)
      strcat (relpath, trailing_delim);
    (state->entry)[state->entrylen].desc = safe_strdup (relpath);

    (state->entry)[state->entrylen].notfolder = 1;
    (state->entrylen)++;
  }
}

static int compare_names(struct folder_file *a, struct folder_file *b) 
{
  return mutt_strcmp(a->name, b->name);
}

static int get_namespace (IMAP_DATA *idata, char *nsbuf, int nsblen, 
			  IMAP_NAMESPACE_INFO *nsi, int nsilen, int *nns)
{
  char buf[LONG_STRING];
  char seq[16];
  char *s;
  int n;
  char ns[LONG_STRING];
  char delim = '/';
  int type;
  int nsbused = 0;

  *nns = 0;
  nsbuf[nsblen-1] = '\0';

  imap_make_sequence (seq, sizeof (seq));
  snprintf (buf, sizeof (buf), "%s NAMESPACE\r\n", seq);

  mutt_socket_write (idata->conn, buf);
  do 
  {
    if (mutt_socket_read_line_d (buf, sizeof (buf), idata->conn) < 0)
    {
      return (-1);
    }

    if (buf[0] == '*') 
    {
      s = imap_next_word (buf);
      if (mutt_strncasecmp ("NAMESPACE", s, 9) == 0)
      {
	/* There are three sections to the response, User, Other, Shared,
	 * and maybe more by extension */
	for (type = IMAP_NS_PERSONAL; *s; type++)
	{
	  s = imap_next_word (s);
	  if (*s && strncmp (s, "NIL", 3))
	  {
	    s++;
	    while (*s && *s != ')')
	    {
	      s++; /* skip ( */
	      /* copy namespace */
	      n = 0;
	      if (*s == '\"')
	      {
		s++;
		while (*s && *s != '\"') 
		{
		  if (*s == '\\')
		    s++;
		  ns[n++] = *s;
		  s++;
		}
	      }
	      else
		while (*s && !ISSPACE (*s)) 
		{
		  ns[n++] = *s;
		  s++;
		}
	      ns[n] = '\0';
	      /* delim? */
	      s = imap_next_word (s);
	      if (*s && *s == '\"')
              {
		if (s[1] && s[2] == '\"')
		  delim = s[1];
		else if (s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
		  delim = s[2];
              }
	      /* Save result (if space) */
	      if ((nsbused < nsblen) && (*nns < nsilen))
	      {
		nsi->type = type;
		strncpy(nsbuf+nsbused,ns,nsblen-nsbused-1);
		nsi->prefix = nsbuf+nsbused;
		nsbused += n+1;
		nsi->delim = delim;
		nsi++;
		(*nns)++;
	      }
	      while (*s && *s != ')') s++;
	      s++;
	    }
	  }
	}
      }
      else
      {
	if (imap_handle_untagged (idata, buf) != 0)
	  return (-1);
      }
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  return 0;
}

/* Check which namespaces actually exist */
static int verify_namespace (CONNECTION *conn, IMAP_NAMESPACE_INFO *nsi, 
  int nns)
{
  char buf[LONG_STRING];
  char seq[16];
  int i = 0;
  char *name;
  char delim;

  for (i = 0; i < nns; i++, nsi++)
  {
    imap_make_sequence (seq, sizeof (seq));
    snprintf (buf, sizeof (buf), "%s %s \"\" \"%s\"\r\n", seq, 
	      option (OPTIMAPLSUB) ? "LSUB" : "LIST", nsi->prefix);
    mutt_socket_write (conn, buf);

    nsi->listable = 0;
    nsi->home_namespace = 0;
    do 
    {
      if (imap_parse_list_response(conn, buf, sizeof(buf), &name,
				   &(nsi->noselect), &(nsi->noinferiors), 
				   &delim) != 0)
	return (-1);
      nsi->listable |= (name != NULL);
    }
    while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  }
  return (0);
}

