#!/bin/bash
#
# db-88f6720-build.sh - Build a fully-working u-boot-with-spl.kwb for the
# Marvell DB-88F6720 (Armada 375) board, chaining the vendor's closed-source
# "bin_hdr" blob (SerDes PHY lane config + DDR3 HW training) ahead of
# mainline U-Boot's own SPL.
#
# ---------------------------------------------------------------------------
# WHY THIS SCRIPT EXISTS
# ---------------------------------------------------------------------------
# Armada 375 DDR3 write/read-leveling training has no open-source
# implementation anywhere (it only exists as a precompiled, closed-source
# library in the vendor tree: tools/marvell/bin_hdr/src_ddr/lib/
# ddr3_training_a375.lib). Mainline U-Boot therefore cannot train DRAM on
# this SoC by itself. The workaround is to let the BootROM execute the
# vendor's prebuilt "bin_hdr.elf" binary (which performs SerDes + DDR3 init)
# BEFORE mainline's own SPL runs, by chaining both into the kwbimage's
# BINARY extension header.
#
# ---------------------------------------------------------------------------
# PREREQUISITES
# ---------------------------------------------------------------------------
# 1. BIN_HDR_ELF (below) must point at a prebuilt vendor bin_hdr.elf for
#    this board. A local copy is tracked at
#    board/Marvell/db-88f6720/vendor-blobs/bin_hdr.elf (explicitly
#    committed to this repository, as an exception to the top-level
#    "*.elf" .gitignore rule - see .gitignore for the exact rule/comment).
#
#    NOTE: bin_hdr.elf CANNOT be rebuilt with a modern toolchain - it comes
#    from an ancient (2013.01-based) Marvell vendor U-Boot tree that only
#    builds with the old Marvell/Linaro GCC 4.6.4/4.8.4 toolchain (modern
#    gcc fails with "linux/compiler-gcc16.h: No such file or directory" and
#    similar). If you ever need to rebuild it yourself (e.g. for a
#    different board/config), see the reference vendor tree and build
#    instructions at: https://github.com/josephok/u-boot-marvell
#    (config target "armada_375_config", old toolchain), then copy the
#    resulting bin_hdr.elf to that path. This script does NOT attempt that
#    build for you.
#
# 2. Must be run from the root of this mainline u-boot checkout
#    (arm-linux-gnu- toolchain, already configured for db-88f6720_defconfig
#    at least once).
#
# ---------------------------------------------------------------------------
# USAGE
# ---------------------------------------------------------------------------
#   ./db-88f6720-build.sh
#
# Optional environment variable overrides:
#   BIN_HDR_ELF=/path/to/bin_hdr.elf
#   CROSS_COMPILE=arm-linux-gnu-
#   JOBS=$(nproc)
#
# Output: u-boot-with-spl.kwb in the current directory (ready for
# "kwboot -b u-boot-with-spl.kwb -t -B 115200 /dev/ttyUSBx" or SPI flashing).
# ---------------------------------------------------------------------------

set -euo pipefail

# Must be run from the u-boot checkout root; sanity-check that first.
if [ ! -f "board/Marvell/db-88f6720/db-88f6720.c" ]; then
	echo "ERROR: run this script from the root of the u-boot checkout" >&2
	echo "(board/Marvell/db-88f6720/db-88f6720.c not found in $(pwd))." >&2
	exit 1
fi

BIN_HDR_ELF="${BIN_HDR_ELF:-board/Marvell/db-88f6720/vendor-blobs/bin_hdr.elf}"
CROSS_COMPILE="${CROSS_COMPILE:-arm-linux-gnu-}"
JOBS="${JOBS:-$(nproc)}"

# This fixed padded size (bytes) is how binary.0's size is decoupled from
# CONFIG_SPL_TEXT_BASE, regardless of the real bin_hdr.elf's exact size -
# both this script and the defconfig must agree on it. It was derived as:
#
#   BINARY0_PAD_SIZE = CONFIG_SPL_TEXT_BASE - LOAD_ADDRESS - 12
#
# where LOAD_ADDRESS (0x40000030) is the bin_hdr BINARY entry's fixed load
# address (see KWB_CFG_BINARY in arch/arm/mach-mvebu/Makefile), and 12 bytes
# is the kwbimage tool's empirically-measured per-BINARY-header overhead
# (found by trial: padding to exactly SPL_TEXT_BASE - LOAD_ADDRESS made the
# tool reject the resulting SPL_TEXT_BASE as 12 bytes too low).
#
# With CONFIG_SPL_TEXT_BASE=0x40015588 (configs/db-88f6720_defconfig):
#   0x40015588 - 0x40000030 - 12 (0xc) = 0x1554c
#
# If CONFIG_SPL_TEXT_BASE ever changes, recompute BINARY0_PAD_SIZE with this
# same formula and update it here too.
BINARY0_PAD_SIZE=0x1554c

BIN_HDR_ELF_PATH="${BIN_HDR_ELF}"
BINARY0_DEST="board/Marvell/db-88f6720/binary.0"

echo "==> Checking vendor bin_hdr.elf"
if [ ! -f "${BIN_HDR_ELF_PATH}" ]; then
	echo "ERROR: ${BIN_HDR_ELF_PATH} not found." >&2
	echo "Copy a prebuilt bin_hdr.elf there first (see script header comments)." >&2
	exit 1
fi

echo "==> Extracting binary.0 from bin_hdr.elf"
TMP_RAW="$(mktemp)"
TMP_PADDED="$(mktemp)"
trap 'rm -f "${TMP_RAW}" "${TMP_PADDED}"' EXIT

"${CROSS_COMPILE}objcopy" -S -O binary "${BIN_HDR_ELF_PATH}" "${TMP_RAW}"
RAW_SIZE=$(stat -c %s "${TMP_RAW}")
PAD_SIZE_DEC=$((BINARY0_PAD_SIZE))
echo "    extracted size: ${RAW_SIZE} bytes (pad target: ${PAD_SIZE_DEC} bytes)"

if [ "${RAW_SIZE}" -gt "${PAD_SIZE_DEC}" ]; then
	echo "ERROR: extracted bin_hdr (${RAW_SIZE} bytes) no longer fits in the" >&2
	echo "fixed ${PAD_SIZE_DEC}-byte budget. You must recompute" >&2
	echo "CONFIG_SPL_TEXT_BASE in configs/db-88f6720_defconfig for this new" >&2
	echo "size (attempt the build below, read the valid address range from" >&2
	echo "the kwbimage tool's error message, update the defconfig, update" >&2
	echo "BINARY0_PAD_SIZE here, then re-run this script)." >&2
	exit 1
fi

echo "==> Padding binary.0 to fixed size ${PAD_SIZE_DEC} bytes"
"${CROSS_COMPILE}objcopy" -I binary -O binary --pad-to="${BINARY0_PAD_SIZE}" \
	"${TMP_RAW}" "${TMP_PADDED}"

echo "==> Installing binary.0 into ${BINARY0_DEST} (working tree only, not staged in git)"
cp "${TMP_PADDED}" "${BINARY0_DEST}"

echo "==> Configuring db-88f6720_defconfig"
make ARCH=arm CROSS_COMPILE="${CROSS_COMPILE}" db-88f6720_defconfig

echo "==> Building u-boot-with-spl.kwb (-j${JOBS})"
make ARCH=arm CROSS_COMPILE="${CROSS_COMPILE}" -j"${JOBS}"

echo "==> Done: u-boot-with-spl.kwb"
ls -la u-boot-with-spl.kwb
