// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Marvell International Ltd. and its affiliates
 *
 * Armada 375 SerDes PHY power-up sequence, ported from the Marvell
 * u-boot-2013.01 "bin_hdr" sources (mvHighSpeedEnvSpec.c, A0 revision
 * code path) for the DB-88F6720 board.
 *
 * The DB-88F6720 SerDes topology is fixed:
 *   lane0 = PEX0, lane1 = PEX1, lane2 = SATA, lane3 = USB3
 *
 * Unlike the original source, this does not implement legacy PCIe bus
 * enumeration / Gen1->Gen2 upgrade probing, since that role is now
 * handled by the generic DM PCI subsystem (see drivers/pci/pcie_mvebu.c
 * and friends) once U-Boot proper is running.
 */

#include <config.h>
#include <log.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/arch/soc.h>

#include "high_speed_env_spec.h"

#define SERDES_VERSION		"0.1.1 (a375 port)"

static inline u32 reg_read(u32 offset)
{
	return readl(INTREG_BASE + offset);
}

static inline void reg_write(u32 offset, u32 val)
{
	writel(val, INTREG_BASE + offset);
}

/*
 * A375-A0 common PHY configuration bits applied to every connected lane
 * (Power up IVREF, enable PHY digital reference clock, keep TX idle
 * during PHY init).
 */
static inline u32 a375_a0_common_phy_config(u32 reg)
{
	reg |= POWER_UP_IVREF_MASK;
	reg &= ~REF_CLK_DIS_MASK;
	reg |= PIN_TX_IDLE_MASK;

	return reg;
}

static void a375_a0_reset_dfe_sequence(u32 lane)
{
	reg_write(POWER_REG1_REG(lane), 0xe409); /* reset DFE sequence */
	reg_write(POWER_REG1_REG(lane), 0xe008); /* un-reset DFE sequence */
}

static void reset_phy_and_pipe(u32 lane, bool reset)
{
	u32 reg = reg_read(COMMON_PHY_CONFIGURATION1_REG(lane));

	if (reset) {
		reg |= PHY_SOFTWARE_RESET_MASK;
		reg |= PHY_RESET_CORE_MASK;
		reg &= ~PHY_CORE_RST_MASK;
		reg &= ~PHY_POWER_ON_RESET_MASK;
	} else {
		reg &= ~PHY_SOFTWARE_RESET_MASK;
		reg &= ~PHY_RESET_CORE_MASK;
		reg |= PHY_CORE_RST_MASK;
		reg |= PHY_POWER_ON_RESET_MASK;
	}

	reg_write(COMMON_PHY_CONFIGURATION1_REG(lane), reg);
}

static u32 get_lane_selector_config(const u8 *serdes_map)
{
	u32 reg, tmp;

	reg = 0x1; /* lane 0 is always PEX */

	switch (serdes_map[1]) {
	default:
	case SERDES_UNIT_PEX:
		tmp = 0;
		break;
	case SERDES_UNIT_SGMII:
		tmp = 1;
		break;
	case SERDES_UNIT_SATA:
		tmp = 2;
		break;
	}
	reg |= tmp << 1;

	switch (serdes_map[2]) {
	default:
	case SERDES_UNIT_PEX:
		tmp = 0;
		break;
	case SERDES_UNIT_SGMII:
		tmp = 1;
		break;
	case SERDES_UNIT_SATA:
		tmp = 2;
		break;
	}
	reg |= tmp << 2;

	reg |= (serdes_map[3] == SERDES_UNIT_SGMII ? 1 : 0) << 4;

	return reg;
}

static u32 get_serdes_speed_config(enum serdes_unit_type type)
{
	switch (type) {
	case SERDES_UNIT_USB3:
	case SERDES_UNIT_SATA:
		return 0x1;
	case SERDES_UNIT_SGMII:
		/* Only GEN1 (1.25G) SGMII is supported by this port */
		return 0x6;
	default:
		return 0x0;
	}
}

