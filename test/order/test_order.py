#!/usr/bin/env python3
"""
test_order.py <dgx-ui directory> <work directory>: the made C of a component that names a picture of a file waits for
DGX's rule that makes the file, whatever the order project_include.cmake of dgx-ui and DGX's defer their calls in. On
CMake and Ninja alone (CMakeLists.txt beside it): the component waits for dgx_images_made; a clean build with -j makes the
file before the made C; the file deleted, both made again; the picture changed, the made C made again with it.
"""
import os
import shutil
import subprocess
import sys

dgx_ui, work = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
here = os.path.dirname(os.path.abspath(__file__))
failures = []


def check(held, what):
    if not held:
        failures.append(what)
        print("  failed: " + what, file=sys.stderr)


source = os.path.join(work, "source")
shutil.rmtree(work, ignore_errors=True)
shutil.copytree(here, source, ignore=shutil.ignore_patterns("*.py", "__pycache__"))
build = os.path.join(work, "build")
run = subprocess.run(["cmake", "-G", "Ninja", "-S", source, "-B", build, f"-DDGX_UI={dgx_ui}"], capture_output=True, text=True, timeout=120)
check(run.returncode == 0, "configured: " + run.stdout[-1500:] + run.stderr[-1500:])
waits = open(os.path.join(build, "waits.txt")).read() if os.path.exists(os.path.join(build, "waits.txt")) else ""
check("dgx_images_made" in waits, f"the component waits for DGX's rule: it waits for \"{waits}\"")


def made():
    path = os.path.join(build, "made.c")
    return open(path).read() if os.path.exists(path) else ""


run = subprocess.run(["ninja", "-C", build, "-j", "8"], capture_output=True, text=True, timeout=120)
check(run.returncode == 0 and "version 1" in made(), "a clean build with -j: " + run.stdout[-1500:])
os.remove(os.path.join(build, "dgx_images", "logo.dgxi"))
run = subprocess.run(["ninja", "-C", build, "-j", "8"], capture_output=True, text=True, timeout=120)
check(run.returncode == 0 and os.path.exists(os.path.join(build, "dgx_images", "logo.dgxi")) and "version 1" in made(), "the file deleted, made again: " + run.stdout[-1500:])
with open(os.path.join(source, "logo.txt"), "w") as out:
    out.write("the picture, version 2\n")
run = subprocess.run(["ninja", "-C", build, "-j", "8"], capture_output=True, text=True, timeout=120)
check(run.returncode == 0 and "version 2" in made(), "the picture changed, the made C made again: " + run.stdout[-1500:])
shutil.rmtree(work, ignore_errors=True)
if failures:
    sys.exit(f"test_order.py: {len(failures)} failed")
print("test_order.py: the made C waits for DGX's rule, dgx-ui's file included first; made again with the file")
