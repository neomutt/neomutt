/* Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
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
 *     Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
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

/* Notes:
 * Any references to compressed files also apply to encrypted files.
 * ctx->path     == plaintext file
 * ctx->realpath == compressed file
 */

/**
 * struct COMPRESS_INFO - Private data for compress
 *
 * This object gets attached to the mailbox's CONTEXT.
 */
typedef struct
{
	const char *append;   /* append-hook command */
	const char *close;    /* close-hook  command */
	const char *open;     /* open-hook   command */
	off_t size;           /* size of the compressed file */
} COMPRESS_INFO;

char echo_cmd[HUGE_STRING];

/**
 * lock_mailbox - Try to lock a mailbox (exclusively)
 * @ctx:  Mailbox to lock
 * @fp:   File pointer to the mailbox file
 * @excl: Lock exclusively?
 *
 * Try to (exclusively) lock the mailbox.  If we succeed, then we mark the
 * mailbox as locked.  If we fail, but we didn't want exclusive rights, then
 * the mailbox will be marked readonly.
 *
 * Returns:
 *	 0: Success (locked or readonly)
 *	-1: Error (can't lock the file)
 */
static int
lock_mailbox (CONTEXT *ctx, FILE *fp, int excl)
{
	if (!ctx || !fp)
		return -1;

	int r = mx_lock_file (ctx->realpath, fileno (fp), excl, 1, 1);

	if (r == 0) {
		ctx->locked = 1;
	} else if (excl == 0) {
		ctx->readonly = 1;
		return 0;
	}

	return r;
}

/**
 * restore_path - Put back the original mailbox name
 * @ctx: Mailbox to modify
 *
 * When we use a compressed mailbox, we change the CONTEXT to refer to the
 * uncompressed file.  We store the original name in ctx->realpath.
 *	ctx->path     = "/tmp/mailbox"
 *	ctx->realpath = "mailbox.gz"
 *
 * When we have finished with a compressed mailbox, we put back the original
 * name.
 *	ctx->path     = "mailbox.gz"
 *	ctx->realpath = NULL
 */
static void
restore_path (CONTEXT *ctx)
{
	if (!ctx)
		return;

	FREE(&ctx->path);
	ctx->path = ctx->realpath;
	ctx->realpath = NULL;
}

/**
 * remove_file - Delete the plaintext file
 * @ctx: Mailbox
 *
 * Delete the uncompressed file of a mailbox.
 * This only works for mbox or mmdf mailbox files.
 */
static void
remove_file (const CONTEXT *ctx)
{
	if (!ctx)
		return;

	if ((ctx->magic == M_MBOX) || (ctx->magic == M_MMDF)) {
		remove (ctx->path);
	}
}

/**
 * unlock_mailbox - Unlock a mailbox
 * @ctx: Mailbox to unlock
 * @fp:  File pointer to mailbox file
 *
 * Unlock a mailbox previously locked by lock_mailbox().
 */
static void
unlock_mailbox (CONTEXT *ctx, FILE *fp)
{
	if (!ctx || !fp)
		return;

	if (ctx->locked) {
		fflush (fp);

		mx_unlock_file (ctx->realpath, fileno (fp), 1);
		ctx->locked = 0;
	}
}

/**
 * file_exists - Does the file exist?
 * @path: Pathname to check
 *
 * Returns:
 *	1: File exists
 *	0: Non-existant file
 */
static int
file_exists (const char *path)
{
	if (!path)
		return 0;

	return (access (path, W_OK) != 0 && errno == ENOENT) ? 1 : 0;
}

/**
 * find_hook - Find a hook to match a path
 * @type: Type of hook, e.g. M_CLOSEHOOK
 * @path: Filename to test
 *
 * Each hook has a type and a pattern.  Find a command that matches the type
 * and path supplied. e.g.
 *
 * User config:
 *	open-hook '\.gz$' "gzip -cd '%f' > '%t'"
 *
 * Call:
 *	find_hook (M_OPENHOOK, "myfile.gz");
 *
 * Returns:
 *	string: Matching hook command
 *	NULL:   No matches
 */
