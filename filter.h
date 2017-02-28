/*
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

#ifndef FILTER_INCLUDED
#define FILTER_INCLUDED

#include <unistd.h>
#include <stdio.h>

pid_t
mutt_create_filter_fd (const char *cmd, FILE **in, FILE **out, FILE **err,
                       int fdin, int fdout, int fderr);
pid_t mutt_create_filter (const char *s, FILE **in, FILE **out, FILE **err);
int mutt_wait_filter (pid_t pid);

#endif

