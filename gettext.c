
#include <string.h>

#include "mutt.h"

#include "hash.h"

#include "iconv.h"
#include "lib.h"
#include "charset.h"

#ifdef ENABLE_NLS

/*
 * One day, gettext will return strings in the appropriate
 * encoding. In the meantime, we use this code to handle
 * the conversion.
 */

/*
 * Note: the routines in this file can be invoked before debugfile
 * has been opened.  When using dprint here, please check if
 * debugfile != NULL before.
 */

static HASH *Messages = NULL;
static char *PoHeader = NULL;
static char *PoCharset = NULL;
static char *MessageCharset = NULL;

struct msg
{
  char *key;
  char *data;
};

static char *get_charset (const char *header)
{
  /* FIXME: the comparison should at least be case-insensitive */
  const char f[] = "\nContent-Type: text/plain; charset=";
  char *s, *t;

  if (!header) return NULL;
  
  if ((s = strstr (header, f)))
  {
    s += sizeof (f)-1;
    for (t = s; *t >= 32; t++)
      ;
    return mutt_substrdup (s, t);
  }
  
  /* else */
  
  return NULL;
}

static void destroy_one_message (void *vp)
{
  struct msg *mp = (struct msg *) vp;
  
  safe_free ((void **) &mp->key);
  safe_free ((void **) &mp->data);
}

static void destroy_messages (void)
{
  if (Messages)
    hash_destroy (&Messages, destroy_one_message);
}

static void set_po_charset (void)
{
  char *t;
  char *empty = "";

  t = gettext (empty);

  if (mutt_strcmp (t, PoHeader))
  {
    mutt_str_replace (&PoHeader, t);
    safe_free ((void **) &PoCharset);
    PoCharset = get_charset (PoHeader);
    
    destroy_messages ();
  }
}

static void set_message_charset (void)
{
  if (mutt_strcmp (MessageCharset, Charset))
  {
    mutt_str_replace (&MessageCharset, Charset);
    destroy_messages ();
  }
}

    
char *mutt_gettext (const char *message)
{
  char *orig;
  struct msg *mp;
  
  set_po_charset ();
  set_message_charset ();

  if (!Messages && MessageCharset)
    Messages = hash_create (127);

  orig = gettext (message);

# ifdef DEBUG
  if (debugfile)
    dprint (3, (debugfile, "mutt_gettext (`%s'): original gettext returned `%s'\n",
		message, orig));
# endif
  
  if (!Messages)
    return orig;
  
  if ((mp = hash_find (Messages, orig)))
  {
# ifdef DEBUG
    if (debugfile)
      dprint (3, (debugfile, "mutt_gettext: cache hit - key = `%s', data = `%s'\n", orig, mp->data));
# endif
    return mp->data;
  }

  /* the message could not be found in the hash */
  mp = safe_malloc (sizeof (struct msg));
  mp->key = safe_strdup (orig);
  mp->data = safe_strdup (orig);
  mutt_convert_string (&mp->data, PoCharset ? PoCharset : "utf-8", MessageCharset);

# ifdef DEBUG
  if (debugfile)
    dprint (3, (debugfile, "mutt_gettext: conversion done - src = `%s', res = `%s'\n", 
		mp->key, mp->data));
# endif
  
  hash_insert (Messages, mp->key, mp, 0);
  
  return mp->data;
}

#endif /* ENABLE_NLS */
