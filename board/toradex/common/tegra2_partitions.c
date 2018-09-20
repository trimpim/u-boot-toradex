#include <common.h>

#include <asm/arch-tegra/ap20.h>
#include <asm/arch/gpio.h>
#include <asm/arch/pinmux.h>
#include <asm/errno.h>
#include <asm/io.h>

#include <fdt_decode.h>
#include <malloc.h>
#if (defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)) || \
    defined(CONFIG_COLIBRI_T30) || defined(CONFIG_APALIS_T30)
#include <mmc.h>
#endif
#ifdef CONFIG_COLIBRI_T20
#include <nand.h>

#include "tegra2_nand.h"
#endif /* CONFIG_COLIBRI_T20 */
#include "tegra2_partitions.h"

#define DEBUG 0

#if DEBUG > 1
#define DEBUG_PARTITION(partinfo) \
	printf("Part:\tID=%u\tNAME=%s\tNAME2=%s\n\t\tTYPE=%u\tALLOC_POLICY=%u" \
	       "\tFS_TYPE=%u\n\t\tVIRTUAL START SEC=%u\tVIRTUAL SIZE=%u\n", \
	     partinfo->id, partinfo->name, partinfo->name2, \
	     partinfo->type, partinfo->allocation_policy, \
	     partinfo->filesystem_type, partinfo->virtual_start_sector, \
	     partinfo->virtual_size); \
	printf("\t\tSTART SEC=%u\tEND SEC=%u\tTOTAL=%u\n", \
	       partinfo->start_sector, partinfo->end_sector, \
	       partinfo->end_sector + 1 - partinfo->start_sector);
#else
#define DEBUG_PARTITION(...)
#endif

DECLARE_GLOBAL_DATA_PTR;

/* physical NAND block size, virtual block size in eMMC/SD card case */
static int block_size;

#if (defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)) || \
    defined(CONFIG_COLIBRI_T30) || defined(CONFIG_APALIS_T30)
static u32 __def_get_boot_size_mult(struct mmc *mmc)
{
	/* return default boot size. */
	return 0;
}
u32 get_boot_size_mult(struct mmc *mmc)
		__attribute__((weak, alias("__def_get_boot_size_mult")));

/**
 * nvtegra_mmc_read - read data from mmc (unaligned)
 * @param startAddress:	data offset in bytes
 * @param dataCount:	data count in bytes
 * @param dst:			destination buffer
 * @return:				number of read bytes or 0 for error
 */
ulong nvtegra_mmc_read(ulong startAddress, ulong dataCount, void *dst)
{
	ulong readBlocks, startBlock, i;
	void *buffer;

	if (dataCount == 0 || dst == NULL)
		return 0;

	struct mmc *mmc = find_mmc_device(EMMC_DEV);
	if (!mmc)
		return 0;

	mmc_init(mmc); // init if not inited

	// align read size to blocks
	startBlock = startAddress / EMMC_BLOCK_SIZE;
	readBlocks = (startAddress + dataCount + EMMC_BLOCK_SIZE - 1) /
		     EMMC_BLOCK_SIZE - startBlock; // ceil

	buffer = malloc(readBlocks * EMMC_BLOCK_SIZE);
	if (!buffer)
		return 0;

	i = mmc->block_dev.block_read(EMMC_DEV, startBlock, readBlocks,
				      buffer);

	if (i != readBlocks) {
		free(buffer);
		return 0;
	}
	memcpy(dst, buffer, dataCount);
	free(buffer);

	return dataCount;
}
#endif /* (CONFIG_ENV_IS_IN_MMC & CONFIG_COLIBRI_T20) | CONFIG_COLIBRI_T30 |
	  CONFIG_APALIS_T30 */

/**
 * nvtegra_print_partition_table - prints partition table info
 * @param pt:  nvtegra_parttable_t structure
 */
