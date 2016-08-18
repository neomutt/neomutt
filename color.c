/*
 * Copyright (C) 1996-2002,2012 Michael R. Elkins <me@mutt.org>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */ 

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mapping.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* globals */
int *ColorQuote;
int ColorQuoteUsed;
int ColorDefs[MT_COLOR_MAX];
COLOR_LINE *ColorHdrList = NULL;
COLOR_LINE *ColorBodyList = NULL;
COLOR_LINE *ColorIndexList = NULL;

/* local to this file */
static int ColorQuoteSize;

#ifdef HAVE_COLOR

#define COLOR_DEFAULT (-2)

typedef struct color_list
{
  short fg;
  short bg;
  short index;
  short count;
  struct color_list *next;
} COLOR_LIST;

static COLOR_LIST *ColorList = NULL;
static int UserColors = 0;

static const struct mapping_t Colors[] =
{
  { "black",	COLOR_BLACK },
  { "blue",	COLOR_BLUE },
  { "cyan",	COLOR_CYAN },
  { "green",	COLOR_GREEN },
  { "magenta",	COLOR_MAGENTA },
  { "red",	COLOR_RED },
  { "white",	COLOR_WHITE },
  { "yellow",	COLOR_YELLOW },
#if defined (USE_SLANG_CURSES) || defined (HAVE_USE_DEFAULT_COLORS)
  { "default",	COLOR_DEFAULT },
#endif
  { 0, 0 }
};

#endif /* HAVE_COLOR */

static const struct mapping_t Fields[] =
{
  { "hdrdefault",	MT_COLOR_HDEFAULT },
  { "quoted",		MT_COLOR_QUOTED },
  { "signature",	MT_COLOR_SIGNATURE },
  { "indicator",	MT_COLOR_INDICATOR },
  { "status",		MT_COLOR_STATUS },
  { "tree",		MT_COLOR_TREE },
  { "error",		MT_COLOR_ERROR },
  { "normal",		MT_COLOR_NORMAL },
  { "tilde",		MT_COLOR_TILDE },
  { "markers",		MT_COLOR_MARKERS },
  { "header",		MT_COLOR_HEADER },
  { "body",		MT_COLOR_BODY },
  { "message",		MT_COLOR_MESSAGE },
  { "attachment",	MT_COLOR_ATTACHMENT },
  { "search",		MT_COLOR_SEARCH },
  { "bold",		MT_COLOR_BOLD },
  { "underline",	MT_COLOR_UNDERLINE },
  { "index",		MT_COLOR_INDEX },
  { "prompt",		MT_COLOR_PROMPT },
#ifdef USE_SIDEBAR
  { "sidebar_divider",	MT_COLOR_DIVIDER },
  { "sidebar_flagged",	MT_COLOR_FLAGGED },
  { "sidebar_highlight",MT_COLOR_HIGHLIGHT },
  { "sidebar_indicator",MT_COLOR_SB_INDICATOR },
  { "sidebar_new",	MT_COLOR_NEW },
  { "sidebar_spoolfile",MT_COLOR_SB_SPOOLFILE },
#endif
  { NULL,		0 }
};

#define COLOR_QUOTE_INIT	8

static COLOR_LINE *mutt_new_color_line (void)
{
  COLOR_LINE *p = safe_calloc (1, sizeof (COLOR_LINE));

  p->fg = p->bg = -1;
  
  return (p);
}

static void mutt_free_color_line(COLOR_LINE **l, 
				 int free_colors)
{
  COLOR_LINE *tmp;
 
  if(!l || !*l)
    return;

  tmp = *l;

#ifdef HAVE_COLOR
  if(free_colors && tmp->fg != -1 && tmp->bg != -1)
    mutt_free_color(tmp->fg, tmp->bg);
#endif

  /* we should really introduce a container
   * type for regular expressions.
   */
  
  regfree(&tmp->rx);
  mutt_pattern_free(&tmp->color_pattern);
  FREE (&tmp->pattern);
  FREE (l);		/* __FREE_CHECKED__ */
}

