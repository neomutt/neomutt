
#include <string.h>

#include "mutt.h"
#include "iconv.h"
#include "lib.h"
#include "charset.h"

/*
 * One day, gettext will return strings in the appropriate
 * encoding. In the meantime, we use this code to handle
 * the conversion.
 */

struct gt_hash_elem
{
  const char *key;
  char *data;
  struct gt_hash_elem *next;
};

#define gt_hash_size 127

static char *get_charset (const char *header)
{
  /* FIXME: the comparison should at least be case-insensitive */
  const char f[] = "\nContent-Type: text/plain; charset=";
  char *charset, *i, *j;

  charset = 0;
  i = strstr (header, f);
  if (i)
  {
    i += sizeof (f)-1;
    for (j = i; *j >= 32; j++)
      ;
    charset = safe_malloc (j-i+1);
    memcpy (charset, i, j-i);
    charset[j-i] = '\0';
  }
  return charset;
}

char *mutt_gettext (const char *message)
{
  static struct gt_hash_elem **messages = 0;
  static char *po_header = 0;
  static char *po_charset = 0;
  static char *message_charset = 0;
  static char *outrepl = "?";
  static iconv_t cd = (iconv_t)-1;
  int change_cd = 0;
  char *t, *orig;
  char *header_msgid = "";

  /* gettext ("") doesn't work due to __builtin_constant_p optimisation */
  if ((t = gettext (header_msgid)) != po_header)
  {
    po_header = t;
    t = get_charset (po_header);
    if (t != po_charset &&
	(!t || !po_charset || strcmp (t, po_charset)))
    {
      free (po_charset);
      po_charset = t;
      change_cd = 1;
    }
    else
      free (t);
  }

  if (message_charset != Charset &&
      (!message_charset || !Charset || strcmp (message_charset, Charset)))
  {
    free (message_charset);
    if (Charset)
    {
      int n = strlen (Charset);
      message_charset = safe_malloc (n+1);
      memcpy (message_charset, Charset, n+1);
    }
    else
      message_charset = 0;
    outrepl = mutt_is_utf8 (message_charset) ? "\357\277\275" : "?";
    change_cd = 1;
  }

  if (change_cd)
  {
    if (cd != (iconv_t)-1)
      iconv_close (cd);
    if (message_charset)
      cd = iconv_open (message_charset, po_charset ? po_charset : "UTF-8");
    else
      cd = (iconv_t)-1;

    if (messages)
    {
      int i;
      struct gt_hash_elem *p, *pn;

      for (i = 0; i < gt_hash_size; i++)
	for (p = messages[i]; p; p = pn)
	{
	  pn = p->next;
	  free (p);
	}
      free (messages);
      messages = 0;
    }
  }

  orig = gettext (message);

  if (cd == (iconv_t)-1)
    return orig;
  else
  {
    struct gt_hash_elem *p;
    int hash;
    char *s, *t;
    int n, nn;
    const char *ib;
    char *ob;
    size_t ibl, obl;

    if (!messages)
    {
      messages = safe_malloc (gt_hash_size * sizeof (*messages));
      memset (messages, 0, gt_hash_size * sizeof (*messages));
    }
    hash = (long int)orig % gt_hash_size; /* not very clever */
    for (p = messages[hash]; p && p->key != orig; p = p->next)
      ;
    if (p)
      return p->data;

    n = strlen (orig);
    nn = n + 1;
    t = safe_malloc (nn);

    ib = orig, ibl = n;
    ob = t, obl = n;
    for (;;)
    {
      mutt_iconv (cd, &ib, &ibl, &ob, &obl, 0, outrepl);
      if (!ibl || obl > 256)
	break;
      s = t;
      safe_realloc ((void **)&t, nn += n);
      ob += t - s;
      obl += n;
    }
    *ob = '\0';
    n = strlen (t);
    s = safe_malloc (n+1);
    memcpy (s, t, n+1);
    free (t);

    p = safe_malloc (sizeof (struct gt_hash_elem));
    p->key = orig;
    p->data = s;
    p->next = messages[hash];
    messages[hash] = p;
    return s;
  }
}
