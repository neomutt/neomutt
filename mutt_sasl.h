/*
 * Copyright (C) 2000 Brendan Cully <brendan@kublai.com>
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
 *     Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 */ 

/* common SASL helper routines */

#ifndef _MUTT_SASL_H_
#define _MUTT_SASL_H_ 1

#include "mutt_socket.h"

#include <sasl.h>

int mutt_sasl_start (void);
sasl_callback_t* mutt_sasl_get_callbacks (ACCOUNT* account);

#endif /* _MUTT_SASL_H_ */
