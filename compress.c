/*
 * Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
 * Copyright (C) 2016 Richard Russon <rich@flatcap.org>
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

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "mutt.h"
#include "mailbox.h"
#include "mutt_curses.h"
#include "mx.h"

typedef struct
{
	const char *append;   /* append-hook command */
	const char *close;    /* close-hook  command */
	const char *open;     /* open-hook   command */
	off_t size;           /* size of real folder */
} COMPRESS_INFO;

char echo_cmd[HUGE_STRING];

/* parameters:
 * ctx - context to lock
 * excl - exclusive lock?
 * retry - should retry if unable to lock?
 */
static int
lock_mailbox (CONTEXT *ctx, FILE *fp, int excl, int retry)
{
	int r;

	if ((r = mx_lock_file (ctx->realpath, fileno (fp), excl, 1, retry)) == 0)
		ctx->locked = 1;
	else if (retry && !excl)
	{
		ctx->readonly = 1;
		return 0;
	}

	return (r);
}

static void
restore_path (CONTEXT *ctx)
{
	FREE(&ctx->path);
	ctx->path = ctx->realpath;
}

/* remove the temporary mailbox */
static void
remove_file (CONTEXT *ctx)
{
	if (ctx->magic == M_MBOX || ctx->magic == M_MMDF)
		remove (ctx->path);
}

static void
unlock_mailbox (CONTEXT *ctx, FILE *fp)
{
	if (ctx->locked)
	{
		fflush (fp);

		mx_unlock_file (ctx->realpath, fileno (fp), 1);
		ctx->locked = 0;
	}
}

static int
file_exists (const char *path)
{
	return (access (path, W_OK) != 0 && errno == ENOENT) ? 1 : 0;
}

static const char *
find_hook (int type, const char *path)
{
	const char *c = mutt_find_hook (type, path);
	return (!c || !*c) ? NULL : c;
}

/* if the file is new, we really do not append, but create, and so use
 * close-hook, and not append-hook
 */
static const char *
get_append_command (const char *path, const CONTEXT *ctx)
{
	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;
	return (file_exists (path)) ? ci->append : ci->close;
}

/* open a compressed mailbox */
static COMPRESS_INFO *
set_compress_info (CONTEXT *ctx)
{
	COMPRESS_INFO *ci;

	/* Now lets uncompress this thing */
	ci = safe_malloc (sizeof (COMPRESS_INFO));
	ctx->compress_info = (void*) ci;
	ci->append = find_hook (M_APPENDHOOK, ctx->path);
	ci->open   = find_hook (M_OPENHOOK, ctx->path);
	ci->close  = find_hook (M_CLOSEHOOK, ctx->path);
	return ci;
}

static void
setup_paths (CONTEXT *ctx)
{
	char tmppath[_POSIX_PATH_MAX];

	/* Setup the right paths */
	ctx->realpath = ctx->path;

	/* Uncompress to /tmp */
	mutt_mktemp (tmppath, sizeof(tmppath));
	ctx->path = safe_strdup (tmppath);
}

static int
get_size (const char *path)
{
	struct stat sb;
	if (stat (path, &sb) != 0)
		return 0;
	return (sb.st_size);
}

static void
store_size (CONTEXT *ctx)
{
	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;
	ci->size = get_size (ctx->realpath);
}

static const char *
cb_format_str (char *dest, size_t destlen, size_t col, char op, const char *src,
	const char *fmt, const char *ifstring, const char *elsestring,
	unsigned long data, format_flag flags)
{
	char tmp[SHORT_STRING];

	CONTEXT *ctx = (CONTEXT *) data;
	switch (op)
	{
		case 'f':
			snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
			snprintf (dest, destlen, tmp, ctx->realpath);
			break;
		case 't':
			snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
			snprintf (dest, destlen, tmp, ctx->path);
			break;
	}
	return (src);
}

static char *
get_compression_cmd (const char *cmd, const CONTEXT *ctx)
{
	char expanded[_POSIX_PATH_MAX];

	mutt_FormatString (expanded, sizeof (expanded), 0, cmd, cb_format_str, (unsigned long) ctx, 0);
	return safe_strdup (expanded);
}


int
comp_can_read (const char *path)
{
	return find_hook (M_OPENHOOK, path) ? 1 : 0;
}

