/*
 * Copyright (C) 1996-2002 Michael R. Elkins <me@mutt.org>
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
#include "mapping.h"
#include "mutt_curses.h"
#include "mutt_menu.h"
#include "mutt_regex.h"
#include "history.h"
#include "keymap.h"
#include "mbyte.h"
#include "charset.h"
#include "mutt_crypt.h"
#include "mutt_idna.h"

#if defined(USE_SSL)
#include "mutt_ssl.h"
#endif



#include "mx.h"
#include "init.h"
#include "mailbox.h"

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/utsname.h>
#include <errno.h>
#include <sys/wait.h>

#define CHECK_PAGER \
  if ((CurrentMenu == MENU_PAGER) && (idx >= 0) &&	\
	    (MuttVars[idx].flags & R_RESORT)) \
	{ \
	  snprintf (err->data, err->dsize, \
	    _("Not available in this menu.")); \
	  return (-1); \
	}

typedef struct myvar
{
  char *name;
  char *value;
  struct myvar* next;
} myvar_t;

static myvar_t* MyVars;

static int var_to_string (int idx, char* val, size_t len);

static void myvar_set (const char* var, const char* val);
static const char* myvar_get (const char* var);
static void myvar_del (const char* var);

static void toggle_quadoption (int opt)
{
  int n = opt/4;
  int b = (opt % 4) * 2;

  QuadOptions[n] ^= (1 << b);
}

void set_quadoption (int opt, int flag)
{
  int n = opt/4;
  int b = (opt % 4) * 2;

  QuadOptions[n] &= ~(0x3 << b);
  QuadOptions[n] |= (flag & 0x3) << b;
}

int quadoption (int opt)
{
  int n = opt/4;
  int b = (opt % 4) * 2;

  return (QuadOptions[n] >> b) & 0x3;
}

int query_quadoption (int opt, const char *prompt)
{
  int v = quadoption (opt);

  switch (v)
  {
    case M_YES:
    case M_NO:
      return (v);

    default:
      v = mutt_yesorno (prompt, (v == M_ASKYES));
      CLEARLINE (LINES - 1);
      return (v);
  }

  /* not reached */
}

/* given the variable ``s'', return the index into the rc_vars array which
   matches, or -1 if the variable is not found.  */
static int mutt_option_index (char *s)
{
  int i;

  for (i = 0; MuttVars[i].option; i++)
    if (mutt_strcmp (s, MuttVars[i].option) == 0)
      return (MuttVars[i].type == DT_SYN ?  mutt_option_index ((char *) MuttVars[i].data) : i);
  return (-1);
}

int mutt_extract_token (BUFFER *dest, BUFFER *tok, int flags)
{
  char		ch;
  char		qc = 0; /* quote char */
  char		*pc;

  /* reset the destination pointer to the beginning of the buffer */
  dest->dptr = dest->data;

  SKIPWS (tok->dptr);
  while ((ch = *tok->dptr))
  {
    if (!qc)
    {
      if ((ISSPACE (ch) && !(flags & M_TOKEN_SPACE)) ||
	  (ch == '#' && !(flags & M_TOKEN_COMMENT)) ||
	  (ch == '=' && (flags & M_TOKEN_EQUAL)) ||
	  (ch == ';' && !(flags & M_TOKEN_SEMICOLON)) ||
	  ((flags & M_TOKEN_PATTERN) && strchr ("~%=!|", ch)))
	break;
    }

    tok->dptr++;

    if (ch == qc)
      qc = 0; /* end of quote */
    else if (!qc && (ch == '\'' || ch == '"') && !(flags & M_TOKEN_QUOTE))
      qc = ch;
    else if (ch == '\\' && qc != '\'')
    {
	if (!*tok->dptr)
	    return -1; /* premature end of token */
      switch (ch = *tok->dptr++)
      {
	case 'c':
	case 'C':
	    if (!*tok->dptr)
		return -1; /* premature end of token */
	  mutt_buffer_addch (dest, (toupper ((unsigned char) *tok->dptr)
                                    - '@') & 0x7f);
	  tok->dptr++;
	  break;
	case 'r':
	  mutt_buffer_addch (dest, '\r');
	  break;
	case 'n':
	  mutt_buffer_addch (dest, '\n');
	  break;
	case 't':
	  mutt_buffer_addch (dest, '\t');
	  break;
	case 'f':
	  mutt_buffer_addch (dest, '\f');
	  break;
	case 'e':
	  mutt_buffer_addch (dest, '\033');
	  break;
	default:
	  if (isdigit ((unsigned char) ch) &&
	      isdigit ((unsigned char) *tok->dptr) &&
	      isdigit ((unsigned char) *(tok->dptr + 1)))
	  {

	    mutt_buffer_addch (dest, (ch << 6) + (*tok->dptr << 3) + *(tok->dptr + 1) - 3504);
	    tok->dptr += 2;
	  }
	  else
	    mutt_buffer_addch (dest, ch);
      }
    }
    else if (ch == '^' && (flags & M_TOKEN_CONDENSE))
    {
	if (!*tok->dptr)
	    return -1; /* premature end of token */
      ch = *tok->dptr++;
      if (ch == '^')
	mutt_buffer_addch (dest, ch);
      else if (ch == '[')
	mutt_buffer_addch (dest, '\033');
      else if (isalpha ((unsigned char) ch))
	mutt_buffer_addch (dest, toupper ((unsigned char) ch) - '@');
      else
      {
	mutt_buffer_addch (dest, '^');
	mutt_buffer_addch (dest, ch);
      }
    }
    else if (ch == '`' && (!qc || qc == '"'))
    {
      FILE	*fp;
      pid_t	pid;
      char	*cmd, *ptr;
      size_t	expnlen;
      BUFFER	expn;
      int	line = 0;

      pc = tok->dptr;
      do {
	if ((pc = strpbrk (pc, "\\`")))
	{
	  /* skip any quoted chars */
	  if (*pc == '\\')
	    pc += 2;
	}
      } while (pc && *pc != '`');
      if (!pc)
      {
	dprint (1, (debugfile, "mutt_get_token: mismatched backticks\n"));
	return (-1);
      }
      cmd = mutt_substrdup (tok->dptr, pc);
      if ((pid = mutt_create_filter (cmd, NULL, &fp, NULL)) < 0)
      {
	dprint (1, (debugfile, "mutt_get_token: unable to fork command: %s", cmd));
	FREE (&cmd);
	return (-1);
      }
      FREE (&cmd);

      tok->dptr = pc + 1;

      /* read line */
      memset (&expn, 0, sizeof (expn));
      expn.data = mutt_read_line (NULL, &expn.dsize, fp, &line, 0);
      safe_fclose (&fp);
      mutt_wait_filter (pid);

      /* if we got output, make a new string consiting of the shell ouptput
	 plus whatever else was left on the original line */
      /* BUT: If this is inside a quoted string, directly add output to 
       * the token */
      if (expn.data && qc)
      {
	mutt_buffer_addstr (dest, expn.data);
	FREE (&expn.data);
      }
      else if (expn.data)
      {
	expnlen = mutt_strlen (expn.data);
	tok->dsize = expnlen + mutt_strlen (tok->dptr) + 1;
	ptr = safe_malloc (tok->dsize);
	memcpy (ptr, expn.data, expnlen);
	strcpy (ptr + expnlen, tok->dptr);	/* __STRCPY_CHECKED__ */
	if (tok->destroy)
	  FREE (&tok->data);
	tok->data = ptr;
	tok->dptr = ptr;
	tok->destroy = 1; /* mark that the caller should destroy this data */
	ptr = NULL;
	FREE (&expn.data);
      }
    }
    else if (ch == '$' && (!qc || qc == '"') && (*tok->dptr == '{' || isalpha ((unsigned char) *tok->dptr)))
    {
      const char *env = NULL;
      char *var = NULL;
      int idx;

      if (*tok->dptr == '{')
      {
	tok->dptr++;
	if ((pc = strchr (tok->dptr, '}')))
	{
	  var = mutt_substrdup (tok->dptr, pc);
	  tok->dptr = pc + 1;
	}
      }
      else
      {
	for (pc = tok->dptr; isalnum ((unsigned char) *pc) || *pc == '_'; pc++)
	  ;
	var = mutt_substrdup (tok->dptr, pc);
	tok->dptr = pc;
      }
      if (var)
      {
        if ((env = getenv (var)) || (env = myvar_get (var)))
          mutt_buffer_addstr (dest, env);
        else if ((idx = mutt_option_index (var)) != -1)
        {
          /* expand settable mutt variables */
          char val[LONG_STRING];

          if (var_to_string (idx, val, sizeof (val)))
            mutt_buffer_addstr (dest, val);
        }
        FREE (&var);
      }
    }
    else
      mutt_buffer_addch (dest, ch);
  }
  mutt_buffer_addch (dest, 0); /* terminate the string */
  SKIPWS (tok->dptr);
  return 0;
}

static void mutt_free_opt (struct option_t* p)
{
  REGEXP* pp;

  switch (p->type & DT_MASK)
  {
  case DT_ADDR:
    rfc822_free_address ((ADDRESS**)p->data);
    break;
  case DT_RX:
    pp = (REGEXP*)p->data;
    FREE (&pp->pattern);
    if (pp->rx)
    {
      regfree (pp->rx);
      FREE (&pp->rx);
    }
    break;
  case DT_PATH:
  case DT_STR:
    FREE ((char**)p->data);		/* __FREE_CHECKED__ */
    break;
  }
}

/* clean up before quitting */
void mutt_free_opts (void)
{
  int i;

  for (i = 0; MuttVars[i].option; i++)
    mutt_free_opt (MuttVars + i);

  mutt_free_rx_list (&Alternates);
  mutt_free_rx_list (&UnAlternates);
  mutt_free_rx_list (&MailLists);
  mutt_free_rx_list (&UnMailLists);
  mutt_free_rx_list (&SubscribedLists);
  mutt_free_rx_list (&UnSubscribedLists);
  mutt_free_rx_list (&NoSpamList);
}

static void add_to_list (LIST **list, const char *str)
{
  LIST *t, *last = NULL;

  /* don't add a NULL or empty string to the list */
  if (!str || *str == '\0')
    return;

  /* check to make sure the item is not already on this list */
  for (last = *list; last; last = last->next)
  {
    if (ascii_strcasecmp (str, last->data) == 0)
    {
      /* already on the list, so just ignore it */
      last = NULL;
      break;
    }
    if (!last->next)
      break;
  }

  if (!*list || last)
  {
    t = (LIST *) safe_calloc (1, sizeof (LIST));
    t->data = safe_strdup (str);
    if (last)
    {
      last->next = t;
      last = last->next;
    }
    else
      *list = last = t;
  }
}

int mutt_add_to_rx_list (RX_LIST **list, const char *s, int flags, BUFFER *err)
{
  RX_LIST *t, *last = NULL;
  REGEXP *rx;

  if (!s || !*s)
    return 0;

  if (!(rx = mutt_compile_regexp (s, flags)))
  {
    snprintf (err->data, err->dsize, "Bad regexp: %s\n", s);
    return -1;
  }

  /* check to make sure the item is not already on this list */
  for (last = *list; last; last = last->next)
  {
    if (ascii_strcasecmp (rx->pattern, last->rx->pattern) == 0)
    {
      /* already on the list, so just ignore it */
      last = NULL;
      break;
    }
    if (!last->next)
      break;
  }

  if (!*list || last)
  {
    t = mutt_new_rx_list();
    t->rx = rx;
    if (last)
    {
      last->next = t;
      last = last->next;
    }
    else
      *list = last = t;
  }
  else /* duplicate */
    mutt_free_regexp (&rx);

  return 0;
}

