#!/usr/bin/env python3
"""
The builder of pages served from a terminal, for a browser that gives a page no folder to open, as the browsers inside
VS Code: the builder's own files, and a project's files read and written through this server.

Usage, in a project's folder:
    python3 managed_components/jef-sure__dgx-ui/tools/dgx_ui_serve.py [--project DIR] [--port N]
(managed_components/dgx-ui/ for dgx-ui taken from git).

It prints the link of the builder, which opens the project at once. The project is the current folder, or --project. A
project with its own copy of dgx-ui, managed_components/jef-sure__dgx-ui/ of the registry or managed_components/dgx-ui/
of git, is served the builder's page of that copy, so the builder is of the version the project builds with; nothing
of the copy is run here (page_copy()). Standard library only, so the Python of ESP-IDF runs it. tools/dgx_ui_mcp.py
--serve is the same server; without --serve, it puts an agent beside the page. Another server of this computer mounts
the builder under a prefix of its own paths by mounted().

Only this computer reaches it: bound to 127.0.0.1, every request of this Host, every page and file under a
Content-Security-Policy that lets a page call nothing but this server. The project's calls carry a token the link gives,
a write comes from the builder's own page, and the paths lie in the project. What is read: the project's CMakeLists and
CMakePresets.json, the files its calls name, the pages and appearances there, the fonts DGX writes, the board's entry,
and of a build's sdkconfig, or of the project's sdkconfig.<BOARD> of its preset, the lines of the panel alone. What is written: the files the project's dgx_ui_tree(),
dgx_ui_appearance() and dgx_ui_names() calls name, read at each write, and pages and appearances already there, of
.json or .txt; never a file or folder of a dot, nor one the build reads by its name.
"""
import argparse
import base64
import hashlib
import http.server
import json
import os
import pathlib
import posixpath
import re
import secrets
import subprocess
import sys
import tempfile
import threading
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent  # the copy of dgx-ui this runs from
ADDRESS = "127.0.0.1"  # this computer only: bound to it, and the Host of every request
SERVED = ROOT / "editor"  # the builder's files: nothing else of the component is served
DEFAULT_PORT = 8741  # fixed, so that the page's origin, and the settings it keeps in the browser, stay from run to run
LONGEST_BODY = 4 * 1024 * 1024
TIMEOUT_S = 20  # a connection that sends nothing for so long is closed: a body cut short ends so too
SERVER_API = 1  # the calls of the server the builder's page makes: a page of another number is not served by this one
# what a page served may do: its own scripts and module, its own address to call, nothing of another host. Vue compiles
# the builder's templates in the browser by Function(), so 'unsafe-eval'; the preview's module needs 'wasm-unsafe-eval'.
# No frame-ancestors: the browser of VS Code shows the page in a frame of its own; a page of another site that frames it
# has not the token of the link, which no address carries to it
CSP = ("default-src 'self'; connect-src 'self'; img-src 'self' data: blob:; style-src 'self' 'unsafe-inline'; font-src 'self' data:; "
       "script-src 'self' 'unsafe-eval' 'wasm-unsafe-eval'; base-uri 'none'; form-action 'self'")

# what the builder writes: of these endings only. What it never writes, whatever a call names, compared in any case of
# letters (a disk of macOS or Windows takes cmakelists.txt for CMakeLists.txt): a file or folder of a dot, a tool's; a
# file the build or CMake reads by its name, sdkconfig* and dependencies.lock* of no such ending; a folder of the build's
WRITTEN_SUFFIXES = (".json", ".txt")
NEVER_WRITTEN = ("cmakelists.txt", "cmakecache.txt", "requirements.txt")
NEVER_WRITTEN_PREFIXES = ("sdkconfig", "dependencies.lock")
NEVER_UNDER = ("build", "managed_components")

# the calls whose files the builder edits, the CMakeLists it reads them in (components/<name>/ too), the words of
# theirs that are no file, and the folders whose pages it does not look for
CALLS = re.compile(r"(?<![A-Za-z0-9_])(dgx_ui_tree|dgx_ui_appearance|dgx_ui_names|dgx_image_needs_file)[ \t]*\(", re.IGNORECASE)
NEEDS_CALL = re.compile(r"(?<![A-Za-z0-9_])dgx_image_needs_file[ \t]*\(", re.IGNORECASE)
CALL_LISTS = ("CMakeLists.txt", "main/CMakeLists.txt")
CALL_KEYWORDS = ("PREFIX", "NAMES", "APPEARANCE", "HEADERS")
NOT_SEARCHED = ("node_modules", "managed_components", "dist", "vendor")

