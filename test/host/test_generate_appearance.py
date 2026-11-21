#!/usr/bin/env python3
"""
test_generate_appearance.py <dgx_ui_c> <appearance_dump> <directory> <compile command...>: appearances made C by
dgx_ui_c -A against their loader, dgx_ui_appearance_load().

A names file of every draw of the library, a draw of the program's own with no description, two fonts, one of them
handwritten, both hands, and looks and styles of the program's own, made the loader's tables by -t: the tables of
pages and those of appearances. Appearances are made of the descriptions the library gives out (appearance_dump):
one with a look of every draw, every field said; one of the program's draw with no parameters; others by chance, every
kind of field, colours in any order, names of '.' and '-', editors; one by the rule, with no names file. Each is made
C, all are compiled into one program with test_generate_appearance.c, which loads each on the tables of the names file
and holds the made one against it field by field, and a page of a widget for each look of it drawn on both pixel by
pixel. The same appearances spoiled at a byte are given to dgx_ui_c and to the loader on the same tables: the same
verdict, the same code, byte and name. A tree made with -a on an appearance against the loader on the tables -t -a
makes of the two; the tree's file compiled against an appearance of other names stops at its stamp. The tool's own
refusals: a look of the program's draw that gives parameters, a file of another kind not written over.
"""
import json
import os
import random
import re
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
tool, dump, work, compile_command = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), os.path.abspath(sys.argv[3]), sys.argv[4:]
os.makedirs(work, exist_ok=True)
for leftover in os.listdir(work):
    if leftover.endswith((".c", ".h", ".json", ".txt")):
        os.remove(os.path.join(work, leftover))
failures = []
described = json.loads(subprocess.run([dump], capture_output=True, text=True, check=True, timeout=30).stdout)
DRAWS = sorted(described["draws"])
COLOR, UNDER, NUMBER, BOOL, WORD, FLAGS, BIT, BANDS, FONT, HAND, VARIANTS = range(11)
codes = {"DGX_UI_LOAD_" + word: k for k, word in enumerate(["OK", "SYNTAX", "VERSION", "KEY", "KIND", "FIELD", "RANGE", "NAME_BAD", "UNKNOWN", "NO_LOOK",
                                                           "NO_STYLE", "DEPTH_MAX", "MEMORY", "NODES", "ID", "MISSING"])}

NAMES = "".join(f"draw {word}\n" for word in DRAWS) + """draw dial draw_dial
font small TerminusTTFMedium12
font hand font0ss handwritten
hand handwriting
hand handwriting_canvas
look switch
look slider
look mine &mine
style plain &plain
value v &volume
"""
names_path = os.path.join(work, "names.txt")
with open(names_path, "w") as out:
    out.write(NAMES)


def run(*options):
    done = subprocess.run([tool, "-i", "appearance_program.h", "-d", work, *options], capture_output=True, text=True, timeout=60)
    return done.returncode, done.stderr.strip()


# ---- appearances by chance, of the descriptions ----

