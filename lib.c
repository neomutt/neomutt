static char rcsid[]="$Id$";
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
#include "mutt_curses.h"
#include "mime.h"
#include "mailbox.h"
#include "mx.h"

#ifdef _PGPPATH
#include "pgp.h"
#endif

#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>

BODY *mutt_new_body (void)
{
  BODY *p = (BODY *) safe_calloc (1, sizeof (BODY));
    
  p->disposition = DISPATTACH;
  p->use_disp = 1;
  return (p);
}

BODY *mutt_dup_body (BODY *b)
{
  BODY *bn;

  bn = mutt_new_body();
  memcpy(bn, b, sizeof (BODY));
  return bn;
}

void mutt_free_body (BODY **p)
{
  BODY *a = *p, *b;

  while (a)
  {
    b = a;
    a = a->next; 

    if (b->parameter)
      mutt_free_parameter (&b->parameter);
    if (b->unlink && b->filename)
      unlink (b->filename);
    safe_free ((void **) &b->filename);
    safe_free ((void **) &b->content);
    safe_free ((void **) &b->xtype);
    safe_free ((void **) &b->subtype);
    safe_free ((void **) &b->description);
    safe_free ((void **) &b->form_name);

    if (b->hdr)
    {
      /* Don't free twice (b->hdr->content = b->parts) */
      b->hdr->content = NULL;
      mutt_free_header(&b->hdr);
    }

    if (b->parts)
      mutt_free_body (&b->parts);

    safe_free ((void **) &b);
  }

  *p = 0;
}

void mutt_free_parameter (PARAMETER **p)
{
  PARAMETER *t = *p;
  PARAMETER *o;

  while (t)
  {
    safe_free ((void **) &t->attribute);
    safe_free ((void **) &t->value);
    o = t;
    t = t->next;
    safe_free ((void **) &o);
  }
  *p = 0;
}

LIST *mutt_add_list (LIST *head, const char *data)
{
  LIST *tmp;

  for (tmp = head; tmp && tmp->next; tmp = tmp->next)
    ;
  if (tmp)
  {
    tmp->next = safe_malloc (sizeof (LIST));
    tmp = tmp->next;
  }
  else
    head = tmp = safe_malloc (sizeof (LIST));

  tmp->data = safe_strdup (data);
  tmp->next = NULL;
  return head;
}

void mutt_free_list (LIST **list)
{
  LIST *p;
  
  if (!list) return;
  while (*list)
  {
    p = *list;
    *list = (*list)->next;
    safe_free ((void **) &p->data);
    safe_free ((void **) &p);
  }
}

HEADER *mutt_dup_header(HEADER *h)
{
  HEADER *hnew;

  hnew = mutt_new_header();
  memcpy(hnew, h, sizeof (HEADER));
  return hnew;
}

void mutt_free_header (HEADER **h)
{
  mutt_free_envelope (&(*h)->env);
  mutt_free_body (&(*h)->content);
  safe_free ((void **) &(*h)->tree);
  safe_free ((void **) &(*h)->path);
  safe_free ((void **) h);
}

/* returns true if the header contained in "s" is in list "t" */
int mutt_matches_ignore (const char *s, LIST *t)
{
  for (; t; t = t->next)
  {
    if (!strncasecmp (s, t->data, strlen (t->data)) || *t->data == '*')
      return 1;
  }
  return 0;
}

/* prepend the path part of *path to *link */
void mutt_expand_link (char *newpath, const char *path, const char *link)
{
  const char *lb = NULL;
  size_t len;

  /* link is full path */
  if (*link == '/')
  {
    strfcpy (newpath, link, _POSIX_PATH_MAX);
    return;
  }

  if ((lb = strrchr (path, '/')) == NULL)
  {
    /* no path in link */
    strfcpy (newpath, link, _POSIX_PATH_MAX);
    return;
  }

  len = lb - path + 1;
  memcpy (newpath, path, len);
  strfcpy (newpath + len, link, _POSIX_PATH_MAX - len);
}