# what the builder reads besides what a call names and the pages and appearances there: the preset, the board's entry
# of jef-sure/boards, the fonts DGX writes; of a build's sdkconfig only the lines of the computer's board's panel
FONTS_INDEX = "dgx_fonts-files.txt"
IMAGES_INDEX = "dgx_images-files.txt"
# a picture: a PNG written beside a file of pictures' needs, and made a file of DGX by the writer of the project's own
# copy of DGX, of the registry: its words, its time, what it may print
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
PICTURE_WORDS = {"bits": (1, 4, 8, 16), "ink": ("dark", "light")}
WRITER_S = 30
WRITER_SAID = 4096
LONGEST_PICTURE = 16 * 1024 * 1024
# the size a picture may have, read of its PNG's head before the writer runs: DGX's writer takes a picture into memory
# whole, 4 bytes a pixel, so a small file of one colour may ask for gigabytes
WIDEST_PICTURE = 4096
MOST_PIXELS = 4 * 1024 * 1024
DGX_COPY = "managed_components/jef-sure__dgx"
BOARD_FILES = (re.compile(r"components/local-boards/boards/[a-z0-9_]+/board\.json"),
               re.compile(r"managed_components/jef-sure__boards/catalog/boards/[a-z0-9_]+/board\.json"))
SDKCONFIG_SHOWN = re.compile(rb"^CONFIG_BOARD_(?:DISPLAY|HOST)_(?:WIDTH|HEIGHT|COLOR_BITS)=[0-9]+[ \t\r]*$", re.M)


def inside(root, path):
    """Whether a real path lies in the real root, or is it."""
    return path == root or path.startswith(root + os.sep)


def without_comments(text):
    """A CMakeLists' text with its comments made spaces, a quoted # kept."""
    out, quoted, k = [], False, 0
    while k < len(text):
        c = text[k]
        if quoted:
            out.append(c)
            if c == "\\" and k + 1 < len(text):
                k += 1
                out.append(text[k])
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
            out.append(c)
        elif c == "#":
            while k < len(text) and text[k] != "\n":
                out.append(" ")
                k += 1
            continue
        else:
            out.append(c)
        k += 1
    return "".join(out)


def call_words(text, start):
    """The words of a call's arguments from just after its "(" to its ")", their quotes taken off."""
    words, word, quoted, depth, k = [], [], False, 1, start
    while k < len(text) and depth:
        c = text[k]
        if quoted:
            if c == "\\" and k + 1 < len(text):
                k += 1
                word.append(text[k])
            elif c == '"':
                quoted = False
            else:
                word.append(c)
        elif c == '"':
            quoted = True
        elif c in "()":
            depth += 1 if c == "(" else -1
        elif c.isspace():
            if word:
                words.append("".join(word))
                word = []
        else:
            word.append(c)
        k += 1
    if word:
        words.append("".join(word))
    return words


def called_files(root):
    """The paths of the project that a call of dgx_ui_tree(), dgx_ui_appearance() or dgx_ui_names() in its CMakeLists
    names: each word of its arguments but the keywords, from the folder of its CMakeLists. Wider than the builder's own
    reading of them (no block, variable or form of argument is told apart): what the builder writes is among them."""
    lists = list(CALL_LISTS)
    try:
        lists += [f"components/{name}/CMakeLists.txt" for name in sorted(os.listdir(os.path.join(root, "components")))]
    except OSError:
        pass
    named = set()
    for rel in lists:
        try:
            with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as source:
                text = without_comments(source.read(LONGEST_BODY))
        except OSError:
            continue
        folder = posixpath.dirname(rel)
        for call in CALLS.finditer(text):
            for word in call_words(text, call.end()):
                if word in CALL_KEYWORDS or word.startswith("/"):
                    continue
                path = posixpath.normpath(posixpath.join(folder, word))
                if path != ".." and not path.startswith("../"):
                    named.add(path)
    return named


def page_there(path):
    """Whether a file is a description as the builder finds one: a .json holding "dgx-ui" and "tree", a page, or "dgx-ui"
    and "colors", an appearance, read as JSON, or by its text when it does not read (one there to be mended)."""
    try:
        with open(path, "rb") as source:
            data = source.read(LONGEST_BODY)
    except OSError:
        return False
    try:
        doc = json.loads(data)
        return isinstance(doc, dict) and "dgx-ui" in doc and ("tree" in doc or "colors" in doc)
    except ValueError:
        return re.search(rb'"dgx-ui"[\s\S]*"(tree|colors)"', data) is not None


def in_build(root, real):
    """Whether a file lies in a folder of a build: one holding CMakeCache.txt, or under it."""
    for above in [os.path.dirname(real), *(str(parent) for parent in pathlib.Path(os.path.dirname(real)).parents)]:
        if not inside(root, above):
            return False
        if os.path.exists(os.path.join(above, "CMakeCache.txt")):
            return True
    return False


def font_listed(root, rel):
    """Whether a path is a file of the fonts DGX writes: build/dgx_fonts/ or build/<board>/dgx_fonts/, its index or a
    file its index names."""
    parts = rel.split("/")
    if not (len(parts) in (3, 4) and parts[0] == "build" and parts[-2] == "dgx_fonts"):
        return False
    if parts[-1] == FONTS_INDEX:
        return True
    try:
        with open(os.path.join(root, *parts[:-1], FONTS_INDEX), encoding="utf-8", errors="replace") as index:
            return any(len(words) >= 2 and words[1] == parts[-1] for words in (line.split() for line in index.read(LONGEST_BODY).splitlines()))
    except OSError:
        return False


