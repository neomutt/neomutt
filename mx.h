/*
 * Copyright (C) 1996-2002,2013 Michael R. Elkins <me@mutt.org>
 * Copyright (C) 1999-2002 Thomas Roessler <roessler@does-not-exist.org>
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

/*
 * This header file contains prototypes for internal functions used by the
 * generic mailbox api.  None of these functions should be called directly.
 */

#ifndef _MX_H
#define _MX_H

#include "mailbox.h"
#include "buffy.h"

/* supported mailbox formats */
enum
{
  MUTT_MBOX = 1,
  MUTT_MMDF,
  MUTT_MH,
  MUTT_MAILDIR,
  MUTT_IMAP,
  MUTT_POP
};

WHERE short DefaultMagic INITVAL (MUTT_MBOX);

#define MMDF_SEP "\001\001\001\001\n"
#define MAXLOCKATTEMPT 5

int mbox_sync_mailbox (CONTEXT *, int *);
int mbox_lock_mailbox (CONTEXT *, int, int);
int mbox_parse_mailbox (CONTEXT *);
int mmdf_parse_mailbox (CONTEXT *);
void mbox_unlock_mailbox (CONTEXT *);
int mbox_check_empty (const char *);
void mbox_reset_atime (CONTEXT *, struct stat *);

int mh_sync_mailbox (CONTEXT *, int *);
int mh_check_empty (const char *);

int maildir_check_empty (const char *);

FILE *maildir_open_find_message (const char *, const char *);

int mbox_strict_cmp_headers (const HEADER *, const HEADER *);
int mutt_reopen_mailbox (CONTEXT *, int *);

void mx_alloc_memory (CONTEXT *);
void mx_update_context (CONTEXT *, int);
void mx_update_tables (CONTEXT *, int);


int mx_lock_file (const char *, int, int, int, int);
int mx_unlock_file (const char *path, int fd, int dot);

extern struct mx_ops mx_maildir_ops;
extern struct mx_ops mx_mbox_ops;
extern struct mx_ops mx_mh_ops;
extern struct mx_ops mx_mmdf_ops;

#endif
