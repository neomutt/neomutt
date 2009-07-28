/*
 * Copyright (C) 2006 Thomas Roessler <roessler@does-not-exist.org>
 * Copyright (C) 2009 Rocco Rutte <pdmef@gmx.net>
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

#ifndef _MUTT_GROUP_H_
#define _MUTT_GROUP_H_ 1

#define M_GROUP		0
#define M_UNGROUP	1

void mutt_group_add_adrlist (group_t *g, ADDRESS *a);

void mutt_group_context_add (group_context_t **ctx, group_t *group);
void mutt_group_context_destroy (group_context_t **ctx);
void mutt_group_context_add_adrlist (group_context_t *ctx, ADDRESS *a);
int mutt_group_context_add_rx (group_context_t *ctx, const char *s, int flags, BUFFER *err);

int mutt_group_match (group_t *g, const char *s);

int mutt_group_context_clear (group_context_t **ctx);
int mutt_group_context_remove_rx (group_context_t *ctx, const char *s);
int mutt_group_context_remove_adrlist (group_context_t *ctx, ADDRESS *);

#endif /* _MUTT_GROUP_H_ */