/* USB 2.0 PHY init, specific for 40nm silicon (88F6720 A0) */
static int usb2_phy_init(u32 dev)
{
	u32 reg;

	/* Set USB PHY config selector to USB 2.0 */
	reg = reg_read(USB_CLUSTER_CONTROL_REG);
	reg &= ~0x1;
	reg_write(USB_CLUSTER_CONTROL_REG, reg);

	/* Configure Rx PLL tolerance to ppm deviation (change to 2000ppm) */
	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 8));
	reg = (reg & ~0xc) | 8;
	reg_write(USB2_PHY_CHANNEL_REG(dev, 8), reg);

	/*
	 * Set the PLL clocks to 480 MHz. Ref clock is 25 MHz, so
	 * divider = 5, multiplier = 96.
	 */
	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 1));
	reg = (reg & ~0x1ff) | 96;
	reg = (reg & ~0x1e00) | (5 << 9);
	reg_write(USB2_PHY_CHANNEL_REG(dev, 1), reg);

	/* Turn on the PLL and wait 200 usec */
	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 2));
	reg |= BIT(13);
	reg_write(USB2_PHY_CHANNEL_REG(dev, 2), reg);
	udelay(200);

	/* Enable the analog part of the PHY */
	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 13));
	reg |= BIT(14);
	reg_write(USB2_PHY_CHANNEL_REG(dev, 13), reg);
	udelay(2);

	/* Turn on the VCO calibration */
	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 2));
	reg |= BIT(2);
	reg_write(USB2_PHY_CHANNEL_REG(dev, 2), reg);
	udelay(10);

	/* Perform impedance calibration for 40 usec */
	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 4));
	reg |= BIT(13);
	reg_write(USB2_PHY_CHANNEL_REG(dev, 4), reg);
	udelay(1);

	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 4));
	reg &= ~BIT(13);
	reg_write(USB2_PHY_CHANNEL_REG(dev, 4), reg);
	udelay(400);

	/* Check if the PHY is ready */
	reg = reg_read(USB2_PHY_CHANNEL_REG(dev, 2));
	if (!(reg & BIT(15))) {
		printf("Error: USB2 UTMI PHY %d not ready\n", dev);
		return -1;
	}

	debug("USB2 UTMI PHY %d initialized successfully\n", dev);

	return 0;
}

