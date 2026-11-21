#!/usr/bin/env python3
"""
make_scenes.py <scene> <dgx_ui_preview.js> <DGX> <work directory> <generate directory> <generate_appearance directory>:
the module built to WebAssembly held to the same preview built for the computer, scene by scene, byte for byte. <scene>
is scene.c built here, on the DGX of this run, and the module must be of that DGX's version: of another, the run fails,
unless PREVIEW_SCENES_OTHER_DGX=skip says to pass by, for a run on a DGX the module is not built of (a checkout).

The scenes: the pages of test_generate (its directory: descriptions and names files), each by the rule and on a names
file, drawn at 16, 18 and 24 bits and at a point of 1 and 2 pixels, clicked, keyed and given values; the appearances of
test_generate_appearance under a page that names their looks; a page that opens another by the action page and goes
back; and each of these texts spoiled at a byte. Every scene ends in a dump of the widgets, the events, the notes and
the frame (scene.c).
"""
import glob
import json
import os
import random
import re
import shutil
import subprocess
import sys

scene_binary, module, dgx, work, generate, generate_appearance = sys.argv[1:7]
rand = random.Random(4747)

dgx_version = re.search(r'^version:\s*"?([^"\s]+)', open(os.path.join(dgx, "idf_component.yml")).read(), re.M).group(1)
said = subprocess.run(["node", "-e", f"require({json.dumps(os.path.abspath(module))}).create({{log: () => {{}}}})"
                       ".then(p => console.log(p.version().dgx))"], capture_output=True, text=True, timeout=120)
if said.returncode:
    sys.exit(f"make_scenes.py: the module does not load in Node: {said.stderr.strip()}")
module_dgx = said.stdout.strip().split(" ")[0]
if module_dgx != dgx_version:
    skip = os.environ.get("PREVIEW_SCENES_OTHER_DGX") == "skip"
    print(f"make_scenes.py: the module is of DGX {module_dgx}, this run's DGX {dgx_version}: not compared"
          + ("" if skip else "; PREVIEW_SCENES_OTHER_DGX=skip passes by"))
    sys.exit(0 if skip else 1)

shutil.rmtree(work, ignore_errors=True)
os.makedirs(work)
texts = {}


def text_file(data):
    """a text in the work directory, by its bytes"""
    if isinstance(data, str):
        data = data.encode("utf-8")
    name = texts.get(data)
    if not name:
        name = texts[data] = f"t{len(texts)}.txt"
        with open(os.path.join(work, name), "wb") as out:
            out.write(data)
    return name


def spoiled(data):
    data = bytearray(data.encode("utf-8") if isinstance(data, str) else data)
    if data:
        data[rand.randrange(len(data))] = ord(rand.choice('"{}[],:#0a1 x.-\n&')) if rand.random() < 0.9 else 0xff
    return bytes(data)


scenes = []


def scene(lines):
    scenes.append("\n".join(lines) + "\n")


def play(width, height, depth, unit, names, appearance, page, extra=()):
    lines = [f"screen {width} {height} {depth} {unit}", f"names {text_file(names) if names is not None else '-'}",
             f"appearance {text_file(appearance) if appearance is not None else '-'}", f"page {text_file(page)}", "paint 1000", "dump"]
    lines += [f"pointer {width // 2} {height // 2} 1 1100", "paint 1100", f"pointer {width // 2} {height // 2} 0 1150", "paint 1150",
              "key 0", "key 2", "paint 1200", "value v 50", "value volume 70", "value level 3", "paint 1300", "dump"]
    lines += list(extra)
    scene(lines)


pages = sorted(glob.glob(os.path.join(generate, "*.json")))
names_files = sorted(glob.glob(os.path.join(generate, "*.txt")))
appearances = sorted(glob.glob(os.path.join(generate_appearance, "*.json")))
appearance_names = sorted(glob.glob(os.path.join(generate_appearance, "*.txt")))
if not pages or not appearances:
    sys.exit("make_scenes.py: no pages or no appearances: run test_generate and test_generate_appearance first")

