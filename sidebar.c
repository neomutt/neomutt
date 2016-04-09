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
#include "mx.h"
#include "sort.h"

/* Previous values for some sidebar config */
static short  OldVisible;	/* sidebar_visible */
static short  OldWidth;		/* sidebar_width */
static short  PreviousSort;	/* sidebar_sort_method */
static time_t LastRefresh;	/* Time of last refresh */

/* Keep track of various BUFFYs */
static BUFFY *TopBuffy;		/* First mailbox visible in sidebar */
static BUFFY *OpnBuffy;		/* Current (open) mailbox */
static BUFFY *HilBuffy;		/* Highlighted mailbox */
static BUFFY *BotBuffy;		/* Last mailbox visible in sidebar */
static BUFFY *Outgoing;		/* Last mailbox in the linked list */

/**
 * struct sidebar_entry - Info about folders in the sidebar
 *
 * Used in the mutt_FormatString callback
 */
struct sidebar_entry {
	char         box[SHORT_STRING];
	BUFFY       *buffy;
};

enum {
	SB_SRC_NONE = 0,
	SB_SRC_VIRT,
	SB_SRC_INCOMING
};
static int sidebar_source = SB_SRC_NONE;

static BUFFY *
get_incoming (void)
{
	switch (sidebar_source) {
	case SB_SRC_NONE:
		sidebar_source = SB_SRC_INCOMING;

#ifdef USE_NOTMUCH
		if (option (OPTVIRTSPOOLFILE) && VirtIncoming) {
			sidebar_source = SB_SRC_VIRT;
			return VirtIncoming;
		}
		break;
	case SB_SRC_VIRT:
		if (VirtIncoming) {
			return VirtIncoming;
		}
		break;
#endif
	case SB_SRC_INCOMING:
		break;
	}

	return Incoming;	/* default */
}

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
	BUFFY *b = HilBuffy;
	if (!b)
		return NULL;

	do {
		b = b->next;
		if (!b && wrap) {
			b = get_incoming();
		}
		if (!b || (b == HilBuffy)) {
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
	BUFFY *b = HilBuffy;
	if (!b)
		return NULL;

	do {
		b = b->prev;
		if (!b && wrap) {
			b = Outgoing;
		}
		if (!b || (b == HilBuffy)) {
			break;
		}
		if (b->msg_unread || b->msg_flagged || mutt_find_list (SidebarWhitelist, b->path)) {
			return b;
		}
	} while (b);

	return NULL;
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

	BUFFY *b = sbe->buffy;
	if (!b)
		return src;

#if 1
	int c = Context && (mutt_strcmp (Context->path, b->path) == 0);
#endif

	optional = flags & M_FORMAT_OPTIONAL;

	switch (op) {
		case 'B':
			mutt_format_s (dest, destlen, prefix, sbe->box);
			break;

		case 'd':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, c ? Context->deleted : 0);
			} else if ((c && Context->deleted == 0) || !c) {
				optional = 0;
			}
			break;

		case 'F':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, b->msg_flagged);
			} else if (b->msg_flagged == 0) {
				optional = 0;
			}
			break;

		case 'L':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, c ? Context->vcount : b->msg_count);
			} else if ((c && Context->vcount == b->msg_count) || !c) {
				optional = 0;
			}
			break;

		case 'N':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, b->msg_unread);
			} else if (b->msg_unread == 0) {
				optional = 0;
			}
			break;

		case 'S':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, b->msg_count);
			} else if (b->msg_count == 0) {
				optional = 0;
			}
			break;

		case 't':
			if (!optional) {
				snprintf (fmt, sizeof (fmt), "%%%sd", prefix);
				snprintf (dest, destlen, fmt, c ? Context->tagged : 0);
			} else if ((c && Context->tagged == 0) || !c) {
				optional = 0;
			}
			break;

		case '!':
			if (b->msg_flagged == 0) {
				mutt_format_s (dest, destlen, prefix, "");
			} else if (b->msg_flagged == 1) {
				mutt_format_s (dest, destlen, prefix, "!");
			} else if (b->msg_flagged == 2) {
				mutt_format_s (dest, destlen, prefix, "!!");
			} else {
				snprintf (buf, sizeof (buf), "%d!", b->msg_flagged);
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
	BUFFY *b)
{
	struct sidebar_entry sbe;

	if (!buf || !box || !b)
		return;

	sbe.buffy = b;
	strncpy (sbe.box, box, sizeof (sbe.box) - 1);

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

	switch ((SidebarSortMethod & SORT_MASK)) {
		case SORT_COUNT:
			result = (b2->msg_count - b1->msg_count);
			break;
		case SORT_COUNT_NEW:
			result = (b2->msg_unread - b1->msg_unread);
			break;
		case SORT_DESC:
			result = mutt_strcmp (b1->desc, b2->desc);
			break;
		case SORT_FLAGGED:
			result = (b2->msg_flagged - b1->msg_flagged);
			break;
		case SORT_PATH:
			result = mutt_strcmp (b1->path, b2->path);
			break;
	}

	if (SidebarSortMethod & SORT_REVERSE)
		result = -result;

	return result;
}