def chance_appearance(rand, name, every=False):
    """an appearance and its text: colours in any order, looks of chance draws (every draw when every), styles"""
    others = [f"c{k}" if rand.random() < 0.7 else f"c.{k}-x" for k in range(rand.randrange(0, 7))]
    colors = ["paper", "ink", "fill", "border"] + others
    written = colors[:]
    rand.shuffle(written)
    color_text = ", ".join(f'"{c}": "#{rand.randrange(1 << 24):06x}"' for c in written)

    def a_color(under):
        return "null" if under and rand.random() < 0.25 else f'"{rand.choice(colors)}"'

    def field_value(field):
        kind = field["kind"]
        if kind in (COLOR, UNDER):
            return a_color(kind == UNDER)
        if kind == NUMBER:
            return str(rand.randint(field["low"], field["high"]))
        if kind == BOOL:
            return rand.choice(["true", "false"])
        if kind == WORD:
            return f'"{rand.choice(field["words"])}"'
        if kind == BANDS:
            return "[" + ", ".join(f"[{rand.randint(-1000, 1000)}, {a_color(False)}]" for _ in range(rand.randrange(0, 4))) + "]"
        raise ValueError(kind)

    looks = []
    draws = DRAWS[:] if every else [rand.choice(DRAWS) for _ in range(rand.randrange(0, 6))]
    for k, draw in enumerate(draws):
        fields = described["draws"][draw]["fields"]
        said = [f for f in fields if f["kind"] in (COLOR, UNDER) or every or rand.random() < 0.6]
        rand.shuffle(said)
        params = ", ".join(f'"{f["name"]}": {field_value(f)}' for f in said)
        parts = [f'"draw": "{draw}"']
        if rand.random() < 0.5:
            parts.append(f'"fill": {a_color(True)}')
        if fields or rand.random() < 0.5:
            parts.append('"params": {' + params + "}")
        rand.shuffle(parts)
        look_name = rand.choice(["slider", "switch", f"l{k}"]) if not every and rand.random() < 0.2 else f"{draw}_{k}"
        if look_name in [n for n, _ in looks]:
            look_name = f"{draw}_{k}"
        looks.append((look_name, "{" + ", ".join(parts) + "}"))

    def variant():
        if rand.random() < 0.4:
            return '"small"'
        hand = rand.random() < 0.5
        parts = [f'"font": "{"hand" if hand else "small"}"']
        if hand or rand.random() < 0.3:
            parts.append(f'"hand": "{rand.choice(["handwriting", "handwriting_canvas"])}"')
        for key, low, high in (("height", 0, 40), ("narrow", 0, 100), ("pen", 0, 5)):
            if rand.random() < 0.5:
                parts.append(f'"{key}": {rand.randint(low, high)}')
        for key in ("joined", "soft"):
            if rand.random() < 0.5:
                parts.append(f'"{key}": {rand.choice(["true", "false"])}')
        rand.shuffle(parts)
        return "{" + ", ".join(parts) + "}"

    styles = []
    for k in range(rand.randrange(1 if every else 0, 4)):
        parts = ['"fonts": [' + ", ".join(variant() for _ in range(rand.randrange(1, 4))) + "]"]
        if rand.random() < 0.6:
            parts.append(f'"align": "{rand.choice(["left", "center", "right"])}"')
        if rand.random() < 0.6:
            parts.append('"flags": [' + ", ".join(f'"{w}"' for w in rand.sample(["middle", "important"], rand.randrange(0, 3))) + "]")
        if rand.random() < 0.5:
            parts.append(f'"scroll_ms": {rand.randrange(65536)}')
        rand.shuffle(parts)
        style_name = "plain" if rand.random() < 0.15 else f"s{k}"
        if style_name in [n for n, _ in styles]:
            style_name = f"s{k}"
        styles.append((style_name, "{" + ", ".join(parts) + "}"))
    top = [f'"dgx-ui": 1', f'"name": "{name}"', '"colors": {' + color_text + "}",
           '"looks": {' + ", ".join(f'"{n}": {v}' for n, v in looks) + "}", '"styles": {' + ", ".join(f'"{n}": {v}' for n, v in styles) + "}"]
    if rand.random() < 0.3:
        top.append('"editor": {"zoom": 1.5, "grid": [8, 8.5e0]}')
    rand.shuffle(top)
    return "{" + ", ".join(top) + "}"


cases = []  # (name, text): made C and compiled


def make(name, text, *options):
    with open(os.path.join(work, name + ".json"), "w") as out:
        out.write(text)
    status, said = run(*options, "-A", os.path.join(work, name + ".json"))
    if status:
        failures.append(f"{name}: refused: {said}")
    else:
        cases.append((name, text))
    return status, said


rand = random.Random(4545)
make("every", chance_appearance(rand, "every", every=True), "-n", names_path)
make("dialed", '{"dgx-ui": 1, "name": "dialed", "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#101010", "border": "#202020", "x-y": "#303030"},'
               ' "looks": {"dial": {"draw": "dial", "fill": "x-y"}, "big": {"draw": "button", "params": {"face": "fill", "pressed": "ink", "focused": null,'
               ' "disabled": "paper", "text": "ink", "text_disabled": "border", "shadow": "paper", "depth": 1}}}}', "-n", names_path)
for k in range(40):
    make(f"chance{k}", chance_appearance(rand, f"chance{k}"), "-n", names_path)
