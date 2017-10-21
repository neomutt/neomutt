## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

EXTRA_DIST = lib.h base64.h buffer.h date.h debug.h exit.h file.h hash.h list.h mapping.h md5.h memory.h message.h queue.h sha1.h string2.h

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = libmutt.a

libmutt_a_SOURCES = base64.c buffer.c date.c debug.c exit.c file.c hash.c list.c mapping.c md5.c memory.c message.c sha1.c string.c