/**
 * buffy_going - Prevent our pointers becoming invalid
 * @b: BUFFY about to be deleted
 *
 * If we receive a delete-notification for a BUFFY, we need to change any
 * pointers we have to reference a different BUFFY, or set them to NULL.
 *
 * We don't update the prev/next pointers, they'll be fixed on the next
 * call to prepare_sidebar().
 *
 * Returns:
 *	A valid alternative BUFFY, or NULL
 */
static BUFFY *
buffy_going (const BUFFY *b)
{
	if (!b)
		return NULL;

	if (b->prev) {
		b->prev->next = NULL;
	}

	if (b->next)
		b->next->prev = NULL;
		return b->next;

	return b->prev;
}

/**
 * update_buffy_visibility - Should a BUFFY be displayed in the sidebar
 * @arr:     array of BUFFYs
 * @arr_len: number of BUFFYs in array
 *
 * For each BUFFY in the array, check whether we should display it.
 * This is determined by several criteria.  If the BUFFY:
 *	is the currently open mailbox
 *	is the currently highlighted mailbox
 *	has unread messages
 *	has flagged messages
 *	is whitelisted
 */
static void
update_buffy_visibility (BUFFY **arr, int arr_len)
{
	if (!arr)
		return;

	short new_only = option (OPTSIDEBARNEWMAILONLY);

	BUFFY *b;
	int i;
	for (i = 0; i < arr_len; i++) {
		b = arr[i];

		b->is_hidden = 0;

		if (!new_only)
			continue;

		if ((b == OpnBuffy) || (b->msg_unread  > 0) ||
		    (b == HilBuffy) || (b->msg_flagged > 0)) {
			continue;
		}

		if (Context && (strcmp (b->path, Context->path) == 0)) {
			/* Spool directory */
			continue;
		}

		if (mutt_find_list (SidebarWhitelist, b->path)) {
			/* Explicitly asked to be visible */
			continue;
		}

		b->is_hidden = 1;
	}
}

/**
 * sort_buffy_array - Sort an array of BUFFY pointers
 * @arr:     array of BUFFYs
 * @arr_len: number of BUFFYs in array
 *
 * Sort an array of BUFFY pointers according to the current sort config
 * option "sidebar_sort_method". This calls qsort to do the work which calls our
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
 * prepare_sidebar - Prepare the list of BUFFYs for the sidebar display
 * @page_size:  The number of lines on a page
 *
 * Before painting the sidebar, we count the BUFFYs, determine which are
 * visible, sort them and set up our page pointers.
 *
 * This is a lot of work to do each refresh, but there are many things that
 * can change outside of the sidebar that we don't hear about.
 *
 * Returns:
 *	0:  No, don't draw the sidebar
 *	1: Yes, draw the sidebar
 */