char *mutt_expand_path (char *s, size_t slen)
{
  char p[_POSIX_PATH_MAX] = "";
  char *q = NULL;

  if (*s == '~')
  {
    if (*(s + 1) == '/' || *(s + 1) == 0)
      snprintf (p, sizeof (p), "%s%s", NONULL(Homedir), s + 1);
    else
    {
      struct passwd *pw;

      q = strchr (s + 1, '/');
      if (q)
	*q = 0;
      if ((pw = getpwnam (s + 1)))
	snprintf (p, sizeof (p), "%s/%s", pw->pw_dir, q ? q + 1 : "");
      else
      {
	/* user not found! */
	if (q)
	  *q = '/';
	return (NULL);
      }
    }
  }
  else if (*s == '=' || *s == '+')
    snprintf (p, sizeof (p), "%s/%s", NONULL (Maildir), s + 1);
  else if (*s == '@')
  {
    /* elm compatibility, @ expands alias to user name */
    HEADER *h;
    ADDRESS *alias;

    alias = mutt_lookup_alias (s + 1);
    if (alias != NULL)
    {
      h = mutt_new_header();
      h->env = mutt_new_envelope();
      h->env->from = h->env->to = alias;
      mutt_default_save (p, sizeof (p), h);
      h->env->from = h->env->to = NULL;
      mutt_free_header (&h);
      /* Avoid infinite recursion if the resulting folder starts with '@' */
      if (*p != '@')
	mutt_expand_path (p, sizeof (p));
    }
  }
  else
  {
    if (*s == '>')
      q = Inbox;
    else if (*s == '<')
      q = Outbox;
    else if (*s == '!')
      q = Spoolfile;
    else if (*s == '-')
      q = LastFolder;
    else
      return s;

    if (!q)
      return s;
    snprintf (p, sizeof (p), "%s%s", q, s + 1);
  }
  if (*p)
    strfcpy (s, p, slen); /* replace the string with the expanded version. */
  return (s);
}

void *safe_calloc (size_t nmemb, size_t size)
{
  void *p;

  if (!nmemb || !size)
    return NULL;
  if (!(p = calloc (nmemb, size)))
  {
    mutt_error _("Out of memory!");
    sleep (1);
    mutt_exit (1);
  }
  return p;
}

void *safe_malloc (unsigned int siz)
{
  void *p;

  if (siz == 0)
    return 0;
  if ((p = (void *) malloc (siz)) == 0)
  {
    mutt_error _("Out of memory!");
    sleep (1);
    mutt_exit (1);
  }
  return (p);
}

void safe_realloc (void **p, size_t siz)
{
  void *r;

  if (siz == 0)
  {
    if (*p)
    {
      free (*p);
      *p = NULL;
    }
    return;
  }

  if (*p)
    r = (void *) realloc (*p, siz);
  else
  {
    /* realloc(NULL, nbytes) doesn't seem to work under SunOS 4.1.x */
    r = (void *) malloc (siz);
  }

  if (!r)
  {
    mutt_error _("Out of memory!");
    sleep (1);
    mutt_exit (1);
  }

  *p = r;
}

void safe_free (void **p)
{
  if (*p)
  {
    free (*p);
    *p = 0;
  }
}

char *safe_strdup (const char *s)
{
  char *p;
  size_t l;

  if (!s || !*s) return 0;
  l = strlen (s) + 1;
  p = (char *)safe_malloc (l);
  memcpy (p, s, l);
  return (p);
}

char *mutt_skip_whitespace (char *p)
{
  SKIPWS (p);
  return p;
}

int mutt_copy_bytes (FILE *in, FILE *out, size_t size)
{
  char buf[2048];
  size_t chunk;

  while (size > 0)
  {
    chunk = (size > sizeof (buf)) ? sizeof (buf) : size;
    if ((chunk = fread (buf, 1, chunk, in)) < 1)
      break;
    if (fwrite (buf, 1, chunk, out) != chunk)
    {
      dprint (1, (debugfile, "mutt_copy_bytes(): fwrite() returned short byte count\n"));
      return (-1);
    }
    size -= chunk;
  }

  return 0;
}