# by the rule: no names file; every draw of the library, both hands, the fonts dgx_font_<name>, the program's own
make("ruled", '{"dgx-ui": 1, "name": "ruled", "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#101010", "border": "#202020"},'
              ' "styles": {"body": {"fonts": ["small", {"font": "hand", "hand": "handwriting", "height": 20}]}}, "looks": {"pane": {"draw": "frame", "params": {"border": "ink", "width": 1}}}}')
# by the rule a font is taken for a bitmap one: a handwritten font with no hand is taken, and the file says it was not checked
with open(os.path.join(work, "unchecked.json"), "w") as out:
    out.write('{"dgx-ui": 1, "name": "unchecked", "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#101010", "border": "#202020"},'
              ' "styles": {"body": {"fonts": [{"font": "hand", "height": 20}]}}}')
status, said = run("-A", os.path.join(work, "unchecked.json"))
words = " ".join(open(os.path.join(work, "unchecked.h")).read().replace("\n *", " ").split()) if not status else ""
if status or "Not checked for want of the program's tables: which fonts are handwritten, whose variants must name a hand." not in words:
    failures.append(f"by the rule, a hand not checked: {status} {said}")
for suffix in (".c", ".h"):
    if os.path.exists(os.path.join(work, "unchecked" + suffix)):
        os.remove(os.path.join(work, "unchecked" + suffix))

# ---- the loader's tables of the names file, and the tables of pages on an appearance ----
status, said = run("-n", names_path, "-t", "vocab")
if status:
    failures.append(f"-t vocab: {said}")
status, said = run("-n", names_path, "-a", os.path.join(work, "every.json"), "-an", names_path, "-af", "every", "-t", "every_tables")
if status:
    failures.append(f"-t -a: {said}")
# a tree on the appearance, by -a: a widget of each of its first looks, a text of its first style and one of the program's
every = json.loads(cases[0][1])
look_names = list(every["looks"])
page = {"dgx-ui": 1, "name": "page", "tree": {"kind": "box", "flags": ["column"], "children": [
    {"kind": "slider", "value": "v", "want": [0, 8]}, {"kind": "switch", "value": "v", "want": [0, 9]},
    {"kind": "label", "text": "Ab"}, {"kind": "label", "text": "Cd", "style": "plain"},
    {"kind": "button", "look": [n for n in look_names if n.startswith("button_")][0], "text": "Go", "want": [0, 16]},
    {"kind": "frame", "look": [n for n in look_names if n.startswith("frame_")][0], "want": [0, 6]}]}}
with open(os.path.join(work, "page.json"), "w") as out:
    json.dump(page, out)
status, said = run("-n", names_path, "-a", os.path.join(work, "every.json"), "-an", names_path, "-af", "every", os.path.join(work, "page.json"))
if status:
    failures.append(f"-a page: {said}")

# ---- spoiled: dgx_ui_c and the loader on the same tables, the same verdict ----
spoiled = []  # (name, text, tool's code, byte, name) or a taken one with code None
for k in range(240):
    name, text = cases[k % len(cases[:-1])]
    if name == "ruled":
        continue
    data = bytearray(text, "utf-8")
    data[rand.randrange(len(data))] = ord(rand.choice('"{}[],:#0an x.-'))
    spoiled_name = f"spoiled{k}"
    path = os.path.join(work, spoiled_name + ".json")
    with open(path, "wb") as out:
        out.write(data)
    status, said = run("-n", names_path, "-p", spoiled_name, "-A", path)
    found = re.search(r": byte (-?\d+): (DGX_UI_LOAD_[A-Z_]+)(?: \"(.*)\")?$", said)
    if status and found:
        spoiled.append((spoiled_name, bytes(data), codes[found.group(2)], int(found.group(1)), found.group(3) or ""))
    elif status:
        spoiled.append((spoiled_name, bytes(data), "tool", 0, said))  # the tool's own words: held below
    else:
        spoiled.append((spoiled_name, bytes(data), 0, 0, ""))
    for suffix in (".c", ".h"):
        if os.path.exists(os.path.join(work, spoiled_name + suffix)):
            os.remove(os.path.join(work, spoiled_name + suffix))


def c_literal(data):
    if isinstance(data, str):
        data = data.encode("utf-8")
    return '"' + "".join(chr(b) if 32 <= b < 127 and chr(b) not in '"\\?' else f"\\{b:03o}" for b in data) + '"'


