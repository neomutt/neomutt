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
#include "mutt_menu.h"
#include "mailbox.h"
#include "mapping.h"
#include "sort.h"
#include "buffy.h"



#ifdef _PGPPATH
#include "pgp.h"
#endif










#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define CHECK_MSGCOUNT if (!Context) \
	{ \
	  	mutt_flushinp (); \
		mutt_error ("No mailbox is open."); \
		break; \
	} \
	else if (!Context->msgcount) \
	{ \
	  	mutt_flushinp (); \
		mutt_error ("There are no messages."); \
		break; \
	}

#define CHECK_READONLY if (Context->readonly) \
			{ \
			  	mutt_flushinp (); \
				mutt_error ("Mailbox is read-only."); \
				break; \
			}

#define CURHDR Context->hdrs[Context->v2r[menu->current]]
#define OLDHDR Context->hdrs[Context->v2r[menu->oldcurrent]]
#define UNREAD(h) mutt_thread_contains_unread (Context, h)

extern const char *ReleaseDate;

void index_make_entry (char *s, size_t l, MUTTMENU *menu, int num)
{
  format_flag flag = M_FORMAT_MAKEPRINT | M_FORMAT_ARROWCURSOR;
  int reverse = Sort & SORT_REVERSE, edgemsgno;
  HEADER *tmp, *h = Context->hdrs[Context->v2r[num]];

  if ((Sort & SORT_MASK) == SORT_THREADS && h->tree)
  {
    flag |= M_FORMAT_TREE; /* display the thread tree */

    if (h->display_subject)
      flag |= M_FORMAT_FORCESUBJ;
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

      for (tmp = h->parent; tmp; tmp = tmp->parent)
      {
	if ((reverse && tmp->msgno > edgemsgno)
	    || (!reverse && tmp->msgno < edgemsgno))
	{
	  flag |= M_FORMAT_FORCESUBJ;
	  break;
	}
	else if (tmp->virtual >= 0)
	  break;
      }
      if ((flag & M_FORMAT_FORCESUBJ) && h->prev)
      {
	for (tmp = h->prev; tmp; tmp = tmp->prev)
	{
	  if ((reverse && tmp->msgno > edgemsgno)
	      || (!reverse && tmp->msgno < edgemsgno))
	    break;
	  else if (tmp->virtual >= 0)
	  {
	    flag &= ~M_FORMAT_FORCESUBJ;
	    break;
	  }
	}
      }
    }
  }

  _mutt_make_string (s, l, NONULL (HdrFmt), Context, h, flag | M_FORMAT_INDEX);
}

int index_color (int index_no)
{
  return Context->hdrs[Context->v2r[index_no]]->pair;
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
    mutt_error ("Cannot toggle write on a readonly mailbox!");
    return -1;
  }

  if (ctx->dontwrite)
  {
    ctx->dontwrite = 0;
    mutt_message ("Changes to folder will be written on folder exit.");
  }
  else
  {
    ctx->dontwrite = 1;
    mutt_message ("Changes to folder will not be written.");
  }

  return 0;
}

struct mapping_t IndexHelp[] = {
  { "Quit",  OP_QUIT },
  { "Del",   OP_DELETE },
  { "Undel", OP_UNDELETE },
  { "Save",  OP_SAVE },
  { "Mail",  OP_MAIL },
  { "Reply", OP_REPLY },
  { "Group", OP_GROUP_REPLY },
  { "Help",  OP_HELP },
  { NULL }
};

/* This function handles the message index window as well as commands returned
 * from the pager (MENU_PAGER).
 */
