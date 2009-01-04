/*
 * Copyright (C) 2000-5,2008 Brendan Cully <brendan@kublai.com>
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

/* common SASL helper routines */

#ifndef _MUTT_SASL_H_
#define _MUTT_SASL_H_ 1

#include <sasl/sasl.h>

#include "mutt_socket.h"

int mutt_sasl_client_new (CONNECTION*, sasl_conn_t**);
sasl_callback_t* mutt_sasl_get_callbacks (ACCOUNT*);
int mutt_sasl_interact (sasl_interact_t*);
void mutt_sasl_setup_conn (CONNECTION*, sasl_conn_t*);
void mutt_sasl_done (void);

typedef struct 
{
  sasl_conn_t* saslconn;
  const sasl_ssf_t* ssf;
  const unsigned int* pbufsize;

  /* read buffer */
  const char *buf;
  unsigned int blen;
  unsigned int bpos;

  /* underlying socket data */
  void* sockdata;
  int (*msasl_open) (CONNECTION* conn);
  int (*msasl_close) (CONNECTION* conn);
  int (*msasl_read) (CONNECTION* conn, char* buf, size_t len);
  int (*msasl_write) (CONNECTION* conn, const char* buf, size_t count);
  int (*msasl_poll) (CONNECTION* conn);
}
SASL_DATA;

#endif /* _MUTT_SASL_H_ */
