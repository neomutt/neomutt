/**
 * Copyright (C) 1996-2000,2007,2010,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2008 Thomas Roessler <roessler@does-not-exist.org>
 *
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

#include "config.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include "mutt.h"
#include "buffer.h"
#include "filter.h"
#include "mailbox.h"
#include "mime.h"
#include "mutt_crypt.h"
#include "mutt_curses.h"
#include "mx.h"
#include "url.h"
#ifdef USE_IMAP
#include "imap/imap.h"
#endif
#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#endif

static const char *xdg_env_vars[] = {
        [kXDGConfigHome] = "XDG_CONFIG_HOME",
        [kXDGConfigDirs] = "XDG_CONFIG_DIRS",
};

static const char *xdg_defaults[] = {
        [kXDGConfigHome] = "~/.config", [kXDGConfigDirs] = "/etc/xdg",
};

BODY *mutt_new_body(void)
{
  BODY *p = safe_calloc(1, sizeof(BODY));

  p->disposition = DISPATTACH;
  p->use_disp = true;
  return p;
}


/* Modified by blong to accept a "suggestion" for file name.  If
 * that file exists, then construct one with unique name but
 * keep any extension.  This might fail, I guess.
 * Renamed to mutt_adv_mktemp so I only have to change where it's
 * called, and not all possible cases.
 */
void mutt_adv_mktemp(char *s, size_t l)
{
  char prefix[_POSIX_PATH_MAX];
  char *suffix = NULL;
  struct stat sb;

  if (s[0] == '\0')
  {
    mutt_mktemp(s, l);
  }
  else
  {
    strfcpy(prefix, s, sizeof(prefix));
    mutt_sanitize_filename(prefix, 1);
    snprintf(s, l, "%s/%s", NONULL(Tempdir), prefix);
    if (lstat(s, &sb) == -1 && errno == ENOENT)
      return;

    if ((suffix = strrchr(prefix, '.')) != NULL)
    {
      *suffix = 0;
      ++suffix;
    }
    mutt_mktemp_pfx_sfx(s, l, prefix, suffix);
  }
}

/* create a send-mode duplicate from a receive-mode body */
int mutt_copy_body(FILE *fp, BODY **tgt, BODY *src)
{
  if (!tgt || !src)
    return -1;

  char tmp[_POSIX_PATH_MAX];
  BODY *b = NULL;

  PARAMETER *par = NULL, **ppar = NULL;

  bool use_disp;

  if (src->filename)
  {
    use_disp = true;
    strfcpy(tmp, src->filename, sizeof(tmp));
  }
  else
  {
    use_disp = false;
    tmp[0] = '\0';
  }

  mutt_adv_mktemp(tmp, sizeof(tmp));
  if (mutt_save_attachment(fp, src, tmp, 0, NULL) == -1)
    return -1;

  *tgt = mutt_new_body();
  b = *tgt;

  memcpy(b, src, sizeof(BODY));
  b->parts = NULL;
  b->next = NULL;

  b->filename = safe_strdup(tmp);
  b->use_disp = use_disp;
  b->unlink = true;

  if (mutt_is_text_part(b))
    b->noconv = true;

  b->xtype = safe_strdup(b->xtype);
  b->subtype = safe_strdup(b->subtype);
  b->form_name = safe_strdup(b->form_name);
  b->d_filename = safe_strdup(b->d_filename);
  /* mutt_adv_mktemp() will mangle the filename in tmp,
   * so preserve it in d_filename */
  if (!b->d_filename && use_disp)
    b->d_filename = safe_strdup(src->filename);
  b->description = safe_strdup(b->description);

  /*
   * we don't seem to need the HEADER structure currently.
   * XXX - this may change in the future
   */

  if (b->hdr)
    b->hdr = NULL;

  /* copy parameters */
  for (par = b->parameter, ppar = &b->parameter; par; ppar = &(*ppar)->next, par = par->next)
  {
    *ppar = mutt_new_parameter();
    (*ppar)->attribute = safe_strdup(par->attribute);
    (*ppar)->value = safe_strdup(par->value);
  }

  mutt_stamp_attachment(b);

  return 0;
}


void mutt_free_body(BODY **p)
{
  BODY *a = *p, *b = NULL;

  while (a)
  {
    b = a;
    a = a->next;

    if (b->parameter)
      mutt_free_parameter(&b->parameter);
    if (b->filename)
    {
      if (b->unlink)
        unlink(b->filename);
      mutt_debug(1, "mutt_free_body: %sunlinking %s.\n",
                 b->unlink ? "" : "not ", b->filename);
    }

    FREE(&b->filename);
    FREE(&b->d_filename);
    FREE(&b->charset);
    FREE(&b->content);
    FREE(&b->xtype);
    FREE(&b->subtype);
    FREE(&b->description);
    FREE(&b->form_name);

    if (b->hdr)
    {
      /* Don't free twice (b->hdr->content = b->parts) */
      b->hdr->content = NULL;
      mutt_free_header(&b->hdr);
    }

    if (b->parts)
      mutt_free_body(&b->parts);

    FREE(&b);
  }

  *p = 0;
}

void mutt_free_parameter(PARAMETER **p)
{
  PARAMETER *t = *p;
  PARAMETER *o = NULL;

  while (t)
  {
    FREE(&t->attribute);
    FREE(&t->value);
    o = t;
    t = t->next;
    FREE(&o);
  }
  *p = 0;
}

LIST *mutt_add_list(LIST *head, const char *data)
{
  size_t len = mutt_strlen(data);

  return mutt_add_list_n(head, data, len ? len + 1 : 0);
}

LIST *mutt_add_list_n(LIST *head, const void *data, size_t len)
{
  LIST *tmp = NULL;

  for (tmp = head; tmp && tmp->next; tmp = tmp->next)
    ;
  if (tmp)
  {
    tmp->next = safe_malloc(sizeof(LIST));
    tmp = tmp->next;
  }
  else
    head = tmp = safe_malloc(sizeof(LIST));

  tmp->data = safe_malloc(len);
  if (len)
    memcpy(tmp->data, data, len);
  tmp->next = NULL;
  return head;
}

LIST *mutt_find_list(LIST *l, const char *data)
{
  LIST *p = l;

  while (p)
  {
    if (data == p->data)
      return p;
    if (data && p->data && (mutt_strcmp(p->data, data) == 0))
      return p;
    p = p->next;
  }
  return NULL;
}

void mutt_push_list(LIST **head, const char *data)
{
  LIST *tmp = NULL;
  tmp = safe_malloc(sizeof(LIST));
  tmp->data = safe_strdup(data);
  tmp->next = *head;
  *head = tmp;
}

bool mutt_pop_list(LIST **head)
{
  LIST *elt = *head;
  if (!elt)
    return false;
  *head = elt->next;
  FREE(&elt->data);
  FREE(&elt);
  return true;
}

const char *mutt_front_list(LIST *head)
{
  if (!head || !head->data)
    return "";
  return head->data;
}