void ci_start_color (void)
{
  memset (ColorDefs, A_NORMAL, sizeof (int) * MT_COLOR_MAX);
  ColorQuote = (int *) safe_malloc (COLOR_QUOTE_INIT * sizeof (int));
  memset (ColorQuote, A_NORMAL, sizeof (int) * COLOR_QUOTE_INIT);
  ColorQuoteSize = COLOR_QUOTE_INIT;
  ColorQuoteUsed = 0;

  /* set some defaults */
  ColorDefs[MT_COLOR_STATUS] = A_REVERSE;
  ColorDefs[MT_COLOR_INDICATOR] = A_REVERSE;
  ColorDefs[MT_COLOR_SEARCH] = A_REVERSE;
  ColorDefs[MT_COLOR_MARKERS] = A_REVERSE;
#ifdef USE_SIDEBAR
  ColorDefs[MT_COLOR_HIGHLIGHT] = A_UNDERLINE;
#endif
  /* special meaning: toggle the relevant attribute */
  ColorDefs[MT_COLOR_BOLD] = 0;
  ColorDefs[MT_COLOR_UNDERLINE] = 0;

#ifdef HAVE_COLOR
  start_color ();
#endif
}

#ifdef HAVE_COLOR

#ifdef USE_SLANG_CURSES
static char *get_color_name (char *dest, size_t destlen, int val)
{
  static const char * const missing[3] = {"brown", "lightgray", "default"};
  int i;

  switch (val)
  {
    case COLOR_YELLOW:
      strfcpy (dest, missing[0], destlen);
      return dest;

    case COLOR_WHITE:
      strfcpy (dest, missing[1], destlen);
      return dest;
      
    case COLOR_DEFAULT:
      strfcpy (dest, missing[2], destlen);
      return dest;
  }

  for (i = 0; Colors[i].name; i++)
  {
    if (Colors[i].value == val)
    {
      strfcpy (dest, Colors[i].name, destlen);
      return dest;
    }
  }

  /* Sigh. If we got this far, the color is of the form 'colorN'
   * Slang can handle this itself, so just return 'colorN'
   */

  snprintf (dest, destlen, "color%d", val);
  return dest;
}
#endif

int mutt_alloc_color (int fg, int bg)
{
  COLOR_LIST *p = ColorList;
  int i;
  
#if defined (USE_SLANG_CURSES)
  char fgc[SHORT_STRING], bgc[SHORT_STRING];
#endif

  /* check to see if this color is already allocated to save space */
  while (p)
  {
    if (p->fg == fg && p->bg == bg)
    {
      (p->count)++;
      return (COLOR_PAIR (p->index));
    }
    p = p->next;
  }

  /* check to see if there are colors left */
  if (++UserColors > COLOR_PAIRS) return (A_NORMAL);

  /* find the smallest available index (object) */
  i = 1;
  FOREVER
  {
    p = ColorList;
    while (p)
    {
      if (p->index == i) break;
      p = p->next;
    }
    if (p == NULL) break;
    i++;
  }

  p = (COLOR_LIST *) safe_malloc (sizeof (COLOR_LIST));
  p->next = ColorList;
  ColorList = p;

  p->index = i;
  p->count = 1;
  p->bg = bg;
  p->fg = fg;

#if defined (USE_SLANG_CURSES)
  if (fg == COLOR_DEFAULT || bg == COLOR_DEFAULT)
    SLtt_set_color (i, NULL, get_color_name (fgc, sizeof (fgc), fg), get_color_name (bgc, sizeof (bgc), bg));
  else
#elif defined (HAVE_USE_DEFAULT_COLORS)
  if (fg == COLOR_DEFAULT)
    fg = -1;
  if (bg == COLOR_DEFAULT)
    bg = -1;
#endif

  init_pair(i, fg, bg);

  dprint (3, (debugfile,"mutt_alloc_color(): Color pairs used so far: %d\n",
	      UserColors));

  return (COLOR_PAIR (p->index));
}

