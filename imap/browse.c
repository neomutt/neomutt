/*
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2000 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* Mutt browser support routines */

#include <stdlib.h>
#include <ctype.h>

#include "mutt.h"
#include "imap_private.h"

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
  char list_cmd[5];
  char seq[16];
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
  IMAP_MBOX mx;

  if (imap_parse_path (path, &mx))
  {
    mutt_error ("%s is an invalid IMAP path", path);
    return -1;
  }

  strfcpy (list_cmd, option (OPTIMAPLSUB) ? "LSUB" : "LIST", sizeof (list_cmd));

  conn = mutt_socket_find (&mx, 0);
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
      return -1;
  }

  if (mx.mbox[0] == '\0')
  {
    home_namespace = 1;
    mbox[0] = '\0';		/* Do not replace "" with "INBOX" here */
    mx.mbox = ImapHomeNamespace;
    nns = 0;
    if (mutt_bit_isset(idata->capabilities,NAMESPACE))
    {
      mutt_message _("Getting namespaces...");
      if (get_namespace (idata, nsbuf, sizeof (nsbuf), 
			 nsi, sizeof (nsi),  &nns) != 0)
	return -1;
      if (verify_namespace (conn, nsi, nns) != 0)
	return -1;
    }
    /* What if you have a shared namespace of ""? You'll never be
     * able to browse it. This isn't conjecture: connect to the Cyrus
     * reference server (cyrus.andrew.cmu.edu) as anonymous. argh! */
#if 0
    if (!mx.mbox)   /* Any explicitly set imap_home_namespace wins */
    { 
      for (i = 0; i < nns; i++)
	if (nsi[i].listable &&
	    (nsi[i].type == IMAP_NS_PERSONAL || nsi[i].type == IMAP_NS_SHARED))
	{
	  mx.mbox = nsi->prefix;
	  nsi->home_namespace = 1;
	  break;
	}
    }
    else
      dprint (4, (debugfile, "Home namespace: %s\n", mx.mbox));
#endif
  }

  mutt_message _("Getting folder list...");

  /* skip check for parents when at the root */
  if (mx.mbox && mx.mbox[0] != '\0')
  {
    imap_fix_path (idata, mx.mbox, mbox, sizeof (mbox));
    imap_munge_mbox_name (buf, sizeof (buf), mbox);
    imap_unquote_string(buf); /* As kludgy as it gets */
    mbox[sizeof (mbox) - 1] = '\0';
    strncpy (mbox, buf, sizeof (mbox) - 1);
    n = mutt_strlen (mbox);

    dprint (3, (debugfile, "imap_init_browse: mbox: %s\n", mbox));

    /* if our target exists and has inferiors, enter it if we
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
          imap_unmunge_mbox_name (cur_folder);

          if (!noinferiors && cur_folder[0] &&
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
      /* don't show parents in the home namespace */
      if (!home_namespace)
	showparents = 1;
      imap_qualify_path (buf, sizeof (buf), &mx, mbox, NULL);
      state->folder = safe_strdup (buf);
      n--;
    }

    /* Find superiors to list
     * Note: UW-IMAP servers return folder + delimiter when asked to list
     *  folder + delimiter. Cyrus servers don't. So we ask for folder,
     *  and tack on delimiter ourselves.
     * Further note: UW-IMAP servers return nothing when asked for 
     *  NAMESPACES without delimiters at the end. Argh! */
    for (n--; n >= 0 && mbox[n] != idata->delim ; n--);
    if (n > 0)			/* "aaaa/bbbb/" -> "aaaa" */
    {
      /* forget the check, it is too delicate (see above). Have we ever
       * had the parent not exist? */
      ctmp = mbox[n];
      mbox[n] = '\0';
#if 0
      /* List it to see if it can be selected */
      dprint (2, (debugfile, "imap_init_browse: listing possible parent %s\n", mbox));
      imap_make_sequence (seq, sizeof (seq));
      snprintf (buf, sizeof (buf), "%s %s \"\" \"%s\"\r\n", seq, 
        list_cmd, mbox);
      /* add this entry as a superior, if we aren't tab-completing */
      if (showparents && add_list_result (conn, seq, buf, state, 1))
          return -1;
#else
      if (showparents)
      {
	dprint (2, (debugfile, "imap_init_browse: adding parent %s\n", mbox));
	imap_add_folder (idata->delim, mbox, 1, 0, state, 1);
      }
#endif
      /* if our target isn't a folder, we are in our superior */
      if (!state->folder)
      {
        /* store folder with delimiter */
        mbox[n++] = ctmp;
        ctmp = mbox[n];
        mbox[n] = '\0';
        imap_qualify_path (buf, sizeof (buf), &mx, mbox, NULL);
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
        imap_qualify_path (buf, sizeof (buf), &mx, relpath, NULL);
        state->folder = safe_strdup (buf);
      }
    }
  }

  /* no namespace, no folder: set folder to host only */
  if (!state->folder)
  {
    imap_qualify_path (buf, sizeof (buf), &mx, NULL, NULL);
    state->folder = safe_strdup (buf);
  }

  if (home_namespace && mbox[0] != '\0')
  {
    /* Listing the home namespace, so INBOX should be included. Home 
     * namespace is not "", so we have to list it explicitly. We ask the 
     * server to see if it has descendants. */
    dprint (4, (debugfile, "imap_init_browse: adding INBOX\n"));
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
      if (nsi[i].listable && !nsi[i].home_namespace) {
	imap_add_folder(nsi[i].delim, nsi[i].prefix, nsi[i].noselect,
			nsi[i].noinferiors, state, 0);
	dprint (4, (debugfile, "imap_init_browse: adding namespace: %s\n",
		    nsi[i].prefix));
      }
  }

  mutt_clear_error ();
  return 0;
}

