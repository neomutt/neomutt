/**
 * @file
 * Handling of email attachments
 *
 * @authors
 * Copyright (C) 2018-2023 Richard Russon <rich@flatcap.org>
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

/* common protos for compose / attach menus */

#ifndef MUTT_ATTACH_MUTT_ATTACH_H
#define MUTT_ATTACH_MUTT_ATTACH_H

#include <stdbool.h>
#include <stdio.h>
#include "mutt/lib.h"

struct AttachCtx;
struct Body;
struct ConfigSubset;
struct Email;
struct Menu;
struct MuttWindow;

/**
 * enum ViewAttachMode - Options for mutt_view_attachment()
 */
enum ViewAttachMode
{
  MUTT_VA_REGULAR = 1, ///< View using default method
  MUTT_VA_MAILCAP,     ///< Force viewing using mailcap entry
  MUTT_VA_AS_TEXT,     ///< Force viewing as text
  MUTT_VA_PAGER,       ///< View attachment in pager using copiousoutput mailcap
};

/**
 * enum SaveAttach - Options for saving attachments
 *
 * @sa mutt_save_attachment(), mutt_decode_save_attachment(),
 *     save_attachment_open(), mutt_check_overwrite()
 */
enum SaveAttach
{
  MUTT_SAVE_NO_FLAGS = 0, ///< No flags set
  MUTT_SAVE_APPEND,       ///< Append to existing file
  MUTT_SAVE_OVERWRITE,    ///< Overwrite existing file
};

int mutt_attach_display_loop(struct ConfigSubset *sub, struct Menu *menu, int op, struct Email *e, struct AttachCtx *actx, bool recv);

void mutt_save_attachment_list (struct AttachCtx *actx, FILE *fp, bool tag, struct Body *b, struct Email *e, struct Menu *menu);
void mutt_pipe_attachment_list (struct AttachCtx *actx, FILE *fp, bool tag, struct Body *b, bool filter);
void mutt_print_attachment_list(struct AttachCtx *actx, FILE *fp, bool tag, struct Body *b);

int mutt_view_attachment(FILE *fp, struct Body *b, enum ViewAttachMode mode, struct Email *e, struct AttachCtx *actx, struct MuttWindow *win);

void mutt_check_lookup_list     (struct Body *b, char *type, size_t len);
int  mutt_compose_attachment    (struct Body *b);
int  mutt_decode_save_attachment(FILE *fp, struct Body *b, const char *path, StateFlags flags, enum SaveAttach opt);
bool mutt_edit_attachment       (struct Body *b);
int  mutt_get_tmp_attachment    (struct Body *b);
int  mutt_pipe_attachment       (FILE *fp, struct Body *b, const char *path, const char *outfile);
int  mutt_print_attachment      (FILE *fp, struct Body *b);
int  mutt_save_attachment       (FILE *fp, struct Body *b, const char *path, enum SaveAttach opt, struct Email *e);

/* small helper functions to handle temporary attachment files */
void mutt_add_temp_attachment(const char *filename);
void mutt_temp_attachments_cleanup(void);

#endif /* MUTT_ATTACH_MUTT_ATTACH_H */
