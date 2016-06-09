/*
 * Copyright (C) 1996-2000,2002,2007,2010,2012 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 2004 g10 Code GmbH
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
#include "mutt_idna.h"
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

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>

static const char* There_are_no_attachments = N_("There are no attachments.");

#define CHECK_COUNT if (idxlen == 0) { mutt_error _(There_are_no_attachments); break; }



enum
{
  HDR_FROM  = 0,
  HDR_TO,
  HDR_CC,
  HDR_BCC,
  HDR_SUBJECT,
  HDR_REPLYTO,
  HDR_FCC,

#ifdef MIXMASTER
  HDR_MIX,
#endif

  HDR_CRYPT,
  HDR_CRYPTINFO,

  HDR_ATTACH  = (HDR_FCC + 5) /* where to start printing the attachments */
};

#define HDR_XOFFSET 10
#define TITLE_FMT "%10s" /* Used for Prompts, which are ASCII */
#define W (MuttIndexWindow->cols - HDR_XOFFSET)

static const char * const Prompts[] =
{
  "From: ",
  "To: ",
  "Cc: ",
  "Bcc: ",
  "Subject: ",
  "Reply-To: ",
  "Fcc: "
};

static const struct mapping_t ComposeHelp[] = {
  { N_("Send"),    OP_COMPOSE_SEND_MESSAGE },
  { N_("Abort"),   OP_EXIT },
  { "To",      OP_COMPOSE_EDIT_TO },
  { "CC",      OP_COMPOSE_EDIT_CC },
  { "Subj",    OP_COMPOSE_EDIT_SUBJECT },
  { N_("Attach file"),  OP_COMPOSE_ATTACH_FILE },
  { N_("Descrip"), OP_COMPOSE_EDIT_DESCRIPTION },
  { N_("Help"),    OP_HELP },
  { NULL,	0 }
};

static void snd_entry (char *b, size_t blen, MUTTMENU *menu, int num)
{
  mutt_FormatString (b, blen, 0, MuttIndexWindow->cols, NONULL (AttachFormat), mutt_attach_fmt,
	    (unsigned long)(((ATTACHPTR **) menu->data)[num]),
	    MUTT_FORMAT_STAT_FILE | MUTT_FORMAT_ARROWCURSOR);
}



#include "mutt_crypt.h"

static void redraw_crypt_lines (HEADER *msg)
{
  mutt_window_mvprintw (MuttIndexWindow, HDR_CRYPT, 0, TITLE_FMT, "Security: ");

  if ((WithCrypto & (APPLICATION_PGP | APPLICATION_SMIME)) == 0)
  {
    addstr(_("Not supported"));
    return;
  }

  if ((msg->security & (ENCRYPT | SIGN)) == (ENCRYPT | SIGN))
    addstr (_("Sign, Encrypt"));
  else if (msg->security & ENCRYPT)
    addstr (_("Encrypt"));
  else if (msg->security & SIGN)
    addstr (_("Sign"));
  else
    addstr (_("None"));

  if ((msg->security & (ENCRYPT | SIGN)))
  {
    if ((WithCrypto & APPLICATION_PGP) && (msg->security & APPLICATION_PGP))
    {
      if ((msg->security & INLINE))
        addstr (_(" (inline PGP)"));
      else
        addstr (_(" (PGP/MIME)"));
    }
    else if ((WithCrypto & APPLICATION_SMIME) &&
             (msg->security & APPLICATION_SMIME))
      addstr (_(" (S/MIME)"));
  }

  if (option (OPTCRYPTOPPORTUNISTICENCRYPT) && (msg->security & OPPENCRYPT))
      addstr (_(" (OppEnc mode)"));

  mutt_window_clrtoeol (MuttIndexWindow);
  mutt_window_move (MuttIndexWindow, HDR_CRYPTINFO, 0);
  mutt_window_clrtoeol (MuttIndexWindow);

  if ((WithCrypto & APPLICATION_PGP)
      && (msg->security & APPLICATION_PGP) && (msg->security & SIGN))
    printw (TITLE_FMT "%s", _("sign as: "), PgpSignAs ? PgpSignAs : _("<default>"));

  if ((WithCrypto & APPLICATION_SMIME)
      && (msg->security & APPLICATION_SMIME) && (msg->security & SIGN)) {
      printw (TITLE_FMT "%s", _("sign as: "), SmimeDefaultKey ? SmimeDefaultKey : _("<default>"));
  }

  if ((WithCrypto & APPLICATION_SMIME)
      && (msg->security & APPLICATION_SMIME)
      && (msg->security & ENCRYPT)
      && SmimeCryptAlg
      && *SmimeCryptAlg) {
    mutt_window_mvprintw (MuttIndexWindow, HDR_CRYPTINFO, 40, "%s%s", _("Encrypt with: "),
		NONULL(SmimeCryptAlg));
  }
}


