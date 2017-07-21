/**
 * @file
 * IMAP helper functions
 *
 * @authors
 * Copyright (C) 1996-1998,2010,2012-2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1996-1999 Brandon Long <blong@fiction.net>
 * Copyright (C) 1999-2009,2012 Brendan Cully <brendan@kublai.com>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* general IMAP utility functions */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "imap_private.h"
#include "mutt.h"
#include "account.h"
#include "ascii.h"
#include "bcache.h"
#include "buffer.h"
#include "context.h"
#include "globals.h"
#include "header.h"
#include "imap/imap.h"
#include "lib.h"
#include "list.h"
#include "mailbox.h"
#include "message.h"
#include "mutt_socket.h"
#include "mx.h"
#include "options.h"
#include "protos.h"
#include "url.h"
#ifdef USE_HCACHE
#include "hcache/hcache.h"
#endif

/**
 * imap_expand_path - Canonicalise an IMAP path
 * @param path Buffer containing path
 * @param len  Buffer length
 * @retval 0 on success
 * @retval -1 on error
 *
 * IMAP implementation of mutt_expand_path. Rewrite an IMAP path in canonical
 * and absolute form.  The buffer is rewritten in place with the canonical IMAP
 * path.
 *
 * Function can fail if imap_parse_path() or url_ciss_tostring() fail,
 * of if the buffer isn't large enough.
 */
int imap_expand_path(char *path, size_t len)
{
  struct ImapMbox mx;
  struct ImapData *idata = NULL;
  struct CissUrl url;
  char fixedpath[LONG_STRING];
  int rc;

  if (imap_parse_path(path, &mx) < 0)
    return -1;

  idata = imap_conn_find(&mx.account, MUTT_IMAP_CONN_NONEW);
  mutt_account_tourl(&mx.account, &url);
  imap_fix_path(idata, mx.mbox, fixedpath, sizeof(fixedpath));
  url.path = fixedpath;

  rc = url_ciss_tostring(&url, path, len, U_DECODE_PASSWD);
  FREE(&mx.mbox);

  return rc;
}

/**
 * imap_get_parent - Get an IMAP folder's parent
 */
void imap_get_parent(char *output, const char *mbox, size_t olen, char delim)
{
  int n;

  /* Make a copy of the mailbox name, but only if the pointers are different */
  if (mbox != output)
    strfcpy(output, mbox, olen);

  n = mutt_strlen(output);

  /* Let's go backwards until the next delimiter
   *
   * If output[n] is a '/', the first n-- will allow us
   * to ignore it. If it isn't, then output looks like
   * "/aaaaa/bbbb". There is at least one "b", so we can't skip
   * the "/" after the 'a's.
   *
   * If output == '/', then n-- => n == 0, so the loop ends
   * immediately
   */
  for (n--; n >= 0 && output[n] != delim; n--)
    ;

  /* We stopped before the beginning. There is a trailing
   * slash.
   */
  if (n > 0)
  {
    /* Strip the trailing delimiter.  */
    output[n] = '\0';
  }
  else
  {
    output[0] = (n == 0) ? delim : '\0';
  }
}

/**
 * imap_get_parent_path - Get the path of the parent folder
 *
 * Provided an imap path, returns in output the parent directory if
 * existent. Else returns the same path.
 */
void imap_get_parent_path(char *output, const char *path, size_t olen)
{
  struct ImapMbox mx;
  struct ImapData *idata = NULL;
  char mbox[LONG_STRING] = "";

  if (imap_parse_path(path, &mx) < 0)
  {
    strfcpy(output, path, olen);
    return;
  }

  idata = imap_conn_find(&mx.account, MUTT_IMAP_CONN_NONEW);
  if (!idata)
  {
    strfcpy(output, path, olen);
    return;
  }

  /* Stores a fixed path in mbox */
  imap_fix_path(idata, mx.mbox, mbox, sizeof(mbox));

  /* Gets the parent mbox in mbox */
  imap_get_parent(mbox, mbox, sizeof(mbox), idata->delim);

  /* Returns a fully qualified IMAP url */
  imap_qualify_path(output, olen, &mx, mbox);
}

/**
 * imap_clean_path - Cleans an IMAP path using imap_fix_path
 *
 * Does it in place.
 */
