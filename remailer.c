/*
 * Copyright (C) 1999-2001 Thomas Roessler <roessler@does-not-exist.org>
 * 
 *     This program is free software; you can redistribute it
 *     and/or modify it under the terms of the GNU General Public
 *     License as published by the Free Software Foundation; either
 *     version 2 of the License, or (at your option) any later
 *     version.
 * 
 *     This program is distributed in the hope that it will be
 *     useful, but WITHOUT ANY WARRANTY; without even the implied
 *     warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *     PURPOSE.  See the GNU General Public License for more
 *     details.
 * 
 *     You should have received a copy of the GNU General Public
 *     License along with this program; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *     Boston, MA  02110-1301, USA.
 */ 

/*
 * Mixmaster support for Mutt
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_regex.h"
#include "mapping.h"

#include "remailer.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/file.h>
#include <fcntl.h>

#ifdef MIXMASTER

struct coord
{
  short r, c;
};

static REMAILER **mix_type2_list (size_t *l);
static REMAILER *mix_new_remailer (void);
static const char *mix_format_caps (REMAILER *r);
static int mix_chain_add (MIXCHAIN *chain, const char *s, REMAILER **type2_list);
static int mix_get_caps (const char *capstr);
static void mix_add_entry (REMAILER ***, REMAILER *, size_t *, size_t *);
static void mix_entry (char *b, size_t blen, MUTTMENU *menu, int num);
static void mix_free_remailer (REMAILER **r);
static void mix_free_type2_list (REMAILER ***ttlp);
static void mix_redraw_ce (REMAILER **type2_list, struct coord *coords, MIXCHAIN *chain, int i, short selected);
static void mix_redraw_chain (REMAILER **type2_list, struct coord *coords, MIXCHAIN *chain, int cur);
static void mix_redraw_head (MIXCHAIN *);
static void mix_screen_coordinates (REMAILER **type2_list, struct coord **, MIXCHAIN *, int);

static int mix_get_caps (const char *capstr)
{
  int caps = 0;

  while (*capstr)
  {
    switch (*capstr)
    {
      case 'C':
      	caps |= MIX_CAP_COMPRESS;
        break;
      
      case 'M':
        caps |= MIX_CAP_MIDDLEMAN;
        break;
      
      case 'N':
      {
	switch (*++capstr)
	{
	  case 'm':
	    caps |= MIX_CAP_NEWSMAIL;
	    break;
	  
	  case 'p':
	    caps |= MIX_CAP_NEWSPOST;
	    break;
	  
	}
      }
    }
    
    if (*capstr) capstr++;
  }
  
  return caps;
}

static void mix_add_entry (REMAILER ***type2_list, REMAILER *entry,
			   size_t *slots, size_t *used)
{
  if (*used == *slots)
  {
    *slots += 5;
    safe_realloc (type2_list, sizeof (REMAILER *) * (*slots));
  }
  
  (*type2_list)[(*used)++] = entry;
  if (entry) entry->num = *used;
}

static REMAILER *mix_new_remailer (void)
{
  return safe_calloc (1, sizeof (REMAILER));
}

static void mix_free_remailer (REMAILER **r)
{
  FREE (&(*r)->shortname);
  FREE (&(*r)->addr);
  FREE (&(*r)->ver);
  
  FREE (r);		/* __FREE_CHECKED__ */
}

/* parse the type2.list as given by mixmaster -T */

