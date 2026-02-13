# ============================================================================
# config.mk — Single configuration file for the entire project
#
# Edit the variables below to match your environment. All step Makefiles
# include this file so you only need to update paths here.
# ============================================================================

# Project root (auto-detected from this file's location)
PROJECT_ROOT := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

# ---------- User-configurable settings ----------

# PetaLinux project name (the directory name you passed to build_versal_linux.sh -n)
PETALINUX_PROJECT ?= my_foe_flx

# Vitis version
VERSION ?= 2024.2

# Platform name (matches what platform_creation.py creates)
PLATFORM_NAME ?= custom_platform

# ---------- Derived paths (no need to edit below this line) ----------

# Step 1 output: Vivado XSA files
XSA_DIR := $(PROJECT_ROOT)/step1_vp1552/build/vivado
XSA_NAME := custom_hardware_platform

# PetaLinux output: Linux images directory
LINUX_IMAGE_DIR := $(PROJECT_ROOT)/versal_vp1552/$(PETALINUX_PROJECT)/images/linux

# Step 2 output: Vitis platform (.xpfm)
PLATFORM := $(PROJECT_ROOT)/step2_vp1552/ws/$(PLATFORM_NAME)/export/$(PLATFORM_NAME)/$(PLATFORM_NAME).xpfm

# Sysroot for cross-compilation
SYSROOT := $(LINUX_IMAGE_DIR)/sdk/sysroots/cortexa72-cortexa53-xilinx-linux
