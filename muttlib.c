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
 * @page muttlib Some miscellaneous functions
 *
 * Some miscellaneous functions
 */

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mutt/mutt.h"
#include "address/lib.h"
#include "config/lib.h"
#include "email/lib.h"
#include "core/lib.h"
#include "gui/lib.h"
#include "mutt.h"
#include "muttlib.h"
#include "alias.h"
#include "filter.h"
#include "format_flags.h"
#include "globals.h"
#include "hook.h"
#include "mx.h"
#include "ncrypt/ncrypt.h"
#include "protos.h"
#if defined(HAVE_SYSCALL_H)
#include <syscall.h>
#elif defined(HAVE_SYS_SYSCALL_H)
#include <sys/syscall.h>
#endif
#ifdef USE_IMAP
#include "imap/imap.h"
#endif

/* These Config Variables are only used in muttlib.c */
struct Regex *C_GecosMask; ///< Config: Regex for parsing GECOS field of /etc/passwd

static FILE *fp_random;

static const unsigned char base32[] = "abcdefghijklmnopqrstuvwxyz234567";

static const char *xdg_env_vars[] = {
  [XDG_CONFIG_HOME] = "XDG_CONFIG_HOME",
  [XDG_CONFIG_DIRS] = "XDG_CONFIG_DIRS",
};

