/*
 * Copyright (c)2019 ZeroTier, Inc.
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file in the project's root directory.
 *
 * Change Date: 2023-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2.0 of the Apache License.
 */
/****/

#ifndef ZT_AES_HPP
#define ZT_AES_HPP

#include "Constants.hpp"
#include "Utils.hpp"
#include "SHA512.hpp"

#if (defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(__AMD64) || defined(__AMD64__) || defined(_M_X64))

#include <wmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>

#define ZT_AES_AESNI 1

// AES-aesni.c
extern "C" void zt_crypt_ctr_aesni(const __m128i key[14],const uint8_t iv[16],const uint8_t *in,unsigned int len,uint8_t *out);

#endif // x64

#define ZT_AES_KEY_SIZE 32
#define ZT_AES_BLOCK_SIZE 16

namespace ZeroTier {

/**
 * AES-256 and pals
 */
class AES
{
public:
	/**
	 * This will be true if your platform's type of AES acceleration is supported on this machine
	 */
	static const bool HW_ACCEL;

	ZT_ALWAYS_INLINE AES() {}
	ZT_ALWAYS_INLINE AES(const uint8_t key[32]) { this->init(key); }
	ZT_ALWAYS_INLINE ~AES() { Utils::burn(&_k,sizeof(_k)); }

	/**
	 * Set (or re-set) this AES256 cipher's key
	 */
	ZT_ALWAYS_INLINE void init(const uint8_t key[32])
	{
#ifdef ZT_AES_AESNI
		if (likely(HW_ACCEL)) {
			_init_aesni(key);
			return;
		}
#endif

		_initSW(key);
	}

	/**
	 * Encrypt a single AES block (ECB mode)
	 *
	 * @param in Input block
	 * @param out Output block (can be same as input)
	 */
	ZT_ALWAYS_INLINE void encrypt(const uint8_t in[16],uint8_t out[16]) const
	{
#ifdef ZT_AES_AESNI
		if (likely(HW_ACCEL)) {
			_encrypt_aesni(in,out);
			return;
		}
#endif

		_encryptSW(in,out);
	}

	/**
	 * Compute GMAC-AES256 (GCM without ciphertext)
	 *
	 * @param iv 96-bit IV
	 * @param in Input data
	 * @param len Length of input
	 * @param out 128-bit authorization tag from GMAC
	 */
	ZT_ALWAYS_INLINE void gmac(const uint8_t iv[12],const void *in,const unsigned int len,uint8_t out[16]) const
	{
#ifdef ZT_AES_AESNI
		if (likely(HW_ACCEL)) {
			_gmac_aesni(iv,(const uint8_t *)in,len,out);
			return;
		}
#endif

		_gmacSW(iv,(const uint8_t *)in,len,out);
	}

	/**
	 * Encrypt or decrypt (they're the same) using AES256-CTR
	 *
	 * The counter here is a 128-bit big-endian that starts at the IV. The code only
	 * increments the least significant 64 bits, making it only safe to use for a
	 * maximum of 2^64-1 bytes (much larger than we ever do).
	 *
	 * @param iv 128-bit CTR IV
	 * @param in Input plaintext or ciphertext
	 * @param len Length of input
	 * @param out Output plaintext or ciphertext
	 */
	ZT_ALWAYS_INLINE void ctr(const uint8_t iv[16],const void *in,unsigned int len,void *out) const
	{
#ifdef ZT_AES_AESNI
		if (likely(HW_ACCEL)) {
			zt_crypt_ctr_aesni(_k.ni.k,iv,(const uint8_t *)in,len,(uint8_t *)out);
			return;
		}
#endif

		uint64_t ctr[2],cenc[2];
		memcpy(ctr,iv,16);
		uint64_t bctr = Utils::ntoh(ctr[1]);

		const uint8_t *i = (const uint8_t *)in;
		uint8_t *o = (uint8_t *)out;

		while (len >= 16) {
			_encryptSW((const uint8_t *)ctr,(uint8_t *)cenc);
			ctr[1] = Utils::hton(++bctr);
			for(unsigned int k=0;k<16;++k)
				*(o++) = *(i++) ^ ((uint8_t *)cenc)[k];
			len -= 16;
		}

		if (len) {
			_encryptSW((const uint8_t *)ctr,(uint8_t *)cenc);
			for(unsigned int k=0;k<len;++k)
				*(o++) = *(i++) ^ ((uint8_t *)cenc)[k];
		}
	}

