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

#ifndef _IMAP_H
#define _IMAP_H 1

#include "browser.h"
#include "mailbox.h"

typedef struct
{
  char user[32];
  char pass[32];
  char host[128];
  int port;
  char type[16];
  int socktype;
  char *mbox;
  int flags;
} IMAP_MBOX;


/* imap.c */
int imap_check_mailbox (CONTEXT *ctx, int *index_hint);
int imap_create_mailbox (CONTEXT* idata, char* mailbox);
int imap_close_connection (CONTEXT *ctx);
int imap_delete_mailbox (CONTEXT* idata, char* mailbox);
int imap_open_mailbox (CONTEXT *ctx);
int imap_open_mailbox_append (CONTEXT *ctx);
int imap_select_mailbox (CONTEXT *ctx, const char* path);
void imap_set_logout (CONTEXT *ctx);
int imap_sync_mailbox (CONTEXT *ctx, int expunge);
void imap_fastclose_mailbox (CONTEXT *ctx);
int imap_buffy_check (char *path);
int imap_mailbox_check (char *path, int new);
int imap_subscribe (char *path, int subscribe);
int imap_complete (char* dest, size_t dlen, char* path);

void imap_allow_reopen (CONTEXT *ctx);
void imap_disallow_reopen (CONTEXT *ctx);

/* browse.c */
int imap_init_browse (char *path, struct browser_state *state);

/* message.c */
int imap_append_message (CONTEXT* ctx, MESSAGE* msg);
int imap_copy_messages (CONTEXT* ctx, HEADER* h, char* dest, int delete);
int imap_fetch_message (MESSAGE* msg, CONTEXT* ctx, int msgno);

/* util.c */
int imap_parse_path (const char* path, IMAP_MBOX *mx);
void imap_qualify_path (char* dest, size_t len, const IMAP_MBOX *mx,
  const char* path, const char* name);

int imap_wait_keepalive (pid_t pid);

#endif
