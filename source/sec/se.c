/*
 * Copyright (c) 2018 naehrwert
 * Copyright (c) 2018 CTCaer
 * Copyright (c) 2018 Atmosphère-NX
 * Copyright (c) 2019 shchmue
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "../sec/se.h"
#include "../mem/heap.h"
#include "../soc/bpmp.h"
#include "../soc/t210.h"
#include "../sec/se_t210.h"
#include "../utils/util.h"

typedef struct _se_ll_t
{
	vu32 num;
	vu32 addr;
	vu32 size;
} se_ll_t;

static u32 _se_rsa_mod_sizes[TEGRA_SE_RSA_KEYSLOT_COUNT];
static u32 _se_rsa_exp_sizes[TEGRA_SE_RSA_KEYSLOT_COUNT];

static void _gf256_mul_x(void *block)
{
	u8 *pdata = (u8 *)block;
	u32 carry = 0;

	for (int i = 0xF; i >= 0; i--)
	{
		u8 b = pdata[i];
		pdata[i] = (b << 1) | carry;
		carry = b >> 7;
	}

	if (carry)
		pdata[0xF] ^= 0x87;
}

static void _gf256_mul_x_le(void *block)
{
	u8 *pdata = (u8 *)block;
	u32 carry = 0;

	for (u32 i = 0; i < 0x10; i++)
	{
		u8 b = pdata[i];
		pdata[i] = (b << 1) | carry;
		carry = b >> 7;
	}

	if (carry)
		pdata[0x0] ^= 0x87;
}

static void _se_ll_init(se_ll_t *ll, u32 addr, u32 size)
{
	ll->num = 0;
	ll->addr = addr;
	ll->size = size;
}

static void _se_ll_set(se_ll_t *dst, se_ll_t *src)
{
	SE(SE_IN_LL_ADDR_REG_OFFSET) = (u32)src;
	SE(SE_OUT_LL_ADDR_REG_OFFSET) = (u32)dst;
}

static int _se_wait()
{
	while (!(SE(SE_INT_STATUS_REG_OFFSET) & SE_INT_OP_DONE(INT_SET)))
		;
	if (SE(SE_INT_STATUS_REG_OFFSET) & SE_INT_ERROR(INT_SET) ||
		SE(SE_STATUS_0) & 3 ||
		SE(SE_ERR_STATUS_0) != 0)
		return 0;
	return 1;
}

static int _se_execute(u32 op, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	se_ll_t *ll_dst = (se_ll_t *)0xECFFFFE0, *ll_src = (se_ll_t *)0xECFFFFF0;

	if (dst)
	{
		_se_ll_init(ll_dst, (u32)dst, dst_size);
	}

	if (src)
	{
		_se_ll_init(ll_src, (u32)src, src_size);
	}

	_se_ll_set(ll_dst, ll_src);

	SE(SE_ERR_STATUS_0) = SE(SE_ERR_STATUS_0);
	SE(SE_INT_STATUS_REG_OFFSET) = SE(SE_INT_STATUS_REG_OFFSET);

	bpmp_mmu_maintenance(BPMP_MMU_MAINT_CLN_INV_WAY);

	SE(SE_OPERATION_REG_OFFSET) = SE_OPERATION(op);
	int res = _se_wait();

	bpmp_mmu_maintenance(BPMP_MMU_MAINT_CLN_INV_WAY);

	return res;
}

static int _se_execute_one_block(u32 op, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	if (!src || !dst)
		return 0;

	u8 *block = (u8 *)malloc(0x10);
	memset(block, 0, 0x10);

	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;

	memcpy(block, src, src_size);
	int res = _se_execute(op, block, 0x10, block, 0x10);
	memcpy(dst, block, dst_size);

	free(block);
	return res;
}

static void _se_aes_ctr_set(void *ctr)
{
	u32 *data = (u32 *)ctr;
	for (u32 i = 0; i < 4; i++)
		SE(SE_CRYPTO_CTR_REG_OFFSET + 4 * i) = data[i];
}

void se_rsa_acc_ctrl(u32 rs, u32 flags)
{
	if (flags & 0x7F)
		SE(SE_RSA_KEYTABLE_ACCESS_REG_OFFSET + 4 * rs) = (((flags >> 4) & 4) | (flags & 3)) ^ 7;
	if (flags & 0x80)
		SE(SE_RSA_KEYTABLE_ACCESS_LOCK_OFFSET) &= ~(1 << rs);
}

// se_rsa_key_set() was derived from Atmosphère's set_rsa_keyslot
void se_rsa_key_set(u32 ks, const void *mod, u32 mod_size, const void *exp, u32 exp_size)
{
	u32 *data = (u32 *)mod;
	for (u32 i = 0; i < mod_size / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_MOD) | i;
		SE(SE_RSA_KEYTABLE_DATA) = byte_swap_32(data[mod_size / 4 - i - 1]);
	}

	data = (u32 *)exp;
	for (u32 i = 0; i < exp_size / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_EXP) | i;
		SE(SE_RSA_KEYTABLE_DATA) = byte_swap_32(data[exp_size / 4 - i - 1]);
	}

	_se_rsa_mod_sizes[ks] = mod_size;
	_se_rsa_exp_sizes[ks] = exp_size;
}

// se_rsa_key_clear() was derived from Atmosphère's clear_rsa_keyslot
void se_rsa_key_clear(u32 ks)
{
	for (u32 i = 0; i < TEGRA_SE_RSA2048_DIGEST_SIZE / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_MOD) | i;
		SE(SE_RSA_KEYTABLE_DATA) = 0;
	}
	for (u32 i = 0; i < TEGRA_SE_RSA2048_DIGEST_SIZE / 4; i++)
	{
		SE(SE_RSA_KEYTABLE_ADDR) = RSA_KEY_NUM(ks) | RSA_KEY_TYPE(RSA_KEY_TYPE_EXP) | i;
		SE(SE_RSA_KEYTABLE_DATA) = 0;
	}
}

// se_rsa_exp_mod() was derived from Atmosphère's se_synchronous_exp_mod and se_get_exp_mod_output
int se_rsa_exp_mod(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	int res;
	u8 stack_buf[TEGRA_SE_RSA2048_DIGEST_SIZE];

	for (u32 i = 0; i < src_size; i++)
		stack_buf[i] = *((u8 *)src + src_size - i - 1);

	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_RSA) | SE_CONFIG_DST(DST_RSAREG);
	SE(SE_RSA_CONFIG) = RSA_KEY_SLOT(ks);
	SE(SE_RSA_KEY_SIZE_REG_OFFSET) = (_se_rsa_mod_sizes[ks] >> 6) - 1;
	SE(SE_RSA_EXP_SIZE_REG_OFFSET) = _se_rsa_exp_sizes[ks] >> 2;

	res = _se_execute(OP_START, NULL, 0, stack_buf, src_size);

	// Copy output hash.
	u32 *dst32 = (u32 *)dst;
	for (u32 i = 0; i < dst_size / 4; i++)
		dst32[dst_size / 4 - i - 1] = byte_swap_32(SE(SE_RSA_OUTPUT + (i << 2)));

	return res;
}

void se_key_acc_ctrl(u32 ks, u32 flags)
{
	if (flags & 0x7F)
		SE(SE_KEY_TABLE_ACCESS_REG_OFFSET + 4 * ks) = ~flags;
	if (flags & 0x80)
		SE(SE_KEY_TABLE_ACCESS_LOCK_OFFSET) &= ~(1 << ks);
}

void se_aes_key_set(u32 ks, const void *key, u32 size)
{
	u32 *data = (u32 *)key;
	for (u32 i = 0; i < size / 4; i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | i;
		SE(SE_KEYTABLE_DATA0_REG_OFFSET) = data[i];
	}
}

void se_aes_key_read(u32 ks, void *key, u32 size)
{
	u32 *data = (u32 *)key;
	for (u32 i = 0; i < size / 4; i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | i;
		data[i] = SE(SE_KEYTABLE_DATA0_REG_OFFSET);
	}
}

void se_aes_key_clear(u32 ks)
{
	for (u32 i = 0; i < TEGRA_SE_AES_MAX_KEY_SIZE / 4; i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | i;
		SE(SE_KEYTABLE_DATA0_REG_OFFSET) = 0;
	}
}

void se_aes_key_iv_clear(u32 ks)
{
	for (u32 i = 0; i < TEGRA_SE_AES_MAX_KEY_SIZE / 4; i++)
	{
		SE(SE_KEYTABLE_REG_OFFSET) = SE_KEYTABLE_SLOT(ks) | 8 | i;
		SE(SE_KEYTABLE_DATA0_REG_OFFSET) = 0;
	}
}

int se_aes_unwrap_key(u32 ks_dst, u32 ks_src, const void *input)
{
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_DEC_ALG(ALG_AES_DEC) | SE_CONFIG_DST(DST_KEYTAB);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks_src) | SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;
	SE(SE_CRYPTO_KEYTABLE_DST_REG_OFFSET) = SE_CRYPTO_KEYTABLE_DST_KEY_INDEX(ks_dst);

	return _se_execute(OP_START, NULL, 0, input, 0x10);
}

int se_aes_crypt_ecb(u32 ks, u32 enc, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	if (enc)
	{
		SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_MEMORY);
		SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_CORE_SEL(CORE_ENCRYPT);
	}
	else
	{
		SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_DEC_ALG(ALG_AES_DEC) | SE_CONFIG_DST(DST_MEMORY);
		SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_CORE_SEL(CORE_DECRYPT);
	}
	SE(SE_BLOCK_COUNT_REG_OFFSET) = (src_size >> 4) - 1;
	return _se_execute(OP_START, dst, dst_size, src, src_size);
}

int se_aes_crypt_block_ecb(u32 ks, u32 enc, void *dst, const void *src)
{
	return se_aes_crypt_ecb(ks, enc, dst, 0x10, src, 0x10);
}

int se_aes_crypt_ctr(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size, void *ctr)
{
	SE(SE_SPARE_0_REG_OFFSET) = 1;
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_MEMORY);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | SE_CRYPTO_CORE_SEL(CORE_ENCRYPT) |
		SE_CRYPTO_XOR_POS(XOR_BOTTOM) | SE_CRYPTO_INPUT_SEL(INPUT_LNR_CTR) | SE_CRYPTO_CTR_VAL(1);
	_se_aes_ctr_set(ctr);

	u32 src_size_aligned = src_size & 0xFFFFFFF0;
	u32 src_size_delta = src_size & 0xF;

	if (src_size_aligned)
	{
		SE(SE_BLOCK_COUNT_REG_OFFSET) = (src_size >> 4) - 1;
		if (!_se_execute(OP_START, dst, dst_size, src, src_size_aligned))
			return 0;
	}

	if (src_size - src_size_aligned && src_size_aligned < dst_size)
		return _se_execute_one_block(OP_START, dst + src_size_aligned,
			MIN(src_size_delta, dst_size - src_size_aligned),
			src + src_size_aligned, src_size_delta);

	return 1;
}

int se_aes_xts_crypt_sec(u32 ks1, u32 ks2, u32 enc, u64 sec, void *dst, const void *src, u32 secsize)
{
	int res = 0;
	u8 *tweak = (u8 *)malloc(0x10);
	u8 *pdst = (u8 *)dst;
	u8 *psrc = (u8 *)src;

	//Generate tweak.
	for (int i = 0xF; i >= 0; i--)
	{
		tweak[i] = sec & 0xFF;
		sec >>= 8;
	}
	if (!se_aes_crypt_block_ecb(ks1, 1, tweak, tweak))
		goto out;

	//We are assuming a 0x10-aligned sector size in this implementation.
	for (u32 i = 0; i < secsize / 0x10; i++)
	{
		for (u32 j = 0; j < 0x10; j++)
			pdst[j] = psrc[j] ^ tweak[j];
		if (!se_aes_crypt_block_ecb(ks2, enc, pdst, pdst))
			goto out;
		for (u32 j = 0; j < 0x10; j++)
			pdst[j] = pdst[j] ^ tweak[j];
		_gf256_mul_x_le(tweak);
		psrc += 0x10;
		pdst += 0x10;
	}

	res = 1;

out:;
	free(tweak);
	return res;
}

int se_aes_xts_crypt(u32 ks1, u32 ks2, u32 enc, u64 sec, void *dst, const void *src, u32 secsize, u32 num_secs)
{
	u8 *pdst = (u8 *)dst;
	u8 *psrc = (u8 *)src;

	for (u32 i = 0; i < num_secs; i++)
		if (!se_aes_xts_crypt_sec(ks1, ks2, enc, sec + i, pdst + secsize * i, psrc + secsize * i, secsize))
			return 0;

	return 1;
}

// se_aes_cmac() was derived from Atmosphère's se_compute_aes_cmac
int se_aes_cmac(u32 ks, void *dst, u32 dst_size, const void *src, u32 src_size)
{
	int res = 0;
	u8 *key = (u8 *)calloc(0x10, 1);
	u8 *last_block = (u8 *)calloc(0x10, 1);

	// generate derived key
	if (!se_aes_crypt_block_ecb(ks, 1, key, key))
		goto out;
	_gf256_mul_x(key);
	if (src_size & 0xF)
		_gf256_mul_x(key);

	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_ALG(ALG_AES_ENC) | SE_CONFIG_DST(DST_HASHREG);
	SE(SE_CRYPTO_REG_OFFSET) = SE_CRYPTO_KEY_INDEX(ks) | 0x145;
	se_aes_key_iv_clear(ks);

	u32 num_blocks = (src_size + 0xf) >> 4;
	if (num_blocks > 1) {
		SE(SE_BLOCK_COUNT_REG_OFFSET) = num_blocks - 2;
		if (!_se_execute(OP_START, NULL, 0, src, src_size))
			goto out;
		SE(SE_CRYPTO_REG_OFFSET) |= SE_CRYPTO_IV_SEL(IV_UPDATED);
	}

	if (src_size & 0xf) {
		memcpy(last_block, src + (src_size & ~0xf), src_size & 0xf);
		last_block[src_size & 0xf] = 0x80;
	} else if (src_size >= 0x10) {
		memcpy(last_block, src + src_size - 0x10, 0x10);
	}

	for (u32 i = 0; i < 0x10; i++)
		last_block[i] ^= key[i];

	SE(SE_BLOCK_COUNT_REG_OFFSET) = 0;
	res = _se_execute(OP_START, NULL, 0, last_block, 0x10);

	u32 *dst32 = (u32 *)dst;
	for (u32 i = 0; i < (dst_size >> 2); i++)
		dst32[i] = SE(SE_HASH_RESULT_REG_OFFSET + (i << 2));

out:;
	free(key);
	free(last_block);
	return res;
}

// se_calc_sha256() was derived from Atmosphère's se_calculate_sha256.
int se_calc_sha256(void *dst, const void *src, u32 src_size)
{
	int res;
	// Setup config for SHA256, size = BITS(src_size).
	SE(SE_CONFIG_REG_OFFSET) = SE_CONFIG_ENC_MODE(MODE_SHA256) | SE_CONFIG_ENC_ALG(ALG_SHA) | SE_CONFIG_DST(DST_HASHREG);
	SE(SE_SHA_CONFIG_REG_OFFSET) = SHA_INIT_ENABLE;
	SE(SE_SHA_MSG_LENGTH_REG_OFFSET) = (u32)(src_size << 3);
	SE(SE_SHA_MSG_LENGTH_REG_OFFSET + 4 * 1) = 0;
	SE(SE_SHA_MSG_LENGTH_REG_OFFSET + 4 * 2) = 0;
	SE(SE_SHA_MSG_LENGTH_REG_OFFSET + 4 * 3) = 0;
	SE(SE_SHA_MSG_LEFT_REG_OFFSET) = (u32)(src_size << 3);
	SE(SE_SHA_MSG_LEFT_REG_OFFSET + 4 * 1) = 0;
	SE(SE_SHA_MSG_LEFT_REG_OFFSET + 4 * 2) = 0;
	SE(SE_SHA_MSG_LEFT_REG_OFFSET + 4 * 3) = 0;

	// Trigger the operation.
	res = _se_execute(OP_START, NULL, 0, src, src_size);

	// Copy output hash.
	u32 *dst32 = (u32 *)dst;
	for (u32 i = 0; i < 8; i++)
		dst32[i] = byte_swap_32(SE(SE_HASH_RESULT_REG_OFFSET + (i << 2)));

	return res;
}

int se_calc_hmac_sha256(void *dst, const void *src, u32 src_size, const void *key, u32 key_size) {
	int res = 0;
	u8 *secret = (u8 *)malloc(0x40);
	u8 *ipad = (u8 *)malloc(0x40 + src_size);
	u8 *opad = (u8 *)malloc(0x60);

	if (key_size > 0x40)
	{
		if (!se_calc_sha256(secret, key, key_size))
			goto out;
		memset(secret + 0x20, 0, 0x20);
	}
	else
	{
		memcpy(secret, key, key_size);
		memset(secret + key_size, 0, 0x40 - key_size);
	}

	u32 *secret32 = (u32 *)secret;
	u32 *ipad32 = (u32 *)ipad;
	u32 *opad32 = (u32 *)opad;
	for (u32 i = 0; i < 0x10; i++)
	{
		ipad32[i] = secret32[i] ^ 0x36363636;
		opad32[i] = secret32[i] ^ 0x5C5C5C5C;
	}

	memcpy(ipad + 0x40, src, src_size);
	if (!se_calc_sha256(dst, ipad, 0x40 + src_size))
		goto out;
	memcpy(opad + 0x40, dst, 0x20);
	if (!se_calc_sha256(dst, opad, 0x60))
		goto out;

	res = 1;

out:;
	free(secret);
	free(ipad);
	free(opad);
	return res;
}