static int remove_from_spam_list (SPAM_LIST **list, const char *pat);

static int add_to_spam_list (SPAM_LIST **list, const char *pat, const char *templ, BUFFER *err)
{
  SPAM_LIST *t = NULL, *last = NULL;
  REGEXP *rx;
  int n;
  const char *p;

  if (!pat || !*pat || !templ)
    return 0;

  if (!(rx = mutt_compile_regexp (pat, REG_ICASE)))
  {
    snprintf (err->data, err->dsize, _("Bad regexp: %s"), pat);
    return -1;
  }

  /* check to make sure the item is not already on this list */
  for (last = *list; last; last = last->next)
  {
    if (ascii_strcasecmp (rx->pattern, last->rx->pattern) == 0)
    {
      /* Already on the list. Formerly we just skipped this case, but
       * now we're supporting removals, which means we're supporting
       * re-adds conceptually. So we probably want this to imply a
       * removal, then do an add. We can achieve the removal by freeing
       * the template, and leaving t pointed at the current item.
       */
      t = last;
      FREE(&t->template);
      break;
    }
    if (!last->next)
      break;
  }

  /* If t is set, it's pointing into an extant SPAM_LIST* that we want to
   * update. Otherwise we want to make a new one to link at the list's end.
   */
  if (!t)
  {
    t = mutt_new_spam_list();
    t->rx = rx;
    if (last)
      last->next = t;
    else
      *list = t;
  }

  /* Now t is the SPAM_LIST* that we want to modify. It is prepared. */
  t->template = safe_strdup(templ);

  /* Find highest match number in template string */
  t->nmatch = 0;
  for (p = templ; *p;)
  {
    if (*p == '%')
    {
        n = atoi(++p);
        if (n > t->nmatch)
          t->nmatch = n;
        while (*p && isdigit((int)*p))
          ++p;
    }
    else
        ++p;
  }

  if (t->nmatch > t->rx->rx->re_nsub)
  {
    snprintf (err->data, err->dsize, _("Not enough subexpressions for spam "
                                       "template"));
    remove_from_spam_list(list, pat);
    return -1;
  }

  t->nmatch++;         /* match 0 is always the whole expr */

  return 0;
}

static int remove_from_spam_list (SPAM_LIST **list, const char *pat)
{
  SPAM_LIST *spam, *prev;
  int nremoved = 0;

  /* Being first is a special case. */
  spam = *list;
  if (!spam)
    return 0;
  if (spam->rx && !mutt_strcmp(spam->rx->pattern, pat))
  {
    *list = spam->next;
    mutt_free_regexp(&spam->rx);
    FREE(&spam->template);
    FREE(&spam);
    return 1;
  }

  prev = spam;
  for (spam = prev->next; spam;)
  {
    if (!mutt_strcmp(spam->rx->pattern, pat))
    {
      prev->next = spam->next;
      mutt_free_regexp(&spam->rx);
      FREE(&spam->template);
      FREE(&spam);
      spam = prev->next;
      ++nremoved;
    }
    else
      spam = spam->next;
  }

  return nremoved;
}


static void remove_from_list (LIST **l, const char *str)
{
  LIST *p, *last = NULL;

  if (mutt_strcmp ("*", str) == 0)
    mutt_free_list (l);    /* ``unCMD *'' means delete all current entries */
  else
  {
    p = *l;
    last = NULL;
    while (p)
    {
      if (ascii_strcasecmp (str, p->data) == 0)
      {
	FREE (&p->data);
	if (last)
	  last->next = p->next;
	else
	  (*l) = p->next;
	FREE (&p);
      }
      else
      {
	last = p;
	p = p->next;
      }
    }
  }
}

static int remove_from_rx_list (RX_LIST **l, const char *str)
{
  RX_LIST *p, *last = NULL;
  int rv = -1;

  if (mutt_strcmp ("*", str) == 0)
  {
    mutt_free_rx_list (l);    /* ``unCMD *'' means delete all current entries */
    rv = 0;
  }
  else
  {
    p = *l;
    last = NULL;
    while (p)
    {
      if (ascii_strcasecmp (str, p->rx->pattern) == 0)
      {
	mutt_free_regexp (&p->rx);
	if (last)
	  last->next = p->next;
	else
	  (*l) = p->next;
	FREE (&p);
	rv = 0;
      }
      else
      {
	last = p;
	p = p->next;
      }
    }
  }
  return (rv);
}

static int parse_unignore (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  do
  {
    mutt_extract_token (buf, s, 0);

    /* don't add "*" to the unignore list */
    if (strcmp (buf->data, "*")) 
      add_to_list (&UnIgnore, buf->data);

    remove_from_list (&Ignore, buf->data);
  }
  while (MoreArgs (s));

  return 0;
}

static int parse_ignore (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  do
  {
    mutt_extract_token (buf, s, 0);
    remove_from_list (&UnIgnore, buf->data);
    add_to_list (&Ignore, buf->data);
  }
  while (MoreArgs (s));

  return 0;
}

static int parse_list (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  do
  {
    mutt_extract_token (buf, s, 0);
    add_to_list ((LIST **) data, buf->data);
  }
  while (MoreArgs (s));

  return 0;
}

static void _alternates_clean (void)
{
  int i;
  if (Context && Context->msgcount) 
  {
    for (i = 0; i < Context->msgcount; i++)
      Context->hdrs[i]->recip_valid = 0;
  }
}

static int parse_alternates (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  group_context_t *gc = NULL;
  
  _alternates_clean();

  do
  {
    mutt_extract_token (buf, s, 0);

    if (parse_group_context (&gc, buf, s, data, err) == -1)
      goto bail;

    remove_from_rx_list (&UnAlternates, buf->data);

    if (mutt_add_to_rx_list (&Alternates, buf->data, REG_ICASE, err) != 0)
      goto bail;

    if (mutt_group_context_add_rx (gc, buf->data, REG_ICASE, err) != 0)
      goto bail;
  }
  while (MoreArgs (s));
  
  mutt_group_context_destroy (&gc);
  return 0;
  
 bail:
  mutt_group_context_destroy (&gc);
  return -1;
}

static int parse_unalternates (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  _alternates_clean();
  do
  {
    mutt_extract_token (buf, s, 0);
    remove_from_rx_list (&Alternates, buf->data);

    if (mutt_strcmp (buf->data, "*") &&
	mutt_add_to_rx_list (&UnAlternates, buf->data, REG_ICASE, err) != 0)
      return -1;

  }
  while (MoreArgs (s));

  return 0;
}

static int parse_spam_list (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  BUFFER templ;

  memset(&templ, 0, sizeof(templ));

  /* Insist on at least one parameter */
  if (!MoreArgs(s))
  {
    if (data == M_SPAM)
      strfcpy(err->data, _("spam: no matching pattern"), err->dsize);
    else
      strfcpy(err->data, _("nospam: no matching pattern"), err->dsize);
    return -1;
  }

  /* Extract the first token, a regexp */
  mutt_extract_token (buf, s, 0);

  /* data should be either M_SPAM or M_NOSPAM. M_SPAM is for spam commands. */
  if (data == M_SPAM)
  {
    /* If there's a second parameter, it's a template for the spam tag. */
    if (MoreArgs(s))
    {
      mutt_extract_token (&templ, s, 0);

      /* Add to the spam list. */
      if (add_to_spam_list (&SpamList, buf->data, templ.data, err) != 0) {
	  FREE(&templ.data);
          return -1;
      }
      FREE(&templ.data);
    }

    /* If not, try to remove from the nospam list. */
    else
    {
      remove_from_rx_list(&NoSpamList, buf->data);
    }

    return 0;
  }

  /* M_NOSPAM is for nospam commands. */
  else if (data == M_NOSPAM)
  {
    /* nospam only ever has one parameter. */

    /* "*" is a special case. */
    if (!mutt_strcmp(buf->data, "*"))
    {
      mutt_free_spam_list (&SpamList);
      mutt_free_rx_list (&NoSpamList);
      return 0;
    }

    /* If it's on the spam list, just remove it. */
    if (remove_from_spam_list(&SpamList, buf->data) != 0)
      return 0;

    /* Otherwise, add it to the nospam list. */
    if (mutt_add_to_rx_list (&NoSpamList, buf->data, REG_ICASE, err) != 0)
      return -1;

    return 0;
  }

  /* This should not happen. */
  strfcpy(err->data, "This is no good at all.", err->dsize);
  return -1;
}


static int parse_unlist (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  do
  {
    mutt_extract_token (buf, s, 0);
    /*
     * Check for deletion of entire list
     */
    if (mutt_strcmp (buf->data, "*") == 0)
    {
      mutt_free_list ((LIST **) data);
      break;
    }
    remove_from_list ((LIST **) data, buf->data);
  }
  while (MoreArgs (s));

  return 0;
}

static int parse_lists (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  group_context_t *gc = NULL;

  do
  {
    mutt_extract_token (buf, s, 0);
    
    if (parse_group_context (&gc, buf, s, data, err) == -1)
      goto bail;
    
    remove_from_rx_list (&UnMailLists, buf->data);
    
    if (mutt_add_to_rx_list (&MailLists, buf->data, REG_ICASE, err) != 0)
      goto bail;
    
    if (mutt_group_context_add_rx (gc, buf->data, REG_ICASE, err) != 0)
      goto bail;
  }
  while (MoreArgs (s));

  mutt_group_context_destroy (&gc);
  return 0;
  
 bail:
  mutt_group_context_destroy (&gc);
  return -1;
}

typedef enum group_state_t {
  NONE, RX, ADDR
} group_state_t;

static int parse_group (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  group_context_t *gc = NULL;
  group_state_t state = NONE;
  ADDRESS *addr = NULL;
  char *estr = NULL;
  
  do 
  {
    mutt_extract_token (buf, s, 0);
    if (parse_group_context (&gc, buf, s, data, err) == -1)
      goto bail;
    
    if (!mutt_strcasecmp (buf->data, "-rx"))
      state = RX;
    else if (!mutt_strcasecmp (buf->data, "-addr"))
      state = ADDR;
    else 
    {
      switch (state) 
      {
	case NONE:
	  strfcpy (err->data, _("Missing -rx or -addr."), err->dsize);
	  goto bail;
	
	case RX:
	  if (mutt_group_context_add_rx (gc, buf->data, REG_ICASE, err) != 0)
	    goto bail;
	  break;
	
	case ADDR:
	  if ((addr = mutt_parse_adrlist (NULL, buf->data)) == NULL)
	    goto bail;
	  if (mutt_addrlist_to_idna (addr, &estr)) 
	  {
	    snprintf (err->data, err->dsize, _("Warning: Bad IDN '%s'.\n"),
		      estr);
	    goto bail;
	  }
	  mutt_group_context_add_adrlist (gc, addr);
	  rfc822_free_address (&addr);
	  break;
      }
    }
  } while (MoreArgs (s));

  mutt_group_context_destroy (&gc);
  return 0;

  bail:
  mutt_group_context_destroy (&gc);
  return -1;
}

