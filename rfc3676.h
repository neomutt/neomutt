/*
 * Copyright (C) 2005 Andreas Krennmair <ak@synflood.at>
 * Copyright (C) 2005 Peter J. Holzer <hjp@hjp.net>
 * Copyright (C) 2005,2007 Rocco Rutte <pdmef@gmx.net>
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
 *
 */ 

/* This file was originally part of mutt-ng */

#ifndef _MUTT_RFC3676_H
#define _MUTT_RFC3676_H

#include "mutt.h"

/* body handler implementing RfC 3676 for format=flowed */
int rfc3676_handler (BODY *a, STATE *s);

/* this does the space-stuffing for RfC3676 style messages */
void rfc3676_space_stuff (HEADER *hdr);

#endif /* !_MUTT_RFC3676_H */
