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

#include <sasl.h>

#include "mutt_socket.h"

int mutt_sasl_start (void);
sasl_callback_t* mutt_sasl_get_callbacks (ACCOUNT* account);
int mutt_sasl_interact (sasl_interact_t* interaction);
void mutt_sasl_setup_conn (CONNECTION* conn, sasl_conn_t* saslconn);

typedef struct 
{
  sasl_conn_t* saslconn;
  const sasl_ssf_t* ssf;
  const unsigned int* pbufsize;

  /* read buffer */
  char* buf;
  unsigned int blen;
  unsigned int bpos;

  /* underlying socket data */
  void* sockdata;
  int (*open) (CONNECTION* conn);
  int (*close) (CONNECTION* conn);
  int (*read) (CONNECTION* conn);
  int (*write) (CONNECTION* conn, const char* buf, size_t count);
}
SASL_DATA;

#endif /* _MUTT_SASL_H_ */