	/**
	 * Perform AES-GMAC-SIV encryption
	 *
	 * This is basically AES-CMAC-SIV but with GMAC in place of CMAC after
	 * GMAC is run through AES as a keyed hash to make it behave like a
	 * proper PRF.
	 *
	 * See: https://github.com/miscreant/meta/wiki/AES-SIV
	 *
	 * The advantage is that this can be described in terms of FIPS and NSA
	 * ceritifable primitives that are present in FIPS-compliant crypto
	 * modules.
	 *
	 * The extra AES-ECB (keyed hash) encryption of the AES-CTR IV prior
	 * to use makes the IV itself a secret. This is not strictly necessary
	 * but comes at little cost.
	 *
	 * This code is ZeroTier-specific in a few ways, like the way the IV
	 * is specified, but would not be hard to generalize.
	 *
	 * @param k1 GMAC key
	 * @param k2 GMAC auth tag keyed hash key
	 * @param k3 CTR IV keyed hash key
	 * @param k4 AES-CTR key
	 * @param iv 64-bit packet IV
	 * @param pc Packet characteristics byte
	 * @param in Message plaintext
	 * @param len Length of plaintext
	 * @param out Output buffer to receive ciphertext
	 * @param tag Output buffer to receive 64-bit authentication tag
	 */
	static ZT_ALWAYS_INLINE void gmacSivEncrypt(const AES &k1,const AES &k2,const AES &k3,const AES &k4,const uint8_t iv[8],const uint8_t pc,const void *in,const unsigned int len,void *out,uint8_t tag[8])
	{
#ifdef __GNUC__
		uint8_t __attribute__ ((aligned (16))) miv[12];
		uint8_t __attribute__ ((aligned (16))) ctrIv[16];
#else
		uint8_t miv[12];
		uint8_t ctrIv[16];
#endif

		// GMAC IV is 64-bit packet IV followed by other packet attributes to extend to 96 bits
#ifndef __GNUC__
		for(unsigned int i=0;i<8;++i) miv[i] = iv[i];
#else
		*((uint64_t *)miv) = *((const uint64_t *)iv);
#endif
		miv[8] = pc;
		miv[9] = (uint8_t)(len >> 16);
		miv[10] = (uint8_t)(len >> 8);
		miv[11] = (uint8_t)len;

		// Compute auth tag: AES-ECB[k2](GMAC[k1](miv,plaintext))[0:8]
		k1.gmac(miv,in,len,ctrIv);
		k2.encrypt(ctrIv,ctrIv); // ECB mode encrypt step is because GMAC is not a PRF
#ifdef ZT_NO_TYPE_PUNNING
		for(unsigned int i=0;i<8;++i) tag[i] = ctrIv[i];
#else
		*((uint64_t *)tag) = *((uint64_t *)ctrIv);
#endif

		// Create synthetic CTR IV: AES-ECB[k3](TAG | MIV[0:4] | (MIV[4:8] XOR MIV[8:12]))
#ifndef __GNUC__
		for(unsigned int i=0;i<4;++i) ctrIv[i+8] = miv[i];
		for(unsigned int i=4;i<8;++i) ctrIv[i+8] = miv[i] ^ miv[i+4];
#else
		((uint32_t *)ctrIv)[2] = ((const uint32_t *)miv)[0];
		((uint32_t *)ctrIv)[3] = ((const uint32_t *)miv)[1] ^ ((const uint32_t *)miv)[2];
#endif
		k3.encrypt(ctrIv,ctrIv);

		// Encrypt with AES[k4]-CTR
		k4.ctr(ctrIv,in,len,out);
	}

