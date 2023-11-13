/**
 * @file
 * Some miscellaneous functions
 *
 * @authors
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2008 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2019 Pietro Cerutti <gahr@gahr.ch>
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
 * @page neo_muttlib Some miscellaneous functions
 *
 * Some miscellaneous functions
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/lib.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "alias/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "muttlib.h"
#include "browser/lib.h"
#include "editor/lib.h"
#include "history/lib.h"
#include "ncrypt/lib.h"
#include "parse/lib.h"
#include "question/lib.h"
#include "format_flags.h"
#include "globals.h"
#include "hook.h"
#include "mx.h"
#include "protos.h"
#ifdef USE_IMAP
#include "imap/lib.h"
#endif

/// Accepted XDG environment variables
static const char *XdgEnvVars[] = {
  [XDG_CONFIG_HOME] = "XDG_CONFIG_HOME",
  [XDG_CONFIG_DIRS] = "XDG_CONFIG_DIRS",
};

/// XDG default locations
static const char *XdgDefaults[] = {
  [XDG_CONFIG_HOME] = "~/.config",
  [XDG_CONFIG_DIRS] = "/etc/xdg",
};

/**
 * mutt_adv_mktemp - Create a temporary file
 * @param buf Buffer for the name
 *
 * Accept a "suggestion" for file name.  If that file exists, then
 * construct one with unique name but keep any extension.
 * This might fail, I guess.
 */
void mutt_adv_mktemp(struct Buffer *buf)
{
  if (!(buf->data && (buf->data[0] != '\0')))
  {
    buf_mktemp(buf);
  }
  else
  {
    struct Buffer *prefix = buf_pool_get();
    buf_strcpy(prefix, buf->data);
    mutt_file_sanitize_filename(prefix->data, true);
    const char *const c_tmp_dir = cs_subset_path(NeoMutt->sub, "tmp_dir");
    buf_printf(buf, "%s/%s", NONULL(c_tmp_dir), buf_string(prefix));

    struct stat st = { 0 };
    if ((lstat(buf_string(buf), &st) == -1) && (errno == ENOENT))
      goto out;

    char *suffix = strchr(prefix->data, '.');
    if (suffix)
    {
      *suffix = '\0';
      suffix++;
    }
    buf_mktemp_pfx_sfx(buf, prefix->data, suffix);

  out:
    buf_pool_release(&prefix);
  }
}

/**
 * mutt_expand_path - Create the canonical path
 * @param buf    Buffer with path
 * @param buflen Length of buffer
 * @retval ptr The expanded string
 *
 * @note The path is expanded in-place
 */
char *mutt_expand_path(char *buf, size_t buflen)
{
  return mutt_expand_path_regex(buf, buflen, false);
}

/**
 * buf_expand_path_regex - Create the canonical path (with regex char escaping)
 * @param buf     Buffer with path
 * @param regex If true, escape any regex characters
 *
 * @note The path is expanded in-place
 */
void buf_expand_path_regex(struct Buffer *buf, bool regex)
{
  const char *s = NULL;
  const char *tail = "";

  bool recurse = false;

  struct Buffer *p = buf_pool_get();
  struct Buffer *q = buf_pool_get();
  struct Buffer *tmp = buf_pool_get();

  do
  {
    recurse = false;
    s = buf_string(buf);

    switch (*s)
    {
      case '~':
      {
        if ((s[1] == '/') || (s[1] == '\0'))
        {
          buf_strcpy(p, HomeDir);
          tail = s + 1;
        }
        else
        {
          char *t = strchr(s + 1, '/');
          if (t)
            *t = '\0';

          struct passwd *pw = getpwnam(s + 1);
          if (pw)
          {
            buf_strcpy(p, pw->pw_dir);
            if (t)
            {
              *t = '/';
              tail = t;
            }
            else
            {
              tail = "";
            }
          }
          else
          {
            /* user not found! */
            if (t)
              *t = '/';
            buf_reset(p);
            tail = s;
          }
        }
        break;
      }

      case '=':
      case '+':
      {
        const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
        enum MailboxType mb_type = mx_path_probe(c_folder);

        /* if folder = {host} or imap[s]://host/: don't append slash */
        if ((mb_type == MUTT_IMAP) && ((c_folder[strlen(c_folder) - 1] == '}') ||
                                       (c_folder[strlen(c_folder) - 1] == '/')))
        {
          buf_strcpy(p, NONULL(c_folder));
        }
        else if (mb_type == MUTT_NOTMUCH)
        {
          buf_strcpy(p, NONULL(c_folder));
        }
        else if (c_folder && (c_folder[strlen(c_folder) - 1] == '/'))
        {
          buf_strcpy(p, NONULL(c_folder));
        }
        else
        {
          buf_printf(p, "%s/", NONULL(c_folder));
        }

        tail = s + 1;
        break;
      }

        /* elm compatibility, @ expands alias to user name */

      case '@':
      {
        struct AddressList *al = alias_lookup(s + 1);
        if (al && !TAILQ_EMPTY(al))
        {
          struct Email *e = email_new();
          e->env = mutt_env_new();
          mutt_addrlist_copy(&e->env->from, al, false);
          mutt_addrlist_copy(&e->env->to, al, false);

          buf_alloc(p, PATH_MAX);
          mutt_default_save(p, e);

          email_free(&e);
          /* Avoid infinite recursion if the resulting folder starts with '@' */
          if (*p->data != '@')
            recurse = true;

          tail = "";
        }
        break;
      }

      case '>':
      {
        const char *const c_mbox = cs_subset_string(NeoMutt->sub, "mbox");
        buf_strcpy(p, c_mbox);
        tail = s + 1;
        break;
      }

      case '<':
      {
        const char *const c_record = cs_subset_string(NeoMutt->sub, "record");
        buf_strcpy(p, c_record);
        tail = s + 1;
        break;
      }

      case '!':
      {
        if (s[1] == '!')
        {
          buf_strcpy(p, LastFolder);
          tail = s + 2;
        }
        else
        {
          const char *const c_spool_file = cs_subset_string(NeoMutt->sub, "spool_file");
          buf_strcpy(p, c_spool_file);
          tail = s + 1;
        }
        break;
      }

      case '-':
      {
        buf_strcpy(p, LastFolder);
        tail = s + 1;
        break;
      }

      case '^':
      {
        buf_strcpy(p, CurrentFolder);
        tail = s + 1;
        break;
      }

      default:
      {
        buf_reset(p);
        tail = s;
      }
    }

    if (regex && *(buf_string(p)) && !recurse)
    {
      mutt_file_sanitize_regex(q, buf_string(p));
      buf_printf(tmp, "%s%s", buf_string(q), tail);
    }
    else
    {
      buf_printf(tmp, "%s%s", buf_string(p), tail);
    }

    buf_copy(buf, tmp);
  } while (recurse);

  buf_pool_release(&p);
  buf_pool_release(&q);
  buf_pool_release(&tmp);

#ifdef USE_IMAP
  /* Rewrite IMAP path in canonical form - aids in string comparisons of
   * folders. May possibly fail, in which case buf should be the same. */
  if (imap_path_probe(buf_string(buf), NULL) == MUTT_IMAP)
    imap_expand_path(buf);
#endif
}

