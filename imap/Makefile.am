## Process this file with automake to produce Makefile.in

AR=@AR@

AUTOMAKE_OPTIONS = foreign

if USE_GSS
GSSSOURCES = auth_gss.c
endif

if USE_SASL
AUTHENTICATORS = auth_sasl.c $(GSSSOURCES)
else
AUTHENTICATORS = auth_anon.c auth_cram.c md5c.c $(GSSSOURCES)
endif

if USE_SSL
SSLSOURCES = imap_ssl.c
SSLHEADERS = imap_ssl.h
endif

EXTRA_DIST = BUGS README TODO imap_ssl.c imap_ssl.h auth_anon.c \
	auth_cram.c auth_gss.c auth_sasl.c md5c.c

INCLUDES = -I$(top_srcdir) -I../intl

noinst_LIBRARIES = libimap.a
noinst_HEADERS = auth.h imap_private.h md5.h message.h $(SSLHEADERS)

libimap_a_SOURCES = auth.c auth_login.c browse.c command.c imap.c imap.h \
	message.c utf7.c util.c $(AUTHENTICATORS) $(SSLSOURCES)

