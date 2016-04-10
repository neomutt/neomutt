/* 
 * Copyright (C) 1996-2000,2010,2013 Michael R. Elkins <me@mutt.org>
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
#include "buffy.h"
#include "mailbox.h"
#include "mx.h"

#include "mutt_curses.h"

#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif

#ifdef USE_IMAP
#include "imap.h"
#endif

#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <utime.h>
#include <ctype.h>
#include <unistd.h>

#include <stdio.h>

static time_t BuffyTime = 0;	/* last time we started checking for mail */
time_t BuffyDoneTime = 0;	/* last time we knew for sure how much mail there was. */
static short BuffyCount = 0;	/* how many boxes with new mail */
static short BuffyNotify = 0;	/* # of unnotified new boxes */

static BUFFY* buffy_get (const char *path);

/* Find the last message in the file. 
 * upon success return 0. If no message found - return -1 */

static int fseek_last_message (FILE * f)
{
  LOFF_T pos;
  char buffer[BUFSIZ + 9];	/* 7 for "\n\nFrom " */
  int bytes_read;
  int i;			/* Index into `buffer' for scanning.  */

  memset (buffer, 0, sizeof(buffer));
  fseek (f, 0, SEEK_END);
  pos = ftello (f);

  /* Set `bytes_read' to the size of the last, probably partial, buffer; 0 <
   * `bytes_read' <= `BUFSIZ'.  */
  bytes_read = pos % BUFSIZ;
  if (bytes_read == 0)
    bytes_read = BUFSIZ;
  /* Make `pos' a multiple of `BUFSIZ' (0 if the file is short), so that all
   * reads will be on block boundaries, which might increase efficiency.  */
  while ((pos -= bytes_read) >= 0)
  {
    /* we save in the buffer at the end the first 7 chars from the last read */
    strncpy (buffer + BUFSIZ, buffer, 5+2); /* 2 == 2 * mutt_strlen(CRLF) */
    fseeko (f, pos, SEEK_SET);
    bytes_read = fread (buffer, sizeof (char), bytes_read, f);
    if (bytes_read == -1)
      return -1;
    for (i = bytes_read; --i >= 0;)
      if (!mutt_strncmp (buffer + i, "\n\nFrom ", mutt_strlen ("\n\nFrom ")))
      {				/* found it - go to the beginning of the From */
	fseeko (f, pos + i + 2, SEEK_SET);
	return 0;
      }
    bytes_read = BUFSIZ;
  }

  /* here we are at the beginning of the file */
  if (!mutt_strncmp ("From ", buffer, 5))
  {
    fseek (f, 0, 0);
    return (0);
  }

  return (-1);
}

/* Return 1 if the last message is new */
static int test_last_status_new (FILE * f)
{
  HEADER *hdr;
  ENVELOPE* tmp_envelope;
  int result = 0;

  if (fseek_last_message (f) == -1)
    return (0);

  hdr = mutt_new_header ();
  tmp_envelope = mutt_read_rfc822_header (f, hdr, 0, 0);
  if (!(hdr->read || hdr->old))
    result = 1;

  mutt_free_envelope(&tmp_envelope);
  mutt_free_header (&hdr);

  return result;
}

static int test_new_folder (const char *path)
{
  FILE *f;
  int rc = 0;
  int typ;

  typ = mx_get_magic (path);

  if (typ != M_MBOX && typ != M_MMDF)
    return 0;

  if ((f = fopen (path, "rb")))
  {
    rc = test_last_status_new (f);
    safe_fclose (&f);
  }

  return rc;
}

void mutt_buffy_cleanup (const char *buf, struct stat *st)
{
  struct utimbuf ut;
  BUFFY *tmp;

  if (option(OPTCHECKMBOXSIZE))
  {
    tmp = mutt_find_mailbox (buf);
    if (tmp && !tmp->new)
      mutt_update_mailbox (tmp);
  }
  else
  {
    /* fix up the times so buffy won't get confused */
    if (st->st_mtime > st->st_atime)
    {
      ut.actime = st->st_atime;
      ut.modtime = time (NULL);
      utime (buf, &ut); 
    }
    else
      utime (buf, NULL);
  }
}

