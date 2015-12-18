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
#include "sort.h"

/* Previous values for some sidebar config */
static short  OldVisible;	/* sidebar_visible */
static short  OldWidth;		/* sidebar_width */
static time_t LastRefresh;	/* Time of last refresh */

static BUFFY *TopBuffy;
static BUFFY *BotBuffy;
static int    known_lines;

/**
 * struct sidebar_entry - Info about folders in the sidebar
 *
 * Used in the mutt_FormatString callback
 */
struct sidebar_entry {
	char         box[SHORT_STRING];
	unsigned int size;
	unsigned int new;
	unsigned int flagged;
};


/**
 * find_next_new - Find the next folder that contains new mail
 * @wrap: Wrap around to the beginning if the end is reached
 *
 * Search down the list of mail folders for one containing new, or flagged,
 * mail, or a folder that is in the SidebarWhitelist.
 *
 * Returns:
 *	BUFFY*: Success
 *	NULL:   Failure
 */
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

/**
 * find_prev_new - Find the previous folder that contains new mail
 * @wrap: Wrap around to the beginning if the end is reached
 *
 * Search up the list of mail folders for one containing new, or flagged, mail,
 * or a folder that is in the SidebarWhitelist.
 *
 * Returns:
 *	BUFFY*: Success
 *	NULL:   Failure
 */
