#!/usr/bin/env python3
"""
check_build_fonts.py <work directory>: the module given the fonts of a project as DGX itself writes them. A copy of
examples/thin_paint with dgx_fonts_files() in its main/CMakeLists.txt is built for the computer by ESP-IDF, on the DGX
the component manager gives for dgx-ui's manifest; then its build's dgx_fonts/dgx_fonts-files.txt and files go to the
module's fonts() in Node: every verdict 0, and a page in styles of the example's two needs, "large" and "small" by the
rule, drawn with no stand-in, its soft letters in more than two levels. Run in ESP-IDF's environment, a release step as
check_packed.py is.
"""
import json
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
work = os.path.abspath(sys.argv[1])
shutil.rmtree(work, ignore_errors=True)
example = os.path.join(work, "thin_paint")
shutil.copytree(os.path.join(ROOT, "examples", "thin_paint"), example,
                ignore=shutil.ignore_patterns("build*", "managed_components", "dependencies.lock*", "sdkconfig", "sdkconfig.old"))
lists = os.path.join(example, "CMakeLists.txt")
text = open(lists).read()
if "set(EXTRA_COMPONENT_DIRS ../..)" not in text:
    sys.exit("check_build_fonts.py: examples/thin_paint/CMakeLists.txt no longer takes dgx-ui by ../..")
open(lists, "w").write(text.replace("set(EXTRA_COMPONENT_DIRS ../..)", f"set(EXTRA_COMPONENT_DIRS {ROOT})")
                       .replace("get_filename_component(dgx_ui_name ../.. ABSOLUTE)", f"get_filename_component(dgx_ui_name {ROOT} ABSOLUTE)"))
main = os.path.join(example, "main", "CMakeLists.txt")
text = open(main).read()
if not re.search(r"^dgx_fonts_rules\(", text, re.M):
    sys.exit("check_build_fonts.py: examples/thin_paint/main/CMakeLists.txt has no dgx_fonts_rules() to put dgx_fonts_files() after")
open(main, "w").write(re.sub(r"^(dgx_fonts_rules\(.*\))$", r"\1\ndgx_fonts_files()", text, count=1, flags=re.M))
build = os.path.join(work, "build")
IDF_PY = [os.environ.get("IDF_PYTHON", "python"), os.path.join(os.environ["IDF_PATH"], "tools", "idf.py")]
said = subprocess.run(IDF_PY + ["-B", build, "-DIDF_TARGET=linux", f"-DSDKCONFIG={os.path.join(work, 'sdkconfig')}", "build"], cwd=example,
                      capture_output=True, text=True, timeout=2400)
if said.returncode:
    sys.exit("check_build_fonts.py: the example did not build:\n" + (said.stdout + said.stderr)[-3000:])
took = re.findall(r"NOTICE: \[\d+/\d+\] (jef-sure/\S+ \(\S+\))", said.stdout)
index = os.path.join(build, "dgx_fonts", "dgx_fonts-files.txt")
if not os.path.exists(index):
    sys.exit(f"check_build_fonts.py: no {index}")
script = """
const fs = require('fs'), path = require('path');
const [module, directory] = process.argv.slice(1);
require(module).create({log: () => {}}).then(async preview => {
    preview.screen(160, 80, 16, 1), preview.names(null);
    const given = await preview.fonts(fs.readFileSync(path.join(directory, 'dgx_fonts-files.txt'), 'utf8'),
                                      file => fs.promises.readFile(path.join(directory, file)));
    const appearance = preview.appearance('{"dgx-ui": 1, "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#202020", ' +
        '"border": "#808080"}, "styles": {"big": {"fonts": ["large"]}, "small": {"fonts": ["small"]}}}');
    const page = preview.page('{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "label", ' +
        '"text": "CO2 123", "style": "big", "want": [0, 40]}, {"kind": "label", "text": "a small one", "style": "small"}]}}');
    preview.paint(1000);
    const frame = preview.frame(), levels = new Set();
    for (let k = 0; k < frame.length; k += 4) levels.add(frame[k]);
    console.log(JSON.stringify({given: given.map(one => [one.name, one.verdict.code, one.verdict.said]), appearance: appearance.code,
                                page: page.code, notes: preview.notes(), levels: levels.size, version: preview.version().dgx}));
});
"""
said = subprocess.run(["node", "-e", script, os.path.join(ROOT, "editor", "preview", "dgx_ui_preview.js"), os.path.dirname(index)],
                      capture_output=True, text=True, timeout=300)
if said.returncode:
    sys.exit("check_build_fonts.py: the module in Node: " + said.stderr[-2000:])
result = json.loads(said.stdout)
refused = [one for one in result["given"] if one[1] != 0]
wrong = refused or result["appearance"] or result["page"] or result["notes"] or result["levels"] <= 2
print(f"check_build_fonts.py: {', '.join(took)}; {len(result['given'])} fonts of DGX's index, {len(refused)} refused; "
      f"appearance {result['appearance']}, page {result['page']}, notes {result['notes']}, {result['levels']} levels; "
      f"module of DGX {result['version']}")
sys.exit(1 if wrong else 0)