int mutt_remove_from_rx_list(RX_LIST **l, const char *str)
{
  RX_LIST *p = NULL, *last = NULL;
  int rv = -1;

  if (mutt_strcmp("*", str) == 0)
  {
    mutt_free_rx_list(l); /* ``unCMD *'' means delete all current entries */
    rv = 0;
  }
  else
  {
    p = *l;
    last = NULL;
    while (p)
    {
      if (ascii_strcasecmp(str, p->rx->pattern) == 0)
      {
        mutt_free_regexp(&p->rx);
        if (last)
          last->next = p->next;
        else
          (*l) = p->next;
        FREE(&p);
        rv = 0;
      }
      else
      {
        last = p;
        p = p->next;
      }
    }
  }
  return rv;
}

void mutt_free_list(LIST **list)
{
  LIST *p = NULL;

  if (!list)
    return;
  while (*list)
  {
    p = *list;
    *list = (*list)->next;
    FREE(&p->data);
    FREE(&p);
  }
}

LIST *mutt_copy_list(LIST *p)
{
  LIST *t = NULL, *r = NULL, *l = NULL;

  for (; p; p = p->next)
  {
    t = safe_malloc(sizeof(LIST));
    t->data = safe_strdup(p->data);
    t->next = NULL;
    if (l)
    {
      r->next = t;
      r = r->next;
    }
    else
      l = r = t;
  }
  return l;
}

void mutt_free_header(HEADER **h)
{
  if (!h || !*h)
    return;
  mutt_free_envelope(&(*h)->env);
  mutt_free_body(&(*h)->content);
  FREE(&(*h)->maildir_flags);
  FREE(&(*h)->tree);
  FREE(&(*h)->path);
#ifdef MIXMASTER
  mutt_free_list(&(*h)->chain);
#endif
#if defined(USE_POP) || defined(USE_IMAP) || defined(USE_NNTP) || defined(USE_NOTMUCH)
  if ((*h)->free_cb)
    (*h)->free_cb(*h);
  FREE(&(*h)->data);
#endif
  FREE(h); /* __FREE_CHECKED__ */
}

/* returns true if the header contained in "s" is in list "t" */
bool mutt_matches_list(const char *s, LIST *t)
{
  for (; t; t = t->next)
  {
    if ((ascii_strncasecmp(s, t->data, mutt_strlen(t->data)) == 0) || *t->data == '*')
      return true;
  }
  return false;
}

/* checks Ignore and UnIgnore using mutt_matches_list */
int mutt_matches_ignore(const char *s)
{
  return mutt_matches_list(s, Ignore) && !mutt_matches_list(s, UnIgnore);
}

char *mutt_expand_path(char *s, size_t slen)
{
  return _mutt_expand_path(s, slen, 0);
}

char *_mutt_expand_path(char *s, size_t slen, int rx)
{
  char p[_POSIX_PATH_MAX] = "";
  char q[_POSIX_PATH_MAX] = "";
  char tmp[_POSIX_PATH_MAX];
  char *t = NULL;

  char *tail = "";

  int recurse = 0;

  do
  {
    recurse = 0;

    switch (*s)
    {
      case '~':
      {
        if (*(s + 1) == '/' || *(s + 1) == 0)
        {
          strfcpy(p, NONULL(Homedir), sizeof(p));
          tail = s + 1;
        }
        else
        {
          struct passwd *pw = NULL;
          if ((t = strchr(s + 1, '/')))
            *t = 0;

          if ((pw = getpwnam(s + 1)))
          {
            strfcpy(p, pw->pw_dir, sizeof(p));
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
            *p = '\0';
            tail = s;
          }
        }
      }
      break;

      case '=':
      case '+':
      {
#ifdef USE_IMAP
        /* if folder = {host} or imap[s]://host/: don't append slash */
        if (mx_is_imap(NONULL(Maildir)) && (Maildir[strlen(Maildir) - 1] == '}' ||
                                            Maildir[strlen(Maildir) - 1] == '/'))
          strfcpy(p, NONULL(Maildir), sizeof(p));
        else
#endif
#ifdef USE_NOTMUCH
            if (mx_is_notmuch(NONULL(Maildir)))
          strfcpy(p, NONULL(Maildir), sizeof(p));
        else
#endif
            if (Maildir && *Maildir && Maildir[strlen(Maildir) - 1] == '/')
          strfcpy(p, NONULL(Maildir), sizeof(p));
        else
          snprintf(p, sizeof(p), "%s/", NONULL(Maildir));

        tail = s + 1;
      }
      break;

      /* elm compatibility, @ expands alias to user name */

      case '@':
      {
        HEADER *h = NULL;
        ADDRESS *alias = NULL;

        if ((alias = mutt_lookup_alias(s + 1)))
        {
          h = mutt_new_header();
          h->env = mutt_new_envelope();
          h->env->from = h->env->to = alias;
          mutt_default_save(p, sizeof(p), h);
          h->env->from = h->env->to = NULL;
          mutt_free_header(&h);
          /* Avoid infinite recursion if the resulting folder starts with '@' */
          if (*p != '@')
            recurse = 1;

          tail = "";
        }
      }
      break;

      case '>':
      {
        strfcpy(p, NONULL(Inbox), sizeof(p));
        tail = s + 1;
      }
      break;

      case '<':
      {
        strfcpy(p, NONULL(Outbox), sizeof(p));
        tail = s + 1;
      }
      break;

      case '!':
      {
        if (*(s + 1) == '!')
        {
          strfcpy(p, NONULL(LastFolder), sizeof(p));
          tail = s + 2;
        }
        else
        {
          strfcpy(p, NONULL(Spoolfile), sizeof(p));
          tail = s + 1;
        }
      }
      break;

      case '-':
      {
        strfcpy(p, NONULL(LastFolder), sizeof(p));
        tail = s + 1;
      }
      break;

      case '^':
      {
        strfcpy(p, NONULL(CurrentFolder), sizeof(p));
        tail = s + 1;
      }
      break;

      default:
      {
        *p = '\0';
        tail = s;
      }
    }

    if (rx && *p && !recurse)
    {
      mutt_rx_sanitize_string(q, sizeof(q), p);
      snprintf(tmp, sizeof(tmp), "%s%s", q, tail);
    }
    else
      snprintf(tmp, sizeof(tmp), "%s%s", p, tail);

    strfcpy(s, tmp, slen);
  } while (recurse);

#ifdef USE_IMAP
  /* Rewrite IMAP path in canonical form - aids in string comparisons of
   * folders. May possibly fail, in which case s should be the same. */
  if (mx_is_imap(s))
    imap_expand_path(s, slen);
#endif

  return s;
}

/* Extract the real name from /etc/passwd's GECOS field.
 * When set, honor the regular expression in GecosMask,
 * otherwise assume that the GECOS field is a
 * comma-separated list.
 * Replace "&" by a capitalized version of the user's login
 * name.
 */
