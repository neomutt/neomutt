/*
 * Copyright (C) 1996-2000,2002,2010,2012-2013 Michael R. Elkins <me@mutt.org>
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
#include "mx.h"
#include "mutt_menu.h"
#include "mailbox.h"
#include "mapping.h"
#include "sort.h"
#include "buffy.h"
#include "mx.h"

#ifdef USE_SIDEBAR
#include "sidebar.h"
#endif

#ifdef USE_POP
#include "pop.h"
#endif

#ifdef USE_IMAP
#include "imap_private.h"
#endif

#ifdef USE_NOTMUCH
#include "mutt_notmuch.h"
#endif

#include "mutt_crypt.h"

#ifdef USE_NNTP
#include "nntp.h"
#endif


#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#include <assert.h>

static const char *No_mailbox_is_open = N_("No mailbox is open.");
static const char *There_are_no_messages = N_("There are no messages.");
static const char *Mailbox_is_read_only = N_("Mailbox is read-only.");
static const char *Function_not_permitted_in_attach_message_mode = N_("Function not permitted in attach-message mode.");
static const char *No_visible = N_("No visible messages.");

#define CHECK_IN_MAILBOX if (!Context) \
	{ \
		mutt_flushinp (); \
		mutt_error _(No_mailbox_is_open); \
		break; \
	}

#define CHECK_MSGCOUNT if (!Context) \
	{ \
	  	mutt_flushinp (); \
		mutt_error _(No_mailbox_is_open); \
		break; \
	} \
	else if (!Context->msgcount) \
	{ \
	  	mutt_flushinp (); \
		mutt_error _(There_are_no_messages); \
		break; \
	}

#define CHECK_VISIBLE if (Context && menu->current >= Context->vcount) \
  	{\
	  	mutt_flushinp (); \
	  	mutt_error _(No_visible); \
	  	break; \
	}


#define CHECK_READONLY if (Context->readonly) \
			{ \
			  	mutt_flushinp (); \
				mutt_error _(Mailbox_is_read_only); \
				break; \
			}

#define CHECK_ACL(aclbit,action) \
		if (!mutt_bit_isset(Context->rights,aclbit)) { \
			mutt_flushinp(); \
        /* L10N: %s is one of the CHECK_ACL entries below. */ \
			mutt_error (_("%s: Operation not permitted by ACL"), action); \
			break; \
		}

#define CHECK_ATTACH if(option(OPTATTACHMSG)) \
		     {\
			mutt_flushinp (); \
			mutt_error _(Function_not_permitted_in_attach_message_mode); \
			break; \
		     }

#define CURHDR Context->hdrs[Context->v2r[menu->current]]
#define OLDHDR Context->hdrs[Context->v2r[menu->oldcurrent]]
#define UNREAD(h) mutt_thread_contains_unread (Context, h)

/* de facto standard escapes for tsl/fsl */
static char *tsl = "\033]0;";
static char *fsl = "\007";

/**
 * collapse/uncollapse all threads
 * @param menu   current menu
 * @param toggle toggle collapsed state
 *
 * This function is called by the OP_MAIN_COLLAPSE_ALL command and on folder
 * enter if the OPTCOLLAPSEALL option is set. In the first case, the @toggle
 * parameter is 1 to actually toggle collapsed/uncollapsed state on all
 * threads. In the second case, the @toggle parameter is 0, actually turning
 * this function into a one-way collapse.
 */
static void collapse_all(MUTTMENU *menu, int toggle)
{
  HEADER *h, *base;
  THREAD *thread, *top;
  int final;

  if (!Context || (Context->msgcount == 0))
    return;

  /* Figure out what the current message would be after folding / unfolding,
   * so that we can restore the cursor in a sane way afterwards. */
  if (CURHDR->collapsed && toggle)
    final = mutt_uncollapse_thread (Context, CURHDR);
  else if (option (OPTCOLLAPSEUNREAD) || !UNREAD (CURHDR))
    final = mutt_collapse_thread (Context, CURHDR);
  else
    final = CURHDR->virtual;

  base = Context->hdrs[Context->v2r[final]];

  /* Iterate all threads, perform collapse/uncollapse as needed */
  top = Context->tree;
  Context->collapsed = toggle ? !Context->collapsed : 1;
  while ((thread = top) != NULL)
  {
    while (!thread->message)
      thread = thread->child;
    h = thread->message;

    if (h->collapsed != Context->collapsed)
    {
      if (h->collapsed)
        mutt_uncollapse_thread (Context, h);
      else if (option (OPTCOLLAPSEUNREAD) || !UNREAD (h))
        mutt_collapse_thread (Context, h);
    }
    top = top->next;
  }

  /* Restore the cursor */
  mutt_set_virtual (Context);
  int j;
  for (j = 0; j < Context->vcount; j++)
  {
    if (Context->hdrs[Context->v2r[j]]->index == base->index)
    {
      menu->current = j;
      break;
    }
  }

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
}

/* terminal status capability check. terminfo must have been initialized. */
short mutt_ts_capability(void)
{
  char *term = getenv("TERM");
  char *tcaps;
#ifdef HAVE_USE_EXTENDED_NAMES
  int tcapi;
#endif
  char **termp;
  char *known[] = {
    "color-xterm",
    "cygwin",
    "eterm",
    "kterm",
    "nxterm",
    "putty",
    "rxvt",
    "screen",
    "xterm",
    NULL
  };

  /* If tsl is set, then terminfo says that status lines work. */
  tcaps = tigetstr("tsl");
  if (tcaps && tcaps != (char *)-1 && *tcaps)
  {
    /* update the static defns of tsl/fsl from terminfo */
    tsl = safe_strdup(tcaps);

    tcaps = tigetstr("fsl");
    if (tcaps && tcaps != (char *)-1 && *tcaps)
      fsl = safe_strdup(tcaps);

    return 1;
  }

  /* If XT (boolean) is set, then this terminal supports the standard escape. */
  /* Beware: tigetflag returns -1 if XT is invalid or not a boolean. */
#ifdef HAVE_USE_EXTENDED_NAMES
  use_extended_names (TRUE);
  tcapi = tigetflag("XT");
  if (tcapi == 1)
    return 1;
#endif /* HAVE_USE_EXTENDED_NAMES */

  /* Check term types that are known to support the standard escape without
   * necessarily asserting it in terminfo. */
  for (termp = known; termp; termp++)
  {
    if (term && *termp && mutt_strncasecmp (term, *termp, strlen(*termp)))
      return 1;
  }

  /* not supported */
  return 0;
}

void mutt_ts_status(char *str)
{
  /* If empty, do not set.  To clear, use a single space. */
  if (str == NULL || *str == '\0')
    return;
  fprintf(stderr, "%s%s%s", tsl, str, fsl);
}

void mutt_ts_icon(char *str)
{
  /* If empty, do not set.  To clear, use a single space. */
  if (str == NULL || *str == '\0')
    return;

  /* icon setting is not supported in terminfo, so hardcode the escape - yuck */
  fprintf(stderr, "\033]1;%s\007", str);
}

void index_make_entry (char *s, size_t l, MUTTMENU *menu, int num)
{
  if (!Context || !menu || (num < 0) || (num >= Context->hdrmax))
    return;

  HEADER *h = Context->hdrs[Context->v2r[num]];
  if (!h)
    return;

  format_flag flag = MUTT_FORMAT_MAKEPRINT | MUTT_FORMAT_ARROWCURSOR | MUTT_FORMAT_INDEX;
  int edgemsgno, reverse = Sort & SORT_REVERSE;
  THREAD *tmp;

  if ((Sort & SORT_MASK) == SORT_THREADS && h->tree)
  {
    flag |= MUTT_FORMAT_TREE; /* display the thread tree */
    if (h->display_subject)
      flag |= MUTT_FORMAT_FORCESUBJ;
    else
    {
      if (reverse)
      {
	if (menu->top + menu->pagelen > menu->max)
	  edgemsgno = Context->v2r[menu->max - 1];
	else
	  edgemsgno = Context->v2r[menu->top + menu->pagelen - 1];
      }
      else
	edgemsgno = Context->v2r[menu->top];

      for (tmp = h->thread->parent; tmp; tmp = tmp->parent)
      {
	if (!tmp->message)
	  continue;

	/* if no ancestor is visible on current screen, provisionally force
	 * subject... */
	if (reverse ? tmp->message->msgno > edgemsgno : tmp->message->msgno < edgemsgno)
	{
	  flag |= MUTT_FORMAT_FORCESUBJ;
	  break;
	}
	else if (tmp->message->virtual >= 0)
	  break;
      }
      if (flag & MUTT_FORMAT_FORCESUBJ)
      {
	for (tmp = h->thread->prev; tmp; tmp = tmp->prev)
	{
	  if (!tmp->message)
	    continue;

	  /* ...but if a previous sibling is available, don't force it */
	  if (reverse ? tmp->message->msgno > edgemsgno : tmp->message->msgno < edgemsgno)
	    break;
	  else if (tmp->message->virtual >= 0)
	  {
	    flag &= ~MUTT_FORMAT_FORCESUBJ;
	    break;
	  }
	}
      }
    }
  }

  _mutt_make_string (s, l, NONULL (HdrFmt), Context, h, flag);
}

int index_color (int index_no)
{
  if (!Context || (index_no < 0))
    return 0;

  HEADER *h = Context->hdrs[Context->v2r[index_no]];

  if (h && h->pair)
    return h->pair;

  mutt_set_header_color (Context, h);
  if (h)
    return h->pair;

  return 0;
}

static int ci_next_undeleted (int msgno)
{
  int i;

  for (i=msgno+1; i < Context->vcount; i++)
    if (! Context->hdrs[Context->v2r[i]]->deleted)
      return (i);
  return (-1);
}

static int ci_previous_undeleted (int msgno)
{
  int i;

  for (i=msgno-1; i>=0; i--)
    if (! Context->hdrs[Context->v2r[i]]->deleted)
      return (i);
  return (-1);
}

/* Return the index of the first new message, or failing that, the first
 * unread message.
 */
static int ci_first_message (void)
{
  int old = -1, i;

  if (Context && Context->msgcount)
  {
    for (i=0; i < Context->vcount; i++)
    {
      if (! Context->hdrs[Context->v2r[i]]->read &&
	  ! Context->hdrs[Context->v2r[i]]->deleted)
      {
	if (! Context->hdrs[Context->v2r[i]]->old)
	  return (i);
	else if (old == -1)
	  old = i;
      }
    }
    if (old != -1)
      return (old);

    /* If Sort is reverse and not threaded, the latest message is first.
     * If Sort is threaded, the latest message is first iff exactly one
     * of Sort and SortAux are reverse.
     */
    if (((Sort & SORT_REVERSE) && (Sort & SORT_MASK) != SORT_THREADS) ||
	((Sort & SORT_MASK) == SORT_THREADS &&
	 ((Sort ^ SortAux) & SORT_REVERSE)))
      return 0;
    else
      return (Context->vcount ? Context->vcount - 1 : 0);
  }
  return 0;
}

/* This should be in mx.c, but it only gets used here. */
static int mx_toggle_write (CONTEXT *ctx)
{
  if (!ctx)
    return -1;

  if (ctx->readonly)
  {
    mutt_error _("Cannot toggle write on a readonly mailbox!");
    return -1;
  }

  if (ctx->dontwrite)
  {
    ctx->dontwrite = 0;
    mutt_message _("Changes to folder will be written on folder exit.");
  }
  else
  {
    ctx->dontwrite = 1;
    mutt_message _("Changes to folder will not be written.");
  }

  return 0;
}

