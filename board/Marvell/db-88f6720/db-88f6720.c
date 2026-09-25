// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Stefan Roese <sr@denx.de>
 */

#include <init.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#include <asm/arch/soc.h>
#include <linux/bitops.h>

DECLARE_GLOBAL_DATA_PTR;

/*
 * Those values and defines are taken from the Marvell U-Boot version
 * "u-boot-2013.01-2014_T2.0" for the board Armada 375 DB-88F6720
 */
#define DB_88F6720_MPP0_7		0x00020020 /* SPI */
#define DB_88F6720_MPP8_15		0x22000022 /* SPI , I2C */
#define DB_88F6720_MPP16_23		0x22222222 /* UART, TDM*/
#define DB_88F6720_MPP24_31		0x33333333 /* SDIO, SPI1*/
#define DB_88F6720_MPP32_39		0x04403330 /* SPI1, External SMI */
#define DB_88F6720_MPP40_47		0x22002044 /* UART1, GE0, SATA0 LED */
#define DB_88F6720_MPP48_55		0x22222222 /* GE0 */
#define DB_88F6720_MPP56_63		0x04444422 /* GE0 , LED_MATRIX, GPIO */
#define DB_88F6720_MPP64_67		0x014	/* LED_MATRIX, SATA1 LED*/

#define DB_88F6720_GPP_OUT_ENA_LOW	0xFFFFFFFF
#define DB_88F6720_GPP_OUT_ENA_MID	0x7FFFFFFF
#define DB_88F6720_GPP_OUT_ENA_HIGH	0xFFFFFFFF
#define DB_88F6720_GPP_OUT_VAL_LOW	0x0
#define DB_88F6720_GPP_OUT_VAL_MID	BIT(31)	/* SATA Power output enable */
#define DB_88F6720_GPP_OUT_VAL_HIGH	0x0
#define DB_88F6720_GPP_POL_LOW		0x0
#define DB_88F6720_GPP_POL_MID		0x0
#define DB_88F6720_GPP_POL_HIGH		0x0

/*
 * Armada 375 "Ethernet Complex" crossbar registers. These route the two
 * GbE MACs to the internal quad-GbE-PHY / switch / RGMII / ComPhy blocks
 * and are *not* configured by the vendor's closed-source bin_hdr blob -
 * the vendor U-Boot (mvEthComplexInit(), board/mv_ebu/a375/.../
 * mvCtrlEthCompLib.c) sets them up in its normal board init. Without this,
 * the internal GbE PHY stays held in reset and its MDIO/SMI bus is left
 * sourced from the switch instead of the MAC, so the mvpp2 driver's MDIO
 * reads simply time out ("Could not get PHY for mdio: addr 0").
 *
 * This board's ethSataComplexOpt is MV_ETHCOMP_GE_MAC0_2_GE_PHY_P0 (only
 * MAC0 -> internal GbE PHY0 is wired up; MAC1 is left unconnected, matching
 * the vendor board config), so only that path is replicated here.
 */
#define MVEBU_ETHCOMP_BASE		MVEBU_REGISTER(0x18900)
#define ETHCOMP_GOP_CTRL_STAT_REG	(MVEBU_ETHCOMP_BASE + 0x00)
#define ETHCOMP_CTRL_REG		(MVEBU_ETHCOMP_BASE + 0x10)
#define ETHCOMP_CLOCK_CTRL_REG		(MVEBU_ETHCOMP_BASE + 0x1c)
#define ETHCOMP_GBE_PHY_CTRL1_REG(phy)	(MVEBU_ETHCOMP_BASE + 0xb0 + (phy) * 4)
#define ETHCOMP_QUAD_GBE_PHY_CTRL_STAT_REG (MVEBU_ETHCOMP_BASE + 0xd0)