# ---- the program ----
with open(os.path.join(work, "appearance_cases.h"), "w") as out:
    out.write('/* the cases of test_generate_appearance.py: made by it */\n#include "vocab.h"\n#include "every_tables.h"\n#include "page.h"\n')
    for name, _ in cases:
        out.write(f'#include "{name}.h"\n')
    out.write("static const appearance_case_t cases[] = {\n")
    for name, text in cases:
        out.write(f'    {{"{name}", {c_literal(text)}, &{name}_appearance, {name}_start}},\n')
    out.write("};\nstatic const appearance_refusal_t refusals[] = {\n")
    for name, data, *_ in spoiled:
        out.write(f'    {{"{name}", {c_literal(data)}, {len(data)}}},\n')
    out.write("};\n")
    out.write(f"static const char page_text[] = {c_literal(json.dumps(page))};\n")
program = os.path.join(work, "test_generate_appearance")
made = [os.path.join(work, f"{name}.c") for name, _ in cases] + [os.path.join(work, f) for f in ("vocab.c", "every_tables.c", "page.c")]
built = subprocess.run(compile_command + ["-I" + work, "-I" + here, "-o", program] + made, capture_output=True, text=True, timeout=600)
if built.returncode:
    print(built.stderr[:6000], file=sys.stderr)
    sys.exit("test_generate_appearance.py: the made files did not compile")
ran = subprocess.run([program], capture_output=True, text=True, timeout=300)
sys.stderr.write(ran.stderr[:6000])
if ran.returncode:
    failures.append("test_generate_appearance.c failed")
said_by_loader = dict(re.findall(r"^VERDICT (\S+) (.*)$", ran.stdout, re.M))
alike = 0
for name, data, code, offset, error_name in spoiled:
    loader = said_by_loader.get(name)
    if code == "tool":
        failures.append(f"{name}: the tool's own words on a spoiled appearance: {error_name}")
    elif loader != f'{code} {offset} "{error_name}"':
        failures.append(f"{name}: dgx_ui_c said {code} at {offset} \"{error_name}\", the loader {loader}")
    else:
        alike += 1

# ---- the stamp: the tree's file against an appearance of other names stops at compile time ----
other = os.path.join(work, "other")
os.makedirs(other, exist_ok=True)
renamed = every.copy()
renamed["looks"] = dict(reversed(list(every["looks"].items())))
with open(os.path.join(other, "every.json"), "w") as out:
    json.dump(renamed, out)
status, said = run("-n", names_path, "-d", other, "-A", os.path.join(other, "every.json"))
for kept in ("page.c", "page.h"):  # the tree's files kept beside the appearance of other names: its header found first
    with open(os.path.join(work, kept)) as source, open(os.path.join(other, kept), "w") as copy:
        copy.write(source.read())
stale = subprocess.run(compile_command[:1] + [w for w in compile_command[1:] if w.startswith(("-I", "-D", "-std", "-include")) or w.endswith("esp_log.h")] +
                       ["-I" + other, "-I" + here, "-c", "-o", os.devnull, os.path.join(other, "page.c")], capture_output=True, text=True, timeout=120)
if status or stale.returncode == 0 or "every.h is of another appearance than this file was made on: make it again" not in stale.stderr:
    failures.append(f"the stamp: {status} {said} {stale.returncode} {stale.stderr[-800:]}")

# ---- the tool's own refusals ----
with open(os.path.join(work, "dial_params.json"), "w") as out:
    out.write('{"dgx-ui": 1, "name": "dp", "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#101010", "border": "#202020"}, "looks": {"knob": {"draw": "dial", "params": {"needle": "ink"}}}}')
status, said = run("-n", names_path, "-A", os.path.join(work, "dial_params.json"))
if status == 0 or "the draw \"dial\"'s parameters are described in the program's C, which dgx_ui_c cannot read: name the look \"knob\" as a look of the program's own" not in said:
    failures.append(f"a look of the program's draw with params: {status} {said}")
with open(os.path.join(work, "no_hand.json"), "w") as out:
    out.write('{"dgx-ui": 1, "name": "nh", "colors": {"paper": "#000000", "ink": "#ffffff", "fill": "#101010", "border": "#202020"}, "styles": {"s": {"fonts": [{"font": "hand"}]}}}')