void update_index (MUTTMENU *menu, CONTEXT *ctx, int check,
			  int oldcount, int index_hint)
{
  /* store pointers to the newly added messages */
  HEADER  **save_new = NULL;
  int j;

  if (!menu || !ctx)
    return;

  /* take note of the current message */
  if (oldcount)
  {
    if (menu->current < ctx->vcount)
      menu->oldcurrent = index_hint;
    else
      oldcount = 0; /* invalid message number! */
  }

  /* We are in a limited view. Check if the new message(s) satisfy
   * the limit criteria. If they do, set their virtual msgno so that
   * they will be visible in the limited view */
  if (ctx->pattern)
  {
#define THIS_BODY ctx->hdrs[j]->content
    for (j = (check == MUTT_REOPENED) ? 0 : oldcount; j < ctx->msgcount; j++)
    {
      if (!j)
	ctx->vcount = 0;

      if (mutt_pattern_exec (ctx->limit_pattern,
			     MUTT_MATCH_FULL_ADDRESS,
			     ctx, ctx->hdrs[j]))
      {
	assert (ctx->vcount < ctx->msgcount);
	ctx->hdrs[j]->virtual = ctx->vcount;
	ctx->v2r[ctx->vcount] = j;
	ctx->hdrs[j]->limited = 1;
	ctx->vcount++;
	ctx->vsize += THIS_BODY->length + THIS_BODY->offset - THIS_BODY->hdr_offset;
      }
    }
#undef THIS_BODY
  }

  /* save the list of new messages */
  if (option(OPTUNCOLLAPSENEW) && oldcount && check != MUTT_REOPENED
      && ((Sort & SORT_MASK) == SORT_THREADS))
  {
    save_new = (HEADER **) safe_malloc (sizeof (HEADER *) * (ctx->msgcount - oldcount));
    for (j = oldcount; j < ctx->msgcount; j++)
      save_new[j-oldcount] = ctx->hdrs[j];
  }

  /* if the mailbox was reopened, need to rethread from scratch */
  mutt_sort_headers (ctx, (check == MUTT_REOPENED));

  /* uncollapse threads with new mail */
  if (option(OPTUNCOLLAPSENEW) && ((Sort & SORT_MASK) == SORT_THREADS))
  {
    if (check == MUTT_REOPENED)
    {
      THREAD *h, *j;

      ctx->collapsed = 0;

      for (h = ctx->tree; h; h = h->next)
      {
	for (j = h; !j->message; j = j->child)
	  ;
	mutt_uncollapse_thread (ctx, j->message);
      }
      mutt_set_virtual (ctx);
    }
    else if (oldcount)
    {
      for (j = 0; j < ctx->msgcount - oldcount; j++)
      {
	int k;

	for (k = 0; k < ctx->msgcount; k++)
	{
	  HEADER *h = ctx->hdrs[k];
	  if (h == save_new[j] && (!ctx->pattern || h->limited))
	    mutt_uncollapse_thread (ctx, h);
	}
      }
      FREE (&save_new);
      mutt_set_virtual (ctx);
    }
  }

  menu->current = -1;
  if (oldcount)
  {
    /* restore the current message to the message it was pointing to */
    for (j = 0; j < ctx->vcount; j++)
    {
      if (ctx->hdrs[ctx->v2r[j]]->index == menu->oldcurrent)
      {
	menu->current = j;
	break;
      }
    }
  }

  if (menu->current < 0)
    menu->current = ci_first_message ();

}

static void resort_index (MUTTMENU *menu)
{
  int i;
  HEADER *current = CURHDR;

  menu->current = -1;
  mutt_sort_headers (Context, 0);
  /* Restore the current message */

  for (i = 0; i < Context->vcount; i++)
  {
    if (Context->hdrs[Context->v2r[i]] == current)
    {
      menu->current = i;
      break;
    }
  }

  if ((Sort & SORT_MASK) == SORT_THREADS && menu->current < 0)
    menu->current = mutt_parent_message (Context, current, 0);

  if (menu->current < 0)
    menu->current = ci_first_message ();

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
}

/**
 * mutt_draw_statusline - Draw a highlighted status bar
 * @cols:  Maximum number of screen columns
 * @buf:   Message to be displayed
 *
 * Users configure the highlighting of the status bar, e.g.
 *     color status red default "[0-9][0-9]:[0-9][0-9]"
 *
 * Where regexes overlap, the one nearest the start will be used.
 * If two regexes start at the same place, the longer match will be used.
 */
void
mutt_draw_statusline (int cols, const char *buf, int buflen)
{
  int i      = 0;
  int offset = 0;
  int found  = 0;
  int chunks = 0;
  int len    = 0;

  struct syntax_t
  {
    int color;
    int first;
    int last;
  } *syntax = NULL;

  if (!buf)
    return;

  do
  {
    COLOR_LINE *cl;
    found = 0;

    if (!buf[offset])
      break;

    /* loop through each "color status regex" */
    for (cl = ColorStatusList; cl; cl = cl->next)
    {
      regmatch_t pmatch[cl->match + 1];

      if (regexec (&cl->rx, buf + offset, cl->match + 1, pmatch, 0) != 0)
        continue; /* regex doesn't match the status bar */

      int first = pmatch[cl->match].rm_so + offset;
      int last  = pmatch[cl->match].rm_eo + offset;

      if (first == last)
        continue; /* ignore an empty regex */

      if (!found)
      {
        chunks++;
        safe_realloc (&syntax, chunks * sizeof (struct syntax_t));
      }

      i = chunks - 1;
      if (!found || (first < syntax[i].first) || ((first == syntax[i].first) && (last > syntax[i].last)))
      {
        syntax[i].color = cl->pair;
        syntax[i].first = first;
        syntax[i].last  = last;
      }
      found = 1;
    }

    if (syntax)
    {
      offset = syntax[i].last;
    }
  } while (found);

  /* Only 'len' bytes will fit into 'cols' screen columns */
  len = mutt_wstr_trunc (buf, buflen, cols, NULL);

  offset = 0;

  if ((chunks > 0) && (syntax[0].first > 0))
  {
    /* Text before the first highlight */
    addnstr (buf, MIN(len, syntax[0].first));
    attrset (ColorDefs[MT_COLOR_STATUS]);
    if (len <= syntax[0].first)
      goto dsl_finish;  /* no more room */

    offset = syntax[0].first;
  }

  for (i = 0; i < chunks; i++)
  {
    /* Highlighted text */
    attrset (syntax[i].color);
    addnstr (buf + offset, MIN(len, syntax[i].last) - offset);
    if (len <= syntax[i].last)
      goto dsl_finish;  /* no more room */

    int next;
    if ((i + 1) == chunks)
    {
      next = len;
    }
    else
    {
      next = MIN (len, syntax[i+1].first);
    }

    attrset (ColorDefs[MT_COLOR_STATUS]);
    offset = syntax[i].last;
    addnstr (buf + offset, next - offset);

    offset = next;
    if (offset >= len)
      goto dsl_finish;  /* no more room */
  }

  attrset (ColorDefs[MT_COLOR_STATUS]);
  if (offset < len)
  {
    /* Text after the last highlight */
    addnstr (buf + offset, len - offset);
  }

  int width = mutt_strwidth (buf);
  if (width < cols)
  {
    /* Pad the rest of the line with whitespace */
    mutt_paddstr (cols - width, "");
  }
dsl_finish:
  FREE(&syntax);
}

static int main_change_folder(MUTTMENU *menu, int op, char *buf, size_t bufsz,
			  int *oldcount, int *index_hint, int flags)
{
#ifdef USE_NNTP
  if (option (OPTNEWS))
  {
    unset_option (OPTNEWS);
    nntp_expand_path (buf, bufsz, &CurrentNewsSrv->conn->account);
  }
  else
#endif
  mutt_expand_path (buf, bufsz);
  if (mx_get_magic (buf) <= 0)
  {
    mutt_error (_("%s is not a mailbox."), buf);
    return -1;
  }
  mutt_str_replace (&CurrentFolder, buf);

  /* keepalive failure in mutt_enter_fname may kill connection. #3028 */
  if (Context && !Context->path)
    FREE (&Context);

  if (Context)
  {
    int check;

#ifdef USE_COMPRESSED
	  if (Context->compress_info && Context->realpath)
	    mutt_str_replace (&LastFolder, Context->realpath);
	  else
#endif
    mutt_str_replace (&LastFolder, Context->path);
    *oldcount = Context ? Context->msgcount : 0;

    if ((check = mx_close_mailbox (Context, index_hint)) != 0)
    {
      if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
        update_index (menu, Context, check, *oldcount, *index_hint);

      set_option (OPTSEARCHINVALID);
      menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
      return 0;
    }
    FREE (&Context);
  }

  if (Labels)
    hash_destroy(&Labels, NULL);

  mutt_sleep (0);

  /* Set CurrentMenu to MENU_MAIN before executing any folder
   * hooks so that all the index menu functions are available to
   * the exec command.
   */

  CurrentMenu = MENU_MAIN;
  mutt_folder_hook (buf);

  if ((Context = mx_open_mailbox (buf,
		(option (OPTREADONLY) || op == OP_MAIN_CHANGE_FOLDER_READONLY) ?
		MUTT_READONLY : 0, NULL)) != NULL)
  {
    Labels = hash_create(131, 0);
    mutt_scan_labels(Context);
    menu->current = ci_first_message ();
  }
  else
    menu->current = 0;

  if (((Sort & SORT_MASK) == SORT_THREADS) && option (OPTCOLLAPSEALL))
    collapse_all (menu, 0);

#ifdef USE_SIDEBAR
        mutt_sb_set_open_buffy ();
#endif

  mutt_clear_error ();
  mutt_buffy_check(1); /* force the buffy check after we have changed the folder */
  menu->redraw = REDRAW_FULL;
  set_option (OPTSEARCHINVALID);

  return 0;
}

static const struct mapping_t IndexHelp[] = {
  { N_("Quit"),  OP_QUIT },
  { N_("Del"),   OP_DELETE },
  { N_("Undel"), OP_UNDELETE },
  { N_("Save"),  OP_SAVE },
  { N_("Mail"),  OP_MAIL },
  { N_("Reply"), OP_REPLY },
  { N_("Group"), OP_GROUP_REPLY },
  { N_("Help"),  OP_HELP },
  { NULL,	 0 }
};

#ifdef USE_NNTP
struct mapping_t IndexNewsHelp[] = {
  { N_("Quit"),     OP_QUIT },
  { N_("Del"),      OP_DELETE },
  { N_("Undel"),    OP_UNDELETE },
  { N_("Save"),     OP_SAVE },
  { N_("Post"),     OP_POST },
  { N_("Followup"), OP_FOLLOWUP },
  { N_("Catchup"),  OP_CATCHUP },
  { N_("Help"),     OP_HELP },
  { NULL,           0 }
};
#endif

/* This function handles the message index window as well as commands returned
 * from the pager (MENU_PAGER).
 */