/**
 * buf_expand_path - Create the canonical path
 * @param buf     Buffer with path
 *
 * @note The path is expanded in-place
 */
void buf_expand_path(struct Buffer *buf)
{
  buf_expand_path_regex(buf, false);
}

/**
 * mutt_expand_path_regex - Create the canonical path (with regex char escaping)
 * @param buf     Buffer with path
 * @param buflen  Length of buffer
 * @param regex If true, escape any regex characters
 * @retval ptr The expanded string
 *
 * @note The path is expanded in-place
 */
char *mutt_expand_path_regex(char *buf, size_t buflen, bool regex)
{
  struct Buffer *tmp = buf_pool_get();

  buf_addstr(tmp, NONULL(buf));
  buf_expand_path_regex(tmp, regex);
  mutt_str_copy(buf, buf_string(tmp), buflen);

  buf_pool_release(&tmp);

  return buf;
}

/**
 * mutt_gecos_name - Lookup a user's real name in /etc/passwd
 * @param dest    Buffer for the result
 * @param destlen Length of buffer
 * @param pw      Passwd entry
 * @retval ptr Result buffer on success
 *
 * Extract the real name from /etc/passwd's GECOS field.  When set, honor the
 * regular expression in `$gecos_mask`, otherwise assume that the GECOS field is a
 * comma-separated list.
 * Replace "&" by a capitalized version of the user's login name.
 */
char *mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw)
{
  regmatch_t pat_match[1];
  size_t pwnl;
  char *p = NULL;

  if (!pw || !pw->pw_gecos)
    return NULL;

  memset(dest, 0, destlen);

  const struct Regex *c_gecos_mask = cs_subset_regex(NeoMutt->sub, "gecos_mask");
  if (mutt_regex_capture(c_gecos_mask, pw->pw_gecos, 1, pat_match))
  {
    mutt_str_copy(dest, pw->pw_gecos + pat_match[0].rm_so,
                  MIN(pat_match[0].rm_eo - pat_match[0].rm_so + 1, destlen));
  }
  else if ((p = strchr(pw->pw_gecos, ',')))
  {
    mutt_str_copy(dest, pw->pw_gecos, MIN(destlen, p - pw->pw_gecos + 1));
  }
  else
  {
    mutt_str_copy(dest, pw->pw_gecos, destlen);
  }

  pwnl = strlen(pw->pw_name);

  for (int idx = 0; dest[idx]; idx++)
  {
    if (dest[idx] == '&')
    {
      memmove(&dest[idx + pwnl], &dest[idx + 1],
              MAX((ssize_t) (destlen - idx - pwnl - 1), 0));
      memcpy(&dest[idx], pw->pw_name, MIN(destlen - idx - 1, pwnl));
      dest[idx] = toupper((unsigned char) dest[idx]);
    }
  }

  return dest;
}

/**
 * mutt_needs_mailcap - Does this type need a mailcap entry do display
 * @param b Attachment body to be displayed
 * @retval true  NeoMutt requires a mailcap entry to display
 * @retval false otherwise
 */
bool mutt_needs_mailcap(struct Body *b)
{
  switch (b->type)
  {
    case TYPE_TEXT:
      if (mutt_istr_equal("plain", b->subtype))
        return false;
      break;
    case TYPE_APPLICATION:
      if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_application_pgp(b))
        return false;
      if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(b))
        return false;
      break;

    case TYPE_MULTIPART:
    case TYPE_MESSAGE:
      return false;
  }

  return true;
}

/**
 * mutt_is_text_part - Is this part of an email in plain text?
 * @param b Part of an email
 * @retval true Part is in plain text
 */
bool mutt_is_text_part(struct Body *b)
{
  int t = b->type;
  char *s = b->subtype;

  if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_application_pgp(b))
    return false;

  if (t == TYPE_TEXT)
    return true;

  if (t == TYPE_MESSAGE)
  {
    if (mutt_istr_equal("delivery-status", s))
      return true;
  }

  if (((WithCrypto & APPLICATION_PGP) != 0) && (t == TYPE_APPLICATION))
  {
    if (mutt_istr_equal("pgp-keys", s))
      return true;
  }

  return false;
}

/**
 * mutt_pretty_mailbox - Shorten a mailbox path using '~' or '='
 * @param buf    Buffer containing string to shorten
 * @param buflen Length of buffer
 *
 * Collapse the pathname using ~ or = when possible
 */