static int parse_ungroup (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  strfcpy (err->data, "not implemented", err->dsize);
  return -1;
}

/* always wise to do what someone else did before */
static void _attachments_clean (void)
{
  int i;
  if (Context && Context->msgcount) 
  {
    for (i = 0; i < Context->msgcount; i++)
      Context->hdrs[i]->attach_valid = 0;
  }
}

static int parse_attach_list (BUFFER *buf, BUFFER *s, LIST **ldata, BUFFER *err)
{
  ATTACH_MATCH *a;
  LIST *listp, *lastp;
  char *p;
  char *tmpminor;
  int len;

  /* Find the last item in the list that data points to. */
  lastp = NULL;
  dprint(5, (debugfile, "parse_attach_list: ldata = %p, *ldata = %p\n",
	      (void *)ldata, (void *)*ldata));
  for (listp = *ldata; listp; listp = listp->next)
  {
    a = (ATTACH_MATCH *)listp->data;
    dprint(5, (debugfile, "parse_attach_list: skipping %s/%s\n",
		a->major, a->minor));
    lastp = listp;
  }

  do
  {
    mutt_extract_token (buf, s, 0);

    if (!buf->data || *buf->data == '\0')
      continue;
   
    a = safe_malloc(sizeof(ATTACH_MATCH));

    /* some cheap hacks that I expect to remove */
    if (!ascii_strcasecmp(buf->data, "any"))
      a->major = safe_strdup("*/.*");
    else if (!ascii_strcasecmp(buf->data, "none"))
      a->major = safe_strdup("cheap_hack/this_should_never_match");
    else
      a->major = safe_strdup(buf->data);

    if ((p = strchr(a->major, '/')))
    {
      *p = '\0';
      ++p;
      a->minor = p;
    }
    else
    {
      a->minor = "unknown";
    }

    len = strlen(a->minor);
    tmpminor = safe_malloc(len+3);
    strcpy(&tmpminor[1], a->minor); /* __STRCPY_CHECKED__ */
    tmpminor[0] = '^';
    tmpminor[len+1] = '$';
    tmpminor[len+2] = '\0';

    a->major_int = mutt_check_mime_type(a->major);
    regcomp(&a->minor_rx, tmpminor, REG_ICASE|REG_EXTENDED);

    FREE(&tmpminor);

    dprint(5, (debugfile, "parse_attach_list: added %s/%s [%d]\n",
		a->major, a->minor, a->major_int));

    listp = safe_malloc(sizeof(LIST));
    listp->data = (char *)a;
    listp->next = NULL;
    if (lastp)
    {
      lastp->next = listp;
    }
    else
    {
      *ldata = listp;
    }
    lastp = listp;
  }
  while (MoreArgs (s));
   
  _attachments_clean();
  return 0;
}

static int parse_unattach_list (BUFFER *buf, BUFFER *s, LIST **ldata, BUFFER *err)
{
  ATTACH_MATCH *a;
  LIST *lp, *lastp, *newlp;
  char *tmp;
  int major;
  char *minor;

  do
  {
    mutt_extract_token (buf, s, 0);

    if (!ascii_strcasecmp(buf->data, "any"))
      tmp = safe_strdup("*/.*");
    else if (!ascii_strcasecmp(buf->data, "none"))
      tmp = safe_strdup("cheap_hack/this_should_never_match");
    else
      tmp = safe_strdup(buf->data);

    if ((minor = strchr(tmp, '/')))
    {
      *minor = '\0';
      ++minor;
    }
    else
    {
      minor = "unknown";
    }
    major = mutt_check_mime_type(tmp);

    /* We must do our own walk here because remove_from_list() will only
     * remove the LIST->data, not anything pointed to by the LIST->data. */
    lastp = NULL;
    for(lp = *ldata; lp; )
    {
      a = (ATTACH_MATCH *)lp->data;
      dprint(5, (debugfile, "parse_unattach_list: check %s/%s [%d] : %s/%s [%d]\n",
		  a->major, a->minor, a->major_int, tmp, minor, major));
      if (a->major_int == major && !mutt_strcasecmp(minor, a->minor))
      {
	dprint(5, (debugfile, "parse_unattach_list: removed %s/%s [%d]\n",
		    a->major, a->minor, a->major_int));
	regfree(&a->minor_rx);
	FREE(&a->major);

	/* Relink backward */
	if (lastp)
	  lastp->next = lp->next;
	else
	  *ldata = lp->next;

        newlp = lp->next;
        FREE(&lp->data);	/* same as a */
        FREE(&lp);
        lp = newlp;
        continue;
      }

      lastp = lp;
      lp = lp->next;
    }

  }
  while (MoreArgs (s));
   
  FREE(&tmp);
  _attachments_clean();
  return 0;
}

static int print_attach_list (LIST *lp, char op, char *name)
{
  while (lp) {
    printf("attachments %c%s %s/%s\n", op, name,
           ((ATTACH_MATCH *)lp->data)->major,
           ((ATTACH_MATCH *)lp->data)->minor);
    lp = lp->next;
  }

  return 0;
}


static int parse_attachments (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  char op, *category;
  LIST **listp;

  mutt_extract_token(buf, s, 0);
  if (!buf->data || *buf->data == '\0') {
    strfcpy(err->data, _("attachments: no disposition"), err->dsize);
    return -1;
  }

  category = buf->data;
  op = *category++;

  if (op == '?') {
    mutt_endwin (NULL);
    fflush (stdout);
    printf("\nCurrent attachments settings:\n\n");
    print_attach_list(AttachAllow,   '+', "A");
    print_attach_list(AttachExclude, '-', "A");
    print_attach_list(InlineAllow,   '+', "I");
    print_attach_list(InlineExclude, '-', "I");
    set_option (OPTFORCEREDRAWINDEX);
    set_option (OPTFORCEREDRAWPAGER);
    mutt_any_key_to_continue (NULL);
    return 0;
  }

  if (op != '+' && op != '-') {
    op = '+';
    category--;
  }
  if (!ascii_strncasecmp(category, "attachment", strlen(category))) {
    if (op == '+')
      listp = &AttachAllow;
    else
      listp = &AttachExclude;
  }
  else if (!ascii_strncasecmp(category, "inline", strlen(category))) {
    if (op == '+')
      listp = &InlineAllow;
    else
      listp = &InlineExclude;
  }
  else {
    strfcpy(err->data, _("attachments: invalid disposition"), err->dsize);
    return -1;
  }

  return parse_attach_list(buf, s, listp, err);
}

static int parse_unattachments (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  char op, *p;
  LIST **listp;

  mutt_extract_token(buf, s, 0);
  if (!buf->data || *buf->data == '\0') {
    strfcpy(err->data, _("unattachments: no disposition"), err->dsize);
    return -1;
  }

  p = buf->data;
  op = *p++;
  if (op != '+' && op != '-') {
    op = '+';
    p--;
  }
  if (!ascii_strncasecmp(p, "attachment", strlen(p))) {
    if (op == '+')
      listp = &AttachAllow;
    else
      listp = &AttachExclude;
  }
  else if (!ascii_strncasecmp(p, "inline", strlen(p))) {
    if (op == '+')
      listp = &InlineAllow;
    else
      listp = &InlineExclude;
  }
  else {
    strfcpy(err->data, _("unattachments: invalid disposition"), err->dsize);
    return -1;
  }

  return parse_unattach_list(buf, s, listp, err);
}

static int parse_unlists (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  do
  {
    mutt_extract_token (buf, s, 0);
    remove_from_rx_list (&SubscribedLists, buf->data);
    remove_from_rx_list (&MailLists, buf->data);
    
    if (mutt_strcmp (buf->data, "*") && 
	mutt_add_to_rx_list (&UnMailLists, buf->data, REG_ICASE, err) != 0)
      return -1;
  }
  while (MoreArgs (s));

  return 0;
}

static int parse_subscribe (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  group_context_t *gc = NULL;
  
  do
  {
    mutt_extract_token (buf, s, 0);

    if (parse_group_context (&gc, buf, s, data, err) == -1)
      goto bail;
    
    remove_from_rx_list (&UnMailLists, buf->data);
    remove_from_rx_list (&UnSubscribedLists, buf->data);

    if (mutt_add_to_rx_list (&MailLists, buf->data, REG_ICASE, err) != 0)
      goto bail;
    if (mutt_add_to_rx_list (&SubscribedLists, buf->data, REG_ICASE, err) != 0)
      goto bail;
    if (mutt_group_context_add_rx (gc, buf->data, REG_ICASE, err) != 0)
      goto bail;
  }
  while (MoreArgs (s));
  
  mutt_group_context_destroy (&gc);
  return 0;
  
 bail:
  mutt_group_context_destroy (&gc);
  return -1;
}

static int parse_unsubscribe (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  do
  {
    mutt_extract_token (buf, s, 0);
    remove_from_rx_list (&SubscribedLists, buf->data);
    
    if (mutt_strcmp (buf->data, "*") &&
	mutt_add_to_rx_list (&UnSubscribedLists, buf->data, REG_ICASE, err) != 0)
      return -1;
  }
  while (MoreArgs (s));

  return 0;
}
  
static int parse_unalias (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  ALIAS *tmp, *last = NULL;

  do
  {
    mutt_extract_token (buf, s, 0);

    if (mutt_strcmp ("*", buf->data) == 0)
    {
      if (CurrentMenu == MENU_ALIAS)
      {
	for (tmp = Aliases; tmp ; tmp = tmp->next) 
	  tmp->del = 1;
	set_option (OPTFORCEREDRAWINDEX);
      }
      else
	mutt_free_alias (&Aliases);
      break;
    }
    else
      for (tmp = Aliases; tmp; tmp = tmp->next)
      {
	if (mutt_strcasecmp (buf->data, tmp->name) == 0)
	{
	  if (CurrentMenu == MENU_ALIAS)
	  {
	    tmp->del = 1;
	    set_option (OPTFORCEREDRAWINDEX);
	    break;
	  }

	  if (last)
	    last->next = tmp->next;
	  else
	    Aliases = tmp->next;
	  tmp->next = NULL;
	  mutt_free_alias (&tmp);
	  break;
	}
	last = tmp;
      }
  }
  while (MoreArgs (s));
  return 0;
}

