# Felix XCVP1552 Extensible Platform Project

Custom Vitis extensible platform for the AMD/Xilinx **XCVP1552** (Versal Premium VP1552) targeting the `vsva3340-2MHP-e-S` package. Built and tested with **Vitis / PetaLinux 2024.2**.

## Project Structure

```
felix-xpfm-project/
├── config.mk               # ** Edit this file to configure all paths **
├── step1_vp1552/            # Vivado hardware design (XSA generation)
├── step2_vp1552/            # PetaLinux build (Linux images + sysroot)
├── step3_vp1552/            # Vitis platform creation (.xpfm)
└── step4_vp1552/            # Vitis application build & emulation (vadd)
```

Build order: **step1 → step2 → step3 → step4**.

## Quick Start — Path Configuration

All paths are defined in **one file**: `config.mk` at the project root. The `step3` and `step4` Makefiles include it automatically.

After cloning, the only thing you may need to edit is:

```makefile
# config.mk
PETALINUX_PROJECT ?= my_flx_lnx    # name you passed to build_versal_linux.sh -n
```

Everything else (XSA path, platform path, sysroot, linux images) is derived from the project layout. If you keep the default directory structure and project name, it works out of the box.

---

## Prerequisites

- AMD Vitis 2024.2 (includes Vivado, v++, HLS)
- PetaLinux 2024.2
- Source the Vitis and PetaLinux `settings.sh` before running any step

---

## Step 1 — Vivado Hardware Design (`step1_vp1552/`)

Creates the extensible hardware platform and exports two XSA files (one for hardware, one for hw_emu).

| File | Purpose |
|------|---------|
| `run.tcl` | Vivado batch Tcl — creates the block design using the `ext_platform_part` example, configures CIPS (PS) and NoC/DDR4, applies pin constraints, writes `_hw.xsa` and `_hwemu.xsa` |
| `pinout.xdc` | Pin constraints for the VP1552 board (DDR4, SD, UART, Ethernet, USB, I2C) |
| `Makefile` | Runs `vivado -mode batch -source run.tcl` |

Key design choices in `run.tcl`:
- Part: `xcvp1552-vsva3340-2MHP-e-S`
- DDR4: config17, dual-rank UDIMM DDR4-2666V, 72-bit, 200 MHz ref clock (FLX-155 board pinout)
- LPDDR4: removed (not present on FLX-155 board)
- PS peripherals: SD 3.0, GbE (RGMII), USB 3.0, UART, I2C x2
- PL: 3 platform clocks (see table below), 32 interrupts

### Vitis Region Platform Clocks

The platform exports three clocks to the Vitis region via `clk_wizard_0` (source: CIPS PMC PL0_REF_CLK at 100 MHz):

| Clock Output | Clock ID | Frequency | Default | Reset Block |
|---|---|---|---|---|
| `clk_out1` | 0 | ~156 MHz | No | `proc_sys_reset_0` |
| `clk_out2` | 2 | ~300 MHz | **Yes** | `proc_sys_reset_1` |
| `clk_out3` | 3 | ~500 MHz | No | `proc_sys_reset_2` |

By default, all Vitis kernels use the **default clock (id=2, ~300 MHz)**. To target a different clock during `v++ --link`:

```bash
# Use 500 MHz for all kernels:
v++ -l --clock.defaultFreqHz 500000000 ...

# Assign a specific kernel to a specific clock id:
v++ -l --clock.id 3:krnl_vadd_1 ...
```

```bash
cd step1_vp1552
make all
# Output: build/vivado/custom_hardware_platform_hw.xsa
#         build/vivado/custom_hardware_platform_hwemu.xsa
```

---

## Step 2 — PetaLinux Build (`step2_vp1552/`)

Builds the Linux system: kernel Image, rootfs.ext4, boot firmware, and the cross-compilation sysroot.

| File | Purpose |
|------|---------|
| `build_versal_linux.sh` | Automated PetaLinux flow: create project → import XSA → configure rootfs (XRT, OpenAMP) → build → generate SDK/sysroot → package boot |
| `config` | Kconfig-style helper script for manipulating PetaLinux `.config` files |

```bash
cd step2_vp1552
./build_versal_linux.sh -n my_flx_lnx -x ../step1_vp1552/build/vivado/custom_hardware_platform_hw.xsa
```

Produces (inside `my_flx_lnx/`):
- `images/linux/Image` — Linux kernel
- `images/linux/rootfs.ext4` — Root filesystem with XRT
- `images/linux/sdk/sysroots/cortexa72-cortexa53-xilinx-linux/` — Cross-compilation sysroot
- Boot firmware (plm.elf, psmfw.elf, bl31.elf, u-boot.elf, QEMU DTBs)

> The PetaLinux build output (~61 GB) is not included in this repository.

---

## Step 3 — Vitis Platform Creation (`step3_vp1552/`)

Combines the Vivado XSA + PetaLinux boot artifacts into a Vitis extensible platform (`.xpfm`).

| File | Purpose |
|------|---------|
| `platform_creation.py` | Python script using the Vitis API to create the platform, configure the XRT Linux domain, and build |
| `system-user.dtsi` | Custom device tree overlay for the VP1552 board (GbE PHY, SD, I2C, USB) |
| `Makefile` | Includes `config.mk`, invokes `vitis -s platform_creation.py` |

```bash
cd step3_vp1552
make all
# Output: ws/custom_platform/export/custom_platform/custom_platform.xpfm
```

---

## Step 4 — Application Build & Emulation (`step4_vp1552/`)

Builds and runs the AMD `vadd` example on the custom platform in hardware emulation.