static const char *
find_hook (int type, const char *path)
{
	if (!path)
		return NULL;

	const char *c = mutt_find_hook (type, path);
	return (!c || !*c) ? NULL : c;
}

/**
 * get_append_command - Get the command for appending to a file
 * @ctx:  Mailbox to append to
 * @path: Compressed file
 *
 * If the file exists, we can use the 'append-hook' command.
 * Otherwise, use the 'close-hook' command.
 *
 * Returns:
 *	string: Append command or Close command
 *	NULL:   On error
 */
static const char *
get_append_command (const CONTEXT *ctx, const char *path)
{
	if (!path || !ctx)
		return NULL;

	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;

	return (file_exists (path)) ? ci->append : ci->close;
}

/**
 * set_compress_info - Find the compress hooks for a mailbox
 * @ctx: Mailbox to examine
 *
 * When a mailbox is opened, we check if there are any matching hooks.
 *
 * Note: Caller must free the COMPRESS_INFO when done.
 *
 * Returns:
 *	COMPRESS_INFO: Hook info for the mailbox's path
 *	NULL:          On error
 */
static COMPRESS_INFO *
set_compress_info (CONTEXT *ctx)
{
	if (!ctx)
		return NULL;

	COMPRESS_INFO *ci;

	/* Now lets uncompress this thing */
	ci = safe_malloc (sizeof (COMPRESS_INFO));
	ctx->compress_info = (void*) ci;
	ci->append = find_hook (M_APPENDHOOK, ctx->path);
	ci->open   = find_hook (M_OPENHOOK, ctx->path);
	ci->close  = find_hook (M_CLOSEHOOK, ctx->path);
	return ci;
}

/**
 * setup_paths - Set the mailbox paths
 * @ctx: Mailbox to modify
 *
 * Save the compressed filename in ctx->realpath.
 * Create a temporary file and put its name in ctx->path.
 *
 * Note: ctx->path will be freed by restore_path()
 */
static void
setup_paths (CONTEXT *ctx)
{
	if (!ctx)
		return;

	char tmppath[_POSIX_PATH_MAX];

	/* Setup the right paths */
	ctx->realpath = ctx->path;

	/* Uncompress to /tmp */
	mutt_mktemp (tmppath, sizeof(tmppath));
	ctx->path = safe_strdup (tmppath);
}

/**
 * get_size - Get the size of a file
 * @path: File to measure
 *
 * Returns:
 *	number: Size in bytes
 *	0: XXX -1 on error?
 */
static int
get_size (const char *path)
{
	if (!path)
		return 0;

	struct stat sb;
	if (stat (path, &sb) != 0)
		return 0;

	return sb.st_size;
}

/**
 * store_size - Save the size of the compressed file
 * @ctx: Mailbox
 *
 * Save the compressed file size in the compress_info struct.
 */
static void
store_size (const CONTEXT *ctx)
{
	if (!ctx)
		return;

	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;
	ci->size = get_size (ctx->realpath);
}

/**
 * cb_format_str - Expand the filenames in the command string
 * @dest:        Buffer in which to save string
 * @destlen:     Buffer length
 * @col:         Starting column, UNUSED
 * @op:          printf-like operator, e.g. 't'
 * @src:         printf-like format string
 * @prefix:      Field formatting string, UNUSED
 * @ifstring:    If condition is met, display this string, UNUSED
 * @elsestring:  Otherwise, display this string, UNUSED
 * @data:        Pointer to our sidebar_entry
 * @flags:       Format flags, UNUSED
 *
 * cb_format_str is a callback function for mutt_FormatString.  It understands
 * two operators. '%f' : 'from' filename, '%t' : 'to' filename.
 *
 * Returns: src (unchanged)
 */