BUFFY *mutt_find_mailbox (const char *path)
{
  BUFFY *tmp = NULL;
  struct stat sb;
  struct stat tmp_sb;
  
  if (stat (path,&sb) != 0)
    return NULL;

  for (tmp = Incoming; tmp; tmp = tmp->next)
  {
    if (stat (tmp->path,&tmp_sb) ==0 && 
	sb.st_dev == tmp_sb.st_dev && sb.st_ino == tmp_sb.st_ino)
      break;
  }
  return tmp;
}

void mutt_update_mailbox (BUFFY * b)
{
  struct stat sb;

  if (!b)
    return;

  if (stat (b->path, &sb) == 0)
    b->size = (off_t) sb.st_size;
  else
    b->size = 0;
  return;
}

static BUFFY *buffy_new (const char *path)
{
  BUFFY* buffy;
#ifdef USE_SIDEBAR
  char rp[PATH_MAX] = "";
  char *r = NULL;
#endif

  buffy = (BUFFY *) safe_calloc (1, sizeof (BUFFY));
  strfcpy (buffy->path, path, sizeof (buffy->path));
#ifdef USE_SIDEBAR
  r = realpath (path, rp);
  strfcpy (buffy->realpath, r ? rp : path, sizeof (buffy->realpath));
#endif
  buffy->next = NULL;
  buffy->magic = 0;

  return buffy;
}

static void buffy_free (BUFFY **mailbox)
{
  if (mailbox && *mailbox)
    FREE (&(*mailbox)->desc);
  FREE (mailbox); /* __FREE_CHECKED__ */
}

int mutt_parse_mailboxes (BUFFER *path, BUFFER *s, unsigned long data, BUFFER *err)
{
  BUFFY **tmp,*tmp1;
  char buf[_POSIX_PATH_MAX];
  struct stat sb;
  char f1[PATH_MAX];
#ifndef USE_SIDEBAR
  char f2[PATH_MAX];
#endif
  char *p, *q;

  while (MoreArgs (s))
  {
    mutt_extract_token (path, s, 0);
    strfcpy (buf, path->data, sizeof (buf));

    if(data == M_UNMAILBOXES && mutt_strcmp(buf,"*") == 0)
    {
      for (tmp = &Incoming; *tmp;)
      {
        tmp1=(*tmp)->next;
#ifdef USE_SIDEBAR
	sb_notify_mailbox (*tmp, 0);
#endif
        buffy_free (tmp);
        *tmp=tmp1;
      }
      return 0;
    }

    mutt_expand_path (buf, sizeof (buf));

    /* Skip empty tokens. */
    if(!*buf) continue;

    /* avoid duplicates */
    p = realpath (buf, f1);
    for (tmp = &Incoming; *tmp; tmp = &((*tmp)->next))
    {
#ifdef USE_SIDEBAR
      q = (*tmp)->realpath;
      if (mutt_strcmp (p ? p : buf, q) == 0)
#else
      q = realpath ((*tmp)->path, f2);
      if (mutt_strcmp (p ? p : buf, q ? q : (*tmp)->path) == 0)
#endif
      {
	dprint(3,(debugfile,"mailbox '%s' already registered as '%s'\n", buf, (*tmp)->path));
	break;
      }
    }

    if(data == M_UNMAILBOXES)
    {
      if(*tmp)
      {
        tmp1=(*tmp)->next;
#ifdef USE_SIDEBAR
	sb_notify_mailbox (*tmp, 0);
#endif
        buffy_free (tmp);
        *tmp=tmp1;
      }
      continue;
    }

    if (!*tmp) {
      *tmp = buffy_new (buf);
#ifdef USE_SIDEBAR
      sb_notify_mailbox (*tmp, 1);
#endif
    }

    (*tmp)->new = 0;
    (*tmp)->notified = 1;
    (*tmp)->newly_created = 0;

    /* for check_mbox_size, it is important that if the folder is new (tested by
     * reading it), the size is set to 0 so that later when we check we see
     * that it increased .  without check_mbox_size we probably don't care.
     */
    if (option(OPTCHECKMBOXSIZE) &&
	stat ((*tmp)->path, &sb) == 0 && !test_new_folder ((*tmp)->path))
    {
      /* some systems out there don't have an off_t type */
      (*tmp)->size = (off_t) sb.st_size;
    }
    else
      (*tmp)->size = 0;
  }
  return 0;
}

