/**
 * @file
 * Run external commands in the background
 *
 * @authors
 * Copyright (C) 2026 Reza Jelveh
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

/**
 * @page lib_background Background commands
 *
 * Run external commands in the background
 *
 * | File                    | Description                        |
 * | :---------------------- | :--------------------------------- |
 * | background/background.c | @subpage background_background     |
 * | background/dlg_background.c | @subpage background_dlg_background |
 * | background/functions.c  | @subpage background_functions      |
 */

#ifndef MUTT_BACKGROUND_LIB_H
#define MUTT_BACKGROUND_LIB_H

struct NeoMutt;
struct SubMenu;

void background_init_keys(struct NeoMutt *n, struct SubMenu *sm_generic);
void bg_cleanup(void);
void bg_init(void);
int bg_job_exit_code(int slot);
int bg_job_start(const char *cmd);
int bg_reap(void);
void bg_start_command(void);
void bg_wait(void);
void dlg_background(void);

#endif /* MUTT_BACKGROUND_LIB_H */
