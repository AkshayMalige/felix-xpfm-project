#!/bin/bash
# build_host.sh — compile run_kernel for x86 (sw_emu/hw_emu) and aarch64 (hw)
#
# Usage:
#   ./build_host.sh          # build both
#   ./build_host.sh x86      # x86 only
#   ./build_host.sh aarch64  # aarch64 only
#
# Prerequisites:
#   source /tools/Xilinx/Vitis/2024.2/settings64.sh
#   source /opt/xilinx/xrt/setup.sh    (for x86 build)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SRC="$SCRIPT_DIR/src/run_kernel.cpp"

# Paths from config.mk
SDK_ENV="$PROJECT_ROOT/step2_vp1552/my_foe_flx/images/linux/sdk/environment-setup-cortexa72-cortexa53-xilinx-linux"
SYSROOT="$PROJECT_ROOT/step2_vp1552/my_foe_flx/images/linux/sdk/sysroots/cortexa72-cortexa53-xilinx-linux"
XRT_DIR="/opt/xilinx/xrt"

TARGET="${1:-both}"

# ---------- x86 build ----------
build_x86() {
    echo "=== Building x86 ==="
    if [ ! -d "$XRT_DIR/include" ]; then
        echo "WARNING: $XRT_DIR/include not found — skipping x86 build"
        echo "         (source /opt/xilinx/xrt/setup.sh first)"
        return
    fi
    g++ -O2 -std=c++14 \
        -I"$XRT_DIR/include" \
        -o "$SCRIPT_DIR/run_kernel_x86" \
        "$SRC" \
        -L"$XRT_DIR/lib" -lOpenCL -pthread
    echo "  => run_kernel_x86"
}

# ---------- aarch64 build ----------
# Use the Vitis cross-compiler directly (same as step4 Makefile).
# Avoids sourcing the SDK env script which fails when LD_LIBRARY_PATH is set.
build_aarch64() {
    echo "=== Building aarch64 (board) ==="
    CROSS_CXX="${XILINX_VITIS}/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-g++"
    if [ ! -f "$CROSS_CXX" ]; then
        echo "ERROR: Cross-compiler not found: $CROSS_CXX"
        echo "       source /tools/Xilinx/Vitis/2024.2/settings64.sh first"
        exit 1
    fi
    if [ ! -d "$SYSROOT" ]; then
        echo "ERROR: Sysroot not found: $SYSROOT"
        exit 1
    fi

    "$CROSS_CXX" -O2 -std=c++14 \
        -I"$SYSROOT/usr/include/xrt" \
        -I"${XILINX_VIVADO}/include" \
        --sysroot="$SYSROOT" \
        -o "$SCRIPT_DIR/run_kernel_aarch64" \
        "$SRC" \
        -L"$SYSROOT/usr/lib" -lxilinxopencl -pthread -lrt -lstdc++
    echo "  => run_kernel_aarch64"
}

case "$TARGET" in
    x86)     build_x86    ;;
    aarch64) build_aarch64 ;;
    both)    build_x86; build_aarch64 ;;
    *)       echo "Usage: $0 [x86|aarch64|both]"; exit 1 ;;
esac

echo ""
echo "Done. Copy run_kernel_aarch64 and your .xclbin to the board, then:"
echo "  ./run_kernel_aarch64 /path/to/vadd.xclbin  vadd  1024"
echo "  ./run_kernel_aarch64 /path/to/vmult.xclbin vmult 1024"