static const char *
cb_format_str (char *dest, size_t destlen, size_t col, char op, const char *src,
	const char *fmt, const char *ifstring, const char *elsestring,
	unsigned long data, format_flag flags)
{
	if (!fmt || !dest || (data == 0))
		return src;

	char tmp[SHORT_STRING];

	CONTEXT *ctx = (CONTEXT *) data;

	switch (op) {
		case 'f':
			/* Compressed file */
			snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
			snprintf (dest, destlen, tmp, ctx->realpath);
			break;
		case 't':
			/* Plaintext, temporary file */
			snprintf (tmp, sizeof (tmp), "%%%ss", fmt);
			snprintf (dest, destlen, tmp, ctx->path);
			break;
	}
	return src;
}

/**
 * get_compression_cmd - Expand placeholders in command string
 * @ctx: Mailbox for paths
 * @cmd: Command string from config file
 *
 * This function takes a hook command and expands the filename placeholders
 * within it.  The function calls mutt_FormatString() to do the replacement
 * which calls our callback function cb_format_str(). e.g.
 *
 * Template command:
 *	gzip -cd '%f' > '%t'
 *
 * Result:
 *	gzip -dc '~/mail/abc.gz' > '/tmp/xyz'
 *
 * Note: Caller must free the returned string.
 *
 * Returns:
 *	string: Expanded command string
 *	NULL:   Error occurred
 */
static char *
get_compression_cmd (const CONTEXT *ctx, const char *cmd)
{
	if (!cmd || !ctx)
		return NULL;

	char expanded[_POSIX_PATH_MAX];

	mutt_FormatString (expanded, sizeof (expanded), 0, cmd, cb_format_str, (unsigned long) ctx, 0);
	return safe_strdup (expanded);
}


/**
 * comp_can_read - Can we read from this file?
 * @path: Pathname of file to be tested
 *
 * Search for an 'open-hook' with a regex that matches the path.
 *
 * A match means it's our responsibility to open the file.
 *
 * Returns:
 *	1: Yes, we can read the file
 *	0: No, we cannot read the file
 */
int
comp_can_read (const char *path)
{
	if (!path)
		return 0;

	return find_hook (M_OPENHOOK, path) ? 1 : 0;
}

/**
 * comp_can_append - Can we append to this path?
 * @path: pathname of file to be tested
 *
 * To append to a file we can either use an 'append-hook' or a combination of
 * 'open-hook' and 'close-hook'.
 *
 * A match means it's our responsibility to append to the file.
 *
 * Returns:
 *	1: Yes, we can append to the file
 *	0: No, appending isn't possible
 */
