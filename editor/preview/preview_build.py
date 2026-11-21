#!/usr/bin/env python3
"""
preview_build.py: the steps of the preview's build (Makefile) that are not the compiler's.

    preview_build.py headers <directory>
        library_looks.h and library_draws.h, the library's looks and draws by their words, as dgx_ui_c.py makes them
    preview_build.py version <DGX> <preview_version.h> <dgx-ui directory> <source>...
        what the module is made of: dgx-ui's version, DGX's with the hash the registry gave its copy, the toolchain's,
        and a hash of every source and header given; refused, a DGX that is not the registry's copy at the version
        dgx-ui asks for at least: one of another version, or a checkout
    preview_build.py js <module.wasm> <preview_class.js> <preview_version.h> <dgx_ui_preview.js>
        the module's bytes in base64 and the class that calls them, one file a page loads with no server
    preview_build.py taken <dgx_ui_preview.map> <taken.txt>
        what the module took of the C library and of the compiler's builtins, by the link map, for licenses/

Every output is a function of its inputs alone: no time, no path of the machine.
"""
import base64
import hashlib
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "tools"))


def fail(text):
    sys.exit("preview_build.py: " + text)


def write_if_other(path, text):
    old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
    if old != text:
        with open(path, "w", encoding="utf-8", newline="\n") as out:
            out.write(text)


def headers(directory):
    import dgx_ui_c
    os.makedirs(directory, exist_ok=True)
    write_if_other(os.path.join(directory, "library_looks.h"), dgx_ui_c.library_looks())
    write_if_other(os.path.join(directory, "library_draws.h"), dgx_ui_c.library_draws())


def top_version(manifest):
    found = re.search(r'^version:\s*"?([^"\s]+)"?\s*$', open(manifest, encoding="utf-8").read(), re.M)
    if not found:
        fail(f"{manifest}: no version")
    return found.group(1)


def floor_of_dgx(manifest):
    """the ^x.y.z dgx-ui asks of DGX: quoted as in the tree, bare as the component manager writes the packed manifest"""
    text = open(manifest, encoding="utf-8").read()
    found = re.search(r'^\s+jef-sure/dgx:\s*\n\s+version:\s*["\']?\^([0-9.]+)["\']?', text, re.M)
    if not found:
        fail(f"{manifest}: no version of jef-sure/dgx in the form ^x.y.z")
    return found.group(1)


def package_version(name):
    said = subprocess.run(["dpkg-query", "-W", "-f=${Version}", name], capture_output=True, text=True)
    if said.returncode or not said.stdout:
        fail(f"the package {name} is not installed: apt install lld-21 wasi-libc libclang-rt-21-dev-wasm32")
    return said.stdout


def label_of(path, dgx_ui, dgx):
    path = os.path.abspath(path)
    for root, label in ((dgx_ui, "dgx-ui"), (dgx, "dgx")):
        if path.startswith(root + os.sep):
            return label + "/" + os.path.relpath(path, root).replace(os.sep, "/")
    fail(f"{path}: a source of neither dgx-ui nor DGX")


def version(dgx, out, dgx_ui, sources, clang):
    dgx, dgx_ui = os.path.abspath(dgx), os.path.abspath(dgx_ui)
    if os.path.exists(os.path.join(dgx, ".git")):
        fail(f"{dgx} is a checkout: the module is built from the registry's copy of DGX, a project's managed_components/jef-sure__dgx")
    hash_file = os.path.join(dgx, ".component_hash")
    if not os.path.exists(hash_file):
        fail(f"{dgx} has no .component_hash: the module is built from the registry's copy of DGX, a project's managed_components/jef-sure__dgx")
    dgx_version, floor = top_version(os.path.join(dgx, "idf_component.yml")), floor_of_dgx(os.path.join(dgx_ui, "idf_component.yml"))
    if dgx_version != floor:
        fail(f"DGX {dgx_version}: the module is built on the version dgx-ui asks for at least, {floor}")
    said = subprocess.run([clang, "--version"], capture_output=True, text=True)
    found = re.search(r"clang version (\S+)", said.stdout)
    if said.returncode or not found:
        fail(f"{clang}: no version said")
    toolchain = (f"clang {found.group(1)}, wasi-libc {package_version('wasi-libc')}, "
                 f"libclang-rt-21-dev-wasm32 {package_version('libclang-rt-21-dev-wasm32')}, lld-21 {package_version('lld-21')}")
    digest = hashlib.sha256()
    for label, path in sorted((label_of(path, dgx_ui, dgx), path) for path in set(sources)):
        data = open(path, "rb").read()
        digest.update(label.encode() + b"\0" + str(len(data)).encode() + b"\0" + data)
    component_hash = open(hash_file, encoding="utf-8").read().strip()
    text = ("/* what the preview's module is made of: written by preview_build.py, not to be edited */\n"
            f'#define PREVIEW_DGX_UI_VERSION "{top_version(os.path.join(dgx_ui, "idf_component.yml"))}"\n'
            f'#define PREVIEW_DGX_VERSION "{dgx_version} of the registry, {component_hash[:16]}"\n'
            f'#define PREVIEW_TOOLCHAIN "{toolchain}"\n'
            f'#define PREVIEW_SOURCES "sha256 {digest.hexdigest()}"\n')
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    write_if_other(out, text)