char *mutt_gecos_name(char *dest, size_t destlen, struct passwd *pw)
{
  regmatch_t pat_match[1];
  size_t pwnl;
  int idx;
  char *p = NULL;

  if (!pw || !pw->pw_gecos)
    return NULL;

  memset(dest, 0, destlen);

  if (GecosMask.rx)
  {
    if (regexec(GecosMask.rx, pw->pw_gecos, 1, pat_match, 0) == 0)
      strfcpy(dest, pw->pw_gecos + pat_match[0].rm_so,
              MIN(pat_match[0].rm_eo - pat_match[0].rm_so + 1, destlen));
  }
  else if ((p = strchr(pw->pw_gecos, ',')))
    strfcpy(dest, pw->pw_gecos, MIN(destlen, p - pw->pw_gecos + 1));
  else
    strfcpy(dest, pw->pw_gecos, destlen);

  pwnl = strlen(pw->pw_name);

  for (idx = 0; dest[idx]; idx++)
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


char *mutt_get_parameter(const char *s, PARAMETER *p)
{
  for (; p; p = p->next)
    if (ascii_strcasecmp(s, p->attribute) == 0)
      return p->value;

  return NULL;
}

void mutt_set_parameter(const char *attribute, const char *value, PARAMETER **p)
{
  PARAMETER *q = NULL;

  if (!value)
  {
    mutt_delete_parameter(attribute, p);
    return;
  }

  for (q = *p; q; q = q->next)
  {
    if (ascii_strcasecmp(attribute, q->attribute) == 0)
    {
      mutt_str_replace(&q->value, value);
      return;
    }
  }

  q = mutt_new_parameter();
  q->attribute = safe_strdup(attribute);
  q->value = safe_strdup(value);
  q->next = *p;
  *p = q;
}

void mutt_delete_parameter(const char *attribute, PARAMETER **p)
{
  PARAMETER *q = NULL;

  for (q = *p; q; p = &q->next, q = q->next)
  {
    if (ascii_strcasecmp(attribute, q->attribute) == 0)
    {
      *p = q->next;
      q->next = NULL;
      mutt_free_parameter(&q);
      return;
    }
  }
}

/* returns 1 if Mutt can't display this type of data, 0 otherwise */
bool mutt_needs_mailcap(BODY *m)
{
  switch (m->type)
  {
    case TYPETEXT:
      if (ascii_strcasecmp("plain", m->subtype) == 0)
        return false;
      break;
    case TYPEAPPLICATION:
      if ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp(m))
        return false;
      if ((WithCrypto & APPLICATION_SMIME) && mutt_is_application_smime(m))
        return false;
      break;

    case TYPEMULTIPART:
    case TYPEMESSAGE:
      return false;
  }

  return true;
}

bool mutt_is_text_part(BODY *b)
{
  int t = b->type;
  char *s = b->subtype;

  if ((WithCrypto & APPLICATION_PGP) && mutt_is_application_pgp(b))
    return false;

  if (t == TYPETEXT)
    return true;

  if (t == TYPEMESSAGE)
  {
    if (ascii_strcasecmp("delivery-status", s) == 0)
      return true;
  }

  if ((WithCrypto & APPLICATION_PGP) && t == TYPEAPPLICATION)
  {
    if (ascii_strcasecmp("pgp-keys", s) == 0)
      return true;
  }

  return false;
}

void mutt_free_envelope(ENVELOPE **p)
{
  if (!*p)
    return;
  rfc822_free_address(&(*p)->return_path);
  rfc822_free_address(&(*p)->from);
  rfc822_free_address(&(*p)->to);
  rfc822_free_address(&(*p)->cc);
  rfc822_free_address(&(*p)->bcc);
  rfc822_free_address(&(*p)->sender);
  rfc822_free_address(&(*p)->reply_to);
  rfc822_free_address(&(*p)->mail_followup_to);

  FREE(&(*p)->list_post);
  FREE(&(*p)->subject);
  /* real_subj is just an offset to subject and shouldn't be freed */
  FREE(&(*p)->disp_subj);
  FREE(&(*p)->message_id);
  FREE(&(*p)->supersedes);
  FREE(&(*p)->date);
  FREE(&(*p)->x_label);
  FREE(&(*p)->organization);
#ifdef USE_NNTP
  FREE(&(*p)->newsgroups);
  FREE(&(*p)->xref);
  FREE(&(*p)->followup_to);
  FREE(&(*p)->x_comment_to);
#endif

  mutt_buffer_free(&(*p)->spam);

  mutt_free_list(&(*p)->references);
  mutt_free_list(&(*p)->in_reply_to);
  mutt_free_list(&(*p)->userhdrs);
  FREE(p); /* __FREE_CHECKED__ */
}

/* move all the headers from extra not present in base into base */
void mutt_merge_envelopes(ENVELOPE *base, ENVELOPE **extra)
{
/* copies each existing element if necessary, and sets the element
  * to NULL in the source so that mutt_free_envelope doesn't leave us
  * with dangling pointers. */
#define MOVE_ELEM(h)                                                           \
  if (!base->h)                                                                \
  {                                                                            \
    base->h = (*extra)->h;                                                     \
    (*extra)->h = NULL;                                                        \
  }
  MOVE_ELEM(return_path);
  MOVE_ELEM(from);
  MOVE_ELEM(to);
  MOVE_ELEM(cc);
  MOVE_ELEM(bcc);
  MOVE_ELEM(sender);
  MOVE_ELEM(reply_to);
  MOVE_ELEM(mail_followup_to);
  MOVE_ELEM(list_post);
  MOVE_ELEM(message_id);
  MOVE_ELEM(supersedes);
  MOVE_ELEM(date);
  MOVE_ELEM(x_label);
  MOVE_ELEM(x_original_to);
  if (!base->refs_changed)
  {
    MOVE_ELEM(references);
  }
  if (!base->irt_changed)
  {
    MOVE_ELEM(in_reply_to);
  }

  /* real_subj is subordinate to subject */
  if (!base->subject)
  {
    base->subject = (*extra)->subject;
    base->real_subj = (*extra)->real_subj;
    base->disp_subj = (*extra)->disp_subj;
    (*extra)->subject = NULL;
    (*extra)->real_subj = NULL;
    (*extra)->disp_subj = NULL;
  }
  /* spam and user headers should never be hashed, and the new envelope may
    * have better values. Use new versions regardless. */
  mutt_buffer_free(&base->spam);
  mutt_free_list(&base->userhdrs);
  MOVE_ELEM(spam);
  MOVE_ELEM(userhdrs);
#undef MOVE_ELEM

  mutt_free_envelope(extra);
}

static FILE *frandom;

static void mutt_randbuf(void *out, size_t len)
{
  if (len > 1048576)
  {
    mutt_error(_("mutt_randbuf len=%zu"), len);
    exit(1);
  }
/* XXX switch to HAVE_GETRANDOM and getrandom() in about 2017 */
#if defined(SYS_getrandom) && defined(__linux__)
  long ret;
  do
  {
    ret = syscall(SYS_getrandom, out, len, 0, 0, 0, 0);
  } while ((ret == -1) && (errno == EINTR));
  if (ret == len)
    return;
/* let's try urandom in case we're on an old kernel, or the user has
   * configured selinux, seccomp or something to not allow getrandom */
#endif
  if (frandom == NULL)
  {
    frandom = fopen("/dev/urandom", "rb");
    if (frandom == NULL)
    {
      mutt_error(_("open /dev/urandom: %s"), strerror(errno));
      exit(1);
    }
    setbuf(frandom, NULL);
  }
  if (fread(out, 1, len, frandom) != len)
  {
    mutt_error(_("read /dev/urandom: %s"), strerror(errno));
    exit(1);
  }
}

