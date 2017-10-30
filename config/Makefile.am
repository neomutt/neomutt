## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

EXTRA_DIST = account.h address.h bool.h inheritance.h magic.h mbtable.h number.h path.h quad.h regex2.h set.h sort.h string3.h types.h

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = libconfig.a

libconfig_a_SOURCES = account.c address.c bool.c magic.c mbtable.c number.c path.c quad.c regex.c set.c sort.c string.c