static const char *xdg_defaults[] = {
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
  if (!(buf->data && buf->data[0]))
  {
    mutt_buffer_mktemp(buf);
  }
  else
  {
    struct Buffer *prefix = mutt_buffer_pool_get();
    mutt_buffer_strcpy(prefix, buf->data);
    mutt_file_sanitize_filename(prefix->data, true);
    mutt_buffer_printf(buf, "%s/%s", NONULL(C_Tmpdir), mutt_b2s(prefix));

    struct stat sb;
    if ((lstat(mutt_b2s(buf), &sb) == -1) && (errno == ENOENT))
      goto out;

    char *suffix = strchr(prefix->data, '.');
    if (suffix)
    {
      *suffix = '\0';
      suffix++;
    }
    mutt_buffer_mktemp_pfx_sfx(buf, prefix->data, suffix);

  out:
    mutt_buffer_pool_release(&prefix);
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
 * mutt_buffer_expand_path_regex - Create the canonical path (with regex char escaping)
 * @param buf     Buffer with path
 * @param regex If true, escape any regex characters
 *
 * @note The path is expanded in-place
 */
void mutt_buffer_expand_path_regex(struct Buffer *buf, bool regex)
{
  const char *s = NULL;
  const char *tail = "";

  bool recurse = false;

  struct Buffer *p = mutt_buffer_pool_get();
  struct Buffer *q = mutt_buffer_pool_get();
  struct Buffer *tmp = mutt_buffer_pool_get();

  do
  {
    recurse = false;
    s = mutt_b2s(buf);

    switch (*s)
    {
      case '~':
      {
        if ((s[1] == '/') || (s[1] == '\0'))
        {
          mutt_buffer_strcpy(p, HomeDir);
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
            mutt_buffer_strcpy(p, pw->pw_dir);
            if (t)
            {
              *t = '/';
              tail = t;
            }
            else
              tail = "";
          }
          else
          {
            /* user not found! */
            if (t)
              *t = '/';
            mutt_buffer_reset(p);
            tail = s;
          }
        }
        break;
      }

      case '=':
      case '+':
      {
        enum MailboxType mb_type = mx_path_probe(C_Folder, NULL);

        /* if folder = {host} or imap[s]://host/: don't append slash */
        if ((mb_type == MUTT_IMAP) && ((C_Folder[strlen(C_Folder) - 1] == '}') ||
                                       (C_Folder[strlen(C_Folder) - 1] == '/')))
        {
          mutt_buffer_strcpy(p, NONULL(C_Folder));
        }
        else if (mb_type == MUTT_NOTMUCH)
          mutt_buffer_strcpy(p, NONULL(C_Folder));
        else if (C_Folder && (C_Folder[strlen(C_Folder) - 1] == '/'))
          mutt_buffer_strcpy(p, NONULL(C_Folder));
        else
          mutt_buffer_printf(p, "%s/", NONULL(C_Folder));

        tail = s + 1;
        break;
      }

        /* elm compatibility, @ expands alias to user name */

      case '@':
      {
        struct AddressList *al = mutt_alias_lookup(s + 1);
        if (!TAILQ_EMPTY(al))
        {
          struct Email *e = email_new();
          e->env = mutt_env_new();
          mutt_addrlist_copy(&e->env->from, al, false);
          mutt_addrlist_copy(&e->env->to, al, false);

          /* TODO: fix mutt_default_save() to use Buffer */
          mutt_buffer_alloc(p, PATH_MAX);
          mutt_default_save(p->data, p->dsize, e);
          mutt_buffer_fix_dptr(p);

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
        mutt_buffer_strcpy(p, C_Mbox);
        tail = s + 1;
        break;
      }

      case '<':
      {
        mutt_buffer_strcpy(p, C_Record);
        tail = s + 1;
        break;
      }

      case '!':
      {
        if (s[1] == '!')
        {
          mutt_buffer_strcpy(p, LastFolder);
          tail = s + 2;
        }
        else
        {
          mutt_buffer_strcpy(p, C_Spoolfile);
          tail = s + 1;
        }
        break;
      }

      case '-':
      {
        mutt_buffer_strcpy(p, LastFolder);
        tail = s + 1;
        break;
      }

      case '^':
      {
        mutt_buffer_strcpy(p, CurrentFolder);
        tail = s + 1;
        break;
      }

      default:
      {
        mutt_buffer_reset(p);
        tail = s;
      }
    }

    if (regex && *(mutt_b2s(p)) && !recurse)
    {
      mutt_file_sanitize_regex(q, mutt_b2s(p));
      mutt_buffer_printf(tmp, "%s%s", mutt_b2s(q), tail);
    }
    else
      mutt_buffer_printf(tmp, "%s%s", mutt_b2s(p), tail);

    mutt_buffer_copy(buf, tmp);
  } while (recurse);

  mutt_buffer_pool_release(&p);
  mutt_buffer_pool_release(&q);
  mutt_buffer_pool_release(&tmp);

#ifdef USE_IMAP
  /* Rewrite IMAP path in canonical form - aids in string comparisons of
   * folders. May possibly fail, in which case buf should be the same. */
  if (imap_path_probe(mutt_b2s(buf), NULL) == MUTT_IMAP)
    imap_expand_path(buf);
  else
#endif
  {
    /* Resolve symbolic links */
    struct stat st;
    int rc = lstat(mutt_b2s(buf), &st);
    if ((rc != -1) && S_ISLNK(st.st_mode))
    {
      char path[PATH_MAX];
      if (realpath(mutt_b2s(buf), path))
      {
        mutt_buffer_strcpy(buf, path);
      }
    }
  }
}

/**
 * mutt_buffer_expand_path - Create the canonical path
 * @param buf     Buffer with path
 *
 * @note The path is expanded in-place
 */
void mutt_buffer_expand_path(struct Buffer *buf)
{
  mutt_buffer_expand_path_regex(buf, false);
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
  struct Buffer *tmp = mutt_buffer_pool_get();

  mutt_buffer_addstr(tmp, NONULL(buf));
  mutt_buffer_expand_path_regex(tmp, regex);
  mutt_str_strfcpy(buf, mutt_b2s(tmp), buflen);

  mutt_buffer_pool_release(&tmp);

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
 * regular expression in #C_GecosMask, otherwise assume that the GECOS field is a
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

  if (mutt_regex_capture(C_GecosMask, pw->pw_gecos, 1, pat_match))
  {
    mutt_str_strfcpy(dest, pw->pw_gecos + pat_match[0].rm_so,
                     MIN(pat_match[0].rm_eo - pat_match[0].rm_so + 1, destlen));
  }
  else if ((p = strchr(pw->pw_gecos, ',')))
    mutt_str_strfcpy(dest, pw->pw_gecos, MIN(destlen, p - pw->pw_gecos + 1));
  else
    mutt_str_strfcpy(dest, pw->pw_gecos, destlen);

  pwnl = strlen(pw->pw_name);

  for (int idx = 0; dest[idx]; idx++)
  {
    if (dest[idx] == '&')
    {
      memmove(&dest[idx + pwnl], &dest[idx + 1],
              MAX((ssize_t)(destlen - idx - pwnl - 1), 0));
      memcpy(&dest[idx], pw->pw_name, MIN(destlen - idx - 1, pwnl));
      dest[idx] = toupper((unsigned char) dest[idx]);
    }
  }

  return dest;
}

/**
 * mutt_needs_mailcap - Does this type need a mailcap entry do display
 * @param m Attachment body to be displayed
 * @retval true  NeoMutt requires a mailcap entry to display
 * @retval false otherwise
 */
bool mutt_needs_mailcap(struct Body *m)
{
  switch (m->type)
  {
    case TYPE_TEXT:
      if (mutt_str_strcasecmp("plain", m->subtype) == 0)
        return false;
      break;
    case TYPE_APPLICATION:
      if (((WithCrypto & APPLICATION_PGP) != 0) && mutt_is_application_pgp(m))
        return false;
      if (((WithCrypto & APPLICATION_SMIME) != 0) && mutt_is_application_smime(m))
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
 * @retval true If part is in plain text
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
    if (mutt_str_strcasecmp("delivery-status", s) == 0)
      return true;
  }

  if (((WithCrypto & APPLICATION_PGP) != 0) && (t == TYPE_APPLICATION))
  {
    if (mutt_str_strcasecmp("pgp-keys", s) == 0)
      return true;
  }

  return false;
}

/**
 * mutt_randbuf - Fill a buffer with randomness
 * @param buf    Buffer for result
 * @param buflen Size of buffer
 * @retval  0 Success
 * @retval -1 Error
 */
int mutt_randbuf(void *buf, size_t buflen)
{
  if (buflen > 1048576)
  {
    mutt_error(_("mutt_randbuf buflen=%zu"), buflen);
    return -1;
  }
/* XXX switch to HAVE_GETRANDOM and getrandom() in about 2017 */
#if defined(SYS_getrandom) && defined(__linux__)
  long ret;
  do
  {
    ret = syscall(SYS_getrandom, buf, buflen, 0, 0, 0, 0);
  } while ((ret == -1) && (errno == EINTR));
  if (ret == buflen)
    return 0;
#endif
  /* let's try urandom in case we're on an old kernel, or the user has
   * configured selinux, seccomp or something to not allow getrandom */
  if (!fp_random)
  {
    fp_random = fopen("/dev/urandom", "rb");
    if (!fp_random)
    {
      mutt_error(_("open /dev/urandom: %s"), strerror(errno));
      return -1;
    }
    setbuf(fp_random, NULL);
  }
  if (fread(buf, 1, buflen, fp_random) != buflen)
  {
    mutt_error(_("read /dev/urandom: %s"), strerror(errno));
    return -1;
  }

  return 0;
}

/**
 * mutt_rand_base32 - Fill a buffer with a base32-encoded random string
 * @param buf    Buffer for result
 * @param buflen Length of buffer
 */
void mutt_rand_base32(void *buf, size_t buflen)
{
  uint8_t *p = buf;

  if (mutt_randbuf(p, buflen) < 0)
    mutt_exit(1);
  for (size_t pos = 0; pos < buflen; pos++)
    p[pos] = base32[p[pos] % 32];
}

/**
 * mutt_rand32 - Create a 32-bit random number
 * @retval num Random number
 */
uint32_t mutt_rand32(void)
{
  uint32_t num = 0;

  if (mutt_randbuf(&num, sizeof(num)) < 0)
    mutt_exit(1);
  return num;
}

/**
 * mutt_rand64 - Create a 64-bit random number
 * @retval num Random number
 */
uint64_t mutt_rand64(void)
{
  uint64_t num = 0;

  if (mutt_randbuf(&num, sizeof(num)) < 0)
    mutt_exit(1);
  return num;
}

/**
 * mutt_buffer_mktemp_full - Create a temporary file
 * @param buf    Buffer for result
 * @param prefix Prefix for filename
 * @param suffix Suffix for filename
 * @param src    Source file of caller
 * @param line   Source line number of caller
 */
void mutt_buffer_mktemp_full(struct Buffer *buf, const char *prefix,
                             const char *suffix, const char *src, int line)
{
  mutt_buffer_printf(buf, "%s/%s-%s-%d-%d-%" PRIu64 "%s%s", NONULL(C_Tmpdir),
                     NONULL(prefix), NONULL(ShortHostname), (int) getuid(),
                     (int) getpid(), mutt_rand64(), suffix ? "." : "", NONULL(suffix));

  mutt_debug(LL_DEBUG3, "%s:%d: mutt_mktemp returns \"%s\"\n", src, line, mutt_b2s(buf));
  if (unlink(mutt_b2s(buf)) && (errno != ENOENT))
  {
    mutt_debug(LL_DEBUG1, "%s:%d: ERROR: unlink(\"%s\"): %s (errno %d)\n", src,
               line, mutt_b2s(buf), strerror(errno), errno);
  }
}

/**
 * mutt_mktemp_full - Create a temporary filename
 * @param buf    Buffer for result
 * @param buflen Length of buffer
 * @param prefix Prefix for filename
 * @param suffix Suffix for filename
 * @param src    Source file of caller
 * @param line   Source line number of caller
 *
 * @note This doesn't create the file, only the name
 */
void mutt_mktemp_full(char *buf, size_t buflen, const char *prefix,
                      const char *suffix, const char *src, int line)
{
  size_t n =
      snprintf(buf, buflen, "%s/%s-%s-%d-%d-%" PRIu64 "%s%s", NONULL(C_Tmpdir),
               NONULL(prefix), NONULL(ShortHostname), (int) getuid(),
               (int) getpid(), mutt_rand64(), suffix ? "." : "", NONULL(suffix));
  if (n >= buflen)
  {
    mutt_debug(LL_DEBUG1,
               "%s:%d: ERROR: insufficient buffer space to hold temporary "
               "filename! buflen=%zu but need %zu\n",
               src, line, buflen, n);
  }
  mutt_debug(LL_DEBUG3, "%s:%d: mutt_mktemp returns \"%s\"\n", src, line, buf);
  if (unlink(buf) && (errno != ENOENT))
  {
    mutt_debug(LL_DEBUG1, "%s:%d: ERROR: unlink(\"%s\"): %s (errno %d)\n", src,
               line, buf, strerror(errno), errno);
  }
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
  char tmp[PATH_MAX];

  scheme = url_check_scheme(buf);

  if ((scheme == U_IMAP) || (scheme == U_IMAPS))
  {
    imap_pretty_mailbox(buf, buflen, C_Folder);
    return;
  }

  if (scheme == U_NOTMUCH)
    return;

  /* if buf is an url, only collapse path component */
  if (scheme != U_UNKNOWN)
  {
    p = strchr(buf, ':') + 1;
    if (strncmp(p, "//", 2) == 0)
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
        *q++ = *p++;
    }
    *q = '\0';
  }
  else if (strstr(p, "..") && ((scheme == U_UNKNOWN) || (scheme == U_FILE)) &&
           realpath(p, tmp))
  {
    mutt_str_strfcpy(p, tmp, buflen - (p - buf));
  }

  if ((len = mutt_str_startswith(buf, C_Folder, CASE_MATCH)) && (buf[len] == '/'))
  {
    *buf++ = '=';
    memmove(buf, buf + len, mutt_str_strlen(buf + len) + 1);
  }
  else if ((len = mutt_str_startswith(buf, HomeDir, CASE_MATCH)) && (buf[len] == '/'))
  {
    *buf++ = '~';
    memmove(buf, buf + len - 1, mutt_str_strlen(buf + len - 1) + 1);
  }
}

/**
 * mutt_buffer_pretty_mailbox - Shorten a mailbox path using '~' or '='
 * @param buf Buffer containing Mailbox name
 */
void mutt_buffer_pretty_mailbox(struct Buffer *buf)
{
  if (!buf || !buf->data)
    return;
  /* This reduces the size of the Buffer, so we can pass it through.
   * We adjust the size just to make sure buf->data is not NULL though */
  mutt_buffer_alloc(buf, PATH_MAX);
  mutt_pretty_mailbox(buf->data, buf->dsize);
  mutt_buffer_fix_dptr(buf);
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
  struct stat st;

  mutt_buffer_strcpy(fname, path);
  if (access(mutt_b2s(fname), F_OK) != 0)
    return 0;
  if (stat(mutt_b2s(fname), &st) != 0)
    return -1;
  if (S_ISDIR(st.st_mode))
  {
    enum QuadOption ans = MUTT_NO;
    if (directory)
    {
      switch (mutt_multi_choice
              /* L10N: Means "The path you specified as the destination file is a directory."
                 See the msgid "Save to file: " (alias.c, recvattach.c)
                 These three letters correspond to the choices in the string.  */
              (_("File is a directory, save under it: (y)es, (n)o, (a)ll?"), _("yna")))
      {
        case 3: /* all */
          mutt_str_replace(directory, mutt_b2s(fname));
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
    else if ((ans = mutt_yesorno(_("File is a directory, save under it?"), MUTT_YES)) != MUTT_YES)
      return (ans == MUTT_NO) ? 1 : -1;

    struct Buffer *tmp = mutt_buffer_pool_get();
    mutt_buffer_strcpy(tmp, mutt_path_basename(NONULL(attname)));
    if ((mutt_buffer_get_field(_("File under directory: "), tmp, MUTT_FILE | MUTT_CLEAR) != 0) ||
        mutt_buffer_is_empty(tmp))
    {
      mutt_buffer_pool_release(&tmp);
      return (-1);
    }
    mutt_buffer_concat_path(fname, path, mutt_b2s(tmp));
    mutt_buffer_pool_release(&tmp);
  }

  if ((*opt == MUTT_SAVE_NO_FLAGS) && (access(mutt_b2s(fname), F_OK) == 0))
  {
    switch (
        mutt_multi_choice(_("File exists, (o)verwrite, (a)ppend, or (c)ancel?"),
                          // L10N: Options for: File exists, (o)verwrite, (a)ppend, or (c)ancel?
                          _("oac")))
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
    mutt_str_strfcpy(buf, addr->mailbox, buflen);
    if (!C_SaveAddress)
    {
      char *p = strpbrk(buf, "%@");
      if (p)
        *p = '\0';
    }
    mutt_str_strlower(buf);
  }
  else
    *buf = '\0';
}

/**
 * mutt_buffer_save_path - Make a safe filename from an email address
 * @param dest Buffer for the result
 * @param a    Address to use
 */
void mutt_buffer_save_path(struct Buffer *dest, const struct Address *a)
{
  if (a && a->mailbox)
  {
    mutt_buffer_strcpy(dest, a->mailbox);
    if (!C_SaveAddress)
    {
      char *p = strpbrk(dest->data, "%@");
      if (p)
      {
        *p = '\0';
        mutt_buffer_fix_dptr(dest);
      }
    }
    mutt_str_strlower(dest->data);
  }
  else
    mutt_buffer_reset(dest);
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
  mutt_buffer_save_path(dest, a);
  for (char *p = dest->data; *p; p++)
    if ((*p == '/') || IS_SPACE(*p) || !IsPrint((unsigned char) *p))
      *p = '_';
}

/**
 * mutt_expando_format - Expand expandos (%x) in a string
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
                         format_t *callback, unsigned long data, MuttFormatFlags flags)
{
  char prefix[128], tmp[1024];
  char *cp = NULL, *wptr = buf;
  char ch;
  char if_str[128], else_str[128];
  size_t wlen, count, len, wid;
  FILE *fp_filter = NULL;
  char *recycler = NULL;

  char src2[256];
  mutt_str_strfcpy(src2, src, mutt_str_strlen(src) + 1);
  src = src2;

  prefix[0] = '\0';
  buflen--; /* save room for the terminal \0 */
  wlen = ((flags & MUTT_FORMAT_ARROWCURSOR) && C_ArrowCursor) ? 3 : 0;
  col += wlen;

  if ((flags & MUTT_FORMAT_NOFILTER) == 0)
  {
    int off = -1;

    /* Do not consider filters if no pipe at end */
    int n = mutt_str_strlen(src);
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
      char srccopy[1024];
      int i = 0;

      mutt_debug(LL_DEBUG3, "fmtpipe = %s\n", src);

      strncpy(srccopy, src, n);
      srccopy[n - 1] = '\0';

      /* prepare Buffers */
      struct Buffer srcbuf = mutt_buffer_make(0);
      mutt_buffer_addstr(&srcbuf, srccopy);
      /* note: we are resetting dptr and *reading* from the buffer, so we don't
       * want to use mutt_buffer_reset(). */
      srcbuf.dptr = srcbuf.data;
      struct Buffer word = mutt_buffer_make(0);
      struct Buffer cmd = mutt_buffer_make(0);

      /* Iterate expansions across successive arguments */
      do
      {
        /* Extract the command name and copy to command line */
        mutt_debug(LL_DEBUG3, "fmtpipe +++: %s\n", srcbuf.dptr);
        if (word.data)
          *word.data = '\0';
        mutt_extract_token(&word, &srcbuf, MUTT_TOKEN_NO_FLAGS);
        mutt_debug(LL_DEBUG3, "fmtpipe %2d: %s\n", i++, word.data);
        mutt_buffer_addch(&cmd, '\'');
        mutt_expando_format(tmp, sizeof(tmp), 0, cols, word.data, callback,
                            data, flags | MUTT_FORMAT_NOFILTER);
        for (char *p = tmp; p && *p; p++)
        {
          if (*p == '\'')
          {
            /* shell quoting doesn't permit escaping a single quote within
             * single-quoted material.  double-quoting instead will lead
             * shell variable expansions, so break out of the single-quoted
             * span, insert a double-quoted single quote, and resume. */
            mutt_buffer_addstr(&cmd, "'\"'\"'");
          }
          else
            mutt_buffer_addch(&cmd, *p);
        }
        mutt_buffer_addch(&cmd, '\'');
        mutt_buffer_addch(&cmd, ' ');
      } while (MoreArgs(&srcbuf));

      mutt_debug(LL_DEBUG3, "fmtpipe > %s\n", cmd.data);

      col -= wlen; /* reset to passed in value */
      wptr = buf;  /* reset write ptr */
      pid_t pid = mutt_create_filter(cmd.data, NULL, &fp_filter, NULL);
      if (pid != -1)
      {
        int rc;

        n = fread(buf, 1, buflen /* already decremented */, fp_filter);
        mutt_file_fclose(&fp_filter);
        rc = mutt_wait_filter(pid);
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
              recycler = mutt_str_strdup(buf);
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

      mutt_buffer_dealloc(&cmd);
      mutt_buffer_dealloc(&srcbuf);
      mutt_buffer_dealloc(&word);
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
        for (; *p && *p != '?'; p++)
          ;
        /* nothing */
        if (*p == '?')
          p++;
        /* fix up the "y&z" section */
        for (; *p && *p != '?'; p++)
        {
          /* escape '<' and '>' to work inside nested-if */
          if ((*p == '<') || (*p == '>'))
          {
            memmove(p + 2, p, mutt_str_strlen(p) + 1);
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
        while ((count < sizeof(prefix)) && (*src != '?'))
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
        while ((count < sizeof(prefix)) && (isdigit((unsigned char) *src) || (*src == '.') ||
                                            (*src == '-') || (*src == '=')))
        {
          *cp++ = *src++;
          count++;
        }
        *cp = '\0';

        if (!*src)
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
        while ((lrbalance > 0) && (count < sizeof(else_str)) && *src)
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

        if (!*src)
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
          len = mutt_str_strlen(tmp);
          wid = mutt_strwidth(tmp);

          pad = (cols - col - wid) / pw;
          if (pad >= 0)
          {
            /* try to consume as many columns as we can, if we don't have
             * memory for that, use as much memory as possible */
            if (wlen + (pad * pl) + len > buflen)
              pad = (buflen > (wlen + len)) ? ((buflen - wlen - len) / pl) : 0;
            else
            {
              /* Add pre-spacing to make multi-column pad characters and
               * the contents after padding line up */
              while ((col + (pad * pw) + wid < cols) && (wlen + (pad * pl) + len < buflen))
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
            int offset = ((flags & MUTT_FORMAT_ARROWCURSOR) && C_ArrowCursor) ? 3 : 0;
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
            while ((col + wid < avail_cols) && (wlen + len < buflen))
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
          if ((c > 0) && (wlen + (c * pl) > buflen))
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
        src = callback(tmp, sizeof(tmp), col, cols, ch, src, prefix, if_str,
                       else_str, data, flags);

        if (to_lower)
          mutt_str_strlower(tmp);
        if (no_dots)
        {
          char *p = tmp;
          for (; *p; p++)
            if (*p == '.')
              *p = '_';
        }

        len = mutt_str_strlen(tmp);
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
  struct stat s;

  size_t len = mutt_str_strlen(path);
  if (len == 0)
  {
    return NULL;
  }

  if (path[len - 1] == '|')
  {
    /* read from a pipe */

    char *p = mutt_str_strdup(path);

    p[len - 1] = 0;
    mutt_endwin();
    *thepid = mutt_create_filter(p, NULL, &fp, NULL);
    FREE(&p);
  }
  else
  {
    if (stat(path, &s) < 0)
      return NULL;
    if (S_ISDIR(s.st_mode))
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
 * @retval  0 if OK to proceed
 * @retval -1 to abort
 * @retval  1 to retry
 */
int mutt_save_confirm(const char *s, struct stat *st)
{
  int ret = 0;

  enum MailboxType magic = mx_path_probe(s, NULL);

#ifdef USE_POP
  if (magic == MUTT_POP)
  {
    mutt_error(_("Can't save message to POP mailbox"));
    return 1;
  }
#endif

  if ((magic != MUTT_MAILBOX_ERROR) && (magic != MUTT_UNKNOWN) && !mx_access(s, W_OK))
  {
    if (C_Confirmappend)
    {
      struct Buffer *tmp = mutt_buffer_pool_get();
      mutt_buffer_printf(tmp, _("Append messages to %s?"), s);
      enum QuadOption ans = mutt_yesorno(mutt_b2s(tmp), MUTT_YES);
      if (ans == MUTT_NO)
        ret = 1;
      else if (ans == MUTT_ABORT)
        ret = -1;
      mutt_buffer_pool_release(&tmp);
    }
  }

#ifdef USE_NNTP
  if (magic == MUTT_NNTP)
  {
    mutt_error(_("Can't save message to news server"));
    return 0;
  }
#endif

  if (stat(s, st) != -1)
  {
    if (magic == MUTT_MAILBOX_ERROR)
    {
      mutt_error(_("%s is not a mailbox"), s);
      return 1;
    }
  }
  else if (magic != MUTT_IMAP)
  {
    st->st_mtime = 0;
    st->st_atime = 0;

    /* pathname does not exist */
    if (errno == ENOENT)
    {
      if (C_Confirmcreate)
      {
        struct Buffer *tmp = mutt_buffer_pool_get();
        mutt_buffer_printf(tmp, _("Create %s?"), s);
        enum QuadOption ans = mutt_yesorno(mutt_b2s(tmp), MUTT_YES);
        if (ans == MUTT_NO)
          ret = 1;
        else if (ans == MUTT_ABORT)
          ret = -1;
        mutt_buffer_pool_release(&tmp);
      }

      /* user confirmed with MUTT_YES or set C_Confirmcreate */
      if (ret == 0)
      {
        /* create dir recursively */
        char *tmp_path = mutt_path_dirname(s);
        if (mutt_file_mkdir(tmp_path, S_IRWXU) == -1)
        {
          /* report failure & abort */
          mutt_perror(s);
          FREE(&tmp_path);
          return 1;
        }
        FREE(&tmp_path);
      }
    }
    else
    {
      mutt_perror(s);
      return 1;
    }
  }

  mutt_window_clearline(MuttMessageWindow, 0);
  return ret;
}

/**
 * mutt_sleep - Sleep for a while
 * @param s Number of seconds to sleep
 *
 * If the user config '$sleep_time' is larger, sleep that long instead.
 */
void mutt_sleep(short s)
{
  if (C_SleepTime > s)
    sleep(C_SleepTime);
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
 * mutt_encode_path - Convert a path into the user's preferred character set
 * @param buf    Buffer for the result
 * @param buflen Length of buffer
 * @param src  Path to convert (OPTIONAL)
 *
 * If `src` is NULL, the path in `buf` will be converted in-place.
 */
void mutt_encode_path(char *buf, size_t buflen, const char *src)
{
  char *p = mutt_str_strdup(src);
  int rc = mutt_ch_convert_string(&p, C_Charset, "us-ascii", 0);
  /* 'src' may be NULL, such as when called from the pop3 driver. */
  size_t len = mutt_str_strfcpy(buf, (rc == 0) ? p : src, buflen);

  /* convert the path to POSIX "Portable Filename Character Set" */
  for (size_t i = 0; i < len; i++)
  {
    if (!isalnum(buf[i]) && !strchr("/.-_", buf[i]))
    {
      buf[i] = '_';
    }
  }
  FREE(&p);
}

/**
 * mutt_buffer_encode_path - Convert a path into the user's preferred character set
 * @param buf Buffer for the result
 * @param src Path to convert (OPTIONAL)
 *
 * If `src` is NULL, the path in `buf` will be converted in-place.
 */
void mutt_buffer_encode_path(struct Buffer *buf, const char *src)
{
  char *p = mutt_str_strdup(src);
  int rc = mutt_ch_convert_string(&p, C_Charset, "utf-8", 0);
  mutt_buffer_strcpy(buf, (rc == 0) ? NONULL(p) : NONULL(src));
  FREE(&p);
}

/**
 * mutt_set_xdg_path - Find an XDG path or its fallback
 * @param type    Type of XDG variable, e.g. #XDG_CONFIG_HOME
 * @param buf     Buffer to save path
 * @param bufsize Buffer length
 * @retval 1 if an entry was found that actually exists on disk and 0 otherwise
 *
 * Process an XDG environment variable or its fallback.
 */
int mutt_set_xdg_path(enum XdgType type, char *buf, size_t bufsize)
{
  const char *xdg_env = mutt_str_getenv(xdg_env_vars[type]);
  char *xdg = xdg_env ? mutt_str_strdup(xdg_env) : mutt_str_strdup(xdg_defaults[type]);
  char *x = xdg; /* strsep() changes xdg, so free x instead later */
  char *token = NULL;
  int rc = 0;

  while ((token = strsep(&xdg, ":")))
  {
    if (snprintf(buf, bufsize, "%s/%s/neomuttrc", token, PACKAGE) < 0)
      continue;
    mutt_expand_path(buf, bufsize);
    if (access(buf, F_OK) == 0)
    {
      rc = 1;
      break;
    }

    if (snprintf(buf, bufsize, "%s/%s/Muttrc", token, PACKAGE) < 0)
      continue;
    mutt_expand_path(buf, bufsize);
    if (access(buf, F_OK) == 0)
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
  enum MailboxType mb_magic = mx_path_probe(path, NULL);

  if (mb_magic == MUTT_IMAP)
    imap_get_parent_path(path, buf, buflen);
  else if (mb_magic == MUTT_NOTMUCH)
    mutt_str_strfcpy(buf, C_Folder, buflen);
  else
  {
    mutt_str_strfcpy(buf, path, buflen);
    int n = mutt_str_strlen(buf);
    if (n == 0)
      return;

    /* remove any final trailing '/' */
    if (buf[n - 1] == '/')
      buf[n - 1] = '\0';

    /* Remove everything until the next slash */
    for (n--; ((n >= 0) && (buf[n] != '/')); n--)
      ;

    if (n > 0)
      buf[n] = '\0';
    else
    {
      buf[0] = '/';
      buf[1] = '\0';
    }
  }
}

/**
 * mutt_inbox_cmp - do two folders share the same path and one is an inbox
 * @param a First path
 * @param b Second path
 * @retval -1 if a is INBOX of b
 * @retval 0 if none is INBOX
 * @retval 1 if b is INBOX for a
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
    return (mutt_str_strcasecmp(a + 1, "inbox") == 0) ?
               -1 :
               (mutt_str_strcasecmp(b + 1, "inbox") == 0) ? 1 : 0;
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
             (b[min + 1] != '\0') && (mutt_str_strncasecmp(a, b, min) == 0);

  if (!same)
    return 0;

  if (mutt_str_strcasecmp(&a[min + 1], "inbox") == 0)
    return -1;

  if (mutt_str_strcasecmp(&b[min + 1], "inbox") == 0)
    return 1;

  return 0;
}

/**
 * mutt_buffer_sanitize_filename - Replace unsafe characters in a filename
 * @param buf   Buffer for the result
 * @param path  Filename to make safe
 * @param slash Replace '/' characters too
 */
void mutt_buffer_sanitize_filename(struct Buffer *buf, const char *path, short slash)
{
  if (!buf || !path)
    return;

  mutt_buffer_reset(buf);

  for (; *path; path++)
  {
    if ((slash && (*path == '/')) || !strchr(filename_safe_chars, *path))
      mutt_buffer_addch(buf, '_');
    else
      mutt_buffer_addch(buf, *path);
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

  if (C_SizeShowBytes && (num < 1024))
  {
    snprintf(buf, buflen, "%d", (int) num);
  }
  else if (num == 0)
  {
    mutt_str_strfcpy(buf, C_SizeUnitsOnLeft ? "K0" : "0K", buflen);
  }
  else if (C_SizeShowFractions && (num < 10189)) /* 0.1K - 9.9K */
  {
    snprintf(buf, buflen, C_SizeUnitsOnLeft ? "K%3.1f" : "%3.1fK",
             (num < 103) ? 0.1 : (num / 1024.0));
  }
  else if (!C_SizeShowMb || (num < 1023949)) /* 10K - 999K */
  {
    /* 51 is magic which causes 10189/10240 to be rounded up to 10 */
    snprintf(buf, buflen, C_SizeUnitsOnLeft ? ("K%zu") : ("%zuK"), (num + 51) / 1024);
  }
  else if (C_SizeShowFractions && (num < 10433332)) /* 1.0M - 9.9M */
  {
    snprintf(buf, buflen, C_SizeUnitsOnLeft ? "M%3.1f" : "%3.1fM", num / 1048576.0);
  }
  else /* 10M+ */
  {
    /* (10433332 + 52428) / 1048576 = 10 */
    snprintf(buf, buflen, C_SizeUnitsOnLeft ? ("M%zu") : ("%zuM"), (num + 52428) / 1048576);
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
    if (mutt_str_strcasecmp(str, np->data) == 0)
    {
      return;
    }
  }
  mutt_list_insert_tail(head, mutt_str_strdup(str));
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
  if (mutt_str_strcmp("*", str) == 0)
    mutt_list_free(head); /* "unCMD *" means delete all current entries */
  else
  {
    struct ListNode *np = NULL, *tmp = NULL;
    STAILQ_FOREACH_SAFE(np, head, entries, tmp)
    {
      if (mutt_str_strcasecmp(str, np->data) == 0)
      {
        STAILQ_REMOVE(head, np, ListNode, entries);
        FREE(&np->data);
        FREE(&np);
        break;
      }
    }
  }
}