void nvtegra_print_partition_table(nvtegra_parttable_t * pt)
{
	int i;
	nvtegra_partinfo_t *p;

	// sanity check
	if (!pt) {
		printf("%s: Error! pt arg is NULL\n", __FUNCTION__);
		return;
	}

	p = &(pt->partinfo[0]);
	printf("\n------ NVTEGRA Partitions ------\n");

#if DEBUG > 1
	printf("UNK:\n");
	for (i = 0; i < 18; i++) {
		if (pt->_unknown[i] > 1000000)
			printf("0x%08X\t", pt->_unknown[i]);
		else
			printf("%u\t", pt->_unknown[i]);
		if ((i + 1) % 6 == 0)
			printf("\n");
	}
	printf("\n");
#endif

	for (i = 0; (p->id < 128) && (i < TEGRA_MAX_PARTITIONS); i++) {
		printf("\n[%u]\t", i);
		DEBUG_PARTITION(p);

#if DEBUG > 1
		printf("\t\tUNK1=%u\t%u\n", p->_unknown1[0], p->_unknown1[1]);
		printf("\t\tUNK2=%u\t%u\t0x%08x\n", p->_unknown2[0],
		       p->_unknown2[1], p->_unknown2[2]);
		printf("\t\tUNK3=%u\n", p->_unknown3);
		printf("\t\tUNK4=%u\n", p->_unknown4);
		printf("\t\tUNK5=%u\n", p->_unknown5);
		printf("\t\tUNK6=%u\t%u\n", p->_unknown6[0], p->_unknown6[1]);
#endif

		p++;
	}
	printf("--------------------------------\n");
}

/**
 * nvtegra_read_partition_table - reads nvidia's partition table
 * @param boot_media: 0: NAND, 1: eMMC or SD card
 * @return:
 *  1 - Success
 *  0 - Error
 */
