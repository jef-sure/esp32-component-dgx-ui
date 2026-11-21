#!/usr/bin/env python3
"""
dgx_ui_c.py <DGX directory> <directory>: dgx_ui_c built for the computer into <directory>, the one way it is built.

The tool is tools/dgx_ui_c.c and six sources of dgx-ui's src/ (the tree, its text boxes, rectangles and messages, the
loader and its reader of JSON), compiled against dgx-ui's include/ and src/ and DGX's include/ and host/stubs/, which
DGX's package carries for its build on a computer: no source of DGX, no library but C's. The words of the library's
looks, which a description may name with no names file, are read from include/dgx_ui_paint.h into library_looks.h.

The compiler is the computer's, as DGX's font2c is built: $CC unless it is a chip's, else cc (gcc on Windows), with a
PATH that does not lead to the chips' tools first. Run by the rule project_include.cmake makes once for a project, and
by tools/Makefile. Its refusals begin with "dgx_ui_c.py:", the tool's with "dgx_ui_c:".
"""
import os
import re
import subprocess
import sys

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
SOURCES = [os.path.join(TOOLS, "dgx_ui_c.c")] + [os.path.join(ROOT, "src", name) for name in
                                                 ("dgx_ui.c", "dgx_ui_text.c", "dgx_ui_rect.c", "dgx_ui_msg.c", "dgx_ui_load.c", "dgx_ui_json.c")]


def fail(text):
    sys.exit("dgx_ui_c.py: " + text)


def library_looks():
    """LOOK(word) for each look of the library: the names of the declarations `extern const dgx_ui_look_t ...;`"""
    with open(os.path.join(ROOT, "include", "dgx_ui_paint.h"), encoding="utf-8") as header:
        text = header.read()
    words = set()
    for declaration in re.findall(r"^extern const dgx_ui_look_t [^;]*;", text, re.M):
        words.update(word for word in re.findall(r"dgx_ui_look_([a-z0-9_]+)", declaration) if word != "t")
    if not words:
        fail("no look of the library's found in include/dgx_ui_paint.h")
    return "".join(f"LOOK({word})\n" for word in sorted(words))


def write_if_other(path, text):
    old = None
    if os.path.exists(path):
        with open(path, encoding="utf-8") as made:
            old = made.read()
    if old != text:
        with open(path, "w", encoding="utf-8") as made:
            made.write(text)


def main():
    if len(sys.argv) != 3:
        fail("dgx_ui_c.py <DGX directory> <directory>")
    dgx, out = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    for needed in ("include", os.path.join("host", "stubs")):
        if not os.path.isdir(os.path.join(dgx, needed)):
            fail(f"no {needed} in {dgx}: not a DGX component")
    os.makedirs(out, exist_ok=True)
    write_if_other(os.path.join(out, "library_looks.h"), library_looks())
    # the computer's compiler with the computer's tools: inside an ESP-IDF build the PATH leads to the chips' tools first
    env = dict(os.environ, PATH=os.pathsep.join(p for p in os.environ.get("PATH", "").split(os.pathsep) if ".espressif" not in p))
    compiler = os.environ.get("CC", "")
    if not compiler or ".espressif" in compiler or "-esp" in os.path.basename(compiler):  # a CC of a chip is no use here
        compiler = "gcc" if os.name == "nt" else "cc"
    binary = os.path.join(out, "dgx_ui_c.exe" if os.name == "nt" else "dgx_ui_c")
    stubs = os.path.join(dgx, "host", "stubs")
    command = [compiler, "-O2", "-std=gnu11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Wno-sign-compare",
               "-I" + os.path.join(ROOT, "include"), "-I" + os.path.join(ROOT, "src"), "-I" + os.path.join(dgx, "include"), "-I" + stubs,
               "-include", os.path.join(stubs, "esp_log.h"), "-I" + out, "-o", binary + ".new"] + SOURCES
    try:
        run = subprocess.run(command, capture_output=True, text=True, env=env)
    except OSError:
        fail(f"no C compiler of the computer ({compiler}; set CC to another) to build dgx_ui_c with")
    if run.returncode:
        fail("dgx_ui_c failed to build:\n" + run.stderr.strip())
    if run.stderr.strip():
        print(run.stderr.strip(), file=sys.stderr)
    os.replace(binary + ".new", binary)


if __name__ == "__main__":
    main()
