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
#include "mutt_regex.h"
#include "keymap.h"
#include "mutt_menu.h"
#include "mapping.h"
#include "sort.h"
#include "pager.h"
#include "attach.h"



#ifdef _PGPPATH
#include "pgp.h"
#endif









#include <sys/stat.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define M_NOSHOW	0
#define M_SHOWFLAT	(1 << 0)
#define M_SHOWCOLOR	(1 << 1)
#define M_HIDE		(1 << 2)
#define M_SEARCH	(1 << 3)
#define M_TYPES		(1 << 4)
#define M_SHOW		(M_SHOWCOLOR | M_SHOWFLAT)

#define ISHEADER(x) ((x) == MT_COLOR_HEADER || (x) == MT_COLOR_HDEFAULT)

#define IsAttach(x) (x && (x)->bdy)
#define IsHeader(x) (x && (x)->hdr)

#define CHECK_MODE(x)	if (!(x)) \
			{ \
			  	mutt_flushinp (); \
				mutt_error ("Not available in this menu."); \
				break; \
			}

#define CHECK_READONLY	if (Context->readonly) \
			{ \
				mutt_flushinp (); \
				mutt_error _("Mailbox is read-only.");	\
				break; \
			}

#define CHECK_ATTACH if(option(OPTATTACHMSG)) \
		     {\
			mutt_flushinp (); \
			mutt_error ("Function not permitted in attach-message mode."); \
			break; \
		     }

struct q_class_t
{
  int length;
  int index;
  int color;
  char *prefix;
  struct q_class_t *next, *prev;
  struct q_class_t *down, *up;
};

struct syntax_t
{
  int color;
  short first;
  short last;
};

struct line_t
{
  long offset;
  short type;
  short continuation;
  short chunks;
  short search_cnt;
  struct syntax_t *syntax;
  struct syntax_t *search;
  struct q_class_t *quote;
};

#define ANSI_OFF       (1<<0)
#define ANSI_BLINK     (1<<1)
#define ANSI_BOLD      (1<<2)
#define ANSI_UNDERLINE (1<<3)
#define ANSI_REVERSE   (1<<4)
#define ANSI_COLOR     (1<<5)

typedef struct _ansi_attr {
  int attr;
  int fg;
  int bg;
  int pair;
} ansi_attr;

static short InHelp = 0;

#define NumSigLines 4

static int check_sig (const char *s, struct line_t *info, int n)
{
  int count = 0;

  while (n > 0 && count <= NumSigLines)
  {
    if (info[n].type != MT_COLOR_SIGNATURE)
      break;
    count++;
    n--;
  }

  if (count == 0)
    return (-1);

  if (count > NumSigLines)
  {
    /* check for a blank line */
    while (*s)
    {
      if (!ISSPACE (*s))
	return 0;
      s++;
    }

    return (-1);
  }

  return (0);
}

static void
resolve_color (struct line_t *lineInfo, int n, int cnt, int flags, int special,
    ansi_attr *a)
{
  int def_color;		/* color without syntax hilight */
  int color;			/* final color */
  static int last_color;	/* last color set */
  int search = 0, i, m;

  if (!cnt)
    last_color = -1;		/* force attrset() */

  if (lineInfo[n].continuation)
  {
    if (!cnt && option (OPTMARKERS))
    {
      SETCOLOR (MT_COLOR_MARKERS);
      addch ('+');
      last_color = ColorDefs[MT_COLOR_MARKERS];
    }
    m = (lineInfo[n].syntax)[0].first;
    cnt += (lineInfo[n].syntax)[0].last;
  }
  else
    m = n;
  if (!(flags & M_SHOWCOLOR))
    def_color = ColorDefs[MT_COLOR_NORMAL];
  else if (lineInfo[m].type == MT_COLOR_HEADER)
    def_color = (lineInfo[m].syntax)[0].color;
  else
    def_color = ColorDefs[lineInfo[m].type];

  if ((flags & M_SHOWCOLOR) && lineInfo[m].type == MT_COLOR_QUOTED)
  {
    struct q_class_t *class = lineInfo[m].quote;

    if (class)
    {
      def_color = class->color;

      while (class && class->length > cnt)
      {
	def_color = class->color;
	class = class->up;
      }
    }
  }

  color = def_color;
  if (flags & M_SHOWCOLOR)
  {
    for (i = 0; i < lineInfo[m].chunks; i++)
    {
      /* we assume the chunks are sorted */
      if (cnt > (lineInfo[m].syntax)[i].last)
	continue;
      if (cnt < (lineInfo[m].syntax)[i].first)
	break;
      if (cnt != (lineInfo[m].syntax)[i].last)
      {
	color = (lineInfo[m].syntax)[i].color;
	break;
      }
      /* don't break here, as cnt might be 
       * in the next chunk as well */
    }
  }

  if (flags & M_SEARCH)
  {
    for (i = 0; i < lineInfo[m].search_cnt; i++)
    {
      if (cnt > (lineInfo[m].search)[i].last)
	continue;
      if (cnt < (lineInfo[m].search)[i].first)
	break;
      if (cnt != (lineInfo[m].search)[i].last)
      {
	color = ColorDefs[MT_COLOR_SEARCH];
	search = 1;
	break;
      }
    }
  }

  /* handle "special" bold & underlined characters */
  if (special || a->attr)
  {
    if (special == A_BOLD || (a->attr & ANSI_BOLD))
    {
      if (ColorDefs[MT_COLOR_BOLD] && !search)
	color = ColorDefs[MT_COLOR_BOLD];
      else
	color ^= A_BOLD;
    }
    else if (special == A_UNDERLINE || (a->attr & ANSI_UNDERLINE))
    {
      if (ColorDefs[MT_COLOR_UNDERLINE] && !search)
	color = ColorDefs[MT_COLOR_UNDERLINE];
      else
	color ^= A_UNDERLINE;
    }
    else if (a->attr & ANSI_REVERSE) 
    {
      color ^= A_REVERSE;
    }
    else if (a->attr & ANSI_BLINK) 
    {
      color ^= A_BLINK;
    }
#ifdef HAVE_COLOR
    else if (a->attr & ANSI_COLOR)
    {
      if (a->pair == -1)
	a->pair = mutt_alloc_color (a->fg,a->bg);
      color = a->pair;
    }
#endif
    else if (a->attr & ANSI_OFF)
    {
      a->attr = 0;
    }
  }

  if (color != last_color)
  {
    attrset (color);
    last_color = color;
  }
}

static void
append_line (struct line_t *lineInfo, int n, int cnt)
{
  int m;

  lineInfo[n+1].type = lineInfo[n].type;
  (lineInfo[n+1].syntax)[0].color = (lineInfo[n].syntax)[0].color;
  lineInfo[n+1].continuation = 1;

  /* find the real start of the line */
  m = n;
  while (m >= 0)
  {
    if (lineInfo[m].continuation == 0) break;
    m--;
  }
  (lineInfo[n+1].syntax)[0].first = m;
  (lineInfo[n+1].syntax)[0].last = (lineInfo[n].continuation) ? 
    cnt + (lineInfo[n].syntax)[0].last : cnt;
}

static void
new_class_color (struct q_class_t *class, int *q_level)
{
  class->index = (*q_level)++;
  class->color = ColorQuote[class->index % ColorQuoteUsed];
}

static void
shift_class_colors (struct q_class_t *QuoteList, struct q_class_t *new_class,
		      int index, int *q_level)
{
  struct q_class_t * q_list;

  q_list = QuoteList;
  new_class->index = -1;

  while (q_list)
  {
    if (q_list->index >= index)
    {
      q_list->index++;
      q_list->color = ColorQuote[q_list->index % ColorQuoteUsed];
    }
    if (q_list->down)
      q_list = q_list->down;
    else if (q_list->next)
      q_list = q_list->next;
    else
    {
      while (!q_list->next)
      {
	q_list = q_list->up;
	if (q_list == NULL)
	  break;
      }
      if (q_list)
	q_list = q_list->next;
    }
  }

  new_class->index = index;
  new_class->color = ColorQuote[index % ColorQuoteUsed];
  (*q_level)++;
}

static void
cleanup_quote (struct q_class_t **QuoteList)
{
  struct q_class_t *ptr;

  while (*QuoteList)
  {
    if ((*QuoteList)->down)
      cleanup_quote (&((*QuoteList)->down));
    ptr = (*QuoteList)->next;
    if ((*QuoteList)->prefix)
      safe_free ((void **) &(*QuoteList)->prefix);
    safe_free ((void **) QuoteList);
    *QuoteList = ptr;
  }

  return;
}

