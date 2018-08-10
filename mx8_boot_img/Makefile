-include include/config/auto.conf

ifeq ("${CONFIG_IMX8QM}", "y")
	DEPLOY_IMG ?= /build/krm/oe-core_imx8/build/tmp/deploy/images/apalis-imx8
	MKIMG = $(DEPLOY_IMG)/imx-boot-tools/mkimage_imx8

	SOC = QM
	DCD_BOARD ?= apalis-imx8
	DCD_CFG ?= $(DEPLOY_IMG)/imx-boot-tools/imx8qm_$(DCD_BOARD)_dcd.cfg.tmp
	SCFW ?= $(DEPLOY_IMG)/imx-boot-tools/scfw_tcm.bin
	ATF ?= $(DEPLOY_IMG)/imx-boot-tools/bl31-imx8qm.bin
	U_BOOT_BIN ?= ../u-boot.bin
#	M40_TCM_BIN ?= -m4 $(DEPLOY_IMG)/imx-boot-tools/m40_tcm.bin 0 0x34FE0000 -m4 $(DEPLOY_IMG)/imx-boot-tools/m41_tcm.bin 0 0x38FE0000
	M40_TCM_BIN ?= -m4 $(DEPLOY_IMG)/imx-boot-tools/m40_tcm.bin 0 0x34FE0000
	M4_IMG_BIN ?= m4_image.bin
else ifeq ("${CONFIG_IMX8QXP}", "y")
	DEPLOY_IMG ?= /build/krm/oe-core_imx8/build/tmp/deploy/images/colibri-imx8qxp
	MKIMG = $(DEPLOY_IMG)/imx-boot-tools/mkimage_imx8

	SOC = QX
	DCD_BOARD ?= colibri-imx8
	DCD_CFG ?= $(DEPLOY_IMG)/imx-boot-tools/imx8qx_$(DCD_BOARD)_dcd.cfg.tmp
	SCFW ?= $(DEPLOY_IMG)/imx-boot-tools/scfw_tcm.bin
	ATF ?= $(DEPLOY_IMG)/imx-boot-tools/bl31-imx8qxp.bin
	U_BOOT_BIN ?= ../u-boot.bin
	M40_TCM_BIN ?= -m4 $(DEPLOY_IMG)/imx-boot-tools/m40_tcm.bin 0 0x34FE0000
	M4_IMG_BIN ?= m4_image.bin
else
	$(error creating the bootimg file is only supported for apalis/colibri-imx8)
endif

OUTFILE = ../flash.bin

ifneq ("$(wildcard $(MKIMG))","")
	DEFTARGET = flash_multi_cores
else
	DEFTARGET = print-error
endif

DEFAULT: $(DEFTARGET)

print-error:
	@echo "$(MKIMG) not found."; touch $(OUTFILE);

$(M40_TCM_BIN):
	cp /build/krm/oe-core_imx8/build-apalis/tmp/work/colibri_imx8qxp-poky-linux/imx-boot/0.2-r0/git/iMX8QX/m40_tcm.bin $(DEPLOY_IMG)/imx-boot-tools/

u-boot-atf.bin: $(U_BOOT_BIN) $(ATF) Makefile
	@cp $(ATF) u-boot-atf.bin
	$(MKIMG) -commit > head.hash
	@cat $(U_BOOT_BIN) head.hash > u-boot-hash.bin
	rm head.hash
	@dd if=u-boot-hash.bin of=u-boot-atf.bin bs=1K seek=128

flash_scfw: $(MKIMG) $(SCFW)
	$(MKIMG) -soc $(SOC) -c -scfw $(SCFW) -out $(OUTFILE)

flash_dcd: $(MKIMG) $(DCD_CFG) $(SCFW) u-boot-atf.bin
	$(MKIMG) -soc $(SOC) -c -dcd $(DCD_CFG) -scfw $(SCFW) -c -ap u-boot-atf.bin a35 0x80000000 -out $(OUTFILE)

flash: $(MKIMG) $(SCFW) u-boot-atf.bin
	$(MKIMG) -soc $(SOC) -c -scfw $(SCFW) -c -ap u-boot-atf.bin a35 0x80000000 -out $(OUTFILE)

flash_early: $(MKIMG) $(SCFW) u-boot-atf.bin
	$(MKIMG) -soc $(SOC) -c -flags 0x00400000 -scfw $(SCFW) -c -ap u-boot-atf.bin a35 0x80000000 -out $(OUTFILE)

flash_multi_cores: $(MKIMG) $(DCD_CFG) $(SCFW) $(M40_TCM_BIN) u-boot-atf.bin
	$(MKIMG) -soc $(SOC) -c -dcd $(DCD_CFG) -scfw $(SCFW) $(M40_TCM_BIN) -c -ap u-boot-atf.bin a35 0x80000000 -out $(OUTFILE);

flash_nand: $(MKIMG) $(DCD_CFG) $(SCFW) u-boot-atf.bin
	$(MKIMG) -soc $(SOC) -dev nand -c -dcd $(DCD_CFG) -scfw $(SCFW) -c -ap u-boot-atf.bin a35 0x80000000 -out $(OUTFILE)

flash_cm4_0: $(MKIMG) $(DCD_CFG) $(SCFW) $(M4_IMG_BIN)
	$(MKIMG) -soc $(SOC) -c -dcd $(DCD_CFG) -scfw $(SCFW) -m4 $(M4_IMG_BIN) 0 0x34FE0000 -out $(OUTFILE)

flash_all: $(MKIMG) $(DCD_CFG) $(SCFW) $(M4_IMG_BIN) u-boot-atf.bin scd.bin csf.bin csf_ap.bin
	$(MKIMG) -soc $(SOC) -c -dcd $(DCD_CFG) -scfw $(SCFW) -m4 $(M4_IMG_BIN) 0 0x34FE0000 -scd scd.bin -csf csf.bin -c -ap u-boot-atf.bin a35 0x80000000 -csf csf_ap.bin -out $(OUTFILE)

flash_ca35_ddrstress: $(MKIMG) $(SCFW) mx8qx_ddr_stress_test.bin
	$(MKIMG) -soc $(SOC) -c -flags 0x00800000 -scfw $(SCFW) -c -ap mx8qx_ddr_stress_test.bin a35 0x00112000 -out $(OUTFILE)

flash_ca35_ddrstress_dcd: $(MKIMG) $(DCD_CFG) $(SCFW) mx8qx_ddr_stress_test.bin
	$(MKIMG) -soc $(SOC) -c -flags 0x00800000 -dcd $(DCD_CFG) -scfw $(SCFW) -c -ap mx8qx_ddr_stress_test.bin a35 0x00112000 -out $(OUTFILE)
	
flash_cm4ddr: $(MKIMG) $(DCD_CFG) $(SCFW) $(M4_IMG_BIN)
	$(MKIMG) -soc $(SOC) -c -dcd $(DCD_CFG) -scfw $(SCFW) -m4 $(M4_IMG_BIN) 0 0x88000000 -out $(OUTFILE)

flash_fastboot: $(MKIMG) $(DCD_CFG) $(SCFW) u-boot-atf.bin $(M4_IMG_BIN)
	$(MKIMG) -soc $(SOC) -dev emmc_fast -c -dcd $(DCD_CFG) -scfw $(SCFW) -ap u-boot-atf.bin a35 0x80000000 -out $(OUTFILE)

.PHONY = print-error
