/* Copyright (C) 2000 Michael R. Elkins <me@mutt.org>
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

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <prinit.h>
#include <pk11func.h>
#include <prtypes.h>
#include <prio.h>
#include <prnetdb.h>
#include "nss.h"
#include "ssl.h"
#include "sechash.h"
#include "cert.h"
#include "cdbhdl.h"
#include "mutt.h"
#include "mutt_socket.h"
#include "mutt_curses.h"

static int MuttNssInitialized = 0;

/* internal data struct we use with the CONNECTION.  this is where NSS-specific
 * data gets stuffed so that the main mutt_socket.h doesn't have to be
 * modified.
 */
typedef struct
{
  PRFileDesc *fd;
  CERTCertDBHandle *db;
}
mutt_nss_t;

/* nss callback to grab the user's password. */
static char *
mutt_nss_password_func (PK11SlotInfo * slot, PRBool retry, void *arg)
{
  return NULL;
}

static int
mutt_nss_error (const char *call)
{
  mutt_error (_("%s failed (error %d)"), call, PR_GetError ());
  return -1;
}

/* initialize the NSS library for use.  must be called prior to any other
 * functions in this module.
 */
static int
mutt_nss_init (void)
{
  if (!MuttNssInitialized)
  {
    PK11_SetPasswordFunc (mutt_nss_password_func);
    if (NSS_Init (SslCertFile) == SECFailure)
      return mutt_nss_error ("NSS_Init");
    
    /* always use strong crypto. */
    if (NSS_SetDomesticPolicy () == SECFailure)
      return mutt_nss_error ("NSS_SetDomesticPolicy");
    
    /* intialize the session cache */
    SSL_ClearSessionCache ();
    
    MuttNssInitialized = 1;
  }
  return 0;
}

/* convert from int64 to a readable string and print on the screen */
static void
mutt_nss_pretty_time (int64 usecs)
{
  struct tm t;
  PRExplodedTime ex;
  char timebuf[128];
  
  PR_ExplodeTime (usecs, PR_LocalTimeParameters, &ex);
  
  t.tm_sec = ex.tm_sec;
  t.tm_min = ex.tm_min;
  t.tm_hour = ex.tm_hour;
  t.tm_mday = ex.tm_mday;
  t.tm_mon = ex.tm_month;
  t.tm_year = ex.tm_year - 1900;	/* PRExplodedTime uses the absolute year */
  t.tm_wday = ex.tm_wday;
  t.tm_yday = ex.tm_yday;
  
  strfcpy (timebuf, asctime (&t), sizeof (timebuf));
  timebuf[strlen (timebuf) - 1] = 0;
  
  addstr (timebuf);
}

/* this function is called by the default hook CERT_AuthCertificate when it
 * can't verify a cert based upon the contents of the user's certificate
 * database.  we are given the option to override the decision and accept
 * it anyway.
 */
static SECStatus
mutt_nss_bad_cert (void *arg, PRFileDesc * fd)
{
  PRErrorCode err;
  CERTCertificate *cert, *issuer;
  unsigned char hash[16];
  int i;
  CERTCertTrust trust;
  int64 not_before, not_after;
  event_t ch;
  char status[256];

  /* first lets see why this certificate failed.  we only want to override
   * in the case where the cert was not found.
   */
  err = PR_GetError ();
  mutt_error (_("SSL_AuthCertificate failed (error %d)"), err);

  /* fetch the cert in question */
  cert = SSL_PeerCertificate (fd);

  move (LINES - 8, 0);
  clrtoeol ();
  move (LINES - 7, 0);
  clrtoeol ();
  addstr ("Issuer:      ");
  addstr (CERT_NameToAscii (&cert->issuer));
  move (LINES - 6, 0);
  clrtoeol ();
  addstr ("Subject:     ");
  addstr (CERT_NameToAscii (&cert->subject));

  move (LINES - 5, 0);
  clrtoeol ();
  addstr ("Valid:       ");
  CERT_GetCertTimes (cert, &not_before, &not_after);
  mutt_nss_pretty_time (not_before);
  addstr (" to ");
  mutt_nss_pretty_time (not_after);

  move (LINES - 4, 0);
  clrtoeol ();
  addstr ("Fingerprint: ");

  /* calculate the MD5 hash of the raw certificate */
  HASH_HashBuf (HASH_AlgMD5, hash, cert->derCert.data, cert->derCert.len);
  for (i = 0; i < 16; i++)
  {
    printw ("%0x", hash[i]);
    if (i != 15)
      addch (':');
  }
  
  mvaddstr (LINES - 3, 0, "Signature:   ");
  clrtoeol ();

  /* find the cert which signed this cert */
  issuer = CERT_FindCertByName ((CERTCertDBHandle *) arg, &cert->derIssuer);

  /* verify the sig (only) if we have the issuer cert handy */
  if (issuer &&
      CERT_VerifySignedData (&cert->signatureWrap, issuer, PR_Now (), NULL)
      == SECSuccess)
    addstr ("GOOD");
  else
    addstr ("BAD");

  move (LINES - 2, 0);
  SETCOLOR (MT_COLOR_STATUS);
  memset (status, ' ', sizeof (status) - 1);
  if (COLS < sizeof (status))
    status[COLS - 1] = 0;
  else
    status[sizeof (status) - 1] = 0;
  memcpy (status, "--- SSL Certificate Check",
	  sizeof ("--- SSL Certificate Check") - 1);
  addstr (status);
  clrtoeol ();
  SETCOLOR (MT_COLOR_NORMAL);

  for (;;)
  {
    mvaddstr (LINES - 1, 0, "(r)eject, accept (o)nce, (a)lways accept?");
    clrtoeol ();
    ch = mutt_getch ();
    if (ch.ch == -1)
    {
      i = SECFailure;
      break;
    }
    else if (ascii_tolower (ch.ch) == 'r')
    {
      i = SECFailure;
      break;
    }
    else if (ascii_tolower (ch.ch) == 'o')
    {
      i = SECSuccess;
      break;
    }
    else if (ascii_tolower (ch.ch) == 'a')
    {
      /* push this certificate onto the user's certificate store so it
       * automatically becomes valid next time we see it
       */
      
      /* set this certificate as a valid peer for SSL-auth ONLY. */
      CERT_DecodeTrustString (&trust, "P,,");
      
      CERT_AddTempCertToPerm (cert, NULL, &trust);
      i = SECSuccess;
      break;
    }
    BEEP ();
  }
  
  /* SSL_PeerCertificate() returns a copy with an updated ref count, so
   * we have to destroy our copy here.
   */
  CERT_DestroyCertificate (cert);
  
  return i;
}

