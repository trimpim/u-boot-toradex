/*
 * Copyright (C) 2014-2016, Toradex AG
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * Helpers for Freescale PMIC PF8100
*/

#include <common.h>
#include <i2c.h>
#include <asm/mach-imx/sci/sci.h>
#include <asm/arch/imx8-pins.h>
#include <asm/gpio.h>
#include <asm/arch/iomux.h>

#include "pf8100_otp.inc"

/* Register Addresses */
#define PF8100_DEVICEID			0x00
#define PF8100_REVID			0x01
#define PF8100_EMREV			0x02
#define PF8100_PROGID			0x03
#define PF8100_PAGE_SELECT		0x9f

/* output some informational messages, return if device is programmed */
/* i.e. 0: unprogrammed (progid=0xfff, 1: programmed */
unsigned pmic_init(void);

/* programmes OTP fuses to values required on a Toradex Apalis iMX6 */
int pf8100_prog(void);

/* define for PMIC register dump */
#define DEBUG

/* Fuse the PMIC in production
 *
 * TBBEN:       ADC_IN2 LSIO.GPIO1.IO12		on module
 * ProgVolt:    SC_P_USB_SS3_TC1_GPIO4_IO04	SODIMM 133
*/
#define GPIO_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))
static iomux_cfg_t const pmic_prog_pads[] = {
	SC_P_ADC_IN2 | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
#	define PMIC_TBBEN IMX_GPIO_NR(1, 12)
	SC_P_USB_SS3_TC1 | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
#	define PMIC_PROG_VOLTAGE IMX_GPIO_NR(4, 4)
};


int pmic_i2c_read(unsigned reg, unsigned *val)
{
	sc_err_t err;
	err = sc_misc_set_control(SC_IPC_CH, SC_R_BOARD_R0, SC_C_PMIC_I2C_READ_REG, reg);
	err |= sc_misc_get_control(SC_IPC_CH, SC_R_BOARD_R0, SC_C_PMIC_I2C, val);
	return err != SC_ERR_NONE;
}

int pmic_i2c_write(unsigned reg, unsigned val)
{
	sc_err_t err;
	val = (val & 0xff) | ((reg & 0xff) << 8);
	err = sc_misc_set_control(SC_IPC_CH, SC_R_BOARD_R0, SC_C_PMIC_I2C, val);
	return err != SC_ERR_NONE;
}

unsigned pmic_init(void)
{
	unsigned devid, revid, emrev, progid;

	puts("PMIC: ");

	/* get device ident */
	if (pmic_i2c_read(PF8100_DEVICEID, &devid)) {
		puts("i2c pmic devid read failed\n");
		return 0;
	}
	if (pmic_i2c_read(PF8100_REVID, &revid) < 0) {
		puts("i2c pmic revid read failed\n");
		return 0;
	}
	if (pmic_i2c_read(PF8100_EMREV, &emrev)) {
		puts("i2c pmic emrev read failed\n");
		return 0;
	}
	if (pmic_i2c_read(PF8100_PROGID, &progid) < 0) {
		puts("i2c pmic prog id read failed\n");
		return 0;
	}

	progid |= (emrev & 0xf0) << 4;
	printf("device id: 0x%.2x, revision id: 0x%.2x, emrev %x, prog id %.3x\n",
		devid, revid, emrev & 7, progid);


#ifdef DEBUG
	{
		unsigned i, j, val;

		/* Set TBBEN high */
		gpio_request(PMIC_TBBEN, "PMIC_TBBEN");
		gpio_direction_output(PMIC_TBBEN, 1);
		mdelay(100);

		/* Page select 1 */
		pmic_i2c_write(PF8100_PAGE_SELECT, 0);
		mdelay(100);
		pmic_i2c_read(PF8100_PAGE_SELECT, &val);
		printf(" readback is %x\n", val);

		for (i = 0; i < 16; i++)
			printf("\t%x", i);
		for (j = 0; j < 0xa0; ) {
			printf("\n%2x", j);
			for (i = 0; i < 16; i++) {
				pmic_i2c_read(j+i, &val);
				printf("\t%2x", val);
			}
			j += 0x10;
		}

		printf("\nEXT Page 1");
		/* Page select 1 */
		pmic_i2c_write(PF8100_PAGE_SELECT, 1);
		mdelay(100);
		pmic_i2c_read(PF8100_PAGE_SELECT, &val);
		printf(" readback is %x", val);

		for (j = 0xa0; j < 0xf0; ) {
			printf("\n%2x", j);
			for (i = 0; i < 16; i++) {
				pmic_i2c_read(j+i, &val);
				printf("\t%2x", val);
			}
			j += 0x10;
		}

		printf("\nEXT Page 2");
		/* Page select 2 */
		pmic_i2c_write(PF8100_PAGE_SELECT, 2);
		mdelay(100);
		pmic_i2c_read(PF8100_PAGE_SELECT, &val);
		printf(" readback is %x", val);

		for (j = 0xa0; j < 0xc0; ) {
			printf("\n%2x", j);
			for (i = 0; i < 16; i++) {
				pmic_i2c_read(j+i, &val);
				printf("\t%2x", val);
			}
			j += 0x10;
		}
		printf("\n");

		/* Set TBBEN low */
		gpio_direction_output(PMIC_TBBEN, 0);
		mdelay(100);
	}
#endif

	return (progid != 0xfff);
}

