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

/**
 * @page imap_util IMAP helper functions
 *
 * IMAP helper functions
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "imap_private.h"
#include "mutt/mutt.h"
#include "conn/conn.h"
#include "bcache.h"
#include "context.h"
#include "globals.h"
#include "header.h"
#include "imap/imap.h"
#include "mailbox.h"
#include "message.h"
#include "mutt_account.h"
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
 * @retval  0 Success
 * @retval -1 Error
 *
 * IMAP implementation of mutt_expand_path. Rewrite an IMAP path in canonical
 * and absolute form.  The buffer is rewritten in place with the canonical IMAP
 * path.
 *
 * Function can fail if imap_parse_path() or url_tostring() fail,
 * of if the buffer isn't large enough.
 */
int imap_expand_path(char *path, size_t len)
{
  struct ImapMbox mx;
  struct ImapData *idata = NULL;
  struct Url url;
  char fixedpath[LONG_STRING];
  int rc;

  if (imap_parse_path(path, &mx) < 0)
    return -1;

  idata = imap_conn_find(&mx.account, MUTT_IMAP_CONN_NONEW);
  mutt_account_tourl(&mx.account, &url);
  imap_fix_path(idata, mx.mbox, fixedpath, sizeof(fixedpath));
  url.path = fixedpath;

  rc = url_tostring(&url, path, len, U_DECODE_PASSWD);
  FREE(&mx.mbox);

  return rc;
}

/**
 * imap_get_parent - Get an IMAP folder's parent
 * @param output Buffer for the result
 * @param mbox   Mailbox whose parent is to be determined
 * @param olen   Length of the buffer
 * @param delim  Path delimiter
 */
void imap_get_parent(char *output, const char *mbox, size_t olen, char delim)
{
  int n;

  /* Make a copy of the mailbox name, but only if the pointers are different */
  if (mbox != output)
    mutt_str_strfcpy(output, mbox, olen);

  n = mutt_str_strlen(output);

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
 * @param output Buffer for the result
 * @param path   Mailbox whose parent is to be determined
 * @param olen   Length of the buffer
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
    mutt_str_strfcpy(output, path, olen);
    return;
  }

  idata = imap_conn_find(&mx.account, MUTT_IMAP_CONN_NONEW);
  if (!idata)
  {
    mutt_str_strfcpy(output, path, olen);
    return;
  }

  /* Stores a fixed path in mbox */
  imap_fix_path(idata, mx.mbox, mbox, sizeof(mbox));

  /* Gets the parent mbox in mbox */
  imap_get_parent(mbox, mbox, sizeof(mbox), idata->delim);

  /* Returns a fully qualified IMAP url */
  imap_qualify_path(output, olen, &mx, mbox);
  FREE(&mx.mbox);
}

/**
 * imap_clean_path - Cleans an IMAP path using imap_fix_path
 * @param path Path to be cleaned
 * @param plen Length of the buffer
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
/**
 * imap_hcache_namer - Generate a filename for the header cache
 * @param path Path for the header cache file
 * @param dest Buffer for result
 * @param dlen Length of buffer
 * @retval num Chars written to dest
 */
static int imap_hcache_namer(const char *path, char *dest, size_t dlen)
{
  return snprintf(dest, dlen, "%s.hcache", path);
}

/**
 * imap_hcache_open - Open a header cache
 * @param idata Server data
 * @param path  Path to the header cache
 * @retval ptr HeaderCache
 * @retval NULL Failure
 */
header_cache_t *imap_hcache_open(struct ImapData *idata, const char *path)
{
  struct ImapMbox mx;
  struct Url url;
  char cachepath[PATH_MAX];
  char mbox[PATH_MAX];

  if (path)
    imap_cachepath(idata, path, mbox, sizeof(mbox));
  else
  {
    if (!idata->ctx || imap_parse_path(idata->ctx->path, &mx) < 0)
      return NULL;

    imap_cachepath(idata, mx.mbox, mbox, sizeof(mbox));
    FREE(&mx.mbox);
  }

  if (strstr(mbox, "/../") || (strcmp(mbox, "..") == 0) || (strncmp(mbox, "../", 3) == 0))
    return NULL;
  size_t len = strlen(mbox);
  if ((len > 3) && (strcmp(mbox + len - 3, "/..") == 0))
    return NULL;

  mutt_account_tourl(&idata->conn->account, &url);
  url.path = mbox;
  url_tostring(&url, cachepath, sizeof(cachepath), U_PATH);

  return mutt_hcache_open(HeaderCache, cachepath, imap_hcache_namer);
}

/**
 * imap_hcache_close - Close the header cache
 * @param idata Server data
 */