static REMAILER **mix_type2_list (size_t *l)
{
  FILE *fp;
  pid_t mm_pid;
  int devnull;

  char cmd[HUGE_STRING + _POSIX_PATH_MAX];
  char line[HUGE_STRING];
  char *t;
  
  REMAILER **type2_list = NULL, *p;
  size_t slots = 0, used = 0;

  if (!l)
    return NULL;
  
  if ((devnull = open ("/dev/null", O_RDWR)) == -1)
    return NULL;
  
  snprintf (cmd, sizeof (cmd), "%s -T", Mixmaster);
  
  if ((mm_pid = mutt_create_filter_fd (cmd, NULL, &fp, NULL, devnull, -1, devnull)) == -1)
  {
    close (devnull);
    return NULL;
  }

  /* first, generate the "random" remailer */
  
  p = mix_new_remailer ();
  p->shortname = safe_strdup ("<random>");
  mix_add_entry (&type2_list, p, &slots, &used);
  
  while (fgets (line, sizeof (line), fp))
  {
    p = mix_new_remailer ();
    
    if (!(t = strtok (line, " \t\n")))
      goto problem;
    
    p->shortname = safe_strdup (t);
    
    if (!(t = strtok (NULL, " \t\n")))
      goto problem;

    p->addr = safe_strdup (t);
    
    if (!(t = strtok (NULL, " \t\n")))
      goto problem;

    if (!(t = strtok (NULL, " \t\n")))
      goto problem;

    p->ver = safe_strdup (t);
    
    if (!(t = strtok (NULL, " \t\n")))
      goto problem;

    p->caps = mix_get_caps (t);
    
    mix_add_entry (&type2_list, p, &slots, &used);
    continue;
    
    problem:
    mix_free_remailer (&p);
  }
  
  *l = used;

  mix_add_entry (&type2_list, NULL, &slots, &used);
  mutt_wait_filter (mm_pid);

  close (devnull);
  
  return type2_list;
}

static void mix_free_type2_list (REMAILER ***ttlp)
{
  int i;
  REMAILER **type2_list = *ttlp;
  
  for (i = 0; type2_list[i]; i++)
    mix_free_remailer (&type2_list[i]);
  
  FREE (type2_list);		/* __FREE_CHECKED__ */
}


#define MIX_HOFFSET 2
#define MIX_VOFFSET (MuttIndexWindow->rows - 4)
#define MIX_MAXROW  (MuttIndexWindow->rows - 1)


static void mix_screen_coordinates (REMAILER **type2_list,
				    struct coord **coordsp,
				    MIXCHAIN *chain,
				    int i)
{
  short c, r, oc;
  struct coord *coords;

  if (!chain->cl)
    return;
  
  safe_realloc (coordsp, sizeof (struct coord) * chain->cl);
  
  coords = *coordsp;
  
  if (i)
  {
    c = coords[i-1].c + strlen (type2_list[chain->ch[i-1]]->shortname) + 2;
    r = coords[i-1].r;
  }
  else
  {
    r = MIX_VOFFSET;
    c = MIX_HOFFSET;
  }
    
  
  for (; i < chain->cl; i++)
  {
    oc = c;
    c += strlen (type2_list[chain->ch[i]]->shortname) + 2;

    if (c  >= MuttIndexWindow->cols)
    {
      oc = c = MIX_HOFFSET;
      r++;
    }
    
    coords[i].c = oc;
    coords[i].r = r;
    
  }
  
}

static void mix_redraw_ce (REMAILER **type2_list,
			   struct coord *coords,
			   MIXCHAIN *chain,
			   int i,
			   short selected)
{
  if (!coords || !chain)
    return;
  
  if (coords[i].r < MIX_MAXROW)
  {
    
    if (selected)
      SETCOLOR (MT_COLOR_INDICATOR);
    else
      NORMAL_COLOR;
    
    mutt_window_mvaddstr (MuttIndexWindow, coords[i].r, coords[i].c,
                          type2_list[chain->ch[i]]->shortname);
    NORMAL_COLOR;

    if (i + 1 < chain->cl)
      addstr (", ");
  }
}

static void mix_redraw_chain (REMAILER **type2_list,
			      struct coord *coords,
			      MIXCHAIN *chain,
			      int cur)
{
  int i;
  
  for (i = MIX_VOFFSET; i < MIX_MAXROW; i++)
  {
    mutt_window_move (MuttIndexWindow, i, 0);
    mutt_window_clrtoeol (MuttIndexWindow);
  }

  for (i = 0; i < chain->cl; i++)
    mix_redraw_ce (type2_list, coords, chain, i, i == cur);
}