int nvtegra_read_partition_table(nvtegra_parttable_t * pt, int boot_media)
{
	size_t size;
	int i, j;
	nvtegra_partinfo_t *p;
	u32 bct_start, pt_logical = 0, pt_offset;

	// sanity check
	if (!pt) {
		printf("%s: Error! pt arg is NULL\n", __FUNCTION__);
		return 0;
	}

	/*
	 * Partition table offset is stored in the BCT in IRAM by the BootROM.
	 * The BCT start and size are stored in the BIT in IRAM.
	 * Read the data @ bct_start + (bct_size - 260). This works
	 * on T20 and T30 BCTs, which are locked down. If this changes
	 * in new chips (T114, etc.), we can revisit this algorithm.
	 */
	bct_start = readl(AP20_BASE_PA_SRAM + NVBOOTINFOTABLE_BCTPTR);
#if DEBUG > 1
	printf("bct_start=0x%08x\n", bct_start);
#endif

	/* Search PT logical offset
	   Note: 0x326 for T30 Fastboot, 0xb48 for Eboot resp. Android
		 Fastboot and 0xeec for Vibrante Fastboot */
	for (i = 0; i < 0x800; i++) {
		if (readw(bct_start) == 0x40) {
			/* Either previous or 3rd next word */
			pt_logical = readw(bct_start - 2);
			if (pt_logical < 0x100)
				pt_logical = readw(bct_start + 6);
			break;
		}
		bct_start += 2;
	}
#if DEBUG > 1
#ifdef CONFIG_COLIBRI_T20
	if (boot_media == 0)
		printf("logical=0x%08x writesize=0x%08x erasesize=0x%08x\n",
		       pt_logical, block_size, nand_info->erasesize);
	else
#endif /* CONFIG_COLIBRI_T20 */
		printf("logical=0x%08x divisor=0x%08x\n", pt_logical,
		       block_size);
#endif

	/* In case we are running with a recovery BCT missing the partition
	   table offset information */
#if defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)
	if (1) {
#else
	if (pt_logical == 0) {
#endif
		if (boot_media == 0) {
			/* On NAND BCT partition size is 3 M in our default
			   layout */
			pt_logical = 3 * 1024 * 1024 / block_size;
		} else {
// 3 M - BootPartitions
#if defined(CONFIG_COLIBRI_T30) || defined(CONFIG_APALIS_T30)
			pt_logical = 0x8000;
#else
			pt_logical = 0x4000;
#endif
		}
#if DEBUG > 1
		printf("forced logical=0x%08x\n", pt_logical);
#endif
	}

#ifdef CONFIG_COLIBRI_T20
	if (boot_media == 0) {
		/* StartLogicalSector * PageSize + 4 * BlockSize */
		pt_offset = pt_logical * nand_info->writesize +
			    4 * nand_info->erasesize;
	} else
#endif /* CONFIG_COLIBRI_T20 */
	{
#if (defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)) || \
    defined(CONFIG_COLIBRI_T30) || defined(CONFIG_APALIS_T30)
		/* The PT offset has been calculated from the .cfg eMMC
		   partition configuration file using virtual linearised
		   addressing across all eMMC regions as expected by nvflash.
		   Due to the lack of a region control mechanism in nvflash/
		   .cfg flashing utility in order to obtain the actual PT
		   offset from the start of the user region the size of the
		   boot regions must be subtracted. */
		struct mmc *mmc = find_mmc_device(EMMC_DEV);
		u32 boot_size_mult = 0;
		if (mmc && !mmc_init(mmc)) {
			boot_size_mult = get_boot_size_mult(mmc);
			if (boot_size_mult == 16)
				pt_logical -= 0x5000; //why?
		}

		/* StartLogicalSector / LogicalBlockSize * PhysicalBlockSize +
		   BootPartitions */
		if (boot_size_mult == 32) {
#if DEBUG > 1
			printf("switch to second boot partition for PT access\n");
#endif
			mmc_switch_part(0, 2);
			pt_offset = 0;
		} else
#endif /* (CONFIG_ENV_IS_IN_MMC && CONFIG_COLIBRI_T20) || CONFIG_COLIBRI_T30 ||
	  CONFIG_APALIS_T30 */
			pt_offset = pt_logical / block_size * 512 + 1024 * 1024;
	}
#if DEBUG > 1
	printf("physical=0x%08x\n", pt_offset);
#endif

	size = sizeof(nvtegra_parttable_t);
#ifdef CONFIG_COLIBRI_T20
	if (boot_media == 0) {
		/* badblock may cause drift */
		for (j = 0; j < 16; j++) {
			i = nand_read_skip_bad(&nand_info[0], pt_offset, &size,
					       (unsigned char *)pt);
			if ((i != 0) || (size != sizeof(nvtegra_parttable_t))) {
				printf("%s: Error! nand_read_skip_bad failed. "
				       "block_size=%d ret=%d\n",
				       __FUNCTION__, block_size, i);
				return 0;
			}

			/* some heuristics */
			p = &(pt->partinfo[0]);
			if ((p->id == 2) && !memcmp(p->name, "BCT\0", 4) && !memcmp(p->name2, "BCT\0", 4) && (p->virtual_start_sector == 0))
				break;

			/* skip block and try again */
			pt_offset+=nand_info->erasesize;
		}
	}
#endif /* CONFIG_COLIBRI_T20 */
#if defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)
	else
#endif
#if (defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)) || \
    defined(CONFIG_COLIBRI_T30) || defined(CONFIG_APALIS_T30)
	{
		size = nvtegra_mmc_read(pt_offset, size, (void *)pt);
		if (!size || size != sizeof(nvtegra_parttable_t)) {
			printf("%s: Error! mmc block read failed. Read=%d\n",
			       __FUNCTION__, size);
			/* make sure we are back at the user partition */
			mmc_switch_part(0, 0);
			return 0;
		}
		/* make sure we are back at the user partition */
		mmc_switch_part(0, 0);
	}
#endif /* (CONFIG_ENV_IS_IN_MMC & CONFIG_COLIBRI_T20) | CONFIG_COLIBRI_T30 |
	  CONFIG_APALIS_T30 */

	/* some heuristics */
	p = &(pt->partinfo[0]);
	p->name[3] = '\0'; /* avoid buffer overflow */
	if ((p->id != 2) || memcmp(p->name, "BCT\0", 4)
	    || memcmp(p->name2, "BCT\0", 4) || (p->virtual_start_sector != 0)) {
		printf("%s: Error! Partition table offset is probably "
		       "incorrect. name='%s'\n", __FUNCTION__, p->name);
		return 0;
	}

	return 1;
}

/**
 * nvtegra_find_partition - parse nvidia partition table
 * @param pt:  nvtegra_parttable_t structure
 * @param name:  partition name
 * @param partinfo:  output pointer to nvtegra_partinfo_t structure
 * @return:
 *  1 - Success, partinfo contains partition info
 *  0 - Not found or error
 */
int nvtegra_find_partition(nvtegra_parttable_t * pt, const char *name,
			   nvtegra_partinfo_t ** partinfo)
{
	int i, l;
	nvtegra_partinfo_t *p;

	// sanity checks
	if (!pt) {
		printf("%s: Error! pt arg is NULL\n", __FUNCTION__);
		return 0;
	}
	if (!name) {
		printf("%s: Error! name arg is NULL\n", __FUNCTION__);
		return 0;
	}
	if (!partinfo) {
		printf("%s: Error! partinfo arg is NULL\n", __FUNCTION__);
		return 0;
	}

	p = &(pt->partinfo[0]);
	l = strlen(name) + 1;	// string length + \0
	for (i = 0; (p->id < 128) && (i < TEGRA_MAX_PARTITIONS); i++) {
		if (memcmp(p->name, name, l) == 0
		    || memcmp(p->name2, name, l) == 0) {
			*partinfo = p;
			return 1;
		}
		p++;
	}

	printf("%s: Error! Partition '%s' not found.\n", __FUNCTION__, name);
	return 0;
}

#ifdef CONFIG_COLIBRI_T20
int nvtegra_mtdparts_string(char *output, int size)
{
	int i, j = 0;
	nvtegra_parttable_t *pt;
	nvtegra_partinfo_t *p, *usr;
	char buffer[512];

	// sanity checks
	if (!output) {
		printf("%s: Error! output arg is NULL.\n", __FUNCTION__);
		return 0;
	}
	if (size > 256) {
		printf("%s: Error! size is too large. Increase buffer size "
		       "here.\n", __FUNCTION__);
		return 0;
	}
	// parse nvidia partition table
	pt = malloc(sizeof(nvtegra_parttable_t));
	if (!pt) {
		printf("%s: Error calling malloc(%d)\n", __FUNCTION__,
		       sizeof(nvtegra_parttable_t));
		return 0;
	}

	block_size = nand_info->writesize;
	if (!nvtegra_read_partition_table(pt, 0)) {
		free(pt);
		return 0;
	}
	// make rootfs partition the first in the list
	if (nvtegra_find_partition(pt, "USR", &p)) {
		sprintf(buffer + j, "%uK@%uK(USR)",
			p->virtual_size * nand_info->writesize / 1024,
			p->start_sector * nand_info->writesize / 1024);
		j += strlen(buffer + j);
	}
	usr = p;

	p = &(pt->partinfo[0]);
	for (i = 0; (p->id < 128) && (i < TEGRA_MAX_PARTITIONS); i++) {
		if (p != usr) {
			/* add coma separator after previous entries */
			if (j > 0) {
				sprintf(buffer + j, ",");
				j++;
			}

			if (strlen(p->name))
				sprintf(buffer + j, "%uK@%uK(%s)",
					p->virtual_size *
					nand_info->writesize / 1024,
					p->start_sector *
					nand_info->writesize / 1024, p->name);
			else
				sprintf(buffer + j, "%uK@%uK",
					p->virtual_size *
					nand_info->writesize / 1024,
					p->start_sector *
					nand_info->writesize / 1024);

			j += strlen(buffer + j);
		}
		if (strlen(buffer) >= size)
			break;
		p++;
	}

	memcpy(output, buffer, (j < size ? j + 1 : size));
	free(pt);
	return 1;
}
#endif /* CONFIG_COLIBRI_T20 */

//3 boot types: 0: Colibri T20 NAND only, 1: Colibri T20 mixed SD-boot but
//config block in NAND or 2: Colibri T30 eMMC only
//mixed case requires two itterations, first pass on NAND and second one on SD
//card
void tegra_partition_init(int boot_type)
{
	int itterations, pass;
	nvtegra_parttable_t *pt;
	nvtegra_partinfo_t *partinfo;

	// parse nvidia partition table
	pt = malloc(sizeof(nvtegra_parttable_t));
	if (!pt) {
		printf("%s: Error calling malloc(%d)\n", __FUNCTION__,
		       sizeof(nvtegra_parttable_t));
		return;
	}

	if (boot_type == 0)
		itterations = 1;
	else
		itterations = 2;

	if (boot_type == 2)
		pass = 1;
	else
		pass = 0;

	for (; pass < itterations; pass++) {
#ifdef CONFIG_COLIBRI_T20
		if (pass == 0)
			block_size = nand_info->writesize;
		else
			block_size = 4;
#else /* CONFIG_COLIBRI_T20 */
			block_size = 8;
#endif /* CONFIG_COLIBRI_T20 */

		//copy partition information to global data
		if (!nvtegra_read_partition_table(pt, pass)) {
			free(pt);
			return;
		}

		/* ConfigBlock */
		if ((pass == 0 || (boot_type == 2 && pass == 1)) &&
		    nvtegra_find_partition(pt, "ARG", &partinfo)) {
			gd->conf_blk_offset =
			    partinfo->start_sector * block_size;
			DEBUG_PARTITION(partinfo);
		}

		if ((boot_type == 0 && pass == 0) || pass == 1) {
			if (nvtegra_find_partition(pt, "ENV", &partinfo)) {
				gd->env_offset =
				    partinfo->start_sector * block_size;
				DEBUG_PARTITION(partinfo);
			}

			if (nvtegra_find_partition(pt, "LNX", &partinfo)) {
				gd->kernel_offset =
				    partinfo->start_sector * block_size;
				DEBUG_PARTITION(partinfo);
			}
		}

#if (defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)) || \
    defined(CONFIG_COLIBRI_T30) || defined(CONFIG_APALIS_T30)
		if ((pass == 1) && nvtegra_find_partition(pt, "GP1", &partinfo))
		{
			gd->gpt_offset =
			    partinfo->start_sector * block_size + 1;
			DEBUG_PARTITION(partinfo);
		}
#endif /* (CONFIG_ENV_IS_IN_MMC & CONFIG_COLIBRI_T20) | CONFIG_COLIBRI_T30 |
	  CONFIG_APALIS_T30 */

#if DEBUG > 0
		nvtegra_print_partition_table(pt);
#endif
	}

#if DEBUG > 0
	printf("gd->conf_blk_offset=%u\n", gd->conf_blk_offset);
	printf("gd->env_offset=%u\n", gd->env_offset);
#if (defined(CONFIG_ENV_IS_IN_MMC) && defined(CONFIG_COLIBRI_T20)) || \
    defined(CONFIG_COLIBRI_T30) || defined(CONFIG_APALIS_T30)
	printf("gd->gpt_offset=%u\n", gd->gpt_offset);
#endif
	printf("gd->kernel_offset=%u\n", gd->kernel_offset);
#endif

	free(pt);
}