static int add_list_result (CONNECTION *conn, const char *seq, const char *cmd,
			    struct browser_state *state, short isparent)
{
  IMAP_DATA *idata = CONN_DATA;
  char buf[LONG_STRING];
  char *name;
  int noselect;
  int noinferiors;
  IMAP_MBOX mx;

  if (imap_parse_path (state->folder, &mx))
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
      /* Let a parent folder never be selectable for navigation */
      if (isparent)
        noselect = 1;
      /* prune current folder from output */
      if (isparent || strncmp (name, mx.mbox, strlen (name)))
        imap_add_folder (idata->delim, name, noselect, noinferiors, state,
          isparent);
    }
  }
  while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  return (0);
}

/* imap_add_folder: add a folder name to the browser list, formatting it as
 *   necessary. */
static void imap_add_folder (char delim, char *folder, int noselect,
  int noinferiors, struct browser_state *state, short isparent)
{
  char tmp[LONG_STRING];
  char relpath[LONG_STRING];
  IMAP_MBOX mx;

  if (imap_parse_path (state->folder, &mx))
    return;

  imap_unmunge_mbox_name (folder);

  if (state->entrylen + 1 == state->entrymax)
  {
    safe_realloc ((void **) &state->entry,
      sizeof (struct folder_file) * (state->entrymax += 256));
    memset (state->entry + state->entrylen, 0,
      (sizeof (struct folder_file) * (state->entrymax - state->entrylen)));
  }

  /* render superiors as unix-standard ".." */
  if (isparent)
    strfcpy (relpath, "../", sizeof (relpath));
  /* strip current folder from target, to render a relative path */
  else if (!strncmp (mx.mbox, folder, strlen (mx.mbox)))
    strfcpy (relpath, folder + strlen (mx.mbox), sizeof (relpath));
  else
    strfcpy (relpath, folder, sizeof (relpath));

  /* apply filemask filter. This should really be done at menu setup rather
   * than at scan, since it's so expensive to scan. But that's big changes
   * to browser.c */
  if (!((regexec (Mask.rx, relpath, 0, NULL, 0) == 0) ^ Mask.not))
    return;

  imap_qualify_path (tmp, sizeof (tmp), &mx, folder, NULL);
  (state->entry)[state->entrylen].name = safe_strdup (tmp);

  /* mark desc with delim in browser if it can have subfolders */
  if (!isparent && !noinferiors && strlen (relpath) < sizeof (relpath) - 1)
  {
    relpath[strlen (relpath) + 1] = '\0';
    relpath[strlen (relpath)] = delim;
  }
  
  (state->entry)[state->entrylen].desc = safe_strdup (relpath);

  (state->entry)[state->entrylen].imap = 1;
  /* delimiter at the root is useless. */
  if (folder[0] == '\0')
    delim = '\0';
  (state->entry)[state->entrylen].delim = delim;
  (state->entry)[state->entrylen].selectable = !noselect;
  (state->entry)[state->entrylen].inferiors = !noinferiors;
  (state->entrylen)++;
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
    if (mutt_socket_readln (buf, sizeof (buf), idata->conn) < 0)
      return -1;

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
	      delim = '\0';

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
	      /* delimiter is meaningless if namespace is "". Why does
	       * Cyrus provide one?! */
	      if (n && *s && *s == '\"')
              {
		if (s[1] && s[2] == '\"')
		  delim = s[1];
		else if (s[1] && s[1] == '\\' && s[2] && s[3] == '\"')
		  delim = s[2];
              }
	      /* skip "" namespaces, they are already listed at the root */
	      if ((ns[0] != '\0') && (nsbused < nsblen) && (*nns < nsilen))
	      {
		dprint (4, (debugfile, "get_namespace: adding %s\n", ns));
		nsi->type = type;
		/* Cyrus doesn't append the delimiter to the namespace,
		 * but UW-IMAP does. We'll strip it here and add it back
		 * as if it were a normal directory, from the browser */
		if (n && (ns[n-1] == delim))
		  ns[--n] = '\0';
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

/* Check which namespaces have contents */
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
    /* Cyrus gives back nothing if the % isn't added. This may return lots
     * of data in some cases, I guess, but I currently feel that's better
     * than invisible namespaces */
    if (nsi->delim)
      snprintf (buf, sizeof (buf), "%s %s \"\" \"%s%c%%\"\r\n", seq,
		option (OPTIMAPLSUB) ? "LSUB" : "LIST", nsi->prefix,
		nsi->delim);
    else
      snprintf (buf, sizeof (buf), "%s %s \"\" \"%s%%\"\r\n", seq,
		option (OPTIMAPLSUB) ? "LSUB" : "LIST", nsi->prefix);
    
    mutt_socket_write (conn, buf);

    nsi->listable = 0;
    nsi->home_namespace = 0;
    do 
    {
      if (imap_parse_list_response(conn, buf, sizeof(buf), &name,
          &(nsi->noselect), &(nsi->noinferiors), &delim) != 0)
	return -1;
      nsi->listable |= (name != NULL);
    }
    while ((mutt_strncmp (buf, seq, SEQLEN) != 0));
  }
  return (0);
}