static int
mutt_nss_socket_open (CONNECTION * con)
{
  mutt_nss_t *sockdata;
  PRNetAddr addr;
  struct hostent *he;

  memset (&addr, 0, sizeof (addr));
  addr.inet.family = AF_INET;
  addr.inet.port = PR_htons (con->account.port);
  he = gethostbyname (con->account.host);
  if (!he)
  {
    mutt_error (_("Unable to find ip for host %s"), con->account.host);
    return -1;
  }
  addr.inet.ip = *((int *) he->h_addr_list[0]);

  sockdata = safe_calloc (1, sizeof (mutt_nss_t));

  do
  {
    sockdata->fd = PR_NewTCPSocket ();
    if (sockdata->fd == NULL)
    {
      mutt_error (_("PR_NewTCPSocket failed."));
      break;
    }
    /* make this a SSL socket */
    sockdata->fd = SSL_ImportFD (NULL, sockdata->fd);
    
    /* set SSL version options based upon user's preferences */
    if (!option (OPTTLSV1))
      SSL_OptionSet (sockdata->fd, SSL_ENABLE_TLS, PR_FALSE);
    
    if (!option (OPTSSLV2))
      SSL_OptionSet (sockdata->fd, SSL_ENABLE_SSL2, PR_FALSE);

    if (!option (OPTSSLV3))
      SSL_OptionSet (sockdata->fd, SSL_ENABLE_SSL3, PR_FALSE);
    
    /* set the host we were attempting to connect to in order to verify
     * the name in the certificate we get back.
     */
    if (SSL_SetURL (sockdata->fd, con->account.host))
    {
      mutt_nss_error ("SSL_SetURL");
      break;
    }

    /* we don't need no stinking pin.  we don't authenticate ourself
     * via SSL.
     */
    SSL_SetPKCS11PinArg (sockdata->fd, 0);
    
    sockdata->db = CERT_GetDefaultCertDB ();
    
    /* use the default supplied hook.  it takes an argument to our
     * certificate database.  the manual lies, you can't really specify
     * NULL for the callback to get the default!
     */
    SSL_AuthCertificateHook (sockdata->fd, SSL_AuthCertificate,
			     sockdata->db);
    /* set the callback to be used when SSL_AuthCertificate() fails.  this
     * allows us to override and insert the cert back into the db
     */
    SSL_BadCertHook (sockdata->fd, mutt_nss_bad_cert, sockdata->db);
    
    if (PR_Connect (sockdata->fd, &addr, PR_INTERVAL_NO_TIMEOUT) ==
	PR_FAILURE)
    {
      mutt_error (_("Unable to connect to host %s"), con->account.host);
      break;
    }
    
    /* store the extra info in the CONNECTION struct for later use. */
    con->sockdata = sockdata;
    
    /* HACK.  some of the higher level calls in mutt_socket.c depend on this
     * being >0 when we are in the connected state.  we just set this to
     * an arbitrary value to avoid hitting that bug, since we neve have the
     * real fd.
     */
    con->fd = 42;
    
    /* success */
    return 0;
  }
  while (0);
  
  /* we get here when we had an oops.  clean up the mess. */

  if (sockdata)
  {
    if (sockdata->fd)
      PR_Close (sockdata->fd);
    if (sockdata->db)
      CERT_ClosePermCertDB (sockdata->db);
    FREE (&sockdata);
  }
  return -1;
}

static int
mutt_nss_socket_close (CONNECTION * con)
{
  mutt_nss_t *sockdata = (mutt_nss_t *) con->sockdata;

  if (PR_Close (sockdata->fd) == PR_FAILURE)
    return -1;

  if (sockdata->db)
    CERT_ClosePermCertDB (sockdata->db);
  /* free up the memory we used for this connection specific to NSS. */
  FREE (&con->sockdata);
  return 0;
}

static int
mutt_nss_socket_read (CONNECTION* conn, char* buf, size_t len)
{
  return PR_Read (((mutt_nss_t*) conn->sockdata)->fd, buf, len);
}

static int
mutt_nss_socket_write (CONNECTION * con, const char *buf, size_t count)
{
  return PR_Write (((mutt_nss_t *) con->sockdata)->fd, buf, count);
}

/* initialize a new connection for use with NSS */
int
mutt_nss_socket_setup (CONNECTION * con)
{
  if (mutt_nss_init ())
    return -1;
  con->open = mutt_nss_socket_open;
  con->read = mutt_nss_socket_read;
  con->write = mutt_nss_socket_write;
  con->close = mutt_nss_socket_close;
  return 0;
}