	/**
	 * Decrypt a message encrypted with AES-GMAC-SIV and check its authenticity
	 *
	 * @param k1 GMAC key
	 * @param k2 GMAC auth tag keyed hash key
	 * @param k3 CTR IV keyed hash key
	 * @param k4 AES-CTR key
	 * @param iv 64-bit message IV
	 * @param pc Packet characteristics byte
	 * @param in Message ciphertext
	 * @param len Length of ciphertext
	 * @param out Output buffer to receive plaintext
	 * @param tag Authentication tag supplied with message
	 * @return True if authentication tags match and message appears authentic
	 */
	static ZT_ALWAYS_INLINE bool gmacSivDecrypt(const AES &k1,const AES &k2,const AES &k3,const AES &k4,const uint8_t iv[8],const uint8_t pc,const void *in,const unsigned int len,void *out,const uint8_t tag[8])
	{
#ifdef __GNUC__
		uint8_t __attribute__ ((aligned (16))) miv[12];
		uint8_t __attribute__ ((aligned (16))) ctrIv[16];
		uint8_t __attribute__ ((aligned (16))) gmacOut[16];
#else
		uint8_t miv[12];
		uint8_t ctrIv[16];
		uint8_t gmacOut[16];
#endif

		// Extend packet IV to 96-bit message IV using direction byte and message length
#ifdef ZT_NO_TYPE_PUNNING
		for(unsigned int i=0;i<8;++i) miv[i] = iv[i];
#else
		*((uint64_t *)miv) = *((const uint64_t *)iv);
#endif
		miv[8] = pc;
		miv[9] = (uint8_t)(len >> 16);
		miv[10] = (uint8_t)(len >> 8);
		miv[11] = (uint8_t)len;

		// Recover synthetic and secret CTR IV from auth tag and packet IV
#ifndef __GNUC__
		for(unsigned int i=0;i<8;++i) ctrIv[i] = tag[i];
		for(unsigned int i=0;i<4;++i) ctrIv[i+8] = miv[i];
		for(unsigned int i=4;i<8;++i) ctrIv[i+8] = miv[i] ^ miv[i+4];
#else
		*((uint64_t *)ctrIv) = *((const uint64_t *)tag);
		((uint32_t *)ctrIv)[2] = ((const uint32_t *)miv)[0];
		((uint32_t *)ctrIv)[3] = ((const uint32_t *)miv)[1] ^ ((const uint32_t *)miv)[2];
#endif
		k3.encrypt(ctrIv,ctrIv);

		// Decrypt with AES[k4]-CTR
		k4.ctr(ctrIv,in,len,out);

		// Compute AES[k2](GMAC[k1](iv,plaintext))
		k1.gmac(miv,out,len,gmacOut);
		k2.encrypt(gmacOut,gmacOut);

		// Check that packet's auth tag matches first 64 bits of AES(GMAC)
#ifdef ZT_NO_TYPE_PUNNING
		return Utils::secureEq(gmacOut,tag,8);
#else
		return (*((const uint64_t *)gmacOut) == *((const uint64_t *)tag));
#endif
	}

	/**
	 * Use KBKDF with HMAC-SHA-384 to derive four sub-keys for AES-GMAC-SIV from a single master key
	 *
	 * See section 5.1 at https://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-108.pdf
	 *
	 * @param masterKey Master 256-bit key
	 * @param k1 GMAC key
	 * @param k2 GMAC auth tag keyed hash key
	 * @param k3 CTR IV keyed hash key
	 * @param k4 AES-CTR key
	 */
	static ZT_ALWAYS_INLINE void initGmacCtrKeys(const uint8_t masterKey[32],AES &k1,AES &k2,AES &k3,AES &k4)
	{
		uint8_t k[32];
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K1,0,0,k);
		k1.init(k);
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K2,0,0,k);
		k2.init(k);
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K3,0,0,k);
		k3.init(k);
		KBKDFHMACSHA384(masterKey,ZT_PROTO_KBKDF_LABEL_KEY_USE_AES_GMAC_SIV_K4,0,0,k);
		k4.init(k);
	}

