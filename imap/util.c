/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * Copyright (C) 1996-9 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2001 Brendan Cully <brendan@kublai.com>
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

/* general IMAP utility functions */

#include "mutt.h"
#include "mx.h"	/* for M_IMAP */
#include "url.h"
#include "imap_private.h"
#include "mutt_ssl.h"

#include <stdlib.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include <errno.h>

/* -- public functions -- */

/* imap_parse_path: given an IMAP mailbox name, return host, port
 *   and a path IMAP servers will recognise.
 * mx.mbox is malloc'd, caller must free it */
int imap_parse_path (const char* path, IMAP_MBOX* mx)
{
  char tmp[128];
  ciss_url_t url;
  char *c;
  int n;

  /* Defaults */
  mx->account.flags = 0;
  mx->account.port = IMAP_PORT;
  mx->account.type = M_ACCT_TYPE_IMAP;

  c = safe_strdup (path);
  url_parse_ciss (&url, c);
  if (url.scheme == U_IMAP || url.scheme == U_IMAPS)
  {
    if (mutt_account_fromurl (&mx->account, &url) < 0)
    {
      FREE (&c);
      return -1;
    }

    mx->mbox = safe_strdup (url.path);

    if (url.scheme == U_IMAPS)
      mx->account.flags |= M_ACCT_SSL;

    FREE (&c);
  }
  /* old PINE-compatibility code */
  else
  {
    FREE (&c);
    if (sscanf (path, "{%128[^}]}", tmp) != 1) 
      return -1;

    c = strchr (path, '}');
    if (!c)
      return -1;
    else
      /* walk past closing '}' */
      mx->mbox = safe_strdup (c+1);
  
    if ((c = strrchr (tmp, '@')))
    {
      *c = '\0';
      strfcpy (mx->account.user, tmp, sizeof (mx->account.user));
      strfcpy (tmp, c+1, sizeof (tmp));
      mx->account.flags |= M_ACCT_USER;
    }
  
    if ((n = sscanf (tmp, "%128[^:/]%128s", mx->account.host, tmp)) < 1)
    {
      dprint (1, (debugfile, "imap_parse_path: NULL host in %s\n", path));
      FREE (&mx->mbox);
      return -1;
    }
  
    if (n > 1) {
      if (sscanf (tmp, ":%hd%128s", &(mx->account.port), tmp) >= 1)
	mx->account.flags |= M_ACCT_PORT;
      if (sscanf (tmp, "/%s", tmp) == 1)
      {
	if (!strncmp (tmp, "ssl", 3))
	  mx->account.flags |= M_ACCT_SSL;
	else
	{
	  dprint (1, (debugfile, "imap_parse_path: Unknown connection type in %s\n", path));
	  FREE (&mx->mbox);
	  return -1;
	}
      }
    }
  }
  
#ifdef USE_SSL
  if (option (OPTIMAPFORCESSL))
    mx->account.flags |= M_ACCT_SSL;
#endif

  if ((mx->account.flags & M_ACCT_SSL) && !(mx->account.flags & M_ACCT_PORT))
    mx->account.port = IMAP_SSL_PORT;

  return 0;
}

/* imap_pretty_mailbox: called by mutt_pretty_mailbox to make IMAP paths
 *   look nice. */
void imap_pretty_mailbox (char* path)
{
  IMAP_MBOX home, target;
  ciss_url_t url;
  char* delim;
  int tlen;
  int hlen = 0;
  char home_match = 0;

  if (imap_parse_path (path, &target) < 0)
    return;

  tlen = mutt_strlen (target.mbox);
  /* check whether we can do '=' substitution */
  if (! imap_parse_path (Maildir, &home))
  {
    hlen = mutt_strlen (home.mbox);
    if (tlen && mutt_account_match (&home.account, &target.account) &&
	!mutt_strncmp (home.mbox, target.mbox, hlen))
    {
      if (! hlen)
	home_match = 1;
      else
	for (delim = ImapDelimChars; *delim != '\0'; delim++)
	  if (target.mbox[hlen] == *delim)
	    home_match = 1;
    }
    FREE (&home.mbox);
  }

  /* do the '=' substitution */
  if (home_match) {
    *path++ = '=';
    /* copy remaining path, skipping delimiter */
    if (! hlen)
      hlen = -1;
    memcpy (path, target.mbox + hlen + 1, tlen - hlen - 1);
    path[tlen - hlen - 1] = '\0';
  }
  else
  {
    mutt_account_tourl (&target.account, &url);
    url.path = target.mbox;
    /* FIXME: That hard-coded constant is bogus. But we need the actual
     *   size of the buffer from mutt_pretty_mailbox. And these pretty
     *   operations usually shrink the result. Still... */
    url_ciss_tostring (&url, path, 1024);
  }

  FREE (&target.mbox);
}