void mutt_pretty_mailbox(char *buf, size_t buflen)
{
  if (!buf)
    return;

  char *p = buf, *q = buf;
  size_t len;
  enum UrlScheme scheme;
  char tmp[PATH_MAX] = { 0 };

  scheme = url_check_scheme(buf);

  const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
  if ((scheme == U_IMAP) || (scheme == U_IMAPS))
  {
    imap_pretty_mailbox(buf, buflen, c_folder);
    return;
  }

  if (scheme == U_NOTMUCH)
    return;

  /* if buf is an url, only collapse path component */
  if (scheme != U_UNKNOWN)
  {
    p = strchr(buf, ':') + 1;
    if (mutt_strn_equal(p, "//", 2))
      q = strchr(p + 2, '/');
    if (!q)
      q = strchr(p, '\0');
    p = q;
  }

  /* cleanup path */
  if (strstr(p, "//") || strstr(p, "/./"))
  {
    /* first attempt to collapse the pathname, this is more
     * lightweight than realpath() and doesn't resolve links */
    while (*p)
    {
      if ((p[0] == '/') && (p[1] == '/'))
      {
        *q++ = '/';
        p += 2;
      }
      else if ((p[0] == '/') && (p[1] == '.') && (p[2] == '/'))
      {
        *q++ = '/';
        p += 3;
      }
      else
      {
        *q++ = *p++;
      }
    }
    *q = '\0';
  }
  else if (strstr(p, "..") && ((scheme == U_UNKNOWN) || (scheme == U_FILE)) &&
           realpath(p, tmp))
  {
    mutt_str_copy(p, tmp, buflen - (p - buf));
  }

  if ((len = mutt_str_startswith(buf, c_folder)) && (buf[len] == '/'))
  {
    *buf++ = '=';
    memmove(buf, buf + len, mutt_str_len(buf + len) + 1);
  }
  else if ((len = mutt_str_startswith(buf, HomeDir)) && (buf[len] == '/'))
  {
    *buf++ = '~';
    memmove(buf, buf + len - 1, mutt_str_len(buf + len - 1) + 1);
  }
}

/**
 * buf_pretty_mailbox - Shorten a mailbox path using '~' or '='
 * @param buf Buffer containing Mailbox name
 */
void buf_pretty_mailbox(struct Buffer *buf)
{
  if (!buf || !buf->data)
    return;
  /* This reduces the size of the Buffer, so we can pass it through.
   * We adjust the size just to make sure buf->data is not NULL though */
  buf_alloc(buf, PATH_MAX);
  mutt_pretty_mailbox(buf->data, buf->dsize);
  buf_fix_dptr(buf);
}

/**
 * mutt_check_overwrite - Ask the user if overwriting is necessary
 * @param[in]  attname   Attachment name
 * @param[in]  path      Path to save the file
 * @param[out] fname     Buffer for filename
 * @param[out] opt       Save option, see #SaveAttach
 * @param[out] directory Directory to save under (OPTIONAL)
 * @retval  0 Success
 * @retval -1 Abort
 * @retval  1 Error
 */
int mutt_check_overwrite(const char *attname, const char *path, struct Buffer *fname,
                         enum SaveAttach *opt, char **directory)
{
  struct stat st = { 0 };

  buf_strcpy(fname, path);
  if (access(buf_string(fname), F_OK) != 0)
    return 0;
  if (stat(buf_string(fname), &st) != 0)
    return -1;
  if (S_ISDIR(st.st_mode))
  {
    enum QuadOption ans = MUTT_NO;
    if (directory)
    {
      switch (mw_multi_choice
              /* L10N: Means "The path you specified as the destination file is a directory."
                 See the msgid "Save to file: " (alias.c, recvattach.c)
                 These three letters correspond to the choices in the string.  */
              (_("File is a directory, save under it: (y)es, (n)o, (a)ll?"), _("yna")))
      {
        case 3: /* all */
          mutt_str_replace(directory, buf_string(fname));
          break;
        case 1: /* yes */
          FREE(directory);
          break;
        case -1: /* abort */
          FREE(directory);
          return -1;
        case 2: /* no */
          FREE(directory);
          return 1;
      }
    }
    /* L10N: Means "The path you specified as the destination file is a directory."
       See the msgid "Save to file: " (alias.c, recvattach.c) */
    else if ((ans = query_yesorno(_("File is a directory, save under it?"), MUTT_YES)) != MUTT_YES)
      return (ans == MUTT_NO) ? 1 : -1;

    struct Buffer *tmp = buf_pool_get();
    buf_strcpy(tmp, mutt_path_basename(NONULL(attname)));
    struct FileCompletionData cdata = { false, NULL, NULL, NULL };
    if ((mw_get_field(_("File under directory: "), tmp, MUTT_COMP_CLEAR,
                      HC_FILE, &CompleteFileOps, &cdata) != 0) ||
        buf_is_empty(tmp))
    {
      buf_pool_release(&tmp);
      return (-1);
    }
    buf_concat_path(fname, path, buf_string(tmp));
    buf_pool_release(&tmp);
  }

  if ((*opt == MUTT_SAVE_NO_FLAGS) && (access(buf_string(fname), F_OK) == 0))
  {
    char buf[4096] = { 0 };
    snprintf(buf, sizeof(buf), "%s - %s", buf_string(fname),
             // L10N: Options for: File %s exists, (o)verwrite, (a)ppend, or (c)ancel?
             _("File exists, (o)verwrite, (a)ppend, or (c)ancel?"));
    switch (mw_multi_choice(buf, _("oac")))
    {
      case -1: /* abort */
        return -1;
      case 3: /* cancel */
        return 1;

      case 2: /* append */
        *opt = MUTT_SAVE_APPEND;
        break;
      case 1: /* overwrite */
        *opt = MUTT_SAVE_OVERWRITE;
        break;
    }
  }
  return 0;
}

/**
 * mutt_save_path - Turn an email address into a filename (for saving)
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param addr   Email address to use
 *
 * If the user hasn't set `$save_address` the name will be truncated to the '@'
 * character.
 */
void mutt_save_path(char *buf, size_t buflen, const struct Address *addr)
{
  if (addr && addr->mailbox)
  {
    mutt_str_copy(buf, buf_string(addr->mailbox), buflen);
    const bool c_save_address = cs_subset_bool(NeoMutt->sub, "save_address");
    if (!c_save_address)
    {
      char *p = strpbrk(buf, "%@");
      if (p)
        *p = '\0';
    }
    mutt_str_lower(buf);
  }
  else
  {
    *buf = '\0';
  }
}

/**
 * buf_save_path - Make a safe filename from an email address
 * @param dest Buffer for the result
 * @param a    Address to use
 */