private:
	static const uint32_t Te0[256];
	static const uint32_t Te1[256];
	static const uint32_t Te2[256];
	static const uint32_t Te3[256];
	static const uint32_t rcon[10];

	void _initSW(const uint8_t key[32]);
	void _encryptSW(const uint8_t in[16],uint8_t out[16]) const;
	void _gmacSW(const uint8_t iv[12],const uint8_t *in,unsigned int len,uint8_t out[16]) const;

	/**************************************************************************/
	union {
#ifdef ZT_AES_ARMNEON
		struct {
			uint32x4_t k[15];
		} neon;
#endif
#ifdef ZT_AES_AESNI
		struct {
			__m128i k[15];
			__m128i h,hh,hhh,hhhh;
		} ni;
#endif
		struct {
			uint64_t h[2];
			uint32_t ek[60];
		} sw;
	} _k;
	/**************************************************************************/

#ifdef ZT_AES_ARMNEON /******************************************************/
	static inline void _aes_256_expAssist_armneon(uint32x4_t prev1,uint32x4_t prev2,uint32_t rcon,uint32x4_t *e1,uint32x4_t *e2)
	{
		uint32_t round1[4], round2[4], prv1[4], prv2[4];
		vst1q_u32(prv1, prev1);
		vst1q_u32(prv2, prev2);
		round1[0] = sub_word(rot_word(prv2[3])) ^ rcon ^ prv1[0];
		round1[1] = sub_word(rot_word(round1[0])) ^ rcon ^ prv1[1];
		round1[2] = sub_word(rot_word(round1[1])) ^ rcon ^ prv1[2];
		round1[3] = sub_word(rot_word(round1[2])) ^ rcon ^ prv1[3];
		round2[0] = sub_word(rot_word(round1[3])) ^ rcon ^ prv2[0];
		round2[1] = sub_word(rot_word(round2[0])) ^ rcon ^ prv2[1];
		round2[2] = sub_word(rot_word(round2[1])) ^ rcon ^ prv2[2];
		round2[3] = sub_word(rot_word(round2[2])) ^ rcon ^ prv2[3];
		*e1 = vld1q_u3(round1);
		*e2 = vld1q_u3(round2);
		//uint32x4_t expansion[2] = {vld1q_u3(round1), vld1q_u3(round2)};
		//return expansion;
	}
	inline void _init_armneon(uint8x16_t encKey)
	{
		uint32x4_t *schedule = _k.neon.k;
		uint32x4_t e1,e2;
		(*schedule)[0] = vld1q_u32(encKey);
		(*schedule)[1] = vld1q_u32(encKey + 16);
		_aes_256_expAssist_armneon((*schedule)[0],(*schedule)[1],0x01,&e1,&e2);
		(*schedule)[2] = e1; (*schedule)[3] = e2;
		_aes_256_expAssist_armneon((*schedule)[2],(*schedule)[3],0x01,&e1,&e2);
		(*schedule)[4] = e1; (*schedule)[5] = e2;
		_aes_256_expAssist_armneon((*schedule)[4],(*schedule)[5],0x01,&e1,&e2);
		(*schedule)[6] = e1; (*schedule)[7] = e2;
		_aes_256_expAssist_armneon((*schedule)[6],(*schedule)[7],0x01,&e1,&e2);
		(*schedule)[8] = e1; (*schedule)[9] = e2;
		_aes_256_expAssist_armneon((*schedule)[8],(*schedule)[9],0x01,&e1,&e2);
		(*schedule)[10] = e1; (*schedule)[11] = e2;
		_aes_256_expAssist_armneon((*schedule)[10],(*schedule)[11],0x01,&e1,&e2);
		(*schedule)[12] = e1; (*schedule)[13] = e2;
		_aes_256_expAssist_armneon((*schedule)[12],(*schedule)[13],0x01,&e1,&e2);
		(*schedule)[14] = e1;
		/*
		doubleRound = _aes_256_expAssist_armneon((*schedule)[0], (*schedule)[1], 0x01);
		(*schedule)[2] = doubleRound[0];
		(*schedule)[3] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[2], (*schedule)[3], 0x02);
		(*schedule)[4] = doubleRound[0];
		(*schedule)[5] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[4], (*schedule)[5], 0x04);
		(*schedule)[6] = doubleRound[0];
		(*schedule)[7] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[6], (*schedule)[7], 0x08);
		(*schedule)[8] = doubleRound[0];
		(*schedule)[9] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[8], (*schedule)[9], 0x10);
		(*schedule)[10] = doubleRound[0];
		(*schedule)[11] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[10], (*schedule)[11], 0x20);
		(*schedule)[12] = doubleRound[0];
		(*schedule)[13] = doubleRound[1];
		doubleRound = _aes_256_expAssist_armneon((*schedule)[12], (*schedule)[13], 0x40);
		(*schedule)[14] = doubleRound[0];
		*/
	}

	inline void _encrypt_armneon(uint8x16_t *data) const
	{
		*data = veorq_u8(*data, _k.neon.k[0]);
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[1]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[2]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[3]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[4]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[5]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[6]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[7]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[8]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[9]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[10]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[11]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[12]));
		*data = vaesmcq_u8(vaeseq_u8(*data, (uint8x16_t)_k.neon.k[13]));
		*data = vaeseq_u8(*data, _k.neon.k[14]);
	}
