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
#include "rfc1524.h"
#include "mime.h"
#include "attach.h"
#include "mapping.h"
#include "mailbox.h"
#include "sort.h"
#include "charset.h"

#ifdef MIXMASTER
#include "remailer.h"
#endif

#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

#define CHECK_COUNT if (idxlen == 0) { mutt_error _("There are no attachments."); break; }



enum
{
  HDR_FROM  = 1,
  HDR_TO,
  HDR_CC,
  HDR_BCC,
  HDR_SUBJECT,
  HDR_REPLYTO,
  HDR_FCC,

#ifdef MIXMASTER
  HDR_MIX,
#endif

#ifdef _PGPPATH
  HDR_PGP,
  HDR_PGPSIGINFO,
#endif
  

  HDR_ATTACH  = (HDR_FCC + 5) /* where to start printing the attachments */
};

#define HDR_XOFFSET 10
#define TITLE_FMT "%10s"
#define W (COLS - HDR_XOFFSET)

static char *Prompts[] =
{
  "From: ",
  "To: ",
  "Cc: ",
  "Bcc: ",
  "Subject: ",
  "Reply-To: ",
  "Fcc: "
};

static struct mapping_t ComposeHelp[] = {
  { N_("Send"),    OP_COMPOSE_SEND_MESSAGE },
  { N_("Abort"),   OP_EXIT },
  { "To",      OP_COMPOSE_EDIT_TO },
  { "CC",      OP_COMPOSE_EDIT_CC },
  { "Subj",    OP_COMPOSE_EDIT_SUBJECT },
  { N_("Attach file"),  OP_COMPOSE_ATTACH_FILE },
  { N_("Descrip"), OP_COMPOSE_EDIT_DESCRIPTION },
  { N_("Help"),    OP_HELP },
  { NULL }
};

static void snd_entry (char *b, size_t blen, MUTTMENU *menu, int num)
{
    mutt_FormatString (b, blen, NONULL (AttachFormat), mutt_attach_fmt,
	    (unsigned long)(((ATTACHPTR **) menu->data)[num]),
	    M_FORMAT_STAT_FILE | M_FORMAT_ARROWCURSOR);
}



#ifdef _PGPPATH
#include "pgp.h"

static void redraw_pgp_lines (int pgp)
{
  mvaddstr (HDR_PGP, 0,     "     PGP: ");
  if ((pgp & (PGPENCRYPT | PGPSIGN)) == (PGPENCRYPT | PGPSIGN))
    addstr (_("Sign, Encrypt"));
  else if (pgp & PGPENCRYPT)
    addstr (_("Encrypt"));
  else if (pgp & PGPSIGN)
    addstr (_("Sign"));
  else
    addstr (_("Clear"));
  clrtoeol ();

  move (HDR_PGPSIGINFO, 0);
  clrtoeol ();
  if (pgp & PGPSIGN)
  {
    printw ("%s%s", _(" sign as: "), PgpSignAs ? PgpSignAs : _("<default>"));
    mvprintw (HDR_PGPSIGINFO, 40, "%s%s", _("MIC algorithm: "),
	      NONULL(PgpSignMicalg));
  }
}

static int pgp_send_menu (int bits, int *redraw)
{
  pgp_key_t *p;
  char input_signas[SHORT_STRING];
  char input_micalg[SHORT_STRING];

  struct pgp_vinfo *pgp = pgp_get_vinfo(PGP_SIGN);

  switch (mutt_multi_choice (_("(e)ncrypt, (s)ign, sign (a)s, (b)oth, select (m)ic algorithm, or (f)orget it? "),
			     _("esabmf")))
  {
  case 1: /* (e)ncrypt */
    bits |= PGPENCRYPT;
    break;

  case 2: /* (s)ign */
    bits |= PGPSIGN;
    break;

  case 3: /* sign (a)s */

    unset_option(OPTPGPCHECKTRUST);

    if (pgp && (p = pgp_ask_for_key (pgp, _("Sign as: "), NULL, KEYFLAG_CANSIGN, PGP_PUBRING)))
    {
      snprintf (input_signas, sizeof (input_signas), "0x%s", pgp_keyid (p));
      safe_free((void **) &PgpSignAs); 	    PgpSignAs = safe_strdup (input_signas);
      safe_free((void **) &PgpSignMicalg);  PgpSignMicalg = safe_strdup (pgp_pkalg_to_mic (p->algorithm));
      pgp_free_key (&p);
      
      bits |= PGPSIGN;
	
      pgp_void_passphrase ();	/* probably need a different passphrase */
    }
    else
    {
      bits &= ~PGPSIGN;
      mutt_error _("An unkown PGP version was defined for signing.");
    }

    *redraw = REDRAW_FULL;
    break;

  case 4: /* (b)oth */
    bits = PGPENCRYPT | PGPSIGN;
    break;

  case 5: /* select (m)ic algorithm */
    if (!(bits & PGPSIGN))
      mutt_error _("This doesn't make sense if you don't want to sign the message.");
    else
    {
      /* Copy the existing MIC algorithm into place */
      strfcpy(input_micalg, NONULL (PgpSignMicalg), sizeof (input_micalg));

      if (mutt_get_field (_("MIC algorithm: "), input_micalg, sizeof (input_micalg), 0) == 0)
      {
	if (mutt_strcasecmp (input_micalg, "pgp-md5") && mutt_strcasecmp (input_micalg, "pgp-sha1")
	   && mutt_strcasecmp (input_micalg, "pgp-rmd160"))
	{
	  mutt_error _("Unknown MIC algorithm, valid ones are: pgp-md5, pgp-sha1, pgp-rmd160");
	}
	else 
	{
	  safe_free ((void **) &PgpSignMicalg);
	  PgpSignMicalg = safe_strdup (input_micalg);
	}
      }
    }
    break;

  case 6: /* (f)orget it */
    bits = 0;
    break;
  }
  if (!*redraw)
    redraw_pgp_lines (bits);
  return (bits);
}
#endif /* _PGPPATH */

