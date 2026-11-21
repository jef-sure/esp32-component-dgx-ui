#!/usr/bin/env python3
"""
check_packed.py <DGX> <work directory>: the component packed as the registry takes it (compote component pack),
unpacked, and the preview's module made again there by `make check` of editor/preview: dgx_ui_preview.js and licenses/
the same, byte for byte, as in the package, so that anyone who has the package makes exactly what it holds. <DGX> is
the registry's copy of the version dgx-ui asks for. Run where compote is on the PATH, in ESP-IDF's environment, and with
the toolchain the Makefile names.
"""
import glob
import os
import shutil
import subprocess
import sys
import tarfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
dgx, work = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
shutil.rmtree(work, ignore_errors=True)
os.makedirs(work)


def run(command, where):
    said = subprocess.run(command, cwd=where, capture_output=True, text=True, timeout=1800)
    if said.returncode:
        sys.exit(f"check_packed.py: {' '.join(command)}: {(said.stdout + said.stderr)[-3000:]}")
    return said.stdout


run(["compote", "component", "pack", "--name", "dgx-ui", "--dest-dir", os.path.join(work, "pack")], ROOT)
archives = glob.glob(os.path.join(work, "pack", "*.tgz"))
if len(archives) != 1:
    sys.exit(f"check_packed.py: {len(archives)} archives packed, not one")
unpacked = os.path.join(work, "unpacked")
with tarfile.open(archives[0]) as archive:
    archive.extractall(unpacked, filter="data")
said = run(["make", "-C", os.path.join(unpacked, "editor", "preview"), f"DGX={dgx}", f"BUILD={os.path.join(work, 'build')}", "check"], unpacked)
if "made again the same, byte for byte" not in said:
    sys.exit("check_packed.py: make check said nothing of the bytes: " + said[-2000:])
print(f"check_packed.py: {os.path.basename(archives[0])} unpacked makes its preview again, the same byte for byte")
