/**
 * @file
 * Signal handling
 *
 * @authors
 * Copyright (C) 2017 Richard Russon <rich@flatcap.org>
 *
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MUTT_LIB_SIGNAL_H
#define MUTT_LIB_SIGNAL_H

#include <stdbool.h>

/**
 * typedef sig_handler_t - Handler for signals
 * @param sig Signal number, e.g. SIGINT
 */
typedef void (*sig_handler_t)(int sig);

void mutt_sig_allow_interrupt(bool allow);
void mutt_sig_block(void);
void mutt_sig_block_system(void);
void mutt_sig_empty_handler(int sig);
void mutt_sig_exit_handler(int sig);
void mutt_sig_init(sig_handler_t sig_fn, sig_handler_t exit_fn, sig_handler_t segv_fn);
void mutt_sig_unblock(void);
void mutt_sig_unblock_system(bool restore);

#endif /* MUTT_LIB_SIGNAL_H */