#ifdef MIXMASTER

static void redraw_mix_line (LIST *chain)
{
  int c;
  char *t;

  mvaddstr (HDR_MIX, 0,     "     Mix: ");

  if (!chain)
  {
    addstr ("<no chain defined>");
    clrtoeol ();
    return;
  }
  
  for (c = 12; chain; chain = chain->next)
  {
    t = chain->data;
    if (t && t[0] == '0' && t[1] == '\0')
      t = "<random>";
    
    if (c + mutt_strlen (t) + 2 >= COLS)
      break;

    addstr (NONULL(t));
    if (chain->next)
      addstr (", ");

    c += mutt_strlen (t) + 2;
  }
}
#endif

static int
check_attachments(ATTACHPTR **idx, short idxlen)
{
  int i, r;
  struct stat st;
  char pretty[_POSIX_PATH_MAX], msg[_POSIX_PATH_MAX + SHORT_STRING];

  for (i = 0; i < idxlen; i++)
  {
    strfcpy(pretty, idx[i]->content->filename, sizeof(pretty));
    if(stat(idx[i]->content->filename, &st) != 0)
    {
      mutt_pretty_mailbox(pretty);
      mutt_error(_("%s [#%d] no longer exists!"),
		 pretty, i+1);
      return -1;
    }
    
    if(idx[i]->content->stamp < st.st_mtime)
    {
      mutt_pretty_mailbox(pretty);
      snprintf(msg, sizeof(msg), _("%s [#%d] modified. Update encoding?"),
	       pretty, i+1);
      
      if((r = mutt_yesorno(msg, M_YES)) == M_YES)
	mutt_update_encoding(idx[i]->content);
      else if(r == -1)
	return -1;
    }
  }

  return 0;
}

static void draw_envelope_addr (int line, ADDRESS *addr)
{
  char buf[STRING];

  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), addr);
  mvprintw (line, 0, TITLE_FMT "%-*.*s", Prompts[line - 1], W, W, buf);
}

static void draw_envelope (HEADER *msg, char *fcc)
{
  draw_envelope_addr (HDR_FROM, msg->env->from);
  draw_envelope_addr (HDR_TO, msg->env->to);
  draw_envelope_addr (HDR_CC, msg->env->cc);
  draw_envelope_addr (HDR_BCC, msg->env->bcc);
  mvprintw (HDR_SUBJECT, 0, TITLE_FMT "%-*.*s", Prompts[HDR_SUBJECT - 1], W, W,
	    NONULL(msg->env->subject));
  draw_envelope_addr (HDR_REPLYTO, msg->env->reply_to);
  mvprintw (HDR_FCC, 0, TITLE_FMT "%-*.*s", Prompts[HDR_FCC - 1], W, W, fcc);



#ifdef _PGPPATH
  redraw_pgp_lines (msg->pgp);
#endif /* _PGPPATH */

#ifdef MIXMASTER
  redraw_mix_line (msg->chain);
#endif

  SETCOLOR (MT_COLOR_STATUS);
  mvaddstr (HDR_ATTACH - 1, 0, _("-- Attachments"));
  BKGDSET (MT_COLOR_STATUS);
  clrtoeol ();

  BKGDSET (MT_COLOR_NORMAL);
  SETCOLOR (MT_COLOR_NORMAL);
}

static int edit_address_list (int line, ADDRESS **addr)
{
  char buf[HUGE_STRING] = ""; /* needs to be large for alias expansion */

  rfc822_write_address (buf, sizeof (buf), *addr);
  if (mutt_get_field (Prompts[line - 1], buf, sizeof (buf), M_ALIAS) == 0)
  {
    rfc822_free_address (addr);
    *addr = mutt_parse_adrlist (*addr, buf);
    *addr = mutt_expand_aliases (*addr);
  }

  if (option (OPTNEEDREDRAW))
  {
    unset_option (OPTNEEDREDRAW);
    return (REDRAW_FULL);
  }

  /* redraw the expanded list so the user can see the result */
  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), *addr);
  mvprintw (line, HDR_XOFFSET, "%-*.*s", W, W, buf);

  return 0;
}

