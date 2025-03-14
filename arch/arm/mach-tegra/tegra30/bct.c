// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2022, Ramin <raminterex@yahoo.com>
 * Copyright (c) 2022, Svyatoslav Ryhel <clamor95@gmail.com>
 */

#include <common.h>
#include <command.h>
#include <log.h>
#include <asm/arch-tegra/crypto.h>
#include "bct.h"
#include "uboot_aes.h"

/* If board does not pass sbk, keep it 0 */
__weak void get_secure_key(u8 *key) {}

/*
 * \param bct		boot config table start in RAM
 * \param ect		bootloader start in RAM
 * \param ebt_size	bootloader file size in bytes
 */
static int bct_patch(u8 *bct, u8 *ebt, u32 ebt_size)
{
	u8 ebt_hash[AES128_KEY_LENGTH] = { 0 };
	u8 sbk[AES128_KEY_LENGTH] = { 0 };
	nvboot_config_table *bct_tbl = NULL;
	u8 *bct_hash = bct;
	int ret;

	get_secure_key(sbk);

	bct += BCT_HASH;

	ret = decrypt_data_block(bct, BCT_LENGTH, sbk);
	if (ret)
		return 1;

	ebt_size = roundup(ebt_size, EBT_ALIGNMENT);

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

static int do_ebtupdate(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	u32 bct_addr = hextoul(argv[1], NULL);
	u32 ebt_addr = hextoul(argv[2], NULL);
	u32 ebt_size = hextoul(argv[3], NULL);

	return bct_patch((u8 *)bct_addr, (u8 *)ebt_addr, ebt_size);
}

U_BOOT_CMD(
	ebtupdate,	4,	0,	do_ebtupdate,
	"update bootloader on Tegra30 devices",
	""
);