def image_listed(root, rel):
    """Whether a path is a file of the pictures DGX makes: build/dgx_images/ or build/<board>/dgx_images/, its index or
    a file its index names."""
    parts = rel.split("/")
    if not (len(parts) in (3, 4) and parts[0] == "build" and parts[-2] == "dgx_images"):
        return False
    if parts[-1] == IMAGES_INDEX:
        return True
    try:
        with open(os.path.join(root, *parts[:-1], IMAGES_INDEX), encoding="utf-8", errors="replace") as index:
            return any(len(words) >= 2 and words[1] == parts[-1] for words in (line.split() for line in index.read(LONGEST_BODY).splitlines()))
    except OSError:
        return False


def needs_files(root):
    """The files of pictures' needs the project's dgx_image_needs_file() calls name, as called_files() finds them."""
    lists = list(CALL_LISTS)
    try:
        lists += [f"components/{name}/CMakeLists.txt" for name in sorted(os.listdir(os.path.join(root, "components")))]
    except OSError:
        pass
    named = set()
    for rel in lists:
        try:
            with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as source:
                text = without_comments(source.read(LONGEST_BODY))
        except OSError:
            continue
        for call in NEEDS_CALL.finditer(text):
            for word in call_words(text, call.end()):
                path = posixpath.normpath(posixpath.join(posixpath.dirname(rel), word))
                if not word.startswith("/") and path != ".." and not path.startswith("../"):
                    named.add(path)
    return named


def needed_pngs(root):
    """The PNG files the lines of the project's files of pictures' needs name: the words after PNG, quoted when they
    hold a space, from the folder of the file, as DGX reads them."""
    keywords = {"BITS", "INDEXED", "PIXELS", "REDUCE", "DITHER", "INK", "ALPHA", "BACKGROUND", "ORIGIN"}
    named = set()
    for rel in needs_files(root):
        try:
            with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as source:
                lines = source.read(LONGEST_BODY).splitlines()
        except OSError:
            continue
        for line in lines:
            words = re.findall(r'"[^"]*"|[^\s"]+', line.split("#", 1)[0] if '"' not in line else line)
            if len(words) < 3 or words[1] != "PNG":
                continue
            for word in words[2:]:
                if word in keywords:
                    break
                path = posixpath.normpath(posixpath.join(posixpath.dirname(rel), word.strip('"')))
                if path != ".." and not path.startswith("../"):
                    named.add(path)
    return named


def png_problem(root, rel, data):
    """What is wrong with a write of a picture, or None: a PNG by its signature and its ending, into the folder of a file
    of pictures' needs or below it, of no dot, out of build/ and managed_components/; a new file, or one a need names. The
    folder images/ beside the file of needs is made when it is not there."""
    if not isinstance(rel, str) or "\\" in rel or "\0" in rel or rel.startswith("/") or any(part in ("", ".", "..") for part in rel.split("/")):
        return "a path within the project, by / and of no empty part, no . and no .."
    if not rel.casefold().endswith(".png") or not data.startswith(PNG_SIGNATURE):
        return "the builder writes a picture as a PNG: a file of .png, its bytes those of a PNG"
    if any(part.startswith(".") for part in rel.split("/")) or rel.split("/")[0].casefold() in NEVER_UNDER:
        return "nothing of a dot, nor under build/ or managed_components/, is written"
    real = os.path.realpath(os.path.join(root, rel))
    if not inside(root, real):
        return "a path that leads out of the project"
    folders = {posixpath.dirname(needs) for needs in needs_files(root)}
    folder = posixpath.dirname(rel)
    if not any(folder == above or folder.startswith(above + "/") if above else True for above in folders):
        return "a picture is written beside the file of pictures' needs a dgx_image_needs_file() call names, or below it"
    if os.path.exists(real) and rel not in needed_pngs(root):
        return f"{rel} is there and no need names it: a picture is written anew, or over one a need names"
    if not os.path.isdir(os.path.dirname(real)) and not (posixpath.basename(folder) == "images" and posixpath.dirname(folder) in folders):
        return "a picture of a folder that is there, or of images/ beside the file of needs"
    return None


def dgx_writer(root):
    """The writer of the files of pictures of the project's own copy of DGX, the registry's in managed_components, held to
    its CHECKSUMS.json; else what to say. Never a path a request gives."""
    copy = os.path.realpath(os.path.join(root, *DGX_COPY.split("/")))
    if not os.path.isdir(copy) or not inside(os.path.realpath(os.path.join(root, "managed_components")), copy):
        return None, f"the project has no copy of DGX of the registry in {DGX_COPY}: the picture is not made; take jef-sure/dgx from the registry"
    tool = os.path.join(copy, "tools", "dgx_images.py")
    if not os.path.isfile(tool):
        return None, "the project's DGX makes no files of pictures: the picture is not made; DGX 0.12.10 or a later one"
    sums = os.path.join(copy, "CHECKSUMS.json")
    if not os.path.isfile(sums):
        return None, f"DGX here is not the registry's copy (no CHECKSUMS.json in {DGX_COPY}): the picture is not made; take jef-sure/dgx from the registry"
    try:
        with open(sums, encoding="utf-8") as listed:
            files = {one["path"]: one["hash"] for one in json.load(listed)["files"]}
        with open(tool, "rb") as data:
            held = files.get("tools/dgx_images.py") == hashlib.sha256(data.read()).hexdigest()
    except (OSError, ValueError, KeyError, TypeError):
        held = False
    if not held:
        return None, f"{DGX_COPY}/tools/dgx_images.py is not as its CHECKSUMS.json says: the picture is not made"
    return tool, None


