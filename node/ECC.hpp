/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2026-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

/*
 * This file defines the elliptic curve crypto used for ZeroTier V1. The normal
 * public version uses C25519 and Ed25519, while the FIPS version uses NIST.
 * FIPS builds are completely incompatible with regular ZeroTier, but that's
 * fine since FIPS users typically want a fully isolated private network. If you
 * are not such a user you probably don't want this.
 */

#ifndef ZT_ECC_HPP
#define ZT_ECC_HPP

#include "Utils.hpp"

#ifdef ZT_FIPS

/* FIPS140/NIST ECC cryptography */
/* Note that to be FIPS we also need to link against a FIPS-certified library. */

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#define ZT_ECC_EPHEMERAL_PUBLIC_KEY_LEN 97       /* Single ECC P-384 key */
#define ZT_ECC_PUBLIC_KEY_SET_LEN       (97 * 2) /* Two ECC P-384 keys */
#define ZT_ECC_PRIVATE_KEY_SET_LEN      (48 * 2) /* Two ECC P-384 secret keys */
#define ZT_ECC_SIGNATURE_LEN            96       /* NIST P-384 ECDSA signature */

class ECC {
  public:
    struct Public {
        uint8_t data[ZT_ECC_PUBLIC_KEY_SET_LEN];
    };
    struct Private {
        uint8_t data[ZT_ECC_PRIVATE_KEY_SET_LEN];
    };
    struct Signature {
        uint8_t data[ZT_ECC_SIGNATURE_LEN];
    };
    struct Pair {
        Public pub;
        Private priv;
    };
};

#else   // Curve25519 / Ed25519

namespace ZeroTier {

#define ZT_ECC_EPHEMERAL_PUBLIC_KEY_LEN 32 /* Single C25519 ECDH key */
#define ZT_ECC_PUBLIC_KEY_SET_LEN       64 /* C25519 and Ed25519 keys */
#define ZT_ECC_PRIVATE_KEY_SET_LEN      64 /* C25519 and Ed25519 secret keys */
#define ZT_ECC_SIGNATURE_LEN            96 /* Ed25519 signature plus (not necessary) hash */

class ECC {
  public:
    struct Public {
        uint8_t data[ZT_ECC_PUBLIC_KEY_SET_LEN];
    };
    struct Private {
        uint8_t data[ZT_ECC_PRIVATE_KEY_SET_LEN];
    };
    struct Signature {
        uint8_t data[ZT_ECC_SIGNATURE_LEN];
    };
    struct Pair {
        Public pub;
        Private priv;
    };

    /**
     * Generate an elliptic curve key pair
     */
    static inline Pair generate()
    {
        Pair kp;
        Utils::getSecureRandom(kp.priv.data, ZT_ECC_PRIVATE_KEY_SET_LEN);
        _calcPubDH(kp);
        _calcPubED(kp);
        return kp;
    }

    /**
     * Generate a key pair satisfying a condition
     *
     * This begins with a random keypair from a random secret key and then
     * iteratively increments the random secret until cond(kp) returns true.
     * This is used to compute key pairs in which the public key, its hash
     * or some other aspect of it satisfies some condition, such as for a
     * hashcash criteria.
     *
     * @param cond Condition function or function object
     * @return Key pair where cond(kp) returns true
     * @tparam F Type of 'cond'
     */
    template <typename F> static inline Pair generateSatisfying(F cond)
    {
        Pair kp;
        void* const priv = (void*)kp.priv.data;
        Utils::getSecureRandom(priv, ZT_ECC_PRIVATE_KEY_SET_LEN);
        _calcPubED(kp);   // do Ed25519 key -- bytes 32-63 of pub and priv
        do {
            ++(((uint64_t*)priv)[1]);
            --(((uint64_t*)priv)[2]);
            _calcPubDH(kp);   // keep regenerating bytes 0-31 until satisfied
        } while (! cond(kp));
        return kp;
    }

    /**
     * Perform C25519 ECC key agreement
     *
     * Actual key bytes are generated from one or more SHA-512 digests of
     * the raw result of key agreement.
     *
     * @param mine My private key
     * @param their Their public key
     * @param keybuf Buffer to fill
     * @param keylen Number of key bytes to generate
     */
    static void agree(const Private& mine, const Public& their, void* keybuf, unsigned int keylen);
    static inline void agree(const Pair& mine, const Public& their, void* keybuf, unsigned int keylen)
    {
        agree(mine.priv, their, keybuf, keylen);
    }

    /**
     * Sign a message with a sender's key pair
     *
     * This takes the SHA-521 of msg[] and then signs the first 32 bytes of this
     * digest, returning it and the 64-byte ed25519 signature in signature[].
     * This results in a signature that verifies both the signer's authenticity
     * and the integrity of the message.
     *
     * This is based on the original ed25519 code from NaCl and the SUPERCOP
     * cipher benchmark suite, but with the modification that it always
     * produces a signature of fixed 96-byte length based on the hash of an
     * arbitrary-length message.
     *
     * @param myPrivate My private key
     * @param myPublic My public key
     * @param msg Message to sign
     * @param len Length of message in bytes
     * @param signature Buffer to fill with signature -- MUST be 96 bytes in length
     */
    static void sign(const Private& myPrivate, const Public& myPublic, const void* msg, unsigned int len, void* signature);
    static inline void sign(const Pair& mine, const void* msg, unsigned int len, void* signature)
    {
        sign(mine.priv, mine.pub, msg, len, signature);
    }

    /**
     * Sign a message with a sender's key pair
     *
     * @param myPrivate My private key
     * @param myPublic My public key
     * @param msg Message to sign
     * @param len Length of message in bytes
     * @return Signature
     */
    static inline Signature sign(const Private& myPrivate, const Public& myPublic, const void* msg, unsigned int len)
    {
        Signature sig;
        sign(myPrivate, myPublic, msg, len, sig.data);
        return sig;
    }
    static inline Signature sign(const Pair& mine, const void* msg, unsigned int len)
    {
        Signature sig;
        sign(mine.priv, mine.pub, msg, len, sig.data);
        return sig;
    }

    /**
     * Verify a message's signature
     *
     * @param their Public key to verify against
     * @param msg Message to verify signature integrity against
     * @param len Length of message in bytes
     * @param signature 96-byte signature
     * @return True if signature is valid and the message is authentic and unmodified
     */
    static bool verify(const Public& their, const void* msg, unsigned int len, const void* signature);

    /**
     * Verify a message's signature
     *
     * @param their Public key to verify against
     * @param msg Message to verify signature integrity against
     * @param len Length of message in bytes
     * @param signature 96-byte signature
     * @return True if signature is valid and the message is authentic and unmodified
     */
    static inline bool verify(const Public& their, const void* msg, unsigned int len, const Signature& signature)
    {
        return verify(their, msg, len, signature.data);
    }

  private:
    // derive first 32 bytes of kp.pub from first 32 bytes of kp.priv
    // this is the ECDH key
    static void _calcPubDH(Pair& kp);

    // derive 2nd 32 bytes of kp.pub from 2nd 32 bytes of kp.priv
    // this is the Ed25519 sign/verify key
    static void _calcPubED(Pair& kp);
};

}   // namespace ZeroTier

#endif

#endif