#endif /*********************************************************************/

#ifdef ZT_AES_AESNI /********************************************************/
	static ZT_ALWAYS_INLINE __m128i _init256_1_aesni(__m128i a,__m128i b)
	{
		__m128i x,y;
		b = _mm_shuffle_epi32(b,0xff);
		y = _mm_slli_si128(a,0x04);
		x = _mm_xor_si128(a,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		x = _mm_xor_si128(x,b);
		return x;
	}
	static ZT_ALWAYS_INLINE __m128i _init256_2_aesni(__m128i a,__m128i b)
	{
		__m128i x,y,z;
		y = _mm_aeskeygenassist_si128(a,0x00);
		z = _mm_shuffle_epi32(y,0xaa);
		y = _mm_slli_si128(b,0x04);
		x = _mm_xor_si128(b,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		y = _mm_slli_si128(y,0x04);
		x = _mm_xor_si128(x,y);
		x = _mm_xor_si128(x,z);
		return x;
	}
	ZT_ALWAYS_INLINE void _init_aesni(const uint8_t key[32])
	{
		__m128i t1,t2;
		_k.ni.k[0] = t1 = _mm_loadu_si128((const __m128i *)key);
		_k.ni.k[1] = t2 = _mm_loadu_si128((const __m128i *)(key+16));
		_k.ni.k[2] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x01));
		_k.ni.k[3] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[4] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x02));
		_k.ni.k[5] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[6] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x04));
		_k.ni.k[7] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[8] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x08));
		_k.ni.k[9] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[10] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x10));
		_k.ni.k[11] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[12] = t1 = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x20));
		_k.ni.k[13] = t2 = _init256_2_aesni(t1,t2);
		_k.ni.k[14] = _init256_1_aesni(t1,_mm_aeskeygenassist_si128(t2,0x40));

		__m128i h = _mm_xor_si128(_mm_setzero_si128(),_k.ni.k[0]);
		h = _mm_aesenc_si128(h,_k.ni.k[1]);
		h = _mm_aesenc_si128(h,_k.ni.k[2]);
		h = _mm_aesenc_si128(h,_k.ni.k[3]);
		h = _mm_aesenc_si128(h,_k.ni.k[4]);
		h = _mm_aesenc_si128(h,_k.ni.k[5]);
		h = _mm_aesenc_si128(h,_k.ni.k[6]);
		h = _mm_aesenc_si128(h,_k.ni.k[7]);
		h = _mm_aesenc_si128(h,_k.ni.k[8]);
		h = _mm_aesenc_si128(h,_k.ni.k[9]);
		h = _mm_aesenc_si128(h,_k.ni.k[10]);
		h = _mm_aesenc_si128(h,_k.ni.k[11]);
		h = _mm_aesenc_si128(h,_k.ni.k[12]);
		h = _mm_aesenc_si128(h,_k.ni.k[13]);
		h = _mm_aesenclast_si128(h,_k.ni.k[14]);

		const __m128i shuf = _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
		__m128i hswap = _mm_shuffle_epi8(h,shuf);
		__m128i hh = _mult_block_aesni(shuf,hswap,h);
		__m128i hhh = _mult_block_aesni(shuf,hswap,hh);
		__m128i hhhh = _mult_block_aesni(shuf,hswap,hhh);
		_k.ni.h = hswap;
		_k.ni.hh = _mm_shuffle_epi8(hh,shuf);
		_k.ni.hhh = _mm_shuffle_epi8(hhh,shuf);
		_k.ni.hhhh = _mm_shuffle_epi8(hhhh,shuf);
	}

	ZT_ALWAYS_INLINE void _encrypt_aesni(const void *in,void *out) const
	{
		__m128i tmp;
		tmp = _mm_loadu_si128((const __m128i *)in);
		tmp = _mm_xor_si128(tmp,_k.ni.k[0]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[1]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[2]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[3]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[4]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[5]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[6]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[7]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[8]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[9]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[10]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[11]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[12]);
		tmp = _mm_aesenc_si128(tmp,_k.ni.k[13]);
		_mm_storeu_si128((__m128i *)out,_mm_aesenclast_si128(tmp,_k.ni.k[14]));
	}

	static ZT_ALWAYS_INLINE __m128i _mult_block_aesni(__m128i shuf,__m128i h,__m128i y)
	{
		y = _mm_shuffle_epi8(y,shuf);
		__m128i t1 = _mm_clmulepi64_si128(h,y,0x00);
		__m128i t2 = _mm_clmulepi64_si128(h,y,0x01);
		__m128i t3 = _mm_clmulepi64_si128(h,y,0x10);
		__m128i t4 = _mm_clmulepi64_si128(h,y,0x11);
		t2 = _mm_xor_si128(t2,t3);
		t3 = _mm_slli_si128(t2,8);
		t2 = _mm_srli_si128(t2,8);
		t1 = _mm_xor_si128(t1,t3);
		t4 = _mm_xor_si128(t4,t2);
		__m128i t5 = _mm_srli_epi32(t1,31);
		t1 = _mm_slli_epi32(t1,1);
		__m128i t6 = _mm_srli_epi32(t4,31);
		t4 = _mm_slli_epi32(t4,1);
		t3 = _mm_srli_si128(t5,12);
		t6 = _mm_slli_si128(t6,4);
		t5 = _mm_slli_si128(t5,4);
		t1 = _mm_or_si128(t1,t5);
		t4 = _mm_or_si128(t4,t6);
		t4 = _mm_or_si128(t4,t3);
		t5 = _mm_slli_epi32(t1,31);
		t6 = _mm_slli_epi32(t1,30);
		t3 = _mm_slli_epi32(t1,25);
		t5 = _mm_xor_si128(t5,t6);
		t5 = _mm_xor_si128(t5,t3);
		t6 = _mm_srli_si128(t5,4);
		t4 = _mm_xor_si128(t4,t6);
		t5 = _mm_slli_si128(t5,12);
		t1 = _mm_xor_si128(t1,t5);
		t4 = _mm_xor_si128(t4,t1);
		t5 = _mm_srli_epi32(t1,1);
		t2 = _mm_srli_epi32(t1,2);
		t3 = _mm_srli_epi32(t1,7);
		t4 = _mm_xor_si128(t4,t2);
		t4 = _mm_xor_si128(t4,t3);
		t4 = _mm_xor_si128(t4,t5);
		return _mm_shuffle_epi8(t4,shuf);
	}
	static ZT_ALWAYS_INLINE __m128i _ghash_aesni(__m128i shuf,__m128i h,__m128i y,__m128i x) { return _mult_block_aesni(shuf,h,_mm_xor_si128(y,x)); }

	ZT_ALWAYS_INLINE void _gmac_aesni(const uint8_t iv[12],const uint8_t *in,const unsigned int len,uint8_t out[16]) const
	{
		const __m128i *const ab = (const __m128i *)in;
		const unsigned int blocks = len / 16;
		const unsigned int pblocks = blocks - (blocks % 4);
		const unsigned int rem = len % 16;

		const __m128i shuf = _mm_set_epi8(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15);
		__m128i y = _mm_setzero_si128();
		unsigned int i = 0;
		for (;i<pblocks;i+=4) {
			__m128i d1 = _mm_shuffle_epi8(_mm_xor_si128(y,_mm_loadu_si128(ab + i + 0)),shuf);
			__m128i d2 = _mm_shuffle_epi8(_mm_loadu_si128(ab + i + 1),shuf);
			__m128i d3 = _mm_shuffle_epi8(_mm_loadu_si128(ab + i + 2),shuf);
			__m128i d4 = _mm_shuffle_epi8(_mm_loadu_si128(ab + i + 3),shuf);
			_mm_prefetch(ab + i + 4,_MM_HINT_T0);
			__m128i t0 = _mm_clmulepi64_si128(_k.ni.hhhh,d1,0x00);
			__m128i t1 = _mm_clmulepi64_si128(_k.ni.hhh,d2,0x00);
			__m128i t2 = _mm_clmulepi64_si128(_k.ni.hh,d3,0x00);
			__m128i t3 = _mm_clmulepi64_si128(_k.ni.h,d4,0x00);
			__m128i t8 = _mm_xor_si128(t0,t1);
			t8 = _mm_xor_si128(t8,t2);
			t8 = _mm_xor_si128(t8,t3);
			__m128i t4 = _mm_clmulepi64_si128(_k.ni.hhhh,d1,0x11);
			__m128i t5 = _mm_clmulepi64_si128(_k.ni.hhh,d2,0x11);
			__m128i t6 = _mm_clmulepi64_si128(_k.ni.hh,d3,0x11);
			__m128i t7 = _mm_clmulepi64_si128(_k.ni.h,d4,0x11);
			__m128i t9 = _mm_xor_si128(t4,t5);
			t9 = _mm_xor_si128(t9,t6);
			t9 = _mm_xor_si128(t9,t7);
			t0 = _mm_shuffle_epi32(_k.ni.hhhh,78);
			t4 = _mm_shuffle_epi32(d1,78);
			t0 = _mm_xor_si128(t0,_k.ni.hhhh);
			t4 = _mm_xor_si128(t4,d1);
			t1 = _mm_shuffle_epi32(_k.ni.hhh,78);
			t5 = _mm_shuffle_epi32(d2,78);
			t1 = _mm_xor_si128(t1,_k.ni.hhh);
			t5 = _mm_xor_si128(t5,d2);
			t2 = _mm_shuffle_epi32(_k.ni.hh,78);
			t6 = _mm_shuffle_epi32(d3,78);
			t2 = _mm_xor_si128(t2,_k.ni.hh);
			t6 = _mm_xor_si128(t6,d3);
			t3 = _mm_shuffle_epi32(_k.ni.h,78);
			t7 = _mm_shuffle_epi32(d4,78);
			t3 = _mm_xor_si128(t3,_k.ni.h);
			t7 = _mm_xor_si128(t7,d4);
			t0 = _mm_clmulepi64_si128(t0,t4,0x00);
			t1 = _mm_clmulepi64_si128(t1,t5,0x00);
			t2 = _mm_clmulepi64_si128(t2,t6,0x00);
			t3 = _mm_clmulepi64_si128(t3,t7,0x00);
			t0 = _mm_xor_si128(t0,t8);
			t0 = _mm_xor_si128(t0,t9);
			t0 = _mm_xor_si128(t1,t0);
			t0 = _mm_xor_si128(t2,t0);
			t0 = _mm_xor_si128(t3,t0);
			t4 = _mm_slli_si128(t0,8);
			t0 = _mm_srli_si128(t0,8);
			t3 = _mm_xor_si128(t4,t8);
			t6 = _mm_xor_si128(t0,t9);
			t7 = _mm_srli_epi32(t3,31);
			t8 = _mm_srli_epi32(t6,31);
			t3 = _mm_slli_epi32(t3,1);
			t6 = _mm_slli_epi32(t6,1);
			t9 = _mm_srli_si128(t7,12);
			t8 = _mm_slli_si128(t8,4);
			t7 = _mm_slli_si128(t7,4);
			t3 = _mm_or_si128(t3,t7);
			t6 = _mm_or_si128(t6,t8);
			t6 = _mm_or_si128(t6,t9);
			t7 = _mm_slli_epi32(t3,31);
			t8 = _mm_slli_epi32(t3,30);
			t9 = _mm_slli_epi32(t3,25);
			t7 = _mm_xor_si128(t7,t8);
			t7 = _mm_xor_si128(t7,t9);
			t8 = _mm_srli_si128(t7,4);
			t7 = _mm_slli_si128(t7,12);
			t3 = _mm_xor_si128(t3,t7);
			t2 = _mm_srli_epi32(t3,1);
			t4 = _mm_srli_epi32(t3,2);
			t5 = _mm_srli_epi32(t3,7);
			t2 = _mm_xor_si128(t2,t4);
			t2 = _mm_xor_si128(t2,t5);
			t2 = _mm_xor_si128(t2,t8);
			t3 = _mm_xor_si128(t3,t2);
			t6 = _mm_xor_si128(t6,t3);
			y = _mm_shuffle_epi8(t6,shuf);
		}

		for (;i<blocks;++i)
			y = _ghash_aesni(shuf,_k.ni.h,y,_mm_loadu_si128(ab + i));

		if (rem) {
			__m128i last = _mm_setzero_si128();
			memcpy(&last,ab + blocks,rem);
			y = _ghash_aesni(shuf,_k.ni.h,y,last);
		}

		y = _ghash_aesni(shuf,_k.ni.h,y,_mm_set_epi64((__m64)0LL,(__m64)Utils::hton((uint64_t)len * (uint64_t)8)));

		__m128i t = _mm_xor_si128(_mm_set_epi32(0x01000000,(int)*((const uint32_t *)(iv+8)),(int)*((const uint32_t *)(iv+4)),(int)*((const uint32_t *)(iv))),_k.ni.k[0]);
		t = _mm_aesenc_si128(t,_k.ni.k[1]);
		t = _mm_aesenc_si128(t,_k.ni.k[2]);
		t = _mm_aesenc_si128(t,_k.ni.k[3]);
		t = _mm_aesenc_si128(t,_k.ni.k[4]);
		t = _mm_aesenc_si128(t,_k.ni.k[5]);
		t = _mm_aesenc_si128(t,_k.ni.k[6]);
		t = _mm_aesenc_si128(t,_k.ni.k[7]);
		t = _mm_aesenc_si128(t,_k.ni.k[8]);
		t = _mm_aesenc_si128(t,_k.ni.k[9]);
		t = _mm_aesenc_si128(t,_k.ni.k[10]);
		t = _mm_aesenc_si128(t,_k.ni.k[11]);
		t = _mm_aesenc_si128(t,_k.ni.k[12]);
		t = _mm_aesenc_si128(t,_k.ni.k[13]);
		t = _mm_aesenclast_si128(t,_k.ni.k[14]);
		_mm_storeu_si128((__m128i *)out,_mm_xor_si128(y,t));
	}
#endif /* ZT_AES_AESNI ******************************************************/
};

} // namespace ZeroTier

#endif