int
comp_can_append (const char *path)
{
	if (!path)
		return 0;

	int magic;

	if (!file_exists (path)) {
		char *dir_path = safe_strdup(path);
		char *aux = strrchr(dir_path, '/');
		int dir_valid = 1;
		if (aux) {
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

/**
 * comp_valid_command - Is this command string allowed?
 * @cmd:  Command string
 *
 * A valid command string must have both "%f" (from file) and "%t" (to file).
 * We don't check if we can actually run the command.
 *
 * Returns:
 *	1: Valid command
 *	0: "%f" and/or "%t" is missing
 */
int
comp_valid_command (const char *cmd)
{
	if (!cmd)
		return 0;

	return (strstr (cmd, "%f") && strstr (cmd, "%t")) ? 1 : 0;
}

/**
 * comp_check_mailbox - Perform quick sanity check
 * @ctx: Mailbox
 *
 * Compare the stored size (in the CONTEXT) against the size in our
 * COMPRESS_INFO.
 *
 * The return codes are picked to match mx_check_mailbox().
 *
 * Returns:
 *	 0: Mailbox OK
 *	-1: Mailbox bad
 */
int
comp_check_mailbox (CONTEXT *ctx)
{
	if (!ctx)
		return -1;

	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;

	if (get_size (ctx->realpath) != ci->size) {
		FREE(&ctx->compress_info);
		mutt_error _("Mailbox was corrupted!");
		return -1;
	}
	return 0;
}

/**
 * comp_open_read - XXX
 * @ctx: Mailbox to open
 *
 * Returns:
 *	 0: Success
 *	-1: Failure
 */
int
comp_open_read (CONTEXT *ctx)
{
	if (!ctx)
		return 0;

	char *cmd;
	FILE *fp;
	int rc;

	COMPRESS_INFO *ci = set_compress_info (ctx);
	if (!ci->open) {
		ctx->magic = 0;
		FREE(&ctx->compress_info);
		return -1;
	}

	if (!ci->close || access (ctx->path, W_OK) != 0)
		ctx->readonly = 1;

	setup_paths (ctx);
	store_size (ctx);

	if (!ctx->quiet)
		mutt_message (_("Decompressing %s..."), ctx->realpath);

	cmd = get_compression_cmd (ctx, ci->open);
	if (!cmd) {
		return -1;
	}
	dprint (2, (debugfile, "DecompressCmd: '%s'\n", cmd));

	fp = fopen (ctx->realpath, "r");
	if (!fp) {
		mutt_perror (ctx->realpath);
		FREE(&cmd);
		return -1;
	}

	mutt_block_signals();
	if (lock_mailbox (ctx, fp, 0) == -1) {
		fclose (fp);
		mutt_unblock_signals();
		mutt_error _("Unable to lock mailbox!");
		FREE(&cmd);
		return -1;
	}

	endwin();
	fflush (stdout);
	sprintf(echo_cmd,_("echo Decompressing %s..."),ctx->realpath); /* __SPRINTF_CHECKED__ */
	mutt_system(echo_cmd);
	rc = mutt_system (cmd);
	unlock_mailbox (ctx, fp);
	mutt_unblock_signals();
	fclose (fp);

	if (rc != 0) {
		mutt_any_key_to_continue (NULL);
		ctx->magic = 0;
		FREE(&ctx->compress_info);
		mutt_error (_("Error executing: %s : unable to open the mailbox!\n"), cmd);
		/* remove the partial uncompressed file */
		remove_file (ctx);
		restore_path (ctx);
	}
	FREE(&cmd);
	if (rc != 0)
		return -1;

	if (comp_check_mailbox (ctx))
		return -1;

	ctx->magic = mx_get_magic (ctx->path);

	return 0;
}

/**
 * comp_open_append - XXX
 * @ctx: Mailbox to append to
 *
 * Returns:
 *	 0: Success
 *	-1: Failure
 */
int
comp_open_append (CONTEXT *ctx)
{
	if (!ctx)
		return 0;

	FILE *fh;
	COMPRESS_INFO *ci = set_compress_info (ctx);

	if (!get_append_command (ctx, ctx->path)) {
		if (ci->open && ci->close) {
			return (comp_open_read (ctx));
		}

		ctx->magic = 0;
		FREE(&ctx->compress_info);
		return -1;
	}

	setup_paths (ctx);

	ctx->magic = DefaultMagic;

	if (file_exists (ctx->realpath)) {
		if (ctx->magic == M_MBOX || ctx->magic == M_MMDF) {
			if ((fh = safe_fopen (ctx->path, "w"))) {
				fclose (fh);
			}
		}
	}
	/* No error checking - the parent function will catch it */

	return 0;
}

/**
 * comp_sync - XXX
 * @ctx: Mailbox to sync
 *
 * Returns:
 *	 0: Success
 *	-1: Failure
 */
int
comp_sync (CONTEXT *ctx)
{
	if (!ctx)
		return 0;

	char *cmd;
	int rc = 0;
	FILE *fp;
	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;

	if (!ctx->quiet)
		mutt_message (_("Compressing %s..."), ctx->realpath);

	cmd = get_compression_cmd (ctx, ci->close);
	if (!cmd)
		return -1;

	fp = fopen (ctx->realpath, "a");
	if (!fp) {
		mutt_perror (ctx->realpath);
		FREE(&cmd);
		return -1;
	}

	mutt_block_signals();
	if (lock_mailbox (ctx, fp, 1) == -1) {
		fclose (fp);
		mutt_unblock_signals();
		mutt_error _("Unable to lock mailbox!");
		store_size (ctx);
		FREE(&cmd);
		return -1;
	}

	dprint (2, (debugfile, "CompressCommand: '%s'\n", cmd));

	endwin();
	fflush (stdout);
	sprintf(echo_cmd,_("echo Compressing %s..."), ctx->realpath); /* __SPRINTF_CHECKED__ */
	mutt_system(echo_cmd);
	if (mutt_system (cmd) != 0) {
		mutt_any_key_to_continue (NULL);
		mutt_error (_("%s: Error compressing mailbox! Original mailbox deleted, uncompressed one kept!\n"), ctx->path);
		rc = -1;
	}

	unlock_mailbox (ctx, fp);
	mutt_unblock_signals();
	fclose (fp);

	FREE(&cmd);

	store_size (ctx);

	return rc;
}

/**
 * comp_fast_close - XXX
 * @ctx: Mailbox to close
 *
 * close a compressed mailbox
 */
void
comp_fast_close (CONTEXT *ctx)
{
	if (!ctx)
		return;

	dprint (2, (debugfile, "comp_fast_close called on '%s'\n",
		ctx->path));

	if (ctx->compress_info) {
		if (ctx->fp) {
			fclose (ctx->fp);
		}
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

/**
 * comp_slow_close - XXX
 * @ctx: Mailbox to close (slowly)
 *
 * Returns:
 *	 0: Success
 *	-1: Failure
 */
int
comp_slow_close (CONTEXT *ctx)
{
	if (!ctx)
		return -1;

	FILE *fp;
	const char *append;
	char *cmd;
	COMPRESS_INFO *ci = (COMPRESS_INFO *) ctx->compress_info;

	dprint (2, (debugfile, "comp_slow_close called on '%s'\n", ctx->path));

	if (!(ctx->append
		&& ((append = get_append_command (ctx, ctx->realpath))
		|| (append = ci->close)))) {
		/* if we can not or should not append, we only have to remove the
		 * compressed info, because sync was already called */
		comp_fast_close (ctx);
		return 0;
	}

	if (ctx->fp) {
		fclose (ctx->fp);
		ctx->fp = NULL;
	}

	if (!ctx->quiet) {
		if (append == ci->close) {
			mutt_message (_("Compressing %s..."), ctx->realpath);
		} else {
			mutt_message (_("Compressed-appending to %s..."), ctx->realpath);
		}
	}

	cmd = get_compression_cmd (ctx, append);
	if (!cmd)
		return -1;

	fp = fopen (ctx->realpath, "a");
	if (!fp) {
		mutt_perror (ctx->realpath);
		FREE(&cmd);
		return -1;
	}

	mutt_block_signals();
	if (lock_mailbox (ctx, fp, 1) == -1) {
		fclose (fp);
		mutt_unblock_signals();
		mutt_error _("Unable to lock mailbox!");
		FREE(&cmd);
		return -1;
	}

	dprint (2, (debugfile, "CompressCmd: '%s'\n", cmd));

	endwin();
	fflush (stdout);

	if (append == ci->close) {
		sprintf(echo_cmd,_("echo Compressing %s..."), ctx->realpath); /* __SPRINTF_CHECKED__ */
	} else {
		sprintf(echo_cmd,_("echo Compressed-appending to %s..."), ctx->realpath); /* __SPRINTF_CHECKED__ */
	}
	mutt_system(echo_cmd);

	if (mutt_system (cmd) != 0) {
		mutt_any_key_to_continue (NULL);
		mutt_error (_(" %s: Error compressing mailbox!  Uncompressed one kept!\n"),
			ctx->path);
		FREE(&cmd);
		unlock_mailbox (ctx, fp);
		mutt_unblock_signals();
		fclose (fp);
		return -1;
	}

	unlock_mailbox (ctx, fp);
	mutt_unblock_signals();
	fclose (fp);
	remove_file (ctx);
	restore_path (ctx);
	FREE(&cmd);
	FREE(&ctx->compress_info);

	return 0;
}

