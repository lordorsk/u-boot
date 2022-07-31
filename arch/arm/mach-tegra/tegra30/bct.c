// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022, Ramin <raminterex@yahoo.com>
 * Copyright (c) 2022, Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <log.h>
#include <asm/arch-tegra/crypto.h>
#include "bct.h"
#include "uboot_aes.h"

/* SBK is device/board specific, insert your here instead of 0x00 */
static u8 sbk[AES128_KEY_LENGTH] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * \param bct		boot config table start in RAM
 * \param ect		bootloader start in RAM
 * \param ebt_size	bootloader file size in bytes
 */
int bct_patch(u8 *bct, u8 *ebt, u32 ebt_size)
{
	u8 ebt_hash[AES128_KEY_LENGTH] = { 0 };
	nvboot_config_table *bct_tbl = NULL;
	u8 *bct_hash = bct;
	int ret;

	bct += BCT_HASH;

	ret = decrypt_data_block(bct, BCT_LENGTH, sbk);
	if (ret)
		return 1;

	ebt_size = ((ebt_size / EBT_ALIGNMENT) + 1) * EBT_ALIGNMENT;

	ret = encrypt_data_block(ebt, ebt_size, sbk);
	if (ret)
		return 1;

	ret = sign_enc_data_block(ebt, ebt_size, ebt_hash, sbk);
	if (ret)
		return 1;

	bct_tbl = (nvboot_config_table *)bct;

	memcpy((u8 *)&bct_tbl->bootloader[0].crypto_hash,
		ebt_hash, NVBOOT_CMAC_AES_HASH_LENGTH * 4);
	bct_tbl->bootloader[0].entry_point = CONFIG_SPL_TEXT_BASE;
	bct_tbl->bootloader[0].load_addr = CONFIG_SPL_TEXT_BASE;
	bct_tbl->bootloader[0].length = ebt_size;

	ret = encrypt_data_block(bct, BCT_LENGTH, sbk);
	if (ret)
		return 1;

	ret = sign_enc_data_block(bct, BCT_LENGTH, bct_hash, sbk);
	if (ret)
		return 1;

	return 0;
}
