/**
 * @file
 * Signal handling
 *
 * @authors
 * Copyright (C) 2017-2024 Richard Russon <rich@flatcap.org>
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

#ifndef MUTT_MUTT_SIGNAL2_H
#define MUTT_MUTT_SIGNAL2_H

#include "config.h"
#include <signal.h>
#include <stdbool.h>

#ifdef USE_DEBUG_BACKTRACE
void show_backtrace(void);
#else
static inline void show_backtrace(void) {} // LCOV_EXCL_LINE
#endif

extern volatile sig_atomic_t SigInt;   ///< true after SIGINT is received
extern volatile sig_atomic_t SigWinch; ///< true after SIGWINCH is received

/**
 * @defgroup sig_handler_api Signal Handling API
 *
 * Prototype for a Signal Handler function
 *
 * @param sig Signal number, e.g. SIGINT
 */
typedef void (*sig_handler_t)(int sig);

void assertion_dump(const char *file, int line, const char *func, const char *cond);

#ifndef NDEBUG
#if __GNUC__
#define ASSERT_STOP __builtin_trap()
#elif _MSC_VER
#define ASSERT_STOP __debugbreak()
#else
#define ASSERT_STOP (*(volatile int *) 0 = 0)
#endif
#define ASSERT(COND)                                                           \
  do                                                                           \
  {                                                                            \
    if (!(COND))                                                               \
    {                                                                          \
      assertion_dump(__FILE__, __LINE__, __func__, #COND);                     \
      ASSERT_STOP;                                                             \
    }                                                                          \
  } while (false);
#else
#define ASSERT(COND)                                                           \
  do                                                                           \
  {                                                                            \
    if (COND)                                                                  \
    {                                                                          \
    }                                                                          \
  } while (false);
#endif

void mutt_sig_allow_interrupt(bool allow);
void mutt_sig_block(void);
void mutt_sig_block_system(void);
void mutt_sig_empty_handler(int sig);
void mutt_sig_exit_handler(int sig);
void mutt_sig_init(sig_handler_t sig_fn, sig_handler_t exit_fn, sig_handler_t segv_fn);
void mutt_sig_reset_child_signals(void);
void mutt_sig_unblock(void);
void mutt_sig_unblock_system(bool restore);

#endif /* MUTT_MUTT_SIGNAL2_H */