static int
prepare_sidebar (int page_size)
{
	BUFFY *b = get_incoming();
	if (!b)
		return 0;

	int count = 0;
	for (; b; b = b->next)
		count++;

	if (count == 0)
		return 0;

	BUFFY **arr = safe_malloc (count * sizeof (*arr));
	if (!arr)
		return 0;

	int i = 0;
	for (b = get_incoming(); b; b = b->next, i++) {
		arr[i] = b;
	}

	update_buffy_visibility (arr, count);
	sort_buffy_array        (arr, count);

	if (sidebar_source == SB_SRC_INCOMING)
		Incoming = arr[0];

	int top_index =  0;
	int opn_index = -1;
	int hil_index = -1;
	int bot_index = -1;

	for (i = 0; i < count; i++) {
		if (TopBuffy == arr[i])
			top_index = i;
		if (OpnBuffy == arr[i])
			opn_index = i;
		if (HilBuffy == arr[i])
			hil_index = i;
		if (BotBuffy == arr[i])
			bot_index = i;
	}

	if (!HilBuffy || (SidebarSortMethod != PreviousSort)) {
		if (OpnBuffy) {
			HilBuffy  = OpnBuffy;
			hil_index = opn_index;
		} else {
			HilBuffy  = arr[0];
			hil_index = 0;
		}
	}
	if (TopBuffy) {
		top_index = (hil_index / page_size) * page_size;
	} else {
		top_index = hil_index;
	}
	TopBuffy = arr[top_index];

	bot_index = top_index + page_size - 1;
	if (bot_index > (count - 1)) {
		bot_index = count - 1;
	}
	BotBuffy  = arr[bot_index];

	Outgoing = arr[count - 1];

	PreviousSort = SidebarSortMethod;
	free (arr);
	return 1;
}

/**
 * visible - Should we display the sidebar?
 *
 * After validating the config options "sidebar_visible" and "sidebar_width",
 * determine whether we should should display the sidebar.
 *
 * When not visible, set the global SidebarSortMethod to 0.
 *
 * Returns:
 *	Boolean
 */
static short
visible (void)
{
	short new_visible = option (OPTSIDEBAR);
	short new_width   = SidebarWidth;

	if (OldWidth != new_width) {
		if (new_width > 0) {
			OldWidth = new_width;
		}
	}

	if (OldVisible != new_visible) {
		if (new_visible) {
			set_option (OPTSIDEBAR);
		} else {
			unset_option (OPTSIDEBAR);
		}
		OldVisible = new_visible;
	} else if (new_width == 0) {
		unset_option (OPTSIDEBAR);
		OldVisible = 0;
	}

	if (!option (OPTSIDEBAR)) {
		SidebarWidth = 0;
	} else if (new_width == 0) {
		SidebarWidth = OldWidth;
	} else {
		SidebarWidth = new_width;
	}

	return new_visible;
}

/**
 * draw_divider - Draw a line between the sidebar and the rest of mutt
 * @first_row:  Screen line to start (0-based)
 * @num_rows:   Number of rows to fill
 *
 * Draw a divider using a character from the config option "sidebar_divider_char".
 * This can be an ASCII or Unicode character.  First we calculate this
 * character's width in screen columns, then subtract that from the config
 * option "sidebar_width".
 *
 * Returns:
 *	-1: Error: bad character, etc
 *	0:  Error: 0 width character
 *	n:  Success: character occupies n screen columns
 */
