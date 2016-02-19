/* Copyright (C) 2004 Justin Hibbits <jrh29@po.cwru.edu>
 * Copyright (C) 2004 Thomer M. Gil <mutt@thomer.com>
 * Copyright (C) 2015-2016 Richard Russon <rich@flatcap.org>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include "mutt.h"
#include "buffy.h"
#include "keymap.h"
#include "mutt_curses.h"
#include "mutt_menu.h"

static BUFFY *TopBuffy;
static BUFFY *BottomBuffy;
static int    known_lines;

struct sidebar_entry {
	char         box[SHORT_STRING];
	unsigned int size;
	unsigned int new;
	unsigned int flagged;
};

static BUFFY *
find_next_new (int wrap)
{
	BUFFY *b = CurBuffy;
	if (!b)
		return NULL;

	do {
		b = b->next;
		if (!b && wrap) {
			b = TopBuffy;
		}
		if (!b || (b == CurBuffy)) {
			break;
		}
		if (b->msg_unread || b->msg_flagged || mutt_find_list (SidebarWhitelist, b->path)) {
			return b;
		}
	} while (b);

	return NULL;
}

static BUFFY *
find_prev_new (int wrap)
{
	BUFFY *b = CurBuffy;
	if (!b)
		return NULL;

	do {
		b = b->prev;
		if (!b && wrap) {
			b = BottomBuffy;
		}
		if (!b || (b == CurBuffy)) {
			break;
		}
		if (b->msg_unread || b->msg_flagged || mutt_find_list (SidebarWhitelist, b->path)) {
			return b;
		}
	} while (b);

	return NULL;
}


static void
calc_boundaries (void)
{
	BUFFY *b = Incoming;
	if (!b)
		return;

	int count = LINES - 2;
	if (option (OPTHELP))
		count--;

	if (known_lines != LINES) {
		TopBuffy = BottomBuffy = NULL;
		known_lines = LINES;
	}
	for (; b->next; b = b->next)
		b->next->prev = b;

	if (!TopBuffy && !BottomBuffy)
		TopBuffy = Incoming;

	if (!BottomBuffy) {
		BottomBuffy = TopBuffy;
		while (--count && BottomBuffy->next) {
			BottomBuffy = BottomBuffy->next;
		}
	} else if (TopBuffy == CurBuffy->next) {
		BottomBuffy = CurBuffy;
		b = BottomBuffy;
		while (--count && b->prev) {
			b = b->prev;
		}
		TopBuffy = b;
	} else if (BottomBuffy == CurBuffy->prev) {
		TopBuffy = CurBuffy;
		b = TopBuffy;
		while (--count && b->next) {
			b = b->next;
		}
		BottomBuffy = b;
	}
}

static const char *
cb_format_str (char *dest, size_t destlen, size_t col, char op, const char *src,
	const char *prefix, const char *ifstring, const char *elsestring,
	unsigned long data, format_flag flags)
{
	struct sidebar_entry *sbe = (struct sidebar_entry *) data;
	unsigned int optional;
	char fmt[SHORT_STRING], buf[SHORT_STRING];

	if (!sbe || !dest)
		return src;

	dest[0] = 0;	/* Just in case there's nothing to do */

	optional = flags & M_FORMAT_OPTIONAL;

	switch (op) {
		case 'B':
			mutt_format_s (dest, destlen, prefix, sbe->box);
			break;

		case 'F':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, sbe->flagged);
			} else if (sbe->flagged == 0) {
				optional = 0;
			}
			break;

		case 'N':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, sbe->new);
			} else if (sbe->new == 0) {
				optional = 0;
			}
			break;

		case 'S':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, sbe->size);
			} else if (sbe->size == 0) {
				optional = 0;
			}
			break;

		case '!':
			if (sbe->flagged == 0) {
				mutt_format_s (dest, destlen, prefix, "");
			} else if (sbe->flagged == 1) {
				mutt_format_s (dest, destlen, prefix, "!");
			} else if (sbe->flagged == 2) {
				mutt_format_s (dest, destlen, prefix, "!!");
			} else {
				snprintf (buf, sizeof (buf), "%d!", sbe->flagged);
				mutt_format_s (dest, destlen, prefix, buf);
			}
			break;
	}

	if (optional)
		mutt_FormatString (dest, destlen, col, ifstring,   cb_format_str, (unsigned long) sbe, flags);
	else if (flags & M_FORMAT_OPTIONAL)
		mutt_FormatString (dest, destlen, col, elsestring, cb_format_str, (unsigned long) sbe, flags);

	/* We return the format string, unchanged */
	return src;
}