char *mutt_get_parameter (const char *s, PARAMETER *p)
{
  for (; p; p = p->next)
    if (strcasecmp (s, p->attribute) == 0)
      return (p->value);

  return NULL;
}

/* returns 1 if Mutt can't display this type of data, 0 otherwise */
int mutt_needs_mailcap (BODY *m)
{
  switch (m->type)
  {
    case TYPETEXT:

      if (!strcasecmp ("plain", m->subtype) ||
	  !strcasecmp ("rfc822-headers", m->subtype) ||
	  !strcasecmp ("enriched", m->subtype))
	return 0;
      break;



#ifdef _PGPPATH
    case TYPEAPPLICATION:
      if(mutt_is_application_pgp(m))
	return 0;
      break;
#endif /* _PGPPATH */


    case TYPEMULTIPART:
    case TYPEMESSAGE:

      return 0;
  }

  return 1;
}

int mutt_is_text_type (int t, char *s)
{
  if (t == TYPETEXT)
    return 1;

  if (t == TYPEMESSAGE)
  {
    if (!strcasecmp ("delivery-status", s))
      return 1;
  }



#ifdef _PGPPATH
  if (t == TYPEAPPLICATION)
  {
    if (!strcasecmp ("pgp-keys", s))
      return 1;
  }
#endif /* _PGPPATH */



  return 0;
}

void mutt_free_envelope (ENVELOPE **p)
{
  if (!*p) return;
  rfc822_free_address (&(*p)->return_path);
  rfc822_free_address (&(*p)->to);
  rfc822_free_address (&(*p)->cc);
  rfc822_free_address (&(*p)->bcc);
  rfc822_free_address (&(*p)->sender);
  rfc822_free_address (&(*p)->from);
  rfc822_free_address (&(*p)->reply_to);
  rfc822_free_address (&(*p)->mail_followup_to);
  safe_free ((void **) &(*p)->subject);
  safe_free ((void **) &(*p)->message_id);
  safe_free ((void **) &(*p)->date);
  mutt_free_list (&(*p)->references);
  mutt_free_list (&(*p)->userhdrs);
  safe_free ((void **) p);
}

void mutt_tabs_to_spaces (char *s)
{
  while (*s)
  {
    if (ISSPACE (*s))
      *s = ' ';
    s++;
  }
}

void mutt_mktemp (char *s)
{
  snprintf (s, _POSIX_PATH_MAX, "%s/mutt-%s-%d-%d", NONULL (Tempdir), NONULL(Hostname), (int) getpid (), Counter++);
  unlink (s);
}

/* convert all characters in the string to lowercase */
char *mutt_strlower (char *s)
{
  char *p = s;

  while (*p)
  {
    *p = tolower (*p);
    p++;
  }

  return (s);
}

/* strcmp() allowing NULL pointers */
int mutt_strcmp (const char *s1, const char *s2)
{
  if (s1 != NULL)
  {
    if (s2 != NULL)
      return strcmp (s1, s2);
    else
      return (1);
  }
  else
    return ((s2 == NULL) ? 0 : -1);
}

void mutt_free_alias (ALIAS **p)
{
  ALIAS *t;

  while (*p)
  {
    t = *p;
    *p = (*p)->next;
    safe_free ((void **) &t->name);
    rfc822_free_address (&t->addr);
    safe_free ((void **) &t);
  }
}

/* collapse the pathname using ~ or = when possible */
void mutt_pretty_mailbox (char *s)
{
  char *p = s, *q = s;
  size_t len;

  /* first attempt to collapse the pathname */
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

  if (strncmp (s, NONULL (Maildir), (len = strlen (NONULL (Maildir)))) == 0 && s[len] == '/')
  {
    *s++ = '=';
    strcpy (s, s + len);
  }
  else if (strncmp (s, NONULL(Homedir), (len = strlen (NONULL(Homedir)))) == 0 &&
	   s[len] == '/')
  {
    *s++ = '~';
    strcpy (s, s + len - 1);
  }
}