void mutt_free_color (int fg, int bg)
{
  COLOR_LIST *p, *q;

  p = ColorList;
  while (p)
  {
    if (p->fg == fg && p->bg == bg)
    {
      (p->count)--;
      if (p->count > 0) return;

      UserColors--;
      dprint(1,(debugfile,"mutt_free_color(): Color pairs used so far: %d\n",
                           UserColors));

      if (p == ColorList)
      {
	ColorList = ColorList->next;
	FREE (&p);
	return;
      }
      q = ColorList;
      while (q)
      {
	if (q->next == p)
	{
	  q->next = p->next;
	  FREE (&p);
	  return;
	}
	q = q->next;
      }
      /* can't get here */
    }
    p = p->next;
  }
}

#endif /* HAVE_COLOR */


#ifdef HAVE_COLOR

static int
parse_color_name (const char *s, int *col, int *attr, int is_fg, BUFFER *err)
{
  char *eptr;
  int is_bright = 0;

  if (ascii_strncasecmp (s, "bright", 6) == 0)
  {
    is_bright = 1;
    s += 6;
  }

  /* allow aliases for xterm color resources */
  if (ascii_strncasecmp (s, "color", 5) == 0)
  {
    s += 5;
    *col = strtol (s, &eptr, 10);
    if (!*s || *eptr || *col < 0 ||
	(*col >= COLORS && !option(OPTNOCURSES) && has_colors()))
    {
      snprintf (err->data, err->dsize, _("%s: color not supported by term"), s);
      return (-1);
    }
  }
  else if ((*col = mutt_getvaluebyname (s, Colors)) == -1)
  {
    snprintf (err->data, err->dsize, _("%s: no such color"), s);
    return (-1);
  }

  if (is_bright)
  {
    if (is_fg)
    {
      *attr |= A_BOLD;
    }
    else if (COLORS < 16)
    {
      /* A_BLINK turns the background color brite on some terms */
      *attr |= A_BLINK;
    }
    else
    {
      /* Advance the color by 8 to get the bright version */
      *col += 8;
    }
  }

  return 0;
}

#endif


/* usage: uncolor index pattern [pattern...]
 * 	  unmono  index pattern [pattern...]
 */

static int 
_mutt_parse_uncolor (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err, 
			 short parse_uncolor);


#ifdef HAVE_COLOR

int mutt_parse_uncolor (BUFFER *buf, BUFFER *s, unsigned long data,
			BUFFER *err)
{
  return _mutt_parse_uncolor(buf, s, data, err, 1);
}

#endif

int mutt_parse_unmono (BUFFER *buf, BUFFER *s, unsigned long data,
		       BUFFER *err)
{
  return _mutt_parse_uncolor(buf, s, data, err, 0);
}