static int
draw_divider (int first_row, int num_rows)
{
	/* Calculate the width of the delimiter in screen cells */
	wchar_t sd[4];
	mbstowcs (sd, NONULL(SidebarDividerChar), 4);
	/* We only consider the first character */
	int delim_len = wcwidth (sd[0]);

	if (delim_len < 1)
		return delim_len;

	if ((SidebarWidth + delim_len) > (COLS + 1))
		return 0;

	if (delim_len > SidebarWidth)
		return -1;

	SETCOLOR(MT_COLOR_DIVIDER);

	short color_pair;
#ifndef USE_SLANG_CURSES
	attr_t attrs;
	attr_get (&attrs, &color_pair, 0);
#else
	color_pair = attr_get();
#endif
	int i;
	for (i = 0; i < num_rows; i++) {
		move (first_row + i, SidebarWidth - delim_len);
		addstr (NONULL(SidebarDividerChar));
#ifndef USE_SLANG_CURSES
		mvchgat (first_row + i, SidebarWidth - delim_len, delim_len, 0, color_pair, NULL);
#endif
	}

	return delim_len;
}

/**
 * fill_empty_space - Wipe the remaining sidebar space
 * @first_row:  Screen line to start (0-based)
 * @num_rows:   Number of rows to fill
 * @width:      Width of the sidebar (minus the divider)
 *
 * Write spaces over the area the sidebar isn't using.
 */
static void
fill_empty_space (int first_row, int num_rows, int width)
{
	/* Fill the remaining rows with blank space */
	SETCOLOR(MT_COLOR_NORMAL);

	int r;
	for (r = 0; r < num_rows; r++) {
		int i = 0;
		move (first_row + r, 0);
		for (; i < width; i++)
			addch (' ');
	}
}

/**
 * draw_sidebar - Write out a list of mailboxes, on the left
 * @first_row:  Screen line to start (0-based)
 * @num_rows:   Number of rows to fill
 * @div_width:  Width in screen characters taken by the divider
 *
 * Display a list of mailboxes in a panel on the left.  What's displayed will
 * depend on our index markers: TopBuffy, OpnBuffy, HilBuffy, BotBuffy.
 * On the first run they'll be NULL, so we display the top of Mutt's list
 * (Incoming).
 *
 * TopBuffy - first visible mailbox
 * BotBuffy - last  visible mailbox
 * OpnBuffy - mailbox shown in Mutt's Index Panel
 * HilBuffy - Unselected mailbox (the paging follows this)
 *
 * The entries are formatted using "sidebar_format" and may be abbreviated:
 * "sidebar_short_path", indented: "sidebar_folder_indent",
 * "sidebar_indent_string" and sorted: "sidebar_sort_method".  Finally, they're
 * trimmed to fit the available space.
 */
static void
draw_sidebar (int first_row, int num_rows, int div_width)
{
	BUFFY *b = TopBuffy;
	if (!b)
		return;

	int w = MIN(COLS, (SidebarWidth - div_width));
	int row = 0;
	for (b = TopBuffy; b && (row < num_rows); b = b->next) {
		if (b->is_hidden) {
			continue;
		}

		if (b == OpnBuffy) {
			if ((ColorDefs[MT_COLOR_SB_INDICATOR] != 0)) {
				SETCOLOR(MT_COLOR_SB_INDICATOR);
			} else {
				SETCOLOR(MT_COLOR_INDICATOR);
			}
		} else if (b == HilBuffy) {
			SETCOLOR(MT_COLOR_HIGHLIGHT);
		} else if ((ColorDefs[MT_COLOR_SB_SPOOLFILE] != 0) &&
			(mutt_strcmp (b->path, Spoolfile) == 0)) {
			SETCOLOR(MT_COLOR_SB_SPOOLFILE);
		} else if (b->msg_unread > 0) {
			SETCOLOR(MT_COLOR_NEW);
		} else if (b->msg_flagged > 0) {
			SETCOLOR(MT_COLOR_FLAGGED);
		} else {
			SETCOLOR(MT_COLOR_NORMAL);
		}

		move (first_row + row, 0);
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
				sidebar_folder_name = malloc (strlen (tmp_folder_name) + sidebar_folder_depth*strlen (NONULL(SidebarIndentString)) + 1);
				sidebar_folder_name[0]=0;
				for (i=0; i < sidebar_folder_depth; i++)
					strncat (sidebar_folder_name, NONULL(SidebarIndentString), strlen (NONULL(SidebarIndentString)));
				strncat (sidebar_folder_name, tmp_folder_name, strlen (tmp_folder_name));
			}
		}