static struct q_class_t *
classify_quote (struct q_class_t **QuoteList, const char *qptr,
		int length, int *force_redraw, int *q_level)
{
  struct q_class_t *q_list = *QuoteList;
  struct q_class_t *class = NULL, *tmp = NULL, *ptr, *save;
  char *tail_qptr;
  int offset, tail_lng;
  int index = -1;

  if (ColorQuoteUsed <= 1)
  {
    /* not much point in classifying quotes... */

    if (*QuoteList == NULL)
    {
      class = (struct q_class_t *) safe_calloc (1, sizeof (struct q_class_t));
      class->color = ColorQuote[0];
      *QuoteList = class;
    }
    return (*QuoteList);
  }

  /* Did I mention how much I like emulating Lisp in C? */

  /* classify quoting prefix */
  while (q_list)
  {
    if (length <= q_list->length)
    {
      /* case 1: check the top level nodes */

      if (strncmp (qptr, q_list->prefix, length) == 0)
      {
	if (length == q_list->length)
	  return q_list;	/* same prefix: return the current class */

	/* found shorter prefix */
	if (tmp == NULL)
	{
	  /* add a node above q_list */
	  tmp = (struct q_class_t *) safe_calloc (1, sizeof (struct q_class_t));
	  tmp->prefix = (char *) safe_calloc (1, length + 1);
	  strncpy (tmp->prefix, qptr, length);
	  tmp->length = length;

	  /* replace q_list by tmp in the top level list */
	  if (q_list->next)
	  {
	    tmp->next = q_list->next;
	    q_list->next->prev = tmp;
	  }
	  if (q_list->prev)
	  {
	    tmp->prev = q_list->prev;
	    q_list->prev->next = tmp;
	  }

	  /* make q_list a child of tmp */
	  tmp->down = q_list;
	  q_list->up = tmp;

	  /* q_list has no siblings for now */
	  q_list->next = NULL;
	  q_list->prev = NULL;

	  /* update the root if necessary */
	  if (q_list == *QuoteList)
	    *QuoteList = tmp;

	  index = q_list->index;

	  /* tmp should be the return class too */
	  class = tmp;

	  /* next class to test; if tmp is a shorter prefix for another
	   * node, that node can only be in the top level list, so don't
	   * go down after this point
	   */
	  q_list = tmp->next;
	}
	else
	{
	  /* found another branch for which tmp is a shorter prefix */

	  /* save the next sibling for later */
	  save = q_list->next;

	  /* unlink q_list from the top level list */
	  if (q_list->next)
	    q_list->next->prev = q_list->prev;
	  if (q_list->prev)
	    q_list->prev->next = q_list->next;

	  /* at this point, we have a tmp->down; link q_list to it */
	  ptr = tmp->down;
	  /* sibling order is important here, q_list should be linked last */
	  while (ptr->next)
	    ptr = ptr->next;
	  ptr->next = q_list;
	  q_list->next = NULL;
	  q_list->prev = ptr;
	  q_list->up = tmp;

	  index = q_list->index;

	  /* next class to test; as above, we shouldn't go down */
	  q_list = save;
	}

	/* we found a shorter prefix, so certain quotes have changed classes */
	*force_redraw = 1;
	continue;
      }
      else
      {
	/* shorter, but not a substring of the current class: try next */
	q_list = q_list->next;
	continue;
      }
    }
    else
    {
      /* case 2: try subclassing the current top level node */
      
      /* tmp != NULL means we already found a shorter prefix at case 1 */
      if (tmp == NULL && strncmp (qptr, q_list->prefix, q_list->length) == 0)
      {
	/* ok, it's a subclass somewhere on this branch */

	ptr = q_list;
	offset = q_list->length;

	q_list = q_list->down;
	tail_lng = length - offset;
	tail_qptr = (char *) qptr + offset;

	while (q_list)
	{
	  if (length <= q_list->length)
	  {
	    if (strncmp (tail_qptr, (q_list->prefix) + offset, tail_lng) == 0)
	    {
	      /* same prefix: return the current class */
	      if (length == q_list->length)
		return q_list;

	      /* found shorter common prefix */
	      if (tmp == NULL)
	      {
		/* add a node above q_list */
		tmp = (struct q_class_t *) safe_calloc (1, 
					    sizeof (struct q_class_t));
		tmp->prefix = (char *) safe_calloc (1, length + 1);
		strncpy (tmp->prefix, qptr, length);
		tmp->length = length;
			
		/* replace q_list by tmp */
		if (q_list->next)
		{
		  tmp->next = q_list->next;
		  q_list->next->prev = tmp;
		}
		if (q_list->prev)
		{
		  tmp->prev = q_list->prev;
		  q_list->prev->next = tmp;
		}

		/* make q_list a child of tmp */
		tmp->down = q_list;
		tmp->up = q_list->up;
		q_list->up = tmp;
		if (tmp->up->down == q_list)
		  tmp->up->down = tmp;

		/* q_list has no siblings */
		q_list->next = NULL;
		q_list->prev = NULL;
                              
		index = q_list->index;

		/* tmp should be the return class too */
		class = tmp;

		/* next class to test */
		q_list = tmp->next;
	      }
	      else
	      {
		/* found another branch for which tmp is a shorter prefix */

		/* save the next sibling for later */
		save = q_list->next;

		/* unlink q_list from the top level list */
		if (q_list->next)
		  q_list->next->prev = q_list->prev;
		if (q_list->prev)
		  q_list->prev->next = q_list->next;

		/* at this point, we have a tmp->down; link q_list to it */
		ptr = tmp->down;
		while (ptr->next)
		  ptr = ptr->next;
		ptr->next = q_list;
		q_list->next = NULL;
		q_list->prev = ptr;
		q_list->up = tmp;

		index = q_list->index;

		/* next class to test */
		q_list = save;
	      }

	      /* we found a shorter prefix, so we need a redraw */
	      *force_redraw = 1;
	      continue;
	    }
	    else
	    {
	      q_list = q_list->next;
	      continue;
	    }
	  }
	  else
	  {
	    /* longer than the current prefix: try subclassing it */
	    if (tmp == NULL && strncmp (tail_qptr, (q_list->prefix) + offset,
			  q_list->length - offset) == 0)
	    {
	      /* still a subclass: go down one level */
	      ptr = q_list;
	      offset = q_list->length;

	      q_list = q_list->down;
	      tail_lng = length - offset;
	      tail_qptr = (char *) qptr + offset;

	      continue;
	    }
	    else
	    {
	      /* nope, try the next prefix */
	      q_list = q_list->next;
	      continue;
	    }
	  }
	}

	/* still not found so far: add it as a sibling to the current node */
	if (class == NULL)
	{
	  tmp = (struct q_class_t *) safe_calloc (1, sizeof (struct q_class_t));
	  tmp->prefix = (char *) safe_calloc (1, length + 1);
	  strncpy (tmp->prefix, qptr, length);
	  tmp->length = length;

	  if (ptr->down)
	  {
	    tmp->next = ptr->down;
	    ptr->down->prev = tmp;
	  }
	  ptr->down = tmp;
	  tmp->up = ptr;

	  new_class_color (tmp, q_level);

	  return tmp;
	}
	else
	{
	  if (index != -1)
	    shift_class_colors (*QuoteList, tmp, index, q_level);

	  return class;
	}
      }
      else
      {
	/* nope, try the next prefix */
	q_list = q_list->next;
	continue;
      }
    }
  }

  if (class == NULL)
  {
    /* not found so far: add it as a top level class */
    class = (struct q_class_t *) safe_calloc (1, sizeof (struct q_class_t));
    class->prefix = (char *) safe_calloc (1, length + 1);
    strncpy (class->prefix, qptr, length);
    class->length = length;
    new_class_color (class, q_level);

    if (*QuoteList)
    {
      class->next = *QuoteList;
      (*QuoteList)->prev = class;
    }
    *QuoteList = class;
  }

  if (index != -1)
    shift_class_colors (*QuoteList, tmp, index, q_level);

  return class;
}

