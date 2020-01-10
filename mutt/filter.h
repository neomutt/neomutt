/**
 * @file
 * Pass files through external commands (filters)
 *
 * @authors
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

#ifndef MUTT_LIB_FILTER_H
#define MUTT_LIB_FILTER_H

#include <stdio.h>
#include <unistd.h>

#define EXEC_SHELL "/bin/sh"

pid_t filter_create   (const char *cmd, FILE **fp_in, FILE **fp_out, FILE **fp_err);
pid_t filter_create_fd(const char *cmd, FILE **fp_in, FILE **fp_out, FILE **fp_err, int fdin, int fdout, int fderr);
int   filter_wait     (pid_t pid);

#endif /* MUTT_LIB_FILTER_H */