void mutt_unlink (const char *s)
{
  FILE *f;
  struct stat sb;
  char buf[2048];
  
  if (stat (s, &sb) == 0)
  {
    if ((f = fopen (s, "r+")))
    {
      unlink (s);
      memset (buf, 0, sizeof (buf));
      while (sb.st_size > 0)
      {
	fwrite (buf, 1, sizeof (buf), f);
	sb.st_size -= sizeof (buf);
      }
      fclose (f);
    }
  }
}

int mutt_copy_stream (FILE *fin, FILE *fout)
{
  size_t l;
  char buf[LONG_STRING];

  while ((l = fread (buf, 1, sizeof (buf), fin)) > 0)
  {
    if (fwrite (buf, 1, l, fout) != l)
      return (-1);
  }

  return 0;
}

void mutt_expand_file_fmt (char *dest, size_t destlen, const char *fmt, const char *src)
{
  char *_src = mutt_quote_filename(src);
  
  mutt_expand_fmt(dest, destlen, fmt, _src);
  safe_free((void **) &_src);
}

void mutt_expand_fmt (char *dest, size_t destlen, const char *fmt, const char *src)
{
  const char *p = fmt;
  const char *last = p;
  size_t len;
  size_t slen;
  int found = 0;

  slen = strlen (src);
  
  while ((p = strchr (p, '%')) != NULL)
  {
    if (p[1] == 's')
    {
      found++;

      len = (size_t) (p - last);
      if (len)
      {
	if (len > destlen - 1)
	  len = destlen - 1;

	memcpy (dest, last, len);
	dest += len;
	destlen -= len;

	if (destlen <= 0)
	{
	  *dest = 0;
	  break; /* no more space */
	}
      }

      strfcpy (dest, src, destlen);
      if (slen > destlen)
      {
	/* no more room */
	break;
      }
      dest += slen;
      destlen -= slen;

      p += 2;
      last = p;
    }
    else if (p[1] == '%')
      p++;

    p++;
  }

  if (found)
    strfcpy (dest, last, destlen);
  else
    snprintf (dest, destlen, "%s %s", fmt, src);
  
}

static int 
compare_stat (struct stat *osb, struct stat *nsb)
{
  if (osb->st_dev != nsb->st_dev || osb->st_ino != nsb->st_ino ||
      osb->st_rdev != nsb->st_rdev)
  {
    return -1;
  }

  return 0;
}

int safe_symlink(const char *oldpath, const char *newpath)
{
  struct stat osb, nsb;

  if(!oldpath || !newpath)
    return -1;
  
  if(unlink(newpath) == -1 && errno != ENOENT)
    return -1;
  
  if(symlink(oldpath, newpath) == -1)
    return -1;
  
  if(stat(oldpath, &osb) == -1 || stat(newpath, &nsb) == -1
     || compare_stat(&osb, &nsb) == -1)
  {
    unlink(newpath);
    return -1;
  }
  
  return 0;
}

int safe_open (const char *path, int flags)
{
  struct stat osb, nsb;
  int fd;

  if ((fd = open (path, flags, 0600)) < 0)
    return fd;

  /* make sure the file is not symlink */
  if (lstat (path, &osb) < 0 || fstat (fd, &nsb) < 0 ||
      compare_stat(&osb, &nsb) == -1)
  {
    dprint (1, (debugfile, "safe_open(): %s is a symlink!\n", path));
    close (fd);
    return (-1);
  }

  return (fd);
}

/* when opening files for writing, make sure the file doesn't already exist
 * to avoid race conditions.
 */
FILE *safe_fopen (const char *path, const char *mode)
{
  if (mode[0] == 'w')
  {
    int fd;
    int flags = O_CREAT | O_EXCL;

    if (mode[1] == '+')
      flags |= O_RDWR;
    else
      flags |= O_WRONLY;

    if ((fd = safe_open (path, flags)) < 0)
      return (NULL);

    return (fdopen (fd, mode));
  }
  else
    return (fopen (path, mode));
}