int pf8100_prog(void)
{
	unsigned int err, i, read;
	int ret = CMD_RET_SUCCESS;

	if (pmic_init() == 1) {
		printf("PMIC already programmed, exiting\n");
		return CMD_RET_FAILURE;
	}
	/* set up gpio to manipulate vprog, initially off */
	imx8_iomux_setup_multiple_pads(pmic_prog_pads, ARRAY_SIZE(pmic_prog_pads));
	gpio_request(PMIC_TBBEN, "PMIC_TBBEN");
	gpio_request(PMIC_PROG_VOLTAGE, "PMIC_PROG_VOLT");

	/* Set TBBEN high */
	gpio_direction_output(PMIC_TBBEN, 1);
	mdelay(100);

	/* Page select 2 */
	if (pmic_i2c_write(PF8100_PAGE_SELECT, 2)) {
		printf("PMIC i2c page select failed\n");
		ret = CMD_RET_FAILURE;
		goto out1;
	}

	err = pmic_i2c_read(0xA7, &read);
	if (err | read) {
		printf("PMIC i2c page 2, A7 != 0\n");
		ret = CMD_RET_FAILURE;
		goto out1;
	}

	/* PROG_VOLTAGE at 8 V */
	gpio_direction_output(PMIC_PROG_VOLTAGE, 1);
	mdelay(200);

	/* Page select 1 */
	if (pmic_i2c_write(PF8100_PAGE_SELECT, 1)) {
		printf("PMIC i2c page select failed\n");
		ret = CMD_RET_FAILURE;
		goto out2;
	}

	/* write all OTP mirror values */
	for(i = 0; i < ARRAY_SIZE(pf8100fuses); i++)
		if (pmic_i2c_write(pf8100fuses[i].address, pf8100fuses[i].data)) {
			printf("PMIC i2c OTP write failed at 0x%.2x\n", pf8100fuses[i].address);
			ret = CMD_RET_FAILURE;
			goto out2;
		}
	err = pmic_i2c_write(0x9F,0x02);  //4.Write 0x9F -> 0x02 // Set register Page to OTP controller page
	err |= pmic_i2c_write(0xA0,0x80);  //5.Write 0xA0 -> 0x80 // Set OTP Controller in Idle state
	err |= pmic_i2c_write(0xA1,0x00);  //6.Write 0xA1 -> 0x00 // Set Start Address to 0x00 in Fuse Array
	err |= pmic_i2c_write(0XA2,0x42);  //7.Write 0XA2 -> 0x42 // Set Stop Address to 0x48 in Fuse Array
	err |= pmic_i2c_write(0xA8,0x0F);  //8.Write 0xA8 -> 0x0F // Set the Maximum number of programming attempts
	err |= pmic_i2c_write(0xA9,0x02);  //9.Write 0xA9 -> 0x02 // Set MRR voltage regulator
	err |= pmic_i2c_write(0xAA,0x07);  //10.Write 0xAA -> 0x07 //Write SC Reference 1
	err |= pmic_i2c_write(0xAB,0x09);  //11.Write 0xAB -> 0x09 // Write I Reference 1
	err |= pmic_i2c_write(0xAC,0x07);  //12.Write 0xAC -> 0x07 // Write SC Reference 2
	err |= pmic_i2c_write(0xAD,0x35);  //13.Write 0xAD -> 0x35 // Write I Reference 2
	err |= pmic_i2c_write(0xAE,0x04);  //14.Write 0xAE -> 0x04 // Set the High byte of the MR_TEST register
	err |= pmic_i2c_write(0xAF,0x42);  //15.Write 0xAF -> 0x42 // Set the Low byte of the MR_TEST register
	err |= pmic_i2c_write(0xB0,0x00);  //16.Write 0xB0 -> 0x00 // Set the High byte of the MREF_TEST register
	err |= pmic_i2c_write(0xB1,0x00);  //17. Write 0xB1 -> 0x00 // Set the Low byte of the MREF_TEST register

	//Calculate CRC to be written to OTP
	err |= pmic_i2c_write(0xA0,0xA5);  //18.Write 0xA0 -> 0xA5 // Calculate CRC values
	err |= pmic_i2c_write(0xA0,0xA4);  //19.Write 0xA0 -> 0xA4 // Test CRC Values

	//Verify CRC is generated -optional (CRC value is dependent of the OTP configuration)
	err |= pmic_i2c_write(0x9F,0x01);  //20.Write 0x9F -> 0x01 // Page select 01
	err |= pmic_i2c_read(0xE7,&read);  //21.Read -> 0xE7 // Must be different than 0x00
	if (read == 0x00)
		printf("ERROR register 0xE7 = 0x0\r\n");
	err |= pmic_i2c_read(0xE8,&read);  //22.Read -> 0xE8 // Must be different than 0x00
	if (read == 0x00)
		printf("ERROR register 0xE8 = 0x0\r\n");

	err |= pmic_i2c_write(0x9F,0x02);  //24.Write 0x9F -> 0x02 // Page select 02
	err |= pmic_i2c_read(0xA7,&read);
	printf("Register 0xA7 = 0x%X\r\n",read);
	err |= pmic_i2c_read(0xA6,&read);
	printf("Register 0xA6 = 0x%X\r\n",read);
	if (err) {
		printf("Programming preparation failed! Don't starting the programming step\n");
		ret = CMD_RET_FAILURE;
		goto out2;
	}

#if 0
	//Lets burn the fuses
	err |= pmic_i2c_write(0x9F,0x02);   //24.Write 0x9F -> 0x02 // Page select 02

	err |= pmic_i2c_write(0xA0,0x96);   //25.Write 0xA0 -> 0x96
	read = 0xFF;
	mdelay(200);
	do {
		err |= pmic_i2c_read(0xA6,&read);   //26.Read -> 0xA0 // if Bit 0 = 1 Controller is still busy.
	} while((read & 0x1) == 0x1);        //27.Loop step 22 until bit 0 = 0 (Controller is done)

	err |= pmic_i2c_write(0xA2,0xFF);    //28.Write A2 -> 0xFF // Set mask to write to full byte.
	err |= pmic_i2c_write(0xA1,0xFC);    //29.Write A1 -> 0xFC // Set single Write Address to Write Protect(WP) Low Byte
	err |= pmic_i2c_write(0xA3,0xAA);    //30.Write A3 -> 0xAA // Set Data for single write on Write Protect(WP) Low Byte
	err |= pmic_i2c_write(0xA0,0x87);    //31.Write A0 -> 0x87 // Burn Write Protect low Byte
	err |= pmic_i2c_write(0xA1,0xFD);    //32.Write A1 -> 0xFD // Set single Write Address to Write Protect high Byte
	err |= pmic_i2c_write(0xA3,0x55);    //33.Write A3 -> 0x55 // Set Data for single write on Write Protect high Byte
	err |= pmic_i2c_write(0xA0,0x87);    //34.Write A0 -> 0x87 // Burn Write Protect low Byte
	err |= pmic_i2c_write(0xA1,0xFE);    //35.Write A1 -> 0xFE // Set single Write Address to Boot Enable (BE)Low Byte
	err |= pmic_i2c_write(0xA3,0xAA);    //36.Write A3 -> 0xAA // Set Data for single write to Boot enable(BE) Low Byte
	err |= pmic_i2c_write(0xA0,0x87);    //37.Write A0 -> 0x87 // Burn Boot enable Low Byte
	err |= pmic_i2c_write(0xA1,0xFF);    //38.Write A1 -> 0xFF // Set single Write Address to Boot Enable High Byte
	err |= pmic_i2c_write(0xA3,0x55);    //39.Write A3 -> 0x55 // Set Data for single write Boot enable High Byte
	err |= pmic_i2c_write(0xA0,0x87);    //40.Write A0 -> 0x87 // Burn Boot enable High Byte
	mdelay(200);

	//41.Apply VDDOTP = 1.5V (but should be 0V)
	gpio_direction_output(PMIC_PROG_VOLTAGE, 0);
	mdelay(100);

	err |= pmic_i2c_read(0x0,&read);    //
	printf("Device ID = 0x%X\r\n",read);
	err |= pmic_i2c_write(0x9F,0x2);
	err |= pmic_i2c_read(0xA7,&read);    //
	printf("SECT_STATUS = 0x%X\r\n",read);
	err |= pmic_i2c_read(0xA6,&read);    //
	printf("FSTATUS = 0x%X\r\n",read);

	if (err) {
		printf("Programming failed!\n");
		ret = CMD_RET_FAILURE;
		goto out2;
	}

#endif
out2:
	gpio_direction_output(PMIC_PROG_VOLTAGE, 0);
	mdelay(100);

out1:
	/* Set TBBEN low */
	gpio_direction_output(PMIC_TBBEN, 0);
	mdelay(100);

	return ret;
}

int do_pf8100_prog(cmd_tbl_t *cmdtp, int flag, int argc,
		char * const argv[])
{
	int ret;
	puts("Programming PMIC OTP...");
	ret = pf8100_prog();
	if (ret == CMD_RET_SUCCESS)
		puts("done.\n");
	else
		puts("failed.\n");
	return ret;
}

U_BOOT_CMD(
	pf8100_otp_prog, 1, 0, do_pf8100_prog,
	"Program the OTP fuses on the PMIC PF8100",
	""
);