static int delete_attachment (MUTTMENU *menu, short *idxlen, int x)
{
  ATTACHPTR **idx = (ATTACHPTR **) menu->data;
  int y;

  menu->redraw = REDRAW_INDEX | REDRAW_STATUS;

  if (x == 0 && menu->max == 1)
  {
    mutt_error _("You may not delete the only attachment.");
    idx[x]->content->tagged = 0;
    return (-1);
  }

  for (y = 0; y < *idxlen; y++)
  {
    if (idx[y]->content->next == idx[x]->content)
    {
      idx[y]->content->next = idx[x]->content->next;
      break;
    }
  }

  idx[x]->content->next = NULL;
  idx[x]->content->parts = NULL;
  mutt_free_body (&(idx[x]->content));
  safe_free ((void **) &idx[x]->tree);
  safe_free ((void **) &idx[x]);
  for (; x < *idxlen - 1; x++)
    idx[x] = idx[x+1];
  menu->max = --(*idxlen);
  
  return (0);
}

static void update_idx (MUTTMENU *menu, ATTACHPTR **idx, short idxlen)
{
  idx[idxlen]->level = (idxlen > 0) ? idx[idxlen-1]->level : 0;
  if (idxlen)
    idx[idxlen - 1]->content->next = idx[idxlen]->content;
  menu->current = idxlen++;
  mutt_update_tree (idx, idxlen);
  menu->max = idxlen;
  return;
}

static int change_attachment_charset (BODY *b)
{
  char buff[SHORT_STRING];

  if (!mutt_is_text_type (b->type, b->subtype))
  {
    mutt_error _("Can't change character set for non-text attachments!");
    return 0;
  }

  mutt_get_send_charset (buff, sizeof(buff), b, 0);
  
  if (mutt_get_field (_("Enter character set: "), buff, sizeof(buff), 0) == -1)
    return 0;
    
  if (mutt_is_utf8(buff))
  {
    if (!b->noconv)
    {
      mutt_error (_("UTF-8 encoding attachments has not yet been implemented."));
      return 0;
    }
  }
  else if (mutt_get_charset (buff) == NULL)
  {
    mutt_error (_("Character set %s is unknown."), buff);
    return 0;
  }
  
  mutt_set_body_charset (b, buff);
  return REDRAW_CURRENT;
}

/* return values:
 *
 * 1	message should be postponed
 * 0	normal exit
 * -1	abort message
 */