/* returns 1 if the specified dir (cur or new) has new mail */
static int buffy_maildir_dir_hasnew(BUFFY* mailbox, const char *dir_name)
{
  char path[_POSIX_PATH_MAX];
  DIR *dirp;
  struct dirent *de;
  char *p;
  int rc = 0;
  struct stat sb;

  snprintf (path, sizeof (path), "%s/%s", mailbox->path, dir_name);

  /* when $mail_check_recent is set, if the new/ directory hasn't been modified since
   * the user last exited the mailbox, then we know there is no recent mail.
   */
  if (option(OPTMAILCHECKRECENT))
  {
    if (stat(path, &sb) == 0 && sb.st_mtime < mailbox->last_visited)
      return 0;
  }

#ifdef USE_SIDEBAR
  if (option (OPTSIDEBAR) && mailbox->msg_unread > 0) {
    mailbox->new = 1;
    return 1;
  }
#endif

  if ((dirp = opendir (path)) == NULL)
  {
    mailbox->magic = 0;
    return 0;
  }

  while ((de = readdir (dirp)) != NULL)
  {
    if (*de->d_name == '.')
      continue;

    if (!(p = strstr (de->d_name, ":2,")) || !(strchr (p + 3, 'T') || strchr(p + 3, 'S')))
    {
      if (option(OPTMAILCHECKRECENT))
      {
	char msgpath[_POSIX_PATH_MAX];

	snprintf(msgpath, sizeof(msgpath), "%s/%s", path, de->d_name);
	/* ensure this message was received since leaving this mailbox */
	if (stat(msgpath, &sb) == 0 && (sb.st_ctime <= mailbox->last_visited))
	  continue;
      }
      /* one new and undeleted message is enough */
      mailbox->new = 1;
      rc = 1;
      break;
    }
  }

  closedir (dirp);

  return rc;
}

#ifdef USE_NOTMUCH
int mutt_parse_virtual_mailboxes (BUFFER *path, BUFFER *s, unsigned long data, BUFFER *err)
{
  BUFFY **tmp;
  char buf[_POSIX_PATH_MAX];

  while (MoreArgs (s))
  {
    char *desc;

    mutt_extract_token (path, s, 0);
    if (path->data && *path->data)
      desc = safe_strdup( path->data);
    else
      continue;

    mutt_extract_token (path, s, 0);
    strfcpy (buf, path->data, sizeof (buf));

    /* Skip empty tokens. */
    if(!*buf) {
	    FREE(&desc);
	    continue;
    }

    /* avoid duplicates */
    for (tmp = &VirtIncoming; *tmp; tmp = &((*tmp)->next))
    {
      if (mutt_strcmp (buf, (*tmp)->path) == 0)
      {
	dprint(3,(debugfile,"vistual mailbox '%s' already registered as '%s'\n", buf, (*tmp)->path));
	break;
      }
    }

    if (!*tmp)
      *tmp = buffy_new (buf);

    (*tmp)->new = 0;
    (*tmp)->notified = 1;
    (*tmp)->newly_created = 0;
    (*tmp)->size = 0;
    (*tmp)->desc = desc;
  }
  return 0;
}
#endif

/* returns 1 if maildir has new mail */
static int buffy_maildir_hasnew (BUFFY* mailbox)
{
  if (buffy_maildir_dir_hasnew(mailbox, "new")) {
      return 1;
  }

  if (!option(OPTMAILDIRCHECKCUR)) {
      return 0;
  }

  if (buffy_maildir_dir_hasnew(mailbox, "cur")) {
      return 1;
  }

  return 0;
}

#ifdef USE_SIDEBAR
/**
 * buffy_maildir_update - Update messages counts for a maildir mailbox
 * @mailbox: BUFFY representing a maildir mailbox
 *
 * Open a mailbox directories and update our record of how many new, or
 * flagged, messages there are.
 */