int
comp_can_append (const char *path)
{
	int magic;

	if (!file_exists (path))
	{
		char *dir_path = safe_strdup(path);
		char *aux = strrchr(dir_path, '/');
		int dir_valid = 1;
		if (aux)
		{
			*aux='\0';
			if (access(dir_path, W_OK|X_OK))
				dir_valid = 0;
		}
		FREE(&dir_path);
		return dir_valid && (find_hook (M_CLOSEHOOK, path) ? 1 : 0);
	}

	magic = mx_get_magic (path);

	if (magic != 0 && magic != M_COMPRESSED)
		return 0;

	return (find_hook (M_APPENDHOOK, path)
			|| (find_hook (M_OPENHOOK, path)
			&& find_hook (M_CLOSEHOOK, path))) ? 1 : 0;
}

/* check that the command has both %f and %t
 * 0 means OK, -1 means error */
int
comp_valid_command (const char *cmd)
{
	return (strstr (cmd, "%f") && strstr (cmd, "%t")) ? 0 : -1;
}

int comp_check_mailbox (CONTEXT *ctx)
{
	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;
	if (ci->size != get_size (ctx->realpath))
	{
		FREE(&ctx->compress_info);
		FREE(&ctx->realpath);
		mutt_error _("Mailbox was corrupted!");
		return (-1);
	}
	return (0);
}

int
comp_open_read (CONTEXT *ctx)
{
	char *cmd;
	FILE *fp;
	int rc;

	COMPRESS_INFO *ci = set_compress_info (ctx);
	if (!ci->open) {
		ctx->magic = 0;
		FREE(&ctx->compress_info);
		return (-1);
	}
	if (!ci->close || access (ctx->path, W_OK) != 0)
		ctx->readonly = 1;

	setup_paths (ctx);
	store_size (ctx);

	if (!ctx->quiet)
		mutt_message (_("Decompressing %s..."), ctx->realpath);

	cmd = get_compression_cmd (ci->open, ctx);
	if (cmd == NULL)
		return (-1);
	dprint (2, (debugfile, "DecompressCmd: '%s'\n", cmd));

	if ((fp = fopen (ctx->realpath, "r")) == NULL)
	{
		mutt_perror (ctx->realpath);
		FREE(&cmd);
		return (-1);
	}
	mutt_block_signals();
	if (lock_mailbox (ctx, fp, 0, 1) == -1)
	{
		fclose (fp);
		mutt_unblock_signals();
		mutt_error _("Unable to lock mailbox!");
		FREE(&cmd);
		return (-1);
	}

	endwin();
	fflush (stdout);
	sprintf(echo_cmd,_("echo Decompressing %s..."),ctx->realpath);
	mutt_system(echo_cmd);
	rc = mutt_system (cmd);
	unlock_mailbox (ctx, fp);
	mutt_unblock_signals();
	fclose (fp);

	if (rc)
	{
		mutt_any_key_to_continue (NULL);
		ctx->magic = 0;
		FREE(&ctx->compress_info);
		mutt_error (_("Error executing: %s : unable to open the mailbox!\n"), cmd);
		// remove the partial uncompressed file
		remove_file (ctx);
		restore_path (ctx);
	}
	FREE(&cmd);
	if (rc)
		return (-1);

	if (comp_check_mailbox (ctx))
		return (-1);

	ctx->magic = mx_get_magic (ctx->path);

	return (0);
}

int
comp_open_append (CONTEXT *ctx)
{
	FILE *fh;
	COMPRESS_INFO *ci = set_compress_info (ctx);

	if (!get_append_command (ctx->path, ctx))
	{
		if (ci->open && ci->close)
			return (comp_open_read (ctx));

		ctx->magic = 0;
		FREE(&ctx->compress_info);
		return (-1);
	}

	setup_paths (ctx);

	ctx->magic = DefaultMagic;

	if (file_exists (ctx->realpath))
		if (ctx->magic == M_MBOX || ctx->magic == M_MMDF)
			if ((fh = safe_fopen (ctx->path, "w")))
				fclose (fh);
	/* No error checking - the parent function will catch it */

	return (0);
}

