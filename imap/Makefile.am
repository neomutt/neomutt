## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

if USE_GSS
GSSSOURCES = auth_gss.c
endif

if USE_SSL
SSLSOURCES = imap_ssl.c
SSLHEADERS = imap_ssl.h
endif

EXTRA_DIST = BUGS README TODO imap_ssl.c imap_ssl.h auth_gss.c

INCLUDES = -I$(top_srcdir) -I../intl

noinst_LIBRARIES = libimap.a
noinst_HEADERS = imap_private.h imap_socket.h md5.h message.h $(SSLHEADERS)

libimap_a_SOURCES = auth.c browse.c command.c imap.c imap.h md5c.c message.c \
	socket.c utf7.c util.c $(GSSSOURCES) $(SSLSOURCES)