void buf_save_path(struct Buffer *dest, const struct Address *a)
{
  if (a && a->mailbox)
  {
    buf_copy(dest, a->mailbox);
    const bool c_save_address = cs_subset_bool(NeoMutt->sub, "save_address");
    if (!c_save_address)
    {
      char *p = strpbrk(dest->data, "%@");
      if (p)
      {
        *p = '\0';
        buf_fix_dptr(dest);
      }
    }
    mutt_str_lower(dest->data);
  }
  else
  {
    buf_reset(dest);
  }
}

/**
 * mutt_safe_path - Make a safe filename from an email address
 * @param dest Buffer for the result
 * @param a    Address to use
 *
 * The filename will be stripped of '/', space, etc to make it safe.
 */
void mutt_safe_path(struct Buffer *dest, const struct Address *a)
{
  buf_save_path(dest, a);
  for (char *p = dest->data; *p; p++)
    if ((*p == '/') || isspace(*p) || !IsPrint((unsigned char) *p))
      *p = '_';
}

/**
 * mutt_expando_format - Expand expandos (%x) in a string - @ingroup expando_api
 * @param[out] buf      Buffer in which to save string
 * @param[in]  buflen   Buffer length
 * @param[in]  col      Starting column
 * @param[in]  cols     Number of screen columns
 * @param[in]  src      Printf-like format string
 * @param[in]  callback Callback - Implements ::format_t
 * @param[in]  data     Callback data
 * @param[in]  flags    Callback flags
 */