static int parse_alias (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  ALIAS *tmp = Aliases;
  ALIAS *last = NULL;
  char *estr = NULL;
  group_context_t *gc = NULL;
  
  if (!MoreArgs (s))
  {
    strfcpy (err->data, _("alias: no address"), err->dsize);
    return (-1);
  }

  mutt_extract_token (buf, s, 0);

  if (parse_group_context (&gc, buf, s, data, err) == -1)
    return -1;
  
  /* check to see if an alias with this name already exists */
  for (; tmp; tmp = tmp->next)
  {
    if (!mutt_strcasecmp (tmp->name, buf->data))
      break;
    last = tmp;
  }

  if (!tmp)
  {
    /* create a new alias */
    tmp = (ALIAS *) safe_calloc (1, sizeof (ALIAS));
    tmp->self = tmp;
    tmp->name = safe_strdup (buf->data);
    /* give the main addressbook code a chance */
    if (CurrentMenu == MENU_ALIAS)
      set_option (OPTMENUCALLER);
  }
  else
  {
    mutt_alias_delete_reverse (tmp);
    /* override the previous value */
    rfc822_free_address (&tmp->addr);
    if (CurrentMenu == MENU_ALIAS)
      set_option (OPTFORCEREDRAWINDEX);
  }

  mutt_extract_token (buf, s, M_TOKEN_QUOTE | M_TOKEN_SPACE | M_TOKEN_SEMICOLON);
  dprint (3, (debugfile, "parse_alias: Second token is '%s'.\n",
	      buf->data));

  tmp->addr = mutt_parse_adrlist (tmp->addr, buf->data);

  if (last)
    last->next = tmp;
  else
    Aliases = tmp;
  if (mutt_addrlist_to_idna (tmp->addr, &estr))
  {
    snprintf (err->data, err->dsize, _("Warning: Bad IDN '%s' in alias '%s'.\n"),
	      estr, tmp->name);
    goto bail;
  }

  mutt_group_context_add_adrlist (gc, tmp->addr);
  mutt_alias_add_reverse (tmp);

#ifdef DEBUG
  if (debuglevel >= 2) 
  {
    ADDRESS *a;
    /* A group is terminated with an empty address, so check a->mailbox */
    for (a = tmp->addr; a && a->mailbox; a = a->next)
    {
      if (!a->group)
	dprint (3, (debugfile, "parse_alias:   %s\n",
		    a->mailbox));
      else
	dprint (3, (debugfile, "parse_alias:   Group %s\n",
		    a->mailbox));
    }
  }
#endif
  mutt_group_context_destroy (&gc);
  return 0;
  
  bail:
  mutt_group_context_destroy (&gc);
  return -1;
}

static int
parse_unmy_hdr (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  LIST *last = NULL;
  LIST *tmp = UserHeader;
  LIST *ptr;
  size_t l;

  do
  {
    mutt_extract_token (buf, s, 0);
    if (mutt_strcmp ("*", buf->data) == 0)
      mutt_free_list (&UserHeader);
    else
    {
      tmp = UserHeader;
      last = NULL;

      l = mutt_strlen (buf->data);
      if (buf->data[l - 1] == ':')
	l--;

      while (tmp)
      {
	if (ascii_strncasecmp (buf->data, tmp->data, l) == 0 && tmp->data[l] == ':')
	{
	  ptr = tmp;
	  if (last)
	    last->next = tmp->next;
	  else
	    UserHeader = tmp->next;
	  tmp = tmp->next;
	  ptr->next = NULL;
	  mutt_free_list (&ptr);
	}
	else
	{
	  last = tmp;
	  tmp = tmp->next;
	}
      }
    }
  }
  while (MoreArgs (s));
  return 0;
}

static int parse_my_hdr (BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  LIST *tmp;
  size_t keylen;
  char *p;

  mutt_extract_token (buf, s, M_TOKEN_SPACE | M_TOKEN_QUOTE);
  if ((p = strpbrk (buf->data, ": \t")) == NULL || *p != ':')
  {
    strfcpy (err->data, _("invalid header field"), err->dsize);
    return (-1);
  }
  keylen = p - buf->data + 1;

  if (UserHeader)
  {
    for (tmp = UserHeader; ; tmp = tmp->next)
    {
      /* see if there is already a field by this name */
      if (ascii_strncasecmp (buf->data, tmp->data, keylen) == 0)
      {
	/* replace the old value */
	FREE (&tmp->data);
	tmp->data = buf->data;
	memset (buf, 0, sizeof (BUFFER));
	return 0;
      }
      if (!tmp->next)
	break;
    }
    tmp->next = mutt_new_list ();
    tmp = tmp->next;
  }
  else
  {
    tmp = mutt_new_list ();
    UserHeader = tmp;
  }
  tmp->data = buf->data;
  memset (buf, 0, sizeof (BUFFER));
  return 0;
}

static int
parse_sort (short *val, const char *s, const struct mapping_t *map, BUFFER *err)
{
  int i, flags = 0;

  if (mutt_strncmp ("reverse-", s, 8) == 0)
  {
    s += 8;
    flags = SORT_REVERSE;
  }
  
  if (mutt_strncmp ("last-", s, 5) == 0)
  {
    s += 5;
    flags |= SORT_LAST;
  }

  if ((i = mutt_getvaluebyname (s, map)) == -1)
  {
    snprintf (err->data, err->dsize, _("%s: unknown sorting method"), s);
    return (-1);
  }

  *val = i | flags;

  return 0;
}

static void mutt_set_default (struct option_t *p)
{
  switch (p->type & DT_MASK)
  {
    case DT_STR:
      if (!p->init && *((char **) p->data))
        p->init = (unsigned long) safe_strdup (* ((char **) p->data));
      break;
    case DT_PATH:
      if (!p->init && *((char **) p->data))
      {
	char *cp = safe_strdup (*((char **) p->data));
	/* mutt_pretty_mailbox (cp); */
        p->init = (unsigned long) cp;
      }
      break;
    case DT_ADDR:
      if (!p->init && *((ADDRESS **) p->data))
      {
	char tmp[HUGE_STRING];
	*tmp = '\0';
	rfc822_write_address (tmp, sizeof (tmp), *((ADDRESS **) p->data), 0);
	p->init = (unsigned long) safe_strdup (tmp);
      }
      break;
    case DT_RX:
    {
      REGEXP *pp = (REGEXP *) p->data;
      if (!p->init && pp->pattern)
	p->init = (unsigned long) safe_strdup (pp->pattern);
      break;
    }
  }
}

static void mutt_restore_default (struct option_t *p)
{
  switch (p->type & DT_MASK)
  {
    case DT_STR:
      if (p->init)
	mutt_str_replace ((char **) p->data, (char *) p->init); 
      break;
    case DT_PATH:
      if (p->init)
      {
	char path[_POSIX_PATH_MAX];

	strfcpy (path, (char *) p->init, sizeof (path));
	mutt_expand_path (path, sizeof (path));
	mutt_str_replace ((char **) p->data, path);
      }
      break;
    case DT_ADDR:
      if (p->init)
      {
	rfc822_free_address ((ADDRESS **) p->data);
	*((ADDRESS **) p->data) = rfc822_parse_adrlist (NULL, (char *) p->init);
      }
      break;
    case DT_BOOL:
      if (p->init)
	set_option (p->data);
      else
	unset_option (p->data);
      break;
    case DT_QUAD:
      set_quadoption (p->data, p->init);
      break;
    case DT_NUM:
    case DT_SORT:
    case DT_MAGIC:
      *((short *) p->data) = p->init;
      break;
    case DT_RX:
      {
	REGEXP *pp = (REGEXP *) p->data;
	int flags = 0;

	FREE (&pp->pattern);
	if (pp->rx)
	{
	  regfree (pp->rx);
	  FREE (&pp->rx);
	}

	if (p->init)
	{
	  char *s = (char *) p->init;

	  pp->rx = safe_calloc (1, sizeof (regex_t));
	  pp->pattern = safe_strdup ((char *) p->init);
	  if (mutt_strcmp (p->option, "mask") != 0)
	    flags |= mutt_which_case ((const char *) p->init);
	  if (mutt_strcmp (p->option, "mask") == 0 && *s == '!')
	  {
	    s++;
	    pp->not = 1;
	  }
	  if (REGCOMP (pp->rx, s, flags) != 0)
	  {
	    fprintf (stderr, _("mutt_restore_default(%s): error in regexp: %s\n"),
		     p->option, pp->pattern);
	    FREE (&pp->pattern);
	    regfree (pp->rx);
	    FREE (&pp->rx);
	  }
	}
      }
      break;
  }

  if (p->flags & R_INDEX)
    set_option (OPTFORCEREDRAWINDEX);
  if (p->flags & R_PAGER)
    set_option (OPTFORCEREDRAWPAGER);
  if (p->flags & R_RESORT_SUB)
    set_option (OPTSORTSUBTHREADS);
  if (p->flags & R_RESORT)
    set_option (OPTNEEDRESORT);
  if (p->flags & R_RESORT_INIT)
    set_option (OPTRESORTINIT);
  if (p->flags & R_TREE)
    set_option (OPTREDRAWTREE);
}

static size_t escape_string (char *dst, size_t len, const char* src)
{
  char* p = dst;

  if (!len)
    return 0;
  len--; /* save room for \0 */
#define ESC_CHAR(C)	do { *p++ = '\\'; if (p - dst < len) *p++ = C; } while(0)
  while (p - dst < len && src && *src)
  {
    switch (*src)
    {
    case '\n':
      ESC_CHAR('n');
      break;
    case '\r':
      ESC_CHAR('r');
      break;
    case '\t':
      ESC_CHAR('t');
      break;
    default:
      if ((*src == '\\' || *src == '"') && p - dst < len - 1)
	*p++ = '\\';
      *p++ = *src;
    }
    src++;
  }
#undef ESC_CHAR
  *p = '\0';
  return p - dst;
}

static void pretty_var (char *dst, size_t len, const char *option, const char *val)
{
  char *p;

  if (!len)
    return;

  strfcpy (dst, option, len);
  len--; /* save room for \0 */
  p = dst + mutt_strlen (dst);

  if (p - dst < len)
    *p++ = '=';
  if (p - dst < len)
    *p++ = '"';
  p += escape_string (p, len - (p - dst) + 1, val);	/* \0 terminate it */
  if (p - dst < len)
    *p++ = '"';
  *p = 0;
}

static int check_charset (struct option_t *opt, const char *val)
{
  char *p, *q = NULL, *s = safe_strdup (val);
  int rc = 0, strict = strcmp (opt->option, "send_charset") == 0;

  for (p = strtok_r (s, ":", &q); p; p = strtok_r (NULL, ":", &q))
  {
    if (!*p)
      continue;
    if (mutt_check_charset (p, strict) < 0)
    {
      rc = -1;
      break;
    }
  }

  FREE(&s);
  return rc;
}

