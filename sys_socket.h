/*
 * Copyright (C) 2016 Kevin J. McCarthy <kevin@8t8.us>
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

/* Solaris, OpenIndiana, and probably other derivatives
 * are including sys/stream.h inside their sys/socket.h.
 *
 * This include file is defining macros M_CMD and M_READ which
 * are conflicting with the same macros Mutt defines in mutt.h
 *
 * To minimize breakage with out-of-tree patches, this is a workaround.
 */

#ifdef M_CMD
# define MUTT_ORIG_CMD M_CMD
# undef M_CMD
#endif

#ifdef M_READ
# define MUTT_ORIG_READ M_READ
# undef M_READ
#endif

#include <sys/socket.h>

#undef M_CMD
#undef M_READ

#ifdef MUTT_ORIG_CMD
# define M_CMD MUTT_ORIG_CMD
# undef MUTT_ORIG_CMD
#endif

#ifdef MUTT_ORIG_READ
# define M_READ MUTT_ORIG_READ
# undef MUTT_ORIG_READ
#endif