def png_size(data):
    """The width and the height of a PNG by its head, IHDR, the first part after the signature; None when it is not."""
    if len(data) < 24 or data[8:16] != b"\0\0\0\x0dIHDR":
        return None
    return int.from_bytes(data[16:20], "big"), int.from_bytes(data[20:24], "big")


def make_picture(root, asked):
    """A file of DGX of the PNG files given, by the writer of the project's DGX, run apart: the server's own Python with
    -I, a fresh folder, an environment of PATH alone, a time, what it prints bounded. Its bytes, or the words of what is
    wrong."""
    pngs = asked.get("pngs") if isinstance(asked, dict) else None
    if not isinstance(pngs, list) or not 1 <= len(pngs) <= 64 or not all(isinstance(one, str) for one in pngs):
        return None, "the PNG files of the picture, 1 to 64, each as base64"
    try:
        datas = [base64.b64decode(one, validate=True) for one in pngs]
    except ValueError:
        return None, "the PNG files of the picture, each as base64"
    if not all(data.startswith(PNG_SIGNATURE) for data in datas):
        return None, "a file given is not a PNG"
    sizes = [png_size(data) for data in datas]
    if None in sizes:
        return None, "a file given is not a PNG: its head, IHDR, is not its first part"
    limits = f"the builder takes pictures of up to {WIDEST_PICTURE} pixels a side and 4 Mpx in all"
    for width, height in sizes:
        if width > WIDEST_PICTURE or height > WIDEST_PICTURE:
            return None, f"a picture of {width}x{height}: {limits}"
    if sum(width * height for width, height in sizes) > MOST_PIXELS:
        return None, f"{len(sizes)} pictures of {sum(width * height for width, height in sizes)} pixels in all: {limits}"
    bits, ink, background, mask = asked.get("bits"), asked.get("ink"), asked.get("background"), asked.get("mask") is True
    if bits not in PICTURE_WORDS["bits"] or (ink is not None and ink not in PICTURE_WORDS["ink"]) or \
            (background is not None and not (isinstance(background, str) and re.fullmatch(r"[0-9a-fA-F]{6}", background))):
        return None, "bits 1, 4, 8 or 16; ink dark or light; a background of six hex digits"
    if mask and bits not in (4, 8):
        return None, "a mask is of 4 or 8 bits"
    tool, said = dgx_writer(root)
    if not tool:
        return None, said
    with tempfile.TemporaryDirectory(prefix="dgx-ui-picture-") as folder:
        names = []
        for k, data in enumerate(datas):
            names.append(f"picture{k}.png")
            with open(os.path.join(folder, names[-1]), "wb") as out:
                out.write(data)
        words = ["PNG", *names, "BITS", str(bits)] + (["MASK"] if mask else []) + (["REDUCE"] if asked.get("reduce") is True else []) + \
            (["DITHER"] if asked.get("dither") is True else [])
        words += (["INK", ink.upper()] if ink else []) + (["BACKGROUND", background] if background else [])
        env = {"PATH": os.environ.get("PATH", "")}
        if os.name == "nt" and "SYSTEMROOT" in os.environ:
            env["SYSTEMROOT"] = os.environ["SYSTEMROOT"]  # Python itself does not start on Windows without it
        run = subprocess.Popen([sys.executable, "-I", tool, "make", "made.dgxi", *words], cwd=folder, env=env, stdin=subprocess.DEVNULL,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        kept = bytearray()

        def drain():  # what it prints read as it comes, its first bytes kept, the rest let go: nothing of it on a disk
            while True:
                chunk = run.stdout.read(65536)
                if not chunk:
                    return
                kept.extend(chunk[:max(0, WRITER_SAID - len(kept))])
        reader = threading.Thread(target=drain, daemon=True)
        reader.start()
        try:
            run.wait(timeout=WRITER_S)
        except subprocess.TimeoutExpired:
            run.kill()
            run.wait()
            return None, f"DGX's writer did not finish in {WRITER_S} s: the picture is not made"
        finally:
            reader.join(5)
            run.stdout.close()
        if run.returncode != 0:
            return None, kept.decode("utf-8", errors="replace").strip() or "DGX's writer refused the picture"
        made = os.path.join(folder, "made.dgxi")
        if not os.path.isfile(made) or os.path.getsize(made) > LONGEST_PICTURE:
            return None, f"DGX's writer made no file of at most {LONGEST_PICTURE} bytes"
        with open(made, "rb") as data:
            return data.read(), None


def preset_board(root):
    """The BOARD of the project's "default" configure preset (CMakePresets.json), a word of [a-z0-9_]; None when
    there is none, read now as the person may have changed the board."""
    try:
        with open(os.path.join(root, "CMakePresets.json"), encoding="utf-8") as presets:
            found = json.load(presets).get("configurePresets", [])
    except (OSError, ValueError, AttributeError):
        return None
    for preset in found if isinstance(found, list) else []:
        if isinstance(preset, dict) and preset.get("name") == "default":
            board = (preset.get("cacheVariables") or {}).get("BOARD") if isinstance(preset.get("cacheVariables"), dict) else None
            return board if isinstance(board, str) and re.fullmatch(r"[a-z0-9_]+", board) else None
    return None


def reading(root, rel, real):
    """What is wrong with a read of a file, or None, and whether only its lines of the panel are given: the builder reads
    the project's CMakeLists and CMakePresets.json, the files its calls name, the pages and appearances there, the fonts
    DGX writes and the board's entry; never a file or folder of a dot, nor dependencies.lock, and of an sdkconfig only
    that of a build or the project's own of its preset's board (sdkconfig.<BOARD>, where a preset of boards keeps it),
    and of it only the panel's lines."""
    written = os.path.relpath(real, root).replace(os.sep, "/")
    for path in (rel, written):
        if any(part.startswith(".") for part in path.split("/")):
            return "nothing of a dot is read: a folder or file of a tool's", False
    name = written.split("/")[-1].casefold()
    if name == "sdkconfig" and in_build(root, real):
        return None, True
    board = preset_board(root) if written.startswith("sdkconfig.") else None
    if board and written == f"sdkconfig.{board}":
        return None, True
    if name.startswith(NEVER_WRITTEN_PREFIXES):
        return f"{written.split('/')[-1]} is not read by the builder", False
    # by the file a link leads to, as a write goes: a link of a list's name to another file is not the list
    if written in {*CALL_LISTS, "CMakePresets.json"} or re.fullmatch(r"components/[^/]+/CMakeLists\.txt", written):
        return None, False
    named = called_files(root)
    if written in named and name.endswith(WRITTEN_SUFFIXES):
        return None, False
    if any(pattern.fullmatch(written) for pattern in BOARD_FILES) or font_listed(root, written) or image_listed(root, written):
        return None, False
    if written in needed_pngs(root):
        return None, False
    searched = not any(part.casefold() in NOT_SEARCHED or part.casefold().startswith("build") for part in written.split("/")[:-1])
    if searched and name.endswith(".json") and page_there(real):
        return None, False
    return ("the builder reads the project's CMakeLists, the files its calls name, the pages and appearances there, the "
            "fonts DGX writes and the board of its preset"), False


def problem_of(root, rel, writing=False, listing=False):
    """What is wrong with a path of the project a call names, or None: relative, of no empty part, no "." or "..", no
    backslash and no NUL, leading nowhere out of the project, links followed; a read by the rule of what is read, a
    write by the rule of what is written, into a folder that is there."""
    if not isinstance(rel, str) or "\\" in rel or "\0" in rel:
        return "a path of the project, by / and of no backslash"
    if rel == "" and listing:
        return None
    if rel.startswith("/"):
        return "a path within the project, not from the root of the disk"
    parts = rel.split("/")
    if any(part in ("", ".", "..") for part in parts):
        return "a path of no empty part, no . and no .."
    real = os.path.realpath(os.path.join(root, rel))
    if not inside(root, real):
        return "a path that leads out of the project"
    if listing and any(part.startswith(".") for path in (rel, os.path.relpath(real, root).replace(os.sep, "/")) for part in path.split("/") if part != "."):
        return "nothing of a dot is listed: a folder of a tool's"
    if not writing and not listing and os.path.isfile(real):
        return reading(root, rel, real)[0]
    if writing:
        # what is never written, by the path as it is named and by the file its links lead to
        written = os.path.relpath(real, root).replace(os.sep, "/")
        for path in (rel, written):
            parts = [part.casefold() for part in path.split("/")]
            name = parts[-1]
            if any(part.startswith(".") for part in parts):
                return "nothing of a dot is written: a folder or file of a tool's"
            if not name.endswith(WRITTEN_SUFFIXES):
                return "the builder writes descriptions and appearances of .json and names files of .txt only"
            if name in NEVER_WRITTEN or name.startswith(NEVER_WRITTEN_PREFIXES) or (name.startswith("cmake") and name.endswith(".json")):
                return f"{path.split('/')[-1]} is the build's own: not written by the builder"
            if any(part in NEVER_UNDER for part in parts[:-1]):
                return "nothing is written under build/ or managed_components/"
        folder = os.path.dirname(real)
        if not os.path.isdir(folder):
            return "a file of a folder that is there: no folder is made"
        for above in [folder, *(str(parent) for parent in pathlib.Path(folder).parents)]:
            if not inside(root, above):
                break
            if os.path.exists(os.path.join(above, "CMakeCache.txt")):
                return "nothing is written in a folder of a build, one holding CMakeCache.txt"
        # what is written: a file a call names, read now, as the person may have changed the CMakeLists; or a page there
        named = called_files(root)
        if rel not in named and written not in named:
            searched = not any(part.casefold() in NOT_SEARCHED or part.casefold().startswith("build") for part in written.split("/")[:-1])
            if not (searched and written.endswith(".json") and page_there(real)):
                return "the builder writes the files the project's dgx_ui_tree(), dgx_ui_appearance() and dgx_ui_names() calls name, and pages and appearances already there"
    return None


def write_file(root, rel, data):
    """The bytes into the file the path names, its links followed: a file beside it first, put in its place at once."""
    target = os.path.realpath(os.path.join(root, rel))
    mode = os.stat(target).st_mode & 0o7777 if os.path.isfile(target) else None
    handle, temporary = tempfile.mkstemp(dir=os.path.dirname(target), prefix=".dgx_ui_serve.")
    try:
        with os.fdopen(handle, "wb") as out:
            out.write(data)
        if mode is not None:
            os.chmod(temporary, mode)
        os.replace(temporary, target)
    except BaseException:
        if os.path.exists(temporary):
            os.remove(temporary)
        raise


# the types of what editor/ holds, a fixed table: a guess of the system's (mimetypes reads the registry of Windows and
# /etc/mime.types) that took .js for text/plain would keep the page's scripts from running
TYPES = {".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8",
         ".json": "application/json", ".md": "text/plain; charset=utf-8", ".txt": "text/plain; charset=utf-8"}


class Calls:
    """The builder's files and the project's calls, for a handler of http.server of any server: this one's
    (handler_of()), or another's that mounts the builder under a prefix of its paths (mounted()). handle() answers what
    it owns, prefix + "/editor/..." and prefix + "/project...", whole, and returns True; any other path it leaves,
    False. The page is served from the editor/ of page, a copy of dgx-ui that page_copy() took, else this one's."""

    def __init__(self, project, token, port, page=None, prefix=""):
        self.root, self.served = os.path.realpath(project), os.path.realpath(os.path.join(page or ROOT, "editor"))
        self.token, self.port, self.prefix = token, port, prefix
        self.own_origin = f"http://{ADDRESS}:{port}"

    def answer(self, request, code, value=None, body=None, kind="application/json"):
        body = body if body is not None else json.dumps({} if value is None else value).encode()
        request.send_response(code)
        if code != 204:
            request.send_header("Content-Type", kind)
            request.send_header("Content-Length", str(len(body)))
        request.send_header("Cache-Control", "no-store")
        request.send_header("Content-Security-Policy", CSP)
        request.end_headers()
        request.wfile.write(body)

    def allowed(self, request):
        """Only this address: another site in the browser can reach 127.0.0.1, by a name of its own that leads here,
        but not with this Host."""
        return request.headers.get("Host") == f"{ADDRESS}:{self.port}"

    def by_token(self, request):
        return secrets.compare_digest(request.headers.get("Authorization", ""), f"Bearer {self.token}")

    def from_own_page(self, request):
        """A page of another site can send to 127.0.0.1 without reading the answer: a write comes from this one. In the
        frame of VS Code's browser the page keeps this origin."""
        return request.headers.get("Origin") == self.own_origin

    def path_asked(self, address, **rule):
        rel = urllib.parse.parse_qs(address.query).get("path", [""])[0]
        return rel, problem_of(self.root, rel, **rule)

    def handle(self, request):
        """The request answered when its path is the builder's, True; else False, nothing sent. The prefix is matched
        at a boundary of the path's parts: "/xy" is not of "/x"."""
        address = urllib.parse.urlsplit(request.path)
        path = address.path
        if self.prefix:
            if not (path == self.prefix or path.startswith(self.prefix + "/")):
                return False
            path = path[len(self.prefix):]
        if not (path in ("/editor", "/project") or path.startswith(("/editor/", "/project/"))):
            return False
        if request.command == "GET":
            self.get(request, address, path)
        elif request.command == "POST":
            self.post(request, address, path)
        else:
            self.answer(request, 405)
        return True

    def get(self, request, address, path):
        if not self.allowed(request):
            return self.answer(request, 403, {"error": "another Host"})
        if path == "/project" or path.startswith("/project/"):
            return self.project(request, address, path)
        if path == "/editor":
            # the folder asked without its slash: to the folder, as a browser's relative paths of its page need
            request.send_response(301)
            request.send_header("Location", f"{self.prefix}/editor/")
            request.send_header("Content-Length", "0")
            request.end_headers()
            return None
        return self.file(request, urllib.parse.unquote(path[len("/editor/"):]))

    def file(self, request, rel):
        """A file of the builder's folder, the path unquoted once, its links followed and still inside it; the folder
        itself its index.html, any other folder none. No listing; the query is not read."""
        if "\0" in rel or rel.endswith("/"):
            return self.answer(request, 404)
        real = os.path.realpath(os.path.join(self.served, rel or "index.html"))
        if not inside(self.served, real) or not os.path.isfile(real):
            return self.answer(request, 404)
        with open(real, "rb") as data:
            body = data.read()
        request.send_response(200)
        request.send_header("Content-Type", TYPES.get(os.path.splitext(real)[1].lower(), "application/octet-stream"))
        request.send_header("Content-Length", str(len(body)))
        # every file asked again: a page cached from another copy or version would run one script of each. No
        # X-Frame-Options and no frame-ancestors: the browser of VS Code shows the page in a frame of its own
        request.send_header("Cache-Control", "no-cache")
        request.send_header("Content-Security-Policy", CSP)
        request.send_header("X-Content-Type-Options", "nosniff")
        request.end_headers()
        request.wfile.write(body)
        return None

    def project(self, request, address, path):
        if not self.by_token(request):
            return self.answer(request, 401, {"error": "the token of the link"})
        root = self.root
        if path == "/project":
            return self.answer(request, 200, {"name": os.path.basename(root)})
        if path == "/project/list":
            rel, problem = self.path_asked(address, listing=True)
            if problem:
                return self.answer(request, 400, {"error": problem})
            folder = os.path.join(root, rel)
            if not os.path.isdir(folder):
                return self.answer(request, 204, body=b"")  # none: not an error, which a browser would show in red
            entries = []
            with os.scandir(folder) as found:
                for entry in found:
                    if entry.is_symlink() and not inside(root, os.path.realpath(entry.path)):
                        continue  # a link out of the project is not shown
                    if entry.name.startswith("."):
                        continue  # nor a file or folder of a dot, a tool's, which the builder never reads
                    entries.append({"name": entry.name, "directory": entry.is_dir()})
            return self.answer(request, 200, sorted(entries, key=lambda entry: entry["name"]))
        if path == "/project/file":
            rel, problem = self.path_asked(address)
            if problem:
                return self.answer(request, 400, {"error": problem})
            full = os.path.join(root, rel)
            if not os.path.isfile(full):
                return self.answer(request, 204, body=b"")
            with open(full, "rb") as data:
                body = data.read()
            if reading(root, rel, os.path.realpath(full))[1]:
                body = b"".join(line.group(0).rstrip() + b"\n" for line in SDKCONFIG_SHOWN.finditer(body))
            return self.answer(request, 200, body=body, kind="application/octet-stream")
        return self.answer(request, 404)

    def post(self, request, address, path):
        root = self.root
        if not self.allowed(request):
            return self.answer(request, 403, {"error": "another Host"})
        if not self.by_token(request):
            return self.answer(request, 401, {"error": "the token of the link"})
        if not self.from_own_page(request):
            return self.answer(request, 403, {"error": "a write from another page"})
        if path not in ("/project/file", "/project/binary", "/project/image"):
            return self.answer(request, 404)
        length = request.headers.get("Content-Length", "")
        if not length.isdigit():
            return self.answer(request, 411)
        if int(length) > LONGEST_BODY:
            return self.answer(request, 413, {"error": f"a file of at most {LONGEST_BODY} bytes"})
        try:
            data = request.rfile.read(int(length))
        except OSError:  # nothing more came for TIMEOUT_S
            data = b""
        if len(data) < int(length):
            return self.answer(request, 400, {"error": "the body ended before its length: nothing written"})
        if path == "/project/image":
            try:
                asked = json.loads(data)
            except ValueError:
                return self.answer(request, 400, {"error": "not JSON"})
            made, problem = make_picture(root, asked)
            if problem:
                return self.answer(request, 400, {"error": problem})
            return self.answer(request, 200, {"dgxi": base64.b64encode(made).decode(), "length": len(made)})
        if path == "/project/binary":
            rel = urllib.parse.parse_qs(address.query).get("path", [""])[0]
            problem = png_problem(root, rel, data)
            if problem:
                return self.answer(request, 400, {"error": problem})
            folder = os.path.dirname(os.path.join(root, rel))
            try:
                os.makedirs(folder, exist_ok=True)  # images/ beside the file of needs, png_problem() allowed it
                write_file(root, rel, data)
            except OSError as error:
                return self.answer(request, 500, {"error": f"{rel}: {error.strerror or error}"})
            return self.answer(request, 200)
        rel, problem = self.path_asked(address, writing=True)
        if problem:
            return self.answer(request, 400, {"error": problem})
        try:
            write_file(root, rel, data)
        except OSError as error:
            # a folder that may not be written, a full disk: the page is told, and the server goes on
            return self.answer(request, 500, {"error": f"{rel}: {error.strerror or error}"})
        return self.answer(request, 200)


def mounted(project, token, port, prefix):
    """The builder for another server on this computer, its page mounting the builder's (boards' tools/boards_mcp.py
    --serve): handle(request) of its http.server handler answers prefix + "/editor/..." and prefix + "/project...", by
    the rules of this server, with its token and port, and returns True; for any other path it returns False and sends
    nothing. The page is the project's copy's, as page_copy() takes it. The agent's /agent/* are not served."""
    return Calls(project, token, port, page_copy(project)[0], prefix).handle


def handler_of(project, token, port, page=None):
    """The handler of this server: the builder's files and calls, by Calls with no prefix; allowed(), by_token(),
    from_own_page() and answer() for a handler that extends it (tools/dgx_ui_mcp.py)."""
    calls = Calls(project, token, port, page)

    class Page(http.server.BaseHTTPRequestHandler):
        timeout = TIMEOUT_S

        def log_message(self, *_):
            pass

        def answer(self, code, value=None, body=None, kind="application/json"):
            calls.answer(self, code, value, body, kind)

        def allowed(self):
            return calls.allowed(self)

        def by_token(self):
            return calls.by_token(self)

        def from_own_page(self):
            return calls.from_own_page(self)

        def do_GET(self):
            if not calls.handle(self):
                self.answer(404)

        def do_POST(self):
            if not calls.handle(self):
                self.answer(404)

        def do_HEAD(self):
            return self.answer(405)

    return Page


def page_copy(project):
    """The copy of dgx-ui whose builder's page the project is served with, and what to say of it: the project's own, of
    the registry (managed_components/jef-sure__dgx-ui) or of git (managed_components/dgx-ui), so the page and its preview
    are of the version the project builds with; else this one's. Nothing of a copy is run here: its page runs in the
    browser, under CSP, and calls this server, whose rules of reading and writing are its own. The copy is taken when it
    lies in the project's managed_components/, its page speaks SERVER_API, read as text, and, where the registry's
    CHECKSUMS.json is there, every file of its page is as that file says. The checksums are of integrity, a copy spoiled
    or cut, not a boundary: a project can write them as it writes the copy; the boundary is the browser's sandbox, CSP
    and this server's rules."""
    managed = os.path.realpath(os.path.join(project, "managed_components"))
    for name in ("jef-sure__dgx-ui", "dgx-ui"):
        copy = os.path.join(project, "managed_components", name)
        real = os.path.realpath(copy)
        if not os.path.isdir(copy):
            continue
        if real == os.path.realpath(ROOT):
            return None, None
        if not inside(managed, real) or real == managed:
            return None, f"managed_components/{name} leads out of the project's managed_components: this server's own page"
        version = None
        try:
            with open(os.path.join(real, "idf_component.yml"), encoding="utf-8", errors="replace") as manifest:
                found = re.search(r'^version:\s*"?([^"\s]+)"?\s*$', manifest.read(LONGEST_BODY), re.M)
                version = found.group(1) if found else None
            with open(os.path.join(real, "editor", "host_server.js"), encoding="utf-8", errors="replace") as host:
                api = re.search(r"\bDGX_UI_SERVER_API\s*=\s*(\d+)", host.read(LONGEST_BODY))
        except OSError:
            return None, f"managed_components/{name} has no builder's page: this server's own page"
        if not api or int(api.group(1)) != SERVER_API:
            return None, (f"the project builds with dgx-ui {version or '?'}, whose builder this server does not speak to: its own, "
                          f"python3 managed_components/{name}/tools/dgx_ui_mcp.py --serve")
        editor = os.path.join(real, "editor")
        for folder, _, names in os.walk(editor, followlinks=False):
            for each in names:
                if not inside(editor, os.path.realpath(os.path.join(folder, each))):
                    return None, f"managed_components/{name}/editor has a link out of it: this server's own page"
        sums = os.path.join(real, "CHECKSUMS.json")
        if os.path.isfile(sums):
            try:
                with open(sums, encoding="utf-8") as listed:
                    files = {one["path"]: one["hash"] for one in json.load(listed)["files"]}
            except (OSError, ValueError, KeyError, TypeError):
                return None, f"managed_components/{name}/CHECKSUMS.json does not read: this server's own page"
            for folder, _, names in os.walk(editor):
                for each in names:
                    rel = os.path.relpath(os.path.join(folder, each), real).replace(os.sep, "/")
                    with open(os.path.join(folder, each), "rb") as data:
                        if files.get(rel) != hashlib.sha256(data.read()).hexdigest():
                            return None, f"managed_components/{name}/{rel} is not as its CHECKSUMS.json says: this server's own page"
        return real, f"the builder's page of the project's dgx-ui {version or '?'}: managed_components/{name}"
    return None, None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--project", type=pathlib.Path, default=pathlib.Path.cwd(), help="the project's folder; the current one by default")
    parser.add_argument("--port", type=int, default=None, help=f"the port; {DEFAULT_PORT} by default, any free one when it is taken")
    args = parser.parse_args()
    if not args.project.is_dir():
        parser.error(f"--project {args.project}: no such folder")
    try:
        server = http.server.ThreadingHTTPServer((ADDRESS, DEFAULT_PORT if args.port is None else args.port), None)
    except OSError as error:
        if args.port is not None:
            parser.error(f"--port {args.port}: {error.strerror or error}")
        server = http.server.ThreadingHTTPServer((ADDRESS, 0), None)
        print(f"Port {DEFAULT_PORT} is taken: served on another, where the builder's settings in the browser start anew.", flush=True)
    port = server.server_address[1]
    token = secrets.token_urlsafe(24)
    page, said = page_copy(os.path.realpath(args.project))
    if said:
        print(said, flush=True)
    server.RequestHandlerClass = handler_of(args.project, token, port, page)
    server.daemon_threads = True
    print(f"http://{ADDRESS}:{port}/editor/#project={token}", flush=True)
    print(f"The builder reads and writes {args.project.resolve()}", flush=True)
    print("Ctrl-C stops the server.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
