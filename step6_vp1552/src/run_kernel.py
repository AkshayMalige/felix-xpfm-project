#!/usr/bin/env python3
"""
run_kernel.py  <xclbin_path>  <kernel_name>  [n]

Works with vadd (c = a+b) and vmult (c = a*b).
Both kernels share the same ABI: (int* a, int* b, int* c, int n)

Examples:
  python3 run_kernel.py /tmp/vadd.xclbin  vadd   1024
  python3 run_kernel.py /tmp/vmult.xclbin vmult  1024
"""
import sys, numpy as np
import xrt


def run(xclbin_path: str, kernel_name: str, n: int = 1024) -> bool:
    size = n * 4  # int32 = 4 bytes
    a = np.arange(n, dtype=np.int32)
    b = np.full(n, 3, dtype=np.int32)

    dev  = xrt.device(0)
    xobj = xrt.xclbin(xclbin_path)
    uuid = dev.register_xclbin(xobj)
    ctx  = xrt.hw_context(dev, uuid)
    krnl = xrt.kernel(ctx, kernel_name)

    bo_a = xrt.bo(dev, size, xrt.bo.flags.normal, krnl.group_id(0))
    bo_b = xrt.bo(dev, size, xrt.bo.flags.normal, krnl.group_id(1))
    bo_c = xrt.bo(dev, size, xrt.bo.flags.normal, krnl.group_id(2))

    bo_a.write(a.tobytes(), 0)
    bo_b.write(b.tobytes(), 0)
    bo_a.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)
    bo_b.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_TO_DEVICE)

    krnl(bo_a, bo_b, bo_c, n).wait()

    bo_c.sync(xrt.xclBOSyncDirection.XCL_BO_SYNC_BO_FROM_DEVICE)
    c   = np.frombuffer(bo_c.read(size, 0), dtype=np.int32).copy()
    exp = (a + b) if kernel_name == "vadd" else (a * b)
    ok  = np.array_equal(c, exp)

    status = "PASS" if ok else "FAIL"
    print(f"[{status}] {kernel_name}  n={n}  c[0:4]={c[:4].tolist()}  expected={exp[:4].tolist()}")
    return ok


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(1)
    xclbin = sys.argv[1]
    kname  = sys.argv[2]
    n      = int(sys.argv[3]) if len(sys.argv) > 3 else 1024
    ok = run(xclbin, kname, n)
    sys.exit(0 if ok else 1)
