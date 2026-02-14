#/*
#Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
#SPDX-License-Identifier: MIT
#*/

ROOT_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))

include ../config.mk
VADD_WORK=$(ROOT_DIR)/vadd_work

.PHONY: help

help:
	@echo "Makefile usage"
	@echo "make vadd_emu  ## Build and run vadd in hw_emu (QEMU + RTL sim)"
	@echo "make vadd_hw   ## Build vadd for hardware (sd_card target)"

# Set up vadd_work from local src/ directory
setup:
	mkdir -p vadd_work
	cp src/* vadd_work/
	cp makefile_vadd vadd_work/Makefile

vadd_emu: setup
	$(MAKE) -C $(VADD_WORK) run TARGET=hw_emu PLATFORM=$(PLATFORM) DEV_ARCH=versal HOST_ARCH=aarch64 EDGE_COMMON_SW=$(LINUX_IMAGE_DIR) SYSROOT=$(SYSROOT)/

vadd_hw: setup
	$(MAKE) -C $(VADD_WORK) sd_card TARGET=hw PLATFORM=$(PLATFORM) DEV_ARCH=versal HOST_ARCH=aarch64 EDGE_COMMON_SW=$(LINUX_IMAGE_DIR) SYSROOT=$(SYSROOT)/

clean:
	$(MAKE) -C $(VADD_WORK) clean

ultraclean:
	rm -rf logs
	rm -rf $(VADD_WORK)