static void
resolve_types (char *buf, struct line_t *lineInfo, int n, int last,
		struct q_class_t **QuoteList, int *q_level, int *force_redraw,
		int q_classify)
{
  COLOR_LINE *color_line;
  regmatch_t pmatch[1], smatch[1];
  int found, offset, null_rx, i;

  if (n == 0 || ISHEADER (lineInfo[n-1].type))
  {
    if (buf[0] == '\n')
      lineInfo[n].type = MT_COLOR_NORMAL;
    else if (n > 0 && (buf[0] == ' ' || buf[0] == '\t'))
    {
      lineInfo[n].type = lineInfo[n-1].type; /* wrapped line */
      (lineInfo[n].syntax)[0].color = (lineInfo[n-1].syntax)[0].color;
    }
    else
    {
      lineInfo[n].type = MT_COLOR_HDEFAULT;
      color_line = ColorHdrList;
      while (color_line)
      {
	if (REGEXEC (color_line->rx, buf) == 0)
	{
	  lineInfo[n].type = MT_COLOR_HEADER;
	  lineInfo[n].syntax[0].color = color_line->pair;
	  break;
	}
	color_line = color_line->next;
      }
    }
  }
  else if (strncmp ("[-- ", buf, 4) == 0)
    lineInfo[n].type = MT_COLOR_ATTACHMENT;
  else if (strcmp ("-- \n", buf) == 0 || strcmp ("-- \r\n", buf) == 0)
  {
    i = n + 1;

    lineInfo[n].type = MT_COLOR_SIGNATURE;
    while (i < last && check_sig (buf, lineInfo, i - 1) == 0 &&
	   (lineInfo[i].type == MT_COLOR_NORMAL ||
	    lineInfo[i].type == MT_COLOR_QUOTED ||
	    lineInfo[i].type == MT_COLOR_HEADER))
      {
	/* oops... */
	if (lineInfo[i].chunks)
	{
	  lineInfo[i].chunks = 0;
	  safe_realloc ((void **) &(lineInfo[n].syntax), 
			sizeof (struct syntax_t));
	}
	lineInfo[i++].type = MT_COLOR_SIGNATURE;
      }
  }
  else if (check_sig (buf, lineInfo, n - 1) == 0)
    lineInfo[n].type = MT_COLOR_SIGNATURE;
  else if (regexec ((regex_t *) QuoteRegexp.rx, buf, 1, pmatch, 0) == 0)
  {
    if (regexec ((regex_t *) Smileys.rx, buf, 1, smatch, 0) == 0)
    {
      if (smatch[0].rm_so > 0)
      {
	char c;

	/* hack to avoid making an extra copy of buf */
	c = buf[smatch[0].rm_so];
	buf[smatch[0].rm_so] = 0;

	if (regexec ((regex_t *) QuoteRegexp.rx, buf, 1, pmatch, 0) == 0)
	{
	  if (q_classify && lineInfo[n].quote == NULL)
	    lineInfo[n].quote = classify_quote (QuoteList,
				  buf + pmatch[0].rm_so,
				  pmatch[0].rm_eo - pmatch[0].rm_so,
				  force_redraw, q_level);
	  lineInfo[n].type = MT_COLOR_QUOTED;
	}
	else
	  lineInfo[n].type = MT_COLOR_NORMAL;

	buf[smatch[0].rm_so] = c;
      }
      else
	lineInfo[n].type = MT_COLOR_NORMAL;
    }
    else
    {
      if (q_classify && lineInfo[n].quote == NULL)
	lineInfo[n].quote = classify_quote (QuoteList, buf + pmatch[0].rm_so,
			      pmatch[0].rm_eo - pmatch[0].rm_so,
			      force_redraw, q_level);
      lineInfo[n].type = MT_COLOR_QUOTED;
    }
  }
  else
    lineInfo[n].type = MT_COLOR_NORMAL;

  /* body patterns */
  if (lineInfo[n].type == MT_COLOR_NORMAL || 
      lineInfo[n].type == MT_COLOR_QUOTED)
  {
    i = 0;

    offset = 0;
    lineInfo[n].chunks = 0;
    do
    {
      if (!buf[offset])
	break;

      found = 0;
      null_rx = 0;
      color_line = ColorBodyList;
      while (color_line)
      {
	if (regexec (&color_line->rx, buf + offset, 1, pmatch,
		     (offset ? REG_NOTBOL : 0)) == 0)
	{
	  if (pmatch[0].rm_eo != pmatch[0].rm_so)
	  {
	    if (!found)
	    {
	      if (++(lineInfo[n].chunks) > 1)
		safe_realloc ((void **)&(lineInfo[n].syntax), 
			      (lineInfo[n].chunks) * sizeof (struct syntax_t));
	    }
	    i = lineInfo[n].chunks - 1;
	    pmatch[0].rm_so += offset;
	    pmatch[0].rm_eo += offset;
	    if (!found ||
		pmatch[0].rm_so < (lineInfo[n].syntax)[i].first ||
		(pmatch[0].rm_so == (lineInfo[n].syntax)[i].first &&
		 pmatch[0].rm_eo > (lineInfo[n].syntax)[i].last))
	    {
	      (lineInfo[n].syntax)[i].color = color_line->pair;
	      (lineInfo[n].syntax)[i].first = pmatch[0].rm_so;
	      (lineInfo[n].syntax)[i].last = pmatch[0].rm_eo;
	    }
	    found = 1;
	    null_rx = 0;
	  }
	  else
	    null_rx = 1; /* empty regexp; don't add it, but keep looking */
	}
	color_line = color_line->next;
      }

      if (null_rx)
	offset++; /* avoid degenerate cases */
      else
	offset = (lineInfo[n].syntax)[i].last;
    } while (found || null_rx);
  }
}

static int
fill_buffer (FILE *f, long *last_pos, long offset, unsigned char *buf, 
	     unsigned char *fmt, size_t blen, int *buf_ready)
{
  unsigned char *p;
  static int b_read;

  if (*buf_ready == 0)
  {
    buf[blen - 1] = 0;
    if (offset != *last_pos)
      fseek (f, offset, 0);
    if (fgets ((char *) buf, blen - 1, f) == NULL)
    {
      fmt[0] = 0;
      return (-1);
    }
    *last_pos = ftell (f);
    b_read = (int) (*last_pos - offset);
    *buf_ready = 1;

    /* copy "buf" to "fmt", but without bold and underline controls */
    p = buf;
    while (*p)
    {
      if (*p == '\010' && (p > buf))
      {
	if (*(p+1) == '_')	/* underline */
	  p += 2;
	else if (*(p+1))	/* bold or overstrike */
	{
	  *(fmt-1) = *(p+1);
	  p += 2;
	}
	else			/* ^H */
	  *fmt++ = *p++;
      }
      else
	*fmt++ = *p++;
    }
    *fmt = 0;
  }
  return b_read;
}

static int grok_ansi(unsigned char *buf, int pos, ansi_attr *a)
{
  int x = pos;

  while (isdigit(buf[x]) || buf[x] == ';')
    x++;

  /* Character Attributes */
  if (a != NULL && buf[x] == 'm')
  {
    while (pos < x)
    {
      if (buf[pos] == '1' && (pos+1 == x || buf[pos+1] == ';'))
      {
	a->attr |= ANSI_BOLD;
	pos += 2;
      } 
      else if (buf[pos] == '4' && (pos+1 == x || buf[pos+1] == ';'))
      {
	a->attr |= ANSI_UNDERLINE;
	pos += 2;
      }
      else if (buf[pos] == '5' && (pos+1 == x || buf[pos+1] == ';'))
      {
	a->attr |= ANSI_BLINK;
	pos += 2;
      }
      else if (buf[pos] == '7' && (pos+1 == x || buf[pos+1] == ';'))
      {
	a->attr |= ANSI_REVERSE;
	pos += 2;
      }
      else if (buf[pos] == '0' && (pos+1 == x || buf[pos+1] == ';'))
      {
#ifdef HAVE_COLOR
	if (a->pair != -1)
	  mutt_free_color(a->fg,a->bg);
#endif
	a->attr = ANSI_OFF;
	a->pair = -1;
	pos += 2;
      }
      else if (buf[pos] == '3' && isdigit(buf[pos+1]))
      {
	a->attr |= ANSI_COLOR;
	a->fg = buf[pos+1] - '0';
	pos += 3;
      }
      else if (buf[pos] == '4' && isdigit(buf[pos+1]))
      {
	a->attr |= ANSI_COLOR;
	a->bg = buf[pos+1] - '0';
	pos += 3;
      }
      else 
      {
	while (pos < x && buf[pos] != ';') pos++;
	pos++;
      }
    }
  }
  pos = x;
  return pos;
}

/*
 * Args:
 *	flags	M_NOSHOW, don't show characters
 *		M_SHOWFLAT, show characters (used for displaying help)
 *		M_SHOWCOLOR, show characters in color
 *		M_HIDE, don't show quoted text
 *		M_SEARCH, resolve search patterns
 *		M_TYPES, compute line's type
 *
 * Return values:
 *	-1	EOF was reached
 *	0	normal exit, line was not displayed
 *	>0	normal exit, line was displayed
 */

