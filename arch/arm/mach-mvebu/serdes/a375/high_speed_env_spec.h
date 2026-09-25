/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Marvell International Ltd. and its affiliates
 *
 * Ported from the Marvell u-boot-2013.01 "bin_hdr" sources
 * (tools/marvell/bin_hdr/src_phy/a375/mvHighSpeedEnvSpec.[ch] and
 * tools/marvell/bin_hdr/platform/sysEnv/a375/mvSysEnvLib.h) to the
 * mainline SPL driver model used by the other mvebu SoCs.
 *
 * NOTE: this only implements the SerDes PHY power-up sequence (the part
 * that is functionally equivalent to a38x/axp serdes_phy_config()). It
 * does not implement the legacy PCIe bus enumeration/Gen2 upgrade code
 * from the original source, as that role is now handled by the generic
 * DM PCI subsystem.
 */

#ifndef __A375_HIGH_SPEED_ENV_SPEC_H
#define __A375_HIGH_SPEED_ENV_SPEC_H

#define MAX_SERDES_LANES		4

enum serdes_unit_type {
	SERDES_UNIT_UNCONNECTED	= 0x0,
	SERDES_UNIT_PEX		= 0x1,
	SERDES_UNIT_SATA	= 0x2,
	SERDES_UNIT_SGMII	= 0x3,
	SERDES_UNIT_USB3	= 0x4,
	SERDES_LAST_UNIT
};

/*
 * DB-88F6720 fixed SerDes lane topology (lane 0 is always PEX0):
 *   lane0 = PEX0, lane1 = PEX1, lane2 = SATA, lane3 = USB3
 */
#define A375_DB_6720_SERDES_CFG					\
	{ SERDES_UNIT_PEX, SERDES_UNIT_PEX, SERDES_UNIT_SATA, SERDES_UNIT_USB3 }

/* MPP / common PHY registers */
#define MPP_CONTROL_REG(id)			(0x18000 + ((id) * 4))

#define COMMON_PHY_SELECTOR_REG		0x18300
#define COMMON_PHY_CONFIGURATION1_REG(phy)	(0x18310 + ((phy) * 4))
#define COMMON_PHY_STATUS1_REG(phy)		(0x18340 + ((phy) * 4))

/* COMMON_PHYn_CONFIGURATION1_REG fields */
#define REF_CLK_DIS_OFFS		0
#define REF_CLK_DIS_MASK		(1 << REF_CLK_DIS_OFFS)
#define POWER_UP_IVREF_OFFS		1
#define POWER_UP_IVREF_MASK		(1 << POWER_UP_IVREF_OFFS)
#define PIPE_SELECT_OFFS		2
#define PIPE_SELECT_MASK		(1 << PIPE_SELECT_OFFS)
#define PHY_SOFTWARE_RESET_OFFS		11
#define PHY_SOFTWARE_RESET_MASK		(1 << PHY_SOFTWARE_RESET_OFFS)
#define PHY_RESET_CORE_OFFS		12
#define PHY_RESET_CORE_MASK		(1 << PHY_RESET_CORE_OFFS)
#define PHY_CORE_RST_OFFS		13
#define PHY_CORE_RST_MASK		(1 << PHY_CORE_RST_OFFS)
#define PHY_POWER_ON_RESET_OFFS		14
#define PHY_POWER_ON_RESET_MASK	(1 << PHY_POWER_ON_RESET_OFFS)
#define PHY_MODE_OFFS			15
#define PHY_MODE_MASK			(1 << PHY_MODE_OFFS)
#define PHY_POWER_UP_PLL_OFFS		16
#define PHY_POWER_UP_PLL_MASK		(1 << PHY_POWER_UP_PLL_OFFS)
#define PHY_POWER_UP_RX_OFFS		17
#define PHY_POWER_UP_RX_MASK		(1 << PHY_POWER_UP_RX_OFFS)
#define PHY_POWER_UP_TX_OFFS		18
#define PHY_POWER_UP_TX_MASK		(1 << PHY_POWER_UP_TX_OFFS)
#define PIN_TX_IDLE_OFFS		19
#define PIN_TX_IDLE_MASK		(1 << PIN_TX_IDLE_OFFS)
#define GEN_RX_OFFS			22
#define GEN_RX_MASK			(0xF << GEN_RX_OFFS)
#define GEN_TX_OFFS			26
#define GEN_TX_MASK			(0xF << GEN_TX_OFFS)

/* Per-lane COMPHY register file (indirect, banked per lane) */
#define COMPHY_REGS_CONFIGURATION_REG(comphy)	(0x30000 + (((comphy) + 2) << 13))
#define POWER_AND_PLL_CONTROL_REG(comphy)	(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x1))
#define KVCO_CALIBRATION_CONTROL_REG(comphy)	(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x2))
#define DIGITAL_LOOPBACK_ENABLE_REG(comphy)	(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x23))
#define PHY_ISOLATION_MODE_CONTROL_REG(comphy)	(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x26))
#define RESET_AND_CLOCK_CONTROL_REG(comphy)	(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0xC1))
#define INTERFACE_REG1_REG(comphy)		(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x25))
#define DIGITAL_RESERVED0_REG(comphy)		(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x3A))
#define MISCELLANEOUS_CONTROL0_REG(comphy)	(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x4F))
#define POWER_REG1_REG(comphy)			(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x52))
#define LANE_CONFIGURATION_4_REG(comphy)	(COMPHY_REGS_CONFIGURATION_REG(comphy) + (4 * 0x88))

/* USB 2.0 PHY (integrated EHCI PHYs, unrelated to the USB3 SerDes lane) */
#define USB_CLUSTER_CONTROL_REG		0x18400
#define USB2_REGS_CONFIGURATION_REG(dev)	(0x50800 + (dev) * 0x4000)
#define USB2_PHY_CHANNEL_REG(dev, reg)		(USB2_REGS_CONFIGURATION_REG(dev) | ((reg) << 2))

/* Misc / SoC control */
#define SOC_CTRL_REG			0x18204
#define PCIE0_ENABLE_OFFS		0
#define PCIE0_ENABLE_MASK		(0x1 << PCIE0_ENABLE_OFFS)
#define PCIE1_ENABLE_OFFS		1
#define PCIE1_ENABLE_MASK		(0x1 << PCIE1_ENABLE_OFFS)

/* PCIe capabilities register, per interface (if0 @ 0x40000, if1 @ 0x44000) */
#define PEX_IF_REGS_BASE(pex_if)	(0x40000 + (pex_if) * 0x4000)
#define PEX_CAPABILITIES_REG(pex_if)	(PEX_IF_REGS_BASE(pex_if) + 0x60)

int serdes_phy_config(void);

#endif /* __A375_HIGH_SPEED_ENV_SPEC_H */