#ifdef MIXMASTER

static void redraw_mix_line (LIST *chain)
{
  int c;
  char *t;

  /* L10N: "Mix" refers to the MixMaster chain for anonymous email */
  mutt_window_mvprintw (MuttIndexWindow, HDR_MIX, 0, TITLE_FMT, _("Mix: "));

  if (!chain)
  {
    addstr ("<no chain defined>");
    mutt_window_clrtoeol (MuttIndexWindow);
    return;
  }
  
  for (c = 12; chain; chain = chain->next)
  {
    t = chain->data;
    if (t && t[0] == '0' && t[1] == '\0')
      t = "<random>";
    
    if (c + mutt_strlen (t) + 2 >= MuttIndexWindow->cols)
      break;

    addstr (NONULL(t));
    if (chain->next)
      addstr (", ");

    c += mutt_strlen (t) + 2;
  }
}
#endif /* MIXMASTER */

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
      mutt_pretty_mailbox(pretty, sizeof (pretty));
      mutt_error(_("%s [#%d] no longer exists!"),
		 pretty, i+1);
      return -1;
    }
    
    if(idx[i]->content->stamp < st.st_mtime)
    {
      mutt_pretty_mailbox(pretty, sizeof (pretty));
      snprintf(msg, sizeof(msg), _("%s [#%d] modified. Update encoding?"),
	       pretty, i+1);
      
      if((r = mutt_yesorno(msg, MUTT_YES)) == MUTT_YES)
	mutt_update_encoding(idx[i]->content);
      else if(r == -1)
	return -1;
    }
  }

  return 0;
}

static void draw_envelope_addr (int line, ADDRESS *addr)
{
  char buf[LONG_STRING];

  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), addr, 1);
  mutt_window_mvprintw (MuttIndexWindow, line, 0, TITLE_FMT, Prompts[line]);
  mutt_paddstr (W, buf);
}

static void draw_envelope (HEADER *msg, char *fcc)
{
  draw_envelope_addr (HDR_FROM, msg->env->from);
  draw_envelope_addr (HDR_TO, msg->env->to);
  draw_envelope_addr (HDR_CC, msg->env->cc);
  draw_envelope_addr (HDR_BCC, msg->env->bcc);
  mutt_window_mvprintw (MuttIndexWindow, HDR_SUBJECT, 0, TITLE_FMT, Prompts[HDR_SUBJECT]);
  mutt_paddstr (W, NONULL (msg->env->subject));
  draw_envelope_addr (HDR_REPLYTO, msg->env->reply_to);
  mutt_window_mvprintw (MuttIndexWindow, HDR_FCC, 0, TITLE_FMT, Prompts[HDR_FCC]);
  mutt_paddstr (W, fcc);

  if (WithCrypto)
    redraw_crypt_lines (msg);

#ifdef MIXMASTER
  redraw_mix_line (msg->chain);
#endif

  SETCOLOR (MT_COLOR_STATUS);
  mutt_window_mvaddstr (MuttIndexWindow, HDR_ATTACH - 1, 0, _("-- Attachments"));
  mutt_window_clrtoeol (MuttIndexWindow);

  NORMAL_COLOR;
}

static int edit_address_list (int line, ADDRESS **addr)
{
  char buf[HUGE_STRING] = ""; /* needs to be large for alias expansion */
  char *err = NULL;
  
  mutt_addrlist_to_local (*addr);
  rfc822_write_address (buf, sizeof (buf), *addr, 0);
  if (mutt_get_field (Prompts[line], buf, sizeof (buf), MUTT_ALIAS) == 0)
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

  if (mutt_addrlist_to_intl (*addr, &err) != 0)
  {
    mutt_error (_("Warning: '%s' is a bad IDN."), err);
    mutt_refresh();
    FREE (&err);
  }

  /* redraw the expanded list so the user can see the result */
  buf[0] = 0;
  rfc822_write_address (buf, sizeof (buf), *addr, 1);
  mutt_window_move (MuttIndexWindow, line, HDR_XOFFSET);
  mutt_paddstr (W, buf);
  
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
  FREE (&idx[x]->tree);
  FREE (&idx[x]);
  for (; x < *idxlen - 1; x++)
    idx[x] = idx[x+1];
  idx[*idxlen - 1] = NULL;
  menu->max = --(*idxlen);
  
  return (0);
}

