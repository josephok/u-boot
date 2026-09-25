// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Marvell International Ltd. and its affiliates
 *
 * Armada 375 DDR3 controller bring-up for the DB-88F6720 board, ported
 * from the Marvell u-boot-2013.01 "bin_hdr" sources
 * (tools/marvell/bin_hdr/src_ddr/ddr3_init.c, DUNIT_STATIC code path).
 *
 * IMPORTANT / KNOWN LIMITATION:
 * This only implements the "stage 1" static D-Unit setup + DRAM init
 * trigger + fast-path CPU->DRAM decode window, matching what the
 * original ddr3StaticMCInit() + DRAM-init + fast-path-window code did.
 *
 * It does NOT implement the original ddr3HwTraining() step (DDR3
 * write/read leveling and per-bit centralization/PBS), which the
 * original code still ran even in the "static" D-Unit configuration
 * (only the D-Unit *register calculation* is static here; signal
 * timing calibration is a separate, still-missing piece). Without it,
 * memory access may be marginal or unreliable depending on board/
 * silicon variance. Treat this as an intermediate bring-up step to be
 * followed by porting the HW training algorithm.
 */

#include <config.h>
#include <log.h>
#include <linux/delay.h>
#include <linux/sizes.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/arch/soc.h>

#include "ddr3_a375_regs.h"

static inline u32 reg_read(u32 offset)
{
	return readl(INTREG_BASE + offset);
}

static inline void reg_write(u32 offset, u32 val)
{
	writel(val, INTREG_BASE + offset);
}

static void ddr3_static_mc_init(void)
{
	static const struct ddr3_mc_init_reg regs[] = A375_DB_6720_533_MC_INIT_REGS;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(regs); i++)
		reg_write(regs[i].reg_addr, regs[i].reg_value);
}

/*
 * Fix read-ready phases for all lanes (0x15C8), as done unconditionally
 * for every SoC in the original ddr3Init_().
 */
static void ddr3_fix_read_ready_phases(void)
{
	u32 reg;

	reg = reg_read(REG_TRAINING_DEBUG_3_ADDR);
	reg &= ~REG_TRAINING_DEBUG_3_MASK;
	reg |= 0x4; /* phase 0 */
	reg &= ~(REG_TRAINING_DEBUG_3_MASK << REG_TRAINING_DEBUG_3_OFFS);
	reg |= 0x4 << (1 * REG_TRAINING_DEBUG_3_OFFS); /* phase 1 */
	reg &= ~(REG_TRAINING_DEBUG_3_MASK << (3 * REG_TRAINING_DEBUG_3_OFFS));
	reg |= 0x6 << (3 * REG_TRAINING_DEBUG_3_OFFS); /* phase 3 */
	reg &= ~(REG_TRAINING_DEBUG_3_MASK << (4 * REG_TRAINING_DEBUG_3_OFFS));
	reg |= 0x6 << (4 * REG_TRAINING_DEBUG_3_OFFS); /* phase 4 */
	reg &= ~(REG_TRAINING_DEBUG_3_MASK << (5 * REG_TRAINING_DEBUG_3_OFFS));
	reg |= 0x6 << (5 * REG_TRAINING_DEBUG_3_OFFS); /* phase 5 */
	reg_write(REG_TRAINING_DEBUG_3_ADDR, reg);
}

/*
 * DLB / MBUS-fabric arbitration setup, matching the
 * "!MV88F67XX && !MV_NEW_TIP" branch of the original code (applies to
 * Armada 375 too).
 */
static void ddr3_dlb_config(void)
{
	u32 reg;

	reg_write(DLB_BUS_OPTIMIZATION_WEIGHTS_REG, 0x18c01e);
	reg_write(DLB_AGING_REGISTER, 0x0f7f007f);
	reg_write(DLB_EVICTION_CONTROL_REG, 0x0);
	reg_write(DLB_EVICTION_TIMERS_REGISTER_REG, 0x00ff3c1f);
	reg_write(MBUS_UNITS_PRIORITY_CONTROL_REG, 0x55555555);
	reg_write(MBUS_UNITS_PREFETCH_CONTROL_REG, 0xffff);
	reg_write(FABRIC_UNITS_PREFETCH_CONTROL_REG, 0xf0f);

	reg = reg_read(REG_STATIC_DRAM_DLB_CONTROL);
	reg |= DLB_ENABLE | DLB_WRITE_COALESING | DLB_AXI_PREFETCH_EN |
		DLB_MBUS_PREFETCH_EN | DLB_PREFETCH_NEXT_LINE_SIZE_TR;
	reg_write(REG_STATIC_DRAM_DLB_CONTROL, reg);
}

/*
 * Trigger the DDR3 SDRAM device initialization sequence (RESET#, CKE,
 * MRS commands using the MR0..MR3 values already written by the
 * static table) and wait for the memory controller to signal it is
 * done.
 */
static void ddr3_trigger_sdram_init(void)
{
	u32 reg;

	reg_write(REG_SDRAM_INIT_CTRL_ADDR, 0x1);
	do {
		reg = reg_read(REG_SDRAM_INIT_CTRL_ADDR) &
			(1 << REG_SDRAM_INIT_CTRL_OFFS);
	} while (reg);
}

/*
 * Open the CPU->DRAM fast-path decode window for CS0 (512 MiB) and set
 * the L2 filtering range accordingly. Without this the CPU cannot
 * actually access the newly initialized DRAM.
 */
static void ddr3_open_fastpath_window(void)
{
	u32 reg;

	reg = 0xffffe1;
	reg |= (A375_DB_6720_SDRAM_CS0_SIZE - 1) & 0xffff0000;
	reg_write(REG_FASTPATH_WIN_CTRL_ADDR(0), reg);
	reg_write(REG_FASTPATH_WIN_BASE_ADDR(0), 0);

	reg_write(REG_L2_FILTERING_ADDR, A375_DB_6720_SDRAM_CS0_SIZE);
}

int ddr3_init(void)
{
	/* Check if DRAM is already initialized (e.g. UART recovery re-entry) */
	if (reg_read(REG_BOOTROM_ROUTINE_ADDR) &
	    (1 << REG_BOOTROM_ROUTINE_DRAM_INIT_OFFS)) {
		debug("DDR3 Training Sequence - 2nd boot - Skip\n");
		return 0;
	}

	printf("DDR3 Training Sequence - Static D-Unit Setup (DB-88F6720, 533 MHz)\n");

	ddr3_static_mc_init();
	ddr3_fix_read_ready_phases();
	ddr3_dlb_config();
	ddr3_trigger_sdram_init();
	ddr3_open_fastpath_window();

	/* Mark DRAM as initialized for BootROM warm-boot detection */
	reg_write(REG_BOOTROM_ROUTINE_ADDR,
		  reg_read(REG_BOOTROM_ROUTINE_ADDR) |
		  (1 << REG_BOOTROM_ROUTINE_DRAM_INIT_OFFS));

	printf("DDR3 Training Sequence - Ended (static config, no HW training yet)\n");

	return 0;
}
