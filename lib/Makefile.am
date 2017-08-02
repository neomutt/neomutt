## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

EXTRA_DIST = lib.h lib_ascii.h lib_base64.h

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = liblib.a
noinst_HEADERS =

liblib_a_SOURCES = lib_ascii.c lib_base64.c