void imap_clean_path(char *path, size_t plen)
{
  struct ImapMbox mx;
  struct ImapData *idata = NULL;
  char mbox[LONG_STRING] = "";

  if (imap_parse_path(path, &mx) < 0)
    return;

  idata = imap_conn_find(&mx.account, MUTT_IMAP_CONN_NONEW);
  if (!idata)
    return;

  /* Stores a fixed path in mbox */
  imap_fix_path(idata, mx.mbox, mbox, sizeof(mbox));

  /* Returns a fully qualified IMAP url */
  imap_qualify_path(path, plen, &mx, mbox);
}

#ifdef USE_HCACHE
static int imap_hcache_namer(const char *path, char *dest, size_t dlen)
{
  return snprintf(dest, dlen, "%s.hcache", path);
}

header_cache_t *imap_hcache_open(struct ImapData *idata, const char *path)
{
  struct ImapMbox mx;
  struct CissUrl url;
  char cachepath[LONG_STRING];
  char mbox[LONG_STRING];

  if (path)
    imap_cachepath(idata, path, mbox, sizeof(mbox));
  else
  {
    if (!idata->ctx || imap_parse_path(idata->ctx->path, &mx) < 0)
      return NULL;

    imap_cachepath(idata, mx.mbox, mbox, sizeof(mbox));
    FREE(&mx.mbox);
  }

  mutt_account_tourl(&idata->conn->account, &url);
  url.path = mbox;
  url_ciss_tostring(&url, cachepath, sizeof(cachepath), U_PATH);

  return mutt_hcache_open(HeaderCache, cachepath, imap_hcache_namer);
}

void imap_hcache_close(struct ImapData *idata)
{
  if (!idata->hcache)
    return;

  mutt_hcache_close(idata->hcache);
  idata->hcache = NULL;
}

struct Header *imap_hcache_get(struct ImapData *idata, unsigned int uid)
{
  char key[16];
  void *uv = NULL;
  struct Header *h = NULL;

  if (!idata->hcache)
    return NULL;

  sprintf(key, "/%u", uid);
  uv = mutt_hcache_fetch(idata->hcache, key, imap_hcache_keylen(key));
  if (uv)
  {
    if (*(unsigned int *) uv == idata->uid_validity)
      h = mutt_hcache_restore(uv);
    else
      mutt_debug(3, "hcache uidvalidity mismatch: %u\n", *(unsigned int *) uv);
    mutt_hcache_free(idata->hcache, &uv);
  }

  return h;
}

int imap_hcache_put(struct ImapData *idata, struct Header *h)
{
  char key[16];

  if (!idata->hcache)
    return -1;

  sprintf(key, "/%u", HEADER_DATA(h)->uid);
  return mutt_hcache_store(idata->hcache, key, imap_hcache_keylen(key), h, idata->uid_validity);
}

int imap_hcache_del(struct ImapData *idata, unsigned int uid)
{
  char key[16];

  if (!idata->hcache)
    return -1;

  sprintf(key, "/%u", uid);
  return mutt_hcache_delete(idata->hcache, key, imap_hcache_keylen(key));
}
#endif

/**
 * imap_parse_path - Parse an IMAP mailbox name into name,host,port
 *
 * Given an IMAP mailbox name, return host, port and a path IMAP servers will
 * recognize.  mx.mbox is malloc'd, caller must free it
 */