/* return 0 on success, -1 on error */
int mutt_check_overwrite (const char *attname, const char *path,
				char *fname, size_t flen, int flags) 
{
  char tmp[_POSIX_PATH_MAX];
  struct stat st;

  strfcpy (fname, path, flen);
  if (access (fname, F_OK) != 0)
    return 0;
  if (stat (fname, &st) != 0)
    return -1;
  if (S_ISDIR (st.st_mode))
  {
    if (mutt_yesorno (_("File is a directory, save under it?"), 1) != M_YES) 
      return (-1);
    if (!attname || !attname[0])
    {
      tmp[0] = 0;
      if (mutt_get_field (_("File under directory: "), tmp, sizeof (tmp),
				      M_FILE | M_CLEAR) != 0 || !tmp[0])
	return (-1);
      snprintf (fname, flen, "%s/%s", path, tmp);
    }
    else
      snprintf (fname, flen, "%s/%s", path, attname);
  }

  if (flags != M_SAVE_APPEND &&
      access (fname, F_OK) == 0 && 
      mutt_yesorno (_("File exists, overwrite?"), 0) != 1)
    return (-1);
  
  return 0;
}

void mutt_remove_trailing_ws (char *s)
{
  char *p;

  for (p = s + strlen (s) - 1 ; p >= s && ISSPACE (*p) ; p--)
    *p = 0;
}

void mutt_pretty_size (char *s, size_t len, long n)
{
  if (n == 0)
    strfcpy (s, "0K", len);
  else if (n < 103)
    strfcpy (s, "0.1K", len);
  else if (n < 10189) /* 0.1K - 9.9K */
    snprintf (s, len, "%3.1fK", n / 1024.0);
  else if (n < 1023949) /* 10K - 999K */
  {
    /* 51 is magic which causes 10189/10240 to be rounded up to 10 */
    snprintf (s, len, "%ldK", (n + 51) / 1024);
  }
  else if (n < 10433332) /* 1.0M - 9.9M */
    snprintf (s, len, "%3.1fM", n / 1048576.0);
  else /* 10M+ */
  {
    /* (10433332 + 52428) / 1048576 = 10 */
    snprintf (s, len, "%ldM", (n + 52428) / 1048576);
  }
}

void mutt_save_path (char *d, size_t dsize, ADDRESS *a)
{
  if (a && a->mailbox)
  {
    strfcpy (d, a->mailbox, dsize);
    if (!option (OPTSAVEADDRESS))
    {
      char *p;

      if ((p = strpbrk (d, "%@")))
	*p = 0;
    }
    mutt_strlower (d);
  }
  else
    *d = 0;
}

void mutt_safe_path (char *s, size_t l, ADDRESS *a)
{
  char *p;

  mutt_save_path (s, l, a);
  for (p = s; *p; p++)
    if (*p == '/' || ISSPACE (*p) || !IsPrint ((unsigned char) *p))
      *p = '_';
}

static char safe_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+@{}._-:%";

void mutt_sanitize_filename(char *f)
{
  if(!f) return;

  for(; *f; f++)
  {
    if(!strchr(safe_chars, *f))
      *f = '_';
  }
}

/* Read a line from ``fp'' into the dynamically allocated ``s'',
 * increasing ``s'' if necessary. The ending "\n" or "\r\n" is removed.
 * If a line ends with "\", this char and the linefeed is removed,
 * and the next line is read too.
 */
char *mutt_read_line (char *s, size_t *size, FILE *fp, int *line)
{
  size_t offset = 0;
  char *ch;

  if (!s)
  {
    s = safe_malloc (STRING);
    *size = STRING;
  }

  FOREVER
  {
    if (fgets (s + offset, *size - offset, fp) == NULL)
    {
      safe_free ((void **) &s);
      return NULL;
    }
    if ((ch = strchr (s + offset, '\n')) != NULL)
    {
      (*line)++;
      *ch = 0;
      if (ch > s && *(ch - 1) == '\r')
	*--ch = 0;
      if (ch == s || *(ch - 1) != '\\')
	return s;
      offset = ch - s - 1;
    }
    else
    {
      /* There wasn't room for the line -- increase ``s'' */
      offset = *size - 1; /* overwrite the terminating 0 */
      *size += STRING;
      safe_realloc ((void **) &s, *size);
    }
  }
}