void imap_hcache_close(struct ImapData *idata)
{
  if (!idata->hcache)
    return;

  mutt_hcache_close(idata->hcache);
  idata->hcache = NULL;
}

/**
 * imap_hcache_get - Get a header cache entry by its UID
 * @param idata Server data
 * @param uid   UID to find
 * @retval ptr Email Header
 * @retval NULL Failure
 */
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

/**
 * imap_hcache_put - Add an entry to the header cache
 * @param idata Server data
 * @param h     Email Header
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_hcache_put(struct ImapData *idata, struct Header *h)
{
  char key[16];

  if (!idata->hcache)
    return -1;

  sprintf(key, "/%u", HEADER_DATA(h)->uid);
  return mutt_hcache_store(idata->hcache, key, imap_hcache_keylen(key), h, idata->uid_validity);
}

/**
 * imap_hcache_del - Delete an item from the header cache
 * @param idata Server data
 * @param uid   UID of entry to delete
 * @retval  0 Success
 * @retval -1 Failure
 */
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
 * @param path Mailbox path to parse
 * @param mx   An IMAP mailbox
 * @retval  0 Success
 * @retval -1 Failure
 *
 * Given an IMAP mailbox name, return host, port and a path IMAP servers will
 * recognize.  mx.mbox is malloc'd, caller must free it
 */