static int parse_set (BUFFER *tmp, BUFFER *s, unsigned long data, BUFFER *err)
{
  int query, unset, inv, reset, r = 0;
  int idx = -1;
  char *p, scratch[_POSIX_PATH_MAX];
  char* myvar;

  while (MoreArgs (s))
  {
    /* reset state variables */
    query = 0;
    unset = data & M_SET_UNSET;
    inv = data & M_SET_INV;
    reset = data & M_SET_RESET;
    myvar = NULL;

    if (*s->dptr == '?')
    {
      query = 1;
      s->dptr++;
    }
    else if (mutt_strncmp ("no", s->dptr, 2) == 0)
    {
      s->dptr += 2;
      unset = !unset;
    }
    else if (mutt_strncmp ("inv", s->dptr, 3) == 0)
    {
      s->dptr += 3;
      inv = !inv;
    }
    else if (*s->dptr == '&')
    {
      reset = 1;
      s->dptr++;
    }

    /* get the variable name */
    mutt_extract_token (tmp, s, M_TOKEN_EQUAL);

    if (!mutt_strncmp ("my_", tmp->data, 3))
      myvar = tmp->data;
    else if ((idx = mutt_option_index (tmp->data)) == -1 &&
	!(reset && !mutt_strcmp ("all", tmp->data)))
    {
      snprintf (err->data, err->dsize, _("%s: unknown variable"), tmp->data);
      return (-1);
    }
    SKIPWS (s->dptr);

    if (reset)
    {
      if (query || unset || inv)
      {
	snprintf (err->data, err->dsize, _("prefix is illegal with reset"));
	return (-1);
      }

      if (s && *s->dptr == '=')
      {
	snprintf (err->data, err->dsize, _("value is illegal with reset"));
	return (-1);
      }
     
      if (!mutt_strcmp ("all", tmp->data))
      {
	if (CurrentMenu == MENU_PAGER)
	{
	  snprintf (err->data, err->dsize, _("Not available in this menu."));
	  return (-1);
	}
	for (idx = 0; MuttVars[idx].option; idx++)
	  mutt_restore_default (&MuttVars[idx]);
	set_option (OPTFORCEREDRAWINDEX);
	set_option (OPTFORCEREDRAWPAGER);
	set_option (OPTSORTSUBTHREADS);
	set_option (OPTNEEDRESORT);
	set_option (OPTRESORTINIT);
	set_option (OPTREDRAWTREE);
	return 0;
      }
      else
      {
	CHECK_PAGER;
        if (myvar)
          myvar_del (myvar);
        else
          mutt_restore_default (&MuttVars[idx]);
      }
    } 
    else if (!myvar && DTYPE (MuttVars[idx].type) == DT_BOOL)
    { 
      if (s && *s->dptr == '=')
      {
	if (unset || inv || query)
	{
	  snprintf (err->data, err->dsize, _("Usage: set variable=yes|no"));
	  return (-1);
	}

	s->dptr++;
	mutt_extract_token (tmp, s, 0);
	if (ascii_strcasecmp ("yes", tmp->data) == 0)
	  unset = inv = 0;
	else if (ascii_strcasecmp ("no", tmp->data) == 0)
	  unset = 1;
	else
	{
	  snprintf (err->data, err->dsize, _("Usage: set variable=yes|no"));
	  return (-1);
	}
      }

      if (query)
      {
	snprintf (err->data, err->dsize, option (MuttVars[idx].data)
			? _("%s is set") : _("%s is unset"), tmp->data);
	return 0;
      }

      CHECK_PAGER;
      if (unset)
	unset_option (MuttVars[idx].data);
      else if (inv)
	toggle_option (MuttVars[idx].data);
      else
	set_option (MuttVars[idx].data);
    }
    else if (myvar || DTYPE (MuttVars[idx].type) == DT_STR ||
	     DTYPE (MuttVars[idx].type) == DT_PATH ||
	     DTYPE (MuttVars[idx].type) == DT_ADDR)
    {
      if (unset)
      {
	CHECK_PAGER;
        if (myvar)
          myvar_del (myvar);
	else if (DTYPE (MuttVars[idx].type) == DT_ADDR)
	  rfc822_free_address ((ADDRESS **) MuttVars[idx].data);
	else
	  /* MuttVars[idx].data is already 'char**' (or some 'void**') or... 
	   * so cast to 'void*' is okay */
	  FREE ((void *) MuttVars[idx].data);		/* __FREE_CHECKED__ */
      }
      else if (query || *s->dptr != '=')
      {
	char _tmp[LONG_STRING];
	const char *val = NULL;

        if (myvar)
        {
          if ((val = myvar_get (myvar)))
          {
	    pretty_var (err->data, err->dsize, myvar, val);
            break;
          }
          else
          {
            snprintf (err->data, err->dsize, _("%s: unknown variable"), myvar);
            return (-1);
          }
        }
	else if (DTYPE (MuttVars[idx].type) == DT_ADDR)
	{
	  _tmp[0] = '\0';
	  rfc822_write_address (_tmp, sizeof (_tmp), *((ADDRESS **) MuttVars[idx].data), 0);
	  val = _tmp;
	}
	else if (DTYPE (MuttVars[idx].type) == DT_PATH)
	{
	  _tmp[0] = '\0';
	  strfcpy (_tmp, NONULL(*((char **) MuttVars[idx].data)), sizeof (_tmp));
	  mutt_pretty_mailbox (_tmp, sizeof (_tmp));
	  val = _tmp;
	}
	else
	  val = *((char **) MuttVars[idx].data);
	
	/* user requested the value of this variable */
	pretty_var (err->data, err->dsize, MuttVars[idx].option, NONULL(val));
	break;
      }
      else
      {
	CHECK_PAGER;
        s->dptr++;

        if (myvar)
	{
	  /* myvar is a pointer to tmp and will be lost on extract_token */
	  myvar = safe_strdup (myvar);
          myvar_del (myvar);
	}

        mutt_extract_token (tmp, s, 0);

        if (myvar)
        {
          myvar_set (myvar, tmp->data);
          FREE (&myvar);
	  myvar="don't resort";
        }
        else if (DTYPE (MuttVars[idx].type) == DT_PATH)
        {
	  /* MuttVars[idx].data is already 'char**' (or some 'void**') or... 
	   * so cast to 'void*' is okay */
	  FREE ((void *) MuttVars[idx].data);		/* __FREE_CHECKED__ */

	  strfcpy (scratch, tmp->data, sizeof (scratch));
	  mutt_expand_path (scratch, sizeof (scratch));
	  *((char **) MuttVars[idx].data) = safe_strdup (scratch);
        }
        else if (DTYPE (MuttVars[idx].type) == DT_STR)
        {
	  if (strstr (MuttVars[idx].option, "charset") &&
	      check_charset (&MuttVars[idx], tmp->data) < 0)
	  {
	    snprintf (err->data, err->dsize, _("Invalid value for option %s: \"%s\""),
		      MuttVars[idx].option, tmp->data);
	    return (-1);
	  }

	  FREE ((void *) MuttVars[idx].data);		/* __FREE_CHECKED__ */
	  *((char **) MuttVars[idx].data) = safe_strdup (tmp->data);
	  if (mutt_strcmp (MuttVars[idx].option, "charset") == 0)
	    mutt_set_charset (Charset);
        }
        else
        {
	  rfc822_free_address ((ADDRESS **) MuttVars[idx].data);
	  *((ADDRESS **) MuttVars[idx].data) = rfc822_parse_adrlist (NULL, tmp->data);
        }
      }
    }
    else if (DTYPE(MuttVars[idx].type) == DT_RX)
    {
      REGEXP *ptr = (REGEXP *) MuttVars[idx].data;
      regex_t *rx;
      int e, flags = 0;

      if (query || *s->dptr != '=')
      {
	/* user requested the value of this variable */
	pretty_var (err->data, err->dsize, MuttVars[idx].option, NONULL(ptr->pattern));
	break;
      }

      if (option(OPTATTACHMSG) && !mutt_strcmp(MuttVars[idx].option, "reply_regexp"))
      {
	snprintf (err->data, err->dsize, "Operation not permitted when in attach-message mode.");
	r = -1;
	break;
      }
      
      CHECK_PAGER;
      s->dptr++;

      /* copy the value of the string */
      mutt_extract_token (tmp, s, 0);

      if (!ptr->pattern || mutt_strcmp (ptr->pattern, tmp->data) != 0)
      {
	int not = 0;

	/* $mask is case-sensitive */
	if (mutt_strcmp (MuttVars[idx].option, "mask") != 0)
	  flags |= mutt_which_case (tmp->data);

	p = tmp->data;
	if (mutt_strcmp (MuttVars[idx].option, "mask") == 0)
	{
	  if (*p == '!')
	  {
	    not = 1;
	    p++;
	  }
	}
	  
	rx = (regex_t *) safe_malloc (sizeof (regex_t));
	if ((e = REGCOMP (rx, p, flags)) != 0)
	{
	  regerror (e, rx, err->data, err->dsize);
	  regfree (rx);
	  FREE (&rx);
	  break;
	}

	/* get here only if everything went smootly */
	if (ptr->pattern)
	{
	  FREE (&ptr->pattern);
	  regfree ((regex_t *) ptr->rx);
	  FREE (&ptr->rx);
	}

	ptr->pattern = safe_strdup (tmp->data);
	ptr->rx = rx;
	ptr->not = not;

	/* $reply_regexp and $alterantes require special treatment */
	
	if (Context && Context->msgcount &&
	    mutt_strcmp (MuttVars[idx].option, "reply_regexp") == 0)
	{
	  regmatch_t pmatch[1];
	  int i;
	  
#define CUR_ENV Context->hdrs[i]->env
	  for (i = 0; i < Context->msgcount; i++)
	  {
	    if (CUR_ENV && CUR_ENV->subject)
	    {
	      CUR_ENV->real_subj = (regexec (ReplyRegexp.rx,
				    CUR_ENV->subject, 1, pmatch, 0)) ?
				    CUR_ENV->subject : 
				    CUR_ENV->subject + pmatch[0].rm_eo;
	    }
	  }
#undef CUR_ENV
	}
      }
    }
    else if (DTYPE(MuttVars[idx].type) == DT_MAGIC)
    {
      if (query || *s->dptr != '=')
      {
	switch (DefaultMagic)
	{
	  case M_MBOX:
	    p = "mbox";
	    break;
	  case M_MMDF:
	    p = "MMDF";
	    break;
	  case M_MH:
	    p = "MH";
	    break;
	  case M_MAILDIR:
	    p = "Maildir";
	    break;
	  default:
	    p = "unknown";
	    break;
	}
	snprintf (err->data, err->dsize, "%s=%s", MuttVars[idx].option, p);
	break;
      }

      CHECK_PAGER;
      s->dptr++;

      /* copy the value of the string */
      mutt_extract_token (tmp, s, 0);
      if (mx_set_magic (tmp->data))
      {
	snprintf (err->data, err->dsize, _("%s: invalid mailbox type"), tmp->data);
	r = -1;
	break;
      }
    }
    else if (DTYPE(MuttVars[idx].type) == DT_NUM)
    {
      short *ptr = (short *) MuttVars[idx].data;
      short val;
      int rc;

      if (query || *s->dptr != '=')
      {
	val = *ptr;
	/* compatibility alias */
	if (mutt_strcmp (MuttVars[idx].option, "wrapmargin") == 0)
	  val = *ptr < 0 ? -*ptr : 0;

	/* user requested the value of this variable */
	snprintf (err->data, err->dsize, "%s=%d", MuttVars[idx].option, val);
	break;
      }

      CHECK_PAGER;
      s->dptr++;

      mutt_extract_token (tmp, s, 0);
      rc = mutt_atos (tmp->data, (short *) &val);

      if (rc < 0 || !*tmp->data)
      {
	snprintf (err->data, err->dsize, _("%s: invalid value (%s)"), tmp->data,
		  rc == -1 ? _("format error") : _("number overflow"));
	r = -1;
	break;
      }
      else
	*ptr = val;

      /* these ones need a sanity check */
      if (mutt_strcmp (MuttVars[idx].option, "history") == 0)
      {
	if (*ptr < 0)
	  *ptr = 0;
	mutt_init_history ();
      }
      else if (mutt_strcmp (MuttVars[idx].option, "pager_index_lines") == 0)
      {
	if (*ptr < 0)
	  *ptr = 0;
      }
      else if (mutt_strcmp (MuttVars[idx].option, "wrapmargin") == 0)
      {
	if (*ptr < 0)
	  *ptr = 0;
	else
	  *ptr = -*ptr;
      }
#ifdef USE_IMAP
      else if (mutt_strcmp (MuttVars[idx].option, "imap_pipeline_depth") == 0)
      {
        if (*ptr < 0)
          *ptr = 0;
      }
#endif
    }
    else if (DTYPE (MuttVars[idx].type) == DT_QUAD)
    {
      if (query)
      {
	char *vals[] = { "no", "yes", "ask-no", "ask-yes" };

	snprintf (err->data, err->dsize, "%s=%s", MuttVars[idx].option,
		  vals [ quadoption (MuttVars[idx].data) ]);
	break;
      }

      CHECK_PAGER;
      if (*s->dptr == '=')
      {
	s->dptr++;
	mutt_extract_token (tmp, s, 0);
	if (ascii_strcasecmp ("yes", tmp->data) == 0)
	  set_quadoption (MuttVars[idx].data, M_YES);
	else if (ascii_strcasecmp ("no", tmp->data) == 0)
	  set_quadoption (MuttVars[idx].data, M_NO);
	else if (ascii_strcasecmp ("ask-yes", tmp->data) == 0)
	  set_quadoption (MuttVars[idx].data, M_ASKYES);
	else if (ascii_strcasecmp ("ask-no", tmp->data) == 0)
	  set_quadoption (MuttVars[idx].data, M_ASKNO);
	else
	{
	  snprintf (err->data, err->dsize, _("%s: invalid value"), tmp->data);
	  r = -1;
	  break;
	}
      }
      else
      {
	if (inv)
	  toggle_quadoption (MuttVars[idx].data);
	else if (unset)
	  set_quadoption (MuttVars[idx].data, M_NO);
	else
	  set_quadoption (MuttVars[idx].data, M_YES);
      }
    }
    else if (DTYPE (MuttVars[idx].type) == DT_SORT)
    {
      const struct mapping_t *map = NULL;

      switch (MuttVars[idx].type & DT_SUBTYPE_MASK)
      {
	case DT_SORT_ALIAS:
	  map = SortAliasMethods;
	  break;
	case DT_SORT_BROWSER:
	  map = SortBrowserMethods;
	  break;
	case DT_SORT_KEYS:
          if ((WithCrypto & APPLICATION_PGP))
            map = SortKeyMethods;
	  break;
	case DT_SORT_AUX:
	  map = SortAuxMethods;
	  break;
	default:
	  map = SortMethods;
	  break;
      }

      if (!map)
      {
	snprintf (err->data, err->dsize, _("%s: Unknown type."), MuttVars[idx].option);
	r = -1;
	break;
      }
      
      if (query || *s->dptr != '=')
      {
	p = mutt_getnamebyvalue (*((short *) MuttVars[idx].data) & SORT_MASK, map);

	snprintf (err->data, err->dsize, "%s=%s%s%s", MuttVars[idx].option,
		  (*((short *) MuttVars[idx].data) & SORT_REVERSE) ? "reverse-" : "",
		  (*((short *) MuttVars[idx].data) & SORT_LAST) ? "last-" : "",
		  p);
	return 0;
      }
      CHECK_PAGER;
      s->dptr++;
      mutt_extract_token (tmp, s , 0);

      if (parse_sort ((short *) MuttVars[idx].data, tmp->data, map, err) == -1)
      {
	r = -1;
	break;
      }
    }
    else
    {
      snprintf (err->data, err->dsize, _("%s: unknown type"), MuttVars[idx].option);
      r = -1;
      break;
    }

    if (!myvar)
    {
      if (MuttVars[idx].flags & R_INDEX)
        set_option (OPTFORCEREDRAWINDEX);
      if (MuttVars[idx].flags & R_PAGER)
        set_option (OPTFORCEREDRAWPAGER);
      if (MuttVars[idx].flags & R_RESORT_SUB)
        set_option (OPTSORTSUBTHREADS);
      if (MuttVars[idx].flags & R_RESORT)
        set_option (OPTNEEDRESORT);
      if (MuttVars[idx].flags & R_RESORT_INIT)
        set_option (OPTRESORTINIT);
      if (MuttVars[idx].flags & R_TREE)
        set_option (OPTREDRAWTREE);
    }
  }
  return (r);
}