/* -- library functions -- */

/* imap_continue: display a message and ask the user if she wants to
 *   go on. */
int imap_continue (const char* msg, const char* resp)
{
  imap_error (msg, resp);
  return mutt_yesorno (_("Continue?"), 0);
}

/* imap_error: show an error and abort */
void imap_error (const char *where, const char *msg)
{
  mutt_error (_("%s [%s]\n"), where, msg);
  sleep (2);
}

/* imap_new_idata: Allocate and initialise a new IMAP_DATA structure.
 *   Returns NULL on failure (no mem) */
IMAP_DATA* imap_new_idata (void) {
  IMAP_DATA* idata;

  idata = safe_calloc (1, sizeof (IMAP_DATA));
  if (!idata)
    return NULL;

  idata->conn = NULL;
  idata->state = IMAP_DISCONNECTED;
  idata->seqno = 0;

  idata->cmd.buf = NULL;
  idata->cmd.blen = 0;
  idata->cmd.state = IMAP_CMD_OK;

  return idata;
}

/* imap_free_idata: Release and clear storage in an IMAP_DATA structure. */
void imap_free_idata (IMAP_DATA** idata) {
  if (!idata)
    return;

  FREE (&((*idata)->cmd.buf));
  FREE (idata);
}

/*
 * Fix up the imap path.  This is necessary because the rest of mutt
 * assumes a hierarchy delimiter of '/', which is not necessarily true
 * in IMAP.  Additionally, the filesystem converts multiple hierarchy
 * delimiters into a single one, ie "///" is equal to "/".  IMAP servers
 * are not required to do this.
 */
char *imap_fix_path (IMAP_DATA *idata, char *mailbox, char *path, 
    size_t plen)
{
  int x = 0;

  if (!mailbox || !*mailbox)
  {
    strfcpy (path, "INBOX", plen);
    return path;
  }

  while (mailbox && *mailbox && (x < (plen - 1)))
  {
    if ((*mailbox == '/') || (*mailbox == idata->delim))
    {
      while ((*mailbox == '/') || (*mailbox == idata->delim)) mailbox++;
      path[x] = idata->delim;
    }
    else
    {
      path[x] = *mailbox;
      mailbox++;
    }
    x++;
  }
  path[x] = '\0';
  return path;
}

/* imap_get_literal_count: write number of bytes in an IMAP literal into
 *   bytes, return 0 on success, -1 on failure. */
int imap_get_literal_count(const char *buf, long *bytes)
{
  char *pc;
  char *pn;

  if (!(pc = strchr (buf, '{')))
    return (-1);
  pc++;
  pn = pc;
  while (isdigit (*pc))
    pc++;
  *pc = 0;
  *bytes = atoi(pn);
  return (0);
}

/* imap_get_qualifier: in a tagged response, skip tag and status for
 *   the qualifier message. Used by imap_copy_message for TRYCREATE */
char* imap_get_qualifier (char* buf)
{
  char *s = buf;

  /* skip tag */
  s = imap_next_word (s);
  /* skip OK/NO/BAD response */
  s = imap_next_word (s);

  return s;
}

/* imap_next_word: return index into string where next IMAP word begins */
char *imap_next_word (char *s)
{
  while (*s && !ISSPACE (*s))
    s++;
  SKIPWS (s);
  return s;
}

/* imap_parse_date: date is of the form: DD-MMM-YYYY HH:MM:SS +ZZzz */
time_t imap_parse_date (char *s)
{
  struct tm t;
  time_t tz;

  t.tm_mday = (s[0] == ' '? s[1] - '0' : (s[0] - '0') * 10 + (s[1] - '0'));  
  s += 2;
  if (*s != '-')
    return 0;
  s++;
  t.tm_mon = mutt_check_month (s);
  s += 3;
  if (*s != '-')
    return 0;
  s++;
  t.tm_year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0') - 1900;
  s += 4;
  if (*s != ' ')
    return 0;
  s++;

  /* time */
  t.tm_hour = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ':')
    return 0;
  s++;
  t.tm_min = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ':')
    return 0;
  s++;
  t.tm_sec = (s[0] - '0') * 10 + (s[1] - '0');
  s += 2;
  if (*s != ' ')
    return 0;
  s++;

  /* timezone */
  tz = ((s[1] - '0') * 10 + (s[2] - '0')) * 3600 +
    ((s[3] - '0') * 10 + (s[4] - '0')) * 60;
  if (s[0] == '+')
    tz = -tz;

  return (mutt_mktime (&t, 0) + tz);
}

/* imap_qualify_path: make an absolute IMAP folder target, given IMAP_MBOX
 *   and relative path. */
void imap_qualify_path (char *dest, size_t len, IMAP_MBOX *mx, char* path)
{
  ciss_url_t url;

  mutt_account_tourl (&mx->account, &url);
  url.path = path;

  url_ciss_tostring (&url, dest, len);
}


