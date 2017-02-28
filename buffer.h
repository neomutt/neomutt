/*
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef BUFFER_INCLUDED
#define BUFFER_INCLUDED

#include <sys/types.h>

typedef struct
{
  char *data;	/* pointer to data */
  char *dptr;	/* current read/write position */
  size_t dsize;	/* length of data */
  int destroy;	/* destroy `data' when done? */
} BUFFER;

/* flags for mutt_extract_token() */
#define MUTT_TOKEN_EQUAL      1       /* treat '=' as a special */
#define MUTT_TOKEN_CONDENSE   (1<<1)  /* ^(char) to control chars (macros) */
#define MUTT_TOKEN_SPACE      (1<<2)  /* don't treat whitespace as a term */
#define MUTT_TOKEN_QUOTE      (1<<3)  /* don't interpret quotes */
#define MUTT_TOKEN_PATTERN    (1<<4)  /* !)|~ are terms (for patterns) */
#define MUTT_TOKEN_COMMENT    (1<<5)  /* don't reap comments */
#define MUTT_TOKEN_SEMICOLON  (1<<6)  /* don't treat ; as special */

BUFFER *mutt_buffer_new (void);
BUFFER * mutt_buffer_init (BUFFER *);
BUFFER * mutt_buffer_from (char *);
void mutt_buffer_free(BUFFER **);
int mutt_buffer_printf (BUFFER*, const char*, ...);
void mutt_buffer_addstr (BUFFER* buf, const char* s);
void mutt_buffer_addch (BUFFER* buf, char c);
int mutt_extract_token (BUFFER *, BUFFER *, int);

#endif