#define MAXERRS 128

/* reads the specified initialization file.  returns -1 if errors were found
   so that we can pause to let the user know...  */
static int source_rc (const char *rcfile, BUFFER *err)
{
  FILE *f;
  int line = 0, rc = 0, conv = 0;
  BUFFER token;
  char *linebuf = NULL;
  char *currentline = NULL;
  size_t buflen;
  pid_t pid;

  dprint (2, (debugfile, "Reading configuration file '%s'.\n",
	  rcfile));
  
  if ((f = mutt_open_read (rcfile, &pid)) == NULL)
  {
    snprintf (err->data, err->dsize, "%s: %s", rcfile, strerror (errno));
    return (-1);
  }

  memset (&token, 0, sizeof (token));
  while ((linebuf = mutt_read_line (linebuf, &buflen, f, &line, M_CONT)) != NULL)
  {
    conv=ConfigCharset && (*ConfigCharset) && Charset;
    if (conv) 
    {
      currentline=safe_strdup(linebuf);
      if (!currentline) continue;
      mutt_convert_string(&currentline, ConfigCharset, Charset, 0);
    } 
    else 
      currentline=linebuf;

    if (mutt_parse_rc_line (currentline, &token, err) == -1)
    {
      mutt_error (_("Error in %s, line %d: %s"), rcfile, line, err->data);
      if (--rc < -MAXERRS) 
      {
        if (conv) FREE(&currentline);
        break;
      }
    }
    else
    {
      if (rc < 0)
        rc = -1;
    }
    if (conv) 
      FREE(&currentline);
  }
  FREE (&token.data);
  FREE (&linebuf);
  safe_fclose (&f);
  if (pid != -1)
    mutt_wait_filter (pid);
  if (rc)
  {
    /* the muttrc source keyword */
    snprintf (err->data, err->dsize, rc >= -MAXERRS ? _("source: errors in %s")
      : _("source: reading aborted due too many errors in %s"), rcfile);
    rc = -1;
  }
  return (rc);
}

#undef MAXERRS

static int parse_source (BUFFER *tmp, BUFFER *s, unsigned long data, BUFFER *err)
{
  char path[_POSIX_PATH_MAX];

  if (mutt_extract_token (tmp, s, 0) != 0)
  {
    snprintf (err->data, err->dsize, _("source: error at %s"), s->dptr);
    return (-1);
  }
  if (MoreArgs (s))
  {
    strfcpy (err->data, _("source: too many arguments"), err->dsize);
    return (-1);
  }
  strfcpy (path, tmp->data, sizeof (path));
  mutt_expand_path (path, sizeof (path));
  return (source_rc (path, err));
}

/* line		command to execute

   token	scratch buffer to be used by parser.  caller should free
   		token->data when finished.  the reason for this variable is
		to avoid having to allocate and deallocate a lot of memory
		if we are parsing many lines.  the caller can pass in the
		memory to use, which avoids having to create new space for
		every call to this function.

   err		where to write error messages */
int mutt_parse_rc_line (/* const */ char *line, BUFFER *token, BUFFER *err)
{
  int i, r = -1;
  BUFFER expn;

  if (!line || !*line)
    return 0;

  memset (&expn, 0, sizeof (expn));
  expn.data = expn.dptr = line;
  expn.dsize = mutt_strlen (line);

  *err->data = 0;

  SKIPWS (expn.dptr);
  while (*expn.dptr)
  {
    if (*expn.dptr == '#')
      break; /* rest of line is a comment */
    if (*expn.dptr == ';')
    {
      expn.dptr++;
      continue;
    }
    mutt_extract_token (token, &expn, 0);
    for (i = 0; Commands[i].name; i++)
    {
      if (!mutt_strcmp (token->data, Commands[i].name))
      {
	if (Commands[i].func (token, &expn, Commands[i].data, err) != 0)
	  goto finish;
        break;
      }
    }
    if (!Commands[i].name)
    {
      snprintf (err->data, err->dsize, _("%s: unknown command"), NONULL (token->data));
      goto finish;
    }
  }
  r = 0;
finish:
  if (expn.destroy)
    FREE (&expn.data);
  return (r);
}


#define NUMVARS (sizeof (MuttVars)/sizeof (MuttVars[0]))
#define NUMCOMMANDS (sizeof (Commands)/sizeof (Commands[0]))
/* initial string that starts completion. No telling how much crap 
 * the user has typed so far. Allocate LONG_STRING just to be sure! */
static char User_typed [LONG_STRING] = {0}; 

static int  Num_matched = 0; /* Number of matches for completion */
static char Completed [STRING] = {0}; /* completed string (command or variable) */
static const char **Matches;
/* this is a lie until mutt_init runs: */
static int  Matches_listsize = MAX(NUMVARS,NUMCOMMANDS) + 10;

static void matches_ensure_morespace(int current)
{
  int base_space, extra_space, space;

  if (current > Matches_listsize - 2)
  {
    base_space = MAX(NUMVARS,NUMCOMMANDS) + 1; 
    extra_space = Matches_listsize - base_space;
    extra_space *= 2;
    space = base_space + extra_space;
    safe_realloc (&Matches, space * sizeof (char *));
    memset (&Matches[current + 1], 0, space - current);
    Matches_listsize = space;
  }
}

/* helper function for completion.  Changes the dest buffer if
   necessary/possible to aid completion.
	dest == completion result gets here.
	src == candidate for completion.
	try == user entered data for completion.
	len == length of dest buffer.
*/
static void candidate (char *dest, char *try, const char *src, int len)
{
  int l;

  if (strstr (src, try) == src)
  {
    matches_ensure_morespace (Num_matched);
    Matches[Num_matched++] = src;
    if (dest[0] == 0)
      strfcpy (dest, src, len);
    else
    {
      for (l = 0; src[l] && src[l] == dest[l]; l++);
      dest[l] = 0;
    }
  }
}