def js(module, class_file, version_header, out):
    data = open(module, "rb").read()
    encoded = base64.b64encode(data).decode("ascii")
    pieces = [encoded[k:k + 100] for k in range(0, len(encoded), 100)]
    said = dict(re.findall(r'#define (PREVIEW_\w+) "([^"]*)"', open(version_header, encoding="utf-8").read()))
    head = ("/*\n * dgx_ui_preview.js: the preview of a description, dgx-ui built to WebAssembly, in one file a page loads with no\n"
            " * server (editor/preview/dgx_ui_preview.h, preview_class.js). Made by editor/preview/Makefile, not to be edited:\n"
            f" * dgx-ui {said['PREVIEW_DGX_UI_VERSION']}, DGX {said['PREVIEW_DGX_VERSION']},\n"
            f" * {said['PREVIEW_TOOLCHAIN']},\n * sources {said['PREVIEW_SOURCES']}.\n"
            f" * The module, {len(data)} bytes, is MIT as dgx-ui and DGX, with code of wasi-libc and compiler-rt and five fonts of\n"
            " * DGX under their own licences: licenses/ beside this file.\n */\n")
    body = "const DGX_UI_PREVIEW_MODULE = [\n" + ",\n".join(f"    '{piece}'" for piece in pieces) + "\n].join('');\n"
    text = head + "(function (root) {\n'use strict';\n" + body + open(class_file, encoding="utf-8").read() + "})(this);\n"
    with open(out, "w", encoding="utf-8", newline="\n") as written:
        written.write(text)


def taken(map_file, out):
    members = {}
    for path, member in re.findall(r"(/usr/lib/[^\s:()]+\.(?:a|o))(?:\(([^)]+)\))?", open(map_file, encoding="utf-8").read()):
        members.setdefault(path, set())
        if member:
            members[path].add(member)
    packages = (("wasi-libc", "/usr/lib/wasm32-wasi/"), ("libclang-rt-21-dev-wasm32", "/usr/lib/llvm-21/lib/clang/21/lib/wasi/"))
    lines = ["What the module took of the C library and of the compiler's builtins, by its link map (preview_build.py taken).", ""]
    for package, lead in packages:
        lines.append(f"{package} {package_version(package)}:")
        for path in sorted(p for p in members if p.startswith(lead)):
            name = path[len(lead):]
            lines.append(f"  {name}: {' '.join(sorted(members[path]))}" if members[path] else f"  {name}")
    others = sorted(p for p in members if not any(p.startswith(lead) for _, lead in packages))
    if others:
        fail(f"{map_file}: taken from no package named here: {' '.join(others)}")
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as written:
        written.write("\n".join(lines) + "\n")


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == "headers":
        headers(sys.argv[2])
    elif len(sys.argv) >= 6 and sys.argv[1] == "version":
        version(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5:], os.environ.get("CLANG", "clang-21"))
    elif len(sys.argv) == 6 and sys.argv[1] == "js":
        js(sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
    elif len(sys.argv) == 4 and sys.argv[1] == "taken":
        taken(sys.argv[2], sys.argv[3])
    else:
        fail("headers <directory> | version <DGX> <preview_version.h> <dgx-ui directory> <source>... | "
             "js <module.wasm> <preview_class.js> <preview_version.h> <dgx_ui_preview.js> | taken <dgx_ui_preview.map> <taken.txt>")


if __name__ == "__main__":
    main()
