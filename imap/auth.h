/*
 * Copyright (C) 2000-2001 Brendan Cully <brendan@kublai.com>
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

/* common defs for authenticators. A good place to set up a generic callback
 * system */

#ifndef _IMAP_AUTH_H
#define _IMAP_AUTH_H 1

typedef enum
{
  IMAP_AUTH_SUCCESS = 0,
  IMAP_AUTH_FAILURE,
  IMAP_AUTH_UNAVAIL
} imap_auth_res_t;


typedef struct
{
  /* do authentication, using named method or any available if method is NULL */
  imap_auth_res_t (*authenticate) (IMAP_DATA* idata, const char* method);
  /* name of authentication method supported, NULL means variable. If this
   * is not null, authenticate may ignore the second parameter. */
  const char* method;
} imap_auth_t;

/* external authenticator prototypes */
#ifndef USE_SASL
imap_auth_res_t imap_auth_anon (IMAP_DATA* idata, const char* method);
imap_auth_res_t imap_auth_cram_md5 (IMAP_DATA* idata, const char* method);
#endif
imap_auth_res_t imap_auth_login (IMAP_DATA* idata, const char* method);
#ifdef USE_GSS
imap_auth_res_t imap_auth_gss (IMAP_DATA* idata, const char* method);
#endif
#ifdef USE_SASL
imap_auth_res_t imap_auth_sasl (IMAP_DATA* idata, const char* method);
#endif

#endif /* _IMAP_AUTH_H */