void
buffy_maildir_update (BUFFY *mailbox)
{
	char path[_POSIX_PATH_MAX] = "";
	DIR *dirp = NULL;
	struct dirent *de = NULL;
	char *p = NULL;

	if (!option (OPTSIDEBAR) || !mailbox)
		return;

	mailbox->msg_count   = 0;
	mailbox->msg_unread  = 0;
	mailbox->msg_flagged = 0;

	snprintf (path, sizeof (path), "%s/new", mailbox->path);

	dirp = opendir (path);
	if (!dirp) {
		mailbox->magic = 0;
		return;
	}

	while ((de = readdir (dirp)) != NULL) {
		if (*de->d_name == '.')
			continue;

		if (!(p = strstr (de->d_name, ":2,")) || !strchr (p + 3, 'T')) {
			mailbox->new = 1;
			mailbox->msg_count++;
			mailbox->msg_unread++;
		}
	}

	closedir (dirp);
	snprintf (path, sizeof (path), "%s/cur", mailbox->path);

	dirp = opendir (path);
	if (!dirp) {
		mailbox->magic = 0;
		return;
	}

	while ((de = readdir (dirp)) != NULL) {
		if (*de->d_name == '.')
			continue;

		if (!(p = strstr (de->d_name, ":2,")) || !strchr (p + 3, 'T')) {
			mailbox->msg_count++;
			if ((p = strstr (de->d_name, ":2,"))) {
				if (!strchr (p + 3, 'T')) {
					if (!strchr (p + 3, 'S'))
						mailbox->msg_unread++;
					if (strchr (p + 3, 'F'))
						mailbox->msg_flagged++;
				}
			}
		}
	}

	mailbox->sb_last_checked = time (NULL);
	closedir (dirp);

	/* make sure the updates are actually put on screen */
	sb_draw();
}
#endif

/* returns 1 if mailbox has new mail */ 
static int buffy_mbox_hasnew (BUFFY* mailbox, struct stat *sb)
{
  int rc = 0;
  int statcheck;

  if (option (OPTCHECKMBOXSIZE))
    statcheck = sb->st_size > mailbox->size;
  else
    statcheck = sb->st_mtime > sb->st_atime
      || (mailbox->newly_created && sb->st_ctime == sb->st_mtime && sb->st_ctime == sb->st_atime);
#ifdef USE_SIDEBAR
  if ((!option (OPTSIDEBAR) && statcheck) || (option (OPTSIDEBAR) && mailbox->msg_unread > 0))
#else
  if (statcheck)
#endif
  {
    if (!option(OPTMAILCHECKRECENT) || sb->st_mtime > mailbox->last_visited)
    {
      rc = 1;
      mailbox->new = 1;
    }
  }
  else if (option(OPTCHECKMBOXSIZE))
  {
    /* some other program has deleted mail from the folder */
    mailbox->size = (off_t) sb->st_size;
  }
  if (mailbox->newly_created &&
      (sb->st_ctime != sb->st_mtime || sb->st_ctime != sb->st_atime))
    mailbox->newly_created = 0;

  return rc;
}

#ifdef USE_SIDEBAR
/**
 * buffy_mbox_update - Update messages counts for an mbox mailbox
 * @mailbox: BUFFY representing an mbox mailbox
 * @sb:      stat(2) infomation about the mailbox file
 *
 * Open a mbox file and update our record of how many new, or flagged,
 * messages there are. If the mailbox hasn't changed since the last call,
 * the function does nothing.
 */
void
buffy_mbox_update (BUFFY *mailbox, struct stat *sb)
{
	CONTEXT *ctx = NULL;

	if (!option (OPTSIDEBAR))
		return;
	if ((mailbox->sb_last_checked > sb->st_mtime) && (mailbox->msg_count != 0))
		return; /* no check necessary */

	ctx = mx_open_mailbox (mailbox->path, M_READONLY | M_QUIET | M_NOSORT | M_PEEK, NULL);
	if (ctx) {
		mailbox->msg_count       = ctx->msgcount;
		mailbox->msg_unread      = ctx->unread;
		mailbox->msg_flagged     = ctx->flagged;
		mailbox->sb_last_checked = time (NULL);
		mx_close_mailbox (ctx, 0);
	}

	/* make sure the updates are actually put on screen */
	sb_draw();
}
#endif