void mutt_expando_format(char *buf, size_t buflen, size_t col, int cols, const char *src,
                         format_t callback, intptr_t data, MuttFormatFlags flags)
{
  char prefix[128], tmp[1024];
  char *cp = NULL, *wptr = buf;
  char ch;
  char if_str[128], else_str[128];
  size_t wlen, count, len, wid;
  FILE *fp_filter = NULL;
  char *recycler = NULL;

  char src2[1024];
  mutt_str_copy(src2, src, mutt_str_len(src) + 1);
  src = src2;

  const bool c_arrow_cursor = cs_subset_bool(NeoMutt->sub, "arrow_cursor");
  const char *const c_arrow_string = cs_subset_string(NeoMutt->sub, "arrow_string");
  const int arrow_width = mutt_strwidth(c_arrow_string);

  prefix[0] = '\0';
  buflen--; /* save room for the terminal \0 */
  wlen = ((flags & MUTT_FORMAT_ARROWCURSOR) && c_arrow_cursor) ? arrow_width + 1 : 0;
  col += wlen;

  if ((flags & MUTT_FORMAT_NOFILTER) == 0)
  {
    int off = -1;

    /* Do not consider filters if no pipe at end */
    int n = mutt_str_len(src);
    if ((n > 1) && (src[n - 1] == '|'))
    {
      /* Scan backwards for backslashes */
      off = n;
      while ((off > 0) && (src[off - 2] == '\\'))
        off--;
    }

    /* If number of backslashes is even, the pipe is real. */
    /* n-off is the number of backslashes. */
    if ((off > 0) && (((n - off) % 2) == 0))
    {
      char srccopy[1024] = { 0 };
      int i = 0;

      mutt_debug(LL_DEBUG3, "fmtpipe = %s\n", src);

      strncpy(srccopy, src, n);
      srccopy[n - 1] = '\0';

      /* prepare Buffers */
      struct Buffer srcbuf = buf_make(0);
      buf_addstr(&srcbuf, srccopy);
      /* note: we are resetting dptr and *reading* from the buffer, so we don't
       * want to use buf_reset(). */
      buf_seek(&srcbuf, 0);
      struct Buffer word = buf_make(0);
      struct Buffer cmd = buf_make(0);

      /* Iterate expansions across successive arguments */
      do
      {
        /* Extract the command name and copy to command line */
        mutt_debug(LL_DEBUG3, "fmtpipe +++: %s\n", srcbuf.dptr);
        if (word.data)
          *word.data = '\0';
        parse_extract_token(&word, &srcbuf, TOKEN_NO_FLAGS);
        mutt_debug(LL_DEBUG3, "fmtpipe %2d: %s\n", i++, word.data);
        buf_addch(&cmd, '\'');
        mutt_expando_format(tmp, sizeof(tmp), 0, cols, word.data, callback,
                            data, flags | MUTT_FORMAT_NOFILTER);
        for (char *p = tmp; p && (*p != '\0'); p++)
        {
          if (*p == '\'')
          {
            /* shell quoting doesn't permit escaping a single quote within
             * single-quoted material.  double-quoting instead will lead
             * shell variable expansions, so break out of the single-quoted
             * span, insert a double-quoted single quote, and resume. */
            buf_addstr(&cmd, "'\"'\"'");
          }
          else
          {
            buf_addch(&cmd, *p);
          }
        }
        buf_addch(&cmd, '\'');
        buf_addch(&cmd, ' ');
      } while (MoreArgs(&srcbuf));

      mutt_debug(LL_DEBUG3, "fmtpipe > %s\n", cmd.data);

      col -= wlen; /* reset to passed in value */
      wptr = buf;  /* reset write ptr */
      pid_t pid = filter_create(cmd.data, NULL, &fp_filter, NULL, EnvList);
      if (pid != -1)
      {
        int rc;

        n = fread(buf, 1, buflen /* already decremented */, fp_filter);
        mutt_file_fclose(&fp_filter);
        rc = filter_wait(pid);
        if (rc != 0)
          mutt_debug(LL_DEBUG1, "format pipe cmd exited code %d\n", rc);
        if (n > 0)
        {
          buf[n] = '\0';
          while ((n > 0) && ((buf[n - 1] == '\n') || (buf[n - 1] == '\r')))
            buf[--n] = '\0';
          mutt_debug(LL_DEBUG5, "fmtpipe < %s\n", buf);

          /* If the result ends with '%', this indicates that the filter
           * generated %-tokens that neomutt can expand.  Eliminate the '%'
           * marker and recycle the string through mutt_expando_format().
           * To literally end with "%", use "%%". */
          if ((n > 0) && (buf[n - 1] == '%'))
          {
            n--;
            buf[n] = '\0'; /* remove '%' */
            if ((n > 0) && (buf[n - 1] != '%'))
            {
              recycler = mutt_str_dup(buf);
              if (recycler)
              {
                /* buflen is decremented at the start of this function
                 * to save space for the terminal nul char.  We can add
                 * it back for the recursive call since the expansion of
                 * format pipes does not try to append a nul itself.  */
                mutt_expando_format(buf, buflen + 1, col, cols, recycler,
                                    callback, data, flags);
                FREE(&recycler);
              }
            }
          }
        }
        else
        {
          /* read error */
          mutt_debug(LL_DEBUG1, "error reading from fmtpipe: %s (errno=%d)\n",
                     strerror(errno), errno);
          *wptr = '\0';
        }
      }
      else
      {
        /* Filter failed; erase write buffer */
        *wptr = '\0';
      }

      buf_dealloc(&cmd);
      buf_dealloc(&srcbuf);
      buf_dealloc(&word);
      return;
    }
  }

  while (*src && (wlen < buflen))
  {
    if (*src == '%')
    {
      if (*++src == '%')
      {
        *wptr++ = '%';
        wlen++;
        col++;
        src++;
        continue;
      }

      if (*src == '?')
      {
        /* change original %? to new %< notation */
        /* %?x?y&z? to %<x?y&z> where y and z are nestable */
        char *p = (char *) src;
        *p = '<';
        /* skip over "x" */
        for (; *p && (*p != '?'); p++)
          ; // do nothing

        /* nothing */
        if (*p == '?')
          p++;
        /* fix up the "y&z" section */
        for (; *p && (*p != '?'); p++)
        {
          /* escape '<' and '>' to work inside nested-if */
          if ((*p == '<') || (*p == '>'))
          {
            memmove(p + 2, p, mutt_str_len(p) + 1);
            *p++ = '\\';
            *p++ = '\\';
          }
        }
        if (*p == '?')
          *p = '>';
      }

      if (*src == '<')
      {
        flags |= MUTT_FORMAT_OPTIONAL;
        ch = *(++src); /* save the character to switch on */
        src++;
        cp = prefix;
        count = 0;
        while ((count < (sizeof(prefix) - 1)) && (*src != '\0') && (*src != '?'))
        {
          *cp++ = *src++;
          count++;
        }
        *cp = '\0';
      }
      else
      {
        flags &= ~MUTT_FORMAT_OPTIONAL;

        /* eat the format string */
        cp = prefix;
        count = 0;
        while ((count < (sizeof(prefix) - 1)) && strchr("0123456789.-=", *src))
        {
          *cp++ = *src++;
          count++;
        }
        *cp = '\0';

        if (*src == '\0')
          break; /* bad format */

        ch = *src++; /* save the character to switch on */
      }

      if (flags & MUTT_FORMAT_OPTIONAL)
      {
        int lrbalance;

        if (*src != '?')
          break; /* bad format */
        src++;

        /* eat the 'if' part of the string */
        cp = if_str;
        count = 0;
        lrbalance = 1;
        while ((lrbalance > 0) && (count < sizeof(if_str)) && *src)
        {
          if ((src[0] == '%') && (src[1] == '>'))
          {
            /* This is a padding expando; copy two chars and carry on */
            *cp++ = *src++;
            *cp++ = *src++;
            count += 2;
            continue;
          }

          if (*src == '\\')
          {
            src++;
            *cp++ = *src++;
          }
          else if ((src[0] == '%') && (src[1] == '<'))
          {
            lrbalance++;
          }
          else if (src[0] == '>')
          {
            lrbalance--;
          }
          if (lrbalance == 0)
            break;
          if ((lrbalance == 1) && (src[0] == '&'))
            break;
          *cp++ = *src++;
          count++;
        }
        *cp = '\0';

        /* eat the 'else' part of the string (optional) */
        if (*src == '&')
          src++; /* skip the & */
        cp = else_str;
        count = 0;
        while ((lrbalance > 0) && (count < sizeof(else_str)) && (*src != '\0'))
        {
          if ((src[0] == '%') && (src[1] == '>'))
          {
            /* This is a padding expando; copy two chars and carry on */
            *cp++ = *src++;
            *cp++ = *src++;
            count += 2;
            continue;
          }

          if (*src == '\\')
          {
            src++;
            *cp++ = *src++;
          }
          else if ((src[0] == '%') && (src[1] == '<'))
          {
            lrbalance++;
          }
          else if (src[0] == '>')
          {
            lrbalance--;
          }
          if (lrbalance == 0)
            break;
          if ((lrbalance == 1) && (src[0] == '&'))
            break;
          *cp++ = *src++;
          count++;
        }
        *cp = '\0';

        if ((*src == '\0'))
          break; /* bad format */

        src++; /* move past the trailing '>' (formerly '?') */
      }

      /* handle generic cases first */
      if ((ch == '>') || (ch == '*'))
      {
        /* %>X: right justify to EOL, left takes precedence
         * %*X: right justify to EOL, right takes precedence */
        int soft = ch == '*';
        int pl, pw;
        pl = mutt_mb_charlen(src, &pw);
        if (pl <= 0)
        {
          pl = 1;
          pw = 1;
        }

        /* see if there's room to add content, else ignore */
        if (((col < cols) && (wlen < buflen)) || soft)
        {
          int pad;

          /* get contents after padding */
          mutt_expando_format(tmp, sizeof(tmp), 0, cols, src + pl, callback, data, flags);
          len = mutt_str_len(tmp);
          wid = mutt_strwidth(tmp);

          pad = (cols - col - wid) / pw;
          if (pad >= 0)
          {
            /* try to consume as many columns as we can, if we don't have
             * memory for that, use as much memory as possible */
            if (wlen + (pad * pl) + len > buflen)
            {
              pad = (buflen > (wlen + len)) ? ((buflen - wlen - len) / pl) : 0;
            }
            else
            {
              /* Add pre-spacing to make multi-column pad characters and
               * the contents after padding line up */
              while (((col + (pad * pw) + wid) < cols) && ((wlen + (pad * pl) + len) < buflen))
              {
                *wptr++ = ' ';
                wlen++;
                col++;
              }
            }
            while (pad-- > 0)
            {
              memcpy(wptr, src, pl);
              wptr += pl;
              wlen += pl;
              col += pw;
            }
          }
          else if (soft)
          {
            int offset = ((flags & MUTT_FORMAT_ARROWCURSOR) && c_arrow_cursor) ?
                             arrow_width + 1 :
                             0;
            int avail_cols = (cols > offset) ? (cols - offset) : 0;
            /* \0-terminate buf for length computation in mutt_wstr_trunc() */
            *wptr = '\0';
            /* make sure right part is at most as wide as display */
            len = mutt_wstr_trunc(tmp, buflen, avail_cols, &wid);
            /* truncate left so that right part fits completely in */
            wlen = mutt_wstr_trunc(buf, buflen - len, avail_cols - wid, &col);
            wptr = buf + wlen;
            /* Multi-column characters may be truncated in the middle.
             * Add spacing so the right hand side lines up. */
            while (((col + wid) < avail_cols) && ((wlen + len) < buflen))
            {
              *wptr++ = ' ';
              wlen++;
              col++;
            }
          }
          if ((len + wlen) > buflen)
            len = mutt_wstr_trunc(tmp, buflen - wlen, cols - col, NULL);
          memcpy(wptr, tmp, len);
          wptr += len;
        }
        break; /* skip rest of input */
      }
      else if (ch == '|')
      {
        /* pad to EOL */
        int pl, pw;
        pl = mutt_mb_charlen(src, &pw);
        if (pl <= 0)
        {
          pl = 1;
          pw = 1;
        }

        /* see if there's room to add content, else ignore */
        if ((col < cols) && (wlen < buflen))
        {
          int c = (cols - col) / pw;
          if ((c > 0) && ((wlen + (c * pl)) > buflen))
            c = ((signed) (buflen - wlen)) / pl;
          while (c > 0)
          {
            memcpy(wptr, src, pl);
            wptr += pl;
            wlen += pl;
            col += pw;
            c--;
          }
        }
        break; /* skip rest of input */
      }
      else
      {
        bool to_lower = false;
        bool no_dots = false;

        while ((ch == '_') || (ch == ':'))
        {
          if (ch == '_')
            to_lower = true;
          else if (ch == ':')
            no_dots = true;

          ch = *src++;
        }

        /* use callback function to handle this case */
        *tmp = '\0';
        src = callback(tmp, sizeof(tmp), col, cols, ch, src, prefix, if_str,
                       else_str, data, flags);

        if (to_lower)
          mutt_str_lower(tmp);
        if (no_dots)
        {
          char *p = tmp;
          for (; *p; p++)
            if (*p == '.')
              *p = '_';
        }

        len = mutt_str_len(tmp);
        if ((len + wlen) > buflen)
          len = mutt_wstr_trunc(tmp, buflen - wlen, cols - col, NULL);

        memcpy(wptr, tmp, len);
        wptr += len;
        wlen += len;
        col += mutt_strwidth(tmp);
      }
    }
    else if (*src == '\\')
    {
      if (!*++src)
        break;
      switch (*src)
      {
        case 'f':
          *wptr = '\f';
          break;
        case 'n':
          *wptr = '\n';
          break;
        case 'r':
          *wptr = '\r';
          break;
        case 't':
          *wptr = '\t';
          break;
        case 'v':
          *wptr = '\v';
          break;
        default:
          *wptr = *src;
          break;
      }
      src++;
      wptr++;
      wlen++;
      col++;
    }
    else
    {
      int bytes, width;
      /* in case of error, simply copy byte */
      bytes = mutt_mb_charlen(src, &width);
      if (bytes < 0)
      {
        bytes = 1;
        width = 1;
      }
      if ((bytes > 0) && ((wlen + bytes) < buflen))
      {
        memcpy(wptr, src, bytes);
        wptr += bytes;
        src += bytes;
        wlen += bytes;
        col += width;
      }
      else
      {
        src += buflen - wlen;
        wlen = buflen;
      }
    }
  }
  *wptr = '\0';
}