static int _mutt_parse_uncolor (BUFFER *buf, BUFFER *s, unsigned long data,
				BUFFER *err, short parse_uncolor)
{
  int object = 0, do_cache = 0;
  COLOR_LINE *tmp, *last = NULL;
  COLOR_LINE **list;

  mutt_extract_token (buf, s, 0);

  if ((object = mutt_getvaluebyname (buf->data, Fields)) == -1)
  {
    snprintf (err->data, err->dsize, _("%s: no such object"), buf->data);
    return (-1);
  }

  if (mutt_strncmp (buf->data, "index", 5) == 0)
    list = &ColorIndexList;
  else if (mutt_strncmp (buf->data, "body", 4) == 0)
    list = &ColorBodyList;
  else if (mutt_strncmp (buf->data, "header", 7) == 0)
    list = &ColorHdrList;
  else
  {
    snprintf (err->data, err->dsize,
	      _("%s: command valid only for index, body, header objects"),
	      parse_uncolor ? "uncolor" : "unmono");
    return (-1);
  }

  if (!MoreArgs (s))
  {
    snprintf (err->data, err->dsize,
	      _("%s: too few arguments"), parse_uncolor ? "uncolor" : "unmono");
    return (-1);
  }

  if(
#ifdef HAVE_COLOR
     /* we're running without curses */
     option (OPTNOCURSES) 
     || /* we're parsing an uncolor command, and have no colors */
     (parse_uncolor && !has_colors())
     /* we're parsing an unmono command, and have colors */
     || (!parse_uncolor && has_colors())
#else
     /* We don't even have colors compiled in */
     parse_uncolor
#endif
     )
  {
    /* just eat the command, but don't do anything real about it */
    do
      mutt_extract_token (buf, s, 0);
    while (MoreArgs (s));

    return 0;
  }

  do
  {
    mutt_extract_token (buf, s, 0);
    if (!mutt_strcmp ("*", buf->data))
    {
      for (tmp = *list; tmp; )
      {
        if (!do_cache)
	  do_cache = 1;
	last = tmp;
	tmp = tmp->next;
	mutt_free_color_line(&last, parse_uncolor);
      }
      *list = NULL;
    }
    else
    {
      for (last = NULL, tmp = *list; tmp; last = tmp, tmp = tmp->next)
      {
	if (!mutt_strcmp (buf->data, tmp->pattern))
	{
          if (!do_cache)
	    do_cache = 1;
	  dprint(1,(debugfile,"Freeing pattern \"%s\" from color list\n",
	                       tmp->pattern));
	  if (last)
	    last->next = tmp->next;
	  else
	    *list = tmp->next;
	  mutt_free_color_line(&tmp, parse_uncolor);
	  break;
	}
      }
    }
  }
  while (MoreArgs (s));


  if (do_cache && !option (OPTNOCURSES))
  {
    int i;
    set_option (OPTFORCEREDRAWINDEX);
    /* force re-caching of index colors */
    for (i = 0; Context && i < Context->msgcount; i++)
      Context->hdrs[i]->pair = 0;
  }
  return (0);
}


static int 
add_pattern (COLOR_LINE **top, const char *s, int sensitive,
	     int fg, int bg, int attr, BUFFER *err,
	     int is_index)
{

  /* is_index used to store compiled pattern
   * only for `index' color object 
   * when called from mutt_parse_color() */

  COLOR_LINE *tmp = *top;

  while (tmp)
  {
    if (sensitive)
    {
      if (mutt_strcmp (s, tmp->pattern) == 0)
	break;
    }
    else
    {
      if (mutt_strcasecmp (s, tmp->pattern) == 0)
	break;
    }
    tmp = tmp->next;
  }

  if (tmp)
  {
#ifdef HAVE_COLOR
    if (fg != -1 && bg != -1)
    {
      if (tmp->fg != fg || tmp->bg != bg)
      {
	mutt_free_color (tmp->fg, tmp->bg);
	tmp->fg = fg;
	tmp->bg = bg;
	attr |= mutt_alloc_color (fg, bg);
      }
      else
	attr |= (tmp->pair & ~A_BOLD);
    }
#endif /* HAVE_COLOR */
    tmp->pair = attr;
  }
  else
  {
    int r;
    char buf[LONG_STRING];

    tmp = mutt_new_color_line ();
    if (is_index) 
    {
      int i;

      strfcpy(buf, NONULL(s), sizeof(buf));
      mutt_check_simple (buf, sizeof (buf), NONULL(SimpleSearch));
      if((tmp->color_pattern = mutt_pattern_comp (buf, MUTT_FULL_MSG, err)) == NULL)
      {
	mutt_free_color_line(&tmp, 1);
	return -1;
      }
      /* force re-caching of index colors */
      for (i = 0; Context && i < Context->msgcount; i++)
	Context->hdrs[i]->pair = 0;
    }
    else if ((r = REGCOMP (&tmp->rx, s, (sensitive ? mutt_which_case (s) : REG_ICASE))) != 0)
    {
      regerror (r, &tmp->rx, err->data, err->dsize);
      mutt_free_color_line(&tmp, 1);
      return (-1);
    }
    tmp->next = *top;
    tmp->pattern = safe_strdup (s);
#ifdef HAVE_COLOR
    if(fg != -1 && bg != -1)
    {
      tmp->fg = fg;
      tmp->bg = bg;
      attr |= mutt_alloc_color (fg, bg);
    }
#endif
    tmp->pair = attr;
    *top = tmp;
  }

  return 0;
}