/* imap_quote_string: quote string according to IMAP rules:
 *   surround string with quotes, escape " and \ with \ */
void imap_quote_string (char *dest, size_t slen, const char *src)
{
  char quote[] = "\"\\", *pt;
  const char *s;
  size_t len = slen;

  pt = dest;
  s  = src;

  *pt++ = '"';
  /* save room for trailing quote-char */
  len -= 2;
  
  for (; *s && len; s++)
  {
    if (strchr (quote, *s))
    {
      len -= 2;
      if (!len)
	break;
      *pt++ = '\\';
      *pt++ = *s;
    }
    else
    {
      *pt++ = *s;
      len--;
    }
  }
  *pt++ = '"';
  *pt = 0;
}

/* imap_unquote_string: equally stupid unquoting routine */
void imap_unquote_string (char *s)
{
  char *d = s;

  if (*s == '\"')
    s++;
  else
    return;

  while (*s)
  {
    if (*s == '\"')
    {
      *d = '\0';
      return;
    }
    if (*s == '\\')
    {
      s++;
    }
    if (*s)
    {
      *d = *s;
      d++;
      s++;
    }
  }
  *d = '\0';
}

/*
 * Quoting and UTF-7 conversion
 */

void imap_munge_mbox_name (char *dest, size_t dlen, const char *src)
{
  char *buf;

  buf = safe_strdup (src);
  imap_utf7_encode (&buf);

  imap_quote_string (dest, dlen, buf);

  safe_free ((void **) &buf);
}

void imap_unmunge_mbox_name (char *s)
{
  char *buf;

  imap_unquote_string(s);

  buf = safe_strdup (s);
  if (buf)
  {
    imap_utf7_decode (&buf);
    strncpy (s, buf, strlen (s));
  }
  
  safe_free ((void **) &buf);
}

/* imap_wordcasecmp: find word a in word list b */
int imap_wordcasecmp(const char *a, const char *b)
{
  char tmp[SHORT_STRING];
  char *s = (char *)b;
  int i;

  tmp[SHORT_STRING-1] = 0;
  for(i=0;i < SHORT_STRING-2;i++,s++)
  {
    if (!*s || ISSPACE(*s))
    {
      tmp[i] = 0;
      break;
    }
    tmp[i] = *s;
  }
  tmp[i+1] = 0;

  return ascii_strcasecmp(a, tmp);
}

/* 
 * Imap keepalive: poll the current folder to keep the
 * connection alive.
 * 
 */

static RETSIGTYPE alrm_handler (int sig)
{
  /* empty */
}

void imap_keepalive (void)
{
  CONTEXT *ctx = Context;
  
  if (ctx == NULL || ctx->magic != M_IMAP || CTX_DATA->ctx != ctx)
    return;

  imap_check_mailbox (ctx, NULL);
}

int imap_wait_keepalive (pid_t pid)
{
  struct sigaction oldalrm;
  struct sigaction act;
  sigset_t oldmask;
  int rc;

  short imap_passive = option (OPTIMAPPASSIVE);
  
  set_option (OPTIMAPPASSIVE);
  set_option (OPTKEEPQUIET);

  sigprocmask (SIG_SETMASK, NULL, &oldmask);

  sigemptyset (&act.sa_mask);
  act.sa_handler = alrm_handler;
#ifdef SA_INTERRUPT
  act.sa_flags = SA_INTERRUPT;
#else
  act.sa_flags = 0;
#endif

  sigaction (SIGALRM, &act, &oldalrm);

  /* RFC 2060 specifies a minimum of 30 minutes before disconnect when in
   * the AUTHENTICATED state. We'll poll at half that. */
  alarm (900);
  while (waitpid (pid, &rc, 0) < 0 && errno == EINTR)
  {
    alarm (0); /* cancel a possibly pending alarm */
    imap_keepalive ();
    alarm (900);
  }

  alarm (0);	/* cancel a possibly pending alarm */
  
  sigaction (SIGALRM, &oldalrm, NULL);
  sigprocmask (SIG_SETMASK, &oldmask, NULL);

  unset_option (OPTKEEPQUIET);
  if (!imap_passive)
    unset_option (OPTIMAPPASSIVE);

  return rc;
}

/* Allow/disallow re-opening a folder upon expunge. */

void imap_allow_reopen (CONTEXT *ctx)
{
  if (ctx && ctx->magic == M_IMAP && CTX_DATA->ctx == ctx)
    CTX_DATA->reopen |= IMAP_REOPEN_ALLOW;
}

void imap_disallow_reopen (CONTEXT *ctx)
{
  if (ctx && ctx->magic == M_IMAP && CTX_DATA->ctx == ctx)
    CTX_DATA->reopen &= ~IMAP_REOPEN_ALLOW;
}