static void mix_redraw_head (MIXCHAIN *chain)
{
  SETCOLOR (MT_COLOR_STATUS);
  mutt_window_mvprintw (MuttIndexWindow, MIX_VOFFSET - 1, 0,
                        "-- Remailer chain [Length: %d]", chain ? chain->cl : 0);
  mutt_window_clrtoeol (MuttIndexWindow);
  NORMAL_COLOR;
}

static const char *mix_format_caps (REMAILER *r)
{
  static char capbuff[10];
  char *t = capbuff;
  
  if (r->caps & MIX_CAP_COMPRESS)
    *t++ = 'C';
  else
    *t++ = ' ';
  
  if (r->caps & MIX_CAP_MIDDLEMAN)
    *t++ = 'M';
  else
    *t++ = ' ';
  
  if (r->caps & MIX_CAP_NEWSPOST)
  {
    *t++ = 'N';
    *t++ = 'p';
  }
  else
  {
    *t++ = ' ';
    *t++ = ' ';
  }
   
  if (r->caps & MIX_CAP_NEWSMAIL)
  {
    *t++ = 'N';
    *t++ = 'm';
  }
  else
  {
    *t++ = ' ';
    *t++ = ' ';
  }
  
  *t = '\0';
  
  return capbuff;
}

/*
 * Format an entry for the remailer menu.
 * 
 * %n	number
 * %c	capabilities
 * %s	short name
 * %a	address
 *
 */

static const char *mix_entry_fmt (char *dest,
				  size_t destlen,
				  size_t col,
                                  int cols,
				  char op,
				  const char *src,
				  const char *prefix,
				  const char *ifstring,
				  const char *elsestring,
				  unsigned long data,
				  format_flag flags)
{
  char fmt[16];
  REMAILER *remailer = (REMAILER *) data;
  int optional = (flags & MUTT_FORMAT_OPTIONAL);

  switch (op)
  {
    case 'n':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	snprintf (dest, destlen, fmt, remailer->num);
      }
      break;
    case 'c':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, mix_format_caps(remailer));
      }
      break;
    case 's':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL(remailer->shortname));
      }
      else if (!remailer->shortname)
        optional = 0;
      break;
    case 'a':
      if (!optional)
      {
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	snprintf (dest, destlen, fmt, NONULL(remailer->addr));
      }
      else if (!remailer->addr)
        optional = 0;
      break;
    
    default:
      *dest = '\0';
  }

  if (optional)
    mutt_FormatString (dest, destlen, col, cols, ifstring, mutt_attach_fmt, data, 0);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    mutt_FormatString (dest, destlen, col, cols, elsestring, mutt_attach_fmt, data, 0);
  return (src);
}


  
static void mix_entry (char *b, size_t blen, MUTTMENU *menu, int num)
{
  REMAILER **type2_list = (REMAILER **) menu->data;
  mutt_FormatString (b, blen, 0, MuttIndexWindow->cols, NONULL (MixEntryFormat), mix_entry_fmt,
		     (unsigned long) type2_list[num], MUTT_FORMAT_ARROWCURSOR);
}

static int mix_chain_add (MIXCHAIN *chain, const char *s, 
			  REMAILER **type2_list)
{
  int i;
  
  if (chain->cl >= MAXMIXES)
    return -1;
  
  if (!mutt_strcmp (s, "0") || !ascii_strcasecmp (s, "<random>"))
  {
    chain->ch[chain->cl++] = 0;
    return 0;
  }

  for (i = 0; type2_list[i]; i++)
  {
    if (!ascii_strcasecmp (s, type2_list[i]->shortname))
    {
      chain->ch[chain->cl++] = i;
      return 0;
    }
  }
  
  /* replace unknown remailers by <random> */
  
  if (!type2_list[i])
    chain->ch[chain->cl++] = 0;

  return 0;
}