static void
make_sidebar_entry (char *buf, unsigned int buflen, int width, char *box,
	unsigned int size, unsigned int new, unsigned int flagged)
{
	struct sidebar_entry sbe;

	if (!buf || !box)
		return;

	sbe.new = new;
	sbe.flagged = flagged;
	sbe.size = size;
	strncpy (sbe.box, box, sizeof (sbe.box)-1);

	int box_len = strlen (box);
	sbe.box[box_len] = '\0';

	/* Temporarily lie about the screen width */
	int oc = COLS;
	COLS = width + SidebarWidth;
	mutt_FormatString (buf, buflen, 0, NONULL(SidebarFormat), cb_format_str, (unsigned long) &sbe, 0);
	COLS = oc;

	/* Force string to be exactly the right width */
	int w = mutt_strwidth (buf);
	int s = strlen (buf);
	if (w < width) {
		/* Pad with spaces */
		memset (buf + s, ' ', width - w);
		buf[s + width - w] = 0;
	} else if (w > width) {
		/* Truncate to fit */
		int len = mutt_wstr_trunc (buf, buflen, width, NULL);
		buf[len] = 0;
	}
}


void
sb_draw (void)
{
#ifndef USE_SLANG_CURSES
	attr_t attrs;
#endif
	short color_pair;

	/* Calculate the width of the delimiter in screen cells */
	wchar_t sd[4];
	mbstowcs (sd, NONULL(SidebarDelim), 4);
	int delim_len = wcwidth (sd[0]);

	static bool initialized = false;
	static int prev_show_value;
	static short saveSidebarWidth;
	int lines = 0;
	int SidebarHeight;

	if (option (OPTSTATUSONTOP) || option (OPTHELP))
		lines++; /* either one will occupy the first line */

	/* initialize first time */
	if (!initialized) {
		prev_show_value = option (OPTSIDEBAR);
		saveSidebarWidth = SidebarWidth;
		if (!option (OPTSIDEBAR))
			SidebarWidth = 0;
		SidebarLastRefresh = time (NULL);
		initialized = true;
	}

	/* save or restore the value SidebarWidth */
	if (prev_show_value != option (OPTSIDEBAR)) {
		if (prev_show_value && !option (OPTSIDEBAR)) {
			saveSidebarWidth = SidebarWidth;
			SidebarWidth = 0;
		} else if (!prev_show_value && option (OPTSIDEBAR)) {
			mutt_buffy_check (1); /* we probably have bad or no numbers */
			SidebarWidth = saveSidebarWidth;
		}
		prev_show_value = option (OPTSIDEBAR);
	}

	if ((SidebarWidth > 0) && option (OPTSIDEBAR) && (delim_len >= SidebarWidth)) {
		unset_option (OPTSIDEBAR);
		if (saveSidebarWidth > delim_len) {
			SidebarWidth = saveSidebarWidth;
			mutt_error (_("Value for sidebar_delim is too long. Disabling sidebar."));
			sleep (2);
		} else {
			SidebarWidth = 0;
			mutt_error (_("Value for sidebar_delim is too long. Disabling sidebar. Please set your sidebar_width to a sane value."));
			sleep (4); /* the advise to set a sane value should be seen long enough */
		}
		saveSidebarWidth = 0;
		return;
	}

	if ((SidebarWidth == 0) || !option (OPTSIDEBAR)) {
		if (SidebarWidth > 0) {
			saveSidebarWidth = SidebarWidth;
			SidebarWidth = 0;
		}
		unset_option (OPTSIDEBAR);
		return;
	}

	/* get attributes for divider */
	SETCOLOR(MT_COLOR_STATUS);
#ifndef USE_SLANG_CURSES
	attr_get (&attrs, &color_pair, 0);
#else
	color_pair = attr_get();
#endif
	SETCOLOR(MT_COLOR_NORMAL);

	/* draw the divider */
	SidebarHeight = LINES - 1;
	if (option (OPTHELP) || !option (OPTSTATUSONTOP))
		SidebarHeight--;

	for (; lines < SidebarHeight; lines++) {
		move (lines, SidebarWidth - delim_len);
		addstr (NONULL(SidebarDelim));
#ifndef USE_SLANG_CURSES
		mvchgat (lines, SidebarWidth - delim_len, delim_len, 0, color_pair, NULL);
#endif
	}

	if (!Incoming)
		return;

	lines = 0;
	if (option (OPTSTATUSONTOP) || option (OPTHELP))
		lines++; /* either one will occupy the first line */

	if ((known_lines != LINES) || !TopBuffy || !BottomBuffy)
		calc_boundaries();
	if (!CurBuffy)
		CurBuffy = Incoming;

	SETCOLOR(MT_COLOR_NORMAL);

	BUFFY *b;
	for (b = TopBuffy; b && (lines < SidebarHeight); b = b->next) {
		if (b == CurBuffy) {
			SETCOLOR(MT_COLOR_INDICATOR);
		} else if (b->msg_unread > 0) {
			SETCOLOR(MT_COLOR_NEW);
		} else if (b->msg_flagged > 0) {
			SETCOLOR(MT_COLOR_FLAGGED);
		} else if (option (OPTSIDEBARNEWMAILONLY)) {
			/* sidebar_newmail_only is enabled... */
			if (b == Incoming ||
					(Context && (strcmp (b->path, Context->path) == 0)) ||
					mutt_find_list (SidebarWhitelist, b->path)) {
				SETCOLOR(MT_COLOR_NORMAL);
			} else {
				/* but mailbox isn't whitelisted */
				continue;
			}
		} else {
			SETCOLOR(MT_COLOR_NORMAL);
		}

		move (lines, 0);
		if (Context && Context->path &&
			(!strcmp (b->path, Context->path)||
			 !strcmp (b->realpath, Context->path))) {
			b->msg_unread = Context->unread;
			b->msgcount = Context->msgcount;
			b->msg_flagged = Context->flagged;
		}

		/* compute length of Maildir without trailing separator */
		size_t maildirlen = strlen (Maildir);
		if (SidebarDelimChars && strchr (SidebarDelimChars, Maildir[maildirlen - 1])) {
			maildirlen--;
		}

		/* check whether Maildir is a prefix of the current folder's path */
		short maildir_is_prefix = 0;
		if ((strlen (b->path) > maildirlen) && (strncmp (Maildir, b->path, maildirlen) == 0)) {
			maildir_is_prefix = 1;
		}
		/* calculate depth of current folder and generate its display name with indented spaces */
		int sidebar_folder_depth = 0;
		char *sidebar_folder_name;
		int i;
		if (option (OPTSIDEBARSHORTPATH)) {
			/* disregard a trailing separator, so strlen() - 2 */
			sidebar_folder_name = b->path;
			for (i = strlen (sidebar_folder_name) - 2; i >= 0; i--) {
				if (SidebarDelimChars &&
						strchr (SidebarDelimChars, sidebar_folder_name[i])) {
					sidebar_folder_name += (i + 1);
					break;
				}
			}
		} else {
			sidebar_folder_name = b->path + maildir_is_prefix * (maildirlen + 1);
		}
		if (maildir_is_prefix && option (OPTSIDEBARFOLDERINDENT)) {
			const char *tmp_folder_name;
			int lastsep = 0;
			tmp_folder_name = b->path + maildirlen + 1;
			for (i = 0; i < strlen (tmp_folder_name) - 1; i++) {
				if (SidebarDelimChars &&
						strchr (SidebarDelimChars, tmp_folder_name[i])) {
					sidebar_folder_depth++;
					lastsep = i + 1;
				}
			}
			if (sidebar_folder_depth > 0) {
				if (option (OPTSIDEBARSHORTPATH)) {
					tmp_folder_name += lastsep;  /* basename */
				}
				sidebar_folder_name = malloc (strlen (tmp_folder_name) + sidebar_folder_depth*strlen (NONULL(SidebarIndentStr)) + 1);
				sidebar_folder_name[0]=0;
				for (i=0; i < sidebar_folder_depth; i++)
					strncat (sidebar_folder_name, NONULL(SidebarIndentStr), strlen (NONULL(SidebarIndentStr)));
				strncat (sidebar_folder_name, tmp_folder_name, strlen (tmp_folder_name));
			}
		}
		char str[SHORT_STRING];
		make_sidebar_entry (str, sizeof (str), SidebarWidth - delim_len,
			sidebar_folder_name, b->msgcount,
			b->msg_unread, b->msg_flagged);
		printw ("%s", str);
		if (sidebar_folder_depth > 0)
			free (sidebar_folder_name);
		lines++;
	}

	SETCOLOR(MT_COLOR_NORMAL);
	for (; lines < SidebarHeight; lines++) {
		int i = 0;
		move (lines, 0);
		for (; i < (SidebarWidth - delim_len); i++)
			addch (' ');
	}
}

