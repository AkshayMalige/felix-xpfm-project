"""
vitis_build.py — Thin wrapper around Vitis v++ for Jupyter-based kernel compilation.

Usage:
    from vitis_build import VitisKernel
    vk = VitisKernel(platform="/path/to/custom.xpfm")
    xclbin = vk.build(hls_source_string, kernel_name="vadd", target="hw_emu")
"""

import subprocess
import os
import shutil
import time
from pathlib import Path
from typing import Optional


class VitisKernel:
    """Compile HLS C++ source to .xclbin against a Vitis platform."""

    def __init__(self, platform: str, build_root: str = "./vitis_builds",
                 rootfs: Optional[str] = None, kernel_image: Optional[str] = None):
        self.platform = os.path.abspath(platform)
        self.build_root = build_root

        if not os.path.exists(self.platform):
            raise FileNotFoundError(f"Platform not found: {self.platform}")

        if not shutil.which("v++"):
            raise EnvironmentError(
                "v++ not found on PATH. "
                "Source your Vitis settings64.sh before launching Jupyter."
            )

        os.makedirs(self.build_root, exist_ok=True)

        # Locate Linux boot artifacts for the package step (v++ -p).
        # In Vitis 2024.2, v++ -l produces .xsa; v++ -p then wraps it into .xclbin.
        # For embedded targets rootfs + kernel_image are needed by the packager.
        self.rootfs = rootfs
        self.kernel_image = kernel_image
        if self.rootfs is None or self.kernel_image is None:
            detected = self._detect_linux_images()
            if self.rootfs is None:
                self.rootfs = detected.get("rootfs")
            if self.kernel_image is None:
                self.kernel_image = detected.get("kernel_image")

        print(f"Platform     : {self.platform}")
        print(f"v++          : {shutil.which('v++')}")
        print(f"Build dir    : {os.path.abspath(self.build_root)}")
        if self.rootfs:
            print(f"rootfs       : {self.rootfs}")
        if self.kernel_image:
            print(f"kernel_image : {self.kernel_image}")

    def _detect_linux_images(self) -> dict:
        """Walk up from the platform path looking for step2 Linux images."""
        for parent in Path(self.platform).parents:
            linux_dir = parent / "step2_vp1552" / "my_foe_flx" / "images" / "linux"
            if linux_dir.exists():
                rootfs  = linux_dir / "rootfs.ext4"
                kernel  = linux_dir / "Image"
                if rootfs.exists() and kernel.exists():
                    return {"rootfs": str(rootfs), "kernel_image": str(kernel)}
        return {}

    def build(
        self,
        source: str,
        kernel_name: str = "krnl_custom",
        target: str = "hw_emu",
        clock_hz: Optional[int] = None,
        extra_vpp_flags: Optional[list] = None,
        clean: bool = False,
    ) -> str:
        """
        Compile HLS C++ source -> .xo -> .xsa -> .xclbin

        Vitis 2024.2 three-step flow:
          1. v++ -c  : HLS synthesis        (.cpp -> .xo)
          2. v++ -l  : system link/P&R      (.xo  -> .xsa)   [output MUST be .xsa]
          3. v++ -p  : package              (.xsa -> .xclbin)

        Args:
            source:          HLS C++ source code as a string
            kernel_name:     Top-level kernel function name (must match extern "C" function)
            target:          "hw_emu" or "hw"
            clock_hz:        HLS target clock frequency in Hz (e.g. 300_000_000 for 300 MHz).
                             If None, the platform default clock is used.
            extra_vpp_flags: Additional flags passed to all v++ steps
            clean:           Remove previous build artifacts before building

        Returns:
            Absolute path to the generated .xclbin
        """
        if target not in ("hw_emu", "hw"):
            raise ValueError(f"target must be 'hw_emu' or 'hw', got '{target}'")

        build_dir = os.path.join(self.build_root, f"{kernel_name}_{target}")
        if clean and os.path.exists(build_dir):
            shutil.rmtree(build_dir)
        os.makedirs(build_dir, exist_ok=True)

        src_path    = os.path.join(build_dir, f"{kernel_name}.cpp")
        xo_path     = os.path.join(build_dir, f"{kernel_name}.xo")
        xsa_path    = os.path.join(build_dir, f"{kernel_name}.xsa")
        xclbin_path = os.path.join(build_dir, f"{kernel_name}.xclbin")
        pkg_dir     = os.path.join(build_dir, "package")
        log_path    = os.path.join(build_dir, "build.log")

        with open(src_path, "w") as f:
            f.write(source)

        extra = extra_vpp_flags or []
        t_start = time.time()

        # --- Step 1: Compile .cpp -> .xo (HLS synthesis) ---
        print(f"[1/3] v++ -c  ({target}) {kernel_name}.cpp -> .xo ...")
        compile_cmd = [
            "v++", "-c",
            "-t", target,
            "--platform", self.platform,
            "-k", kernel_name,
            "--save-temps",
            "-o", xo_path,
            src_path,
        ]
        if clock_hz is not None:
            compile_cmd += ["--hls.clock", f"{clock_hz}:{kernel_name}"]
        compile_cmd += extra

        self._run(compile_cmd, log_path)
        t_compile = time.time() - t_start
        print(f"       Compile done ({t_compile:.0f}s)")

        # --- Step 2: Link .xo -> .xsa (system link / place & route) ---
        # NOTE: v++ 2024.2 requires the -l output to be .xsa, not .xclbin.
        print(f"[2/3] v++ -l  ({target}) .xo -> .xsa ...")
        link_cmd = [
            "v++", "-l",
            "-t", target,
            "--platform", self.platform,
            "--save-temps",
            "-o", xsa_path,
            xo_path,
        ] + extra

        self._run(link_cmd, log_path)
        t_link = time.time() - t_start
        print(f"       Link done ({t_link:.0f}s)")

        # --- Step 3: Package .xsa -> .xclbin ---
        print(f"[3/3] v++ -p  ({target}) .xsa -> .xclbin ...")
        pkg_cmd = [
            "v++", "-p",
            "-t", target,
            "--platform", self.platform,
            "--save-temps",
            "--package.out_dir", pkg_dir,
            "-o", xclbin_path,
            xsa_path,
        ]
        if self.rootfs:
            pkg_cmd += ["--package.rootfs", self.rootfs]
        if self.kernel_image:
            pkg_cmd += ["--package.kernel_image", self.kernel_image]
        pkg_cmd += extra

        self._run(pkg_cmd, log_path)
        t_total = time.time() - t_start

        size_kb = os.path.getsize(xclbin_path) / 1024
        print(f"Build complete in {t_total / 60:.1f} min")
        print(f"  xclbin : {os.path.abspath(xclbin_path)} ({size_kb:.0f} KB)")
        print(f"  log    : {os.path.abspath(log_path)}")

        return os.path.abspath(xclbin_path)

    def _run(self, cmd: list, log_path: str):
        """Run a command, stream warnings/errors to notebook, append full output to log."""
        with open(log_path, "a") as log:
            log.write(f"\n{'='*60}\n$ {' '.join(cmd)}\n{'='*60}\n")
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            for line in proc.stdout:
                log.write(line)
                stripped = line.strip()
                if any(k in stripped.lower() for k in ["error", "warning", "critical"]):
                    print(f"  >> {stripped}")
            proc.wait()
            if proc.returncode != 0:
                print(f"  !! Build failed (rc={proc.returncode}). Full log: {log_path}")
                raise subprocess.CalledProcessError(proc.returncode, cmd)


def list_builds(build_root: str = "./vitis_builds") -> list:
    """List all built xclbin files under build_root."""
    results = []
    for root, dirs, files in os.walk(build_root):
        for f in files:
            if f.endswith(".xclbin"):
                path = os.path.join(root, f)
                results.append({
                    "path": path,
                    "size_kb": os.path.getsize(path) / 1024,
                    "modified": time.ctime(os.path.getmtime(path)),
                })
    return results
