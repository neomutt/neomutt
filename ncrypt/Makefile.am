## Process this file with automake to produce Makefile.in
include $(top_srcdir)/flymake.am

AUTOMAKE_OPTIONS = 1.6 foreign

EXTRA_DIST = crypt_gpgme.c crypt_mod_pgp_classic.c crypt_mod_pgp_gpgme.c \
	crypt_mod_smime_classic.c crypt_mod_smime_gpgme.c gnupgparse.c pgp.c \
	pgpinvoke.c pgpkey.c pgplib.c pgpmicalg.c pgppacket.c smime.c

AM_CPPFLAGS = -I$(top_srcdir)

noinst_LIBRARIES = libncrypt.a
noinst_HEADERS = crypt.h cryptglue.h crypt_gpgme.h crypt_mod.h gnupgparse.h \
	ncrypt.h pgp.h pgpinvoke.h pgpkey.h pgplib.h pgpmicalg.h pgppacket.h \
	smime.h

libncrypt_a_SOURCES = crypt.c cryptglue.c crypt_mod.c

if BUILD_NCRYPT_GPGME
libncrypt_a_SOURCES += crypt_gpgme.c crypt_mod_pgp_gpgme.c crypt_mod_smime_gpgme.c
endif

if BUILD_NCRYPT_PGP
libncrypt_a_SOURCES += pgp.c pgpinvoke.c pgpkey.c pgplib.c gnupgparse.c \
	pgpmicalg.c pgppacket.c crypt_mod_pgp_classic.c
endif

if BUILD_NCRYPT_SMIME
libncrypt_a_SOURCES += smime.c crypt_mod_smime_classic.c
endif