static void buffy_check(BUFFY *tmp, struct stat *contex_sb)
{
    struct stat sb;

    sb.st_size=0;

    if (tmp->magic != M_IMAP)
    {
      tmp->new = 0;
#ifdef USE_POP
      if (mx_is_pop (tmp->path))
	tmp->magic = M_POP;
      else
#endif
#ifdef USE_NOTMUCH
      if (mx_is_notmuch (tmp->path))
	tmp->magic = M_NOTMUCH;
      else
#endif
      if (stat (tmp->path, &sb) != 0 || (S_ISREG(sb.st_mode) && sb.st_size == 0) ||
	  (!tmp->magic && (tmp->magic = mx_get_magic (tmp->path)) <= 0))
      {
	/* if the mailbox still doesn't exist, set the newly created flag to
	 * be ready for when it does. */
	tmp->newly_created = 1;
	tmp->magic = 0;
	tmp->size = 0;
	return;
      }
    }

    /* check to see if the folder is the currently selected folder
     * before polling */
    if (!Context || !Context->path ||
	(( tmp->magic == M_IMAP || tmp->magic == M_POP || tmp->magic == M_NOTMUCH)
	    ? mutt_strcmp (tmp->path, Context->path) :
	      (sb.st_dev != contex_sb->st_dev || sb.st_ino != contex_sb->st_ino)))
    {
      switch (tmp->magic)
      {
      case M_MBOX:
      case M_MMDF:
#ifdef USE_SIDEBAR
	if (option(OPTSIDEBAR))
	  buffy_mbox_update (tmp, &sb);
#endif
	if (buffy_mbox_hasnew (tmp, &sb) > 0)
	  BuffyCount++;
	break;

      case M_MAILDIR:
#ifdef USE_SIDEBAR
	if (option(OPTSIDEBAR))
	  buffy_maildir_update (tmp);
#endif
	if (buffy_maildir_hasnew (tmp) > 0)
	  BuffyCount++;
	break;

      case M_MH:
#ifdef USE_SIDEBAR
	if (option(OPTSIDEBAR))
	  mh_buffy_update (tmp);
#endif
	mh_buffy(tmp);
	if (tmp->new)
	  BuffyCount++;
	break;
#ifdef USE_NOTMUCH
      case M_NOTMUCH:
	tmp->msg_count = 0;
	tmp->msg_unread = 0;
	tmp->msg_flagged = 0;
	nm_nonctx_get_count(tmp->path, &tmp->msg_count, &tmp->msg_unread);
	if (tmp->msg_unread > 0) {
	  BuffyCount++;
	  tmp->new = 1;
	}
#ifdef USE_SIDEBAR
	sb_set_update_time();
#endif
	break;
#endif
      }
    }
    else if (option(OPTCHECKMBOXSIZE) && Context && Context->path)
      tmp->size = (off_t) sb.st_size;	/* update the size of current folder */

    if (!tmp->new)
      tmp->notified = 0;
    else if (!tmp->notified)
      BuffyNotify++;
}

int mutt_buffy_check (int force)
{
  BUFFY *tmp;
  struct stat contex_sb;
  time_t t;

  contex_sb.st_dev=0;
  contex_sb.st_ino=0;

#ifdef USE_IMAP
  /* update postponed count as well, on force */
  if (force)
    mutt_update_num_postponed ();
#endif

  /* fastest return if there are no mailboxes */
#ifdef USE_NOTMUCH
  if (!Incoming && !VirtIncoming)
    return 0;
#else
  if (!Incoming)
    return 0;
#endif
  t = time (NULL);
  if (!force && (t - BuffyTime < BuffyTimeout))
    return BuffyCount;

  BuffyTime = t;
  BuffyCount = 0;
  BuffyNotify = 0;

#ifdef USE_IMAP
  BuffyCount += imap_buffy_check (force);
#endif

  /* check device ID and serial number instead of comparing paths */
  if (!Context || Context->magic == M_IMAP || Context->magic == M_POP
      || stat (Context->path, &contex_sb) != 0)
  {
    contex_sb.st_dev=0;
    contex_sb.st_ino=0;
  }

#ifdef USE_SIDEBAR
  if (sb_should_refresh()) {
    for (tmp = Incoming; tmp; tmp = tmp->next)
      buffy_check(tmp, &contex_sb);
    sb_set_update_time();
  }
#else
  for (tmp = Incoming; tmp; tmp = tmp->next)
    buffy_check(tmp, &contex_sb);
#endif

#ifdef USE_NOTMUCH
  for (tmp = VirtIncoming; tmp; tmp = tmp->next)
    buffy_check(tmp, &contex_sb);
#endif

  BuffyDoneTime = BuffyTime;
  return (BuffyCount);
}

