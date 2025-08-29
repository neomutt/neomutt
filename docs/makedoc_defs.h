/**
 * @file
 * Helper for makedoc to enable all code paths
 *
 * @authors
 * @copyright
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* build complete documentation */

#ifndef _MUTT_MAKEDOC_DEFS_H
#define _MUTT_MAKEDOC_DEFS_H

#ifdef MAKEDOC_FULL
#ifndef CRYPT_BACKEND_CLASSIC_PGP
#define CRYPT_BACKEND_CLASSIC_PGP
#endif
#ifndef CRYPT_BACKEND_CLASSIC_SMIME
#define CRYPT_BACKEND_CLASSIC_SMIME
#endif
#ifndef CRYPT_BACKEND_GPGME
#define CRYPT_BACKEND_GPGME
#endif
#ifndef HAVE_BDB
#define HAVE_BDB
#endif
#ifndef HAVE_GDBM
#define HAVE_GDBM
#endif
#ifndef HAVE_GETADDRINFO
#define HAVE_GETADDRINFO
#endif
#ifndef HAVE_LIBIDN
#define HAVE_LIBIDN
#endif
#ifndef HAVE_LZ4
#define HAVE_LZ4
#endif
#ifndef HAVE_QDBM
#define HAVE_QDBM
#endif
#ifndef HAVE_SSL_PARTIAL_CHAIN
#define HAVE_SSL_PARTIAL_CHAIN
#endif
#ifndef HAVE_ZLIB
#define HAVE_ZLIB
#endif
#ifndef HAVE_ZSTD
#define HAVE_ZSTD
#endif
#ifndef USE_AUTOCRYPT
#define USE_AUTOCRYPT
#endif
#ifndef USE_HCACHE
#define USE_HCACHE
#endif
#ifndef USE_HCACHE_COMPRESSION
#define USE_HCACHE_COMPRESSION
#endif
#ifndef USE_LUA
#define USE_LUA
#endif
#ifndef USE_NOTMUCH
#define USE_NOTMUCH
#endif
#ifndef USE_SASL
#define USE_SASL
#endif
#ifndef USE_SSL
#define USE_SSL
#endif
#ifndef USE_SSL_GNUTLS
#define USE_SSL_GNUTLS
#endif
#ifndef USE_SSL_OPENSSL
#define USE_SSL_OPENSSL
#endif
#ifndef USE_ZLIB
#define USE_ZLIB
#endif
#endif

#endif /* _MUTT_MAKEDOC_DEFS_H */