/**
 * mutt_open_read - Run a command to read from
 * @param[in]  path   Path to command
 * @param[out] thepid PID of the command
 * @retval ptr File containing output of command
 *
 * This function allows the user to specify a command to read stdout from in
 * place of a normal file.  If the last character in the string is a pipe (|),
 * then we assume it is a command to run instead of a normal file.
 */
FILE *mutt_open_read(const char *path, pid_t *thepid)
{
  FILE *fp = NULL;
  struct stat st = { 0 };

  size_t len = mutt_str_len(path);
  if (len == 0)
  {
    return NULL;
  }

  if (path[len - 1] == '|')
  {
    /* read from a pipe */

    char *p = mutt_str_dup(path);

    p[len - 1] = 0;
    mutt_endwin();
    *thepid = filter_create(p, NULL, &fp, NULL, EnvList);
    FREE(&p);
  }
  else
  {
    if (stat(path, &st) < 0)
      return NULL;
    if (S_ISDIR(st.st_mode))
    {
      errno = EINVAL;
      return NULL;
    }
    fp = fopen(path, "r");
    *thepid = -1;
  }
  return fp;
}

/**
 * mutt_save_confirm - Ask the user to save
 * @param s  Save location
 * @param st Timestamp
 * @retval  0 OK to proceed
 * @retval -1 to abort
 * @retval  1 to retry
 */