static const struct mapping_t RemailerHelp[] = 
{
  { N_("Append"), OP_MIX_APPEND },
  { N_("Insert"), OP_MIX_INSERT },
  { N_("Delete"), OP_MIX_DELETE },
  { N_("Abort"),  OP_EXIT       },
  { N_("OK"),     OP_MIX_USE    },
  { NULL,         0 }
};
  

void mix_make_chain (LIST **chainp, int *redraw)
{
  LIST *p;
  MIXCHAIN *chain;
  int c_cur = 0, c_old = 0;
  short c_redraw = 1;
  
  REMAILER **type2_list = NULL;
  size_t ttll = 0;
  
  struct coord *coords = NULL;
  
  MUTTMENU *menu;
  char helpstr[LONG_STRING];
  short loop = 1;
  int op;
  
  int i, j;
  char *t;

  if (!(type2_list = mix_type2_list (&ttll)))
  {
    mutt_error _("Can't get mixmaster's type2.list!");
    return;
  }

  *redraw = REDRAW_FULL;
  
  chain = safe_calloc (sizeof (MIXCHAIN), 1);
  for (p = *chainp; p; p = p->next)
    mix_chain_add (chain, (char *) p->data, type2_list);

  mutt_free_list (chainp);
  
  /* safety check */
  for (i = 0; i < chain->cl; i++)
  {
    if (chain->ch[i] >= ttll)
      chain->ch[i] = 0;
  }
  
  mix_screen_coordinates (type2_list, &coords, chain, 0);
  
  menu = mutt_new_menu (MENU_MIX);
  menu->max = ttll;
  menu->make_entry = mix_entry;
  menu->tag = NULL;
  menu->title = _("Select a remailer chain.");
  menu->data = type2_list;
  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_MIX, RemailerHelp);
  menu->pagelen = MIX_VOFFSET - 1;
  
  while (loop) 
  {
    if (menu->pagelen != MIX_VOFFSET - 1)
    {
      menu->pagelen = MIX_VOFFSET - 1;
      menu->redraw = REDRAW_FULL;
    }
    
    if (c_redraw)
    {
      mix_redraw_head (chain);
      mix_redraw_chain (type2_list, coords, chain, c_cur);
      c_redraw = 0;
    }
    else if (c_cur != c_old)
    {
      mix_redraw_ce (type2_list, coords, chain, c_old, 0);
      mix_redraw_ce (type2_list, coords, chain, c_cur, 1);
    }
    
    c_old = c_cur;
    
    switch ((op = mutt_menuLoop (menu)))
    {
      case OP_REDRAW:
      {
	menu_redraw_status (menu);
	mix_redraw_head (chain);
	mix_screen_coordinates (type2_list, &coords, chain, 0);
	mix_redraw_chain (type2_list, coords, chain, c_cur);
	menu->pagelen = MIX_VOFFSET - 1;
	break;
      }
      
      case OP_EXIT:
      {
	chain->cl = 0;
	loop = 0;
	break;
      }

      case OP_MIX_USE:
      {
	if (!chain->cl)
	{
	  chain->cl++;
	  chain->ch[0] = menu->current;
	  mix_screen_coordinates (type2_list, &coords, chain, c_cur);
	  c_redraw = 1;
	}
	
	if (chain->cl && chain->ch[chain->cl - 1] && 
	    (type2_list[chain->ch[chain->cl-1]]->caps & MIX_CAP_MIDDLEMAN))
	{
	  mutt_error ( _("Error: %s can't be used as the final remailer of a chain."),
		    type2_list[chain->ch[chain->cl - 1]]->shortname);
	}
	else
	{
	  loop = 0;
	}
	break;
      }

      case OP_GENERIC_SELECT_ENTRY:
      case OP_MIX_APPEND:
      {
	if (chain->cl < MAXMIXES && c_cur < chain->cl)
	  c_cur++;
      }
      /* fallthrough */
      case OP_MIX_INSERT:
      {
	if (chain->cl < MAXMIXES)
	{
	  chain->cl++;
	  for (i = chain->cl - 1; i > c_cur; i--)
	    chain->ch[i] = chain->ch[i-1];
	  
	  chain->ch[c_cur] = menu->current;
	  mix_screen_coordinates (type2_list, &coords, chain, c_cur);
	  c_redraw = 1;
	}
	else
	  mutt_error ( _("Mixmaster chains are limited to %d elements."),
		    MAXMIXES);
	
	break;
      }
      
      case OP_MIX_DELETE:
      {
	if (chain->cl)
	{
	  chain->cl--;
	  
	  for (i = c_cur; i < chain->cl; i++)
	    chain->ch[i] = chain->ch[i+1];

	  if (c_cur == chain->cl && c_cur)
	    c_cur--;
	  
	  mix_screen_coordinates (type2_list, &coords, chain, c_cur);
	  c_redraw = 1;
	}
	else
	{
	  mutt_error _("The remailer chain is already empty.");
	}
	break;
      }
      
      case OP_MIX_CHAIN_PREV:
      {
	if (c_cur) 
	  c_cur--;
	else
	  mutt_error _("You already have the first chain element selected.");
	
	break;
      }
      
      case OP_MIX_CHAIN_NEXT:
      {
	if (chain->cl && c_cur < chain->cl - 1)
	  c_cur++;
	else
	  mutt_error _("You already have the last chain element selected.");
	
	break;
      }
    }
  }
  
  mutt_menuDestroy (&menu);

  /* construct the remailer list */
  
  if (chain->cl)
  {
    for (i = 0; i < chain->cl; i++)
    {
      if ((j = chain->ch[i]))
	t = type2_list[j]->shortname;
      else
	t = "*";
      
      *chainp = mutt_add_list (*chainp, t);
    }
  }
  
  mix_free_type2_list (&type2_list);
  FREE (&coords);
  FREE (&chain);
}