int serdes_phy_config(void)
{
	static const u8 serdes_map[MAX_SERDES_LANES] = A375_DB_6720_SERDES_CFG;
	u32 lane, reg, tmp;

	printf("High speed PHY - Version: %s\n", SERDES_VERSION);

	/* Step 1: power down PLL, RX, TX on all lanes */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++) {
		reg = reg_read(COMMON_PHY_CONFIGURATION1_REG(lane));
		reg &= ~PIN_TX_IDLE_MASK;
		reg &= ~(PHY_POWER_UP_PLL_MASK | PHY_POWER_UP_RX_MASK |
			 PHY_POWER_UP_TX_MASK);
		reg_write(COMMON_PHY_CONFIGURATION1_REG(lane), reg);
	}
	udelay(10000);

	/* Step 2: reset PHY and PIPE (A0 revision) */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++)
		reset_phy_and_pipe(lane, true);

	/* Step 2: common PHY selectors */
	reg_write(COMMON_PHY_SELECTOR_REG, get_lane_selector_config(serdes_map));

	/* Step 3: configuration 1 */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++) {
		reg = reg_read(COMMON_PHY_CONFIGURATION1_REG(lane));

		switch (serdes_map[lane]) {
		case SERDES_UNIT_USB3:
			reg = a375_a0_common_phy_config(reg);
			reg |= PHY_MODE_MASK;		/* PHY mode = USB */
			reg |= PIPE_SELECT_MASK;	/* select USB3_PEX */
			break;
		case SERDES_UNIT_PEX:
			reg |= PIPE_SELECT_MASK;	/* select USB3_PEX */
			reg &= ~PHY_MODE_MASK;		/* PHY mode = PEX */
			reg = a375_a0_common_phy_config(reg);
			break;
		case SERDES_UNIT_SGMII:
		case SERDES_UNIT_SATA:
			reg = a375_a0_common_phy_config(reg);
			reg &= ~PIPE_SELECT_MASK;	/* select SATA_SGMII */
			reg |= POWER_UP_IVREF_MASK;
			break;
		case SERDES_UNIT_UNCONNECTED:
		default:
			break;
		}

		tmp = get_serdes_speed_config(serdes_map[lane]);
		reg &= ~GEN_RX_MASK;
		reg |= tmp << GEN_RX_OFFS;
		reg &= ~GEN_TX_MASK;
		reg |= tmp << GEN_TX_OFFS;

		reg_write(COMMON_PHY_CONFIGURATION1_REG(lane), reg);
	}

	/* Step 3.5: un-reset PHY and PIPE (A0 revision) */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++)
		reset_phy_and_pipe(lane, false);

	/* Step 4: per-lane COMPHY register configuration */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++) {
		switch (serdes_map[lane]) {
		case SERDES_UNIT_PEX:
			reg_write(RESET_AND_CLOCK_CONTROL_REG(lane), 0x25);
			reg_write(POWER_AND_PLL_CONTROL_REG(lane), 0xfc60);
			reg_write(MISCELLANEOUS_CONTROL0_REG(lane), 0x6017);
			reg_write(INTERFACE_REG1_REG(lane), 0x1400);
			reg_write(DIGITAL_LOOPBACK_ENABLE_REG(lane), 0x400);
			a375_a0_reset_dfe_sequence(lane);
			reg_write(RESET_AND_CLOCK_CONTROL_REG(lane), 0x24);
			break;
		case SERDES_UNIT_USB3:
			reg_write(RESET_AND_CLOCK_CONTROL_REG(lane), 0x21);
			reg_write(POWER_AND_PLL_CONTROL_REG(lane), 0xfca0);
			reg_write(LANE_CONFIGURATION_4_REG(lane), 0x13);
			reg_write(MISCELLANEOUS_CONTROL0_REG(lane), 0x6017);
			reg_write(INTERFACE_REG1_REG(lane), 0x1400);
			reg_write(DIGITAL_LOOPBACK_ENABLE_REG(lane), 0x400);
			a375_a0_reset_dfe_sequence(lane);
			reg_write(RESET_AND_CLOCK_CONTROL_REG(lane), 0x20);
			break;
		case SERDES_UNIT_SATA:
			reg_write(POWER_AND_PLL_CONTROL_REG(lane), 0xfc01);
			reg_write(MISCELLANEOUS_CONTROL0_REG(lane), 0x6417);
			reg_write(INTERFACE_REG1_REG(lane), 0x1400);
			reg_write(DIGITAL_LOOPBACK_ENABLE_REG(lane), 0x400);
			reg_write(DIGITAL_RESERVED0_REG(lane), 0xe);
			a375_a0_reset_dfe_sequence(lane);
			break;
		case SERDES_UNIT_SGMII:
			reg_write(POWER_AND_PLL_CONTROL_REG(lane), 0xfc81);
			reg_write(DIGITAL_LOOPBACK_ENABLE_REG(lane), 0x0);
			reg_write(MISCELLANEOUS_CONTROL0_REG(lane), 0x6417);
			reg_write(DIGITAL_RESERVED0_REG(lane), 0xe);
			a375_a0_reset_dfe_sequence(lane);
			/* PHY_GEN_TX/RX = 1.25Gbps */
			reg_write(PHY_ISOLATION_MODE_CONTROL_REG(lane), 0x166);
			break;
		case SERDES_UNIT_UNCONNECTED:
		default:
			break;
		}
	}

	/* Step 4.5: power up PLL, RX, TX on all lanes */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++) {
		reg = reg_read(COMMON_PHY_CONFIGURATION1_REG(lane));
		reg |= PHY_POWER_UP_PLL_MASK | PHY_POWER_UP_RX_MASK |
			PHY_POWER_UP_TX_MASK;
		reg_write(COMMON_PHY_CONFIGURATION1_REG(lane), reg);
	}
	udelay(5000);

	/* Step 4.6: wait for SATA/SGMII PHY power-up sequence to finish */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++) {
		if (serdes_map[lane] != SERDES_UNIT_SATA &&
		    serdes_map[lane] != SERDES_UNIT_SGMII)
			continue;

		reg = reg_read(COMMON_PHY_STATUS1_REG(lane));
		if ((reg & 0x6) != 0x6) {
			printf("Serdes lane %d: PHY power up did not finish\n",
			       lane);
			return -1;
		}
	}

	/* Step 5: PEX only - set device type to root complex and enable */
	for (lane = 0; lane < MAX_SERDES_LANES; lane++) {
		if (serdes_map[lane] != SERDES_UNIT_PEX)
			continue;

		reg = reg_read(PEX_CAPABILITIES_REG(lane));
		reg &= ~(0xf << 20);
		reg |= 0x4 << 20; /* root complex */
		reg_write(PEX_CAPABILITIES_REG(lane), reg);
	}

	reg = reg_read(SOC_CTRL_REG);
	reg &= ~0x3;
	reg |= PCIE0_ENABLE_MASK;
	if (serdes_map[1] == SERDES_UNIT_PEX)
		reg |= PCIE1_ENABLE_MASK;
	reg_write(SOC_CTRL_REG, reg);

	/* Init the two integrated USB 2.0 (EHCI) PHYs */
	usb2_phy_init(0);
	usb2_phy_init(1);

	printf("High speed PHY - Ended Successfully\n");

	return 0;
}