shapes = [(96, 64, 16, 1), (96, 64, 18, 1), (96, 64, 24, 1), (160, 128, 16, 2)]
for k, page in enumerate(pages):
    text = open(page, "rb").read()
    width, height, depth, unit = shapes[k % len(shapes)]
    play(width, height, depth, unit, None, None, text)
    if names_files:
        play(width, height, depth, unit, open(names_files[k % len(names_files)], "rb").read(), None, text)
    play(width, height, depth, unit, None, None, spoiled(text))

themed = ('{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "pad": 2, "gap": 2, "children": ['
          '{"kind": "label", "text": "Day"}, {"kind": "slider", "value": "volume", "min": 0, "max": 100, "want": [0, 10]},'
          '{"kind": "button", "text": "Go", "on": "go", "want": [0, 16]}]}}')
for k, appearance in enumerate(appearances):
    text = open(appearance, "rb").read()
    width, height, depth, unit = shapes[k % len(shapes)]
    play(width, height, depth, unit, None, text, themed)
    if appearance_names:
        play(width, height, depth, unit, open(appearance_names[0], "rb").read(), text, themed)
    play(width, height, depth, unit, None, spoiled(text), themed)

home = ('{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": ['
        '{"kind": "button", "text": "More", "on": "page", "ctx": "settings", "want": [0, 16]},'
        '{"kind": "button", "text": "Lost", "on": "page", "ctx": "nowhere", "want": [0, 16]}]}}')
settings = ('{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": ['
            '{"kind": "label", "text": "Settings"}, {"kind": "button", "text": "Back", "on": "back", "want": [0, 16]}]}}')
for depth in (16, 18, 24):
    scene([f"pagefile settings {text_file(settings)}", f"screen 96 64 {depth} 1", "names -", "appearance -", f"page {text_file(home)}",
           "paint 1000", "pointer 48 8 1 1100", "paint 1100", "pointer 48 8 0 1150", "paint 1150", "dump", "key 0", "key 2", "paint 1300", "dump",
           "pointer 48 24 1 1400", "paint 1400", "pointer 48 24 0 1450", "paint 1450", "dump"])
    scene([f"pagefile settings {text_file(spoiled(settings))}", f"screen 96 64 {depth} 1", "names -", "appearance -", f"page {text_file(home)}",
           "paint 1000", "pointer 48 8 1 1100", "paint 1100", "pointer 48 8 0 1150", "paint 1150", "dump"])

apart = 0
for k, text in enumerate(scenes):
    scene_file = os.path.join(work, f"s{k}.scene")
    with open(scene_file, "w") as out:
        out.write(text)
    native, wasm = os.path.join(work, f"s{k}.native"), os.path.join(work, f"s{k}.wasm")
    one = subprocess.run([scene_binary, scene_file, native], capture_output=True, text=True, timeout=120)
    two = subprocess.run(["node", os.path.join(os.path.dirname(os.path.abspath(__file__)), "scene.js"), module, scene_file, wasm],
                         capture_output=True, text=True, timeout=120)
    if one.returncode or two.returncode:
        apart += 1
        print(f"  s{k}: the computer {one.returncode} {one.stderr.strip()[:200]}, the module {two.returncode} {two.stderr.strip()[:200]}")
        continue
    if open(native, "rb").read() != open(wasm, "rb").read():
        apart += 1
        if apart <= 5:
            print(f"  s{k}: apart: {native} {wasm}")
        continue
    os.remove(native), os.remove(wasm)
print(f"make_scenes.py: {len(scenes) - apart} of {len(scenes)} scenes the same byte for byte on the module and on the computer, DGX {dgx_version}"
      if not apart else f"make_scenes.py: {apart} of {len(scenes)} scenes apart")
sys.exit(1 if apart else 0)