int imap_parse_path(const char *path, struct ImapMbox *mx)
{
  static unsigned short ImapPort = 0;
  static unsigned short ImapsPort = 0;
  struct servent *service = NULL;
  char tmp[128];
  struct CissUrl url;
  char *c = NULL;
  int n;

  if (!ImapPort)
  {
    service = getservbyname("imap", "tcp");
    if (service)
      ImapPort = ntohs(service->s_port);
    else
      ImapPort = IMAP_PORT;
    mutt_debug(3, "Using default IMAP port %d\n", ImapPort);
  }
  if (!ImapsPort)
  {
    service = getservbyname("imaps", "tcp");
    if (service)
      ImapsPort = ntohs(service->s_port);
    else
      ImapsPort = IMAP_SSL_PORT;
    mutt_debug(3, "Using default IMAPS port %d\n", ImapsPort);
  }

  /* Defaults */
  memset(&mx->account, 0, sizeof(mx->account));
  mx->account.port = ImapPort;
  mx->account.type = MUTT_ACCT_TYPE_IMAP;

  c = safe_strdup(path);
  url_parse_ciss(&url, c);
  if (url.scheme == U_IMAP || url.scheme == U_IMAPS)
  {
    if (mutt_account_fromurl(&mx->account, &url) < 0 || !*mx->account.host)
    {
      FREE(&c);
      return -1;
    }

    mx->mbox = safe_strdup(url.path);

    if (url.scheme == U_IMAPS)
      mx->account.flags |= MUTT_ACCT_SSL;

    FREE(&c);
  }
  /* old PINE-compatibility code */
  else
  {
    FREE(&c);
    if (sscanf(path, "{%127[^}]}", tmp) != 1)
      return -1;

    c = strchr(path, '}');
    if (!c)
      return -1;
    else
      /* walk past closing '}' */
      mx->mbox = safe_strdup(c + 1);

    if ((c = strrchr(tmp, '@')))
    {
      *c = '\0';
      strfcpy(mx->account.user, tmp, sizeof(mx->account.user));
      strfcpy(tmp, c + 1, sizeof(tmp));
      mx->account.flags |= MUTT_ACCT_USER;
    }

    if ((n = sscanf(tmp, "%127[^:/]%127s", mx->account.host, tmp)) < 1)
    {
      mutt_debug(1, "imap_parse_path: NULL host in %s\n", path);
      FREE(&mx->mbox);
      return -1;
    }

    if (n > 1)
    {
      if (sscanf(tmp, ":%hu%127s", &(mx->account.port), tmp) >= 1)
        mx->account.flags |= MUTT_ACCT_PORT;
      if (sscanf(tmp, "/%s", tmp) == 1)
      {
        if (ascii_strncmp(tmp, "ssl", 3) == 0)
          mx->account.flags |= MUTT_ACCT_SSL;
        else
        {
          mutt_debug(1, "imap_parse_path: Unknown connection type in %s\n", path);
          FREE(&mx->mbox);
          return -1;
        }
      }
    }
  }

  if ((mx->account.flags & MUTT_ACCT_SSL) && !(mx->account.flags & MUTT_ACCT_PORT))
    mx->account.port = ImapsPort;

  return 0;
}

/**
 * imap_mxcmp - Compare mailbox names, giving priority to INBOX
 *
 * silly helper for mailbox name string comparisons, because of INBOX
 */
int imap_mxcmp(const char *mx1, const char *mx2)
{
  char *b1 = NULL;
  char *b2 = NULL;
  int rc;

  if (!mx1 || !*mx1)
    mx1 = "INBOX";
  if (!mx2 || !*mx2)
    mx2 = "INBOX";
  if ((ascii_strcasecmp(mx1, "INBOX") == 0) &&
      (ascii_strcasecmp(mx2, "INBOX") == 0))
    return 0;

  b1 = safe_malloc(strlen(mx1) + 1);
  b2 = safe_malloc(strlen(mx2) + 1);

  imap_fix_path(NULL, mx1, b1, strlen(mx1) + 1);
  imap_fix_path(NULL, mx2, b2, strlen(mx2) + 1);

  rc = mutt_strcmp(b1, b2);
  FREE(&b1);
  FREE(&b2);

  return rc;
}

/**
 * imap_pretty_mailbox - Prettify an IMAP mailbox name
 *
 * Called by mutt_pretty_mailbox() to make IMAP paths look nice.
 */
void imap_pretty_mailbox(char *path)
{
  struct ImapMbox home, target;
  struct CissUrl url;
  char *delim = NULL;
  int tlen;
  int hlen = 0;
  bool home_match = false;

  if (imap_parse_path(path, &target) < 0)
    return;

  tlen = mutt_strlen(target.mbox);
  /* check whether we can do '=' substitution */
  if (mx_is_imap(Maildir) && !imap_parse_path(Maildir, &home))
  {
    hlen = mutt_strlen(home.mbox);
    if (tlen && mutt_account_match(&home.account, &target.account) &&
        (mutt_strncmp(home.mbox, target.mbox, hlen) == 0))
    {
      if (!hlen)
        home_match = true;
      else if (ImapDelimChars)
        for (delim = ImapDelimChars; *delim != '\0'; delim++)
          if (target.mbox[hlen] == *delim)
            home_match = true;
    }
    FREE(&home.mbox);
  }

  /* do the '=' substitution */
  if (home_match)
  {
    *path++ = '=';
    /* copy remaining path, skipping delimiter */
    if (!hlen)
      hlen = -1;
    memcpy(path, target.mbox + hlen + 1, tlen - hlen - 1);
    path[tlen - hlen - 1] = '\0';
  }
  else
  {
    mutt_account_tourl(&target.account, &url);
    url.path = target.mbox;
    /* FIXME: That hard-coded constant is bogus. But we need the actual
     *   size of the buffer from mutt_pretty_mailbox. And these pretty
     *   operations usually shrink the result. Still... */
    url_ciss_tostring(&url, path, 1024, 0);
  }

  FREE(&target.mbox);
}