static int
display_line (FILE *f, long *last_pos, struct line_t **lineInfo, int n, 
	      int *last, int *max, int flags, struct q_class_t **QuoteList,
	      int *q_level, int *force_redraw, regex_t *SearchRE)
{
  unsigned char buf[LONG_STRING], fmt[LONG_STRING];
  unsigned char *buf_ptr = buf, c;
  int ch, vch, t, col, cnt, b_read;
  int buf_ready = 0, change_last = 0;
  int special = 0, last_special = 0;
  int offset;
  int def_color;
  int m;
  ansi_attr a = {0,0,0,-1};
  regmatch_t pmatch[1];

  if (n == *last)
  {
    (*last)++;
    change_last = 1;
  }

  if (*last == *max)
  {
    safe_realloc ((void **)lineInfo, sizeof (struct line_t) * (*max += LINES));
    for (ch = *last; ch < *max ; ch++)
    {
      memset (&((*lineInfo)[ch]), 0, sizeof (struct line_t));
      (*lineInfo)[ch].type = -1;
      (*lineInfo)[ch].search_cnt = -1;
      (*lineInfo)[ch].syntax = safe_malloc (sizeof (struct syntax_t));
      ((*lineInfo)[ch].syntax)[0].first = ((*lineInfo)[ch].syntax)[0].last = -1;
    }
  }

  /* only do color hiliting if we are viewing a message */
  if (flags & (M_SHOWCOLOR | M_TYPES))
  {
    if ((*lineInfo)[n].type == -1)
    {
      /* determine the line class */
      if (fill_buffer (f, last_pos, (*lineInfo)[n].offset, buf, fmt, sizeof (buf), &buf_ready) < 0)
      {
	if (change_last)
	  (*last)--;
	return (-1);
      }

      resolve_types ((char *) fmt, *lineInfo, n, *last,
		      QuoteList, q_level, force_redraw, flags & M_SHOWCOLOR);
    }

    /* this also prevents searching through the hidden lines */
    if ((flags & M_HIDE) && (*lineInfo)[n].type == MT_COLOR_QUOTED)
      flags = M_NOSHOW;
  }

  /* At this point, (*lineInfo[n]).quote may still be undefined. We 
   * don't wont to compute it every time M_TYPES is set, since this
   * would slow down the "bottom" function unacceptably. A compromise
   * solution is hence to call regexec() again, just to find out the
   * length of the quote prefix.
   */
  if ((flags & M_SHOWCOLOR) && !(*lineInfo)[n].continuation &&
      (*lineInfo)[n].type == MT_COLOR_QUOTED && (*lineInfo)[n].quote == NULL)
  {
    if (fill_buffer (f, last_pos, (*lineInfo)[n].offset, buf, fmt, sizeof (buf), &buf_ready) < 0)
    {
      if (change_last)
	(*last)--;
      return (-1);
    }
    regexec ((regex_t *) QuoteRegexp.rx, (char *) fmt, 1, pmatch, 0);
    (*lineInfo)[n].quote = classify_quote (QuoteList,
			    (char *) fmt + pmatch[0].rm_so,
			    pmatch[0].rm_eo - pmatch[0].rm_so,
			    force_redraw, q_level);
  }

  if ((flags & M_SEARCH) && !(*lineInfo)[n].continuation && (*lineInfo)[n].search_cnt == -1) 
  {
    if (fill_buffer (f, last_pos, (*lineInfo)[n].offset, buf, fmt, sizeof (buf), &buf_ready) < 0)
    {
      if (change_last)
	(*last)--;
      return (-1);
    }

    offset = 0;
    (*lineInfo)[n].search_cnt = 0;
    while (regexec (SearchRE, (char *) fmt + offset, 1, pmatch, (offset ? REG_NOTBOL : 0)) == 0)
    {
      if (++((*lineInfo)[n].search_cnt) > 1)
	safe_realloc ((void **) &((*lineInfo)[n].search),
		      ((*lineInfo)[n].search_cnt) * sizeof (struct syntax_t));
      else
	(*lineInfo)[n].search = safe_malloc (sizeof (struct syntax_t));
      pmatch[0].rm_so += offset;
      pmatch[0].rm_eo += offset;
      ((*lineInfo)[n].search)[(*lineInfo)[n].search_cnt - 1].first = pmatch[0].rm_so;
      ((*lineInfo)[n].search)[(*lineInfo)[n].search_cnt - 1].last = pmatch[0].rm_eo;

      if (pmatch[0].rm_eo == pmatch[0].rm_so)
	offset++; /* avoid degenerate cases */
      else
	offset = pmatch[0].rm_eo;
      if (!fmt[offset])
	break;
    }
  }

  if (!(flags & M_SHOW) && (*lineInfo)[n+1].offset > 0)
  {
    /* we've already scanned this line, so just exit */
    return 0;
  }
  if ((flags & M_SHOWCOLOR) && *force_redraw && (*lineInfo)[n+1].offset > 0)
  {
    /* no need to try to display this line... */
    return 1; /* fake display */
  }

  if ((b_read = fill_buffer (f, last_pos, (*lineInfo)[n].offset, buf, fmt, 
			      sizeof (buf), &buf_ready)) < 0)
  {
    if (change_last)
      (*last)--;
    return (-1);
  }

  /* now chose a good place to break the line */

  ch = -1; /* index of the last space or TAB */
  cnt = 0;
  col = option (OPTMARKERS) ? (*lineInfo)[n].continuation : 0;
  while (col < COLS && cnt < b_read)
  {
    c = *buf_ptr++;
    if (c == '\n')
      break;

    while (*buf_ptr == '\010' && cnt + 2 < b_read)
    {
      cnt += 2;
      buf_ptr += 2;
      c = buf[cnt];
    }

    if (*buf_ptr == '\033' && *(buf_ptr + 1) && *(buf_ptr + 1) == '[')
    {
      cnt = grok_ansi(buf, cnt+3, NULL);
      cnt++;
      buf_ptr = buf + cnt;
      continue;
    }

    if (c == '\t')
    {
      ch = cnt;
      /* expand TABs */
      if ((t = (col & ~7) + 8) < COLS)
      {
	col = t;
	cnt++;
      }
      else
	break;
    }
    else if (IsPrint (c))
    {
      if (c == ' ')
	ch = cnt;
      col++;
      cnt++;
    }
    else if (iscntrl (c) && c < '@')
    {
      if (c == '\r' && *buf_ptr == '\n')
	cnt++;
      else if (col < COLS - 1)
      {
	col += 2;
	cnt++;
      }
      else
	break;
    }
    else
    {
      col++;
      cnt++;
    }
  }

  /* move the break point only if smart_wrap is set */
  if (option (OPTWRAP))
  {
    if (col == COLS)
    {
      if (ch != -1 && buf[cnt] != ' ' && buf[cnt] != '\t' && buf[cnt] != '\n' && buf[cnt] != '\r')
      {
	buf_ptr = buf + ch;
	/* skip trailing blanks */
	while (ch && (buf[ch] == ' ' || buf[ch] == '\t' || buf[ch] == '\r'))
	  ch--;
	cnt = ch + 1;
      }
      else
	buf_ptr = buf + cnt; /* a very long word... */
    }
    /* skip leading blanks on the next line too */
    while (*buf_ptr == ' ' || *buf_ptr == '\t') 
      buf_ptr++;
  }

  if (*buf_ptr == '\r')
    buf_ptr++;
  if (*buf_ptr == '\n')
    buf_ptr++;

  if ((int) (buf_ptr - buf) < b_read && !(*lineInfo)[n+1].continuation)
    append_line (*lineInfo, n, (int) (buf_ptr - buf));
  (*lineInfo)[n+1].offset = (*lineInfo)[n].offset + (long) (buf_ptr - buf);

  /* if we don't need to display the line we are done */
  if (!(flags & M_SHOW))
    return 0;

  if (flags & M_SHOWCOLOR)
  {
    m = ((*lineInfo)[n].continuation) ? ((*lineInfo)[n].syntax)[0].first : n;
    if ((*lineInfo)[m].type == MT_COLOR_HEADER)
      def_color = ((*lineInfo)[m].syntax)[0].color;
    else
      def_color = (*lineInfo)[m].type;

    attrset (def_color);
#ifdef HAVE_BKGDSET
    bkgdset (def_color | ' ');
#endif
    clrtoeol ();
    SETCOLOR (MT_COLOR_NORMAL);
    BKGDSET (MT_COLOR_NORMAL);
  }
  else
    clrtoeol ();
  
  /* display the line */
  col = option (OPTMARKERS) ? (*lineInfo)[n].continuation : 0;
  for (ch = 0, vch = 0; ch < cnt; ch++, vch++)
  {
    special = 0;

    c = buf[ch];
    while (buf[ch+1] == '\010' && ch+2 < b_read)
    {
      if (buf[ch+2] == c)
      {
	special = (c == '_' && last_special == A_UNDERLINE)
	  ? A_UNDERLINE : A_BOLD;
	ch += 2;
      }
      else if (buf[ch] == '_' || buf[ch+2] == '_')
      {
	special = A_UNDERLINE;
	ch += 2;
	c = (buf[ch] == '_') ? buf[ch-2] : buf[ch];
      }
      else
      {
	special = 0; /* overstrike: nothing to do! */
	ch += 2;
	c = buf[ch];
      }
      last_special = special;
    }

    /* Handle ANSI sequences */
    if (c == '\033' && buf[ch+1] == '[')
    {
      ch = grok_ansi(buf, ch+2, &a);
      c = buf[ch];
      continue;
    }

    if (c == '\t')
    {
      if ((flags & (M_SHOWCOLOR | M_SEARCH)) || last_special || a.attr)
      {
	resolve_color (*lineInfo, n, vch, flags, special, &a);
	if (!special)
	  last_special = 0;
      }

      t = (col & ~7) + 8;
      while (col < t)
      {
	addch (' ');
	col++;
      }
    }
    else if (IsPrint (c))
    {
      if ((flags & (M_SHOWCOLOR | M_SEARCH)) || special || last_special 
	  || a.attr)
	resolve_color (*lineInfo, n, vch, flags, special, &a);
      if (!special)
	last_special = 0;

      addch (c);
      col++;
    }
    else if (iscntrl (c) && c < '@')
    {
      if ((c != '\r' && c !='\n') || (buf[ch+1] != '\n' && buf[ch+1] != '\0'))
      {
	if ((flags & (M_SHOWCOLOR | M_SEARCH)) || last_special || a.attr)
	{
	  resolve_color (*lineInfo, n, vch, flags, special, &a);
	  if (!special)
	    last_special = 0;
	}

	addch ('^');
	addch (c + '@');
	col += 2;
      }
    }
    else
    {
      if ((flags & (M_SHOWCOLOR | M_SEARCH)) || last_special || a.attr)
      {
	resolve_color (*lineInfo, n, vch, flags, special, &a);
	if (!special)
	  last_special = 0;
      }

      if (ISSPACE (c))
	addch (c);		/* unbreakable space */
      else
	addch ('.');
      col++;
    }
  }

  /* avoid a bug in ncurses... */
#ifndef USE_SLANG_CURSES
  if (col == 0)
  {
    SETCOLOR (MT_COLOR_NORMAL);
    addch (' ');
  }
#endif

  /* end the last color pattern (needed by S-Lang) */
  if (last_special || (col != COLS && (flags & (M_SHOWCOLOR | M_SEARCH))))
    resolve_color (*lineInfo, n, vch, flags, 0, &a);
          
  /* ncurses always wraps lines when you get to the right side of the
   * screen, but S-Lang seems to only wrap if the next character is *not*
   * a newline (grr!).
   */
#ifndef USE_SLANG_CURSES
    if (col < COLS)
#endif
      addch ('\n');

  /* build a return code */
  if (!(flags & M_SHOW))
    flags = 0;

  return (flags);
}