int imap_parse_path(const char *path, struct ImapMbox *mx)
{
  static unsigned short ImapPort = 0;
  static unsigned short ImapsPort = 0;
  struct servent *service = NULL;
  struct Url url;
  char *c = NULL;

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

  c = mutt_str_strdup(path);
  url_parse(&url, c);
  if (url.scheme == U_IMAP || url.scheme == U_IMAPS)
  {
    if (mutt_account_fromurl(&mx->account, &url) < 0 || !*mx->account.host)
    {
      url_free(&url);
      FREE(&c);
      return -1;
    }

    mx->mbox = mutt_str_strdup(url.path);

    if (url.scheme == U_IMAPS)
      mx->account.flags |= MUTT_ACCT_SSL;

    url_free(&url);
    FREE(&c);
  }
  /* old PINE-compatibility code */
  else
  {
    url_free(&url);
    FREE(&c);
    char tmp[128];
    if (sscanf(path, "{%127[^}]}", tmp) != 1)
      return -1;

    c = strchr(path, '}');
    if (!c)
      return -1;
    else
    {
      /* walk past closing '}' */
      mx->mbox = mutt_str_strdup(c + 1);
    }

    c = strrchr(tmp, '@');
    if (c)
    {
      *c = '\0';
      mutt_str_strfcpy(mx->account.user, tmp, sizeof(mx->account.user));
      mutt_str_strfcpy(tmp, c + 1, sizeof(tmp));
      mx->account.flags |= MUTT_ACCT_USER;
    }

    const int n = sscanf(tmp, "%127[^:/]%127s", mx->account.host, tmp);
    if (n < 1)
    {
      mutt_debug(1, "NULL host in %s\n", path);
      FREE(&mx->mbox);
      return -1;
    }

    if (n > 1)
    {
      if (sscanf(tmp, ":%hu%127s", &(mx->account.port), tmp) >= 1)
        mx->account.flags |= MUTT_ACCT_PORT;
      if (sscanf(tmp, "/%s", tmp) == 1)
      {
        if (mutt_str_strncmp(tmp, "ssl", 3) == 0)
          mx->account.flags |= MUTT_ACCT_SSL;
        else
        {
          mutt_debug(1, "Unknown connection type in %s\n", path);
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
 * @param mx1 First mailbox name
 * @param mx2 Second mailbox name
 * @retval <0 First mailbox precedes Second mailbox
 * @retval  0 Mailboxes are the same
 * @retval >0 Second mailbox precedes First mailbox
 *
 * Like a normal sort function except that "INBOX" will be sorted to the
 * beginning of the list.
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
  if ((mutt_str_strcasecmp(mx1, "INBOX") == 0) &&
      (mutt_str_strcasecmp(mx2, "INBOX") == 0))
  {
    return 0;
  }

  b1 = mutt_mem_malloc(strlen(mx1) + 1);
  b2 = mutt_mem_malloc(strlen(mx2) + 1);

  imap_fix_path(NULL, mx1, b1, strlen(mx1) + 1);
  imap_fix_path(NULL, mx2, b2, strlen(mx2) + 1);

  rc = mutt_str_strcmp(b1, b2);
  FREE(&b1);
  FREE(&b2);

  return rc;
}

/**
 * imap_pretty_mailbox - Prettify an IMAP mailbox name
 * @param path Mailbox name to be tidied
 *
 * Called by mutt_pretty_mailbox() to make IMAP paths look nice.
 */
void imap_pretty_mailbox(char *path)
{
  struct ImapMbox home, target;
  struct Url url;
  char *delim = NULL;
  int tlen;
  int hlen = 0;
  bool home_match = false;

  if (imap_parse_path(path, &target) < 0)
    return;

  tlen = mutt_str_strlen(target.mbox);
  /* check whether we can do '=' substitution */
  if (mx_is_imap(Folder) && !imap_parse_path(Folder, &home))
  {
    hlen = mutt_str_strlen(home.mbox);
    if (tlen && mutt_account_match(&home.account, &target.account) &&
        (mutt_str_strncmp(home.mbox, target.mbox, hlen) == 0))
    {
      if (hlen == 0)
        home_match = true;
      else if (ImapDelimChars)
      {
        for (delim = ImapDelimChars; *delim != '\0'; delim++)
          if (target.mbox[hlen] == *delim)
            home_match = true;
      }
    }
    FREE(&home.mbox);
  }

  /* do the '=' substitution */
  if (home_match)
  {
    *path++ = '=';
    /* copy remaining path, skipping delimiter */
    if (hlen == 0)
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
    url_tostring(&url, path, 1024, 0);
  }

  FREE(&target.mbox);
}

/**
 * imap_continue - display a message and ask the user if they want to go on
 * @param msg  Location of the error
 * @param resp Message for user
 * @retval num Result: #MUTT_YES, #MUTT_NO, #MUTT_ABORT
 */
int imap_continue(const char *msg, const char *resp)
{
  imap_error(msg, resp);
  return mutt_yesorno(_("Continue?"), 0);
}

/**
 * imap_error - show an error and abort
 * @param where Location of the error
 * @param msg   Message for user
 */
void imap_error(const char *where, const char *msg)
{
  mutt_error("%s [%s]\n", where, msg);
}

/**
 * imap_new_idata - Allocate and initialise a new ImapData structure
 * @retval NULL Failure (no mem)
 * @retval ptr New ImapData
 */
struct ImapData *imap_new_idata(void)
{
  struct ImapData *idata = mutt_mem_calloc(1, sizeof(struct ImapData));

  idata->cmdbuf = mutt_buffer_new();
  idata->cmdslots = ImapPipelineDepth + 2;
  idata->cmds = mutt_mem_calloc(idata->cmdslots, sizeof(*idata->cmds));

  STAILQ_INIT(&idata->flags);
  STAILQ_INIT(&idata->mboxcache);

  return idata;
}

/**
 * imap_free_idata - Release and clear storage in an ImapData structure
 * @param idata Server data
 */
void imap_free_idata(struct ImapData **idata)
{
  if (!idata)
    return;

  FREE(&(*idata)->capstr);
  mutt_list_free(&(*idata)->flags);
  imap_mboxcache_free(*idata);
  mutt_buffer_free(&(*idata)->cmdbuf);
  FREE(&(*idata)->buf);
  mutt_bcache_close(&(*idata)->bcache);
  FREE(&(*idata)->cmds);
  FREE(idata);
}

/**
 * imap_fix_path - Fix up the imap path
 * @param idata   Server data
 * @param mailbox Mailbox path
 * @param path    Buffer for the result
 * @param plen    Length of buffer
 * @retval ptr Fixed-up path
 *
 * This is necessary because the rest of neomutt assumes a hierarchy delimiter of
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
      {
        mailbox++;
      }
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

/**
 * imap_cachepath - Generate a cache path for a mailbox
 * @param idata   Server data
 * @param mailbox Mailbox name
 * @param dest    Buffer to store cache path
 * @param dlen    Length of buffer
 */
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
 * @param[in]  buf   Number as a string
 * @param[out] bytes Resulting number
 * @retval  0 Success
 * @retval -1 Failure
 */
int imap_get_literal_count(const char *buf, unsigned int *bytes)
{
  char *pc = NULL;
  char *pn = NULL;

  if (!buf || !(pc = strchr(buf, '{')))
    return -1;

  pc++;
  pn = pc;
  while (isdigit((unsigned char) *pc))
    pc++;
  *pc = '\0';
  if (mutt_str_atoui(pn, bytes) < 0)
    return -1;

  return 0;
}

/**
 * imap_get_qualifier - Get the qualifier from a tagged response
 * @param buf Command string to process
 * @retval ptr Start of the qualifier
 *
 * In a tagged response, skip tag and status for the qualifier message.
 * Used by imap_copy_message for TRYCREATE
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
 * imap_next_word - Find where the next IMAP word begins
 * @param s Command string to process
 * @retval ptr Next IMAP word
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
 * imap_qualify_path - Make an absolute IMAP folder target
 * @param dest Buffer for the result
 * @param len  Length of buffer
 * @param mx   Imap mailbox
 * @param path Path relative to the mailbox
 *
 * given ImapMbox and relative path.
 */
void imap_qualify_path(char *dest, size_t len, struct ImapMbox *mx, char *path)
{
  struct Url url;

  mutt_account_tourl(&mx->account, &url);
  url.path = path;

  url_tostring(&url, dest, len, 0);
}

/**
 * imap_quote_string - quote string according to IMAP rules
 * @param dest Buffer for the result
 * @param dlen Length of the buffer
 * @param src  String to be quoted
 *
 * Surround string with quotes, escape " and \ with backslash
 */
void imap_quote_string(char *dest, size_t dlen, const char *src, bool quote_backtick)
{
  const char *quote = "`\"\\";
  if (!quote_backtick)
    quote++;

  char *pt = dest;
  const char *s = src;

  *pt++ = '"';
  /* save room for trailing quote-char */
  dlen -= 2;

  for (; *s && dlen; s++)
  {
    if (strchr(quote, *s))
    {
      if (dlen < 2)
        break;
      dlen -= 2;
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
  *pt = '\0';
}

/**
 * imap_unquote_string - equally stupid unquoting routine
 * @param s String to be unquoted
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
 * imap_munge_mbox_name - Quote awkward characters in a mailbox name
 * @param idata Server data
 * @param dest  Buffer to store safe mailbox name
 * @param dlen  Length of buffer
 * @param src   Mailbox name
 */
void imap_munge_mbox_name(struct ImapData *idata, char *dest, size_t dlen, const char *src)
{
  char *buf = mutt_str_strdup(src);
  imap_utf_encode(idata, &buf);

  imap_quote_string(dest, dlen, buf, false);

  FREE(&buf);
}

/**
 * imap_unmunge_mbox_name - Remove quoting from a mailbox name
 * @param idata Server data
 * @param s     Mailbox name
 *
 * The string will be altered in-place.
 */
void imap_unmunge_mbox_name(struct ImapData *idata, char *s)
{
  imap_unquote_string(s);

  char *buf = mutt_str_strdup(s);
  if (buf)
  {
    imap_utf_decode(idata, &buf);
    strncpy(s, buf, strlen(s));
  }

  FREE(&buf);
}

/**
 * imap_keepalive - poll the current folder to keep the connection alive
 */
void imap_keepalive(void)
{
  struct Connection *conn = NULL;
  struct ImapData *idata = NULL;
  time_t now = time(NULL);

  TAILQ_FOREACH(conn, mutt_socket_head(), entries)
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

/**
 * imap_wait_keepalive - Wait for a process to change state
 * @param pid Process ID to listen to
 * @retval num 'wstatus' from waitpid()
 */
int imap_wait_keepalive(pid_t pid)
{
  struct sigaction oldalrm;
  struct sigaction act;
  sigset_t oldmask;
  int rc;

  bool imap_passive = ImapPassive;

  ImapPassive = true;
  OptKeepQuiet = true;

  sigprocmask(SIG_SETMASK, NULL, &oldmask);

  sigemptyset(&act.sa_mask);
  act.sa_handler = mutt_sig_empty_handler;
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

  OptKeepQuiet = false;
  if (!imap_passive)
    ImapPassive = false;

  return rc;
}

/**
 * imap_allow_reopen - Allow re-opening a folder upon expunge
 * @param ctx Context
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

/**
 * imap_disallow_reopen - Disallow re-opening a folder upon expunge
 * @param ctx Context
 */
void imap_disallow_reopen(struct Context *ctx)
{
  struct ImapData *idata = NULL;
  if (!ctx || !ctx->data || ctx->magic != MUTT_IMAP)
    return;

  idata = ctx->data;
  if (idata->ctx == ctx)
    idata->reopen &= ~IMAP_REOPEN_ALLOW;
}

/**
 * imap_account_match - Compare two Accounts
 * @param a1 First Account
 * @param a2 Second Account
 * @retval true Accounts match
 */
int imap_account_match(const struct Account *a1, const struct Account *a2)
{
  struct ImapData *a1_idata = imap_conn_find(a1, MUTT_IMAP_CONN_NONEW);
  struct ImapData *a2_idata = imap_conn_find(a2, MUTT_IMAP_CONN_NONEW);
  const struct Account *a1_canon = a1_idata == NULL ? a1 : &a1_idata->conn->account;
  const struct Account *a2_canon = a2_idata == NULL ? a2 : &a2_idata->conn->account;

  return mutt_account_match(a1_canon, a2_canon);
}