#ifdef USE_NOTMUCH
		else if (b->magic == M_NOTMUCH) {
			sidebar_folder_name = b->desc;
		}
#endif
		char str[SHORT_STRING];
		make_sidebar_entry (str, sizeof (str), w, sidebar_folder_name, b);
		printw ("%s", str);
		if (sidebar_folder_depth > 0)
			free (sidebar_folder_name);
		row++;
	}

	fill_empty_space (first_row + row, num_rows - row, w);
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
	if (!visible())
		return;

	/* XXX - if transitioning from invisible to visible */
	/* if (OldVisible == 0) */
	/* 	mutt_buffy_check (1); we probably have bad or no numbers */

	int first_row = 0;
	int num_rows  = LINES - 2;

	if (option (OPTHELP) || option (OPTSTATUSONTOP))
		first_row++;

	if (option (OPTHELP))
		num_rows--;

	int div_width = draw_divider (first_row, num_rows);
	if (div_width < 0)
		return;

	if (!get_incoming()) {
		int w = MIN(COLS, (SidebarWidth - div_width));
		fill_empty_space (first_row, num_rows, w);
		return;
	}

	if (!prepare_sidebar (num_rows))
		return;

	draw_sidebar (first_row, num_rows, div_width);
}

/**
 * sb_should_refresh - Check if the sidebar is due to be refreshed
 *
 * The "sidebar_refresh_time" config option allows the user to limit the frequency
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

	if (SidebarRefreshTime == 0)
		return 0;

	time_t diff = (time (NULL) - LastRefresh);

	return (diff >= SidebarRefreshTime);
}

/**
 * sb_change_mailbox - Change the selected mailbox
 * @op: Operation code
 *
 * Change the selected mailbox, e.g. "Next mailbox", "Previous Mailbox
 * with new mail". The operations are listed OPS.SIDEBAR which is built
 * into an enum in keymap_defs.h.
 *
 * If the operation is successful, HilBuffy will be set to the new mailbox.
 * This function only *selects* the mailbox, doesn't *open* it.
 *
 * Allowed values are: OP_SIDEBAR_NEXT, OP_SIDEBAR_NEXT_NEW,
 * OP_SIDEBAR_PAGE_DOWN, OP_SIDEBAR_PAGE_UP, OP_SIDEBAR_PREV,
 * OP_SIDEBAR_PREV_NEW.
 */
