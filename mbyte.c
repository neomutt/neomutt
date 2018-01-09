/**
 * @file
 * Convert strings between multibyte and utf8 encodings
 *
 * @authors
 * Copyright (C) 2000 Edmund Grimley Evans <edmundo@rano.org>
 * Copyright (C) 2000 Takashi Takizawa <taki@luna.email.ne.jp>
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

#include "config.h"
#include <errno.h>
#include <libintl.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>
#include "mutt/mutt.h"
#include "mbyte.h"

#ifndef EILSEQ
#define EILSEQ EINVAL
#endif

bool Charset_is_utf8 = false;

void mutt_set_charset(char *charset)
{
  char buffer[STRING];

  mutt_cs_canonical_charset(buffer, sizeof(buffer), charset);

  if (mutt_cs_is_utf8(buffer))
  {
    Charset_is_utf8 = true;
    ReplacementChar = 0xfffd;
  }
  else
  {
    Charset_is_utf8 = false;
    ReplacementChar = '?';
  }

#if defined(HAVE_BIND_TEXTDOMAIN_CODESET) && defined(ENABLE_NLS)
  bind_textdomain_codeset(PACKAGE, buffer);
#endif
}

bool is_display_corrupting_utf8(wchar_t wc)
{
  if (wc == (wchar_t) 0x200f || /* bidi markers: #3827 */
      wc == (wchar_t) 0x200e || wc == (wchar_t) 0x00ad || /* soft hyphen: #3848 */
      wc == (wchar_t) 0xfeff ||  /* zero width no-break space */
      (wc >= (wchar_t) 0x2066 && /* misc directional markers */
       wc <= (wchar_t) 0x2069) ||
      (wc >= (wchar_t) 0x202a && /* misc directional markers: #3854 */
       wc <= (wchar_t) 0x202e))
    return true;
  else
    return false;
}

int mutt_filter_unprintable(char **s)
{
  struct Buffer *b = NULL;
  wchar_t wc;
  size_t k, k2;
  char scratch[MB_LEN_MAX + 1];
  char *p = *s;
  mbstate_t mbstate1, mbstate2;

  b = mutt_buffer_new();
  if (!b)
    return -1;
  memset(&mbstate1, 0, sizeof(mbstate1));
  memset(&mbstate2, 0, sizeof(mbstate2));
  for (; (k = mbrtowc(&wc, p, MB_LEN_MAX, &mbstate1)); p += k)
  {
    if (k == (size_t)(-1) || k == (size_t)(-2))
    {
      k = 1;
      memset(&mbstate1, 0, sizeof(mbstate1));
      wc = ReplacementChar;
    }
    if (!IsWPrint(wc))
      wc = '?';
    else if (Charset_is_utf8 && is_display_corrupting_utf8(wc))
      continue;
    k2 = wcrtomb(scratch, wc, &mbstate2);
    scratch[k2] = '\0';
    mutt_buffer_addstr(b, scratch);
  }
  FREE(s);
  *s = b->data ? b->data : mutt_mem_calloc(1, 1);
  FREE(&b);
  return 0;
}