static BUFFY *
find_prev_new (int wrap)
{
	BUFFY *b = CurBuffy;
	if (!b)
		return NULL;

	do {
		b = b->prev;
		if (!b && wrap) {
			b = BotBuffy;
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

/**
 * calc_boundaries - Keep our TopBuffy/BotBuffy up-to-date
 *
 * Whenever the sidebar's view of the BUFFYs changes, or the screen changes
 * size, we should check TopBuffy and BotBuffy still have the correct
 * values.
 *
 * Ideally, this should happen in the core of mutt.
 */
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
		TopBuffy = BotBuffy = NULL;
		known_lines = LINES;
	}
	for (; b->next; b = b->next)
		b->next->prev = b;

	if (!TopBuffy && !BotBuffy)
		TopBuffy = Incoming;

	if (!BotBuffy) {
		BotBuffy = TopBuffy;
		while (--count && BotBuffy->next) {
			BotBuffy = BotBuffy->next;
		}
	} else if (TopBuffy == CurBuffy->next) {
		BotBuffy = CurBuffy;
		b = BotBuffy;
		while (--count && b->prev) {
			b = b->prev;
		}
		TopBuffy = b;
	} else if (BotBuffy == CurBuffy->prev) {
		TopBuffy = CurBuffy;
		b = TopBuffy;
		while (--count && b->next) {
			b = b->next;
		}
		BotBuffy = b;
	}
}

/**
 * cb_format_str - Create the string to show in the sidebar
 * @dest:        Buffer in which to save string
 * @destlen:     Buffer length
 * @col:         Starting column, UNUSED
 * @op:          printf-like operator, e.g. 'B'
 * @src:         printf-like format string
 * @prefix:      Field formatting string, UNUSED
 * @ifstring:    If condition is met, display this string
 * @elsestring:  Otherwise, display this string
 * @data:        Pointer to our sidebar_entry
 * @flags:       Format flags, e.g. M_FORMAT_OPTIONAL
 *
 * cb_format_str is a callback function for mutt_FormatString.  It understands
 * five operators. '%B' : Mailbox name, '%F' : Number of flagged messages,
 * '%N' : Number of new messages, '%S' : Size (total number of messages),
 * '%!' : Icon denoting number of flagged messages.
 *
 * Returns: src (unchanged)
 */
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

/**
 * make_sidebar_entry - Turn mailbox data into a sidebar string
 * @buf:     Buffer in which to save string
 * @buflen:  Buffer length
 * @width:   Desired width in screen cells
 * @box:     Mailbox name
 * @size:    Size (total number of messages)
 * @new:     Number of new messages
 * @flagged: Number of flagged messages
 *
 * Take all the relevant mailbox data and the desired screen width and then get
 * mutt_FormatString to do the actual work. mutt_FormatString will callback to
 * us using cb_format_str() for the sidebar specific formatting characters.
 */
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

/**
 * cb_qsort_buffy - qsort callback to sort BUFFYs
 * @a: First  BUFFY to compare
 * @b: Second BUFFY to compare
 *
 * Compare the paths of two BUFFYs taking the locale into account.
 *
 * Returns:
 *	-1: a precedes b
 *	 0: a and b are identical
 *	 1: b precedes a
 */
static int
cb_qsort_buffy (const void *a, const void *b)
{
	const BUFFY *b1 = *(const BUFFY **) a;
	const BUFFY *b2 = *(const BUFFY **) b;

	/* Special case -- move hidden BUFFYs to the end */
	if (b1->is_hidden != b2->is_hidden) {
		if (b1->is_hidden)
			return 1;
		else
			return -1;
	}

	int result = 0;

	switch ((SidebarSort & SORT_MASK)) {
		case SORT_COUNT:
			result = (b2->msg_count - b1->msg_count);
			break;
		case SORT_COUNT_NEW:
			result = (b2->msg_unread - b1->msg_unread);
			break;
		case SORT_FLAGGED:
			result = (b2->msg_flagged - b1->msg_flagged);
			break;
		case SORT_PATH:
			result = mutt_strcmp (b1->path, b2->path);
			break;
	}

	if (SidebarSort & SORT_REVERSE)
		result = -result;

	return result;
}

/**
 * sort_buffy_array - Sort an array of BUFFY pointers
 * @arr:     array of BUFFYs
 * @arr_len: number of BUFFYs in array
 *
 * Sort an array of BUFFY pointers according to the current sort config
 * option "sidebar_sort". This calls qsort to do the work which calls our
 * callback function "cb_qsort_buffy".
 *
 * Once sorted, the prev/next links will be reconstructed.
 */
static void
sort_buffy_array (BUFFY **arr, int arr_len)
{
	if (!arr)
		return;

	qsort (arr, arr_len, sizeof (*arr), cb_qsort_buffy);

	int i;
	for (i = 0; i < (arr_len - 1); i++) {
		arr[i]->next = arr[i + 1];
	}
	arr[arr_len - 1]->next = NULL;

	for (i = 1; i < arr_len; i++) {
		arr[i]->prev = arr[i - 1];
	}
	arr[0]->prev = NULL;
}

/**
 * sb_init - Set some default values for the sidebar.
 */
void
sb_init (void)
{
	OldVisible = option (OPTSIDEBAR);
	if (SidebarWidth > 0) {
		OldWidth = SidebarWidth;
	} else {
		OldWidth = 20;
		if (OldVisible) {
			SidebarWidth = OldWidth;
		}
	}
}

/**
 * sb_draw - Completely redraw the sidebar
 *
 * Completely refresh the sidebar region.  First draw the divider; then, for
 * each BUFFY, call make_sidebar_entry; finally blank out any remaining space.
 */
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

	int row = 0;
	int SidebarHeight;

	if (option (OPTSTATUSONTOP) || option (OPTHELP))
		row++; /* either one will occupy the first line */

	/* save or restore the value SidebarWidth */
	if (OldVisible != option (OPTSIDEBAR)) {
		if (OldVisible && !option (OPTSIDEBAR)) {
			OldWidth = SidebarWidth;
			SidebarWidth = 0;
		} else if (!OldVisible && option (OPTSIDEBAR)) {
			mutt_buffy_check (1); /* we probably have bad or no numbers */
			SidebarWidth = OldWidth;
		}
		OldVisible = option (OPTSIDEBAR);
	}

	if ((SidebarWidth > 0) && option (OPTSIDEBAR) && (delim_len >= SidebarWidth)) {
		unset_option (OPTSIDEBAR);
		if (OldWidth > delim_len) {
			SidebarWidth = OldWidth;
			mutt_error (_("Value for sidebar_delim is too long. Disabling sidebar."));
			sleep (2);
		} else {
			SidebarWidth = 0;
			mutt_error (_("Value for sidebar_delim is too long. Disabling sidebar. Please set your sidebar_width to a sane value."));
			sleep (4); /* the advise to set a sane value should be seen long enough */
		}
		OldWidth = 0;
		return;
	}

	if ((SidebarWidth == 0) || !option (OPTSIDEBAR)) {
		if (SidebarWidth > 0) {
			OldWidth = SidebarWidth;
			SidebarWidth = 0;
		}
		unset_option (OPTSIDEBAR);
		return;
	}

	/* get attributes for divider */
	SETCOLOR(MT_COLOR_DIVIDER);
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

	for (; row < SidebarHeight; row++) {
		move (row, SidebarWidth - delim_len);
		addstr (NONULL(SidebarDelim));
#ifndef USE_SLANG_CURSES
		mvchgat (row, SidebarWidth - delim_len, delim_len, 0, color_pair, NULL);
#endif
	}

	if (!Incoming)
		return;

	row = 0;
	if (option (OPTSTATUSONTOP) || option (OPTHELP))
		row++; /* either one will occupy the first line */

	if ((known_lines != LINES) || !TopBuffy || !BotBuffy)
		calc_boundaries();
	if (!CurBuffy)
		CurBuffy = Incoming;

	SETCOLOR(MT_COLOR_NORMAL);

	BUFFY *b;
	for (b = TopBuffy; b && (row < SidebarHeight); b = b->next) {
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

		move (row, 0);
		if (Context && Context->path &&
			(!strcmp (b->path, Context->path)||
			 !strcmp (b->realpath, Context->path))) {
			b->msg_unread  = Context->unread;
			b->msg_count   = Context->msgcount;
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
			sidebar_folder_name, b->msg_count,
			b->msg_unread, b->msg_flagged);
		printw ("%s", str);
		if (sidebar_folder_depth > 0)
			free (sidebar_folder_name);
		row++;
	}

	SETCOLOR(MT_COLOR_NORMAL);
	for (; row < SidebarHeight; row++) {
		int i = 0;
		move (row, 0);
		for (; i < (SidebarWidth - delim_len); i++)
			addch (' ');
	}
}