static int
parse_object(BUFFER *buf, BUFFER *s, int *o, int *ql, BUFFER *err)
{
  int q_level = 0;
  char *eptr;
  
  if(!MoreArgs(s))
  {
    strfcpy(err->data, _("Missing arguments."), err->dsize);
    return -1;
  }
  
  mutt_extract_token(buf, s, 0);
  if(!mutt_strncmp(buf->data, "quoted", 6))
  {
    if(buf->data[6])
    {
      *ql = strtol(buf->data + 6, &eptr, 10);
      if(*eptr || q_level < 0)
      {
	snprintf(err->data, err->dsize, _("%s: no such object"), buf->data);
	return -1;
      }
    }
    else
      *ql = 0;
    
    *o = MT_COLOR_QUOTED;
  }
  else if ((*o = mutt_getvaluebyname (buf->data, Fields)) == -1)
  {
    snprintf (err->data, err->dsize, _("%s: no such object"), buf->data);
    return (-1);
  }

  return 0;
}

typedef int (*parser_callback_t)(BUFFER *, BUFFER *, int *, int *, int *, BUFFER *);

#ifdef HAVE_COLOR

static int
parse_color_pair(BUFFER *buf, BUFFER *s, int *fg, int *bg, int *attr, BUFFER *err)
{
  if (! MoreArgs (s))
  {
    strfcpy (err->data, _("color: too few arguments"), err->dsize);
    return (-1);
  }

  mutt_extract_token (buf, s, 0);

  if (parse_color_name (buf->data, fg, attr, 1, err) != 0)
    return (-1);

  if (! MoreArgs (s))
  {
    strfcpy (err->data, _("color: too few arguments"), err->dsize);
    return (-1);
  }
  
  mutt_extract_token (buf, s, 0);

  if (parse_color_name (buf->data, bg, attr, 0, err) != 0)
    return (-1);
  
  return 0;
}

#endif

static int
parse_attr_spec(BUFFER *buf, BUFFER *s, int *fg, int *bg, int *attr, BUFFER *err)
{
  
  if(fg) *fg = -1; 
  if(bg) *bg = -1;

  if (! MoreArgs (s))
  {
    strfcpy (err->data, _("mono: too few arguments"), err->dsize);
    return (-1);
  }

  mutt_extract_token (buf, s, 0);

  if (ascii_strcasecmp ("bold", buf->data) == 0)
    *attr |= A_BOLD;
  else if (ascii_strcasecmp ("underline", buf->data) == 0)
    *attr |= A_UNDERLINE;
  else if (ascii_strcasecmp ("none", buf->data) == 0)
    *attr = A_NORMAL;
  else if (ascii_strcasecmp ("reverse", buf->data) == 0)
    *attr |= A_REVERSE;
  else if (ascii_strcasecmp ("standout", buf->data) == 0)
    *attr |= A_STANDOUT;
  else if (ascii_strcasecmp ("normal", buf->data) == 0)
    *attr = A_NORMAL; /* needs use = instead of |= to clear other bits */
  else
  {
    snprintf (err->data, err->dsize, _("%s: no such attribute"), buf->data);
    return (-1);
  }
  
  return 0;
}

static int fgbgattr_to_color(int fg, int bg, int attr)
{
#ifdef HAVE_COLOR
  if(fg != -1 && bg != -1)
    return attr | mutt_alloc_color(fg, bg);
  else
#endif
    return attr;
}

/* usage: color <object> <fg> <bg> [ <regexp> ] 
 * 	  mono  <object> <attr> [ <regexp> ]
 */