int mutt_save_confirm(const char *s, struct stat *st)
{
  int rc = 0;

  enum MailboxType type = mx_path_probe(s);

#ifdef USE_POP
  if (type == MUTT_POP)
  {
    mutt_error(_("Can't save message to POP mailbox"));
    return 1;
  }
#endif

  if ((type != MUTT_MAILBOX_ERROR) && (type != MUTT_UNKNOWN) && (mx_access(s, W_OK) == 0))
  {
    const bool c_confirm_append = cs_subset_bool(NeoMutt->sub, "confirm_append");
    if (c_confirm_append)
    {
      struct Buffer *tmp = buf_pool_get();
      buf_printf(tmp, _("Append messages to %s?"), s);
      enum QuadOption ans = query_yesorno_help(buf_string(tmp), MUTT_YES,
                                               NeoMutt->sub, "confirm_append");
      if (ans == MUTT_NO)
        rc = 1;
      else if (ans == MUTT_ABORT)
        rc = -1;
      buf_pool_release(&tmp);
    }
  }

#ifdef USE_NNTP
  if (type == MUTT_NNTP)
  {
    mutt_error(_("Can't save message to news server"));
    return 0;
  }
#endif

  if (stat(s, st) != -1)
  {
    if (type == MUTT_MAILBOX_ERROR)
    {
      mutt_error(_("%s is not a mailbox"), s);
      return 1;
    }
  }
  else if (type != MUTT_IMAP)
  {
    st->st_mtime = 0;
    st->st_atime = 0;

    /* pathname does not exist */
    if (errno == ENOENT)
    {
      const bool c_confirm_create = cs_subset_bool(NeoMutt->sub, "confirm_create");
      if (c_confirm_create)
      {
        struct Buffer *tmp = buf_pool_get();
        buf_printf(tmp, _("Create %s?"), s);
        enum QuadOption ans = query_yesorno_help(buf_string(tmp), MUTT_YES,
                                                 NeoMutt->sub, "confirm_create");
        if (ans == MUTT_NO)
          rc = 1;
        else if (ans == MUTT_ABORT)
          rc = -1;
        buf_pool_release(&tmp);
      }

      /* user confirmed with MUTT_YES or set `$confirm_create` */
      if (rc == 0)
      {
        /* create dir recursively */
        char *tmp_path = mutt_path_dirname(s);
        if (mutt_file_mkdir(tmp_path, S_IRWXU) == -1)
        {
          /* report failure & abort */
          mutt_perror("%s", s);
          FREE(&tmp_path);
          return 1;
        }
        FREE(&tmp_path);
      }
    }
    else
    {
      mutt_perror("%s", s);
      return 1;
    }
  }

  msgwin_clear_text(NULL);
  return rc;
}

/**
 * mutt_sleep - Sleep for a while
 * @param s Number of seconds to sleep
 *
 * If the user config '$sleep_time' is larger, sleep that long instead.
 */
void mutt_sleep(short s)
{
  const short c_sleep_time = cs_subset_number(NeoMutt->sub, "sleep_time");
  if (c_sleep_time > s)
    sleep(c_sleep_time);
  else if (s)
    sleep(s);
}

/**
 * mutt_make_version - Generate the NeoMutt version string
 * @retval ptr Version string
 *
 * @note This returns a pointer to a static buffer
 */
const char *mutt_make_version(void)
{
  static char vstring[256];
  snprintf(vstring, sizeof(vstring), "NeoMutt %s%s", PACKAGE_VERSION, GitVer);
  return vstring;
}

/**
 * mutt_encode_path - Convert a path to 'us-ascii'
 * @param buf Buffer for the result
 * @param src Path to convert (OPTIONAL)
 *
 * If `src` is NULL, the path in `buf` will be converted in-place.
 */
void mutt_encode_path(struct Buffer *buf, const char *src)
{
  char *p = mutt_str_dup(src);
  int rc = mutt_ch_convert_string(&p, cc_charset(), "us-ascii", MUTT_ICONV_NO_FLAGS);
  size_t len = buf_strcpy(buf, (rc == 0) ? NONULL(p) : NONULL(src));

  /* convert the path to POSIX "Portable Filename Character Set" */
  for (size_t i = 0; i < len; i++)
  {
    if (!isalnum(buf->data[i]) && !strchr("/.-_", buf->data[i]))
    {
      buf->data[i] = '_';
    }
  }
  FREE(&p);
}

/**
 * mutt_set_xdg_path - Find an XDG path or its fallback
 * @param type    Type of XDG variable, e.g. #XDG_CONFIG_HOME
 * @param buf     Buffer to save path
 * @retval 1 An entry was found that actually exists on disk and 0 otherwise
 *
 * Process an XDG environment variable or its fallback.
 */
int mutt_set_xdg_path(enum XdgType type, struct Buffer *buf)
{
  const char *xdg_env = mutt_str_getenv(XdgEnvVars[type]);
  char *xdg = xdg_env ? mutt_str_dup(xdg_env) : mutt_str_dup(XdgDefaults[type]);
  char *x = xdg; /* mutt_str_sep() changes xdg, so free x instead later */
  char *token = NULL;
  int rc = 0;

  while ((token = mutt_str_sep(&xdg, ":")))
  {
    if (buf_printf(buf, "%s/%s/neomuttrc", token, PACKAGE) < 0)
      continue;
    buf_expand_path(buf);
    if (access(buf_string(buf), F_OK) == 0)
    {
      rc = 1;
      break;
    }

    if (buf_printf(buf, "%s/%s/Muttrc", token, PACKAGE) < 0)
      continue;
    buf_expand_path(buf);
    if (access(buf_string(buf), F_OK) == 0)
    {
      rc = 1;
      break;
    }
  }

  FREE(&x);
  return rc;
}

/**
 * mutt_get_parent_path - Find the parent of a path (or mailbox)
 * @param path   Path to use
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 */
void mutt_get_parent_path(const char *path, char *buf, size_t buflen)
{
  enum MailboxType mb_type = mx_path_probe(path);

  const char *const c_folder = cs_subset_string(NeoMutt->sub, "folder");
  if (mb_type == MUTT_IMAP)
  {
    imap_get_parent_path(path, buf, buflen);
  }
  else if (mb_type == MUTT_NOTMUCH)
  {
    mutt_str_copy(buf, c_folder, buflen);
  }
  else
  {
    mutt_str_copy(buf, path, buflen);
    int n = mutt_str_len(buf);
    if (n == 0)
      return;

    /* remove any final trailing '/' */
    if (buf[n - 1] == '/')
      buf[n - 1] = '\0';

    /* Remove everything until the next slash */
    for (n--; ((n >= 0) && (buf[n] != '/')); n--)
      ; // do nothing

    if (n > 0)
    {
      buf[n] = '\0';
    }
    else
    {
      buf[0] = '/';
      buf[1] = '\0';
    }
  }
}