int mutt_index_menu (void)
{
  char buf[LONG_STRING], helpstr[LONG_STRING];
  int flags;
  int op = OP_NULL;
  int done = 0;                /* controls when to exit the "event" loop */
  int i = 0, j;
  int tag = 0;                 /* has the tag-prefix command been pressed? */
  int newcount = -1;
  int oldcount = -1;
  int rc = -1;
  MUTTMENU *menu;
  char *cp;                    /* temporary variable. */
  int index_hint;   /* used to restore cursor position */
  int do_buffy_notify = 1;
  int close = 0; /* did we OP_QUIT or OP_EXIT out of this menu? */
  int attach_msg = option(OPTATTACHMSG);

  menu = mutt_new_menu (MENU_MAIN);
  menu->make_entry = index_make_entry;
  menu->color = index_color;
  menu->current = ci_first_message ();
  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_MAIN,
#ifdef USE_NNTP
	(Context && (Context->magic == MUTT_NNTP)) ? IndexNewsHelp :
#endif
	IndexHelp);

  if (!attach_msg)
    mutt_buffy_check(1); /* force the buffy check after we enter the folder */

  if (((Sort & SORT_MASK) == SORT_THREADS) && option (OPTCOLLAPSEALL))
  {
    collapse_all (menu, 0);
    menu->redraw = REDRAW_FULL;
  }

  FOREVER
  {
    tag = 0; /* clear the tag-prefix */

    /* check if we need to resort the index because just about
     * any 'op' below could do mutt_enter_command(), either here or
     * from any new menu launched, and change $sort/$sort_aux
     */
    if (option (OPTNEEDRESORT) && Context && Context->msgcount && menu->current >= 0)
      resort_index (menu);

    menu->max = Context ? Context->vcount : 0;
    oldcount = Context ? Context->msgcount : 0;

    if (option (OPTREDRAWTREE) && Context && Context->msgcount && (Sort & SORT_MASK) == SORT_THREADS)
    {
      mutt_draw_tree (Context);
      menu->redraw |= REDRAW_STATUS;
      unset_option (OPTREDRAWTREE);
    }

    if (Context && !attach_msg)
    {
      int check;
      /* check for new mail in the mailbox.  If nonzero, then something has
       * changed about the file (either we got new mail or the file was
       * modified underneath us.)
       */

      index_hint = (Context->vcount && menu->current >= 0 && menu->current < Context->vcount) ? CURHDR->index : 0;

      if ((check = mx_check_mailbox (Context, &index_hint)) < 0)
      {
	if (!Context->path)
	{
	  /* fatal error occurred */
	  FREE (&Context);
	  menu->redraw = REDRAW_FULL;
	}

	set_option (OPTSEARCHINVALID);
      }
      else if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED || check == MUTT_FLAGS)
      {
	/* notify the user of new mail */
	if (check == MUTT_REOPENED)
	  mutt_error _("Mailbox was externally modified.  Flags may be wrong.");
	else if (check == MUTT_NEW_MAIL)
	{
	  for (i = oldcount; i < Context->msgcount; i++)
	  {
	    if (Context->hdrs[i]->read == 0)
	    {
	      mutt_message _("New mail in this mailbox.");
	      if (option (OPTBEEPNEW))
		beep ();
	      if (NewMailCmd)
	      {
		char cmd[LONG_STRING];
		menu_status_line(cmd, sizeof(cmd), menu, NONULL(NewMailCmd));
		mutt_system(cmd);
	      }
	      break;
	    }
	  }
	} else if (check == MUTT_FLAGS)
	  mutt_message _("Mailbox was externally modified.");

	/* avoid the message being overwritten by buffy */
	do_buffy_notify = 0;

	int q = Context->quiet;
	Context->quiet = 1;
	update_index (menu, Context, check, oldcount, index_hint);
	Context->quiet = q;

	menu->redraw = REDRAW_FULL;
	menu->max = Context->vcount;

	set_option (OPTSEARCHINVALID);
      }
    }

    if (!attach_msg)
    {
     /* check for new mail in the incoming folders */
     oldcount = newcount;
     if ((newcount = mutt_buffy_check (0)) != oldcount)
       menu->redraw |= REDRAW_STATUS;
     if (do_buffy_notify)
     {
       if (mutt_buffy_notify())
       {
         menu->redraw |= REDRAW_STATUS;
         if (option (OPTBEEPNEW))
           beep();
         if (NewMailCmd)
         {
           char cmd[LONG_STRING];
           menu_status_line(cmd, sizeof(cmd), menu, NONULL(NewMailCmd));
           mutt_system(cmd);
         }
       }
     }
     else
       do_buffy_notify = 1;
    }

    if (op != -1)
      mutt_curs_set (0);

    if (menu->redraw & REDRAW_FULL)
    {
      menu_redraw_full (menu);
      mutt_show_error ();
    }

    if (menu->menu == MENU_MAIN)
    {
#ifdef USE_SIDEBAR
      if (menu->redraw & REDRAW_SIDEBAR || SidebarNeedsRedraw)
      {
        mutt_sb_set_buffystats (Context);
        menu_redraw_sidebar (menu);
      }
#endif
      if (Context && Context->hdrs && !(menu->current >= Context->vcount))
      {
	menu_check_recenter (menu);

	if (menu->redraw & REDRAW_INDEX)
	{
	  menu_redraw_index (menu);
	  menu->redraw |= REDRAW_STATUS;
	}
	else if (menu->redraw & (REDRAW_MOTION_RESYNCH | REDRAW_MOTION))
	  menu_redraw_motion (menu);
	else if (menu->redraw & REDRAW_CURRENT)
	  menu_redraw_current (menu);
      }

      if (menu->redraw & REDRAW_STATUS)
      {
	menu_status_line (buf, sizeof (buf), menu, NONULL (Status));
        mutt_window_move (MuttStatusWindow, 0, 0);
	SETCOLOR (MT_COLOR_STATUS);
	mutt_draw_statusline (MuttStatusWindow->cols, buf, sizeof (buf));
	NORMAL_COLOR;
	menu->redraw &= ~REDRAW_STATUS;
	if (option(OPTTSENABLED) && TSSupported)
	{
	  menu_status_line (buf, sizeof (buf), menu, NONULL (TSStatusFormat));
	  mutt_ts_status(buf);
	  menu_status_line (buf, sizeof (buf), menu, NONULL (TSIconFormat));
	  mutt_ts_icon(buf);
	}
      }

      menu->redraw = 0;
      if (menu->current < menu->max)
	menu->oldcurrent = menu->current;
      else
	menu->oldcurrent = -1;

      if (option (OPTARROWCURSOR))
	mutt_window_move (MuttIndexWindow, menu->current - menu->top + menu->offset, 2);
      else if (option (OPTBRAILLEFRIENDLY))
	mutt_window_move (MuttIndexWindow, menu->current - menu->top + menu->offset, 0);
      else
	mutt_window_move (MuttIndexWindow, menu->current - menu->top + menu->offset,
                          MuttIndexWindow->cols - 1);
      mutt_refresh ();

#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
      if (SigWinch)
      {
	mutt_flushinp ();
	mutt_resize_screen ();
	menu->redraw = REDRAW_FULL;
	menu->menu = MENU_MAIN;
	SigWinch = 0;
	menu->top = 0; /* so we scroll the right amount */
	/*
	 * force a real complete redraw.  clrtobot() doesn't seem to be able
	 * to handle every case without this.
	 */
	clearok(stdscr,TRUE);
	continue;
      }
#endif

      op = km_dokey (MENU_MAIN);

      dprint(4, (debugfile, "mutt_index_menu[%d]: Got op %d\n", __LINE__, op));

      if (op == -1) {
        mutt_timeout_hook();
	continue; /* either user abort or timeout */
      }

      mutt_curs_set (1);

      /* special handling for the tag-prefix function */
      if (op == OP_TAG_PREFIX)
      {
	if (!Context)
	{
	  mutt_error _("No mailbox is open.");
	  continue;
	}

	if (!Context->tagged)
	{
	  mutt_error _("No tagged messages.");
	  continue;
	}
	tag = 1;

	/* give visual indication that the next command is a tag- command */
	mutt_window_mvaddstr (MuttMessageWindow, 0, 0, "tag-");
	mutt_window_clrtoeol (MuttMessageWindow);

	/* get the real command */
	if ((op = km_dokey (MENU_MAIN)) == OP_TAG_PREFIX)
	{
	  /* abort tag sequence */
          mutt_window_clearline (MuttMessageWindow, 0);
	  continue;
	}
      }
      else if (option (OPTAUTOTAG) && Context && Context->tagged)
	tag = 1;

      if (op == OP_TAG_PREFIX_COND)
      {
	if (!Context)
	{
	  mutt_error _("No mailbox is open.");
	  continue;
	}

	if (!Context->tagged)
	{
	  mutt_flush_macro_to_endcond ();
	  mutt_message  _("Nothing to do.");
	  continue;
	}
	tag = 1;

	/* give visual indication that the next command is a tag- command */
	mutt_window_mvaddstr (MuttMessageWindow, 0, 0, "tag-");
	mutt_window_clrtoeol (MuttMessageWindow);

	/* get the real command */
	if ((op = km_dokey (MENU_MAIN)) == OP_TAG_PREFIX)
	{
	  /* abort tag sequence */
	  mutt_window_clearline (MuttMessageWindow, 0);
	  continue;
	}
      }

      mutt_clear_error ();
    }
    else
    {
      if (menu->current < menu->max)
	menu->oldcurrent = menu->current;
      else
	menu->oldcurrent = -1;

      mutt_curs_set (1);	/* fallback from the pager */
    }

#ifdef USE_NNTP
    unset_option (OPTNEWS);	/* for any case */
#endif

#ifdef USE_NOTMUCH
    if (Context)
      nm_debug_check(Context);