static int 
_mutt_parse_color (BUFFER *buf, BUFFER *s, BUFFER *err, 
		   parser_callback_t callback, short dry_run)
{
  int object = 0, attr = 0, fg = 0, bg = 0, q_level = 0;
  int r = 0;

  if(parse_object(buf, s, &object, &q_level, err) == -1)
    return -1;

  if(callback(buf, s, &fg, &bg, &attr, err) == -1)
    return -1;

  /* extract a regular expression if needed */
  
  if (object == MT_COLOR_HEADER || object == MT_COLOR_BODY || object == MT_COLOR_INDEX)
  {
    if (!MoreArgs (s))
    {
      strfcpy (err->data, _("too few arguments"), err->dsize);
      return (-1);
    }

    mutt_extract_token (buf, s, 0);
  }
   
  if (MoreArgs (s))
  {
    strfcpy (err->data, _("too many arguments"), err->dsize);
    return (-1);
  }
  
  /* dry run? */
  
  if(dry_run) return 0;

  
#ifdef HAVE_COLOR
# ifdef HAVE_USE_DEFAULT_COLORS
  if (!option (OPTNOCURSES) && has_colors()
    /* delay use_default_colors() until needed, since it initializes things */
    && (fg == COLOR_DEFAULT || bg == COLOR_DEFAULT)
    && use_default_colors () != OK)
  {
    strfcpy (err->data, _("default colors not supported"), err->dsize);
    return (-1);
  }
# endif /* HAVE_USE_DEFAULT_COLORS */
#endif
  
  if (object == MT_COLOR_HEADER)
    r = add_pattern (&ColorHdrList, buf->data, 0, fg, bg, attr, err,0);
  else if (object == MT_COLOR_BODY)
    r = add_pattern (&ColorBodyList, buf->data, 1, fg, bg, attr, err, 0);
  else if (object == MT_COLOR_INDEX)
  {
    r = add_pattern (&ColorIndexList, buf->data, 1, fg, bg, attr, err, 1);
    set_option (OPTFORCEREDRAWINDEX);
  }
  else if (object == MT_COLOR_QUOTED)
  {
    if (q_level >= ColorQuoteSize)
    {
      safe_realloc (&ColorQuote, (ColorQuoteSize += 2) * sizeof (int));
      ColorQuote[ColorQuoteSize-2] = ColorDefs[MT_COLOR_QUOTED];
      ColorQuote[ColorQuoteSize-1] = ColorDefs[MT_COLOR_QUOTED];
    }
    if (q_level >= ColorQuoteUsed)
      ColorQuoteUsed = q_level + 1;
    if (q_level == 0)
    {
      ColorDefs[MT_COLOR_QUOTED] = fgbgattr_to_color(fg, bg, attr);
      
      ColorQuote[0] = ColorDefs[MT_COLOR_QUOTED];
      for (q_level = 1; q_level < ColorQuoteUsed; q_level++)
      {
	if (ColorQuote[q_level] == A_NORMAL)
	  ColorQuote[q_level] = ColorDefs[MT_COLOR_QUOTED];
      }
    }
    else
      ColorQuote[q_level] = fgbgattr_to_color(fg, bg, attr);
  }
  else
    ColorDefs[object] = fgbgattr_to_color(fg, bg, attr);

  return (r);
}

#ifdef HAVE_COLOR

int mutt_parse_color(BUFFER *buff, BUFFER *s, unsigned long data, BUFFER *err)
{
  int dry_run = 0;
  
  if(option(OPTNOCURSES) || !has_colors())
    dry_run = 1;
  
  return _mutt_parse_color(buff, s, err, parse_color_pair, dry_run);
}

#endif

int mutt_parse_mono(BUFFER *buff, BUFFER *s, unsigned long data, BUFFER *err)
{
  int dry_run = 0;
  
#ifdef HAVE_COLOR
  if(option(OPTNOCURSES) || has_colors())
    dry_run = 1;
#else
  if(option(OPTNOCURSES))
    dry_run = 1;
#endif

  return _mutt_parse_color(buff, s, err, parse_attr_spec, dry_run);
}