static int
upNLines (int nlines, struct line_t *info, int cur, int hiding)
{
  while (cur > 0 && nlines > 0)
  {
    cur--;
    if (!hiding || info[cur].type != MT_COLOR_QUOTED)
      nlines--;
  }

  return cur;
}

static struct mapping_t PagerHelp[] = {
  { N_("Exit"),	OP_PAGER_EXIT },
  { N_("PrevPg"),	OP_PREV_PAGE },
  { N_("NextPg"), OP_NEXT_PAGE },
  { NULL,	0 }
};
static struct mapping_t PagerHelpExtra[] = {
  { N_("View Attachm."),	OP_VIEW_ATTACHMENTS },
  { N_("Del"),	OP_DELETE },
  { N_("Reply"),	OP_REPLY },
  { N_("Next"),	OP_MAIN_NEXT_UNDELETED },
  { NULL,	0 }
};



/* This pager is actually not so simple as it once was.  It now operates in
   two modes: one for viewing messages and the other for viewing help.  These
   can be distinguished by whether or not ``hdr'' is NULL.  The ``hdr'' arg
   is there so that we can do operations on the current message without the
   need to pop back out to the main-menu.  */
int 
mutt_pager (const char *banner, const char *fname, int do_color, pager_t *extra)
{
  static char searchbuf[STRING];
  char buffer[LONG_STRING];
  char helpstr[SHORT_STRING];
  int maxLine, lastLine = 0;
  struct line_t *lineInfo;
  struct q_class_t *QuoteList = NULL;
  int i, j, ch = 0, rc = -1, hideQuoted = 0, q_level = 0, force_redraw = 0;
  int lines = 0, curline = 0, topline = 0, oldtopline = 0, err, first = 1;
  int r = -1;
  int redraw = REDRAW_FULL;
  FILE *fp = NULL;
  long last_pos = 0, last_offset = 0;
  int old_smart_wrap, old_markers;
  struct stat sb;
  regex_t SearchRE;
  int SearchCompiled = 0, SearchFlag = 0, SearchBack = 0;
  int has_types = (IsHeader(extra) || do_color); /* main message or rfc822 attachment */

  int bodyoffset = 1;			/* offset of first line of real text */
  int statusoffset = 0; 		/* offset for the status bar */
  int helpoffset = LINES - 2;		/* offset for the help bar. */
  int bodylen = LINES - 2 - bodyoffset; /* length of displayable area */

  MUTTMENU *index = NULL;		/* the Pager Index (PI) */
  int indexoffset = 0;			/* offset for the PI */
  int indexlen = PagerIndexLines;	/* indexlen not always == PIL */
  int indicator = indexlen / 3; 	/* the indicator line of the PI */
  int old_PagerIndexLines;		/* some people want to resize it
  					 * while inside the pager... */

  do_color = do_color ? M_SHOWCOLOR : M_SHOWFLAT;

  if ((fp = fopen (fname, "r")) == NULL)
  {
    mutt_perror (fname);
    return (-1);
  }

  if (stat (fname, &sb) != 0)
  {
    mutt_perror (fname);
    fclose (fp);
    return (-1);
  }
  unlink (fname);

  /* Initialize variables */

  if (IsHeader (extra) && !extra->hdr->read)
  {
    Context->msgnotreadyet = extra->hdr->msgno;
    mutt_set_flag (Context, extra->hdr, M_READ, 1);
  }

  lineInfo = safe_malloc (sizeof (struct line_t) * (maxLine = LINES));
  for (i = 0 ; i < maxLine ; i++)
  {
    memset (&lineInfo[i], 0, sizeof (struct line_t));
    lineInfo[i].type = -1;
    lineInfo[i].search_cnt = -1;
    lineInfo[i].syntax = safe_malloc (sizeof (struct syntax_t));
    (lineInfo[i].syntax)[0].first = (lineInfo[i].syntax)[0].last = -1;
  }

  mutt_compile_help (helpstr, sizeof (helpstr), MENU_PAGER, PagerHelp);
  if (IsHeader (extra))
  {
    mutt_compile_help (buffer, sizeof (buffer), MENU_PAGER, PagerHelpExtra);
    strcat (helpstr, "  ");
    strcat (helpstr, buffer);
  }
  if (!InHelp)
  {
    mutt_make_help (buffer, sizeof (buffer), _("Help"), MENU_PAGER, OP_HELP);
    strcat (helpstr, "  ");
    strcat (helpstr, buffer);
  }

  while (ch != -1)
  {
    mutt_curs_set (0);

    if (redraw & REDRAW_FULL)
    {
      SETCOLOR (MT_COLOR_NORMAL);
      /* clear() doesn't optimize screen redraws */
      move (0, 0);
      clrtobot ();

      if (IsHeader (extra) && Context->vcount + 1 < PagerIndexLines)
	indexlen = Context->vcount + 1;
      else
	indexlen = PagerIndexLines;

      indicator = indexlen / 3;

      if (option (OPTSTATUSONTOP))
      {
	indexoffset = 0;
	statusoffset = IsHeader (extra) ? indexlen : 0;
	bodyoffset = statusoffset + 1;
	helpoffset = LINES - 2;
	bodylen = helpoffset - bodyoffset;
	if (!option (OPTHELP))
	  bodylen++;
      }
      else
      {
	helpoffset = 0;
	indexoffset = 1;
	statusoffset = LINES - 2;
	if (!option (OPTHELP))
	  indexoffset = 0;
	bodyoffset = indexoffset + (IsHeader (extra) ? indexlen : 0);
	bodylen = statusoffset - bodyoffset;
      }

      if (option (OPTHELP))
      {
	SETCOLOR (MT_COLOR_STATUS);
	mvprintw (helpoffset, 0, "%-*.*s", COLS, COLS, helpstr);
	SETCOLOR (MT_COLOR_NORMAL);
      }

      if (IsHeader (extra) && PagerIndexLines)
      {
	if (index == NULL)
	{
	  /* only allocate the space if/when we need the index.
	     Initialise the menu as per the main index */
	  index = mutt_new_menu();
	  index->menu = MENU_MAIN;
	  index->make_entry = index_make_entry;
	  index->color = index_color;
	  index->max = Context->vcount;
	  index->current = extra->hdr->virtual;
	}

	SETCOLOR (MT_COLOR_NORMAL);
	index->offset  = indexoffset + (option (OPTSTATUSONTOP) ? 1 : 0);

	index->pagelen = indexlen - 1;

	/* some fudge to work out where abouts the indicator should go */
	if (index->current - indicator < 0)
	  index->top = 0;
	else if (index->max - index->current < index->pagelen - indicator)
	  index->top = index->max - index->pagelen;
	else
	  index->top = index->current - indicator;

	menu_redraw_index(index);
      }

      redraw |= REDRAW_BODY | REDRAW_INDEX | REDRAW_STATUS;
      mutt_show_error ();
    }

    if ((redraw & REDRAW_BODY) || topline != oldtopline)
    {
      do {
	move (bodyoffset, 0);
	curline = oldtopline = topline;
	lines = 0;
	force_redraw = 0;

	while (lines < bodylen && lineInfo[curline].offset <= sb.st_size - 1)
	{
	  if (display_line (fp, &last_pos, &lineInfo, curline, &lastLine, 
			    &maxLine, do_color | hideQuoted | SearchFlag, 
			    &QuoteList, &q_level, &force_redraw, &SearchRE) > 0)
	    lines++;
	  curline++;
	}
	last_offset = lineInfo[curline].offset;
      } while (force_redraw);

      SETCOLOR (MT_COLOR_TILDE);
      BKGDSET (MT_COLOR_TILDE);
      while (lines < bodylen)
      {
	clrtoeol ();
	if (option (OPTTILDE))
	  addch ('~');
	addch ('\n');
	lines++;
      }
      /* We are going to update the pager status bar, so it isn't
       * necessary to reset to normal color now. */

      redraw |= REDRAW_STATUS; /* need to update the % seen */
    }

    if (redraw & REDRAW_STATUS)
    {
      /* print out the pager status bar */
      SETCOLOR (MT_COLOR_STATUS);
      BKGDSET (MT_COLOR_STATUS);
      CLEARLINE (statusoffset);
      if (IsHeader (extra))
      {
	_mutt_make_string (buffer,
			   COLS-9 < sizeof (buffer) ? COLS-9 : sizeof (buffer),
			   NONULL (PagerFmt), Context, extra->hdr, M_FORMAT_MAKEPRINT);
      }
      printw ("%-*.*s -- (", COLS-10, COLS-10, IsHeader (extra) ? buffer : banner);
      if (last_pos < sb.st_size - 1)
	printw ("%d%%)", (int) (100 * last_offset / sb.st_size));
      else
	addstr (topline == 0 ? "all)" : "end)");
      BKGDSET (MT_COLOR_NORMAL);
      SETCOLOR (MT_COLOR_NORMAL);
    }

    if ((redraw & REDRAW_INDEX) && index)
    {
      /* redraw the pager_index indicator, because the
       * flags for this message might have changed. */
      menu_redraw_current (index);

      /* print out the index status bar */
      menu_status_line (buffer, sizeof (buffer), index, NONULL(Status));
 
      move (indexoffset + (option (OPTSTATUSONTOP) ? 0 : (indexlen - 1)), 0);
      SETCOLOR (MT_COLOR_STATUS);
      printw ("%-*.*s", COLS, COLS, buffer);
      SETCOLOR (MT_COLOR_NORMAL);
    }

    redraw = 0;

    move (statusoffset, COLS-1);
    mutt_refresh ();
    ch = km_dokey (MENU_PAGER);
    mutt_clear_error ();
    mutt_curs_set (1);

    if (Signals & S_INTERRUPT)
    {
      mutt_query_exit ();
      continue;
    }
#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
    else if (Signals & S_SIGWINCH)
    {
      mutt_resize_screen ();

      for (i = 0; i < maxLine; i++)
      {
	lineInfo[i].offset = 0;
	lineInfo[i].type = -1;
	lineInfo[i].continuation = 0;
	lineInfo[i].chunks = 0;
	lineInfo[i].search_cnt = -1;
	lineInfo[i].quote = NULL;

	safe_realloc ((void **)&(lineInfo[i].syntax), sizeof (struct syntax_t));
	if (SearchCompiled && lineInfo[i].search)
	    safe_free ((void **) &(lineInfo[i].search));
      }

      if (SearchCompiled)
      {
	regfree (&SearchRE);
	SearchCompiled = 0;
	SearchFlag = 0;
      }

      lastLine = 0;
      topline = 0;

      redraw = REDRAW_FULL;
      Signals &= ~S_SIGWINCH;
      ch = 0;
      continue;
    }
#endif
    else if (ch == -1)
    {
      ch = 0;
      continue;
    }

    rc = ch;

    switch (ch)
    {
      case OP_PAGER_EXIT:
	rc = -1;
	ch = -1;
	break;

      case OP_NEXT_PAGE:
	if (lineInfo[curline].offset < sb.st_size-1)
	{
	  topline = upNLines (PagerContext, lineInfo, curline, hideQuoted);
	}
	else if (option (OPTPAGERSTOP))
	{
	  /* emulate "less -q" and don't go on to the next message. */
	  mutt_error _("Bottom of message is shown.");
	}
	else
	{
	  /* end of the current message, so display the next message. */
	  rc = OP_MAIN_NEXT_UNDELETED;
	  ch = -1;
	}
	break;

      case OP_PREV_PAGE:
	if (topline != 0)
	{
	  topline = upNLines (bodylen-PagerContext, lineInfo, topline, hideQuoted);
	}
	else
	  mutt_error _("Top of message is shown.");
	break;

      case OP_NEXT_LINE:
	if (lineInfo[curline].offset < sb.st_size-1)
	{
	  topline++;
	  if (hideQuoted)
	  {
	    while (lineInfo[topline].type == MT_COLOR_QUOTED &&
		   topline < lastLine)
	      topline++;
	  }
	}
	else
	  mutt_error _("Bottom of message is shown.");
	break;

      case OP_PREV_LINE:
	if (topline)
	  topline = upNLines (1, lineInfo, topline, hideQuoted);
	else
	  mutt_error _("Top of message is shown.");
	break;

      case OP_PAGER_TOP:
	topline = 0;
	break;

      case OP_HALF_UP:
	if (topline)
	  topline = upNLines (bodylen/2, lineInfo, topline, hideQuoted);
	else
	  mutt_error _("Top of message is shown.");
	break;

      case OP_HALF_DOWN:
	if (lineInfo[curline].offset < sb.st_size-1)
	{
	  topline = upNLines (bodylen/2, lineInfo, curline, hideQuoted);
	}
	else if (option (OPTPAGERSTOP))
	{
	  /* emulate "less -q" and don't go on to the next message. */
	  mutt_error _("Bottom of message is shown.");
	}
	else
	{
	  /* end of the current message, so display the next message. */
	  rc = OP_MAIN_NEXT_UNDELETED;
	  ch = -1;
	}
	break;

      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:
	if (SearchCompiled)
	{
	  if ((!SearchBack && ch==OP_SEARCH_NEXT) ||
	      (SearchBack &&ch==OP_SEARCH_OPPOSITE))
	  {
	    /* searching forward */
	    for (i = topline + 1; i < lastLine; i++)
	    {
	      if ((!hideQuoted || lineInfo[i].type != MT_COLOR_QUOTED) && 
		    !lineInfo[i].continuation && lineInfo[i].search_cnt > 0)
		break;
	    }

	    if (i < lastLine)
	      topline = i;
	    else
	      mutt_error _("Not found.");
	  }
	  else
	  {
	    /* searching backward */
	    for (i = topline - 1; i >= 0; i--)
	    {
	      if ((!hideQuoted || (has_types && 
		    lineInfo[i].type != MT_COLOR_QUOTED)) && 
		    !lineInfo[i].continuation && lineInfo[i].search_cnt > 0)
		break;
	    }

	    if (i >= 0)
	      topline = i;
	    else
	      mutt_error _("Not found.");
	  }

	  if (lineInfo[topline].search_cnt > 0)
	    SearchFlag = M_SEARCH;

	  break;
	}
	/* no previous search pattern, so fall through to search */

      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
	/* leave SearchBack alone if ch == OP_SEARCH_NEXT */
	if (ch == OP_SEARCH)
	  SearchBack = 0;
	else if (ch == OP_SEARCH_REVERSE)
	  SearchBack = 1;

	if (mutt_get_field ((SearchBack ? _("Reverse search: ") :
			  _("Search: ")), searchbuf, sizeof (searchbuf),
			  M_CLEAR) != 0 || !searchbuf[0])
	  break;

	if (SearchCompiled)
	{
	  regfree (&SearchRE);
	  for (i = 0; i < lastLine; i++)
	  {
	    if (lineInfo[i].search)
	      safe_free ((void **) &(lineInfo[i].search));
	    lineInfo[i].search_cnt = -1;
	  }
	}

	if ((err = REGCOMP (&SearchRE, searchbuf, REG_NEWLINE | mutt_which_case (searchbuf))) != 0)
	{
	  regerror (err, &SearchRE, buffer, sizeof (buffer));
	  mutt_error ("%s", buffer);
	  regfree (&SearchRE);
	  for (i = 0; i < maxLine ; i++)
	  {
	    /* cleanup */
	    if (lineInfo[i].search)
	      safe_free ((void **) &(lineInfo[i].search));
	    lineInfo[i].search_cnt = -1;
	  }
	  SearchFlag = 0;
	  SearchCompiled = 0;
	}
	else
	{
	  SearchCompiled = 1;
	  /* update the search pointers */
	  i = 0;
	  while (display_line (fp, &last_pos, &lineInfo, i, &lastLine, 
				&maxLine, M_SEARCH, &QuoteList, &q_level,
				&force_redraw, &SearchRE) == 0)
	    i++;

	  if (!SearchBack)
	  {
	    /* searching forward */
	    for (i = topline; i < lastLine; i++)
	    {
	      if ((!hideQuoted || lineInfo[i].type != MT_COLOR_QUOTED) && 
		    !lineInfo[i].continuation && lineInfo[i].search_cnt > 0)
		break;
	    }

	    if (i < lastLine) topline = i;
	  }
	  else
	  {
	    /* searching backward */
	    for (i = topline; i >= 0; i--)
	    {
	      if ((!hideQuoted || lineInfo[i].type != MT_COLOR_QUOTED) && 
		    !lineInfo[i].continuation && lineInfo[i].search_cnt > 0)
		break;
	    }

	    if (i >= 0) topline = i;
	  }

	  if (lineInfo[topline].search_cnt == 0)
	  {
	    SearchFlag = 0;
	    mutt_error _("Not found.");
	  }
	  else
	    SearchFlag = M_SEARCH;
	}
	redraw = REDRAW_BODY;
	break;

      case OP_SEARCH_TOGGLE:
	if (SearchCompiled)
	{
	  SearchFlag ^= M_SEARCH;
	  redraw = REDRAW_BODY;
	}
	break;

      case OP_HELP:
	/* don't let the user enter the help-menu from the help screen! */
	if (! InHelp)
	{
	  InHelp = 1;
	  mutt_help (MENU_PAGER);
	  redraw = REDRAW_FULL;
	  InHelp = 0;
	}
	else
	  mutt_error _("Help is currently being shown.");
	break;

      case OP_PAGER_HIDE_QUOTED:
	if (has_types)
	{
	  hideQuoted = hideQuoted ? 0 : M_HIDE;
	  if (hideQuoted && lineInfo[topline].type == MT_COLOR_QUOTED)
	    topline = upNLines (1, lineInfo, topline, hideQuoted);
	  else
	    redraw = REDRAW_BODY;
	}
	break;

      case OP_PAGER_SKIP_QUOTED:
	if (has_types)
	{
	  int dretval = 0;
	  int new_topline = topline;

	  while ((new_topline < lastLine ||
		  (0 == (dretval = display_line (fp, &last_pos, &lineInfo,
			 new_topline, &lastLine, &maxLine, M_TYPES,
			 &QuoteList, &q_level, &force_redraw, &SearchRE))))
		 && lineInfo[new_topline].type != MT_COLOR_QUOTED)
	    new_topline++;

	  if (dretval < 0)
	  {
	    mutt_error _("No more quoted text.");
	    break;
	  }

	  while ((new_topline < lastLine ||
		  (0 == (dretval = display_line (fp, &last_pos, &lineInfo,
			 new_topline, &lastLine, &maxLine, M_TYPES,
			 &QuoteList, &q_level, &force_redraw, &SearchRE))))
		 && lineInfo[new_topline].type == MT_COLOR_QUOTED)
	    new_topline++;

	  if (dretval < 0)
	  {
	    mutt_error _("No more unquoted text after quoted text.");
	    break;	  
	  }
	  topline = new_topline;
	}
	break;

      case OP_PAGER_BOTTOM: /* move to the end of the file */
	if (lineInfo[curline].offset < sb.st_size - 1)
	{
	  i = curline;
	  /* make sure the types are defined to the end of file */
	  while (display_line (fp, &last_pos, &lineInfo, i, &lastLine, 
				&maxLine, (has_types ? M_TYPES : M_NOSHOW), 
				&QuoteList, &q_level, &force_redraw,
				&SearchRE) == 0)
	    i++;
	  topline = upNLines (bodylen, lineInfo, lastLine, hideQuoted);
	}
	else
	  mutt_error _("Bottom of message is shown.");
	break;

      case OP_REDRAW:
	clearok (stdscr, TRUE);
	redraw = REDRAW_FULL;
	break;

      case OP_NULL:
	km_error_key (MENU_PAGER);
	break;

	/* --------------------------------------------------------------------
	 * The following are operations on the current message rather than
	 * adjusting the view of the message.
	 */

      case OP_BOUNCE_MESSAGE:
	CHECK_MODE(IsHeader (extra));
        CHECK_ATTACH;
        ci_bounce_message (extra->hdr, &redraw);
	break;

      case OP_CREATE_ALIAS:
	CHECK_MODE(IsHeader (extra));
	mutt_create_alias (extra->hdr->env, NULL);
	MAYBE_REDRAW (redraw);
	break;

      case OP_DELETE:
	CHECK_MODE(IsHeader (extra));
	CHECK_READONLY;
	mutt_set_flag (Context, extra->hdr, M_DELETE, 1);
	redraw = REDRAW_STATUS | REDRAW_INDEX;
	if (option (OPTRESOLVE))
	{
	  ch = -1;
	  rc = OP_MAIN_NEXT_UNDELETED;
	}
	break;

      case OP_DELETE_THREAD:
      case OP_DELETE_SUBTHREAD:
	CHECK_MODE(IsHeader (extra));
	CHECK_READONLY;

	r = mutt_thread_set_flag (extra->hdr, M_DELETE, 1,
				  ch == OP_DELETE_THREAD ? 0 : 1);

	if (r != -1)
	{
	  if (option (OPTRESOLVE))
	  {
	    rc = (ch == OP_DELETE_THREAD) ?
				  OP_MAIN_NEXT_THREAD : OP_MAIN_NEXT_SUBTHREAD;
	    ch = -1;
	  }

	  if (!option (OPTRESOLVE) && PagerIndexLines)
	    redraw = REDRAW_FULL;
	  else
	    redraw = REDRAW_STATUS | REDRAW_INDEX;
	}
	break;

      case OP_DISPLAY_ADDRESS:
	CHECK_MODE(IsHeader (extra));
	mutt_display_address (extra->hdr->env->from);
	break;

      case OP_ENTER_COMMAND:
	old_smart_wrap = option (OPTWRAP);
	old_markers = option (OPTMARKERS);
	old_PagerIndexLines = PagerIndexLines;

	CurrentMenu = MENU_PAGER;
	mutt_enter_command ();

	if (option (OPTNEEDRESORT))
	{
	  unset_option (OPTNEEDRESORT);
	  CHECK_MODE(IsHeader (extra));
	  set_option (OPTNEEDRESORT);
	}

	if (old_PagerIndexLines != PagerIndexLines)
	{
	  if (index)
	    mutt_menuDestroy (&index);
	  index = NULL;
	}
	
	if (option (OPTWRAP) != old_smart_wrap || 
	    option (OPTMARKERS) != old_markers)
	{
	  /* count the real lines above */
	  j = 0;
	  for (i = 0; i <= topline; i++)
	  {
	    if (!lineInfo[i].continuation)
	      j++;
	  }

	  /* we need to restart the whole thing */
	  for (i = 0; i < maxLine; i++)
	  {
	    lineInfo[i].offset = 0;
	    lineInfo[i].type = -1;
	    lineInfo[i].continuation = 0;
	    lineInfo[i].chunks = 0;
	    lineInfo[i].search_cnt = -1;
	    lineInfo[i].quote = NULL;

	    safe_realloc ((void **)&(lineInfo[i].syntax), sizeof (struct syntax_t));
	    if (SearchCompiled && lineInfo[i].search)
		safe_free ((void **) &(lineInfo[i].search));
	  }

	  if (SearchCompiled)
	  {
	    regfree (&SearchRE);
	    SearchCompiled = 0;
	  }
	  SearchFlag = 0;

	  /* try to keep the old position */
	  topline = 0;
	  lastLine = 0;
	  while (j > 0 && display_line (fp, &last_pos, &lineInfo, topline, 
					&lastLine, &maxLine,
					(has_types ? M_TYPES : 0),
					&QuoteList, &q_level, &force_redraw,
					&SearchRE) == 0)
	  {
	    if (! lineInfo[topline].continuation)
	      j--;
	    if (j > 0)
	      topline++;
	  }

	  ch = 0;
	}

	if (option (OPTFORCEREDRAWPAGER))
	  redraw = REDRAW_FULL;
	unset_option (OPTFORCEREDRAWINDEX);
	unset_option (OPTFORCEREDRAWPAGER);
	break;

      case OP_FLAG_MESSAGE:
	CHECK_MODE(IsHeader (extra));
	CHECK_READONLY;
	mutt_set_flag (Context, extra->hdr, M_FLAG, !extra->hdr->flagged);
	redraw = REDRAW_STATUS | REDRAW_INDEX;
	if (option (OPTRESOLVE))
	{
	  ch = -1;
	  rc = OP_MAIN_NEXT_UNDELETED;
	}
	break;

      case OP_PIPE:
	CHECK_MODE(IsHeader (extra) || IsAttach (extra));
	if (IsAttach (extra))
	  mutt_pipe_attachment_list (extra->fp, 0, extra->bdy, 0);
	else
	  mutt_pipe_message (extra->hdr);
	MAYBE_REDRAW (redraw);
	break;

      case OP_PRINT:
	CHECK_MODE(IsHeader (extra));
	mutt_print_message (extra->hdr);
	break;

      case OP_MAIL:
	CHECK_MODE(IsHeader (extra));
        CHECK_ATTACH;      
	ci_send_message (0, NULL, NULL, NULL, NULL);
	redraw = REDRAW_FULL;
	break;

      case OP_REPLY:
	CHECK_MODE(IsHeader (extra));
        CHECK_ATTACH;      
	ci_send_message (SENDREPLY, NULL, NULL, extra->ctx, extra->hdr);
	redraw = REDRAW_FULL;
	break;

      case OP_RECALL_MESSAGE:
	CHECK_MODE(IsHeader (extra));
        CHECK_ATTACH;
	ci_send_message (SENDPOSTPONED, NULL, NULL, extra->ctx, extra->hdr);
	redraw = REDRAW_FULL;
	break;

      case OP_GROUP_REPLY:
	CHECK_MODE(IsHeader (extra));
        CHECK_ATTACH;
	ci_send_message (SENDREPLY | SENDGROUPREPLY, NULL, NULL, extra->ctx, extra->hdr);
	redraw = REDRAW_FULL;
	break;

      case OP_LIST_REPLY:
	CHECK_MODE(IsHeader (extra));
        CHECK_ATTACH;
	ci_send_message (SENDREPLY | SENDLISTREPLY, NULL, NULL, extra->ctx, extra->hdr);
	redraw = REDRAW_FULL;
	break;

      case OP_FORWARD_MESSAGE:
	CHECK_MODE(IsHeader (extra));
        CHECK_ATTACH;
	ci_send_message (SENDFORWARD, NULL, NULL, extra->ctx, extra->hdr);
	redraw = REDRAW_FULL;
	break;

#ifdef _PGPPATH      
      case OP_DECRYPT_SAVE:
#endif
      case OP_SAVE:
	if (IsAttach (extra))
	{
	  mutt_save_attachment_list (extra->fp, 0, extra->bdy, extra->hdr);
	  break;
	}
	/* fall through */
      case OP_COPY_MESSAGE:
      case OP_DECODE_SAVE:
      case OP_DECODE_COPY:
#ifdef _PGPPATH
      case OP_DECRYPT_COPY:
#endif
	CHECK_MODE(IsHeader (extra));
	if (mutt_save_message (extra->hdr,
#ifdef _PGPPATH
			       (ch == OP_DECRYPT_SAVE) ||
#endif			       
			       (ch == OP_SAVE) || (ch == OP_DECODE_SAVE),
			       (ch == OP_DECODE_SAVE) || (ch == OP_DECODE_COPY),
#ifdef _PGPPATH
			       (ch == OP_DECRYPT_SAVE) || (ch == OP_DECRYPT_COPY),
#else
			       0,
#endif
			       &redraw) == 0 && (ch == OP_SAVE || ch == OP_DECODE_SAVE
#ifdef _PGPPATH
						 || ch == OP_DECRYPT_SAVE
#endif
						 ))
	{
	  if (option (OPTRESOLVE))
	  {
	    ch = -1;
	    rc = OP_MAIN_NEXT_UNDELETED;
	  }
	  else
	    redraw |= REDRAW_STATUS | REDRAW_INDEX;
	}
	MAYBE_REDRAW (redraw);
	break;

      case OP_SHELL_ESCAPE:
	mutt_shell_escape ();
	MAYBE_REDRAW (redraw);
	break;

      case OP_TAG:
	CHECK_MODE(IsHeader (extra));
	mutt_set_flag (Context, extra->hdr, M_TAG, !extra->hdr->tagged);
	redraw = REDRAW_STATUS | REDRAW_INDEX;
	if (option (OPTRESOLVE))
	{
	  ch = -1;
	  rc = OP_MAIN_NEXT_UNDELETED;
	}
	break;

      case OP_TOGGLE_NEW:
	CHECK_MODE(IsHeader (extra));
	CHECK_READONLY;
	if (extra->hdr->read || extra->hdr->old)
	  mutt_set_flag (Context, extra->hdr, M_NEW, 1);
	else if (!first)
	  mutt_set_flag (Context, extra->hdr, M_READ, 1);
	first = 0;
        Context->msgnotreadyet = -1;
	redraw = REDRAW_STATUS | REDRAW_INDEX;
	if (option (OPTRESOLVE))
	{
	  ch = -1;
	  rc = OP_NEXT_ENTRY;
	}
	break;

      case OP_UNDELETE:
	CHECK_MODE(IsHeader (extra));
	CHECK_READONLY;
	mutt_set_flag (Context, extra->hdr, M_DELETE, 0);
	redraw = REDRAW_STATUS | REDRAW_INDEX;
	if (option (OPTRESOLVE))
	{
	  ch = -1;
	  rc = OP_NEXT_ENTRY;
	}
	break;

      case OP_UNDELETE_THREAD:
      case OP_UNDELETE_SUBTHREAD:
	CHECK_MODE(IsHeader (extra));
	CHECK_READONLY;

	r = mutt_thread_set_flag (extra->hdr, M_DELETE, 0,
				  ch == OP_UNDELETE_THREAD ? 0 : 1);

	if (r != -1)
	{
	  if (option (OPTRESOLVE))
	  {
	    rc = (ch == OP_DELETE_THREAD) ?
				  OP_MAIN_NEXT_THREAD : OP_MAIN_NEXT_SUBTHREAD;
	    ch = -1;
	  }

	  if (!option (OPTRESOLVE) && PagerIndexLines)
	    redraw = REDRAW_FULL;
	  else
	    redraw = REDRAW_STATUS | REDRAW_INDEX;
	}
	break;

      case OP_VERSION:
	mutt_version ();
	break;

      case OP_VIEW_ATTACHMENTS:
	CHECK_MODE(IsHeader (extra));
	mutt_view_attachments (extra->hdr);
	if (extra->hdr->attach_del)
	  Context->changed = 1;
	redraw = REDRAW_FULL;
	break;



#ifdef _PGPPATH
      case OP_FORGET_PASSPHRASE:
	mutt_forget_passphrase ();
	break;

      case OP_MAIL_KEY:
	CHECK_MODE(IsHeader(extra));
        CHECK_ATTACH;
	ci_send_message (SENDKEY, NULL, NULL, extra->ctx, extra->hdr);
	redraw = REDRAW_FULL;
	break;
      
      case OP_EXTRACT_KEYS:
        CHECK_MODE(IsHeader(extra));
        pgp_extract_keys_from_messages(extra->hdr);
        redraw = REDRAW_FULL;
        break;
#endif /* _PGPPATH */



      default:
	ch = -1;
	break;
    }
  }

  fclose (fp);
  if (IsHeader (extra))
    Context->msgnotreadyet = -1;
    
  cleanup_quote (&QuoteList);
  
  for (i = 0; i < maxLine ; i++)
  {
    safe_free ((void **) &(lineInfo[i].syntax));
    if (SearchCompiled && lineInfo[i].search)
      safe_free ((void **) &(lineInfo[i].search));
  }
  if (SearchCompiled)
  {
    regfree (&SearchRE);
    SearchCompiled = 0;
  }
  safe_free ((void **) &lineInfo);
  if (index)
    mutt_menuDestroy(&index);
  return (rc != -1 ? rc : 0);
}