/**
 * sb_should_refresh - Check if the sidebar is due to be refreshed
 *
 * The "sidebar_refresh" config option allows the user to limit the frequency
 * with which the sidebar is refreshed.
 *
 * Returns:
 *	1  Yes, refresh is due
 *	0  No,  refresh happened recently
 */
int
sb_should_refresh (void)
{
	if (!option (OPTSIDEBAR))
		return 0;

	if (SidebarRefresh == 0)
		return 0;

	time_t diff = (time (NULL) - LastRefresh);

	return (diff >= SidebarRefresh);
}

/**
 * sb_change_mailbox - Change the selected mailbox
 * @op: Operation code
 *
 * Change the selected mailbox, e.g. "Next mailbox", "Previous Mailbox
 * with new mail". The operations are listed OPS.SIDEBAR which is built
 * into an enum in keymap_defs.h.
 *
 * If the operation is successful, CurBuffy will be set to the new mailbox.
 * This function only *selects* the mailbox, doesn't *open* it.
 *
 * Allowed values are: OP_SIDEBAR_NEXT, OP_SIDEBAR_NEXT_NEW, OP_SIDEBAR_PREV,
 * OP_SIDEBAR_PREV_NEW, OP_SIDEBAR_SCROLL_DOWN, OP_SIDEBAR_SCROLL_UP.
 */
void
sb_change_mailbox (int op)
{
	BUFFY *b;
	if (!CurBuffy)
		return;

	switch (op) {
		case OP_SIDEBAR_NEXT:
			if (!option (OPTSIDEBARNEWMAILONLY)) {
				if (!CurBuffy->next)
					return;
				CurBuffy = CurBuffy->next;
				break;
			}
			/* drop through */
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
			/* drop through */
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
			CurBuffy = BotBuffy;
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

/**
 * sb_set_buffystats - Update the BUFFY's message counts from the CONTEXT
 * @ctx:  A mailbox CONTEXT
 *
 * Given a mailbox CONTEXT, find a matching mailbox BUFFY and copy the message
 * counts into it.
 */
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
			b->msg_count   = ctx->msgcount;
			b->msg_flagged = ctx->flagged;
			break;
		}
	}
}

/**
 * sb_set_open_buffy - Set the CurBuffy based on a mailbox path
 * @path: Mailbox path
 *
 * Search through the list of mailboxes.  If a BUFFY has a matching path, set
 * CurBuffy to it.
 */
void
sb_set_open_buffy (char *path)
{
	/* Even if the sidebar is hidden */

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

/**
 * sb_set_update_time - Note the time that the sidebar was updated
 *
 * Update the timestamp representing the last sidebar update.  If the user
 * configures "sidebar_refresh", this will help to reduce traffic.
 */
void
sb_set_update_time (void)
{
	/* XXX - should this be public? */

	LastRefresh = time (NULL);
}