/**
 * mutt_inbox_cmp - Do two folders share the same path and one is an inbox - @ingroup sort_api
 * @param a First path
 * @param b Second path
 * @retval -1 a is INBOX of b
 * @retval  0 None is INBOX
 * @retval  1 b is INBOX for a
 *
 * This function compares two folder paths. It first looks for the position of
 * the last common '/' character. If a valid position is found and it's not the
 * last character in any of the two paths, the remaining parts of the paths are
 * compared (case insensitively) with the string "INBOX". If one of the two
 * paths matches, it's reported as being less than the other and the function
 * returns -1 (a < b) or 1 (a > b). If no paths match the requirements, the two
 * paths are considered equivalent and this function returns 0.
 *
 * Examples:
 * * mutt_inbox_cmp("/foo/bar",      "/foo/baz") --> 0
 * * mutt_inbox_cmp("/foo/bar/",     "/foo/bar/inbox") --> 0
 * * mutt_inbox_cmp("/foo/bar/sent", "/foo/bar/inbox") --> 1
 * * mutt_inbox_cmp("=INBOX",        "=Drafts") --> -1
 */
int mutt_inbox_cmp(const char *a, const char *b)
{
  /* fast-track in case the paths have been mutt_pretty_mailbox'ified */
  if ((a[0] == '+') && (b[0] == '+'))
  {
    return mutt_istr_equal(a + 1, "inbox") ? -1 :
           mutt_istr_equal(b + 1, "inbox") ? 1 :
                                             0;
  }

  const char *a_end = strrchr(a, '/');
  const char *b_end = strrchr(b, '/');

  /* If one path contains a '/', but not the other */
  if ((!a_end) ^ (!b_end))
    return 0;

  /* If neither path contains a '/' */
  if (!a_end)
    return 0;

  /* Compare the subpaths */
  size_t a_len = a_end - a;
  size_t b_len = b_end - b;
  size_t min = MIN(a_len, b_len);
  int same = (a[min] == '/') && (b[min] == '/') && (a[min + 1] != '\0') &&
             (b[min + 1] != '\0') && mutt_istrn_equal(a, b, min);

  if (!same)
    return 0;

  if (mutt_istr_equal(&a[min + 1], "inbox"))
    return -1;

  if (mutt_istr_equal(&b[min + 1], "inbox"))
    return 1;

  return 0;
}

/**
 * buf_sanitize_filename - Replace unsafe characters in a filename
 * @param buf   Buffer for the result
 * @param path  Filename to make safe
 * @param slash Replace '/' characters too
 */
void buf_sanitize_filename(struct Buffer *buf, const char *path, short slash)
{
  if (!buf || !path)
    return;

  buf_reset(buf);

  for (; *path; path++)
  {
    if ((slash && (*path == '/')) || !strchr(FilenameSafeChars, *path))
      buf_addch(buf, '_');
    else
      buf_addch(buf, *path);
  }
}

/**
 * mutt_str_pretty_size - Display an abbreviated size, like 3.4K
 * @param buf    Buffer for the result
 * @param buflen Length of the buffer
 * @param num    Number to abbreviate
 */
void mutt_str_pretty_size(char *buf, size_t buflen, size_t num)
{
  if (!buf || (buflen == 0))
    return;

  const bool c_size_show_bytes = cs_subset_bool(NeoMutt->sub, "size_show_bytes");
  const bool c_size_show_fractions = cs_subset_bool(NeoMutt->sub, "size_show_fractions");
  const bool c_size_show_mb = cs_subset_bool(NeoMutt->sub, "size_show_mb");
  const bool c_size_units_on_left = cs_subset_bool(NeoMutt->sub, "size_units_on_left");

  if (c_size_show_bytes && (num < 1024))
  {
    snprintf(buf, buflen, "%d", (int) num);
  }
  else if (num == 0)
  {
    mutt_str_copy(buf, c_size_units_on_left ? "K0" : "0K", buflen);
  }
  else if (c_size_show_fractions && (num < 10189)) /* 0.1K - 9.9K */
  {
    snprintf(buf, buflen, c_size_units_on_left ? "K%3.1f" : "%3.1fK",
             (num < 103) ? 0.1 : (num / 1024.0));
  }
  else if (!c_size_show_mb || (num < 1023949)) /* 10K - 999K */
  {
    /* 51 is magic which causes 10189/10240 to be rounded up to 10 */
    snprintf(buf, buflen, c_size_units_on_left ? ("K%zu") : ("%zuK"), (num + 51) / 1024);
  }
  else if (c_size_show_fractions && (num < 10433332)) /* 1.0M - 9.9M */
  {
    snprintf(buf, buflen, c_size_units_on_left ? "M%3.1f" : "%3.1fM", num / 1048576.0);
  }
  else /* 10M+ */
  {
    /* (10433332 + 52428) / 1048576 = 10 */
    snprintf(buf, buflen, c_size_units_on_left ? ("M%zu") : ("%zuM"), (num + 52428) / 1048576);
  }
}

/**
 * add_to_stailq - Add a string to a list
 * @param head String list
 * @param str  String to add
 *
 * @note Duplicate or empty strings will not be added
 */
void add_to_stailq(struct ListHead *head, const char *str)
{
  /* don't add a NULL or empty string to the list */
  if (!str || (*str == '\0'))
    return;

  /* check to make sure the item is not already on this list */
  struct ListNode *np = NULL;
  STAILQ_FOREACH(np, head, entries)
  {
    if (mutt_istr_equal(str, np->data))
    {
      return;
    }
  }
  mutt_list_insert_tail(head, mutt_str_dup(str));
}

/**
 * remove_from_stailq - Remove an item, matching a string, from a List
 * @param head Head of the List
 * @param str  String to match
 *
 * @note The string comparison is case-insensitive
 */
void remove_from_stailq(struct ListHead *head, const char *str)
{
  if (mutt_str_equal("*", str))
  {
    mutt_list_free(head); /* "unCMD *" means delete all current entries */
  }
  else
  {
    struct ListNode *np = NULL, *tmp = NULL;
    STAILQ_FOREACH_SAFE(np, head, entries, tmp)
    {
      if (mutt_istr_equal(str, np->data))
      {
        STAILQ_REMOVE(head, np, ListNode, entries);
        FREE(&np->data);
        FREE(&np);
        break;
      }
    }
  }
}