static void update_idx (MUTTMENU *menu, ATTACHPTR **idx, short idxlen)
{
  idx[idxlen]->level = (idxlen > 0) ? idx[idxlen-1]->level : 0;
  if (idxlen)
    idx[idxlen - 1]->content->next = idx[idxlen]->content;
  idx[idxlen]->content->aptr = idx[idxlen];
  menu->current = idxlen++;
  mutt_update_tree (idx, idxlen);
  menu->max = idxlen;
  return;
}


/* 
 * cum_attachs_size: Cumulative Attachments Size
 *
 * Returns the total number of bytes used by the attachments in the
 * attachment list _after_ content-transfer-encodings have been
 * applied.
 * 
 */

static unsigned long cum_attachs_size (MUTTMENU *menu)
{
  size_t s;
  unsigned short i;
  ATTACHPTR **idx = menu->data;
  CONTENT *info;
  BODY *b;
  
  for (i = 0, s = 0; i < menu->max; i++)
  {
    b = idx[i]->content;

    if (!b->content)
      b->content = mutt_get_content_info (b->filename, b);

    if ((info = b->content))
    {
      switch (b->encoding)
      {
	case ENCQUOTEDPRINTABLE:
	  s += 3 * (info->lobin + info->hibin) + info->ascii + info->crlf;
	  break;
	case ENCBASE64:
	  s += (4 * (info->lobin + info->hibin + info->ascii + info->crlf)) / 3;
	  break;
	default:
	  s += info->lobin + info->hibin + info->ascii + info->crlf;
	  break;
      }
    }
  }

  return s;
}

/* prototype for use below */
static void compose_status_line (char *buf, size_t buflen, size_t col, int cols, MUTTMENU *menu, 
      const char *p);

/*
 * compose_format_str()
 *
 * %a = total number of attachments 
 * %h = hostname  [option]
 * %l = approx. length of current message (in bytes) 
 * %v = Mutt version 
 *
 * This function is similar to status_format_str().  Look at that function for
 * help when modifying this function.
 */

static const char *
compose_format_str (char *buf, size_t buflen, size_t col, int cols, char op, const char *src,
		   const char *prefix, const char *ifstring,
		   const char *elsestring,
		   unsigned long data, format_flag flags)
{
  char fmt[SHORT_STRING], tmp[SHORT_STRING];
  int optional = (flags & MUTT_FORMAT_OPTIONAL);
  MUTTMENU *menu = (MUTTMENU *) data;

  *buf = 0;
  switch (op)
  {
    case 'a': /* total number of attachments */
	snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
	snprintf (buf, buflen, fmt, menu->max);
      break;

    case 'h':  /* hostname */
      snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
      snprintf (buf, buflen, fmt, NONULL(Hostname));
      break;

    case 'l': /* approx length of current message in bytes */
	snprintf (fmt, sizeof (fmt), "%%%ss", prefix);
	mutt_pretty_size (tmp, sizeof (tmp), menu ? cum_attachs_size(menu) : 0);
	snprintf (buf, buflen, fmt, tmp);
      break;

    case 'v':
      snprintf (fmt, sizeof (fmt), "Mutt %%s");
      snprintf (buf, buflen, fmt, MUTT_VERSION);
      break;

    case 0:
      *buf = 0;
      return (src);

    default:
      snprintf (buf, buflen, "%%%s%c", prefix, op);
      break;
  }

  if (optional)
    compose_status_line (buf, buflen, col, cols, menu, ifstring);
  else if (flags & MUTT_FORMAT_OPTIONAL)
    compose_status_line (buf, buflen, col, cols, menu, elsestring);

  return (src);
}