int
sb_should_refresh (void)
{
	if (option (OPTSIDEBAR) && (SidebarRefresh > 0)) {
		if ((time (NULL) - SidebarLastRefresh) >= SidebarRefresh) {
			return 1;
		}
	}
	return 0;
}

void
sb_change_mailbox (int op)
{
	BUFFY *b;
	if ((SidebarWidth == 0) || !CurBuffy)
		return;

	switch (op) {
		case OP_SIDEBAR_NEXT:
			if (!option (OPTSIDEBARNEWMAILONLY)) {
				if (!CurBuffy->next)
					return;
				CurBuffy = CurBuffy->next;
				break;
			}
		case OP_SIDEBAR_NEXT_NEW:
			b = find_next_new (option (OPTSIDEBARNEXTNEWWRAP));
			if (!b) {
				return;
			} else {
				CurBuffy = b;
			}
			break;
		case OP_SIDEBAR_PREV:
			if (!option (OPTSIDEBARNEWMAILONLY)) {
				if (!CurBuffy->prev)
					return;
				CurBuffy = CurBuffy->prev;
				break;
			}
		case OP_SIDEBAR_PREV_NEW:
			b = find_prev_new (option (OPTSIDEBARNEXTNEWWRAP));
			if (!b) {
				return;
			} else {
				CurBuffy = b;
			}
			break;
		case OP_SIDEBAR_SCROLL_UP:
			CurBuffy = TopBuffy;
			if (CurBuffy != Incoming) {
				calc_boundaries();
				CurBuffy = CurBuffy->prev;
			}
			break;
		case OP_SIDEBAR_SCROLL_DOWN:
			CurBuffy = BottomBuffy;
			if (CurBuffy->next) {
				calc_boundaries();
				CurBuffy = CurBuffy->next;
			}
			break;
		default:
			return;
	}
	calc_boundaries();
	sb_draw();
}

void
sb_set_buffystats (const CONTEXT *ctx)
{
	/* Even if the sidebar's hidden,
	 * we should take note of the new data. */
	BUFFY *b = Incoming;
	if (!ctx || !b)
		return;

	for (; b; b = b->next) {
		if (!strcmp (b->path,     ctx->path) ||
		    !strcmp (b->realpath, ctx->path)) {
			b->msg_unread  = ctx->unread;
			b->msgcount    = ctx->msgcount;
			b->msg_flagged = ctx->flagged;
			break;
		}
	}
}

void
sb_set_open_buffy (char *path)
{
	BUFFY *b = CurBuffy = Incoming;

	if (!path || !b)
		return;

	for (; b; b = b->next) {
		if (!strcmp (b->path,     path) ||
		    !strcmp (b->realpath, path)) {
			CurBuffy = b;
			break;
		}
	}
}

void
sb_set_update_time (void)
{
	SidebarLastRefresh = time (NULL);
}