status, said = run("-n", names_path, "-A", os.path.join(work, "no_hand.json"))
if status == 0 or 'DGX_UI_LOAD_MISSING "hand"' not in said:
    failures.append(f"a handwritten font with no hand: {status} {said}")
# a file of another kind not written over: the tree's page.c for an appearance, the appearance's every.c for a tree
status, said = run("-n", names_path, "-p", "page", "-A", os.path.join(work, "every.json"))
if status == 0 or "page.c: made by dgx_ui_c of a description, not of an appearance: not written over" not in said:
    failures.append(f"an appearance over a tree's files: {status} {said}")
with open(os.path.join(work, "bare.json"), "w") as out:
    out.write('{"dgx-ui": 1, "tree": {"kind": "box"}}')
status, said = run("-n", names_path, "-p", "every", os.path.join(work, "bare.json"))
if status == 0 or "every.c: made by dgx_ui_c of an appearance, not of a description: not written over" not in said:
    failures.append(f"a tree over an appearance's files: {status} {said}")
for options, words in [(["-A", os.path.join(work, "every.json"), "-t", "x"], "-A makes an appearance alone"),
                       (["-ap", "x", os.path.join(work, "page.json")], "-an, -ap and -af are of the appearance of -a: no -a here"),
                       (["-a", os.path.join(work, "every.json"), "-an", names_path, "-ap", "page", os.path.join(work, "page.json")], "the tree's prefix page is the appearance's too")]:
    status, said = run("-n", names_path, *options)
    if status == 0 or words not in said:
        failures.append(f"{' '.join(options)}: {status} {said}")

# -a reads the appearance on the names file of -an, as -A read it, or by the rule: one of the program's draw by -an alone
status, said = run("-n", names_path, "-a", os.path.join(work, "dialed.json"), "-p", "bare_on_dialed", os.path.join(work, "bare.json"))
if status == 0 or 'DGX_UI_LOAD_UNKNOWN "dial"' not in said:
    failures.append(f"-a with no -an, an appearance of the program's draw: {status} {said}")
status, said = run("-n", names_path, "-a", os.path.join(work, "dialed.json"), "-an", names_path, "-p", "bare_on_dialed", os.path.join(work, "bare.json"))
if status or "_Static_assert(dialed_stamp" not in open(os.path.join(work, "bare_on_dialed.c")).read():
    failures.append(f"-a with -an: {status} {said}")

# by the rule, with no names file: a text that names no style takes the appearance's first, not the rule's own
with open(os.path.join(work, "ruled_page.json"), "w") as out:
    out.write('{"dgx-ui": 1, "name": "ruled_page", "tree": {"kind": "label", "text": "Ab"}}')
status, said = run("-a", os.path.join(work, "every.json"), "-an", names_path, os.path.join(work, "ruled_page.json"))
ruled_c = open(os.path.join(work, "ruled_page.c")).read() if not status else ""
ruled_h = open(os.path.join(work, "ruled_page.h")).read() if not status else ""
if status or 'DGX_UI_TEXT_CONST("Ab", 1, &every_style[0])' not in ruled_c or "ruled_page_style" in ruled_h:
    failures.append(f"a tree by the rule on an appearance: its default style: {status} {said}")

# the made header read by C++
cxx_flags, take_next = [], False
for word in compile_command:
    if take_next or word.startswith("-I") or word.startswith("-D"):
        cxx_flags.append(word)
    take_next = word == "-include"
    if take_next:
        cxx_flags.append(word)
cxx = subprocess.run(["c++", "-x", "c++", "-fsyntax-only", "-Wall", "-Wextra", *cxx_flags, "-I" + work, os.path.join(work, "every.h")], capture_output=True, text=True, timeout=120)
if cxx.returncode:
    failures.append(f"every.h read by C++: {cxx.stderr[:2000]}")

for failure in failures:
    print("  " + failure[:600], file=sys.stderr)
print(f"test_generate_appearance.py: {len(cases)} made and held, {alike} spoiled judged alike" if not failures else f"test_generate_appearance.py: {len(failures)} failed")
sys.exit(1 if failures else 0)