/* -- library functions -- */

/**
 * imap_continue - display a message and ask the user if they want to go on
 */
int imap_continue(const char *msg, const char *resp)
{
  imap_error(msg, resp);
  return mutt_yesorno(_("Continue?"), 0);
}

/**
 * imap_error - show an error and abort
 */
void imap_error(const char *where, const char *msg)
{
  mutt_error("%s [%s]\n", where, msg);
  mutt_sleep(2);
}

/**
 * imap_new_idata - Allocate and initialise a new ImapData structure
 * @retval NULL on failure (no mem)
 */
struct ImapData *imap_new_idata(void)
{
  struct ImapData *idata = safe_calloc(1, sizeof(struct ImapData));

  if (!(idata->cmdbuf = mutt_buffer_new()))
    FREE(&idata);

  idata->cmdslots = ImapPipelineDepth + 2;
  if (!(idata->cmds = safe_calloc(idata->cmdslots, sizeof(*idata->cmds))))
  {
    mutt_buffer_free(&idata->cmdbuf);
    FREE(&idata);
  }

  return idata;
}

/**
 * imap_free_idata - Release and clear storage in an ImapData structure
 */
void imap_free_idata(struct ImapData **idata)
{
  if (!idata)
    return;

  FREE(&(*idata)->capstr);
  mutt_free_list(&(*idata)->flags);
  imap_mboxcache_free(*idata);
  mutt_buffer_free(&(*idata)->cmdbuf);
  FREE(&(*idata)->buf);
  mutt_bcache_close(&(*idata)->bcache);
  FREE(&(*idata)->cmds);
  FREE(idata);
}

/**
 * imap_fix_path - Fix up the imap path
 *
 * This is necessary because the rest of mutt assumes a hierarchy delimiter of
 * '/', which is not necessarily true in IMAP.  Additionally, the filesystem
 * converts multiple hierarchy delimiters into a single one, ie "///" is equal
 * to "/".  IMAP servers are not required to do this.
 * Moreover, IMAP servers may dislike the path ending with the delimiter.
 */
char *imap_fix_path(struct ImapData *idata, const char *mailbox, char *path, size_t plen)
{
  int i = 0;
  char delim = '\0';

  if (idata)
    delim = idata->delim;

  while (mailbox && *mailbox && i < plen - 1)
  {
    if ((ImapDelimChars && strchr(ImapDelimChars, *mailbox)) || (delim && *mailbox == delim))
    {
      /* use connection delimiter if known. Otherwise use user delimiter */
      if (!idata)
        delim = *mailbox;

      while (*mailbox && ((ImapDelimChars && strchr(ImapDelimChars, *mailbox)) ||
                          (delim && *mailbox == delim)))
        mailbox++;
      path[i] = delim;
    }
    else
    {
      path[i] = *mailbox;
      mailbox++;
    }
    i++;
  }
  if (i && path[--i] != delim)
    i++;
  path[i] = '\0';

  return path;
}

void imap_cachepath(struct ImapData *idata, const char *mailbox, char *dest, size_t dlen)
{
  char *s = NULL;
  const char *p = mailbox;

  for (s = dest; p && *p && dlen; dlen--)
  {
    if (*p == idata->delim)
    {
      *s = '/';
      /* simple way to avoid collisions with UIDs */
      if (*(p + 1) >= '0' && *(p + 1) <= '9')
      {
        if (--dlen)
          *++s = '_';
      }
    }
    else
      *s = *p;
    p++;
    s++;
  }
  *s = '\0';
}

/**
 * imap_get_literal_count - write number of bytes in an IMAP literal into bytes
 * @retval 0 on success
 * @retval -1 on failure
 */