static const unsigned char base32[] = "abcdefghijklmnopqrstuvwxyz234567";

void mutt_rand_base32(void *out, size_t len)
{
  size_t pos;
  uint8_t *p = out;

  mutt_randbuf(p, len);
  for (pos = 0; pos < len; pos++)
    p[pos] = base32[p[pos] % 32];
}

uint32_t mutt_rand32(void)
{
  uint32_t ret;

  mutt_randbuf(&ret, sizeof(ret));
  return ret;
}

uint64_t mutt_rand64(void)
{
  uint64_t ret;

  mutt_randbuf(&ret, sizeof(ret));
  return ret;
}


void _mutt_mktemp(char *s, size_t slen, const char *prefix, const char *suffix,
                  const char *src, int line)
{
  size_t n = snprintf(s, slen, "%s/%s-%s-%d-%d-%" PRIu64 "%s%s", NONULL(Tempdir),
                      NONULL(prefix), NONULL(Hostname), (int) getuid(), (int) getpid(),
                      mutt_rand64(), suffix ? "." : "", NONULL(suffix));
  if (n >= slen)
    mutt_debug(1, "%s:%d: ERROR: insufficient buffer space to hold temporary "
                  "filename! slen=%zu but need %zu\n",
               src, line, slen, n);
  mutt_debug(3, "%s:%d: mutt_mktemp returns \"%s\".\n", src, line, s);
  if (unlink(s) && errno != ENOENT)
    mutt_debug(1, "%s:%d: ERROR: unlink(\"%s\"): %s (errno %d)\n", src, line, s,
               strerror(errno), errno);
}

void mutt_free_alias(ALIAS **p)
{
  ALIAS *t = NULL;

  while (*p)
  {
    t = *p;
    *p = (*p)->next;
    mutt_alias_delete_reverse(t);
    FREE(&t->name);
    rfc822_free_address(&t->addr);
    FREE(&t);
  }
}

/* collapse the pathname using ~ or = when possible */
void mutt_pretty_mailbox(char *s, size_t buflen)
{
  char *p = s, *q = s;
  size_t len;
  url_scheme_t scheme;
  char tmp[PATH_MAX];

  scheme = url_check_scheme(s);

#ifdef USE_IMAP
  if (scheme == U_IMAP || scheme == U_IMAPS)
  {
    imap_pretty_mailbox(s);
    return;
  }
#endif

#ifdef USE_NOTMUCH
  if (scheme == U_NOTMUCH)
    return;
#endif

  /* if s is an url, only collapse path component */
  if (scheme != U_UNKNOWN)
  {
    p = strchr(s, ':') + 1;
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
     * lightweight than realpath() and doesn't resolve links
     */
    while (*p)
    {
      if (*p == '/' && p[1] == '/')
      {
        *q++ = '/';
        p += 2;
      }
      else if (p[0] == '/' && p[1] == '.' && p[2] == '/')
      {
        *q++ = '/';
        p += 3;
      }
      else
        *q++ = *p++;
    }
    *q = 0;
  }
  else if (strstr(p, "..") && (scheme == U_UNKNOWN || scheme == U_FILE) && realpath(p, tmp))
    strfcpy(p, tmp, buflen - (p - s));

  if ((mutt_strncmp(s, Maildir, (len = mutt_strlen(Maildir))) == 0) && s[len] == '/')
  {
    *s++ = '=';
    memmove(s, s + len, mutt_strlen(s + len) + 1);
  }
  else if ((mutt_strncmp(s, Homedir, (len = mutt_strlen(Homedir))) == 0) && s[len] == '/')
  {
    *s++ = '~';
    memmove(s, s + len - 1, mutt_strlen(s + len - 1) + 1);
  }
}

void mutt_pretty_size(char *s, size_t len, LOFF_T n)
{
  if (n == 0)
    strfcpy(s, "0K", len);
  else if (n < 10189) /* 0.1K - 9.9K */
    snprintf(s, len, "%3.1fK", (n < 103) ? 0.1 : n / 1024.0);
  else if (n < 1023949) /* 10K - 999K */
  {
    /* 51 is magic which causes 10189/10240 to be rounded up to 10 */
    snprintf(s, len, OFF_T_FMT "K", (n + 51) / 1024);
  }
  else if (n < 10433332) /* 1.0M - 9.9M */
    snprintf(s, len, "%3.1fM", n / 1048576.0);
  else /* 10M+ */
  {
    /* (10433332 + 52428) / 1048576 = 10 */
    snprintf(s, len, OFF_T_FMT "M", (n + 52428) / 1048576);
  }
}

void mutt_expand_file_fmt(char *dest, size_t destlen, const char *fmt, const char *src)
{
  char tmp[LONG_STRING];

  mutt_quote_filename(tmp, sizeof(tmp), src);
  mutt_expand_fmt(dest, destlen, fmt, tmp);
}

void mutt_expand_fmt(char *dest, size_t destlen, const char *fmt, const char *src)
{
  const char *p = NULL;
  char *d = NULL;
  size_t slen;
  int found = 0;

  slen = mutt_strlen(src);
  destlen--;

  for (p = fmt, d = dest; destlen && *p; p++)
  {
    if (*p == '%')
    {
      switch (p[1])
      {
        case '%':
          *d++ = *p++;
          destlen--;
          break;
        case 's':
          found = 1;
          strfcpy(d, src, destlen + 1);
          d += destlen > slen ? slen : destlen;
          destlen -= destlen > slen ? slen : destlen;
          p++;
          break;
        default:
          *d++ = *p;
          destlen--;
          break;
      }
    }
    else
    {
      *d++ = *p;
      destlen--;
    }
  }

  *d = '\0';

  if (!found && destlen > 0)
  {
    safe_strcat(dest, destlen, " ");
    safe_strcat(dest, destlen, src);
  }
}

/* return 0 on success, -1 on abort, 1 on error */
int mutt_check_overwrite(const char *attname, const char *path, char *fname,
                         size_t flen, int *append, char **directory)
{
  int rc = 0;
  char tmp[_POSIX_PATH_MAX];
  struct stat st;

  strfcpy(fname, path, flen);
  if (access(fname, F_OK) != 0)
    return 0;
  if (stat(fname, &st) != 0)
    return -1;
  if (S_ISDIR(st.st_mode))
  {
    if (directory)
    {
      switch (mutt_multi_choice
              /* L10N:
         Means "The path you specified as the destination file is a directory."
         See the msgid "Save to file: " (alias.c, recvattach.c) */
              (_("File is a directory, save under it? [(y)es, (n)o, (a)ll]"), _("yna")))
      {
        case 3: /* all */
          mutt_str_replace(directory, fname);
          break;
        case 1:            /* yes */
          FREE(directory); /* __FREE_CHECKED__ */
          break;
        case -1:           /* abort */
          FREE(directory); /* __FREE_CHECKED__ */
          return -1;
        case 2:            /* no */
          FREE(directory); /* __FREE_CHECKED__ */
          return 1;
      }
    }
    /* L10N:
       Means "The path you specified as the destination file is a directory."
       See the msgid "Save to file: " (alias.c, recvattach.c) */
    else if ((rc = mutt_yesorno(_("File is a directory, save under it?"), MUTT_YES)) != MUTT_YES)
      return (rc == MUTT_NO) ? 1 : -1;

    strfcpy(tmp, mutt_basename(NONULL(attname)), sizeof(tmp));
    if (mutt_get_field(_("File under directory: "), tmp, sizeof(tmp),
                       MUTT_FILE | MUTT_CLEAR) != 0 ||
        !tmp[0])
      return -1;
    mutt_concat_path(fname, path, tmp, flen);
  }

  if (*append == 0 && access(fname, F_OK) == 0)
  {
    switch (mutt_multi_choice(
        _("File exists, (o)verwrite, (a)ppend, or (c)ancel?"), _("oac")))
    {
      case -1: /* abort */
        return -1;
      case 3: /* cancel */
        return 1;

      case 2: /* append */
        *append = MUTT_SAVE_APPEND;
        break;
      case 1: /* overwrite */
        *append = MUTT_SAVE_OVERWRITE;
        break;
    }
  }
  return 0;
}

