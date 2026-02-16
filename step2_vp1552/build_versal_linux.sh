#!/bin/bash

# ==============================================================================
# Versal Linux Builder Script
# Automated PetaLinux 2025.2 flow for Custom Versal Platforms
# ==============================================================================

set -e

# ================= Configuration =================
PROJECT_NAME=""
XSA_PATH=""
SKIP_BUILD=0

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
CFG_TOOL="${SCRIPT_DIR}/config"

usage() {
    echo "Usage: $0 -n <project_name> -x <xsa_path> [options]"
    exit 1
}

while [[ "$#" -gt 0 ]]; do
    case $1 in
        -n|--name) PROJECT_NAME="$2"; shift ;;
        -x|--xsa) XSA_PATH=$(readlink -f "$2"); shift ;;
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help) usage ;;
        *) usage ;;
    esac
    shift
done

if [ -z "$PROJECT_NAME" ] || [ -z "$XSA_PATH" ]; then usage; fi

# ================= 1. Create Project =================
if [ -d "$PROJECT_NAME" ]; then
    echo "Warning: Directory $PROJECT_NAME exists. Deleting..."
    rm -rf "$PROJECT_NAME"
fi

petalinux-create --type project --template versal --name "$PROJECT_NAME"
cd "$PROJECT_NAME"

# ================= 2. Import Hardware =================
petalinux-config --get-hw-description="$XSA_PATH" --silentconfig

# ================= 3. Configure Project =================
ROOTFS_CFG="project-spec/configs/rootfs_config"
MAIN_CFG="project-spec/configs/config"

# --- Essential Vitis/XRT Configs ---
"$CFG_TOOL" --file "$ROOTFS_CFG" --keep-case --enable xrt
"$CFG_TOOL" --file "$ROOTFS_CFG" --keep-case --enable xrt-dev
"$CFG_TOOL" --file "$ROOTFS_CFG" --keep-case --enable packagegroup-petalinux-openamp
"$CFG_TOOL" --file "$ROOTFS_CFG" --keep-case --enable dnf
"$CFG_TOOL" --file "$ROOTFS_CFG" --keep-case --enable opencl-clhpp-dev
"$CFG_TOOL" --file "$ROOTFS_CFG" --keep-case --enable opencl-headers
"$CFG_TOOL" --file "$ROOTFS_CFG" --keep-case --enable zocl

# --- Boot & Image Settings ---
"$CFG_TOOL" --file "$MAIN_CFG" --keep-case --disable CONFIG_SUBSYSTEM_ROOTFS_INITRAMFS
"$CFG_TOOL" --file "$MAIN_CFG" --keep-case --enable CONFIG_SUBSYSTEM_ROOTFS_EXT4
"$CFG_TOOL" --file "$MAIN_CFG" --set-str CONFIG_SUBSYSTEM_RFS_FORMATS "ext4 tar.gz"
"$CFG_TOOL" --file "$MAIN_CFG" --set-str CONFIG_SUBSYSTEM_SDROOT_DEV "/dev/mmcblk0p2"
"$CFG_TOOL" --file "$MAIN_CFG" --set-str CONFIG_SUBSYSTEM_USER_CMDLINE "console=ttyAMA0 earlycon=pl011,mmio32,0xFF010000,115200n8 clk_ignore_unused root=/dev/mmcblk0p2 rw rootwait cma=512M cpuidle.off=1"

petalinux-config --silentconfig
petalinux-config -c rootfs --silentconfig

if [ "$SKIP_BUILD" -eq 1 ]; then exit 0; fi

# ================= 4. Build System =================
petalinux-build
petalinux-build --sdk
petalinux-package --sysroot

# ================= 5. Package Boot (Optional, for testing) =================
# We generate a BOOT.BIN just for sanity check, but Vitis will make its own later.
petalinux-package --boot --plm --psmfw --u-boot --dtb --force

# ================= 6. Organize Artifacts (THE FIX) =================
echo "[6/6] Organizing Artifacts for Vitis..."

# Define output directories
SW_DIR="$PWD/sw"
BOOT_DIR="$SW_DIR/boot"
IMAGE_DIR="$SW_DIR/image" # Vitis often treats boot/image as the same source for 'sd_dir'

mkdir -p "$BOOT_DIR"
mkdir -p "$IMAGE_DIR"

# 1. Boot Loaders (Required for BOOT.BIN generation)
cp images/linux/plm.elf "$BOOT_DIR/"
cp images/linux/psmfw.elf "$BOOT_DIR/"
cp images/linux/bl31.elf "$BOOT_DIR/"
cp images/linux/u-boot.elf "$BOOT_DIR/"
cp images/linux/system.dtb "$BOOT_DIR/"
cp images/linux/pmc_cdo.bin "$BOOT_DIR/"

# 2. QEMU Specific DTBs (CRITICAL for Custom Board Emulation)
# PetaLinux generates these. We MUST provide them to Vitis.
cp images/linux/versal-qemu-multiarch-pmc.dtb "$BOOT_DIR/"
cp images/linux/versal-qemu-multiarch-ps.dtb "$BOOT_DIR/"

# 3. SD Card Files (Kernel, RootFS, Script)
cp images/linux/boot.scr "$IMAGE_DIR/"
cp images/linux/Image "$IMAGE_DIR/"
cp images/linux/rootfs.ext4 "$IMAGE_DIR/"

echo "=========================================="
echo " Build Success!"
echo " Artifacts located in: $SW_DIR"
echo " Use this path in your gen_plat.py"
echo "=========================================="