/* return 0 on success, -1 on failure */
int
comp_sync (CONTEXT *ctx)
{
	char *cmd;
	int rc = 0;
	FILE *fp;
	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;

	if (!ctx->quiet)
		mutt_message (_("Compressing %s..."), ctx->realpath);

	cmd = get_compression_cmd (ci->close, ctx);
	if (cmd == NULL)
		return (-1);

	if ((fp = fopen (ctx->realpath, "a")) == NULL)
	{
		mutt_perror (ctx->realpath);
		FREE(&cmd);
		return (-1);
	}
	mutt_block_signals();
	if (lock_mailbox (ctx, fp, 1, 1) == -1)
	{
		fclose (fp);
		mutt_unblock_signals();
		mutt_error _("Unable to lock mailbox!");
		store_size (ctx);
		FREE(&cmd);
		return (-1);
	}

	dprint (2, (debugfile, "CompressCommand: '%s'\n", cmd));

	endwin();
	fflush (stdout);
	sprintf(echo_cmd,_("echo Compressing %s..."), ctx->realpath);
	mutt_system(echo_cmd);
	if (mutt_system (cmd))
	{
		mutt_any_key_to_continue (NULL);
		mutt_error (_("%s: Error compressing mailbox! Original mailbox deleted, uncompressed one kept!\n"), ctx->path);
		rc = -1;
	}

	unlock_mailbox (ctx, fp);
	mutt_unblock_signals();
	fclose (fp);

	FREE(&cmd);

	store_size (ctx);

	return (rc);
}

/* close a compressed mailbox */
void
comp_fast_close (CONTEXT *ctx)
{
	dprint (2, (debugfile, "comp_fast_close called on '%s'\n",
							ctx->path));

	if (ctx->compress_info)
	{
		if (ctx->fp)
			fclose (ctx->fp);
		ctx->fp = NULL;
		/* if the folder was removed, remove the gzipped folder too */
		if ((ctx->magic > 0)
				&& (access (ctx->path, F_OK) != 0)
				&& ! option (OPTSAVEEMPTY))
			remove (ctx->realpath);
		else
			remove_file (ctx);

		restore_path (ctx);
		FREE(&ctx->compress_info);
	}
}

int
comp_slow_close (CONTEXT *ctx)
{
	FILE *fp;
	const char *append;
	char *cmd;
	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;

	dprint (2, (debugfile, "comp_slow_close called on '%s'\n", ctx->path));

	if (! (ctx->append
				 && ((append = get_append_command (ctx->realpath, ctx))
						 || (append = ci->close))))
	{
		/* if we can not or should not append, we only have to remove the
		 * compressed info, because sync was already called */
		comp_fast_close (ctx);
		return (0);
	}

	if (ctx->fp)
		fclose (ctx->fp);
	ctx->fp = NULL;

	if (!ctx->quiet)
	{
		if (append == ci->close)
			mutt_message (_("Compressing %s..."), ctx->realpath);
		else
			mutt_message (_("Compressed-appending to %s..."), ctx->realpath);
	}

	cmd = get_compression_cmd (append, ctx);
	if (cmd == NULL)
		return (-1);

	if ((fp = fopen (ctx->realpath, "a")) == NULL)
	{
		mutt_perror (ctx->realpath);
		FREE(&cmd);
		return (-1);
	}
	mutt_block_signals();
	if (lock_mailbox (ctx, fp, 1, 1) == -1)
	{
		fclose (fp);
		mutt_unblock_signals();
		mutt_error _("Unable to lock mailbox!");
		FREE(&cmd);
		return (-1);
	}

	dprint (2, (debugfile, "CompressCmd: '%s'\n", cmd));

	endwin();
	fflush (stdout);

	if (append == ci->close)
		sprintf(echo_cmd,_("echo Compressing %s..."), ctx->realpath);
	else
		sprintf(echo_cmd,_("echo Compressed-appending to %s..."), ctx->realpath);
	mutt_system(echo_cmd);

	if (mutt_system (cmd))
	{
		mutt_any_key_to_continue (NULL);
		mutt_error (_(" %s: Error compressing mailbox!  Uncompressed one kept!\n"),
			ctx->path);
		FREE(&cmd);
		unlock_mailbox (ctx, fp);
		mutt_unblock_signals();
		fclose (fp);
		return (-1);
	}

	unlock_mailbox (ctx, fp);
	mutt_unblock_signals();
	fclose (fp);
	remove_file (ctx);
	restore_path (ctx);
	FREE(&cmd);
	FREE(&ctx->compress_info);

	return (0);
}