int mutt_index_menu (int attach_msg /* invoked while attaching a message */)
{
  char buf[LONG_STRING], attach_msg_status[LONG_STRING] = "", helpstr[SHORT_STRING];
  int op = OP_NULL;                 /* function to execute */
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

  menu = mutt_new_menu ();
  menu->menu = MENU_MAIN;
  menu->offset = 1;
  menu->pagelen = LINES - 3;
  menu->make_entry = index_make_entry;
  menu->color = index_color;
  menu->current = ci_first_message ();
  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_MAIN, IndexHelp);
  
  if (!attach_msg) 
    mutt_buffy_check(1); /* force the buffy check after we enter the folder */

  FOREVER
  {
    tag = 0; /* clear the tag-prefix */

    menu->max = Context ? Context->vcount : 0;
    oldcount = Context ? Context->msgcount : 0;

    if (Context && !attach_msg)
    {
      int check;
      /* check for new mail in the mailbox.  If nonzero, then something has
       * changed about the file (either we got new mail or the file was
       * modified underneath us.)
       */
      index_hint = (Context->vcount) ? CURHDR->index : 0;

      if ((check = mx_check_mailbox (Context, &index_hint)) < 0)
      {
	if (!Context->path)
	{
	  /* fatal error occurred */
	  safe_free ((void **) &Context);
	  menu->redraw = REDRAW_FULL;
	}

	set_option (OPTSEARCHINVALID);
      }
      else if (check == M_NEW_MAIL || check == M_REOPENED)
      {
	int *save_new;

	/* take note of the current message */
	if (oldcount)
	{
	  if (menu->current < Context->vcount)
	    menu->oldcurrent = index_hint;
	  else
	    oldcount = 0; /* invalid message number! */
	}

	/* We are in a limited view. Check if the new message(s) satisfy
	 * the limit criteria. If they do, set their virtual msgno so that
	 * they will be visible in the limited view */
        if (Context->pattern)
	{
#define THIS_BODY Context->hdrs[j]->content
	  if (oldcount || check == M_REOPENED)
	    for (j = (check == M_REOPENED) ? 0 : oldcount; j < Context->msgcount; j++)
	  {
            if (mutt_pattern_exec (Context->limit_pattern,
	                           M_MATCH_FULL_ADDRESS, 
				    Context, Context->hdrs[j]))
            {
		Context->hdrs[j]->virtual = Context->vcount;
		Context->v2r[Context->vcount] = j;
		Context->hdrs[j]->limited = 1;
		Context->vcount++;
		Context->vsize += THIS_BODY->length + THIS_BODY->offset - THIS_BODY->hdr_offset;
            }
	  }
#undef THIS_BODY
        }

	/* save the list of new messages */
	if (oldcount && check != M_REOPENED)
	{
	  save_new = (int *) safe_malloc (sizeof (int) * (Context->msgcount - oldcount));
	  for (j = oldcount; j < Context->msgcount; j++)
	    save_new[j-oldcount] = Context->hdrs[j]->index;
        }

	/* if the mailbox was reopened, need to rethread from scratch */
	set_option (OPTSORTCOLLAPSE);
	mutt_sort_headers (Context, (check == M_REOPENED));
	unset_option (OPTSORTCOLLAPSE);

	/* uncollapse threads with new mail */
	if ((Sort & SORT_MASK) == SORT_THREADS)
	{
	  if (check == M_REOPENED)
	  {
	    HEADER *h;

	    h = Context->tree;
	    Context->collapsed = 0;

	    while (h)
	    {
	      mutt_uncollapse_thread (Context, h);
	      h = h->next;
	    }
	    mutt_set_virtual (Context);
	  }
	  else if (oldcount)
	  {
	    for (j = 0; j < Context->msgcount - oldcount; j++)
	    {
	      int k;
	      
	      for (k = 0; k < Context->msgcount; k++)
	      {
		HEADER *h = Context->hdrs[k];
		if (h->index == save_new[j] && (!Context->pattern || h->limited))
		  mutt_uncollapse_thread (Context, h);
	      }
	    }
	    FREE (&save_new);
	    mutt_set_virtual (Context);
	  }
	}

	menu->current = -1;
	if (oldcount)
	{
	  /* restore the current message to the message it was pointing to */
	  for (j = 0; j < Context->vcount; j++)
	    if (Context->hdrs[Context->v2r[j]]->index == menu->oldcurrent)
	    {
	      menu->current = j;
	      break;
	    }
	}

	if (menu->current < 0)
	  menu->current = ci_first_message ();

	/* notify the user of new mail */
	if (check == M_REOPENED)
	  mutt_error ("Mailbox was externally modified.  Flags may be wrong.");
	else
	{
	  mutt_message ("New mail in this mailbox.");
	  if (option (OPTBEEPNEW))
	    beep ();
	}
	/* avoid the message being overwritten by buffy */
	do_buffy_notify = 0;

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
       if (mutt_buffy_notify () && option (OPTBEEPNEW))
 	beep ();
     }
     else
       do_buffy_notify = 1;
    }

    mutt_curs_set (0);

    if (menu->redraw & REDRAW_FULL)
    {
      menu_redraw_full (menu);
      mutt_show_error ();
    }

    if (menu->menu == MENU_MAIN)
    {
      if (Context)
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
	if (attach_msg)
	{
	 char from_folder [STRING];
	 strfcpy(from_folder, Context->path, sizeof (from_folder));
	 mutt_pretty_mailbox (from_folder);
	 snprintf (attach_msg_status, sizeof (attach_msg_status), 
	           "Folder: %s Tagged messages will be attached upon exiting", 
		   from_folder);
	 snprintf (buf, sizeof (buf), M_MODEFMT, attach_msg_status);
	}
	else
	 menu_status_line (buf, sizeof (buf), menu, NONULL (Status));

	CLEARLINE (option (OPTSTATUSONTOP) ? 0 : LINES-2);
	SETCOLOR (MT_COLOR_STATUS);
	printw ("%-*.*s", COLS, COLS, buf);
	SETCOLOR (MT_COLOR_NORMAL);
	menu->redraw &= ~REDRAW_STATUS;
      }

      menu->redraw = 0;
      menu->oldcurrent = menu->current;

      if (option (OPTARROWCURSOR))
	move (menu->current - menu->top + menu->offset, 2);
      else
	move (menu->current - menu->top + menu->offset, COLS - 1);
      mutt_refresh ();

      if (Timeout > 0)
      {
	timeout (Timeout * 1000); /* milliseconds */      
	op = mutt_getch ();
	timeout (-1); /* restore blocking operation */
	if (op != -1)
	{
	  mutt_ungetch (op);
	  op = km_dokey (MENU_MAIN);
	}
      }
      else
	op = km_dokey (MENU_MAIN);
      mutt_curs_set (1);

#if defined (USE_SLANG_CURSES) || defined (HAVE_RESIZETERM)
      if (Signals & S_SIGWINCH)
      {
	mutt_flushinp ();
	mutt_resize_screen ();
	menu->redraw = REDRAW_FULL;
	menu->menu = MENU_MAIN;
	Signals &= ~S_SIGWINCH;
	menu->top = 0; /* so we scroll the right amount */
	continue;
      }
#endif

      if (op == -1)
	continue; /* either user abort or timeout */

      /* special handling for the tag-prefix function */
      if (op == OP_TAG_PREFIX)
      {
	if (!Context)
	{
	  mutt_error ("No mailbox is open.");
	  continue;
	}

	if (!Context->tagged)
	{
	  mutt_error ("No tagged messages.");
	  continue;
	}
	tag = 1;

	/* give visual indication that the next command is a tag- command */
	mvaddstr (LINES - 1, 0, "tag-");
	clrtoeol ();

	/* get the real command */
	if ((op = km_dokey (MENU_MAIN)) == OP_TAG_PREFIX)
	{
	  /* abort tag sequence */
	  CLEARLINE (LINES-1);
	  continue;
	}
      }
      else if (option (OPTAUTOTAG) && Context && Context->tagged)
	tag = 1;

      mutt_clear_error ();
    }
    else
      mutt_curs_set (1);	/* fallback from the pager */

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

      case OP_JUMP:

	CHECK_MSGCOUNT;
	mutt_ungetch (LastKey);
	buf[0] = 0;
	if (mutt_get_field ("Jump to message: ", buf, sizeof (buf), 0) != 0 ||
	    !buf[0])
	  break;

	if (! isdigit ((unsigned char) buf[0]))
	{
	  mutt_error ("Argument must be a message number.");
	  break;
	}

	i = atoi (buf);
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
	    mutt_error ("That message is not visible.");
	}
	else
	  mutt_error ("Invalid message number.");

	break;

	/* --------------------------------------------------------------------
	 * `index' specific commands
	 */

      case OP_MAIN_DELETE_PATTERN:

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	mutt_pattern_func (M_DELETE, "Delete messages matching: ");
	menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