int imap_get_literal_count(const char *buf, long *bytes)
{
  char *pc = NULL;
  char *pn = NULL;

  if (!buf || !(pc = strchr(buf, '{')))
    return -1;

  pc++;
  pn = pc;
  while (isdigit((unsigned char) *pc))
    pc++;
  *pc = 0;
  *bytes = atoi(pn);

  return 0;
}

/**
 * imap_get_qualifier - Get the qualifier from a tagged reponse
 *
 * In a tagged response, skip tag and status for the qualifier message. Used by
 * imap_copy_message for TRYCREATE
 */
char *imap_get_qualifier(char *buf)
{
  char *s = buf;

  /* skip tag */
  s = imap_next_word(s);
  /* skip OK/NO/BAD response */
  s = imap_next_word(s);

  return s;
}

/**
 * imap_next_word - return index into string where next IMAP word begins
 */
char *imap_next_word(char *s)
{
  int quoted = 0;

  while (*s)
  {
    if (*s == '\\')
    {
      s++;
      if (*s)
        s++;
      continue;
    }
    if (*s == '\"')
      quoted = quoted ? 0 : 1;
    if (!quoted && ISSPACE(*s))
      break;
    s++;
  }

  SKIPWS(s);
  return s;
}

/**
 * imap_parse_date - Parse date of the form: DD-MMM-YYYY HH:MM:SS +ZZzz
 */
time_t imap_parse_date(char *s)
{
  struct tm t;
  time_t tz;

  t.tm_mday = (s[0] == ' ' ? s[1] - '0' : (s[0] - '0') * 10 + (s[1] - '0'));
  s += 2;
  if (*s != '-')
    return 0;
  s++;
  t.tm_mon = mutt_check_month(s);
  s += 3;
  if (*s != '-')
    return 0;
  s++;
  t.tm_year = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 +
              (s[3] - '0') - 1900;
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
  tz = ((s[1] - '0') * 10 + (s[2] - '0')) * 3600 + ((s[3] - '0') * 10 + (s[4] - '0')) * 60;
  if (s[0] == '+')
    tz = -tz;

  return (mutt_mktime(&t, 0) + tz);
}

/**
 * imap_make_date - format date in IMAP style: DD-MMM-YYYY HH:MM:SS +ZZzz
 *
 * Caller should provide a buffer of IMAP_DATELEN bytes
 */
void imap_make_date(char *buf, time_t timestamp)
{
  struct tm *tm = localtime(&timestamp);
  time_t tz = mutt_local_tz(timestamp);

  tz /= 60;

  snprintf(buf, IMAP_DATELEN, "%02d-%s-%d %02d:%02d:%02d %+03d%02d",
           tm->tm_mday, Months[tm->tm_mon], tm->tm_year + 1900, tm->tm_hour,
           tm->tm_min, tm->tm_sec, (int) tz / 60, (int) abs((int) tz) % 60);
}

/**
 * imap_qualify_path - Make an absolute IMAP folder target
 *
 * given ImapMbox and relative path.
 */
void imap_qualify_path(char *dest, size_t len, struct ImapMbox *mx, char *path)
{
  struct CissUrl url;

  mutt_account_tourl(&mx->account, &url);
  url.path = path;

  url_ciss_tostring(&url, dest, len, 0);
}


/**
 * imap_quote_string - quote string according to IMAP rules
 *
 * Surround string with quotes, escape " and \ with backslash
 */
void imap_quote_string(char *dest, size_t dlen, const char *src)
{
  static const char quote[] = "\"\\";
  char *pt = NULL;
  const char *s = NULL;

  pt = dest;
  s = src;

  *pt++ = '"';
  /* save room for trailing quote-char */
  dlen -= 2;

  for (; *s && dlen; s++)
  {
    if (strchr(quote, *s))
    {
      dlen -= 2;
      if (!dlen)
        break;
      *pt++ = '\\';
      *pt++ = *s;
    }
    else
    {
      *pt++ = *s;
      dlen--;
    }
  }
  *pt++ = '"';
  *pt = 0;
}

/**
 * imap_unquote_string - equally stupid unquoting routine
 */
void imap_unquote_string(char *s)
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


/**
 * imap_munge_mbox_name - Quoting and UTF-7 conversion
 */
