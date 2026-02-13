#/*
#Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
#SPDX-License-Identifier:MIT
#*/

include ../config.mk

ROOT_DIR=$(realpath $(dir $(lastword $(MAKEFILE_LIST))))
COMMON_IMAGE_VERSAL ?= $(LINUX_IMAGE_DIR)
XSA_PATH ?= $(XSA_DIR)
XSA_NAME ?= custom_hardware_platform

.PHONY: help

help:
	@echo "Makefile usage"
	@echo "make all  ## to generate platform with pre-built software components"


all: check-common-image pfm
sd_card:all

#target : check  whether the common image is ready
check-common-image:
ifeq ($(wildcard $(COMMON_IMAGE_VERSAL)/Image),)
	@echo "'COMMON_IMAGE_VERSAL' is not accessible, please download common image from Xilinx Downloads website and set this flag to specify the common image path."
	@make help
	@echo "exit ....." && exit 1
endif

# Target: pfm
# Generate the Vitis platform with XSCT
# Input: vck190_custom.xsa in build/vivado directory
pfm:
	@echo "INFO: Creating Final Platform $(PLATFORM_NAME)"
	@echo "set place for vitis to store configuration file "
	mkdir -p tmp && export XILINX_VITIS_DATA_DIR="tmp" && vitis -s platform_creation.py --platform_name $(PLATFORM_NAME) --xsa_path $(XSA_PATH)/$(XSA_NAME)_hw.xsa --xsa-emu_path $(XSA_PATH)/$(XSA_NAME)_hwemu.xsa --boot $(COMMON_IMAGE_VERSAL) --user_dtsi $(ROOT_DIR)/system-user.dtsi
	@echo "INFO: Checking created platforms:" && find . -name "*.xpfm"
	echo "Platform created:" && \
	find . -name "*.xpfm"

clean:
	$(RM) -rf  $(DTB_PLT_NAME) $(PLATFORM_NAME) vivado*.log vivado*.jou IDE* tmp ws
	-$(MAKE) -C platforminfo  clean

ultraclean:clean
	rm -rf  ws/

