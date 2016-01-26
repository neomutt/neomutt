/*
 * Copyright (C) 1997 Alain Penders <Alain@Finale-Dev.com>
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

int mutt_can_read_compressed (const char *);
int mutt_can_append_compressed (const char *);
int mutt_open_read_compressed (CONTEXT *);
int mutt_open_append_compressed (CONTEXT *);
int mutt_slow_close_compressed (CONTEXT *);
int mutt_sync_compressed (CONTEXT *);
int mutt_test_compress_command (const char *);
int mutt_check_mailbox_compressed (CONTEXT *);
void mutt_fast_close_compressed (CONTEXT *);