int mutt_command_complete (char *buffer, size_t len, int pos, int numtabs)
{
  char *pt = buffer;
  int num;
  int spaces; /* keep track of the number of leading spaces on the line */
  myvar_t *myv;

  SKIPWS (buffer);
  spaces = buffer - pt;

  pt = buffer + pos - spaces;
  while ((pt > buffer) && !isspace ((unsigned char) *pt))
    pt--;

  if (pt == buffer) /* complete cmd */
  {
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      Num_matched = 0;
      strfcpy (User_typed, pt, sizeof (User_typed));
      memset (Matches, 0, Matches_listsize);
      memset (Completed, 0, sizeof (Completed));
      for (num = 0; Commands[num].name; num++)
	candidate (Completed, User_typed, Commands[num].name, sizeof (Completed));
      matches_ensure_morespace (Num_matched);
      Matches[Num_matched++] = User_typed;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. dont change 'buffer'. Fake successful return this time */
      if (User_typed[0] == 0)
	return 1;
    }

    if (Completed[0] == 0 && User_typed[0])
      return 0;

     /* Num_matched will _always_ be atleast 1 since the initial
      * user-typed string is always stored */
    if (numtabs == 1 && Num_matched == 2)
      snprintf(Completed, sizeof(Completed),"%s", Matches[0]);
    else if (numtabs > 1 && Num_matched > 2)
      /* cycle thru all the matches */
      snprintf(Completed, sizeof(Completed), "%s", 
	       Matches[(numtabs - 2) % Num_matched]);

    /* return the completed command */
    strncpy (buffer, Completed, len - spaces);
  }
  else if (!mutt_strncmp (buffer, "set", 3)
	   || !mutt_strncmp (buffer, "unset", 5)
	   || !mutt_strncmp (buffer, "reset", 5)
	   || !mutt_strncmp (buffer, "toggle", 6))
  { 		/* complete variables */
    char *prefixes[] = { "no", "inv", "?", "&", 0 };
    
    pt++;
    /* loop through all the possible prefixes (no, inv, ...) */
    if (!mutt_strncmp (buffer, "set", 3))
    {
      for (num = 0; prefixes[num]; num++)
      {
	if (!mutt_strncmp (pt, prefixes[num], mutt_strlen (prefixes[num])))
	{
	  pt += mutt_strlen (prefixes[num]);
	  break;
	}
      }
    }
    
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      Num_matched = 0;
      strfcpy (User_typed, pt, sizeof (User_typed));
      memset (Matches, 0, Matches_listsize);
      memset (Completed, 0, sizeof (Completed));
      for (num = 0; MuttVars[num].option; num++)
	candidate (Completed, User_typed, MuttVars[num].option, sizeof (Completed));
      for (myv = MyVars; myv; myv = myv->next)
	candidate (Completed, User_typed, myv->name, sizeof (Completed));
      matches_ensure_morespace (Num_matched);
      Matches[Num_matched++] = User_typed;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. dont change 'buffer'. Fake successful return this time */
      if (User_typed[0] == 0)
	return 1;
    }

    if (Completed[0] == 0 && User_typed[0])
      return 0;

    /* Num_matched will _always_ be atleast 1 since the initial
     * user-typed string is always stored */
    if (numtabs == 1 && Num_matched == 2)
      snprintf(Completed, sizeof(Completed),"%s", Matches[0]);
    else if (numtabs > 1 && Num_matched > 2)
    /* cycle thru all the matches */
      snprintf(Completed, sizeof(Completed), "%s", 
	       Matches[(numtabs - 2) % Num_matched]);

    strncpy (pt, Completed, buffer + len - pt - spaces);
  }
  else if (!mutt_strncmp (buffer, "exec", 4))
  {
    struct binding_t *menu = km_get_table (CurrentMenu);

    if (!menu && CurrentMenu != MENU_PAGER)
      menu = OpGeneric;
    
    pt++;
    /* first TAB. Collect all the matches */
    if (numtabs == 1)
    {
      Num_matched = 0;
      strfcpy (User_typed, pt, sizeof (User_typed));
      memset (Matches, 0, Matches_listsize);
      memset (Completed, 0, sizeof (Completed));
      for (num = 0; menu[num].name; num++)
	candidate (Completed, User_typed, menu[num].name, sizeof (Completed));
      /* try the generic menu */
      if (Completed[0] == 0 && CurrentMenu != MENU_PAGER) 
      {
	menu = OpGeneric;
	for (num = 0; menu[num].name; num++)
	  candidate (Completed, User_typed, menu[num].name, sizeof (Completed));
      }
      matches_ensure_morespace (Num_matched);
      Matches[Num_matched++] = User_typed;

      /* All matches are stored. Longest non-ambiguous string is ""
       * i.e. dont change 'buffer'. Fake successful return this time */
      if (User_typed[0] == 0)
	return 1;
    }

    if (Completed[0] == 0 && User_typed[0])
      return 0;

    /* Num_matched will _always_ be atleast 1 since the initial
     * user-typed string is always stored */
    if (numtabs == 1 && Num_matched == 2)
      snprintf(Completed, sizeof(Completed),"%s", Matches[0]);
    else if (numtabs > 1 && Num_matched > 2)
    /* cycle thru all the matches */
      snprintf(Completed, sizeof(Completed), "%s", 
	       Matches[(numtabs - 2) % Num_matched]);

    strncpy (pt, Completed, buffer + len - pt - spaces);
  }
  else
    return 0;

  return 1;
}

int mutt_var_value_complete (char *buffer, size_t len, int pos)
{
  char var[STRING], *pt = buffer;
  int spaces;
  
  if (buffer[0] == 0)
    return 0;

  SKIPWS (buffer);
  spaces = buffer - pt;

  pt = buffer + pos - spaces;
  while ((pt > buffer) && !isspace ((unsigned char) *pt))
    pt--;
  pt++; /* move past the space */
  if (*pt == '=') /* abort if no var before the '=' */
    return 0;

  if (mutt_strncmp (buffer, "set", 3) == 0)
  {
    int idx;
    char val[LONG_STRING];
    const char *myvarval;

    strfcpy (var, pt, sizeof (var));
    /* ignore the trailing '=' when comparing */
    var[mutt_strlen (var) - 1] = 0;
    if ((idx = mutt_option_index (var)) == -1)
    {
      if ((myvarval = myvar_get(var)) != NULL)
      {
	pretty_var (pt, len - (pt - buffer), var, myvarval);
	return 1;
      }
      return 0; /* no such variable. */
    }
    else if (var_to_string (idx, val, sizeof (val)))
    {
      snprintf (pt, len - (pt - buffer), "%s=\"%s\"", var, val);
      return 1;
    }
  }
  return 0;
}

static int var_to_string (int idx, char* val, size_t len)
{
  char tmp[LONG_STRING];
  char *vals[] = { "no", "yes", "ask-no", "ask-yes" };

  tmp[0] = '\0';

  if ((DTYPE(MuttVars[idx].type) == DT_STR) ||
      (DTYPE(MuttVars[idx].type) == DT_PATH) ||
      (DTYPE(MuttVars[idx].type) == DT_RX))
  {
    strfcpy (tmp, NONULL (*((char **) MuttVars[idx].data)), sizeof (tmp));
    if (DTYPE (MuttVars[idx].type) == DT_PATH)
      mutt_pretty_mailbox (tmp, sizeof (tmp));
  }
  else if (DTYPE (MuttVars[idx].type) == DT_ADDR)
  {
    rfc822_write_address (tmp, sizeof (tmp), *((ADDRESS **) MuttVars[idx].data), 0);
  }
  else if (DTYPE (MuttVars[idx].type) == DT_QUAD)
    strfcpy (tmp, vals[quadoption (MuttVars[idx].data)], sizeof (tmp));
  else if (DTYPE (MuttVars[idx].type) == DT_NUM)
  {
    short sval = *((short *) MuttVars[idx].data);

    /* avert your eyes, gentle reader */
    if (mutt_strcmp (MuttVars[idx].option, "wrapmargin") == 0)
      sval = sval > 0 ? 0 : -sval;

    snprintf (tmp, sizeof (tmp), "%d", sval);
  }
  else if (DTYPE (MuttVars[idx].type) == DT_SORT)
  {
    const struct mapping_t *map;
    char *p;

    switch (MuttVars[idx].type & DT_SUBTYPE_MASK)
    {
      case DT_SORT_ALIAS:
        map = SortAliasMethods;
        break;
      case DT_SORT_BROWSER:
        map = SortBrowserMethods;
        break;
      case DT_SORT_KEYS:
        if ((WithCrypto & APPLICATION_PGP))
          map = SortKeyMethods;
        else
          map = SortMethods;
        break;
      default:
        map = SortMethods;
        break;
    }
    p = mutt_getnamebyvalue (*((short *) MuttVars[idx].data) & SORT_MASK, map);
    snprintf (tmp, sizeof (tmp), "%s%s%s",
              (*((short *) MuttVars[idx].data) & SORT_REVERSE) ? "reverse-" : "",
              (*((short *) MuttVars[idx].data) & SORT_LAST) ? "last-" : "",
              p);
  }
  else if (DTYPE (MuttVars[idx].type) == DT_MAGIC)
  {
    char *p;

    switch (DefaultMagic)
    {
      case M_MBOX:
        p = "mbox";
        break;
      case M_MMDF:
        p = "MMDF";
        break;
      case M_MH:
        p = "MH";
        break;
      case M_MAILDIR:
        p = "Maildir";
        break;
      default:
        p = "unknown";
    }
    strfcpy (tmp, p, sizeof (tmp));
  }
  else if (DTYPE (MuttVars[idx].type) == DT_BOOL)
    strfcpy (tmp, option (MuttVars[idx].data) ? "yes" : "no", sizeof (tmp));
  else
    return 0;

  escape_string (val, len - 1, tmp);

  return 1;
}

/* Implement the -Q command line flag */
int mutt_query_variables (LIST *queries)
{
  LIST *p;
  
  char errbuff[LONG_STRING];
  char command[STRING];
  
  BUFFER err, token;
  
  memset (&err, 0, sizeof (err));
  memset (&token, 0, sizeof (token));
  
  err.data = errbuff;
  err.dsize = sizeof (errbuff);
  
  for (p = queries; p; p = p->next)
  {
    snprintf (command, sizeof (command), "set ?%s\n", p->data);
    if (mutt_parse_rc_line (command, &token, &err) == -1)
    {
      fprintf (stderr, "%s\n", err.data);
      FREE (&token.data);
      return 1;
    }
    printf ("%s\n", err.data);
  }
  
  FREE (&token.data);
  return 0;
}

/* dump out the value of all the variables we have */
int mutt_dump_variables (void)
{
  int i;
  
  char errbuff[LONG_STRING];
  char command[STRING];
  
  BUFFER err, token;
  
  memset (&err, 0, sizeof (err));
  memset (&token, 0, sizeof (token));
  
  err.data = errbuff;
  err.dsize = sizeof (errbuff);
  
  for (i = 0; MuttVars[i].option; i++)
  {
    if (MuttVars[i].type == DT_SYN)
      continue;

    snprintf (command, sizeof (command), "set ?%s\n", MuttVars[i].option);
    if (mutt_parse_rc_line (command, &token, &err) == -1)
    {
      fprintf (stderr, "%s\n", err.data);
      FREE (&token.data);
      return 1;
    }
    printf("%s\n", err.data);
  }
  
  FREE (&token.data);
  return 0;
}

char *mutt_getnamebyvalue (int val, const struct mapping_t *map)
{
  int i;

  for (i=0; map[i].name; i++)
    if (map[i].value == val)
      return (map[i].name);
  return NULL;
}

int mutt_getvaluebyname (const char *name, const struct mapping_t *map)
{
  int i;

  for (i = 0; map[i].name; i++)
    if (ascii_strcasecmp (map[i].name, name) == 0)
      return (map[i].value);
  return (-1);
}

#ifdef DEBUG
static void start_debug (void)
{
  time_t t;
  int i;
  char buf[_POSIX_PATH_MAX];
  char buf2[_POSIX_PATH_MAX];

  /* rotate the old debug logs */
  for (i=3; i>=0; i--)
  {
    snprintf (buf, sizeof(buf), "%s/.muttdebug%d", NONULL(Homedir), i);
    snprintf (buf2, sizeof(buf2), "%s/.muttdebug%d", NONULL(Homedir), i+1);
    rename (buf, buf2);
  }
  if ((debugfile = safe_fopen(buf, "w")) != NULL)
  {
    t = time (0);
    setbuf (debugfile, NULL); /* don't buffer the debugging output! */
    fprintf (debugfile, "Mutt %s started at %s.\nDebugging at level %d.\n\n",
	     MUTT_VERSION, asctime (localtime (&t)), debuglevel);
  }
}
#endif

static int mutt_execute_commands (LIST *p)
{
  BUFFER err, token;
  char errstr[SHORT_STRING];

  memset (&err, 0, sizeof (err));
  err.data = errstr;
  err.dsize = sizeof (errstr);
  memset (&token, 0, sizeof (token));
  for (; p; p = p->next)
  {
    if (mutt_parse_rc_line (p->data, &token, &err) != 0)
    {
      fprintf (stderr, _("Error in command line: %s\n"), err.data);
      FREE (&token.data);
      return (-1);
    }
  }
  FREE (&token.data);
  return 0;
}

