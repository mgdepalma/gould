#ifndef SHA1_H
#define SHA1_H
/*
* SHA-1 in C By Steve Reid <steve@edmweb.com> 100% Public Domain
* 2024-05-17 Generations Linux <bugs@softcraft.org>
*/
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/* SHA-1 produces a 160-bit (20-byte) hash value */
#define SHA1_DIGEST_SIZE 20
#define SHA1_STRING_SIZE 40

typedef struct
{
    uint32_t state[5];
    uint32_t count[2];
    unsigned char buffer[64];
} SHA1_CTX;

void SHA1Transform(uint32_t state[5], const unsigned char buffer[64]);

void SHA1Init(SHA1_CTX *context);

void SHA1Update(SHA1_CTX *context, const unsigned char *data, uint32_t len);

void SHA1Final(unsigned char digest[20], SHA1_CTX * context);

void SHA1(unsigned char *hash_out, const char *str, uint32_t len);

const char *sha1sum(const char *filepath);

#if defined(__cplusplus)
}
#endif

#endif /* SHA1_H */