void mutt_save_path(char *d, size_t dsize, ADDRESS *a)
{
  if (a && a->mailbox)
  {
    strfcpy(d, a->mailbox, dsize);
    if (!option(OPTSAVEADDRESS))
    {
      char *p = NULL;

      if ((p = strpbrk(d, "%@")))
        *p = 0;
    }
    mutt_strlower(d);
  }
  else
    *d = 0;
}

void mutt_safe_path(char *s, size_t l, ADDRESS *a)
{
  char *p = NULL;

  mutt_save_path(s, l, a);
  for (p = s; *p; p++)
    if (*p == '/' || ISSPACE(*p) || !IsPrint((unsigned char) *p))
      *p = '_';
}

/* Note this function uses a fixed size buffer of LONG_STRING and so
 * should only be used for visual modifications, such as disp_subj. */
char *mutt_apply_replace(char *dbuf, size_t dlen, char *sbuf, REPLACE_LIST *rlist)
{
  REPLACE_LIST *l = NULL;
  static regmatch_t *pmatch = NULL;
  static int nmatch = 0;
  static char twinbuf[2][LONG_STRING];
  int switcher = 0;
  char *p = NULL;
  int i, n;
  size_t cpysize, tlen;
  char *src = NULL, *dst = NULL;

  if (dbuf && dlen)
    dbuf[0] = '\0';

  if (sbuf == NULL || *sbuf == '\0' || (dbuf && !dlen))
    return dbuf;

  twinbuf[0][0] = '\0';
  twinbuf[1][0] = '\0';
  src = twinbuf[switcher];
  dst = src;

  strfcpy(src, sbuf, LONG_STRING);

  for (l = rlist; l; l = l->next)
  {
    /* If this pattern needs more matches, expand pmatch. */
    if (l->nmatch > nmatch)
    {
      safe_realloc(&pmatch, l->nmatch * sizeof(regmatch_t));
      nmatch = l->nmatch;
    }

    if (regexec(l->rx->rx, src, l->nmatch, pmatch, 0) == 0)
    {
      tlen = 0;
      switcher ^= 1;
      dst = twinbuf[switcher];

      mutt_debug(5, "mutt_apply_replace: %s matches %s\n", src, l->rx->pattern);

      /* Copy into other twinbuf with substitutions */
      if (l->template)
      {
        for (p = l->template; *p && (tlen < LONG_STRING - 1);)
        {
          if (*p == '%')
          {
            p++;
            if (*p == 'L')
            {
              p++;
              cpysize = MIN(pmatch[0].rm_so, LONG_STRING - tlen - 1);
              strncpy(&dst[tlen], src, cpysize);
              tlen += cpysize;
            }
            else if (*p == 'R')
            {
              p++;
              cpysize = MIN(strlen(src) - pmatch[0].rm_eo, LONG_STRING - tlen - 1);
              strncpy(&dst[tlen], &src[pmatch[0].rm_eo], cpysize);
              tlen += cpysize;
            }
            else
            {
              n = strtoul(p, &p, 10);             /* get subst number */
              while (isdigit((unsigned char) *p)) /* skip subst token */
                ++p;
              for (i = pmatch[n].rm_so;
                   (i < pmatch[n].rm_eo) && (tlen < LONG_STRING - 1); i++)
                dst[tlen++] = src[i];
            }
          }
          else
            dst[tlen++] = *p++;
        }
      }
      dst[tlen] = '\0';
      mutt_debug(5, "mutt_apply_replace: subst %s\n", dst);
    }
    src = dst;
  }

  if (dbuf)
    strfcpy(dbuf, dst, dlen);
  else
    dbuf = safe_strdup(dst);
  return dbuf;
}