| File | Purpose |
|------|---------|
| `Makefile` | Includes `config.mk`, sets up vadd_work from local `src/`, invokes the sub-make |
| `makefile_vadd` | Inner Makefile with the full v++ compile/link/package/run flow |
| `run_vadd.sh` | Script executed inside QEMU — runs `simple_vadd` with hw_emu |
| `src/krnl_vadd.cpp` | HLS kernel — vector addition accelerator for PL |
| `src/vadd.cpp` | Host application — OpenCL driver running on PS (A72) |
| `src/vadd.h` | OpenCL header with aligned buffer allocator |

### How vadd runs on the Felix CIPS-NoC-DDR4 platform

The `vadd` example exercises the full Versal data path: the **A72 PS** (running Linux + XRT) sends commands through the **CIPS → NoC → PL** path, while the PL kernel reads/writes vectors through the **PL → NoC → DDR4** path.

```
┌─────────────────────────────────────────────────────────────────────┐
│  PS (Cortex-A72)                                                    │
│  ┌──────────────────────────────────────────────────────┐           │
│  │  simple_vadd (host app)                              │           │
│  │  1. Allocates buffers in1[], in2[], out[] in DDR4    │           │
│  │  2. Enqueues kernel via OpenCL / XRT                 │           │
│  │  3. Reads back out[] and verifies                    │           │
│  └──────────────┬───────────────────────────────────────┘           │
│                 │ AXI-FPD (CIPS → cips_noc → PL)                    │
├─────────────────┼───────────────────────────────────────────────────┤
│  NoC            │                                                   │
│            ┌────▼────┐                                              │
│            │ cips_noc│◄─── FPD-AXI, CCI, LPD paths                  │
│            └────┬────┘                                              │
│                 │ NMI (NoC Master Interface)                        │
│            ┌────▼────┐                                              │
│            │ noc_ddr4│                                              │
│            └────┬────┘                                              │
├─────────────────┼───────────────────────────────────────────────────┤
│  DDR4           │                                                   │
│       ┌─────────▼──────────┐                                        │
│       │  in1[]  in2[]  out[]  (buffers in DDR4)                     │
│       └─────────▲──────────┘                                        │
│                 │ M_AXI gmem0 / gmem1 (PL → noc_ddr4 → DDR4)        │
├─────────────────┼───────────────────────────────────────────────────┤
│  PL (krnl_vadd) │                                                   │
│                 │                                                   │
│    ┌────────────┴──────────────────────────────────────┐            │
│    │          HLS dataflow pipeline (krnl_vadd)        │            │
│    │                                                   │            │
│    │          _____________                            │            │
│    │         |             |<── in1[] via gmem0        │            │
│    │         |  load_input |       __                  │            │
│    │         |_____________|----->|  |                 │            │
│    │          _____________       |  | in1_stream      │            │
│    │  in2[] ─|             |      |__|                 │            │
│    │  via  __|  load_input |        |                  │            │
│    │  gmem1 |_____________|        |                   │            │
│    │   __                          |                   │            │
│    │  |  |<────────────────────────                    │            │
│    │  |  | in2_stream                                  │            │
│    │  |__|     _____________                           │            │
│    │     └───>|             |                          │            │
│    │          | compute_add |      __                  │            │
│    │          |_____________|---->|  |                 │            │
│    │           ______________     |  | out_stream      │            │
│    │          |              |<---|__|                 │            │
│    │          | store_result |                         │            │
│    │          |______________|──> out[] via gmem0      │            │
│    │                                                   │            │
│    └───────────────────────────────────────────────────┘            │
└─────────────────────────────────────────────────────────────────────┘
```

The kernel uses two AXI master ports (`gmem0`, `gmem1`) so `load_input` for in1 and in2 can read simultaneously. HLS `#pragma dataflow` pipelines all four stages — both loads, the add, and the store run concurrently through FIFO streams.

```bash
cd step4_vp1552
make vadd_emu
```

This runs the entire flow:

```
krnl_vadd.cpp ──v++ -c──► krnl_vadd.xo ──v++ -l──► krnl_vadd.link.xsa
                                                                 │
vadd.cpp ──aarch64-g++──► simple_vadd                            │
                              │                                  │
                              ▼                                  ▼
                         v++ -p (package with rootfs, Image, xclbin)
                              │
                              ▼
                     package.hw_emu/
                     ├── launch_hw_emu.sh    ← starts QEMU + XSIM
                     ├── rootfs.ext4
                     ├── Image
                     ├── simple_vadd         ← host app (aarch64)
                     ├── krnl_vadd.xclbin    ← PL bitstream for sim
                     └── run_vadd.sh         ← runs inside QEMU
                              │
                              ▼
                   QEMU (PS/ARM) ◄──TLM bridge──► XSIM (PL/RTL)
                         │
                    "TEST PASSED"
```

| # | Command | What it does |
|---|---------|--------------|
| 1 | `v++ -c` | HLS-synthesizes `krnl_vadd.cpp` → compiled kernel object (`.xo`) |
| 2 | `v++ -l` | Links kernel into platform → linked design (`.xsa`) |
| 3 | `aarch64-linux-gnu-g++` | Cross-compiles host app → `simple_vadd` (aarch64 ELF) |
| 4 | `emconfigutil` | Generates `emconfig.json` (XRT emulation config) |
| 5 | `v++ -p` | Packages everything (xclbin + rootfs + Image + app) for QEMU |
| 6 | `launch_hw_emu.sh` | Boots QEMU + RTL simulator, runs `run_vadd.sh` |

---

## License

Source files are under the MIT license (AMD/Xilinx copyright). The helper scripts are under the X11 license.