void
sb_change_mailbox (int op)
{
	if (!option (OPTSIDEBAR))
		return;

	BUFFY *b;
	if (!HilBuffy)	/* It'll get reset on the next draw */
		return;

	switch (op) {
		case OP_SIDEBAR_NEXT:
			if (!HilBuffy->next)
				return;
			if (HilBuffy->next->is_hidden)
				return;
			HilBuffy = HilBuffy->next;
			break;
		case OP_SIDEBAR_NEXT_NEW:
			b = find_next_new (option (OPTSIDEBARNEXTNEWWRAP));
			if (!b) {
				return;
			} else {
				HilBuffy = b;
			}
			break;
		case OP_SIDEBAR_PAGE_DOWN:
			HilBuffy = BotBuffy;
			if (HilBuffy->next) {
				HilBuffy = HilBuffy->next;
			}
			break;
		case OP_SIDEBAR_PAGE_UP:
			HilBuffy = TopBuffy;
			if (HilBuffy != get_incoming()) {
				HilBuffy = HilBuffy->prev;
			}
			break;
		case OP_SIDEBAR_PREV:
			if (!HilBuffy->prev)
				return;
			if (HilBuffy->prev->is_hidden)	/* Can't happen, we've sorted the hidden to the end */
				return;
			HilBuffy = HilBuffy->prev;
			break;
		case OP_SIDEBAR_PREV_NEW:
			b = find_prev_new (option (OPTSIDEBARNEXTNEWWRAP));
			if (!b) {
				return;
			} else {
				HilBuffy = b;
			}
			break;
		default:
			return;
	}
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
	BUFFY *b = get_incoming();
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
 * sb_get_highlight - Get the BUFFY that's highlighted in the sidebar
 *
 * Get the path of the mailbox that's highlighted in the sidebar.
 *
 * Returns:
 *	Mailbox path
 */
const char *
sb_get_highlight (void)
{
	if (!option (OPTSIDEBAR))
		return NULL;

	if (!HilBuffy)
		return NULL;

	return HilBuffy->path;
}

/**
 * sb_set_open_buffy - Set the OpnBuffy based on a mailbox path
 * @path: Mailbox path
 *
 * Search through the list of mailboxes.  If a BUFFY has a matching path, set
 * OpnBuffy to it.
 */
BUFFY *
sb_set_open_buffy (const char *path)
{
	/* Even if the sidebar is hidden */

	BUFFY *b = get_incoming();

	if (!path || !b)
		return NULL;

	OpnBuffy = NULL;

	for (; b; b = b->next) {
		if (!strcmp (b->path,     path) ||
		    !strcmp (b->realpath, path)) {
			OpnBuffy = b;
			HilBuffy = b;
			break;
		}
	}

	return OpnBuffy;
}

/**
 * sb_set_update_time - Note the time that the sidebar was updated
 *
 * Update the timestamp representing the last sidebar update.  If the user
 * configures "sidebar_refresh_time", this will help to reduce traffic.
 */
void
sb_set_update_time (void)
{
	/* XXX - should this be public? */

	LastRefresh = time (NULL);
}

/**
 * sb_notify_mailbox - The state of a BUFFY is about to change
 *
 * We receive a notification:
 *	After a new BUFFY has been created
 *	Before a BUFFY is deleted
 *
 * Before a deletion, check that our pointers won't be invalidated.
 */
void
sb_notify_mailbox (BUFFY *b, int created)
{
	if (!b)
		return;

	/* Any new/deleted mailboxes will cause a refresh.  As long as
	 * they're valid, our pointers will be updated in prepare_sidebar() */

	if (created) {
		if (!TopBuffy)
			TopBuffy = b;
		if (!HilBuffy)
			HilBuffy = b;
		if (!BotBuffy)
			BotBuffy = b;
		if (!Outgoing)
			Outgoing = b;
	} else {
		if (TopBuffy == b)
			TopBuffy = buffy_going (TopBuffy);
		if (OpnBuffy == b)
			OpnBuffy = buffy_going (OpnBuffy);
		if (HilBuffy == b)
			HilBuffy = buffy_going (HilBuffy);
		if (BotBuffy == b)
			BotBuffy = buffy_going (BotBuffy);
		if (Outgoing == b)
			Outgoing = buffy_going (Outgoing);
	}
}

/**
 * sb_toggle_virtual - Switch between regular and virtual folders
 */
void
sb_toggle_virtual (void)
{
	if (sidebar_source == -1)
		get_incoming();

#ifdef USE_NOTMUCH
	if ((sidebar_source == SB_SRC_INCOMING) && VirtIncoming)
		sidebar_source = SB_SRC_VIRT;
	else
#endif
		sidebar_source = SB_SRC_INCOMING;

	TopBuffy = NULL;
	OpnBuffy = NULL;
	HilBuffy = NULL;
	BotBuffy = NULL;
	Outgoing = NULL;

	sb_draw();
}