void mutt_FormatString(char *dest,     /* output buffer */
                       size_t destlen, /* output buffer len */
                       size_t col, /* starting column (nonzero when called recursively) */
                       int cols,           /* maximum columns */
                       const char *src,    /* template string */
                       format_t *callback, /* callback for processing */
                       unsigned long data, /* callback data */
                       format_flag flags)  /* callback flags */
{
  char prefix[SHORT_STRING], buf[LONG_STRING], *cp, *wptr = dest, ch;
  char ifstring[SHORT_STRING], elsestring[SHORT_STRING];
  size_t wlen, count, len, wid;
  pid_t pid;
  FILE *filter = NULL;
  int n;
  char *recycler = NULL;

  prefix[0] = '\0';
  destlen--; /* save room for the terminal \0 */
  wlen = ((flags & MUTT_FORMAT_ARROWCURSOR) && option(OPTARROWCURSOR)) ? 3 : 0;
  col += wlen;

  if ((flags & MUTT_FORMAT_NOFILTER) == 0)
  {
    int off = -1;

    /* Do not consider filters if no pipe at end */
    n = mutt_strlen(src);
    if (n > 1 && src[n - 1] == '|')
    {
      /* Scan backwards for backslashes */
      off = n;
      while (off > 0 && src[off - 2] == '\\')
        off--;
    }

    /* If number of backslashes is even, the pipe is real. */
    /* n-off is the number of backslashes. */
    if (off > 0 && ((n - off) % 2) == 0)
    {
      BUFFER *srcbuf = NULL, *word = NULL, *command = NULL;
      char srccopy[LONG_STRING];
#ifdef DEBUG
      int i = 0;
#endif

      mutt_debug(3, "fmtpipe = %s\n", src);

      strncpy(srccopy, src, n);
      srccopy[n - 1] = '\0';

      /* prepare BUFFERs */
      srcbuf = mutt_buffer_from(srccopy);
      srcbuf->dptr = srcbuf->data;
      word = mutt_buffer_new();
      command = mutt_buffer_new();

      /* Iterate expansions across successive arguments */
      do
      {
        char *p = NULL;

        /* Extract the command name and copy to command line */
        mutt_debug(3, "fmtpipe +++: %s\n", srcbuf->dptr);
        if (word->data)
          *word->data = '\0';
        mutt_extract_token(word, srcbuf, 0);
        mutt_debug(3, "fmtpipe %2d: %s\n", i++, word->data);
        mutt_buffer_addch(command, '\'');
        mutt_FormatString(buf, sizeof(buf), 0, cols, word->data, callback, data,
                          flags | MUTT_FORMAT_NOFILTER);
        for (p = buf; p && *p; p++)
        {
          if (*p == '\'')
            /* shell quoting doesn't permit escaping a single quote within
             * single-quoted material.  double-quoting instead will lead
             * shell variable expansions, so break out of the single-quoted
             * span, insert a double-quoted single quote, and resume. */
            mutt_buffer_addstr(command, "'\"'\"'");
          else
            mutt_buffer_addch(command, *p);
        }
        mutt_buffer_addch(command, '\'');
        mutt_buffer_addch(command, ' ');
      } while (MoreArgs(srcbuf));

      mutt_debug(3, "fmtpipe > %s\n", command->data);

      col -= wlen; /* reset to passed in value */
      wptr = dest; /* reset write ptr */
      wlen = ((flags & MUTT_FORMAT_ARROWCURSOR) && option(OPTARROWCURSOR)) ? 3 : 0;
      if ((pid = mutt_create_filter(command->data, NULL, &filter, NULL)) != -1)
      {
        int rc;

        n = fread(dest, 1, destlen /* already decremented */, filter);
        safe_fclose(&filter);
        rc = mutt_wait_filter(pid);
        if (rc != 0)
          mutt_debug(1, "format pipe command exited code %d\n", rc);
        if (n > 0)
        {
          dest[n] = 0;
          while ((n > 0) && (dest[n - 1] == '\n' || dest[n - 1] == '\r'))
            dest[--n] = '\0';
          mutt_debug(3, "fmtpipe < %s\n", dest);

          /* If the result ends with '%', this indicates that the filter
           * generated %-tokens that mutt can expand.  Eliminate the '%'
           * marker and recycle the string through mutt_FormatString().
           * To literally end with "%", use "%%". */
          if ((n > 0) && dest[n - 1] == '%')
          {
            --n;
            dest[n] = '\0'; /* remove '%' */
            if ((n > 0) && dest[n - 1] != '%')
            {
              recycler = safe_strdup(dest);
              if (recycler)
              {
                /* destlen is decremented at the start of this function
                 * to save space for the terminal nul char.  We can add
                 * it back for the recursive call since the expansion of
                 * format pipes does not try to append a nul itself.
                 */
                mutt_FormatString(dest, destlen + 1, col, cols, recycler,
                                  callback, data, flags);
                FREE(&recycler);
              }
            }
          }
        }
        else
        {
          /* read error */
          mutt_debug(1, "error reading from fmtpipe: %s (errno=%d)\n",
                     strerror(errno), errno);
          *wptr = 0;
        }
      }
      else
      {
        /* Filter failed; erase write buffer */
        *wptr = '\0';
      }

      mutt_buffer_free(&command);
      mutt_buffer_free(&srcbuf);
      mutt_buffer_free(&word);
      return;
    }
  }

  while (*src && wlen < destlen)
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
        for (; *p && *p != '?'; p++)
          ;
        /* nothing */
        if (*p == '?')
        {
          p++;
        }
        for (; *p && *p != '?'; p++)
          ;
        /* nothing */
        if (*p == '?')
        {
          *p = '>';
        }
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
        *cp = 0;
      }
      else
      {
        flags &= ~MUTT_FORMAT_OPTIONAL;

        /* eat the format string */
        cp = prefix;
        count = 0;
        while (count < sizeof(prefix) && (isdigit((unsigned char) *src) ||
                                          *src == '.' || *src == '-' || *src == '='))
        {
          *cp++ = *src++;
          count++;
        }
        *cp = 0;

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

        /* eat the `if' part of the string */
        cp = ifstring;
        count = 0;
        lrbalance = 1;
        while ((lrbalance > 0) && (count < sizeof(ifstring)) && *src)
        {
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
        *cp = 0;

        /* eat the `else' part of the string (optional) */
        if (*src == '&')
          src++; /* skip the & */
        cp = elsestring;
        count = 0;
        while ((lrbalance > 0) && (count < sizeof(elsestring)) && *src)
        {
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
        *cp = 0;

        if (!*src)
          break; /* bad format */

        src++; /* move past the trailing `>' (formerly '?') */
      }

      /* handle generic cases first */
      if (ch == '>' || ch == '*')
      {
        /* %>X: right justify to EOL, left takes precedence
         * %*X: right justify to EOL, right takes precedence */
        int soft = ch == '*';
        int pl, pw;
        if ((pl = mutt_charlen(src, &pw)) <= 0)
          pl = pw = 1;

        /* see if there's room to add content, else ignore */
        if ((col < cols && wlen < destlen) || soft)
        {
          int pad;

          /* get contents after padding */
          mutt_FormatString(buf, sizeof(buf), 0, cols, src + pl, callback, data, flags);
          len = mutt_strlen(buf);
          wid = mutt_strwidth(buf);

          pad = (cols - col - wid) / pw;
          if (pad >= 0)
          {
            /* try to consume as many columns as we can, if we don't have
             * memory for that, use as much memory as possible */
            if (wlen + (pad * pl) + len > destlen)
              pad = (destlen > wlen + len) ? ((destlen - wlen - len) / pl) : 0;
            else
            {
              /* Add pre-spacing to make multi-column pad characters and
               * the contents after padding line up */
              while ((col + (pad * pw) + wid < cols) && (wlen + (pad * pl) + len < destlen))
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
          else if (soft && pad < 0)
          {
            int offset =
                ((flags & MUTT_FORMAT_ARROWCURSOR) && option(OPTARROWCURSOR)) ? 3 : 0;
            int avail_cols = (cols > offset) ? (cols - offset) : 0;
            /* \0-terminate dest for length computation in mutt_wstr_trunc() */
            *wptr = 0;
            /* make sure right part is at most as wide as display */
            len = mutt_wstr_trunc(buf, destlen, avail_cols, &wid);
            /* truncate left so that right part fits completely in */
            wlen = mutt_wstr_trunc(dest, destlen - len, avail_cols - wid, &col);
            wptr = dest + wlen;
            /* Multi-column characters may be truncated in the middle.
             * Add spacing so the right hand side lines up. */
            while ((col + wid < avail_cols) && (wlen + len < destlen))
            {
              *wptr++ = ' ';
              wlen++;
              col++;
            }
          }
          if (len + wlen > destlen)
            len = mutt_wstr_trunc(buf, destlen - wlen, cols - col, NULL);
          memcpy(wptr, buf, len);
          wptr += len;
          wlen += len;
          col += wid;
          src += pl;
        }
        break; /* skip rest of input */
      }
      else if (ch == '|')
      {
        /* pad to EOL */
        int pl, pw, c;
        if ((pl = mutt_charlen(src, &pw)) <= 0)
          pl = pw = 1;

        /* see if there's room to add content, else ignore */
        if (col < cols && wlen < destlen)
        {
          c = (cols - col) / pw;
          if (c > 0 && wlen + (c * pl) > destlen)
            c = ((signed) (destlen - wlen)) / pl;
          while (c > 0)
          {
            memcpy(wptr, src, pl);
            wptr += pl;
            wlen += pl;
            col += pw;
            c--;
          }
          src += pl;
        }
        break; /* skip rest of input */
      }
      else
      {
        short tolower = 0;
        short nodots = 0;

        while (ch == '_' || ch == ':')
        {
          if (ch == '_')
            tolower = 1;
          else if (ch == ':')
            nodots = 1;

          ch = *src++;
        }

        /* use callback function to handle this case */
        src = callback(buf, sizeof(buf), col, cols, ch, src, prefix, ifstring,
                       elsestring, data, flags);

        if (tolower)
          mutt_strlower(buf);
        if (nodots)
        {
          char *p = buf;
          for (; *p; p++)
            if (*p == '.')
              *p = '_';
        }

        if ((len = mutt_strlen(buf)) + wlen > destlen)
          len = mutt_wstr_trunc(buf, destlen - wlen, cols - col, NULL);

        memcpy(wptr, buf, len);
        wptr += len;
        wlen += len;
        col += mutt_strwidth(buf);
      }
    }
    else if (*src == '\\')
    {
      if (!*++src)
        break;
      switch (*src)
      {
        case 'n':
          *wptr = '\n';
          break;
        case 't':
          *wptr = '\t';
          break;
        case 'r':
          *wptr = '\r';
          break;
        case 'f':
          *wptr = '\f';
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
      int tmp, w;
      /* in case of error, simply copy byte */
      if ((tmp = mutt_charlen(src, &w)) < 0)
        tmp = w = 1;
      if (tmp > 0 && wlen + tmp < destlen)
      {
        memcpy(wptr, src, tmp);
        wptr += tmp;
        src += tmp;
        wlen += tmp;
        col += w;
      }
      else
      {
        src += destlen - wlen;
        wlen = destlen;
      }
    }
  }
  *wptr = 0;
}

/* This function allows the user to specify a command to read stdout from in
   place of a normal file.  If the last character in the string is a pipe (|),
   then we assume it is a command to run instead of a normal file. */
FILE *mutt_open_read(const char *path, pid_t *thepid)
{
  FILE *f = NULL;
  struct stat s;

  int len = mutt_strlen(path);

  if (path[len - 1] == '|')
  {
    /* read from a pipe */

    char *p = safe_strdup(path);

    p[len - 1] = 0;
    mutt_endwin(NULL);
    *thepid = mutt_create_filter(p, NULL, &f, NULL);
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
    f = fopen(path, "r");
    *thepid = -1;
  }
  return f;
}

/* returns 0 if OK to proceed, -1 to abort, 1 to retry */
int mutt_save_confirm(const char *s, struct stat *st)
{
  char tmp[_POSIX_PATH_MAX];
  int ret = 0;
  int rc;
  int magic = 0;

  magic = mx_get_magic(s);

#ifdef USE_POP
  if (magic == MUTT_POP)
  {
    mutt_error(_("Can't save message to POP mailbox."));
    return 1;
  }
#endif

  if (magic > 0 && !mx_access(s, W_OK))
  {
    if (option(OPTCONFIRMAPPEND))
    {
      snprintf(tmp, sizeof(tmp), _("Append messages to %s?"), s);
      if ((rc = mutt_yesorno(tmp, MUTT_YES)) == MUTT_NO)
        ret = 1;
      else if (rc == MUTT_ABORT)
        ret = -1;
    }
  }

#ifdef USE_NNTP
  if (magic == MUTT_NNTP)
  {
    mutt_error(_("Can't save message to news server."));
    return 0;
  }
#endif

  if (stat(s, st) != -1)
  {
    if (magic == -1)
    {
      mutt_error(_("%s is not a mailbox!"), s);
      return 1;
    }
  }
  else if (magic != MUTT_IMAP)
  {
    st->st_mtime = 0;
    st->st_atime = 0;

    if (errno == ENOENT)
    {
      if (option(OPTCONFIRMCREATE))
      {
        snprintf(tmp, sizeof(tmp), _("Create %s?"), s);
        if ((rc = mutt_yesorno(tmp, MUTT_YES)) == MUTT_NO)
          ret = 1;
        else if (rc == MUTT_ABORT)
          ret = -1;
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

void state_prefix_putc(char c, STATE *s)
{
  if (s->flags & MUTT_PENDINGPREFIX)
  {
    state_reset_prefix(s);
    if (s->prefix)
      state_puts(s->prefix, s);
  }

  state_putc(c, s);

  if (c == '\n')
    state_set_prefix(s);
}

int state_printf(STATE *s, const char *fmt, ...)
{
  int rv;
  va_list ap;

  va_start(ap, fmt);
  rv = vfprintf(s->fpout, fmt, ap);
  va_end(ap);

  return rv;
}

void state_mark_attach(STATE *s)
{
  if (!s || !s->fpout)
    return;
  if ((s->flags & MUTT_DISPLAY) && (mutt_strcmp(Pager, "builtin") == 0))
    state_puts(AttachmentMarker, s);
}

void state_attach_puts(const char *t, STATE *s)
{
  if (!t || !s || !s->fpout)
    return;

  if (*t != '\n')
    state_mark_attach(s);
  while (*t)
  {
    state_putc(*t, s);
    if (*t++ == '\n' && *t)
      if (*t != '\n')
        state_mark_attach(s);
  }
}

static int state_putwc(wchar_t wc, STATE *s)
{
  char mb[MB_LEN_MAX] = "";
  int rc;

  if ((rc = wcrtomb(mb, wc, NULL)) < 0)
    return rc;
  if (fputs(mb, s->fpout) == EOF)
    return -1;
  return 0;
}

int state_putws(const wchar_t *ws, STATE *s)
{
  const wchar_t *p = ws;

  while (p && *p != L'\0')
  {
    if (state_putwc(*p, s) < 0)
      return -1;
    p++;
  }
  return 0;
}

void mutt_sleep(short s)
{
  if (SleepTime > s)
    sleep(SleepTime);
  else if (s)
    sleep(s);
}

/* Decrease a file's modification time by 1 second */
time_t mutt_decrease_mtime(const char *f, struct stat *st)
{
  struct utimbuf utim;
  struct stat _st;
  time_t mtime;

  if (!st)
  {
    if (stat(f, &_st) == -1)
      return -1;
    st = &_st;
  }

  if ((mtime = st->st_mtime) == time(NULL))
  {
    mtime -= 1;
    utim.actime = mtime;
    utim.modtime = mtime;
    utime(f, &utim);
  }

  return mtime;
}

/* sets mtime of 'to' to mtime of 'from' */
void mutt_set_mtime(const char *from, const char *to)
{
  struct utimbuf utim;
  struct stat st;

  if (stat(from, &st) != -1)
  {
    utim.actime = st.st_mtime;
    utim.modtime = st.st_mtime;
    utime(to, &utim);
  }
}

/* set atime to current time, just as read() would do on !noatime.
 * Silently ignored if unsupported. */
void mutt_touch_atime(int f)
{
#ifdef HAVE_FUTIMENS
  struct timespec times[2] = {{0, UTIME_NOW}, {0, UTIME_OMIT}};
  futimens(f, times);
#endif
}

const char *mutt_make_version(void)
{
  static char vstring[STRING];
  snprintf(vstring, sizeof(vstring), "NeoMutt %s%s (%s)", PACKAGE_VERSION, GitVer, MUTT_VERSION);
  return vstring;
}

REGEXP *mutt_compile_regexp(const char *s, int flags)
{
  REGEXP *pp = safe_calloc(sizeof(REGEXP), 1);
  pp->pattern = safe_strdup(s);
  pp->rx = safe_calloc(sizeof(regex_t), 1);
  if (REGCOMP(pp->rx, NONULL(s), flags) != 0)
    mutt_free_regexp(&pp);

  return pp;
}

void mutt_free_regexp(REGEXP **pp)
{
  FREE(&(*pp)->pattern);
  regfree((*pp)->rx);
  FREE(&(*pp)->rx);
  FREE(pp); /* __FREE_CHECKED__ */
}

void mutt_free_rx_list(RX_LIST **list)
{
  RX_LIST *p = NULL;

  if (!list)
    return;
  while (*list)
  {
    p = *list;
    *list = (*list)->next;
    mutt_free_regexp(&p->rx);
    FREE(&p);
  }
}

void mutt_free_replace_list(REPLACE_LIST **list)
{
  REPLACE_LIST *p = NULL;

  if (!list)
    return;
  while (*list)
  {
    p = *list;
    *list = (*list)->next;
    mutt_free_regexp(&p->rx);
    FREE(&p->template);
    FREE(&p);
  }
}

bool mutt_match_rx_list(const char *s, RX_LIST *l)
{
  if (!s)
    return 0;

  for (; l; l = l->next)
  {
    if (regexec(l->rx->rx, s, (size_t) 0, (regmatch_t *) 0, (int) 0) == 0)
    {
      mutt_debug(5, "mutt_match_rx_list: %s matches %s\n", s, l->rx->pattern);
      return true;
    }
  }

  return false;
}

/* Match a string against the patterns defined by the 'spam' command and output
 * the expanded format into `text` when there is a match.  If textsize<=0, the
 * match is performed but the format is not expanded and no assumptions are made
 * about the value of `text` so it may be NULL.
 *
 * Returns true if the argument `s` matches a pattern in the spam list, otherwise
 * false. */
bool mutt_match_spam_list(const char *s, REPLACE_LIST *l, char *text, int textsize)
{
  static regmatch_t *pmatch = NULL;
  static int nmatch = 0;
  int tlen = 0;
  char *p = NULL;

  if (!s)
    return false;

  for (; l; l = l->next)
  {
    /* If this pattern needs more matches, expand pmatch. */
    if (l->nmatch > nmatch)
    {
      safe_realloc(&pmatch, l->nmatch * sizeof(regmatch_t));
      nmatch = l->nmatch;
    }

    /* Does this pattern match? */
    if (regexec(l->rx->rx, s, (size_t) l->nmatch, (regmatch_t *) pmatch, (int) 0) == 0)
    {
      mutt_debug(5, "mutt_match_spam_list: %s matches %s\n", s, l->rx->pattern);
      mutt_debug(5, "mutt_match_spam_list: %d subs\n", (int) l->rx->rx->re_nsub);

      /* Copy template into text, with substitutions. */
      for (p = l->template; *p && tlen < textsize - 1;)
      {
        /* backreference to pattern match substring, eg. %1, %2, etc) */
        if (*p == '%')
        {
          char *e = NULL; /* used as pointer to end of integer backreference in strtol() call */
          int n;

          ++p; /* skip over % char */
          n = strtol(p, &e, 10);
          /* Ensure that the integer conversion succeeded (e!=p) and bounds check.  The upper bound check
           * should not strictly be necessary since add_to_spam_list() finds the largest value, and
           * the static array above is always large enough based on that value. */
          if (e != p && n >= 0 && n <= l->nmatch && pmatch[n].rm_so != -1)
          {
            /* copy as much of the substring match as will fit in the output buffer, saving space for
             * the terminating nul char */
            int idx;
            for (idx = pmatch[n].rm_so;
                 (idx < pmatch[n].rm_eo) && (tlen < textsize - 1); ++idx)
              text[tlen++] = s[idx];
          }
          p = e; /* skip over the parsed integer */
        }
        else
        {
          text[tlen++] = *p++;
        }
      }
      /* tlen should always be less than textsize except when textsize<=0
       * because the bounds checks in the above code leave room for the
       * terminal nul char.   This should avoid returning an unterminated
       * string to the caller.  When textsize<=0 we make no assumption about
       * the validity of the text pointer. */
      if (tlen < textsize)
      {
        text[tlen] = '\0';
        mutt_debug(5, "mutt_match_spam_list: \"%s\"\n", text);
      }
      return true;
    }
  }

  return false;
}

void mutt_encode_path(char *dest, size_t dlen, const char *src)
{
  char *p = safe_strdup(src);
  int rc = mutt_convert_string(&p, Charset, "utf-8", 0);
  /* `src' may be NULL, such as when called from the pop3 driver. */
  strfcpy(dest, (rc == 0) ? NONULL(p) : NONULL(src), dlen);
  FREE(&p);
}

/*
 * Process an XDG environment variable or its fallback.
 *
 * Return 1 if an entry was found that actually exists on disk and 0 otherwise.
 */
int mutt_set_xdg_path(const XDGType type, char *buf, size_t bufsize)
{
  char *xdg_env = getenv(xdg_env_vars[type]);
  char *xdg = (xdg_env && *xdg_env) ? safe_strdup(xdg_env) :
                                      safe_strdup(xdg_defaults[type]);
  char *x = xdg; /* strsep() changes xdg, so free x instead later */
  char *token = NULL;
  int rc = 0;

  while ((token = strsep(&xdg, ":")))
  {
    if (snprintf(buf, bufsize, "%s/%s/neomuttrc-%s", token, PACKAGE, PACKAGE_VERSION) < 0)
      continue;
    mutt_expand_path(buf, bufsize);
    if (access(buf, F_OK) == 0)
    {
      rc = 1;
      break;
    }

    if (snprintf(buf, bufsize, "%s/%s/neomuttrc", token, PACKAGE) < 0)
      continue;
    mutt_expand_path(buf, bufsize);
    if (access(buf, F_OK) == 0)
    {
      rc = 1;
      break;
    }

    if (snprintf(buf, bufsize, "%s/%s/Muttrc-%s", token, PACKAGE, MUTT_VERSION) < 0)
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

void mutt_get_parent_path(char *output, char *path, size_t olen)
{
#ifdef USE_IMAP
  if (mx_is_imap(path))
    imap_get_parent_path(output, path, olen);
  else
#endif
#ifdef USE_NOTMUCH
      if (mx_is_notmuch(path))
    strfcpy(output, NONULL(Maildir), olen);
  else
#endif
  {
    strfcpy(output, path, olen);
    int n = mutt_strlen(output);

    /* Remove everything until the next slash */
    for (n--; ((n >= 0) && (output[n] != '/')); n--)
      ;

    if (n > 0)
      output[n] = '\0';
    else
    {
      output[0] = '/';
      output[1] = '\0';
    }
  }
}