char *
mutt_substrcpy (char *dest, const char *beg, const char *end, size_t destlen)
{
  size_t len;

  len = end - beg;
  if (len > destlen - 1)
    len = destlen - 1;
  memcpy (dest, beg, len);
  dest[len] = 0;
  return dest;
}

char *mutt_substrdup (const char *begin, const char *end)
{
  size_t len;
  char *p;

  len = end - begin;
  p = safe_malloc (len + 1);
  memcpy (p, begin, len);
  p[len] = 0;
  return p;
}

void mutt_FormatString (char *dest,		/* output buffer */
			size_t destlen,		/* output buffer len */
			const char *src,	/* template string */
			format_t *callback,	/* callback for processing */
			unsigned long data,	/* callback data */
			format_flag flags)	/* callback flags */
{
  char prefix[SHORT_STRING], buf[LONG_STRING], *cp, *wptr = dest, ch;
  char ifstring[SHORT_STRING], elsestring[SHORT_STRING];
  size_t wlen, count, len;

  destlen--; /* save room for the terminal \0 */
  wlen = (flags & M_FORMAT_ARROWCURSOR && option (OPTARROWCURSOR)) ? 3 : 0;
    
  while (*src && wlen < destlen)
  {
    if (*src == '%')
    {
      if (*++src == '%')
      {
	*wptr++ = '%';
	wlen++;
	src++;
	continue;
      }

      if (*src == '?')
      {
	flags |= M_FORMAT_OPTIONAL;
	src++;
      }
      else
      {
	flags &= ~M_FORMAT_OPTIONAL;

	/* eat the format string */
	cp = prefix;
	count = 0;
	while (count < sizeof (prefix) &&
	       (isdigit ((unsigned char) *src) || *src == '.' || *src == '-'))
	{
	  *cp++ = *src++;
	  count++;
	}
	*cp = 0;
      }

      if (!*src)
	break; /* bad format */

      ch = *src++; /* save the character to switch on */

      if (flags & M_FORMAT_OPTIONAL)
      {
        if (*src != '?')
          break; /* bad format */
        src++;

        /* eat the `if' part of the string */
        cp = ifstring;
	count = 0;
        while (count < sizeof (ifstring) && *src && *src != '?' && *src != '&')
	{
          *cp++ = *src++;
	  count++;
	}
        *cp = 0;

	/* eat the `else' part of the string (optional) */
	if (*src == '&')
	  src++; /* skip the & */
	cp = elsestring;
	count = 0;
	while (count < sizeof (elsestring) && *src && *src != '?')
	{
	  *cp++ = *src++;
	  count++;
	}
	*cp = 0;

	if (!*src)
	  break; /* bad format */

        src++; /* move past the trailing `?' */
      }

      /* handle generic cases first */
      if (ch == '>')
      {
	/* right justify to EOL */
	ch = *src++; /* pad char */
	/* calculate space left on line.  if we've already written more data
	   than will fit on the line, ignore the rest of the line */
	count = (COLS < destlen ? COLS : destlen);
	if (count > wlen)
	{
	  count -= wlen; /* how many chars left on this line */
	  mutt_FormatString (buf, sizeof (buf), src, callback, data, flags);
	  len = strlen (buf);
	  if (count > len)
	  {
	    count -= len; /* how many chars to pad */
	    memset (wptr, ch, count);
	    wptr += count;
	    wlen += count;
	  }
	  if (len + wlen > destlen)
	    len = destlen - wlen;
	  memcpy (wptr, buf, len);
	  wptr += len;
	  wlen += len;
	}
	break; /* skip rest of input */
      }
      else if (ch == '|')
      {
	/* pad to EOL */
	ch = *src++;
	if (destlen > COLS)
	  destlen = COLS;
	if (destlen > wlen)
	{
	  count = destlen - wlen;
	  memset (wptr, ch, count);
	  wptr += count;
	}
	break; /* skip rest of input */
      }
      else
      {
	/* use callback function to handle this case */
	src = callback (buf, sizeof (buf), ch, src, prefix, ifstring, elsestring, data, flags);

	if ((len = strlen (buf)) + wlen > destlen)
	  len = (destlen - wlen > 0) ? (destlen - wlen) : 0;
	memcpy (wptr, buf, len);
	wptr += len;
	wlen += len;
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
    }
    else
    {
      *wptr++ = *src++;
      wlen++;
    }
  }
  *wptr = 0;

  if (flags & M_FORMAT_MAKEPRINT)
  {
    /* Make sure that the string is printable by changing all non-printable
       chars to dots, or spaces for non-printable whitespace */
    for (cp = dest ; *cp ; cp++)
      if (!IsPrint (*cp) &&
	  !((flags & M_FORMAT_TREE) && (*cp <= M_TREE_MAX)))
	*cp = isspace ((unsigned char) *cp) ? ' ' : '.';
  }
}

/* This function allows the user to specify a command to read stdout from in
   place of a normal file.  If the last character in the string is a pipe (|),
   then we assume it is a commmand to run instead of a normal file. */
FILE *mutt_open_read (const char *path, pid_t *thepid)
{
  FILE *f;
  int len = strlen (path);

  if (path[len - 1] == '|')
  {
    /* read from a pipe */

    char *s = safe_strdup (path);

    s[len - 1] = 0;
    endwin ();
    *thepid = mutt_create_filter (s, NULL, &f, NULL);
    safe_free ((void **) &s);
  }
  else
  {
    f = fopen (path, "r");
    *thepid = -1;
  }
  return (f);
}

/* returns 1 if OK to proceed, 0 to abort */
int mutt_save_confirm (const char *s, struct stat *st)
{
  char tmp[_POSIX_PATH_MAX];
  int ret = 1;
  int magic = 0;

  magic = mx_get_magic (s);

  if (stat (s, st) != -1)
  {
    if (magic == -1)
    {
      mutt_error (_("%s is not a mailbox!"), s);
      return 0;
    }

    if (option (OPTCONFIRMAPPEND))
    {
      snprintf (tmp, sizeof (tmp), _("Append messages to %s?"), s);
      if (mutt_yesorno (tmp, 1) < 1)
	ret = 0;
    }
  }
  else
  {
    if (magic != M_IMAP)
    {
      st->st_mtime = 0;
      st->st_atime = 0;

      if (errno == ENOENT)
      {
	if (option (OPTCONFIRMCREATE))
	{
	  snprintf (tmp, sizeof (tmp), _("Create %s?"), s);
	  if (mutt_yesorno (tmp, 1) < 1)
	    ret = 0;
	}
      }
      else
      {
	mutt_perror (s);
	return 0;
      }
    }
  }

  CLEARLINE (LINES-1);
  return (ret);
}

/* prepare a file name to survive the shell's quoting rules.
 * From the Unix programming FAQ by way of Liviu.
 */

char *mutt_quote_filename(const char *f)
{
  char *d;
  size_t i,l;

  if(!f) return NULL;
  
  for(i = 0, l = 3; f[i]; i++, l++)
  {
    if(f[i] == '\'')
      l += 3;
  }
  
  d = safe_malloc(l);
  
  l = 0;
  d[l++] = '\'';
  
  for(i = 0; f[i]; i++)
  {
    if(f[i] == '\'')
    {
      d[l++] = '\'';
      d[l++] = '\\';
      d[l++] = '\'';
      d[l++] = '\'';
    }
    else
      d[l++] = f[i];
  }
  
  d[l++] = '\'';
  d[l]   = '\0';
  
  return d;
}

void state_prefix_putc(char c, STATE *s)
{
  if (s->flags & M_PENDINGPREFIX)
  {
    state_reset_prefix(s);
    if (s->prefix)
      state_puts(s->prefix, s);
  }

  state_putc(c, s);

  if(c == '\n')
    state_set_prefix(s);
}