int mutt_buffy_list (void)
{
  BUFFY *tmp;
  char path[_POSIX_PATH_MAX];
  char buffylist[2*STRING];
  size_t pos = 0;
  int first = 1;

  int have_unnotified = BuffyNotify;
  
  buffylist[0] = 0;
  pos += strlen (strncat (buffylist, _("New mail in "), sizeof (buffylist) - 1 - pos)); /* __STRNCAT_CHECKED__ */
  for (tmp = Incoming; tmp; tmp = tmp->next)
  {
    /* Is there new mail in this mailbox? */
    if (!tmp->new || (have_unnotified && tmp->notified))
      continue;

    strfcpy (path, tmp->path, sizeof (path));
    mutt_pretty_mailbox (path, sizeof (path));
    
    if (!first && (COLS - 7 >= 0) && (pos + strlen (path) >= (size_t)COLS - 7))
      break;
    
    if (!first)
      pos += strlen (strncat(buffylist + pos, ", ", sizeof(buffylist)-1-pos)); /* __STRNCAT_CHECKED__ */

    /* Prepend an asterisk to mailboxes not already notified */
    if (!tmp->notified)
    {
      /* pos += strlen (strncat(buffylist + pos, "*", sizeof(buffylist)-1-pos));  __STRNCAT_CHECKED__ */
      tmp->notified = 1;
      BuffyNotify--;
    }
    pos += strlen (strncat(buffylist + pos, path, sizeof(buffylist)-1-pos)); /* __STRNCAT_CHECKED__ */
    first = 0;
  }
  if (!first && tmp)
  {
    strncat (buffylist + pos, ", ...", sizeof (buffylist) - 1 - pos); /* __STRNCAT_CHECKED__ */
  }
  if (!first)
  {
    mutt_message ("%s", buffylist);
    return (1);
  }
  /* there were no mailboxes needing to be notified, so clean up since 
   * BuffyNotify has somehow gotten out of sync
   */
  BuffyNotify = 0;
  return (0);
}

void mutt_buffy_setnotified (const char *path)
{
  BUFFY *buffy;

  buffy = buffy_get(path);
  if (!buffy)
    return;

  buffy->notified = 1;
  time(&buffy->last_visited);
}

int mutt_buffy_notify (void)
{
  if (mutt_buffy_check (0) && BuffyNotify)
  {
    return (mutt_buffy_list ());
  }
  return (0);
}

/* 
 * mutt_buffy() -- incoming folders completion routine
 *
 * given a folder name, this routine gives the next incoming folder with new
 * mail.
 */
void mutt_buffy (char *s, size_t slen)
{
  BUFFY *tmp = Incoming;
  int pass, found = 0;

  mutt_expand_path (s, slen);

  if (mutt_buffy_check (0)) 
  {
    for (pass = 0; pass < 2; pass++)
      for (tmp = Incoming; tmp; tmp = tmp->next) 
      {
	mutt_expand_path (tmp->path, sizeof (tmp->path));
	if ((found || pass) && tmp->new) 
	{
	  strfcpy (s, tmp->path, slen);
	  mutt_pretty_mailbox (s, slen);
	  return;
	}
	if (mutt_strcmp (s, tmp->path) == 0)
	  found = 1;
      }

    mutt_buffy_check (1); /* buffy was wrong - resync things */
  }

  /* no folders with new mail */
  *s = '\0';
}

#ifdef USE_NOTMUCH
void mutt_buffy_vfolder (char *s, size_t slen)
{
  BUFFY *tmp;
  int pass, found = 0;

  if (mutt_buffy_check (0))
  {
    for (pass = 0; pass < 2; pass++) {
      for (tmp = VirtIncoming; tmp; tmp = tmp->next)
      {
	if ((found || pass) && tmp->new)
	{
	  strfcpy (s, tmp->desc, slen);
	  return;
	}
	if (mutt_strcmp (s, tmp->path) == 0)
	  found = 1;
      }
    }

    mutt_buffy_check (1); /* buffy was wrong - resync things */
  }

  /* no folders with new mail */
  *s = '\0';
}
#endif

/* fetch buffy object for given path, if present */
static BUFFY* buffy_get (const char *path)
{
  BUFFY *cur;
  char *epath;

  if (!path)
    return NULL;

  epath = safe_strdup(path);
  mutt_expand_path(epath, mutt_strlen(epath));

  for (cur = Incoming; cur; cur = cur->next)
  {
    /* must be done late because e.g. IMAP delimiter may change */
    mutt_expand_path (cur->path, sizeof (cur->path));
    if (!mutt_strcmp(cur->path, path))
    {
      FREE (&epath);
      return cur;
    }
  }

  FREE (&epath);
  return NULL;
}