void mutt_init (int skip_sys_rc, LIST *commands)
{
  struct passwd *pw;
  struct utsname utsname;
  char *p, buffer[STRING], error[STRING];
  int i, default_rc = 0, need_pause = 0;
  BUFFER err;

  memset (&err, 0, sizeof (err));
  err.data = error;
  err.dsize = sizeof (error);

  Groups = hash_create (1031, 0);
  ReverseAlias = hash_create (1031, 1);
  
  mutt_menu_init ();

  /* 
   * XXX - use something even more difficult to predict?
   */
  snprintf (AttachmentMarker, sizeof (AttachmentMarker),
	    "\033]9;%ld\a", (long) time (NULL));
  
  /* on one of the systems I use, getcwd() does not return the same prefix
     as is listed in the passwd file */
  if ((p = getenv ("HOME")))
    Homedir = safe_strdup (p);

  /* Get some information about the user */
  if ((pw = getpwuid (getuid ())))
  {
    char rnbuf[STRING];

    Username = safe_strdup (pw->pw_name);
    if (!Homedir)
      Homedir = safe_strdup (pw->pw_dir);

    Realname = safe_strdup (mutt_gecos_name (rnbuf, sizeof (rnbuf), pw));
    Shell = safe_strdup (pw->pw_shell);
    endpwent ();
  }
  else 
  {
    if (!Homedir)
    {
      mutt_endwin (NULL);
      fputs (_("unable to determine home directory"), stderr);
      exit (1);
    }
    if ((p = getenv ("USER")))
      Username = safe_strdup (p);
    else
    {
      mutt_endwin (NULL);
      fputs (_("unable to determine username"), stderr);
      exit (1);
    }
    Shell = safe_strdup ((p = getenv ("SHELL")) ? p : "/bin/sh");
  }

#ifdef DEBUG
  /* Start up debugging mode if requested */
  if (debuglevel > 0)
    start_debug ();
#endif

  /* And about the host... */
  uname (&utsname);
  /* some systems report the FQDN instead of just the hostname */
  if ((p = strchr (utsname.nodename, '.')))
  {
    Hostname = mutt_substrdup (utsname.nodename, p);
    p++;
    strfcpy (buffer, p, sizeof (buffer)); /* save the domain for below */
  }
  else
    Hostname = safe_strdup (utsname.nodename);

#ifndef DOMAIN
#define DOMAIN buffer
  if (!p && getdnsdomainname (buffer, sizeof (buffer)) == -1)
    Fqdn = safe_strdup ("@");
  else
#endif /* DOMAIN */
    if (*DOMAIN != '@')
  {
    Fqdn = safe_malloc (mutt_strlen (DOMAIN) + mutt_strlen (Hostname) + 2);
    sprintf (Fqdn, "%s.%s", NONULL(Hostname), DOMAIN);	/* __SPRINTF_CHECKED__ */
  }
  else
    Fqdn = safe_strdup(NONULL(Hostname));

  if ((p = getenv ("MAIL")))
    Spoolfile = safe_strdup (p);
  else if ((p = getenv ("MAILDIR")))
    Spoolfile = safe_strdup (p);
  else
  {
#ifdef HOMESPOOL
    mutt_concat_path (buffer, NONULL (Homedir), MAILPATH, sizeof (buffer));
#else
    mutt_concat_path (buffer, MAILPATH, NONULL(Username), sizeof (buffer));
#endif
    Spoolfile = safe_strdup (buffer);
  }

  if ((p = getenv ("MAILCAPS")))
    MailcapPath = safe_strdup (p);
  else
  {
    /* Default search path from RFC1524 */
    MailcapPath = safe_strdup ("~/.mailcap:" PKGDATADIR "/mailcap:" SYSCONFDIR "/mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap");
  }

  Tempdir = safe_strdup ((p = getenv ("TMPDIR")) ? p : "/tmp");

  p = getenv ("VISUAL");
  if (!p)
  {
    p = getenv ("EDITOR");
    if (!p)
      p = "vi";
  }
  Editor = safe_strdup (p);
  Visual = safe_strdup (p);

  if ((p = getenv ("REPLYTO")) != NULL)
  {
    BUFFER buf, token;

    snprintf (buffer, sizeof (buffer), "Reply-To: %s", p);

    memset (&buf, 0, sizeof (buf));
    buf.data = buf.dptr = buffer;
    buf.dsize = mutt_strlen (buffer);

    memset (&token, 0, sizeof (token));
    parse_my_hdr (&token, &buf, 0, &err);
    FREE (&token.data);
  }

  if ((p = getenv ("EMAIL")) != NULL)
    From = rfc822_parse_adrlist (NULL, p);
  
  mutt_set_langinfo_charset ();
  mutt_set_charset (Charset);
  
  Matches = safe_calloc (Matches_listsize, sizeof (char *));
  
  /* Set standard defaults */
  for (i = 0; MuttVars[i].option; i++)
  {
    mutt_set_default (&MuttVars[i]);
    mutt_restore_default (&MuttVars[i]);
  }

  CurrentMenu = MENU_MAIN;


#ifndef LOCALES_HACK
  /* Do we have a locale definition? */
  if (((p = getenv ("LC_ALL")) != NULL && p[0]) ||
      ((p = getenv ("LANG")) != NULL && p[0]) ||
      ((p = getenv ("LC_CTYPE")) != NULL && p[0]))
    set_option (OPTLOCALES);
#endif

#ifdef HAVE_GETSID
  /* Unset suspend by default if we're the session leader */
  if (getsid(0) == getpid())
    unset_option (OPTSUSPEND);
#endif

  mutt_init_history ();

  
  
  
  /*
   * 
   *			   BIG FAT WARNING
   * 
   * When changing the code which looks for a configuration file,
   * please also change the corresponding code in muttbug.sh.in.
   * 
   * 
   */
  
  
  
  
  if (!Muttrc)
  {
    snprintf (buffer, sizeof(buffer), "%s/.muttrc-%s", NONULL(Homedir), MUTT_VERSION);
    if (access(buffer, F_OK) == -1)
      snprintf (buffer, sizeof(buffer), "%s/.muttrc", NONULL(Homedir));
    if (access(buffer, F_OK) == -1)
      snprintf (buffer, sizeof (buffer), "%s/.mutt/muttrc-%s", NONULL(Homedir), MUTT_VERSION);
    if (access(buffer, F_OK) == -1)
      snprintf (buffer, sizeof (buffer), "%s/.mutt/muttrc", NONULL(Homedir));
    if (access(buffer, F_OK) == -1) /* default to .muttrc for alias_file */
      snprintf (buffer, sizeof(buffer), "%s/.muttrc", NONULL(Homedir));

    default_rc = 1;
    Muttrc = safe_strdup (buffer);
  }
  else
  {
    strfcpy (buffer, Muttrc, sizeof (buffer));
    FREE (&Muttrc);
    mutt_expand_path (buffer, sizeof (buffer));
    Muttrc = safe_strdup (buffer);
  }
  FREE (&AliasFile);
  AliasFile = safe_strdup (NONULL(Muttrc));

  /* Process the global rc file if it exists and the user hasn't explicity
     requested not to via "-n".  */
  if (!skip_sys_rc)
  {
    snprintf (buffer, sizeof(buffer), "%s/Muttrc-%s", SYSCONFDIR, MUTT_VERSION);
    if (access (buffer, F_OK) == -1)
      snprintf (buffer, sizeof(buffer), "%s/Muttrc", SYSCONFDIR);
    if (access (buffer, F_OK) == -1)
      snprintf (buffer, sizeof (buffer), "%s/Muttrc-%s", PKGDATADIR, MUTT_VERSION);
    if (access (buffer, F_OK) == -1)
      snprintf (buffer, sizeof (buffer), "%s/Muttrc", PKGDATADIR);
    if (access (buffer, F_OK) != -1)
    {
      if (source_rc (buffer, &err) != 0)
      {
	fputs (err.data, stderr);
	fputc ('\n', stderr);
	need_pause = 1;
      }
    }
  }

  /* Read the user's initialization file.  */
  if (access (Muttrc, F_OK) != -1)
  {
    if (!option (OPTNOCURSES))
      endwin ();
    if (source_rc (Muttrc, &err) != 0)
    {
      fputs (err.data, stderr);
      fputc ('\n', stderr);
      need_pause = 1;
    }
  }
  else if (!default_rc)
  {
    /* file specified by -F does not exist */
    snprintf (buffer, sizeof (buffer), "%s: %s", Muttrc, strerror (errno));
    mutt_endwin (buffer);
    exit (1);
  }

  if (mutt_execute_commands (commands) != 0)
    need_pause = 1;

  if (need_pause && !option (OPTNOCURSES))
  {
    if (mutt_any_key_to_continue (NULL) == -1)
      mutt_exit(1);
  }

  mutt_read_histfile ();

#if 0
  set_option (OPTWEED); /* turn weeding on by default */
#endif
}

int mutt_get_hook_type (const char *name)
{
  struct command_t *c;

  for (c = Commands ; c->name ; c++)
    if (c->func == mutt_parse_hook && ascii_strcasecmp (c->name, name) == 0)
      return c->data;
  return 0;
}

static int parse_group_context (group_context_t **ctx, BUFFER *buf, BUFFER *s, unsigned long data, BUFFER *err)
{
  while (!mutt_strcasecmp (buf->data, "-group"))
  {
    if (!MoreArgs (s)) 
    {
      strfcpy (err->data, _("-group: no group name"), err->dsize);
      goto bail;
    }
    
    mutt_extract_token (buf, s, 0);

    mutt_group_context_add (ctx, mutt_pattern_group (buf->data));
    
    if (!MoreArgs (s))
    {
      strfcpy (err->data, _("out of arguments"), err->dsize);
      goto bail;
    }
    
    mutt_extract_token (buf, s, 0);
  }
  
  return 0;
  
  bail:
  mutt_group_context_destroy (ctx);
  return -1;
}

static void myvar_set (const char* var, const char* val)
{
  myvar_t** cur;

  for (cur = &MyVars; *cur; cur = &((*cur)->next))
    if (!mutt_strcmp ((*cur)->name, var))
      break;

  if (!*cur)
    *cur = safe_calloc (1, sizeof (myvar_t));
  
  if (!(*cur)->name)
    (*cur)->name = safe_strdup (var);
  
  mutt_str_replace (&(*cur)->value, val);
}

static void myvar_del (const char* var)
{
  myvar_t **cur;
  myvar_t *tmp;
  

  for (cur = &MyVars; *cur; cur = &((*cur)->next))
    if (!mutt_strcmp ((*cur)->name, var))
      break;
  
  if (*cur) 
  {
    tmp = (*cur)->next;
    FREE (&(*cur)->name);
    FREE (&(*cur)->value);
    FREE (cur);		/* __FREE_CHECKED__ */
    *cur = tmp;
  }
}

static const char* myvar_get (const char* var)
{
  myvar_t* cur;

  for (cur = MyVars; cur; cur = cur->next)
    if (!mutt_strcmp (cur->name, var))
      return NONULL(cur->value);

  return NULL;
}
