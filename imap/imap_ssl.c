/*
 * Copyright (C) 1999 Tommi Komulainen <Tommi.Komulainen@iki.fi>
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
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/err.h>

#undef _

#include <string.h>

#include "mutt.h"
#include "imap_socket.h"
#include "mutt_menu.h"
#include "mutt_curses.h"
#include "imap_ssl.h"


char *SslCertFile = NULL;

typedef struct _sslsockdata
{
  SSL_CTX *ctx;
  SSL *ssl;
  X509 *cert;
}
sslsockdata;


static int ssl_check_certificate (sslsockdata * data);


int ssl_socket_read (CONNECTION * conn)
{
  sslsockdata *data = conn->sockdata;
  return SSL_read (data->ssl, conn->inbuf, LONG_STRING);
}

int ssl_socket_write (CONNECTION * conn, const char *buf)
{
  sslsockdata *data = conn->sockdata;
  dprint (1, (debugfile, "ssl_socket_write():%s", buf));
  return SSL_write (data->ssl, buf, mutt_strlen (buf));
}

int ssl_socket_open (CONNECTION * conn)
{
  sslsockdata *data;
  int err;

  if (raw_socket_open (conn) < 0)
    return -1;

  data = (sslsockdata *) safe_calloc (1, sizeof (sslsockdata));
  conn->sockdata = data;

  SSLeay_add_ssl_algorithms ();
  data->ctx = SSL_CTX_new (SSLv23_client_method ());

  data->ssl = SSL_new (data->ctx);
  SSL_set_fd (data->ssl, conn->fd);

  if ((err = SSL_connect (data->ssl)) < 0)
  {
    ssl_socket_close (conn);
    return -1;
  }

  data->cert = SSL_get_peer_certificate (data->ssl);
  if (!data->cert)
  {
    mutt_error (_("Unable to get certificate from peer"));
    sleep (1);
    return -1;
  }

  if (!ssl_check_certificate (data))
  {
    ssl_socket_close (conn);
    return -1;
  }

  mutt_message (_("SSL connection using %s"), SSL_get_cipher (data->ssl));
  sleep (1);

  return 0;
}

int ssl_socket_close (CONNECTION * conn)
{
  sslsockdata *data = conn->sockdata;
  SSL_shutdown (data->ssl);

  X509_free (data->cert);
  SSL_free (data->ssl);
  SSL_CTX_free (data->ctx);

  return raw_socket_close (conn);
}



static char *x509_get_part (char *line, const char *ndx)
{
  static char ret[SHORT_STRING];
  char *c, *c2;

  strncpy (ret, _("Unknown"), sizeof (ret));

  c = strstr (line, ndx);
  if (c)
  {
    c += strlen (ndx);
    c2 = strchr (c, '/');
    if (c2)
      *c2 = '\0';
    strncpy (ret, c, sizeof (ret));
    if (c2)
      *c2 = '/';
  }

  return ret;
}

static void x509_fingerprint (char *s, int l, X509 * cert)
{
  unsigned char md[EVP_MAX_MD_SIZE];
  unsigned int n;
  int j;

  if (!X509_digest (cert, EVP_md5 (), md, &n))
  {
    snprintf (s, l, _("[unable to calculate]"));
  }
  else
  {
    for (j = 0; j < (int) n; j++)
    {
      char ch[8];
      snprintf (ch, 8, "%02X%s", md[j], (j % 2 ? " " : ""));
      strncat (s, ch, l);
    }
  }
}


static int ssl_check_certificate (sslsockdata * data)
{
  char *part[] =
  {"/CN=", "/Email=", "/O=", "/OU=", "/L=", "/ST=", "/C="};
  char helpstr[SHORT_STRING];
  char buf[SHORT_STRING];
  MUTTMENU *menu;
  int done, i;
  FILE *fp;
  char *line = NULL, *c;

  /* automatic check from user's database */
  fp = fopen (SslCertFile, "rt");
  if (fp)
  {
    EVP_PKEY *peer = X509_get_pubkey (data->cert);
    X509 *savedkey = NULL;
    int pass = 0;
    while ((savedkey = PEM_read_X509 (fp, &savedkey, NULL)))
    {
      if (X509_verify (savedkey, peer))
      {
	pass = 1;
	break;
      }
    }
    fclose (fp);
    if (pass)
      return 1;
  }

  menu = mutt_new_menu ();
  menu->max = 15;
  menu->dialog = (char **) safe_calloc (1, menu->max * sizeof (char *));
  for (i = 0; i < menu->max; i++)
    menu->dialog[i] = (char *) safe_calloc (1, SHORT_STRING * sizeof (char));

  strncpy (menu->dialog[0], _("This certificate belongs to:"), SHORT_STRING);
  line = X509_NAME_oneline (X509_get_subject_name (data->cert),
			    buf, sizeof (buf));
  for (i = 1; i <= 5; i++)
  {
    c = x509_get_part (line, part[i - 1]);
    snprintf (menu->dialog[i], SHORT_STRING, "   %s", c);
  }

  strncpy (menu->dialog[7], _("This certificate was issued by:"), SHORT_STRING);
  line = X509_NAME_oneline (X509_get_subject_name (data->cert),
			    buf, sizeof (buf));
  for (i = 8; i <= 12; i++)
  {
    c = x509_get_part (line, part[i - 8]);
    snprintf (menu->dialog[i], SHORT_STRING, "   %s", c);
  }

  buf[0] = '\0';
  x509_fingerprint (buf, sizeof (buf), data->cert);
  snprintf (menu->dialog[14], SHORT_STRING, _("Fingerprint: %s"), buf);

  menu->title = _("SSL Certificate check");
  menu->prompt = _("(r)eject, accept (o)nce, (a)ccept always");
  menu->keys = _("roa");

  helpstr[0] = '\0';
  mutt_make_help (buf, sizeof (buf), _("Exit  "), MENU_GENERIC, OP_EXIT);
  strncat (helpstr, buf, sizeof (helpstr));
  mutt_make_help (buf, sizeof (buf), _("Help"), MENU_GENERIC, OP_HELP);
  strncat (helpstr, buf, sizeof (helpstr));
  menu->help = helpstr;

  done = 0;
  while (!done)
  {
    switch (mutt_menuLoop (menu))
    {
      case -1:			/* abort */
      case OP_MAX + 1:		/* reject */
      case OP_EXIT:
        done = 1;
        break;
      case OP_MAX + 3:		/* accept always */
        done = 0;
	fp = fopen (SslCertFile, "w+t");
	if (fp)
	{
	  if (PEM_write_X509 (fp, data->cert))
	    done = 1;
	  fclose (fp);
	}
	if (!done)
	  mutt_error (_("Warning: Couldn't save certificate"));
	else
	  mutt_message (_("Certificate saved"));
	sleep (1);
        /* fall through */
      case OP_MAX + 2:		/* accept once */
        done = 2;
        break;
    }
  }
  mutt_menuDestroy (&menu);
  return (done == 2);
}
