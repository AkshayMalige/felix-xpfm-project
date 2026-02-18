# Step 6 — HLS Kernel → xclbin Demo (Jupyter)

Compile an HLS C++ kernel to `.xclbin` targeting the custom `xcvp1552` platform
from a Jupyter notebook. No GUI needed — just `v++` and Python.

---

## Prerequisites

| Tool | Version | Check |
|------|---------|-------|
| Vitis / v++ | 2024.2 | `v++ --version` |
| XRT (host) | matching | `xrt-smi examine` or `ls /opt/xilinx/xrt/` |
| Conda / Python | 3.8+ | `python --version` |
| JupyterLab | any | `jupyter lab --version` |

The custom platform (`.xpfm`) must have been built by **step3** first:
```
step3_vp1552/ws/custom_platform/export/custom_platform/custom_platform.xpfm
```

---

## Quick Start

```bash
# 1. Source tools (do this before launching Jupyter)
source /tools/Xilinx/Vitis/2024.2/settings64.sh
source /opt/xilinx/xrt/setup.sh

# 2. Launch Jupyter from this directory
cd step6_vp1552/
jupyter lab
```

Open `versal_kernel_build.ipynb` and run all cells top-to-bottom.

The platform path is **auto-detected** from the step3 directory. Override it with:
```bash
export VERSAL_XPFM=/path/to/your/custom_platform.xpfm
```

---

## Files

| File | Description |
|------|-------------|
| `versal_kernel_build.ipynb` | Demo notebook — write kernel, compile, inspect |
| `vitis_build.py` | Python wrapper around `v++` compile + link |
| `README.md` | This file |

Build artifacts are written to `./vitis_builds/` (git-ignored).

---

## Build Targets

| Target | Time | Use Case |
|--------|------|----------|
| `hw_emu` | 5-15 min | Functional verification (QEMU + RTL sim) |
| `hw` | 45-120 min | Real hardware — produces deployment `.xclbin` |

For a live demo use `hw_emu`. Pre-build `hw` overnight.

---

## HLS Interface Notes (Vitis 2024.2)

- `offset=slave` pragma is **deprecated** — omit it
- Each `m_axi bundle` maps to one AXI master port on the Versal NoC
- `s_axilite port=return` is required; scalar arguments are inferred automatically
- Use `const` on read-only pointer arguments to allow HLS to optimize memory access
