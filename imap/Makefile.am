## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

EXTRA_DIST = README auth_anon.c auth_cram.c auth_gss.c auth_sasl.c

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = libimap.a
noinst_HEADERS = auth.h imap_private.h message.h

libimap_a_SOURCES = auth.c auth_plain.c auth_login.c browse.c command.c \
	imap.c imap.h message.c utf7.c util.c

if USE_GSS
libimap_a_SOURCES += auth_gss.c
endif

if USE_SASL
libimap_a_SOURCES += auth_sasl.c
else
libimap_a_SOURCES += auth_anon.c auth_cram.c
endif

