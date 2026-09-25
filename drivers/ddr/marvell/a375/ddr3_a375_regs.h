/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) Marvell International Ltd. and its affiliates
 *
 * Ported from the Marvell u-boot-2013.01 "bin_hdr" sources
 * (tools/marvell/bin_hdr/src_ddr/ddr3_init.c and
 * tools/marvell/bin_hdr/inc/ddr3_soc/a375/ddr3_a375*.h) for the
 * DB-88F6720 board (single CS0, 512 MiB, DDR3 @ 533 MHz).
 */

#ifndef __A375_DDR3_INIT_H
#define __A375_DDR3_INIT_H

struct ddr3_mc_init_reg {
	u32 reg_addr;
	u32 reg_value;
};

/*
 * DB-88F6720 (Armada 375 32bit) 533 MHz static D-Unit configuration.
 * This is the exact register set used by the vendor bootloader for
 * this board (ddr3_A375_DB_6720_533[] in ddr3_a375_mc_static.h).
 */
#define A375_DB_6720_533_MC_INIT_REGS					\
{									\
	{ 0x00001400, 0x7b005040 }, /* SDRAM Configuration */		\
	{ 0x00001404, 0x36301820 }, /* Dunit Control Low */		\
	{ 0x00001408, 0x35137663 }, /* SDRAM Timing (Low) */		\
	{ 0x0000140c, 0x3809229f }, /* SDRAM Timing (High) */		\
	{ 0x00001410, 0x1b100001 }, /* SDRAM Open Pages Control */	\
	{ 0x00001414, 0x00000700 }, /* SDRAM Open Pages Control */	\
	{ 0x00001424, 0x0060f3ff }, /* Dunit Control High */		\
	{ 0x00001428, 0x000d6720 }, /* DDR ODT Timing (Low) */		\
	{ 0x0000142c, 0x028c50c3 }, /* DDR3 Timing */			\
	{ 0x0000147c, 0x0000b571 }, /* DDR ODT Timing (High) */	\
	{ 0x00001494, 0x00330000 }, /* SDRAM_SRAM_ODT_CNTL_LOW */	\
	{ 0x0000149c, 0x00000001 }, /* SDRAM_ODT_CONTROL */		\
	{ 0x000014a8, 0x00000000 }, /* AXI Control */			\
	{ 0x00001504, 0x3ffffff1 }, /* CS0 Size */			\
	{ 0x0000150c, 0x7ffffff0 }, /* CS1 Size (disabled) */		\
	{ 0x00001514, 0x00fffff0 }, /* CS2 Size (disabled) */		\
	{ 0x0000151c, 0x00fffff0 }, /* CS3 Size (disabled) */		\
	{ 0x00001538, 0x00000007 }, /* Read Data Sample Delays */	\
	{ 0x0000153c, 0x0000000b }, /* Read Data Ready Delay */	\
	{ 0x000015d0, 0x00000630 }, /* MR0 */				\
	{ 0x000015d4, 0x00000044 }, /* MR1 */				\
	{ 0x000015d8, 0x00000408 }, /* MR2 */				\
	{ 0x000015dc, 0x00000000 }, /* MR3 */				\
	{ 0x000015e0, 0x00000001 }, /* DDR3 Rank Control (CS0 only) */\
	{ 0x000015e4, 0x00203c18 }, /* ZQC Configuration */		\
	{ 0x000015ec, 0xd8000025 }, /* DDR PHY */			\
	{ 0x000014c0, 0x19242ddb }, /* DRAM addr/ctrl drive strength */\
	{ 0x000014c4, 0x092424c9 }, /* DRAM data/DQS drive strength */\
}

/* Board memory map: single CS0, 512 MiB */
#define A375_DB_6720_SDRAM_CS0_SIZE	SZ_512M

/* SDRAM controller registers */
#define REG_SDRAM_CONFIG_ADDR		0x1400
#define REG_SDRAM_INIT_CTRL_ADDR	0x1480
#define REG_SDRAM_INIT_CTRL_OFFS	0
#define REG_DRAM_AXI_CTRL_ADDR		0x14a8
#define REG_TRAINING_DEBUG_3_ADDR	0x15c8
#define REG_TRAINING_DEBUG_3_OFFS	3
#define REG_TRAINING_DEBUG_3_MASK	0x7
#define REG_DDR3_RANK_CTRL_ADDR		0x15e0
#define REG_DDR3_RANK_CTRL_CS_ENA_MASK	0xf

/* Dynamic Line Buffer (DLB) / MBUS-fabric arbitration registers */
#define REG_STATIC_DRAM_DLB_CONTROL		0x1700
#define DLB_BUS_OPTIMIZATION_WEIGHTS_REG	0x1704
#define DLB_AGING_REGISTER			0x1708
#define DLB_EVICTION_CONTROL_REG		0x170c
#define DLB_EVICTION_TIMERS_REGISTER_REG	0x1710
#define DLB_ENABLE				0x1
#define DLB_WRITE_COALESING			(0x1 << 2)
#define DLB_AXI_PREFETCH_EN			(0x1 << 3)
#define DLB_MBUS_PREFETCH_EN			(0x1 << 4)
#define DLB_PREFETCH_NEXT_LINE_SIZE_TR		(0x1 << 6)

#define MBUS_UNITS_PRIORITY_CONTROL_REG	0x20420
#define FABRIC_UNITS_PRIORITY_CONTROL_REG	0x20424
#define MBUS_UNITS_PREFETCH_CONTROL_REG	0x20428
#define FABRIC_UNITS_PREFETCH_CONTROL_REG	0x2042c

/* CPU->DRAM decode (fast path) windows and L2 filtering */
#define REG_FASTPATH_WIN_BASE_ADDR(win)	(0x20180 + (0x8 * (win)))
#define REG_FASTPATH_WIN_CTRL_ADDR(win)	(0x20184 + (0x8 * (win)))
#define REG_L2_FILTERING_ADDR			0x8c04

/* BootROM warm-boot indication (skip re-init on UART recovery re-entry) */
#define REG_BOOTROM_ROUTINE_ADDR		0x182d0
#define REG_BOOTROM_ROUTINE_DRAM_INIT_OFFS	12

#endif /* __A375_DDR3_INIT_H */
