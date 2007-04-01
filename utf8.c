#if HAVE_CONFIG_H
# include "config.h"
#endif

#ifndef HAVE_WC_FUNCS

#include <errno.h>

#ifndef EILSEQ
#define EILSEQ EINVAL
#endif

int mutt_wctoutf8 (char *s, unsigned int c)
{
  if (c < (1 << 7)) 
  {
    if (s)
      *s++ = c;
    return 1;
  }
  else if (c < (1 << 11))
  {
    if (s)
    {
      *s++ = 0xc0 | (c >> 6);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 2;
  }
  else if (c < (1 << 16))
  {
    if (s)
    {
      *s++ = 0xe0 | (c >> 12);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 3;
  }
  else if (c < (1 << 21))
  {
    if (s)
    {
      *s++ = 0xf0 | (c >> 18);
      *s++ = 0x80 | ((c >> 12) & 0x3f);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 4;
  }
  else if (c < (1 << 26))
  {
    if (s)
    {
      *s++ = 0xf8 | (c >> 24);
      *s++ = 0x80 | ((c >> 18) & 0x3f);
      *s++ = 0x80 | ((c >> 12) & 0x3f);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 5;
  }
  else if (c < (1 << 31))
  {
    if (s)
    {
      *s++ = 0xfc | (c >> 30);
      *s++ = 0x80 | ((c >> 24) & 0x3f);
      *s++ = 0x80 | ((c >> 18) & 0x3f);
      *s++ = 0x80 | ((c >> 12) & 0x3f);
      *s++ = 0x80 | ((c >> 6) & 0x3f);
      *s++ = 0x80 | (c & 0x3f);
    }
    return 6;
  }
  errno = EILSEQ;
  return -1;
}

#endif /* !HAVE_WC_FUNCS */