/* some safety checks before piping the message to mixmaster */

int mix_check_message (HEADER *msg)
{
  const char *fqdn;
  short need_hostname = 0;
  ADDRESS *p;
  
  if (msg->env->cc || msg->env->bcc)
  {
    mutt_error _("Mixmaster doesn't accept Cc or Bcc headers.");
    return -1;
  }

  /* When using mixmaster, we MUST qualify any addresses since
   * the message will be delivered through remote systems.
   * 
   * use_domain won't be respected at this point, hidden_host will.
   */

  for (p = msg->env->to; p; p = p->next)
  {
    if (!p->group && strchr (p->mailbox, '@') == NULL)
    {
      need_hostname = 1;
      break;
    }
  }
    
  if (need_hostname)
  {
    
    if (!(fqdn = mutt_fqdn (1)))
    {
      mutt_error _("Please set the hostname variable to a proper value when using mixmaster!");
      return (-1);
    }
  
    /* Cc and Bcc are empty at this point. */
    rfc822_qualify (msg->env->to, fqdn);
    rfc822_qualify (msg->env->reply_to, fqdn);
    rfc822_qualify (msg->env->mail_followup_to, fqdn);
  }

  return 0;
}

int mix_send_message (LIST *chain, const char *tempfile)
{
  char cmd[HUGE_STRING];
  char tmp[HUGE_STRING];
  char cd_quoted[STRING];
  int i;

  snprintf (cmd, sizeof (cmd), "cat %s | %s -m ", tempfile, Mixmaster);
  
  for (i = 0; chain; chain = chain->next, i = 1)
  {
    strfcpy (tmp, cmd, sizeof (tmp));
    mutt_quote_filename (cd_quoted, sizeof (cd_quoted), (char *) chain->data);
    snprintf (cmd, sizeof (cmd), "%s%s%s", tmp, i ? "," : " -l ", cd_quoted);
  }

  if (!option (OPTNOCURSES))
    mutt_endwin (NULL);
  
  if ((i = mutt_system (cmd)))
  {
    fprintf (stderr, _("Error sending message, child exited %d.\n"), i);
    if (!option (OPTNOCURSES))
    {
      mutt_any_key_to_continue (NULL);
      mutt_error _("Error sending message.");
    }
  }

  unlink (tempfile);
  return i;
}
  

#endif
