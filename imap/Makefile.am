## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

EXTRA_DIST = BUGS README TODO

INCLUDES = -I$(top_srcdir)

noinst_LIBRARIES = libimap.a
noinst_HEADERS = imap_private.h imap_socket.h md5.h message.h

libimap_a_SOURCES = auth.c browse.c command.c imap.c imap.h md5c.c message.c \
	socket.c util.c