#ifdef USE_POP
      case OP_MAIN_FETCH_MAIL:

	mutt_fetchPopMail ();
	menu->redraw = REDRAW_FULL;
	break;
#endif /* USE_POP */

      case OP_HELP:

	mutt_help (MENU_MAIN);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_MAIN_SHOW_LIMIT:
        CHECK_MSGCOUNT;
	if (!Context->pattern)
	   mutt_message ("No limit pattern is in effect.");
	else
	{
	   char buf[STRING];
	   snprintf (buf, sizeof(buf), "Limit: %s",Context->pattern);
           mutt_message ("%s", buf);
	}
        break;

      case OP_MAIN_LIMIT:

	CHECK_MSGCOUNT;
	menu->oldcurrent = Context->vcount ? CURHDR->index : -1;
	if (mutt_pattern_func (M_LIMIT, "Limit to messages matching: ") == 0)
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
	  mutt_linearize_tree (Context, 0);
	}
	break;	  

      case OP_QUIT:

	close = op;
	if (attach_msg)
	{
	 done = 1;
	 break;
	}

	if (query_quadoption (OPT_QUIT, "Quit Mutt?") == M_YES)
	{ 
	  if (!Context || mx_close_mailbox (Context) == 0)
	    done = 1;
	  else
	    menu->redraw = REDRAW_FULL; /* new mail arrived? */
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
	    menu->oldcurrent = CURHDR->index;
	    mutt_sort_headers (Context, 0);

	    /* try to restore the current message */
	    for (i = 0; i < Context->vcount; i++)
	    {
	      if (Context->hdrs[Context->v2r[i]]->index == menu->oldcurrent)
		menu->current = i;
	    }
	    menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	    set_option (OPTSEARCHINVALID);
	  }
	}
	break;

      case OP_MAIN_SYNC_FOLDER:

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	{
	  int oldvcount = Context->vcount;
	  int dcount = 0;

	  /* calculate the number of messages _above_ the cursor,
	   * so we can keep the cursor on the current message
	   */ 
	  for (j = 0; j < menu->current; j++)
	  {
	    if (Context->hdrs[Context->v2r[j]]->deleted)
	      dcount++;
	  }
	  if (mx_sync_mailbox (Context) == 0)
	  {
	    if (Context->vcount != oldvcount)
	      menu->current -= dcount;
	    set_option (OPTSEARCHINVALID);
	  }
	  
	  /* do a sanity check even if mx_sync_mailbox failed.
	   */

	  if (menu->current < 0 || menu->current >= Context->vcount)
	    menu->current = ci_first_message ();
	}

	/* check for a fatal error, or all messages deleted */
	if (!Context->path)
	  safe_free ((void **) &Context);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_TAG:

	CHECK_MSGCOUNT;
	if (tag && !option (OPTAUTOTAG))
	{
	  for (j = 0; j < Context->vcount; j++)
	    mutt_set_flag (Context, Context->hdrs[Context->v2r[j]], M_TAG, 0);
	  menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
	}
	else
	{
	  mutt_set_flag (Context, CURHDR, M_TAG, !CURHDR->tagged);
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
	mutt_pattern_func (M_TAG, "Tag messages matching: ");
	menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

      case OP_MAIN_UNDELETE_PATTERN:

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	if (mutt_pattern_func (M_UNDELETE, "Undelete messages matching: ") == 0)
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

      case OP_MAIN_UNTAG_PATTERN:

	CHECK_MSGCOUNT;
	if (mutt_pattern_func (M_UNTAG, "Untag messages matching: ") == 0)
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	break;

	/* --------------------------------------------------------------------
	 * The following operations can be performed inside of the pager.
	 */

      case OP_MAIN_CHANGE_FOLDER:
      
	if (attach_msg || option (OPTREADONLY))
	  op = OP_MAIN_CHANGE_FOLDER_READONLY;

	/* fallback to the readonly case */

      case OP_MAIN_CHANGE_FOLDER_READONLY:

        if (op == OP_MAIN_CHANGE_FOLDER)
          cp = "Open mailbox";
        else
          cp = "Open mailbox in read-only mode";

	buf[0] = '\0';
	mutt_buffy (buf);

	if (mutt_enter_fname (cp, buf, sizeof (buf), &menu->redraw, 1) == -1)
	  break;
	if (!buf[0])
	{
	  CLEARLINE (LINES-1);
	  break;
	}

	mutt_expand_path (buf, sizeof (buf));
	if (mx_get_magic (buf) <= 0)
	{
	  mutt_error ("%s is not a mailbox.", buf);
	  break;
	}

        if (Context)
        {
	  FREE (&LastFolder);
	  LastFolder = safe_strdup (Context->path);
	}

	if (Context)
	{
	  if (mx_close_mailbox (Context) != 0)
	  {
	    menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	    break;
	  }
	  safe_free ((void **) &Context);
	}

	sleep (1); /* give a second to read the mailbox status */

	mutt_folder_hook (buf);

	if ((Context = mx_open_mailbox (buf, 
					(option (OPTREADONLY) || op == OP_MAIN_CHANGE_FOLDER_READONLY) ?
					M_READONLY : 0, NULL)) != NULL)
	{
	  menu->current = ci_first_message ();
	}
	else
	  menu->current = 0;

	mutt_clear_error ();
	mutt_buffy_check(1); /* force the buffy check after we have changed
			      the folder */
	menu->redraw = REDRAW_FULL;
	set_option (OPTSEARCHINVALID);
	break;

      case OP_DISPLAY_MESSAGE:
      case OP_DISPLAY_HEADERS: /* don't weed the headers */

	CHECK_MSGCOUNT;
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
	  if (option (OPTUNCOLLAPSEJUMPNEW))
	    menu->current = mutt_thread_next_new (Context, CURHDR);
	}
 
	if ((op = mutt_display_message (CURHDR, attach_msg_status)) == -1)
	{
	  unset_option (OPTNEEDRESORT);
	  break;
	}

	if (option (OPTNEEDRESORT) && Context && Context->msgcount)
	{
	  menu->oldcurrent = CURHDR->index;
	  mutt_sort_headers (Context, 0);

	  /* try to restore the current message */
	  for (i = 0; i < Context->vcount; i++)
	  {
	    if (Context->hdrs[Context->v2r[i]]->index == menu->oldcurrent)
	      menu->current = i;
	  }
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}

	menu->menu = MENU_PAGER;
	menu->oldcurrent = menu->current;
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
				  "Exit Mutt without saving?") == M_YES))
	  done = 1;
	break;

      case OP_MAIN_NEXT_UNDELETED:

	CHECK_MSGCOUNT;
	if (menu->current >= Context->vcount - 1)
	{
	  if (menu->menu == MENU_MAIN)
	    mutt_error ("You are on the last message.");
	  break;
	}
	if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	{
	  menu->current = menu->oldcurrent;
	  if (menu->menu == MENU_MAIN)
	    mutt_error ("No undeleted messages.");
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
	if (menu->current >= Context->vcount - 1)
	{
	  if (menu->menu == MENU_MAIN)
	    mutt_error ("You are on the last message.");
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
	if (menu->current < 1)
	{
	  mutt_error ("You are on the first message.");
	  break;
	}
	if ((menu->current = ci_previous_undeleted (menu->current)) == -1)
	{
	  menu->current = menu->oldcurrent;
	  if (menu->menu == MENU_MAIN)
	    mutt_error ("No undeleted messages.");
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
	if (menu->current < 1)
	{
	  if (menu->menu == MENU_MAIN) mutt_error ("You are on the first message.");
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

      case OP_COPY_MESSAGE:
      case OP_SAVE:
      case OP_DECODE_COPY:
      case OP_DECODE_SAVE:

	CHECK_MSGCOUNT;
	if (mutt_save_message (tag ? NULL : CURHDR,
			       (op == OP_SAVE || op == OP_DECODE_SAVE),
			       (op == OP_DECODE_SAVE || op == OP_DECODE_COPY),
			       &menu->redraw) == 0 &&
	    (op == OP_SAVE || op == OP_DECODE_SAVE))
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

	CHECK_MSGCOUNT;
	i = menu->current;
	menu->current = -1;
	for (j = 0; j != Context->vcount; j++)
	{
#define CUR Context->hdrs[Context->v2r[i]]
	  if (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_NEXT_UNREAD)
	  {
	    i++;
	    if (i > Context->vcount - 1)
	    {
	      mutt_message ("Search wrapped to top.");
	      i = 0;
	    }
	  }
	  else
	  {
	    i--;
	    if (i < 0)
	    {
	      mutt_message ("Search wrapped to bottom.");
	      i = Context->vcount - 1;
	    }
	  }

	  if ((!CUR->deleted && !CUR->read) ||
	      (CUR->collapsed && !CUR->deleted && UNREAD (CUR)))
	  {
	    if (op == OP_MAIN_NEXT_UNREAD || op == OP_MAIN_PREV_UNREAD ||
		!CUR->old)
	    {
	      menu->current = i;
	      break;
	    }
	  }
	}
#undef CUR
	if (menu->current == -1)
	{
	  char *tmp = "";
	  menu->current = menu->oldcurrent;
	  if (Context->pattern)
	    tmp = " in this limited view";
	  mutt_error ("%s%s.", (op == OP_MAIN_NEXT_NEW || op == OP_MAIN_PREV_NEW) ? "No new messages" : "No unread messages", NONULL (tmp));
	}
	else if (menu->menu == MENU_PAGER)
	{
	  op = OP_DISPLAY_MESSAGE;
	  continue;
	}
	else
	  menu->redraw = REDRAW_MOTION;
	break;

      case OP_FLAG_MESSAGE:

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	mutt_set_flag (Context, CURHDR, M_FLAG, !CURHDR->flagged);

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
	break;

      case OP_TOGGLE_NEW:

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	if (tag)
	{
	  for (j = 0; j < Context->vcount; j++)
	  {
	    if (Context->hdrs[Context->v2r[j]]->tagged)
	    {
	      if (Context->hdrs[Context->v2r[j]]->read ||
		  Context->hdrs[Context->v2r[j]]->old)
		mutt_set_flag (Context, Context->hdrs[Context->v2r[j]], M_NEW, 1);
	      else
		mutt_set_flag (Context, Context->hdrs[Context->v2r[j]], M_READ, 1);
	    }
	  }
	  menu->redraw = REDRAW_STATUS | REDRAW_INDEX;
	}
	else
	{
	  if (CURHDR->read || CURHDR->old)
	    mutt_set_flag (Context, CURHDR, M_NEW, 1);
	  else
	    mutt_set_flag (Context, CURHDR, M_READ, 1);

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

	CHECK_MSGCOUNT;
	if (mx_toggle_write (Context) == 0)
	  menu->redraw |= REDRAW_STATUS;
	break;

      case OP_MAIN_NEXT_THREAD:
      case OP_MAIN_NEXT_SUBTHREAD:
      case OP_MAIN_PREV_THREAD:
      case OP_MAIN_PREV_SUBTHREAD:

	CHECK_MSGCOUNT;
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
	    mutt_error ("No more threads.");
	  else
	    mutt_error ("You are on the first thread.");
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
	CHECK_READONLY;
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
	
	if (CURHDR->collapsed)
	{
	  menu->current = mutt_uncollapse_thread (Context, CURHDR);
	  mutt_set_virtual (Context);
	  if (option (OPTUNCOLLAPSEJUMPNEW))
	    menu->current = mutt_thread_next_new (Context, CURHDR);
	}
	else if (option (OPTCOLLAPSENEW) || !UNREAD (CURHDR))
	{
	  menu->current = mutt_collapse_thread (Context, CURHDR);
	  mutt_set_virtual (Context);
	}
	else
	{
	  mutt_error ("Thread contains unread messages.");
	  break;
	}

	menu->redraw = REDRAW_INDEX | REDRAW_STATUS;

       break;

      case OP_MAIN_COLLAPSE_ALL:
       CHECK_MSGCOUNT;

       if ((Sort & SORT_MASK) != SORT_THREADS)
       {
         mutt_error ("Threading is not enabled.");
         break;
       }

       {
	 HEADER *h, *base;
	 int final;

	 if (CURHDR->collapsed)
	   final = mutt_uncollapse_thread (Context, CURHDR);
	 else if (option (OPTCOLLAPSENEW) || !UNREAD (CURHDR))
	   final = mutt_collapse_thread (Context, CURHDR);
	 else
	   final = CURHDR->virtual;

	 base = Context->hdrs[Context->v2r[final]];
	
	 h = Context->tree;
	 Context->collapsed = !Context->collapsed;
	 while (h)
	 {
	   if (h->collapsed != Context->collapsed)
	   {
	     if (h->collapsed)
	       mutt_uncollapse_thread (Context, h);
	     else if (option (OPTCOLLAPSENEW) || !UNREAD (h))
	       mutt_collapse_thread (Context, h);
	   }
	   h = h->next;
	 }

	 mutt_set_virtual (Context);
	 for (j = 0; j < Context->vcount; j++)
	 {
	   if (Context->hdrs[Context->v2r[j]]->index == base->index)
	   {
	     menu->current = j;
	     break;
	   }
	 }

	 menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	 break;
       }

	/* --------------------------------------------------------------------
	 * These functions are invoked directly from the internal-pager
	 */

      case OP_BOUNCE_MESSAGE:

	CHECK_MSGCOUNT;
	ci_bounce_message (tag ? NULL : CURHDR, &menu->redraw);
	break;

      case OP_CREATE_ALIAS:

	mutt_create_alias (Context && Context->vcount ? CURHDR->env : NULL, NULL);
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_QUERY:
	mutt_query_menu (NULL, 0);
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_DELETE:

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	if (tag)
	{
	  mutt_tag_set_flag (M_DELETE, 1);
	  menu->redraw = REDRAW_INDEX;
	}
	else
	{
	  mutt_set_flag (Context, CURHDR, M_DELETE, 1);
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

	CHECK_MSGCOUNT;
	CHECK_READONLY;

	rc = mutt_thread_set_flag (CURHDR, M_DELETE, 1,
				   op == OP_DELETE_THREAD ? 0 : 1);

	if (rc != -1)
	{
	  if (option (OPTRESOLVE))
	    if ((menu->current = ci_next_undeleted (menu->current)) == -1)
	      menu->current = menu->oldcurrent;
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;

      case OP_DISPLAY_ADDRESS:

	CHECK_MSGCOUNT;
	mutt_display_address (CURHDR->env->from);
	break;

      case OP_ENTER_COMMAND:

	mutt_enter_command ();
	mutt_check_rescore (Context);
	if (option (OPTNEEDRESORT) && Context && Context->msgcount)
	{
	  menu->oldcurrent = CURHDR->index;
	  mutt_sort_headers (Context, 0);

	  /* try to restore the current message */
	  for (i = 0; i < Context->vcount; i++)
	  {
	    if (Context->hdrs[Context->v2r[i]]->index == menu->oldcurrent)
	      menu->current = i;
	  }
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	if (option (OPTFORCEREDRAWINDEX))
	  menu->redraw = REDRAW_FULL;
	unset_option (OPTFORCEREDRAWINDEX);
	unset_option (OPTFORCEREDRAWPAGER);
	break;

      case OP_FORWARD_MESSAGE:

	CHECK_MSGCOUNT;
	ci_send_message (SENDFORWARD, NULL, NULL, Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;



#ifdef _PGPPATH
      case OP_FORGET_PASSPHRASE:

	mutt_forget_passphrase ();
	break;
#endif /* _PGPPATH */



      case OP_GROUP_REPLY:

	CHECK_MSGCOUNT;
	ci_send_message (SENDREPLY|SENDGROUPREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_LIST_REPLY:

	CHECK_MSGCOUNT;
	ci_send_message (SENDREPLY|SENDLISTREPLY, NULL, NULL, Context, tag ? NULL : CURHDR);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_MAIL:

	ci_send_message (0, NULL, NULL, NULL, NULL);
	menu->redraw = REDRAW_FULL;
	break;







#ifdef _PGPPATH
      case OP_MAIL_KEY:
	
	ci_send_message (SENDKEY, NULL, NULL, NULL, NULL);
	menu->redraw = REDRAW_FULL;
	break;
      
      case OP_EXTRACT_KEYS:
      
        CHECK_MSGCOUNT;
        pgp_extract_keys_from_messages(tag ? NULL : CURHDR);
        menu->redraw = REDRAW_FULL;
        break;
      
#endif /* _PGPPATH */







      case OP_PIPE:

	CHECK_MSGCOUNT;
	mutt_pipe_message (tag ? NULL : CURHDR);
	break;

      case OP_PRINT:

	CHECK_MSGCOUNT;
	mutt_print_message (tag ? NULL : CURHDR);
	break;

      case OP_MAIN_READ_THREAD:
      case OP_MAIN_READ_SUBTHREAD:

	CHECK_MSGCOUNT;
	CHECK_READONLY;

	rc = mutt_thread_set_flag (CURHDR, M_READ, 1,
				   op == OP_MAIN_READ_THREAD ? 0 : 1);

	if (rc != -1)
	{
	  if (option (OPTRESOLVE))
	  {
	    if ((menu->oldcurrent = ci_next_undeleted (menu->current)) == -1)
	      menu->oldcurrent = menu->current;
	  }
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;

      case OP_RECALL_MESSAGE:

	ci_send_message (SENDPOSTPONED, NULL, NULL, Context, NULL);
	menu->redraw = REDRAW_FULL;
	break;

      case OP_REPLY:

	CHECK_MSGCOUNT;
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
	rc = mutt_thread_set_flag (CURHDR, M_TAG, !CURHDR->tagged,
				   op == OP_TAG_THREAD ? 0 : 1);
	
	if (rc != -1)
	{
	  if (option (OPTRESOLVE))
	  {
	    menu->current = mutt_next_thread (CURHDR);

	    if (menu->current == -1)
	      menu->current = menu->oldcurrent;
	  }
	  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;
	}
	break;

      case OP_UNDELETE:

	CHECK_MSGCOUNT;
	CHECK_READONLY;
	if (tag)
	{
	  mutt_tag_set_flag (M_DELETE, 0);
	  menu->redraw = REDRAW_INDEX;
	}
	else
	{
	  mutt_set_flag (Context, CURHDR, M_DELETE, 0);
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
	CHECK_READONLY;

	rc = mutt_thread_set_flag (CURHDR, M_DELETE, 0,
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

      case OP_VIEW_ATTACHMENTS:
	CHECK_MSGCOUNT;
	mutt_view_attachments (CURHDR);
	if (CURHDR->attach_del)
	  Context->changed = 1;
	menu->redraw = REDRAW_FULL;
	break;

      default:
	if (menu->menu == MENU_MAIN)
	  km_error_key (MENU_MAIN);
    }

    if (menu->menu == MENU_PAGER)
    {
      menu->menu = MENU_MAIN;
      menu->redraw = REDRAW_FULL;
      set_option (OPTWEED); /* turn header weeding back on. */
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
   if (mutt_pattern_exec (color->color_pattern, M_MATCH_FULL_ADDRESS, ctx, curhdr))
   {
      curhdr->pair = color->pair;
      return;
   }
  curhdr->pair = ColorDefs[MT_COLOR_NORMAL];
}

void mutt_cache_index_colors (CONTEXT *ctx)
{
 int i;

 if (ctx)
   for (i = 0; i < ctx->msgcount; i++)
     mutt_set_header_color (ctx, ctx->hdrs[i]);
}