static void compose_status_line (char *buf, size_t buflen, size_t col, int cols,
                                 MUTTMENU *menu, const char *p)
{
  mutt_FormatString (buf, buflen, col, cols, p, compose_format_str, 
        (unsigned long) menu, 0);
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
                       HEADER *cur,   /* current message */
                       int flags)
{
  char helpstr[LONG_STRING];
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
  int oldSort, oldSortAux;
  struct stat st;

  mutt_attach_init (msg->content);
  idx = mutt_gen_attach_list (msg->content, -1, idx, &idxlen, &idxmax, 0, 1);

  menu = mutt_new_menu (MENU_COMPOSE);
  menu->offset = HDR_ATTACH;
  menu->max = idxlen;
  menu->make_entry = snd_entry;
  menu->tag = mutt_tag_attach;
  menu->data = idx;
  menu->help = mutt_compile_help (helpstr, sizeof (helpstr), MENU_COMPOSE, ComposeHelp);

  while (loop)
  {
    switch (op = mutt_menuLoop (menu))
    {
      case OP_REDRAW:
	draw_envelope (msg, fcc);
	menu->offset = HDR_ATTACH;
	menu->pagelen = MuttIndexWindow->rows - HDR_ATTACH;
	break;
      case OP_COMPOSE_EDIT_FROM:
	menu->redraw = edit_address_list (HDR_FROM, &msg->env->from);
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
	break;
      case OP_COMPOSE_EDIT_TO:
	menu->redraw = edit_address_list (HDR_TO, &msg->env->to);
	if (option (OPTCRYPTOPPORTUNISTICENCRYPT))
	{
	  crypt_opportunistic_encrypt (msg);
	  redraw_crypt_lines (msg);
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_BCC:
	menu->redraw = edit_address_list (HDR_BCC, &msg->env->bcc);
	if (option (OPTCRYPTOPPORTUNISTICENCRYPT))
	{
	  crypt_opportunistic_encrypt (msg);
	  redraw_crypt_lines (msg);
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
	break;
      case OP_COMPOSE_EDIT_CC:
	menu->redraw = edit_address_list (HDR_CC, &msg->env->cc);
	if (option (OPTCRYPTOPPORTUNISTICENCRYPT))
	{
	  crypt_opportunistic_encrypt (msg);
	  redraw_crypt_lines (msg);
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);	
        break;
      case OP_COMPOSE_EDIT_SUBJECT:
	if (msg->env->subject)
	  strfcpy (buf, msg->env->subject, sizeof (buf));
	else
	  buf[0] = 0;
	if (mutt_get_field ("Subject: ", buf, sizeof (buf), 0) == 0)
	{
	  mutt_str_replace (&msg->env->subject, buf);
	  mutt_window_move (MuttIndexWindow, HDR_SUBJECT, HDR_XOFFSET);
	  if (msg->env->subject)
	    mutt_paddstr (W, msg->env->subject);
	  else
	    mutt_window_clrtoeol(MuttIndexWindow);
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_REPLY_TO:
	menu->redraw = edit_address_list (HDR_REPLYTO, &msg->env->reply_to);
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
	break;
      case OP_COMPOSE_EDIT_FCC:
	strfcpy (buf, fcc, sizeof (buf));
	if (mutt_get_field ("Fcc: ", buf, sizeof (buf), MUTT_FILE | MUTT_CLEAR) == 0)
	{
	  strfcpy (fcc, buf, fcclen);
	  mutt_pretty_mailbox (fcc, fcclen);
	  mutt_window_move (MuttIndexWindow, HDR_FCC, HDR_XOFFSET);
	  mutt_paddstr (W, fcc);
	  fccSet = 1;
	}
	MAYBE_REDRAW (menu->redraw);
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;
      case OP_COMPOSE_EDIT_MESSAGE:
	if (Editor && (mutt_strcmp ("builtin", Editor) != 0) && !option (OPTEDITHDRS))
	{
	  mutt_edit_file (Editor, msg->content->filename);
	  mutt_update_encoding (msg->content);
	  menu->redraw = REDRAW_FULL;
	  mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
	  break;
	}
	/* fall through */
      case OP_COMPOSE_EDIT_HEADERS:
	if (mutt_strcmp ("builtin", Editor) != 0 &&
	    (op == OP_COMPOSE_EDIT_HEADERS ||
	    (op == OP_COMPOSE_EDIT_MESSAGE && option (OPTEDITHDRS))))
	{
	  char *tag = NULL, *err = NULL;
	  mutt_env_to_local (msg->env);
	  mutt_edit_headers (NONULL (Editor), msg->content->filename, msg,
			     fcc, fcclen);
	  if (mutt_env_to_intl (msg->env, &tag, &err))
	  {
	    mutt_error (_("Bad IDN in \"%s\": '%s'"), tag, err);
	    FREE (&err);
	  }
	  if (option (OPTCRYPTOPPORTUNISTICENCRYPT))
	    crypt_opportunistic_encrypt (msg);
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
          {
            FREE (&idx[i]->tree);
	    FREE (&idx[i]);
          }
	  idxlen = 0;
	  idx = mutt_gen_attach_list (msg->content, -1, idx, &idxlen, &idxmax, 0, 1);
	  menu->data = idx;
	  menu->max = idxlen;
	}

        menu->redraw = REDRAW_FULL;
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
	break;



      case OP_COMPOSE_ATTACH_KEY:
        if (!(WithCrypto & APPLICATION_PGP))
          break;       
	if (idxlen == idxmax)
        {
	  safe_realloc (&idx, sizeof (ATTACHPTR *) * (idxmax += 5));
	  menu->data = idx;
	}
	
	idx[idxlen] = (ATTACHPTR *) safe_calloc (1, sizeof (ATTACHPTR));
	if ((idx[idxlen]->content = crypt_pgp_make_key_attachment(NULL)) != NULL)
	{
	  update_idx (menu, idx, idxlen++);
	  menu->redraw |= REDRAW_INDEX;
	}
	else
	  FREE (&idx[idxlen]);

	menu->redraw |= REDRAW_STATUS;

	if (option(OPTNEEDREDRAW))
	{
	  menu->redraw = REDRAW_FULL;
	  unset_option(OPTNEEDREDRAW);
	}
	
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;


      case OP_COMPOSE_ATTACH_FILE:
	{
	  char *prompt, **files;
	  int error, numfiles;

	  fname[0] = 0;
	  prompt = _("Attach file");
	  numfiles = 0;
	  files = NULL;

	  if (_mutt_enter_fname (prompt, fname, sizeof (fname), &menu->redraw, 0, 1, &files, &numfiles) == -1 ||
	      *fname == '\0')
	    break;

	  if (idxlen + numfiles >= idxmax)
	  {
	    safe_realloc (&idx, sizeof (ATTACHPTR *) * (idxmax += 5 + numfiles));
	    menu->data = idx;
	  }

	  error = 0;
	  if (numfiles > 1)
	    mutt_message _("Attaching selected files...");
	  for (i = 0; i < numfiles; i++)
	  {
	    char *att = files[i];
	    idx[idxlen] = (ATTACHPTR *) safe_calloc (1, sizeof (ATTACHPTR));
            idx[idxlen]->unowned = 1;
	    idx[idxlen]->content = mutt_make_file_attach (att);
	    if (idx[idxlen]->content != NULL)
	      update_idx (menu, idx, idxlen++);
	    else
	    {
	      error = 1;
	      mutt_error (_("Unable to attach %s!"), att);
	      FREE (&idx[idxlen]);
	    }
	  }
	  
	  FREE (&files);
	  if (!error) mutt_clear_error ();

	  menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_ATTACH_MESSAGE:
	{
	  char *prompt;
	  HEADER *h;

	  fname[0] = 0;
	  prompt = _("Open mailbox to attach message from");

	  if (Context)
	  {
	    strfcpy (fname, NONULL (Context->path), sizeof (fname));
	    mutt_pretty_mailbox (fname, sizeof (fname));
	  }

	  if (mutt_enter_fname (prompt, fname, sizeof (fname), &menu->redraw, 1) == -1 || !fname[0])
	    break;

	  mutt_expand_path (fname, sizeof (fname));
#ifdef USE_IMAP
          if (!mx_is_imap (fname))
#endif
#ifdef USE_POP
          if (!mx_is_pop (fname))
#endif
	  /* check to make sure the file exists and is readable */
	  if (access (fname, R_OK) == -1)
	  {
	    mutt_perror (fname);
	    break;
	  }

	  menu->redraw = REDRAW_FULL;

	  ctx = mx_open_mailbox (fname, MUTT_READONLY, NULL);
	  if (ctx == NULL)
	  {
	    mutt_error (_("Unable to open mailbox %s"), fname);
	    break;
	  }

	  if (!ctx->msgcount)
	  {
	    mx_close_mailbox (ctx, NULL);
	    FREE (&ctx);
	    mutt_error _("No messages in that folder.");
	    break;
	  }

	  this = Context; /* remember current folder and sort methods*/
	  oldSort = Sort; oldSortAux = SortAux;
	  
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

	  if (idxlen + Context->tagged >= idxmax)
	  {
	    safe_realloc (&idx, sizeof (ATTACHPTR *) * (idxmax += 5 + Context->tagged));
	    menu->data = idx;
	  }

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
		FREE (&idx[idxlen]);
	      }
	    }
	  }
	  menu->redraw |= REDRAW_FULL;

	  if (close == OP_QUIT) 
	    mx_close_mailbox (Context, NULL);
	  else
	    mx_fastclose_mailbox (Context);
	  FREE (&Context);

	  /* go back to the folder we started from */
	  Context = this;
	  /* Restore old $sort and $sort_aux */
	  Sort = oldSort;
	  SortAux = oldSortAux;
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_DELETE:
	CHECK_COUNT;
        if (idx[menu->current]->unowned)
          idx[menu->current]->content->unlink = 0;
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

        menu->redraw |= REDRAW_STATUS;
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;

#define CURRENT idx[menu->current]->content
      
      case OP_COMPOSE_TOGGLE_RECODE:
      {      
        CHECK_COUNT;
        if (!mutt_is_text_part (CURRENT))
        {
	  mutt_error (_("Recoding only affects text attachments."));
	  break;
	}
        CURRENT->noconv = !CURRENT->noconv;
        if (CURRENT->noconv)
	  mutt_message (_("The current attachment won't be converted."));
        else
	  mutt_message (_("The current attachment will be converted."));
	menu->redraw = REDRAW_CURRENT;
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
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
	  mutt_str_replace (&idx[menu->current]->content->description, buf);
	  menu->redraw = REDRAW_CURRENT;
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_UPDATE_ENCODING:
        CHECK_COUNT;
        if (menu->tagprefix)
        {
	  BODY *top;
	  for (top = msg->content; top; top = top->next)
	  {
	    if (top->tagged)
	      mutt_update_encoding (top);
	  }
	  menu->redraw = REDRAW_FULL;
	}
        else
        {
          mutt_update_encoding(idx[menu->current]->content);
	  menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;
      
      case OP_COMPOSE_TOGGLE_DISPOSITION:
	/* toggle the content-disposition between inline/attachment */
	idx[menu->current]->content->disposition = (idx[menu->current]->content->disposition == DISPINLINE) ? DISPATTACH : DISPINLINE;
	menu->redraw = REDRAW_CURRENT;
	break;

      case OP_EDIT_TYPE:
	CHECK_COUNT;
        {
	  mutt_edit_content_type (NULL, idx[menu->current]->content, NULL);

	  /* this may have been a change to text/something */
	  mutt_update_encoding (idx[menu->current]->content);

	  menu->redraw = REDRAW_CURRENT;
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
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
	    menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
	    mutt_clear_error();
	  }
	  else
	    mutt_error _("Invalid encoding.");
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_SEND_MESSAGE:

        /* Note: We don't invoke send2-hook here, since we want to leave
	 * users an opportunity to change settings from the ":" prompt.
	 */
      
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
	  else if (i == MUTT_NO)
	    *fcc = 0;
	}

	loop = 0;
	r = 0;
	break;

      case OP_COMPOSE_EDIT_FILE:
	CHECK_COUNT;
	mutt_edit_file (NONULL(Editor), idx[menu->current]->content->filename);
	mutt_update_encoding (idx[menu->current]->content);
	menu->redraw = REDRAW_CURRENT | REDRAW_STATUS;
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
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
        /* No send2hook since this doesn't change the message. */
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

        /* No send2hook since this doesn't change the message. */
        break;
      
      case OP_COMPOSE_RENAME_FILE:
	CHECK_COUNT;
	strfcpy (fname, idx[menu->current]->content->filename, sizeof (fname));
	mutt_pretty_mailbox (fname, sizeof (fname));
	if (mutt_get_field (_("Rename to: "), fname, sizeof (fname), MUTT_FILE)
							== 0 && fname[0])
	{
	  if (stat(idx[menu->current]->content->filename, &st) == -1)
	  {
            /* L10N:
               "stat" is a system call. Do "man 2 stat" for more information. */
	    mutt_error (_("Can't stat %s: %s"), fname, strerror (errno));
	    break;
	  }

	  mutt_expand_path (fname, sizeof (fname));
	  if(mutt_rename_file (idx[menu->current]->content->filename, fname))
	    break;
	  
	  mutt_str_replace (&idx[menu->current]->content->filename, fname);
	  menu->redraw = REDRAW_CURRENT;

	  if(idx[menu->current]->content->stamp >= st.st_mtime)
	    mutt_stamp_attachment(idx[menu->current]->content);
	  
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_COMPOSE_NEW_MIME:
	{
	  char type[STRING];
	  char *p;
	  int itype;
	  FILE *fp;

          mutt_window_clearline (MuttMessageWindow, 0);
	  fname[0] = 0;
	  if (mutt_get_field (_("New file: "), fname, sizeof (fname), MUTT_FILE)
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
	    safe_realloc (&idx, sizeof (ATTACHPTR *) * (idxmax += 5));
	    menu->data = idx;
	  }

	  idx[idxlen] = (ATTACHPTR *) safe_calloc (1, sizeof (ATTACHPTR));
	  /* Touch the file */
	  if (!(fp = safe_fopen (fname, "w")))
	  {
	    mutt_error (_("Can't create file %s"), fname);
	    FREE (&idx[idxlen]);
	    continue;
	  }
	  safe_fclose (&fp);

	  if ((idx[idxlen]->content = mutt_make_file_attach (fname)) == NULL)
	  {
	    mutt_error _("What we have here is a failure to make an attachment");
	    continue;
	  }
	  update_idx (menu, idx, idxlen++);

	  idx[menu->current]->content->type = itype;
	  mutt_str_replace (&idx[menu->current]->content->subtype, p);
	  idx[menu->current]->content->unlink = 1;
	  menu->redraw |= REDRAW_INDEX | REDRAW_STATUS;

	  if (mutt_compose_attachment (idx[menu->current]->content))
	  {
	    mutt_update_encoding (idx[menu->current]->content);
	    menu->redraw = REDRAW_FULL;
	  }
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);    
        break;

      case OP_COMPOSE_EDIT_MIME:
	CHECK_COUNT;
	if (mutt_edit_attachment (idx[menu->current]->content))
	{
	  mutt_update_encoding (idx[menu->current]->content);
	  menu->redraw = REDRAW_FULL;
	}
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;

      case OP_VIEW_ATTACH:
      case OP_DISPLAY_HEADERS:
	CHECK_COUNT;
	mutt_attach_display_loop (menu, op, NULL, NULL, NULL, &idx, &idxlen, NULL, 0);
	menu->redraw = REDRAW_FULL;
        /* no send2hook, since this doesn't modify the message */
	break;

      case OP_SAVE:
	CHECK_COUNT;
	mutt_save_attachment_list (NULL, menu->tagprefix, menu->tagprefix ?  msg->content : idx[menu->current]->content, NULL, menu);
	MAYBE_REDRAW (menu->redraw);
        /* no send2hook, since this doesn't modify the message */
	break;

      case OP_PRINT:
	CHECK_COUNT;
	mutt_print_attachment_list (NULL, menu->tagprefix, menu->tagprefix ? msg->content : idx[menu->current]->content);
        /* no send2hook, since this doesn't modify the message */
	break;

      case OP_PIPE:
      case OP_FILTER:
        CHECK_COUNT;
	mutt_pipe_attachment_list (NULL, menu->tagprefix, menu->tagprefix ? msg->content : idx[menu->current]->content, op == OP_FILTER);
	if (op == OP_FILTER) /* cte might have changed */
	  menu->redraw = menu->tagprefix ? REDRAW_FULL : REDRAW_CURRENT;
        menu->redraw |= REDRAW_STATUS;
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
	break;

      case OP_EXIT:
	if ((i = query_quadoption (OPT_POSTPONE, _("Postpone this message?"))) == MUTT_NO)
	{
          for (i = 0; i < idxlen; i++)
            if (idx[i]->unowned)
              idx[i]->content->unlink = 0;

          if (!(flags & MUTT_COMPOSE_NOFREEHEADER))
          {
            while (idxlen-- > 0)
            {
              /* avoid freeing other attachments */
              idx[idxlen]->content->next = NULL;
              idx[idxlen]->content->parts = NULL;
              mutt_free_body (&idx[idxlen]->content);
              FREE (&idx[idxlen]->tree);
              FREE (&idx[idxlen]);
            }
            FREE (&idx);
            idxlen = 0;
            idxmax = 0;
          }
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
        {
	  mutt_update_encoding (msg->content);
	  menu->redraw |= REDRAW_STATUS;
	}
	break;

      case OP_COMPOSE_WRITE_MESSAGE:

       fname[0] = '\0';
       if (Context)
       {
	 strfcpy (fname, NONULL (Context->path), sizeof (fname));
	 mutt_pretty_mailbox (fname, sizeof (fname));
       }
       if (idxlen)
         msg->content = idx[0]->content;
       if (mutt_enter_fname (_("Write message to mailbox"), fname, sizeof (fname),
                             &menu->redraw, 1) != -1 && fname[0])
       {
         mutt_message (_("Writing message to %s ..."), fname);
         mutt_expand_path (fname, sizeof (fname));

         if (msg->content->next)
           msg->content = mutt_make_multipart (msg->content);

         if (mutt_write_fcc (fname, msg, NULL, 0, NULL) < 0)
           msg->content = mutt_remove_multipart (msg->content);
         else
           mutt_message _("Message written.");
       }
       break;



      case OP_COMPOSE_PGP_MENU:
        if (!(WithCrypto & APPLICATION_PGP))
          break;
	if ((WithCrypto & APPLICATION_SMIME)
            && (msg->security & APPLICATION_SMIME))
	{
          if (msg->security & (ENCRYPT | SIGN))
          {
            if (mutt_yesorno (_("S/MIME already selected. Clear & continue ? "),
                              MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error ();
              break;
            }
            msg->security &= ~(ENCRYPT | SIGN);
          }
	  msg->security &= ~APPLICATION_SMIME;
	  msg->security |= APPLICATION_PGP;
          crypt_opportunistic_encrypt (msg);
          redraw_crypt_lines (msg);
	}
	msg->security = crypt_pgp_send_menu (msg, &menu->redraw);
	redraw_crypt_lines (msg);
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;


      case OP_FORGET_PASSPHRASE:
	crypt_forget_passphrase ();
	break;


      case OP_COMPOSE_SMIME_MENU:
        if (!(WithCrypto & APPLICATION_SMIME))
          break;

	if ((WithCrypto & APPLICATION_PGP)
            && (msg->security & APPLICATION_PGP))
	{
          if (msg->security & (ENCRYPT | SIGN))
          {
            if (mutt_yesorno (_("PGP already selected. Clear & continue ? "),
                                MUTT_YES) != MUTT_YES)
            {
              mutt_clear_error ();
              break;
            }
            msg->security &= ~(ENCRYPT | SIGN);
          }
	  msg->security &= ~APPLICATION_PGP;
	  msg->security |= APPLICATION_SMIME;
          crypt_opportunistic_encrypt (msg);
          redraw_crypt_lines (msg);
	}
	msg->security = crypt_smime_send_menu(msg, &menu->redraw);
	redraw_crypt_lines (msg);
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;


#ifdef MIXMASTER
      case OP_COMPOSE_MIX:
      
      	mix_make_chain (&msg->chain, &menu->redraw);
        mutt_message_hook (NULL, msg, MUTT_SEND2HOOK);
        break;
#endif

    }

    /* Draw formatted compose status line */
    if (menu->redraw & REDRAW_STATUS) 
    {
        compose_status_line (buf, sizeof (buf), 0, MuttStatusWindow->cols, menu, NONULL(ComposeFormat));
	mutt_window_move (MuttStatusWindow, 0, 0);
	SETCOLOR (MT_COLOR_STATUS);
	mutt_paddstr (MuttStatusWindow->cols, buf);
	NORMAL_COLOR;
	menu->redraw &= ~REDRAW_STATUS;
    }
  }

  mutt_menuDestroy (&menu);

  if (idxlen)
  {
    msg->content = idx[0]->content;
    for (i = 0; i < idxlen; i++)
    {
      idx[i]->content->aptr = NULL;
      FREE (&idx[i]->tree);
      FREE (&idx[i]);
    }
  }
  else
    msg->content = NULL;

  FREE (&idx);

  return (r);
}

