/**
 * @file
 * Calculate the SHA1 checksum of a buffer
 *
 * SHA-1 in C
 *
 * By Steve Reid <steve@edmweb.com>, with small changes to make it
 * fit into mutt by Thomas Roessler <roessler@does-not-exist.org>.
 */

#ifndef _MUTT_SHA1_H
#define _MUTT_SHA1_H

#include <stdint.h>

/**
 * struct Sha1Ctx - Cursor for the SHA1 hashing
 */
struct Sha1Ctx
{
  uint32_t state[5];
  uint32_t count[2];
  unsigned char buffer[64];
};

void sha1_transform(uint32_t state[5], const unsigned char buffer[64]);
void sha1_init(struct Sha1Ctx *context);
void sha1_update(struct Sha1Ctx *context, const unsigned char *data, uint32_t len);
void sha1_final(unsigned char digest[20], struct Sha1Ctx *context);

#define SHA_DIGEST_LENGTH 20

#endif /* _MUTT_SHA1_H */