#endif

    switch (op)
    {

      /* ----------------------------------------------------------------------
       * movement commands
       */

      case OP_BOTTOM_PAGE:
	menu_bottom_page (menu);
	break;
      case OP_FIRST_ENTRY:
	menu_first_entry (menu);
	break;
      case OP_MIDDLE_PAGE:
	menu_middle_page (menu);
	break;
      case OP_HALF_UP:
	menu_half_up (menu);
	break;
      case OP_HALF_DOWN:
	menu_half_down (menu);
	break;
      case OP_NEXT_LINE:
	menu_next_line (menu);
	break;
      case OP_PREV_LINE:
	menu_prev_line (menu);
	break;
      case OP_NEXT_PAGE:
	menu_next_page (menu);
	break;
      case OP_PREV_PAGE:
	menu_prev_page (menu);
	break;
      case OP_LAST_ENTRY:
	menu_last_entry (menu);
	break;
      case OP_TOP_PAGE:
	menu_top_page (menu);
	break;
      case OP_CURRENT_TOP:
	menu_current_top (menu);
	break;
      case OP_CURRENT_MIDDLE:
	menu_current_middle (menu);
	break;
      case OP_CURRENT_BOTTOM:
	menu_current_bottom (menu);
	break;

#ifdef USE_NNTP
      case OP_GET_PARENT:
	CHECK_MSGCOUNT;
	CHECK_VISIBLE;

      case OP_GET_MESSAGE:
	CHECK_IN_MAILBOX;
	CHECK_READONLY;
	CHECK_ATTACH;
	if (Context->magic == MUTT_NNTP)
	{
	  HEADER *hdr;

	  if (op == OP_GET_MESSAGE)
	  {
	    buf[0] = 0;
	    if (mutt_get_field (_("Enter Message-Id: "),
		buf, sizeof (buf), 0) != 0 || !buf[0])
	      break;
	  }
	  else
	  {
	    LIST *ref = CURHDR->env->references;
	    if (!ref)
	    {
	      mutt_error _("Article has no parent reference.");
	      break;
	    }
	    strfcpy (buf, ref->data, sizeof (buf));
	  }
	  if (!Context->id_hash)
	    Context->id_hash = mutt_make_id_hash (Context);
	  hdr = hash_find (Context->id_hash, buf);
	  if (hdr)
	  {
	    if (hdr->virtual != -1)
	    {
	      menu->current = hdr->virtual;
	      menu->redraw = REDRAW_MOTION_RESYNCH;
	    }
	    else if (hdr->collapsed)
	    {
	      mutt_uncollapse_thread (Context, hdr);
	      mutt_set_virtual (Context);
	      menu->current = hdr->virtual;
	      menu->redraw = REDRAW_MOTION_RESYNCH;
	    }
	    else
	      mutt_error _("Message is not visible in limited view.");
	  }
	  else
	  {
	    int rc;

	    mutt_message (_("Fetching %s from server..."), buf);
	    rc = nntp_check_msgid (Context, buf);
	    if (rc == 0)
	    {
	      hdr = Context->hdrs[Context->msgcount - 1];
	      mutt_sort_headers (Context, 0);
	      menu->current = hdr->virtual;
	      menu->redraw = REDRAW_FULL;
	    }
	    else if (rc > 0)
	      mutt_error (_("Article %s not found on the server."), buf);
	  }
	}
	break;

      case OP_GET_CHILDREN:
      case OP_RECONSTRUCT_THREAD:
	CHECK_MSGCOUNT;
	CHECK_VISIBLE;
	CHECK_READONLY;
	CHECK_ATTACH;
	if (Context->magic == MUTT_NNTP)
	{
	  int oldmsgcount = Context->msgcount;
	  int oldindex = CURHDR->index;
	  int rc = 0;

	  if (!CURHDR->env->message_id)
	  {
	    mutt_error _("No Message-Id. Unable to perform operation.");
	    break;
	  }

	  mutt_message _("Fetching message headers...");
	  if (!Context->id_hash)
	    Context->id_hash = mutt_make_id_hash (Context);
	  strfcpy (buf, CURHDR->env->message_id, sizeof (buf));

	  /* trying to find msgid of the root message */
	  if (op == OP_RECONSTRUCT_THREAD)
	  {
	    LIST *ref = CURHDR->env->references;
	    while (ref)
	    {
	      if (hash_find (Context->id_hash, ref->data) == NULL)
	      {
		rc = nntp_check_msgid (Context, ref->data);
		if (rc < 0)
		  break;
	      }

	      /* the last msgid in References is the root message */
	      if (!ref->next)
		strfcpy (buf, ref->data, sizeof (buf));
	      ref = ref->next;
	    }
	  }

	  /* fetching all child messages */
	  if (rc >= 0)
	    rc = nntp_check_children (Context, buf);

	  /* at least one message has been loaded */
	  if (Context->msgcount > oldmsgcount)
	  {
	    HEADER *oldcur = CURHDR;
	    HEADER *hdr;
	    int i, quiet = Context->quiet;

	    if (rc < 0)
	      Context->quiet = 1;
	    mutt_sort_headers (Context, (op == OP_RECONSTRUCT_THREAD));
	    Context->quiet = quiet;

	    /* Similar to OP_MAIN_ENTIRE_THREAD, keep displaying the old message, but
	       update the index */
	    if (menu->menu == MENU_PAGER)
	    {
	      menu->current = oldcur->virtual;
	      menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
	      op = OP_DISPLAY_MESSAGE;
	      continue;
	    }

	    /* if the root message was retrieved, move to it */
	    hdr = hash_find (Context->id_hash, buf);
	    if (hdr)
	      menu->current = hdr->virtual;

	    /* try to restore old position */
	    else
	    {
	      for (i = 0; i < Context->msgcount; i++)
	      {
		if (Context->hdrs[i]->index == oldindex)
		{
		  menu->current = Context->hdrs[i]->virtual;
		  /* as an added courtesy, recenter the menu
		   * with the current entry at the middle of the screen */
		  menu_check_recenter (menu);
		  menu_current_middle (menu);
		}
	      }
	    }
	    menu->redraw = REDRAW_FULL;
	  }
	  else if (rc >= 0)
	  {
	    mutt_error _("No deleted messages found in the thread.");
	    /* Similar to OP_MAIN_ENTIRE_THREAD, keep displaying the old message, but
	       update the index */
	    if (menu->menu == MENU_PAGER)
	    {
	      op = OP_DISPLAY_MESSAGE;
	      continue;
	    }
	  }
	}
	break;
#endif

      case OP_JUMP:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (isdigit (LastKey)) mutt_unget_event (LastKey, 0);
	buf[0] = 0;
	if (mutt_get_field (_("Jump to message: "), buf, sizeof (buf), 0) != 0
	    || !buf[0])
        {
          if (menu->menu == MENU_PAGER)
          {
            op = OP_DISPLAY_MESSAGE;
            continue;
          }
	  break;
        }

	if (mutt_atoi (buf, &i) < 0)
	{
	  mutt_error _("Argument must be a message number.");
	  break;
	}

	if (i > 0 && i <= Context->msgcount)
	{
	  for (j = i-1; j < Context->msgcount; j++)
	  {
	    if (Context->hdrs[j]->virtual != -1)
	      break;
	  }
	  if (j >= Context->msgcount)
	  {
	    for (j = i-2; j >= 0; j--)
	    {
	      if (Context->hdrs[j]->virtual != -1)
		break;
	    }
	  }

	  if (j >= 0)
	  {
	    menu->current = Context->hdrs[j]->virtual;
	    if (menu->menu == MENU_PAGER)
	    {
	      op = OP_DISPLAY_MESSAGE;
	      continue;
	    }
	    else
	    menu->redraw = REDRAW_MOTION;
	  }
	  else
	    mutt_error _("That message is not visible.");
	}
	else
	  mutt_error _("Invalid message number.");

	break;

	/* --------------------------------------------------------------------
	 * `index' specific commands
	 */

      case OP_MAIN_DELETE_PATTERN:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message(s)"));

	CHECK_ATTACH;
	mutt_pattern_func (MUTT_DELETE, _("Delete messages matching: "));
	menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

#ifdef USE_POP
      case OP_MAIN_FETCH_MAIL:

	CHECK_ATTACH;
	pop_fetch_mail ();
	menu->redraw = REDRAW_FULL;
	break;
#endif /* USE_POP */

      case OP_HELP:

	mutt_help (MENU_MAIN);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_MAIN_SHOW_LIMIT:
	CHECK_IN_MAILBOX;
	if (!Context->pattern)
	   mutt_message _("No limit pattern is in effect.");
	else
	{
	   char buf[STRING];
	   /* L10N: ask for a limit to apply */
	   snprintf (buf, sizeof(buf), _("Limit: %s"),Context->pattern);
           mutt_message ("%s", buf);
	}
        break;

      case OP_LIMIT_CURRENT_THREAD:
      case OP_MAIN_LIMIT:
      case OP_TOGGLE_READ:

	CHECK_IN_MAILBOX;
	menu->oldcurrent = (Context->vcount && menu->current >= 0 && menu->current < Context->vcount) ?
		CURHDR->index : -1;
	if (op == OP_TOGGLE_READ)
	{
	  char buf[LONG_STRING];

	  if (!Context->pattern || strncmp (Context->pattern, "!~R!~D~s", 8) != 0)
	  {
	    snprintf (buf, sizeof (buf), "!~R!~D~s%s",
		      Context->pattern ? Context->pattern : ".*");
	    set_option (OPTHIDEREAD);
	  }
	  else
	  {
	    strfcpy (buf, Context->pattern + 8, sizeof(buf));
	    if (!*buf || strncmp (buf, ".*", 2) == 0)
	      snprintf (buf, sizeof(buf), "~A");
	    unset_option (OPTHIDEREAD);
	  }
	  FREE (&Context->pattern);
	  Context->pattern = safe_strdup (buf);
	}

	if (((op == OP_LIMIT_CURRENT_THREAD) && mutt_limit_current_thread(CURHDR)) ||
            ((op == OP_MAIN_LIMIT) && (mutt_pattern_func (MUTT_LIMIT,
                _("Limit to messages matching: ")) == 0)))
	{
	  if (menu->oldcurrent >= 0)
	  {
	    /* try to find what used to be the current message */
	    menu->current = -1;
	    for (i = 0; i < Context->vcount; i++)
	      if (Context->hdrs[Context->v2r[i]]->index == menu->oldcurrent)
	      {
		menu->current = i;
		break;
	      }
	    if (menu->current < 0) menu->current = 0;
	  }
	  else
	    menu->current = 0;
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	  if (Context->msgcount && (Sort & SORT_MASK) == SORT_THREADS)
	    mutt_draw_tree (Context);
	  menu->redraw = REDRAW_FULL;
	}
        if (Context->pattern)
	  mutt_message _("To view all messages, limit to \"all\".");
	break;

      case OP_QUIT:

	close = op;
	if (attach_msg)
	{
	 done = 1;
	 break;
	}

	if (query_quadoption (OPT_QUIT, _("Quit Mutt?")) == MUTT_YES)
	{
	  int check;

	  oldcount = Context ? Context->msgcount : 0;

	  mutt_startup_shutdown_hook (MUTT_SHUTDOWNHOOK);

	  if (!Context || (check = mx_close_mailbox (Context, &index_hint)) == 0)
	    done = 1;
	  else
	  {
	    if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
	      update_index (menu, Context, check, oldcount, index_hint);

	    menu->redraw = REDRAW_FULL; /* new mail arrived? */
	    set_option (OPTSEARCHINVALID);
	  }
	}
	break;

      case OP_REDRAW:

	clearok (stdscr, TRUE);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_SEARCH:
      case OP_SEARCH_REVERSE:
      case OP_SEARCH_NEXT:
      case OP_SEARCH_OPPOSITE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if ((menu->current = mutt_search_command (menu->current, op)) == -1)
	  menu->current = menu->oldcurrent;
	else
	  menu->redraw = REDRAW_MOTION;
	break;

      case OP_SORT:
      case OP_SORT_REVERSE:

	if (mutt_select_sort ((op == OP_SORT_REVERSE)) == 0)
	{
	  if (Context && Context->msgcount)
	  {
	    resort_index (menu);
	    set_option (OPTSEARCHINVALID);
	  }
	  if (menu->menu == MENU_PAGER)
	  {
	    op = OP_DISPLAY_MESSAGE;
	    continue;
	  }
	  menu->redraw |= REDRAW_STATUS;
	}
	break;

      case OP_TAG:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (tag && !option (OPTAUTOTAG))
	{
	  for (j = 0; j < Context->vcount; j++)
	    mutt_set_flag (Context, Context->hdrs[Context->v2r[j]], MUTT_TAG, 0);
	  menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
	}
	else
	{
	  mutt_set_flag (Context, CURHDR, MUTT_TAG, !CURHDR->tagged);

	  Context->last_tag = CURHDR->tagged ? CURHDR :
	    ((Context->last_tag == CURHDR && !CURHDR->tagged)
	     ? NULL : Context->last_tag);

	  menu->redraw = REDRAW_STATUS;
	  if (option (OPTRESOLVE) && menu->current < Context->vcount - 1)
	  {
	    menu->current++;
	    menu->redraw |= REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw |= REDRAW_CURRENT;
	}
	break;

      case OP_MAIN_TAG_PATTERN:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	mutt_pattern_func (MUTT_TAG, _("Tag messages matching: "));
	menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

      case OP_MAIN_UNDELETE_PATTERN:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message(s)"));

	if (mutt_pattern_func (MUTT_UNDELETE, _("Undelete messages matching: ")) == 0)
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

      case OP_MAIN_UNTAG_PATTERN:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (mutt_pattern_func (MUTT_UNTAG, _("Untag messages matching: ")) == 0)
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

      case OP_COMPOSE_TO_SENDER:

	mutt_compose_to_sender (tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;

	/* --------------------------------------------------------------------
	 * The following operations can be performed inside of the pager.
	 */

#ifdef USE_IMAP
      case OP_MAIN_IMAP_FETCH:
	if (Context && Context->magic == MUTT_IMAP)
	  imap_check_mailbox (Context, &index_hint, 1);
        break;

      case OP_MAIN_IMAP_LOGOUT_ALL:
	if (Context && Context->magic == MUTT_IMAP)
	{
	  if (mx_close_mailbox (Context, &index_hint) != 0)
	  {
	    set_option (OPTSEARCHINVALID);
	    menu->redraw = REDRAW_FULL;
	    break;
	  }
	  FREE (&Context);
	}
	imap_logout_all();
	mutt_message _("Logged out of IMAP servers.");
	set_option (OPTSEARCHINVALID);
	menu->redraw = REDRAW_FULL;
	break;
#endif

      case OP_MAIN_SYNC_FOLDER:

	if (Context && !Context->msgcount)
	  break;

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	{
	  int oldvcount = Context->vcount;
	  int oldcount  = Context->msgcount;
	  int check, newidx;
	  HEADER *newhdr = NULL;

	  /* don't attempt to move the cursor if there are no visible messages in the current limit */
	  if (menu->current < Context->vcount)
	  {
	    /* threads may be reordered, so figure out what header the cursor
	     * should be on. #3092 */
	    newidx = menu->current;
	    if (CURHDR->deleted)
	      newidx = ci_next_undeleted (menu->current);
	    if (newidx < 0)
	      newidx = ci_previous_undeleted (menu->current);
	    if (newidx >= 0)
	      newhdr = Context->hdrs[Context->v2r[newidx]];
	  }

	  if ((check = mx_sync_mailbox (Context, &index_hint)) == 0)
	  {
	    if (newhdr && Context->vcount != oldvcount)
	      for (j = 0; j < Context->vcount; j++)
	      {
		if (Context->hdrs[Context->v2r[j]] == newhdr)
		{
		  menu->current = j;
		  break;
		}
	      }
	    set_option (OPTSEARCHINVALID);
	  }
	  else if (check == MUTT_NEW_MAIL || check == MUTT_REOPENED)
	    update_index (menu, Context, check, oldcount, index_hint);

	  /*
	   * do a sanity check even if mx_sync_mailbox failed.
	   */

	  if (menu->current < 0 || menu->current >= Context->vcount)
	    menu->current = ci_first_message ();
	}

	/* check for a fatal error, or all messages deleted */
	if (!Context->path)
	  FREE (&Context);

	/* if we were in the pager, redisplay the message */
	if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
        else
	  menu->redraw = REDRAW_FULL;
	break;

      case OP_MAIN_QUASI_DELETE:
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (tag) {
	  for (j = 0; j < Context->vcount; j++) {
	    if (Context->hdrs[Context->v2r[j]]->tagged) {
	      Context->hdrs[Context->v2r[j]]->quasi_deleted = TRUE;
	      Context->changed = TRUE;
	    }
	  }
	} else {
	  CURHDR->quasi_deleted = TRUE;
	  Context->changed = 1;
	}
	break;

#ifdef USE_NOTMUCH
      case OP_MAIN_ENTIRE_THREAD:
      {
	int oldcount  = Context->msgcount;
	if (Context->magic != MUTT_NOTMUCH) {
	  mutt_message _("No virtual folder, aborting.");
	  break;
	}
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (nm_read_entire_thread(Context, CURHDR) < 0) {
	   mutt_message _("Failed to read thread, aborting.");
	   break;
	}
	if (oldcount < Context->msgcount) {
		HEADER *oldcur = CURHDR;

		if ((Sort & SORT_MASK) == SORT_THREADS)
			mutt_sort_headers (Context, 0);
		menu->current = oldcur->virtual;
		menu->redraw = REDRAW_STATUS | REDRAW_INDEX;

		if (oldcur->collapsed || Context->collapsed) {
			menu->current = mutt_uncollapse_thread(Context, CURHDR);
			mutt_set_virtual(Context);
		}
	}
	if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	break;
      }

      case OP_MAIN_MODIFY_LABELS:
      case OP_MAIN_MODIFY_LABELS_THEN_HIDE:
      {
	if (Context->magic != MUTT_NOTMUCH) {
	  mutt_message _("No virtual folder, aborting.");
	  break;
	}
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	*buf = '\0';
	if (mutt_get_field ("Add/remove labels: ", buf, sizeof (buf), MUTT_NM_TAG) || !*buf)
	{
          mutt_message _("No label specified, aborting.");
          break;
        }
	if (tag)
	{
	  char msgbuf[STRING];
	  progress_t progress;
	  int px;

	  if (!Context->quiet) {
	    snprintf(msgbuf, sizeof (msgbuf), _("Update labels..."));
	    mutt_progress_init(&progress, msgbuf, MUTT_PROGRESS_MSG,
				   1, Context->tagged);
	  }
	  nm_longrun_init(Context, TRUE);
	  for (px = 0, j = 0; j < Context->vcount; j++) {
	    if (Context->hdrs[Context->v2r[j]]->tagged) {
	      if (!Context->quiet)
		mutt_progress_update(&progress, ++px, -1);
	      nm_modify_message_tags(Context, Context->hdrs[Context->v2r[j]], buf);
	      if (op == OP_MAIN_MODIFY_LABELS_THEN_HIDE)
	      {
		Context->hdrs[Context->v2r[j]]->quasi_deleted = TRUE;
	        Context->changed = TRUE;
	      }
	    }
	  }
	  nm_longrun_done(Context);
	  menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
	}
	else
	{
	  if (nm_modify_message_tags(Context, CURHDR, buf)) {
	    mutt_message _("Failed to modify labels, aborting.");
	    break;
	  }
	  if (op == OP_MAIN_MODIFY_LABELS_THEN_HIDE)
	  {
	    CURHDR->quasi_deleted = TRUE;
	    Context->changed = TRUE;
	  }
	  if (menu->menu == MENU_PAGER)
	  {
	    op = OP_DISPLAY_MESSAGE;
	    continue;
	  }
	  if (option (OPTRESOLVE))
	  {
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	    {
	      menu->current = menu->oldcurrent;
	      menu->redraw = REDRAW_CURRENT;
	    }
	    else
	      menu->redraw = REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw = REDRAW_CURRENT;
	}
	menu->redraw |= REDRAW_STATUS;
	break;
      }

      case OP_MAIN_VFOLDER_FROM_QUERY:
	buf[0] = '\0';
        if (mutt_get_field ("Query: ", buf, sizeof (buf), MUTT_NM_QUERY) != 0 || !buf[0])
        {
          mutt_message _("No query, aborting.");
          break;
        }
	if (!nm_uri_from_query(Context, buf, sizeof (buf)))
	  mutt_message _("Failed to create query, aborting.");
	else
	  main_change_folder(menu, op, buf, sizeof (buf), &oldcount, &index_hint, 0);
	break;

      case OP_MAIN_WINDOWED_VFOLDER_FROM_QUERY:
	dprint(2, (debugfile, "OP_MAIN_WINDOWED_VFOLDER_FROM_QUERY\n"));
	if (0 > NotmuchQueryWindowDuration) 
	{
	  mutt_message _("Windowed queries disabled.");
	  break;
	}
	if (!nm_query_window_check_timebase(NotmuchQueryWindowTimebase)) {
	  mutt_message _("Invalid nm_query_window_timebase value (valid values are: hour, day, week, month or year).");
	  break;
	}
	buf[0] = '\0';
	if (mutt_get_field ("Query: ", buf, sizeof (buf), MUTT_NM_QUERY) != 0 || !buf[0])
	{
	  mutt_message _("No query, aborting.");
	  break;
	}
	nm_setup_windowed_query(buf, sizeof (buf));
	nm_query_window_reset();
	if (!nm_uri_from_windowed_query(Context, buf, sizeof(buf), NotmuchQueryWindowTimebase, NotmuchQueryWindowDuration))
	  mutt_message _("Failed to create query, aborting.");
	else
	{
	  dprint(2, (debugfile, "nm: windowed query (%s)\n", buf));
	  main_change_folder(menu, op, buf, sizeof (buf), &oldcount, &index_hint, 0);
	}
	break;

      case OP_MAIN_WINDOWED_VFOLDER_BACKWARD:
	dprint(2, (debugfile, "OP_MAIN_WINDOWED_VFOLDER_BACKWARD\n"));
	if (0 > NotmuchQueryWindowDuration) 
	{
	  mutt_message _("Windowed queries disabled.");
	  break;
	}
	if (!nm_query_window_check_timebase(NotmuchQueryWindowTimebase)) {
	  mutt_message _("Invalid nm_query_window_timebase value (valid values are: hour, day, week, month or year).");
	  break;
	}
	buf[0] = '\0';
	nm_query_window_backward();
	if (!nm_uri_from_windowed_query(Context, buf, sizeof(buf), NotmuchQueryWindowTimebase, NotmuchQueryWindowDuration))
	  mutt_message _("Failed to create query, aborting.");
	else
	{
	  dprint(2, (debugfile, "nm: - windowed query (%s)\n", buf));
	  main_change_folder(menu, op, buf, sizeof (buf), &oldcount, &index_hint, 0);
	}
	break;

      case OP_MAIN_WINDOWED_VFOLDER_FORWARD:
	dprint(2, (debugfile, "OP_MAIN_WINDOWED_VFOLDER_FORWARD\n"));
	if (0 > NotmuchQueryWindowDuration) 
	{
	  mutt_message _("Windowed queries disabled.");
	  break;
	}
	if (!nm_query_window_check_timebase(NotmuchQueryWindowTimebase)) {
	  mutt_message _("Invalid nm_query_window_timebase value (valid values are: hour, day, week, month or year).");
	  break;
	}
	buf[0] = '\0';
	nm_query_window_forward();
	if (!nm_uri_from_windowed_query(Context, buf, sizeof(buf), NotmuchQueryWindowTimebase, NotmuchQueryWindowDuration))
	  mutt_message _("Failed to create query, aborting.");
	else
	{
	  dprint(2, (debugfile, "nm: + windowed query (%s)\n", buf));
	  main_change_folder(menu, op, buf, sizeof (buf), &oldcount, &index_hint, 0);
	}
	break;

      case OP_MAIN_CHANGE_VFOLDER:
#endif

#ifdef USE_SIDEBAR
      case OP_SIDEBAR_OPEN:
#endif
      case OP_MAIN_CHANGE_FOLDER:
      case OP_MAIN_NEXT_UNREAD_MAILBOX:
      case OP_MAIN_CHANGE_FOLDER_READONLY:
#ifdef USE_NNTP
      case OP_MAIN_CHANGE_GROUP:
      case OP_MAIN_CHANGE_GROUP_READONLY:
	unset_option (OPTNEWS);
#endif
	if (attach_msg || option (OPTREADONLY) ||
#ifdef USE_NNTP
	    op == OP_MAIN_CHANGE_GROUP_READONLY ||
#endif
	    op == OP_MAIN_CHANGE_FOLDER_READONLY)
	  flags = MUTT_READONLY;
	else
	  flags = 0;

	if (flags)
          cp = _("Open mailbox in read-only mode");
#ifdef USE_NOTMUCH
        else if (op == OP_MAIN_CHANGE_VFOLDER)
	  cp = _("Open virtual folder");
#endif
	else
          cp = _("Open mailbox");

	buf[0] = '\0';
	if ((op == OP_MAIN_NEXT_UNREAD_MAILBOX) && Context && Context->path)
	{
	  strfcpy (buf, Context->path, sizeof (buf));
	  mutt_pretty_mailbox (buf, sizeof (buf));
	  mutt_buffy (buf, sizeof (buf));
	  if (!buf[0])
	  {
	    mutt_error _("No mailboxes have new mail");
	    break;
	  }
	}
#ifdef USE_SIDEBAR
        else if (op == OP_SIDEBAR_OPEN)
        {
          const char *path = mutt_sb_get_highlight();
          if (!path || !*path)
            break;
          strncpy (buf, path, sizeof (buf));

          /* Mark the selected dir for the mutt browser */
          mutt_browser_select_dir (buf);
        }
#endif
#ifdef USE_NOTMUCH
	else if (op == OP_MAIN_CHANGE_VFOLDER) {
	  if (Context->magic == MUTT_NOTMUCH) {
		  strfcpy(buf, Context->path, sizeof (buf));
		  mutt_buffy_vfolder (buf, sizeof (buf));
	  }
	  mutt_enter_vfolder (cp, buf, sizeof (buf), &menu->redraw, 1);
	  if (!buf[0])
	  {
            mutt_window_clearline (MuttMessageWindow, 0);
	    break;
	  }
	}
#endif
	else
	{
#ifdef USE_NNTP
	  if (op == OP_MAIN_CHANGE_GROUP ||
	      op == OP_MAIN_CHANGE_GROUP_READONLY)
	  {
	    set_option (OPTNEWS);
	    CurrentNewsSrv = nntp_select_server (NewsServer, 0);
	    if (!CurrentNewsSrv)
	      break;
	    if (flags)
	      cp = _("Open newsgroup in read-only mode");
	    else
	      cp = _("Open newsgroup");
	    nntp_buffy (buf, sizeof (buf));
	  }
	  else
#endif
	  /* By default, fill buf with the next mailbox that contains unread
	   * mail */
	  mutt_buffy (buf, sizeof (buf));

          if (mutt_enter_fname (cp, buf, sizeof (buf), &menu->redraw, 1) == -1)
          {
            if (menu->menu == MENU_PAGER)
            {
              op = OP_DISPLAY_MESSAGE;
              continue;
            }
            else
              break;
          }
          
          /* Selected directory is okay, let's save it.*/
          mutt_browser_select_dir (buf);

	  if (!buf[0])
	  {
            mutt_window_clearline (MuttMessageWindow, 0);
	    break;
	  }
	}

	main_change_folder(menu, op, buf, sizeof (buf), &oldcount, &index_hint, flags);
#ifdef USE_NNTP
	/* mutt_buffy_check() must be done with mail-reader mode! */
	menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_MAIN,
	  (Context && (Context->magic == MUTT_NNTP)) ? IndexNewsHelp : IndexHelp);
#endif
	mutt_expand_path (buf, sizeof (buf));
#ifdef USE_SIDEBAR
        mutt_sb_set_open_buffy();
#endif
	break;

      case OP_DISPLAY_MESSAGE:
      case OP_DISPLAY_HEADERS: /* don't weed the headers */

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	/*
	 * toggle the weeding of headers so that a user can press the key
	 * again while reading the message.
	 */
	if (op == OP_DISPLAY_HEADERS)
	  toggle_option (OPTWEED);

	unset_option (OPTNEEDRESORT);

	if ((Sort & SORT_MASK) == SORT_THREADS && CURHDR->collapsed)
	{
	  mutt_uncollapse_thread (Context, CURHDR);
	  mutt_set_virtual (Context);
	  if (option (OPTUNCOLLAPSEJUMP))
	    menu->current = mutt_thread_next_unread (Context, CURHDR);
	}

	if (option (OPTPGPAUTODEC) && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
	  mutt_check_traditional_pgp (tag ? NULL : CURHDR, &menu->redraw);
	int index_hint = Context->hdrs[Context->v2r[menu->current]]->index;
	if ((op = mutt_display_message (CURHDR)) == -1)
	{
	  unset_option (OPTNEEDRESORT);
	  break;
	}

	menu->menu = MENU_PAGER;
 	menu->oldcurrent = menu->current;
	if (Context)
	  update_index (menu, Context, MUTT_NEW_MAIL, Context->msgcount, index_hint);

	continue;

      case OP_EXIT:

	close = op;
	if (menu->menu == MENU_MAIN && attach_msg)
	{
	 done = 1;
	 break;
	}

	if ((menu->menu == MENU_MAIN)
	    && (query_quadoption (OPT_QUIT,
				  _("Exit NeoMutt without saving?")) == MUTT_YES))
	{
	  if (Context)
	  {
	    mx_fastclose_mailbox (Context);
	    FREE (&Context);
	  }
	  done = 1;
	}
	break;

      case OP_MAIN_BREAK_THREAD:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
	CHECK_ACL(MUTT_ACL_WRITE, _("Cannot break thread"));

        if ((Sort & SORT_MASK) != SORT_THREADS)
	  mutt_error _("Threading is not enabled.");
	else if (CURHDR->env->in_reply_to || CURHDR->env->references)
	{
	  {
	    HEADER *oldcur = CURHDR;

	    mutt_break_thread (CURHDR);
	    mutt_sort_headers (Context, 1);
	    menu->current = oldcur->virtual;
	  }

	  Context->changed = 1;
	  mutt_message _("Thread broken");

	  if (menu->menu == MENU_PAGER)
	  {
	    op = OP_DISPLAY_MESSAGE;
	    continue;
	  }
	  else
	    menu->redraw |= REDRAW_INDEX;
	}
	else
	  mutt_error _("Thread cannot be broken, message is not part of a thread");

	break;

      case OP_MAIN_LINK_THREADS:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_WRITE, _("Cannot link threads"));

        if ((Sort & SORT_MASK) != SORT_THREADS)
	  mutt_error _("Threading is not enabled.");
	else if (!CURHDR->env->message_id)
	  mutt_error _("No Message-ID: header available to link thread");
	else if (!tag && (!Context->last_tag || !Context->last_tag->tagged))
	  mutt_error _("First, please tag a message to be linked here");
	else
	{
	  HEADER *oldcur = CURHDR;

	  if (mutt_link_threads (CURHDR, tag ? NULL : Context->last_tag,
				 Context))
	  {
	    mutt_sort_headers (Context, 1);
	    menu->current = oldcur->virtual;

	    Context->changed = 1;
	    mutt_message _("Threads linked");
	  }
	  else
	    mutt_error _("No thread linked");
	}

	if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw |= REDRAW_STATUS | REDRAW_INDEX;

	break;

      case OP_EDIT_TYPE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_ATTACH;
	mutt_edit_content_type (CURHDR, CURHDR->content, NULL);
	/* if we were in the pager, redisplay the message */
	if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
        else
	  menu->redraw = REDRAW_CURRENT;
	break;

      case OP_MAIN_NEXT_UNDELETED:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (menu->current >= Context->vcount - 1)
	{
	  if (menu->menu == MENU_MAIN)
	    mutt_error _("You are on the last message.");
	  break;
	}
	if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	{
	  menu->current = menu->oldcurrent;
	  if (menu->menu == MENU_MAIN)
	    mutt_error _("No undeleted messages.");
	}
	else if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw = REDRAW_MOTION;
	break;

      case OP_NEXT_ENTRY:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (menu->current >= Context->vcount - 1)
	{
	  if (menu->menu == MENU_MAIN)
	    mutt_error _("You are on the last message.");
	  break;
	}
	menu->current++;
	if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw = REDRAW_MOTION;
	break;

      case OP_MAIN_PREV_UNDELETED:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (menu->current < 1)
	{
	  mutt_error _("You are on the first message.");
	  break;
	}
	if ((menu->current = ci_previous_undeleted (menu->current)) == -1)
	{
	  menu->current = menu->oldcurrent;
	  if (menu->menu == MENU_MAIN)
	    mutt_error _("No undeleted messages.");
	}
	else if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw = REDRAW_MOTION;
	break;

      case OP_PREV_ENTRY:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (menu->current < 1)
	{
	  if (menu->menu == MENU_MAIN) mutt_error _("You are on the first message.");
	  break;
	}
	menu->current--;
	if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw = REDRAW_MOTION;
	break;

      case OP_DECRYPT_COPY:
      case OP_DECRYPT_SAVE:
        if (!WithCrypto)
          break;
        /* fall thru */
      case OP_COPY_MESSAGE:
      case OP_SAVE:
      case OP_DECODE_COPY:
      case OP_DECODE_SAVE:
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (mutt_save_message (tag ? NULL : CURHDR,
			       (op == OP_DECRYPT_SAVE) ||
			       (op == OP_SAVE) || (op == OP_DECODE_SAVE),
			       (op == OP_DECODE_SAVE) || (op == OP_DECODE_COPY),
			       (op == OP_DECRYPT_SAVE) || (op == OP_DECRYPT_COPY) ||
			       0,
			       &menu->redraw) == 0 &&
	     (op == OP_SAVE || op == OP_DECODE_SAVE || op == OP_DECRYPT_SAVE)
	    )
	{
	  if (tag)
	    menu->redraw |= REDRAW_INDEX;
	  else if (option (OPTRESOLVE))
	  {
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	    {
	      menu->current = menu->oldcurrent;
	      menu->redraw |= REDRAW_CURRENT;
	    }
	    else
	      menu->redraw |= REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw |= REDRAW_CURRENT;
	}
	break;

      case OP_MAIN_NEXT_NEW:
      case OP_MAIN_NEXT_UNREAD:
      case OP_MAIN_PREV_NEW:
      case OP_MAIN_PREV_UNREAD:
      case OP_MAIN_NEXT_NEW_THEN_UNREAD:
      case OP_MAIN_PREV_NEW_THEN_UNREAD:

      {
	int first_unread = -1;
	int first_new    = -1;

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;

	i = menu->current;
	menu->current = -1;
	for (j = 0; j != Context->vcount; j++)
	{
#define CURHDRi Context->hdrs[Context->v2r[i]]
	  if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_NEXT_NEW_THEN_UNREAD)
	  {
	    i++;
	    if (i > Context->vcount - 1)
	    {
	      mutt_message _("Search wrapped to top.");
	      i = 0;
	    }
	  }
	  else
	  {
	    i--;
	    if (i < 0)
	    {
	      mutt_message _("Search wrapped to bottom.");
	      i = Context->vcount - 1;
	    }
	  }

	  if (CURHDRi->collapsed && (Sort & SORT_MASK) == SORT_THREADS)
	  {
	    if (UNREAD (CURHDRi) && first_unread == -1)
	      first_unread = i;
	    if (UNREAD (CURHDRi) == 1 && first_new == -1)
	      first_new = i;
	  }
	  else if ((!CURHDRi->deleted && !CURHDRi->read))
	  {
	    if (first_unread == -1)
	      first_unread = i;
	    if ((!CURHDRi->old) && first_new == -1)
	      first_new = i;
	  }

	  if ((op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_PREV_UNREAD) &&
	      first_unread != -1)
	    break;
	  if ((op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW ||
	       op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD)
	      && first_new != -1)
	    break;
	}
#undef CURHDRi
	if ((op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW ||
	     op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD)
	    && first_new != -1)
	  menu->current = first_new;
	else if ((op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_PREV_UNREAD ||
		  op == OP_MAIN_NEXT_NEW_THEN_UNREAD || op == OP_MAIN_PREV_NEW_THEN_UNREAD)
		 && first_unread != -1)
	  menu->current = first_unread;

	if (menu->current == -1)
	{
	  menu->current = menu->oldcurrent;
	  if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW)
          {
            if (Context->pattern)
              mutt_error (_("No new messages in this limited view."));
            else
              mutt_error (_("No new messages."));
          }
          else
          {
            if (Context->pattern)
              mutt_error (_("No unread messages in this limited view."));
            else
              mutt_error (_("No unread messages."));
          }
	}
	else if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw = REDRAW_MOTION;
	break;
      }
      case OP_FLAG_MESSAGE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_WRITE, _("Cannot flag message"));

        if (tag)
        {
	  for (j = 0; j < Context->vcount; j++)
	  {
	    if (Context->hdrs[Context->v2r[j]]->tagged)
	      mutt_set_flag (Context, Context->hdrs[Context->v2r[j]],
			     MUTT_FLAG, !Context->hdrs[Context->v2r[j]]->flagged);
	  }

	  menu->redraw |= REDRAW_INDEX;
	}
        else
        {
	  mutt_set_flag (Context, CURHDR, MUTT_FLAG, !CURHDR->flagged);
	  if (option (OPTRESOLVE))
	  {
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	    {
	      menu->current = menu->oldcurrent;
	      menu->redraw = REDRAW_CURRENT;
	    }
	    else
	      menu->redraw = REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw = REDRAW_CURRENT;
	}
	menu->redraw |= REDRAW_STATUS;
	break;

      case OP_TOGGLE_NEW:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_SEEN, _("Cannot toggle new"));

	if (tag)
	{
	  for (j = 0; j < Context->vcount; j++)
	  {
	    if (Context->hdrs[Context->v2r[j]]->tagged)
	    {
	      if (Context->hdrs[Context->v2r[j]]->read ||
		  Context->hdrs[Context->v2r[j]]->old)
		mutt_set_flag (Context, Context->hdrs[Context->v2r[j]], MUTT_NEW, 1);
	      else
		mutt_set_flag (Context, Context->hdrs[Context->v2r[j]], MUTT_READ, 1);
	    }
	  }
	  menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
	}
	else
	{
	  if (CURHDR->read || CURHDR->old)
	    mutt_set_flag (Context, CURHDR, MUTT_NEW, 1);
	  else
	    mutt_set_flag (Context, CURHDR, MUTT_READ, 1);

	  if (option (OPTRESOLVE))
	  {
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	    {
	      menu->current = menu->oldcurrent;
	      menu->redraw = REDRAW_CURRENT;
	    }
	    else
	      menu->redraw = REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw = REDRAW_CURRENT;
	  menu->redraw |= REDRAW_STATUS;
	}
	break;

      case OP_TOGGLE_WRITE:

	CHECK_IN_MAILBOX;
	if (mx_toggle_write (Context) == 0)
	  menu->redraw |= REDRAW_STATUS;
	break;

      case OP_MAIN_NEXT_THREAD:
      case OP_MAIN_NEXT_SUBTHREAD:
      case OP_MAIN_PREV_THREAD:
      case OP_MAIN_PREV_SUBTHREAD:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	switch (op)
	{
	  case OP_MAIN_NEXT_THREAD:
	    menu->current = mutt_next_thread (CURHDR);
	    break;

	  case OP_MAIN_NEXT_SUBTHREAD:
	    menu->current = mutt_next_subthread (CURHDR);
	    break;

	  case OP_MAIN_PREV_THREAD:
	    menu->current = mutt_previous_thread (CURHDR);
	    break;

	  case OP_MAIN_PREV_SUBTHREAD:
	    menu->current = mutt_previous_subthread (CURHDR);
	    break;
	}

	if (menu->current < 0)
	{
	  menu->current = menu->oldcurrent;
	  if (op == OP_MAIN_NEXT_THREAD || op == OP_MAIN_NEXT_SUBTHREAD)
	    mutt_error _("No more threads.");
	  else
	    mutt_error _("You are on the first thread.");
	}
	else if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw = REDRAW_MOTION;
	break;

      case OP_MAIN_ROOT_MESSAGE:
      case OP_MAIN_PARENT_MESSAGE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;

	if ((menu->current = mutt_parent_message (Context, CURHDR,
                                                  op == OP_MAIN_ROOT_MESSAGE)) < 0)
	{
	  menu->current = menu->oldcurrent;
	}
	else if (menu->menu == MENU_PAGER)
        {
          op = OP_DISPLAY_MESSAGE;
          continue;
        }
        else
          menu->redraw = REDRAW_MOTION;
	break;

      case OP_MAIN_SET_FLAG:
      case OP_MAIN_CLEAR_FLAG:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
	/* CHECK_ACL(MUTT_ACL_WRITE); */

	if (mutt_change_flag (tag ? NULL : CURHDR, (op == OP_MAIN_SET_FLAG)) == 0)
	{
	  menu->redraw = REDRAW_STATUS;
	  if (tag)
	    menu->redraw |= REDRAW_INDEX;
	  else if (option (OPTRESOLVE))
	  {
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	    {
	      menu->current = menu->oldcurrent;
	      menu->redraw |= REDRAW_CURRENT;
	    }
	    else
	      menu->redraw |= REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw |= REDRAW_CURRENT;
	}
	break;

      case OP_MAIN_COLLAPSE_THREAD:
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if ((Sort & SORT_MASK) != SORT_THREADS)
        {
	  mutt_error _("Threading is not enabled.");
	  break;
	}

	if (CURHDR->collapsed)
	{
	  menu->current = mutt_uncollapse_thread (Context, CURHDR);
	  mutt_set_virtual (Context);
	  if (option (OPTUNCOLLAPSEJUMP))
	    menu->current = mutt_thread_next_unread (Context, CURHDR);
	}
	else if (option (OPTCOLLAPSEUNREAD) || !UNREAD (CURHDR))
	{
	  menu->current = mutt_collapse_thread (Context, CURHDR);
	  mutt_set_virtual (Context);
	}
	else
	{
	  mutt_error _("Thread contains unread messages.");
	  break;
	}

	menu->redraw = REDRAW_INDEX | REDRAW_STATUS;

       break;

      case OP_MAIN_COLLAPSE_ALL:
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if ((Sort & SORT_MASK) != SORT_THREADS)
        {
          mutt_error _("Threading is not enabled.");
          break;
        }
        collapse_all (menu, 1);
	break;

      /* --------------------------------------------------------------------
       * These functions are invoked directly from the internal-pager
       */

      case OP_BOUNCE_MESSAGE:

	CHECK_ATTACH;
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	ci_bounce_message (tag ? NULL : CURHDR, &menu->redraw);
	break;

      case OP_CREATE_ALIAS:

        mutt_create_alias (Context && Context->vcount ? CURHDR->env : NULL, NULL);
	MAYBE_REDRAW (menu->redraw);
        menu->redraw |= REDRAW_CURRENT;
	break;

      case OP_QUERY:
	CHECK_ATTACH;
	mutt_query_menu (NULL, 0);
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_PURGE_MESSAGE:
      case OP_DELETE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message"));

	if (tag)
	{
	  mutt_tag_set_flag (MUTT_DELETE, 1);
          mutt_tag_set_flag (MUTT_PURGE, (op == OP_PURGE_MESSAGE));
	  if (option (OPTDELETEUNTAG))
	    mutt_tag_set_flag (MUTT_TAG, 0);
	  menu->redraw = REDRAW_INDEX;
	}
	else
	{
	  mutt_set_flag (Context, CURHDR, MUTT_DELETE, 1);
	  mutt_set_flag (Context, CURHDR, MUTT_PURGE, (op == OP_PURGE_MESSAGE));
	  if (option (OPTDELETEUNTAG))
	    mutt_set_flag (Context, CURHDR, MUTT_TAG, 0);
	  if (option (OPTRESOLVE))
	  {
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	    {
	      menu->current = menu->oldcurrent;
	      menu->redraw = REDRAW_CURRENT;
	    }
	    else if (menu->menu == MENU_PAGER)
	    {
	      op = OP_DISPLAY_MESSAGE;
	      continue;
	    }
	    else
	      menu->redraw |= REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw = REDRAW_CURRENT;
	}
	menu->redraw |= REDRAW_STATUS;
	break;

      case OP_DELETE_THREAD:
      case OP_DELETE_SUBTHREAD:
      case OP_PURGE_THREAD:

	CHECK_MSGCOUNT;
	CHECK_VISIBLE;
	CHECK_READONLY;
	/* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_DELETE, _("Cannot delete message(s)"));

	{
	  int subthread = (op == OP_DELETE_SUBTHREAD);
	  rc = mutt_thread_set_flag (CURHDR, MUTT_DELETE, 1, subthread);
	  if (rc == -1)
	    break;
	  if (op == OP_PURGE_THREAD)
	  {
	    rc = mutt_thread_set_flag (CURHDR, MUTT_PURGE, 1, subthread);
	    if (rc == -1)
	      break;
	  }

	  if (option (OPTDELETEUNTAG))
	    mutt_thread_set_flag (CURHDR, MUTT_TAG, 0, subthread);
	  if (option (OPTRESOLVE))
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	      menu->current = menu->oldcurrent;
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;

#ifdef USE_NNTP
      case OP_CATCHUP:
	CHECK_MSGCOUNT;
	CHECK_READONLY;
	CHECK_ATTACH
	if (Context && Context->magic == MUTT_NNTP)
	{
	  NNTP_DATA *nntp_data = Context->data;
	  if (mutt_newsgroup_catchup (nntp_data->nserv, nntp_data->group))
	    menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;
#endif

      case OP_DISPLAY_ADDRESS:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	mutt_display_address (CURHDR->env);
	break;

      case OP_ENTER_COMMAND:

	CurrentMenu = MENU_MAIN;
	mutt_enter_command ();
	mutt_check_rescore (Context);
	if (option (OPTFORCEREDRAWINDEX))
	  menu->redraw = REDRAW_FULL;
	unset_option (OPTFORCEREDRAWINDEX);
	unset_option (OPTFORCEREDRAWPAGER);
	break;

      case OP_EDIT_MESSAGE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
	CHECK_ATTACH;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_INSERT, _("Cannot edit message"));

	if (option (OPTPGPAUTODEC) && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
	  mutt_check_traditional_pgp (tag ? NULL : CURHDR, &menu->redraw);
        mutt_edit_message (Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;

	break;

      case OP_FORWARD_MESSAGE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_ATTACH;
	if (option (OPTPGPAUTODEC) && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
	  mutt_check_traditional_pgp (tag ? NULL : CURHDR, &menu->redraw);
	ci_send_message (SENDFORWARD, NULL, NULL, Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;


      case OP_FORGET_PASSPHRASE:
	crypt_forget_passphrase ();
	break;

      case OP_GROUP_REPLY:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_ATTACH;
	if (option (OPTPGPAUTODEC) && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
	  mutt_check_traditional_pgp (tag ? NULL : CURHDR, &menu->redraw);
	ci_send_message (SENDREPLY|SENDGROUPREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_EDIT_LABEL:

	CHECK_MSGCOUNT;
	CHECK_VISIBLE;
	CHECK_READONLY;
	rc = mutt_label_message(tag ? NULL : CURHDR);
	if (rc > 0) {
	  Context->changed = 1;
	  menu->redraw = REDRAW_FULL;
	  mutt_message ("%d label%s changed.", rc, rc == 1 ? "" : "s");
	}
	else {
	  mutt_message _("No labels changed.");
	}
	break;

      case OP_LIST_REPLY:

	CHECK_ATTACH;
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (option (OPTPGPAUTODEC) && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
	  mutt_check_traditional_pgp (tag ? NULL : CURHDR, &menu->redraw);
	ci_send_message (SENDREPLY|SENDLISTREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_MAIL:

	CHECK_ATTACH;
	ci_send_message (0, NULL, NULL, Context, NULL);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_MAIL_KEY:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
	CHECK_ATTACH;
	ci_send_message (SENDKEY, NULL, NULL, NULL, NULL);
	menu->redraw = REDRAW_FULL;
	break;


      case OP_EXTRACT_KEYS:
        if (!WithCrypto)
          break;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        crypt_extract_keys_from_messages(tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;
        break;


      case OP_CHECK_TRADITIONAL:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;
        if (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED))
	  mutt_check_traditional_pgp (tag ? NULL : CURHDR, &menu->redraw);

        if (menu->menu == MENU_PAGER)
        {
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
        break;

      case OP_PIPE:

	CHECK_MSGCOUNT;
	CHECK_VISIBLE;
	mutt_pipe_message (tag ? NULL : CURHDR);

#ifdef USE_IMAP
	/* in an IMAP folder index with imap_peek=no, piping could change
	 * new or old messages status to read. Redraw what's needed.
	 */
	if (Context->magic == MUTT_IMAP && !option (OPTIMAPPEEK))
	{
	  menu->redraw = (tag ? REDRAW_INDEX : REDRAW_CURRENT) | REDRAW_STATUS;
	}
#endif

	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_PRINT:

	CHECK_MSGCOUNT;
	CHECK_VISIBLE;
	mutt_print_message (tag ? NULL : CURHDR);

#ifdef USE_IMAP
	/* in an IMAP folder index with imap_peek=no, printing could change
	 * new or old messages status to read. Redraw what's needed.
	 */
	if (Context->magic == MUTT_IMAP && !option (OPTIMAPPEEK))
	{
	  menu->redraw = (tag ? REDRAW_INDEX : REDRAW_CURRENT) | REDRAW_STATUS;
	}
#endif

	break;

      case OP_MAIN_READ_THREAD:
      case OP_MAIN_READ_SUBTHREAD:

	CHECK_MSGCOUNT;
	CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_SEEN, _("Cannot mark message(s) as read"));

	rc = mutt_thread_set_flag (CURHDR, MUTT_READ, 1,
				   op == OP_MAIN_READ_THREAD ? 0 : 1);

	if (rc != -1)
	{
	  if (option (OPTRESOLVE))
	  {
	    if ((menu->current = (op == OP_MAIN_READ_THREAD ?
				  mutt_next_thread (CURHDR) : mutt_next_subthread (CURHDR))) == -1)
	      menu->current = menu->oldcurrent;
	    else if (menu->menu == MENU_PAGER)
	    {
	      op = OP_DISPLAY_MESSAGE;
	      continue;
	    }
	  }
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;


      case OP_MARK_MSG:

	CHECK_MSGCOUNT;
	CHECK_VISIBLE;
	if (CURHDR->env->message_id)
	{
	  char str[STRING], macro[STRING];
	  char buf[128];

	  buf[0] = '\0';
          /* L10N: This is the prompt for <mark-message>.  Whatever they
             enter will be prefixed by $mark_macro_prefix and will become
             a macro hotkey to jump to the currently selected message. */
	  if (!mutt_get_field (_("Enter macro stroke: "), buf, sizeof(buf),
	  		       MUTT_CLEAR) && buf[0])
	  {
	    snprintf(str, sizeof(str), "%s%s", MarkMacroPrefix, buf);
	    snprintf(macro, sizeof(macro),
		     "<search>~i \"%s\"\n", CURHDR->env->message_id);
            /* L10N: "message hotkey" is the key bindings menu description of a
               macro created by <mark-message>. */
	    km_bind(str, MENU_MAIN, OP_MACRO, macro, _("message hotkey"));

            /* L10N: This is echoed after <mark-message> creates a new hotkey
               macro.  %s is the hotkey string ($mark_macro_prefix followed
               by whatever they typed at the prompt.) */
	    snprintf(buf, sizeof(buf), _("Message bound to %s."), str);
	    mutt_message(buf);
	    dprint (1, (debugfile, "Mark: %s => %s\n", str, macro));
	  }
	}
	else
          /* L10N: This error is printed if <mark-message> cannot find a
             Message-ID for the currently selected message in the index. */
	  mutt_error _("No message ID to macro.");
	break;

      case OP_RECALL_MESSAGE:

	CHECK_ATTACH;
	ci_send_message (SENDPOSTPONED, NULL, NULL, Context, NULL);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_RESEND:

        CHECK_ATTACH;
        CHECK_MSGCOUNT;
        CHECK_VISIBLE;

        if (tag)
        {
	  for (j = 0; j < Context->vcount; j++)
	  {
	    if (Context->hdrs[Context->v2r[j]]->tagged)
	      mutt_resend_message (NULL, Context, Context->hdrs[Context->v2r[j]]);
	  }
	}
        else
	  mutt_resend_message (NULL, Context, CURHDR);

        menu->redraw = REDRAW_FULL;
        break;

#ifdef USE_NNTP
      case OP_FOLLOWUP:
      case OP_FORWARD_TO_GROUP:

	CHECK_MSGCOUNT;
	CHECK_VISIBLE;

      case OP_POST:

	CHECK_ATTACH;
	if (op != OP_FOLLOWUP || !CURHDR->env->followup_to ||
	    mutt_strcasecmp (CURHDR->env->followup_to, "poster") ||
	    query_quadoption (OPT_FOLLOWUPTOPOSTER,
	    _("Reply by mail as poster prefers?")) != MUTT_YES)
	{
	  if (Context && Context->magic == MUTT_NNTP &&
	      !((NNTP_DATA *)Context->data)->allowed &&
	      query_quadoption (OPT_TOMODERATED,
	      _("Posting to this group not allowed, may be moderated. Continue?")) != MUTT_YES)
	    break;
	  if (op == OP_POST)
	    ci_send_message (SENDNEWS, NULL, NULL, Context, NULL);
	  else
	  {
	    CHECK_MSGCOUNT;
	    ci_send_message ((op == OP_FOLLOWUP ? SENDREPLY : SENDFORWARD) |
			SENDNEWS, NULL, NULL, Context, tag ? NULL : CURHDR);
	  }
	  menu->redraw = REDRAW_FULL;
	  break;
	}
#endif

      case OP_REPLY:

	CHECK_ATTACH;
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	if (option (OPTPGPAUTODEC) && (tag || !(CURHDR->security & PGP_TRADITIONAL_CHECKED)))
	  mutt_check_traditional_pgp (tag ? NULL : CURHDR, &menu->redraw);
	ci_send_message (SENDREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_SHELL_ESCAPE:

	mutt_shell_escape ();
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_TAG_THREAD:
      case OP_TAG_SUBTHREAD:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	rc = mutt_thread_set_flag (CURHDR, MUTT_TAG, !CURHDR->tagged,
				   op == OP_TAG_THREAD ? 0 : 1);

	if (rc != -1)
	{
	  if (option (OPTRESOLVE))
	  {
	    if (op == OP_TAG_THREAD)
	      menu->current = mutt_next_thread (CURHDR);
	    else
	      menu->current = mutt_next_subthread (CURHDR);

	    if (menu->current == -1)
	      menu->current = menu->oldcurrent;
	  }
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;

      case OP_UNDELETE:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message"));

	if (tag)
	{
	  mutt_tag_set_flag (MUTT_DELETE, 0);
	  mutt_tag_set_flag (MUTT_PURGE, 0);
	  menu->redraw = REDRAW_INDEX;
	}
	else
	{
	  mutt_set_flag (Context, CURHDR, MUTT_DELETE, 0);
	  mutt_set_flag (Context, CURHDR, MUTT_PURGE, 0);
	  if (option (OPTRESOLVE) && menu->current < Context->vcount - 1)
	  {
	    menu->current++;
	    menu->redraw = REDRAW_MOTION_RESYNCH;
	  }
	  else
	    menu->redraw = REDRAW_CURRENT;
	}
	menu->redraw |= REDRAW_STATUS;
	break;

      case OP_UNDELETE_THREAD:
      case OP_UNDELETE_SUBTHREAD:

	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	CHECK_READONLY;
        /* L10N: CHECK_ACL */
	CHECK_ACL(MUTT_ACL_DELETE, _("Cannot undelete message(s)"));

	rc = mutt_thread_set_flag (CURHDR, MUTT_DELETE, 0,
				   op == OP_UNDELETE_THREAD ? 0 : 1);
        if (rc != -1)
          rc = mutt_thread_set_flag (CURHDR, MUTT_PURGE, 0,
                                     op == OP_UNDELETE_THREAD ? 0 : 1);
	if (rc != -1)
	{
	  if (option (OPTRESOLVE))
	  {
	    if (op == OP_UNDELETE_THREAD)
	      menu->current = mutt_next_thread (CURHDR);
	    else
	      menu->current = mutt_next_subthread (CURHDR);

	    if (menu->current == -1)
	      menu->current = menu->oldcurrent;
	  }
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;

      case OP_VERSION:
	mutt_version ();
	break;

      case OP_BUFFY_LIST:
	mutt_buffy_list ();
	break;

      case OP_VIEW_ATTACHMENTS:
	CHECK_MSGCOUNT;
        CHECK_VISIBLE;
	mutt_view_attachments (CURHDR);
	if (Context && CURHDR->attach_del)
	  Context->changed = 1;
	menu->redraw = REDRAW_FULL;
	break;

      case OP_END_COND:
	break;

      case OP_WHAT_KEY:
	mutt_what_key();
	break;

#ifdef USE_SIDEBAR
      case OP_SIDEBAR_NEXT:
      case OP_SIDEBAR_NEXT_NEW:
      case OP_SIDEBAR_PAGE_DOWN:
      case OP_SIDEBAR_PAGE_UP:
      case OP_SIDEBAR_PREV:
      case OP_SIDEBAR_PREV_NEW:
        mutt_sb_change_mailbox (op);
        break;

      case OP_SIDEBAR_TOGGLE_VISIBLE:
	toggle_option (OPTSIDEBAR);
        mutt_reflow_windows();
	menu->redraw = REDRAW_FULL;
	break;

      case OP_SIDEBAR_TOGGLE_VIRTUAL:
	mutt_sb_toggle_virtual();
	break;
#endif
      default:
	if (menu->menu == MENU_MAIN)
	  km_error_key (MENU_MAIN);
    }

#ifdef USE_NOTMUCH
    if (Context)
      nm_debug_check(Context);
#endif

    if (menu->menu == MENU_PAGER)
    {
      mutt_clear_pager_position ();
      menu->menu = MENU_MAIN;
      menu->redraw = REDRAW_FULL;
#if 0
      set_option (OPTWEED); /* turn header weeding back on. */
#endif
    }

    if (done) break;
  }

  mutt_menuDestroy (&menu);
  return (close);
}

void mutt_set_header_color (CONTEXT *ctx, HEADER *curhdr)
{
  COLOR_LINE *color;

  if (!curhdr)
    return;

  for (color = ColorIndexList; color; color = color->next)
   if (mutt_pattern_exec (color->color_pattern, MUTT_MATCH_FULL_ADDRESS, ctx, curhdr))
   {
      curhdr->pair = color->pair;
      return;
   }
  curhdr->pair = ColorDefs[MT_COLOR_NORMAL];
}