int mutt_compose_menu (HEADER *msg,   /* structure for new message */
		    char *fcc,     /* where to save a copy of the message */
		    size_t fcclen,
		    HEADER *cur)   /* current message */
{
  char helpstr[SHORT_STRING];
  char buf[LONG_STRING];
  char fname[_POSIX_PATH_MAX];
  MUTTMENU *menu;
  ATTACHPTR **idx = NULL;
  short idxlen = 0;
  short idxmax = 0;
  int i, close = 0;
  int r = -1;		/* return value */
  int op = 0;
  int loop = 1;
  int fccSet = 0;	/* has the user edited the Fcc: field ? */
  CONTEXT *ctx = NULL, *this = NULL;
  /* Sort, SortAux could be changed in mutt_index_menu() */
  int oldSort = Sort, oldSortAux = SortAux;
  struct stat st;
  char **files;
  int numfiles;

  idx = mutt_gen_attach_list (msg->content, -1, idx, &idxlen, &idxmax, 0, 1);

  menu = mutt_new_menu ();
  menu->menu = MENU_COMPOSE;
  menu->offset = HDR_ATTACH;
  menu->max = idxlen;
  menu->make_entry = snd_entry;
  menu->tag = mutt_tag_attach;
  menu->title = _("Compose");
  menu->data = idx;
  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_COMPOSE, ComposeHelp);
  
  while (loop)
  {
    switch (op = mutt_menuLoop (menu))
    {
      case OP_REDRAW:
	menu_redraw_status (menu);
	draw_envelope (msg, fcc);
	menu->offset = HDR_ATTACH;
	menu->pagelen = LINES - HDR_ATTACH - 2;
	break;
      case OP_COMPOSE_EDIT_FROM:
	menu->redraw = edit_address_list (HDR_FROM, &msg->env->from);
	break;
      case OP_COMPOSE_EDIT_TO:
	menu->redraw = edit_address_list (HDR_TO, &msg->env->to);
	break;
      case OP_COMPOSE_EDIT_BCC:
	menu->redraw = edit_address_list (HDR_BCC, &msg->env->bcc);
	break;
      case OP_COMPOSE_EDIT_CC:
	menu->redraw = edit_address_list (HDR_CC, &msg->env->cc);
	break;
      case OP_COMPOSE_EDIT_SUBJECT:
	if (msg->env->subject)
	  strfcpy (buf, msg->env->subject, sizeof (buf));
	else
	  buf[0] = 0;
	if (mutt_get_field ("Subject: ", buf, sizeof (buf), 0) == 0)
	{
	  safe_free ((void **) &msg->env->subject);
	  msg->env->subject = safe_strdup (buf);
	  move (HDR_SUBJECT, HDR_XOFFSET);
	  clrtoeol ();
	  if (msg->env->subject)
	    printw ("%-*.*s", W, W, msg->env->subject);
	}
	break;
      case OP_COMPOSE_EDIT_REPLY_TO:
	menu->redraw = edit_address_list (HDR_REPLYTO, &msg->env->reply_to);
	break;
      case OP_COMPOSE_EDIT_FCC:
	strfcpy (buf, fcc, sizeof (buf));
	if (mutt_get_field ("Fcc: ", buf, sizeof (buf), M_FILE | M_CLEAR) == 0)
	{
	  strfcpy (fcc, buf, _POSIX_PATH_MAX);
	  mutt_pretty_mailbox (fcc);
	  mvprintw (HDR_FCC, HDR_XOFFSET, "%-*.*s", W, W, fcc);
	  fccSet = 1;
	}
	MAYBE_REDRAW (menu->redraw);
	break;
      case OP_COMPOSE_EDIT_MESSAGE:
	if (Editor && (mutt_strcmp ("builtin", Editor) != 0) && !option (OPTEDITHDRS))
	{
	  mutt_edit_file (Editor, msg->content->filename);
	  mutt_update_encoding (msg->content);
	  menu->redraw = REDRAW_CURRENT;
	  break;
	}
	/* fall through */
      case OP_COMPOSE_EDIT_HEADERS:
	if (op == OP_COMPOSE_EDIT_HEADERS ||
	    (op == OP_COMPOSE_EDIT_MESSAGE && option (OPTEDITHDRS)))
	{
	  mutt_edit_headers ((!Editor || mutt_strcmp ("builtin", Editor) == 0) ? NONULL(Visual) : NONULL(Editor),
			     msg->content->filename, msg, fcc, fcclen);
	}
	else
	{
	  /* this is grouped with OP_COMPOSE_EDIT_HEADERS because the
	     attachment list could change if the user invokes ~v to edit
	     the message with headers, in which we need to execute the
	     code below to regenerate the index array */
	  mutt_builtin_editor (msg->content->filename, msg, cur);
	}
	mutt_update_encoding (msg->content);

	/* attachments may have been added */
	if (idxlen && idx[idxlen - 1]->content->next)
	{
	  for (i = 0; i < idxlen; i++)
	    safe_free ((void **) &idx[i]);
	  idxlen = 0;
	  idx = mutt_gen_attach_list (msg->content, -1, idx, &idxlen, &idxmax, 0, 1);
	  menu->data = idx;
	  menu->max = idxlen;
	}

	menu->redraw = REDRAW_FULL;
	break;



#ifdef _PGPPATH
      case OP_COMPOSE_ATTACH_KEY:

	if (idxlen == idxmax)
        {
	  safe_realloc ((void **) &idx, sizeof (ATTACHPTR *) * (idxmax += 5));
	  menu->data = idx;
	}
	
	idx[idxlen] = (ATTACHPTR *) safe_calloc (1, sizeof (ATTACHPTR));
	if ((idx[idxlen]->content = pgp_make_key_attachment(NULL)) != NULL)
	{
	  idx[idxlen]->level = (idxlen > 0) ? idx[idxlen-1]->level : 0;

	  if(idxlen)
	    idx[idxlen - 1]->content->next = idx[idxlen]->content;
	  
	  menu->current = idxlen++;
	  mutt_update_tree (idx, idxlen);
	  menu->max = idxlen;
	  menu->redraw |= REDRAW_INDEX;
	}
	else
	  safe_free ((void **) &idx[idxlen]);

	menu->redraw |= REDRAW_STATUS;

	if(option(OPTNEEDREDRAW))
	{
	  menu->redraw = REDRAW_FULL;
	  unset_option(OPTNEEDREDRAW);
	}
	
	break;
#endif



      case OP_COMPOSE_ATTACH_FILE:
      case OP_COMPOSE_ATTACH_MESSAGE:

	fname[0] = 0;
	{
	  char* prompt;
	  int flag;

	  if (op == OP_COMPOSE_ATTACH_FILE)
	  {
	    prompt = _("Attach file");
	    flag = 0;
	    numfiles = 0;
	    files = NULL;
	    if (_mutt_enter_fname (prompt, fname, sizeof (fname), &menu->redraw, flag, 1, &files, &numfiles) == -1)
	      break;
	  }
	  else
	  {
	    prompt = _("Open mailbox to attach message from");
	    if (Context)
	    {
	      strfcpy (fname, NONULL (Context->path), sizeof (fname));
	      mutt_pretty_mailbox (fname);
	    }
	    flag = 1;
	    if (mutt_enter_fname (prompt, fname, sizeof (fname), &menu->redraw, flag) == -1 || !fname[0])
	      break;
	    mutt_expand_path (fname, sizeof (fname));
	    /* check to make sure the file exists and is readable */
	    if (access (fname, R_OK) == -1)
	    {
	      mutt_perror (fname);
	      break;
	    }
	  }
	}

	if (op == OP_COMPOSE_ATTACH_MESSAGE)
	{
	  menu->redraw = REDRAW_FULL;

	  ctx = mx_open_mailbox (fname, M_READONLY, NULL);
	  if (ctx == NULL)
	  {
	    mutt_perror (fname);
	    break;
	  }

	  if (!ctx->msgcount)
	  {
	    mx_close_mailbox (ctx);
	    safe_free ((void **) &ctx);
	    mutt_error _("No messages in that folder.");
	    break;
	  }
	  
	  this = Context; /* remember current folder */
	  Context = ctx;
	  set_option(OPTATTACHMSG);
	  mutt_message _("Tag the messages you want to attach!");
	  close = mutt_index_menu ();
	  unset_option(OPTATTACHMSG);

	  if (!Context)
	  {
	    /* go back to the folder we started from */
	    Context = this;
	    /* Restore old $sort and $sort_aux */
	    Sort = oldSort;
	    SortAux = oldSortAux;
	    menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
	    break;
	  }
	}

        {
	  int numtag = 0;

	  if (op == OP_COMPOSE_ATTACH_MESSAGE)
	    numtag = Context->tagged;
	  else
	    numtag = numfiles;
	  if (idxlen + numtag >= idxmax)
	  {
	    safe_realloc ((void **) &idx, sizeof (ATTACHPTR *) * (idxmax += 5 + numtag));
	    menu->data = idx;
	  }
	}

	if (op == OP_COMPOSE_ATTACH_FILE)
	{
	  int error = 0;
	  mutt_message _("Attaching selected files...");
	  for (i = 0; i < numfiles; i++)
	  {
	    char *att = files[i];
	    idx[idxlen] = (ATTACHPTR *) safe_calloc (1, sizeof (ATTACHPTR));
	    idx[idxlen]->content = mutt_make_file_attach (att);
	    if (idx[idxlen]->content != NULL)
	      update_idx (menu, idx, idxlen++);
	    else
	    {
	      error = 1;
	      mutt_error (_("Unable to attach %s!"), att);
	      safe_free ((void **) &idx[idxlen]);
	    }
	    FREE (&this);
	  }
	  FREE (&files);
	  if (!error)
	    mutt_clear_error ();
	  menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
        }
	else
	{
	  HEADER *h;
	  for (i = 0; i < Context->msgcount; i++)
	  {
	    h = Context->hdrs[i];
	    if (h->tagged)
	    {
	      idx[idxlen] = (ATTACHPTR *) safe_calloc (1, sizeof (ATTACHPTR));
	      idx[idxlen]->content = mutt_make_message_attach (Context, h, 1);
	      if (idx[idxlen]->content != NULL)
		update_idx (menu, idx, idxlen++);
	      else
	      {
		mutt_error _("Unable to attach!");
		safe_free ((void **) &idx[idxlen]);
	      }
	    }
	  }
	  menu->redraw |= REDRAW_FULL;

	  if (close == OP_QUIT) 
	    mx_close_mailbox (Context);
	  else
	    mx_fastclose_mailbox (Context);
	  safe_free ((void **) &Context);

	  /* go back to the folder we started from */
	  Context = this;
	  /* Restore old $sort and $sort_aux */
	  Sort = oldSort;
	  SortAux = oldSortAux;
	}

	break;

      case OP_DELETE:
	CHECK_COUNT;
	if (delete_attachment (menu, &idxlen, menu->current) == -1)
	  break;
	mutt_update_tree (idx, idxlen);
	if (idxlen)
	{
	  if (menu->current > idxlen - 1)
	    menu->current = idxlen - 1;
	}
	else
	  menu->current = 0;

	if (menu->current == 0)
	  msg->content = idx[0]->content;
	break;

      case OP_COMPOSE_CHANGE_CHARSET:
        CHECK_COUNT;
        menu->redraw = change_attachment_charset(idx[menu->current]->content);
        break;

#define CURRENT idx[menu->current]->content
      
      case OP_COMPOSE_TOGGLE_RECODE:
      {      
        CHECK_COUNT;
        if (!mutt_is_text_type (CURRENT->type, CURRENT->subtype))
        {
	  mutt_error (_("Recoding only affects text attachments."));
	  break;
	}
	if (mutt_is_utf8 (mutt_get_parameter ("charset", CURRENT->parameter)))
	{
	  mutt_error (_("We currently can't encode to utf-8."));
	  break;
	}
        CURRENT->noconv = !CURRENT->noconv;
        if (CURRENT->noconv)
	  mutt_message (_("The current attachment won't be converted."));
        else
	  mutt_message (_("The current attachment will be converted."));
	menu->redraw = REDRAW_CURRENT;
        break;
      }

      case OP_COMPOSE_RECODE:
      {
	const char *chs;
	int rv;

        CHECK_COUNT;
        if (!mutt_is_text_type (CURRENT->type, CURRENT->subtype))
        {
	  mutt_error (_("Recoding only affetcs text attachments."));
	  break;
	}

	if (!(chs = mutt_get_parameter ("charset", CURRENT->parameter)))
	  chs = SendCharset;

        if (CURRENT->noconv)
	  rv = mutt_recode_file (CURRENT->filename, chs, Charset);
	else
	  rv = mutt_recode_file (CURRENT->filename, Charset, chs);

	mutt_update_encoding (CURRENT);
	
	if (rv == 0)
	{
	  mutt_message (_("Recoding successful."));
	  CURRENT->noconv = !CURRENT->noconv;
	}

	menu->redraw = REDRAW_CURRENT;
	break;
      }
#undef CURRENT

      case OP_COMPOSE_EDIT_DESCRIPTION:
	CHECK_COUNT;
	strfcpy (buf,
		 idx[menu->current]->content->description ?
		 idx[menu->current]->content->description : "",
		 sizeof (buf));
	/* header names should not be translated */
	if (mutt_get_field ("Description: ", buf, sizeof (buf), 0) == 0)
	{
	  safe_free ((void **) &idx[menu->current]->content->description);
	  idx[menu->current]->content->description = safe_strdup (buf);
	  menu->redraw = REDRAW_CURRENT;
	}
	break;

      case OP_COMPOSE_UPDATE_ENCODING:
        CHECK_COUNT;
        if(menu->tagprefix)
        {
	  BODY *top;
	  for(top = msg->content; top; top = top->next)
	  {
	    if(top->tagged)
	      mutt_update_encoding(top);
	  }
	  menu->redraw = REDRAW_FULL;
	}
        else
        {
          mutt_update_encoding(idx[menu->current]->content);
	  menu->redraw = REDRAW_CURRENT;
	}
        break;
      
      case OP_COMPOSE_EDIT_TYPE:
	CHECK_COUNT;
	snprintf (buf, sizeof (buf), "%s/%s",
		  TYPE (idx[menu->current]->content),
		  idx[menu->current]->content->subtype);
	if (mutt_get_field ("Content-Type: ", buf, sizeof (buf), 0) == 0 && buf[0])
	{
	  char *s;
	  PARAMETER *par;
	  BODY *b;
	  
	  b = idx[menu->current]->content;
	  
	  s  = b->filename;  par = b->parameter;
	  b->filename  = NULL;  b->parameter = NULL;
	  
	  mutt_parse_content_type (buf, b);

	  safe_free ((void **) &b->filename);
	  b->filename = s;
	  
	  if ((s = mutt_get_parameter("charset", b->parameter)))
	    mutt_set_parameter("charset", s, &par);
	  
	  /* These are needed for "traditional" PGP.
	   * Should we switch to a "negative list" instead?
	   */

	  if ((s = mutt_get_parameter("x-action", b->parameter)))
	    mutt_set_parameter("x-action", s, &par);
	  if ((s = mutt_get_parameter("format", b->parameter)))
	    mutt_set_parameter("format", s, &par);

	  /* ignore the other parameters for now */
	  mutt_free_parameter(&b->parameter);
	  b->parameter = par;

	  /* this may have been a "structured" message */
	  if  (b->parts)
	    mutt_free_body (&b->parts);
	  if (b->hdr)
	  {
	    b->hdr->content = NULL;
	    mutt_free_header (&b->hdr);
	  }
	  
	  menu->redraw = REDRAW_CURRENT;
	}
	break;

      case OP_COMPOSE_EDIT_ENCODING:
	CHECK_COUNT;
	strfcpy (buf, ENCODING (idx[menu->current]->content->encoding),
							      sizeof (buf));
	if (mutt_get_field ("Content-Transfer-Encoding: ", buf,
					    sizeof (buf), 0) == 0 && buf[0])
	{
	  if ((i = mutt_check_encoding (buf)) != ENCOTHER && i != ENCUUENCODED)
	  {
	    idx[menu->current]->content->encoding = i;
	    menu->redraw = REDRAW_CURRENT;
	    mutt_clear_error();
	  }
	  else
	    mutt_error _("Invalid encoding.");
	}
	break;

      case OP_COMPOSE_SEND_MESSAGE:
      
        if(check_attachments(idx, idxlen) != 0)
        {
	  menu->redraw = REDRAW_FULL;
	  break;
	}

      
#ifdef MIXMASTER
        if (msg->chain && mix_check_message (msg) != 0)
	  break;
#endif
      
	if (!fccSet && *fcc)
	{
	  if ((i = query_quadoption (OPT_COPY,
				_("Save a copy of this message?"))) == -1)
	    break;
	  else if (i == M_NO)
	    *fcc = 0;
	}

	loop = 0;
	r = 0;
	break;

      case OP_COMPOSE_EDIT_FILE:
	CHECK_COUNT;
	mutt_edit_file ((!Editor || mutt_strcmp ("builtin", Editor) == 0) ? NONULL(Visual) : NONULL(Editor),
			idx[menu->current]->content->filename);
	mutt_update_encoding (idx[menu->current]->content);
	menu->redraw = REDRAW_CURRENT;
	break;

      case OP_COMPOSE_TOGGLE_UNLINK:
	CHECK_COUNT;
	idx[menu->current]->content->unlink = !idx[menu->current]->content->unlink;

#if 0
        /* OPTRESOLVE is otherwise ignored on this menu.
	 * Where's the bug?
	 */

        if (option (OPTRESOLVE) && menu->current + 1 < menu->max)
	  menu->current++;
# endif
	menu->redraw = REDRAW_INDEX;
	break;

      case OP_COMPOSE_GET_ATTACHMENT:
        CHECK_COUNT;
        if(menu->tagprefix)
        {
	  BODY *top;
	  for(top = msg->content; top; top = top->next)
	  {
	    if(top->tagged)
	      mutt_get_tmp_attachment(top);
	  }
	  menu->redraw = REDRAW_FULL;
	}
        else if (mutt_get_tmp_attachment(idx[menu->current]->content) == 0)
	  menu->redraw = REDRAW_CURRENT;

        break;
      
      case OP_COMPOSE_RENAME_FILE:
	CHECK_COUNT;
	strfcpy (fname, idx[menu->current]->content->filename, sizeof (fname));
	mutt_pretty_mailbox (fname);
	if (mutt_get_field (_("Rename to: "), fname, sizeof (fname), M_FILE)
							== 0 && fname[0])
	{
	  if(stat(idx[menu->current]->content->filename, &st) == -1)
	  {
	    mutt_error (_("Can't stat: %s"), fname);
	    break;
	  }

	  mutt_expand_path (fname, sizeof (fname));
	  if(mutt_rename_file (idx[menu->current]->content->filename, fname))
	    break;
	  
	  safe_free ((void **) &idx[menu->current]->content->filename);
	  idx[menu->current]->content->filename = safe_strdup (fname);
	  menu->redraw = REDRAW_CURRENT;

	  if(idx[menu->current]->content->stamp >= st.st_mtime)
	    mutt_stamp_attachment(idx[menu->current]->content);
	  
	}
	break;

      case OP_COMPOSE_NEW_MIME:
	{
	  char type[STRING];
	  char *p;
	  int itype;
	  FILE *fp;

	  CLEARLINE (LINES-1);
	  fname[0] = 0;
	  if (mutt_get_field (_("New file: "), fname, sizeof (fname), M_FILE)
	      != 0 || !fname[0])
	    continue;
	  mutt_expand_path (fname, sizeof (fname));

	  /* Call to lookup_mime_type () ?  maybe later */
	  type[0] = 0;
	  if (mutt_get_field ("Content-Type: ", type, sizeof (type), 0) != 0 
	      || !type[0])
	    continue;

	  if (!(p = strchr (type, '/')))
	  {
	    mutt_error _("Content-Type is of the form base/sub");
	    continue;
	  }
	  *p++ = 0;
	  if ((itype = mutt_check_mime_type (type)) == TYPEOTHER)
	  {
	    mutt_error (_("Unknown Content-Type %s"), type);
	    continue;
	  }
	  if (idxlen == idxmax)
	  {
	    safe_realloc ((void **) &idx, sizeof (ATTACHPTR *) * (idxmax += 5));
	    menu->data = idx;
	  }

	  idx[idxlen] = (ATTACHPTR *) safe_calloc (1, sizeof (ATTACHPTR));
	  /* Touch the file */
	  if (!(fp = safe_fopen (fname, "w")))
	  {
	    mutt_error (_("Can't create file %s"), fname);
	    safe_free ((void **) &idx[idxlen]);
	    continue;
	  }
	  fclose (fp);

	  if ((idx[idxlen]->content = mutt_make_file_attach (fname)) == NULL)
	  {
	    mutt_error _("What we have here is a failure to make an attachment");
	    continue;
	  }
	  
	  idx[idxlen]->level = (idxlen > 0) ? idx[idxlen-1]->level : 0;
	  if (idxlen)
	    idx[idxlen - 1]->content->next = idx[idxlen]->content;
	  
	  menu->current = idxlen++;
	  mutt_update_tree (idx, idxlen);
	  menu->max = idxlen;

	  idx[menu->current]->content->type = itype;
	  safe_free ((void **) &idx[menu->current]->content->subtype);
	  idx[menu->current]->content->subtype = safe_strdup (p);
	  idx[menu->current]->content->unlink = 1;
	  menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;

	  if (mutt_compose_attachment (idx[menu->current]->content))
	  {
	    mutt_update_encoding (idx[menu->current]->content);
	    menu->redraw = REDRAW_FULL;
	  }
	}
	break;

      case OP_COMPOSE_EDIT_MIME:
	CHECK_COUNT;
	if (mutt_edit_attachment (idx[menu->current]->content))
	{
	  mutt_update_encoding (idx[menu->current]->content);
	  menu->redraw = REDRAW_FULL;
	}
	break;

      case OP_VIEW_ATTACH:
      case OP_DISPLAY_HEADERS:
	CHECK_COUNT;
	mutt_attach_display_loop (menu, op, NULL, idx);
	menu->redraw = REDRAW_FULL;
	mutt_clear_error ();
	break;

      case OP_SAVE:
	CHECK_COUNT;
	mutt_save_attachment_list (NULL, menu->tagprefix, menu->tagprefix ?  msg->content : idx[menu->current]->content, NULL);
	MAYBE_REDRAW (menu->redraw);
	break;

      case OP_PRINT:
	CHECK_COUNT;
	mutt_print_attachment_list (NULL, menu->tagprefix, menu->tagprefix ? msg->content : idx[menu->current]->content);
	break;

      case OP_PIPE:
      case OP_FILTER:
        CHECK_COUNT;
	mutt_pipe_attachment_list (NULL, menu->tagprefix, menu->tagprefix ? msg->content : idx[menu->current]->content, op == OP_FILTER);
	if (op == OP_FILTER) /* cte might have changed */
	  menu->redraw = menu->tagprefix ? REDRAW_FULL : REDRAW_CURRENT; 
	break;

      case OP_EXIT:
	if ((i = query_quadoption (OPT_POSTPONE, _("Postpone this message?"))) == M_NO)
	{
	  while (idxlen-- > 0)
	  {
	    /* avoid freeing other attachments */
	    idx[idxlen]->content->next = NULL;
	    idx[idxlen]->content->parts = NULL;
	    mutt_free_body (&idx[idxlen]->content);
	    safe_free ((void **) &idx[idxlen]->tree);
	    safe_free ((void **) &idx[idxlen]);
	  }
	  safe_free ((void **) &idx);
	  idxlen = 0;
	  idxmax = 0;
	  r = -1;
	  loop = 0;
	  break;
	}
	else if (i == -1)
	  break; /* abort */

	/* fall through to postpone! */

      case OP_COMPOSE_POSTPONE_MESSAGE:

        if(check_attachments(idx, idxlen) != 0)
        {
	  menu->redraw = REDRAW_FULL;
	  break;
	}
      
	loop = 0;
	r = 1;
	break;

      case OP_COMPOSE_ISPELL:
	endwin ();
	snprintf (buf, sizeof (buf), "%s -x %s", NONULL(Ispell), msg->content->filename);
	if (mutt_system (buf) == -1)
	  mutt_error (_("Error running \"%s\"!"), buf);
	else
	  mutt_update_encoding (msg->content);
	break;

      case OP_COMPOSE_WRITE_MESSAGE:

       fname[0] = '\0';
       if (Context)
       {
	 strfcpy (fname, NONULL (Context->path), sizeof (fname));
	 mutt_pretty_mailbox (fname);
       }
       if (idxlen)
         msg->content = idx[0]->content;
       if (mutt_enter_fname (_("Write message to mailbox"), fname, sizeof (fname),
                             &menu->redraw, 1) != -1 && fname[0])
       {
	 int oldhdrdate;
         mutt_message (_("Writing message to %s ..."), fname);
         mutt_expand_path (fname, sizeof (fname));

         if (msg->content->next)
           msg->content = mutt_make_multipart (msg->content);

	 oldhdrdate = option(OPTUSEHEADERDATE);
	 set_option(OPTUSEHEADERDATE);
         if (mutt_write_fcc (NONULL (fname), msg, NULL, 1, NULL) < 0)
           msg->content = mutt_remove_multipart (msg->content);
         else
           mutt_message _("Message written.");
	 if(!oldhdrdate) unset_option(OPTUSEHEADERDATE);
       }
       break;



#ifdef _PGPPATH
      case OP_COMPOSE_PGP_MENU:

	msg->pgp = pgp_send_menu (msg->pgp, &menu->redraw);
	break;

      case OP_FORGET_PASSPHRASE:

	mutt_forget_passphrase ();
	break;

#endif /* _PGPPATH */


#ifdef MIXMASTER
      case OP_COMPOSE_MIX:
      
      	mix_make_chain (&msg->chain, &menu->redraw);
        break;
#endif

    }
  }

  mutt_menuDestroy (&menu);

  if (idxlen)
  {
    msg->content = idx[0]->content;
    for (i = 0; i < idxlen; i++)
      safe_free ((void **) &idx[i]);
  }
  else
    msg->content = NULL;

  safe_free ((void **) &idx);

  return (r);
}