static void mvebu_a375_eth_complex_init(void)
{
	u32 reg;

	/* GbE PHY clock control: sample on negative edge (A0 revision) */
	reg = readl(ETHCOMP_CLOCK_CTRL_REG);
	reg &= ~(BIT(23) | BIT(28));
	writel(reg, ETHCOMP_CLOCK_CTRL_REG);

	/* Route GbE MAC0 -> internal quad-GbE-PHY port 0 */
	reg = readl(ETHCOMP_CTRL_REG);
	reg &= ~BIT(14);	/* GbE PHY0 port source = MAC (not switch) */
	reg &= ~(0x3 << 10);
	reg |= (0x2 << 10);	/* GbE MAC0 source = internal GbE PHY complex */
	reg &= ~BIT(20);	/* PHY0 SMI source = MAC (not switch) */
	reg &= ~BIT(7);		/* Switch port 6 source = not connected */
	reg &= ~BIT(4);		/* Switch port 0 source = not connected */
	reg &= ~BIT(15);	/* PHY1 SMI source = MAC (unused port) */
	reg &= ~BIT(16);	/* PHY2 SMI source = MAC (unused port) */
	writel(reg, ETHCOMP_CTRL_REG);

	/* Enable GOP device and set port0 data-path clock source */
	reg = readl(ETHCOMP_GOP_CTRL_STAT_REG);
	reg |= BIT(19);
	reg |= BIT(13);
	writel(reg, ETHCOMP_GOP_CTRL_STAT_REG);

	/* Internal GbE PHY0: no power-down, auto MDI/MDI-X crossover */
	reg = readl(ETHCOMP_GBE_PHY_CTRL1_REG(0));
	reg &= ~(0x7 << 14);
	reg &= ~(0x3 << 12);
	reg |= (0x3 << 12);
	writel(reg, ETHCOMP_GBE_PHY_CTRL1_REG(0));

	/* Release the internal quad-GbE-PHY block from reset */
	reg = readl(ETHCOMP_QUAD_GBE_PHY_CTRL_STAT_REG);
	reg |= BIT(0) | BIT(4);
	writel(reg, ETHCOMP_QUAD_GBE_PHY_CTRL_STAT_REG);
}

int board_early_init_f(void)
{
	/* Configure MPP */
	writel(DB_88F6720_MPP0_7, MVEBU_MPP_BASE + 0x00);
	writel(DB_88F6720_MPP8_15, MVEBU_MPP_BASE + 0x04);
	writel(DB_88F6720_MPP16_23, MVEBU_MPP_BASE + 0x08);
	writel(DB_88F6720_MPP24_31, MVEBU_MPP_BASE + 0x0c);
	writel(DB_88F6720_MPP32_39, MVEBU_MPP_BASE + 0x10);
	writel(DB_88F6720_MPP40_47, MVEBU_MPP_BASE + 0x14);
	writel(DB_88F6720_MPP48_55, MVEBU_MPP_BASE + 0x18);
	writel(DB_88F6720_MPP56_63, MVEBU_MPP_BASE + 0x1c);
	writel(DB_88F6720_MPP64_67, MVEBU_MPP_BASE + 0x20);

	/* Configure GPIO */
	/* Set GPP Out value */
	writel(DB_88F6720_GPP_OUT_VAL_LOW, MVEBU_GPIO0_BASE + 0x00);
	writel(DB_88F6720_GPP_OUT_VAL_MID, MVEBU_GPIO1_BASE + 0x00);
	writel(DB_88F6720_GPP_OUT_VAL_HIGH, MVEBU_GPIO2_BASE + 0x00);

	/* Set GPP Polarity */
	writel(DB_88F6720_GPP_POL_LOW, MVEBU_GPIO0_BASE + 0x0c);
	writel(DB_88F6720_GPP_POL_MID, MVEBU_GPIO1_BASE + 0x0c);
	writel(DB_88F6720_GPP_POL_HIGH, MVEBU_GPIO2_BASE + 0x0c);

	/* Set GPP Out Enable */
	writel(DB_88F6720_GPP_OUT_ENA_LOW, MVEBU_GPIO0_BASE + 0x04);
	writel(DB_88F6720_GPP_OUT_ENA_MID, MVEBU_GPIO1_BASE + 0x04);
	writel(DB_88F6720_GPP_OUT_ENA_HIGH, MVEBU_GPIO2_BASE + 0x04);

	return 0;
}

int board_init(void)
{
	/* adress of boot parameters */
	gd->bd->bi_boot_params = mvebu_sdram_bar(0) + 0x100;

	mvebu_a375_eth_complex_init();

	return 0;
}

int checkboard(void)
{
	puts("Board: Marvell DB-88F6720\n");

	return 0;
}

int board_eth_init(struct bd_info *bis)
{
	cpu_eth_init(bis); /* Built in controller(s) come first */
	return pci_eth_init(bis);
}