void imap_munge_mbox_name(struct ImapData *idata, char *dest, size_t dlen, const char *src)
{
  char *buf = NULL;

  buf = safe_strdup(src);
  imap_utf_encode(idata, &buf);

  imap_quote_string(dest, dlen, buf);

  FREE(&buf);
}

void imap_unmunge_mbox_name(struct ImapData *idata, char *s)
{
  char *buf = NULL;

  imap_unquote_string(s);

  buf = safe_strdup(s);
  if (buf)
  {
    imap_utf_decode(idata, &buf);
    strncpy(s, buf, strlen(s));
  }

  FREE(&buf);
}

/**
 * imap_wordcasecmp - find word a in word list b
 */
int imap_wordcasecmp(const char *a, const char *b)
{
  char tmp[SHORT_STRING];
  char *s = (char *) b;
  int i;

  tmp[SHORT_STRING - 1] = 0;
  for (i = 0; i < SHORT_STRING - 2; i++, s++)
  {
    if (!*s || ISSPACE(*s))
    {
      tmp[i] = 0;
      break;
    }
    tmp[i] = *s;
  }
  tmp[i + 1] = 0;

  return ascii_strcasecmp(a, tmp);
}

/*
 * Imap keepalive: poll the current folder to keep the
 * connection alive.
 */

static void alrm_handler(int sig)
{
  /* empty */
}

void imap_keepalive(void)
{
  struct Connection *conn = NULL;
  struct ImapData *idata = NULL;
  time_t now = time(NULL);

  for (conn = mutt_socket_head(); conn; conn = conn->next)
  {
    if (conn->account.type == MUTT_ACCT_TYPE_IMAP)
    {
      idata = conn->data;
      if (idata->state >= IMAP_AUTHENTICATED && now >= idata->lastread + ImapKeepalive)
      {
        imap_check(idata, 1);
      }
    }
  }
}

int imap_wait_keepalive(pid_t pid)
{
  struct sigaction oldalrm;
  struct sigaction act;
  sigset_t oldmask;
  int rc;

  short imap_passive = option(OPTIMAPPASSIVE);

  set_option(OPTIMAPPASSIVE);
  set_option(OPTKEEPQUIET);

  sigprocmask(SIG_SETMASK, NULL, &oldmask);

  sigemptyset(&act.sa_mask);
  act.sa_handler = alrm_handler;
#ifdef SA_INTERRUPT
  act.sa_flags = SA_INTERRUPT;
#else
  act.sa_flags = 0;
#endif

  sigaction(SIGALRM, &act, &oldalrm);

  alarm(ImapKeepalive);
  while (waitpid(pid, &rc, 0) < 0 && errno == EINTR)
  {
    alarm(0); /* cancel a possibly pending alarm */
    imap_keepalive();
    alarm(ImapKeepalive);
  }

  alarm(0); /* cancel a possibly pending alarm */

  sigaction(SIGALRM, &oldalrm, NULL);
  sigprocmask(SIG_SETMASK, &oldmask, NULL);

  unset_option(OPTKEEPQUIET);
  if (!imap_passive)
    unset_option(OPTIMAPPASSIVE);

  return rc;
}

/**
 * imap_allow_reopen - Allow/disallow re-opening a folder upon expunge
 */
void imap_allow_reopen(struct Context *ctx)
{
  struct ImapData *idata = NULL;
  if (!ctx || !ctx->data || ctx->magic != MUTT_IMAP)
    return;

  idata = ctx->data;
  if (idata->ctx == ctx)
    idata->reopen |= IMAP_REOPEN_ALLOW;
}

void imap_disallow_reopen(struct Context *ctx)
{
  struct ImapData *idata = NULL;
  if (!ctx || !ctx->data || ctx->magic != MUTT_IMAP)
    return;

  idata = ctx->data;
  if (idata->ctx == ctx)
    idata->reopen &= ~IMAP_REOPEN_ALLOW;
}

int imap_account_match(const struct Account *a1, const struct Account *a2)
{
  struct ImapData *a1_idata = imap_conn_find(a1, MUTT_IMAP_CONN_NONEW);
  struct ImapData *a2_idata = imap_conn_find(a2, MUTT_IMAP_CONN_NONEW);
  const struct Account *a1_canon = a1_idata == NULL ? a1 : &a1_idata->conn->account;
  const struct Account *a2_canon = a2_idata == NULL ? a2 : &a2_idata->conn->account;

  return mutt_account_match(a1_canon, a2_canon);
}
