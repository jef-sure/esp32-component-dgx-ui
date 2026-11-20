#!/usr/bin/env python3
"""
test_generate.py <dgx_ui_c> <directory> <compile command...>: dgx_ui_c against the loader.

The descriptions of the cases are written to the directory and dgx_ui_c is run on each, with the names file of the
program of test_generate.c or by its rule. Every description it took is compiled with test_generate.c by the compile
command given (the program's sources and flags; this adds the files, -I the directory and -o), and the program adds
each made tree beside the loaded one and holds them together. What dgx_ui_c said of each description it refused is
held against what the loader says of it, the code, the byte and the name; the first comment of a file made by the rule
names what was not checked, and one made by a names file that gives every table names nothing.

The cases: the description of every kind and key, by the names file and by the rule; trees made by chance, by the
names file (some made wrong on purpose: an unknown name, a key of no meaning, a number for a ctx its handler does not
take, a switch of one picture, an id twice, a kind unknown, an icon past its pictures) and by the rule; the
descriptions test_load refuses; a name the rule cannot make C, a names file that is wrong, a names file of one table.
"""
import os
import random
import re
import subprocess
import sys

tool, work, compile_command = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2]), sys.argv[3:]
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(os.path.dirname(here))
os.makedirs(work, exist_ok=True)
failures = []

# the codes of the loader's errors, in the order of include/dgx_ui_load.h
with open(os.path.join(root, "include", "dgx_ui_load.h")) as header:
    codes = re.findall(r"^\s+(DGX_UI_LOAD_[A-Z_]+),", header.read(), re.M)
assert codes[0] == "DGX_UI_LOAD_OK" and "DGX_UI_LOAD_ID" in codes, codes

# the program's vocabulary, as the tables of test_generate.c have it: the table, the name, the C, a word
LIBRARY_LOOKS = ["button", "slider", "switch", "list", "dropdown", "input", "radio", "picture", "ring", "checkbox", "level", "text",
                 "picture_button", "box", "frame"]
VOCABULARY = [("look", word, "&dgx_ui_look_" + word, "") for word in LIBRARY_LOOKS] + [
    ("look", "mine", "&mine", ""),
    ("value", "volume", "&volume", ""), ("value", "choice", "&choice", ""), ("value", "ticked", "&ticked", ""),
    ("value", "picked", "&picked", ""), ("value", "loudness", "&volume", ""),
    ("picture", "dot", "dot", "1"), ("picture", "dots", "dots", "2"), ("picture", "pip", "dot", "1"),
    ("style", "text", "&text", ""), ("style", "small", "&small", ""), ("style", "big", "&small", ""),
    ("on", "back", "dgx_ui_load_on_back", ""), ("on", "page", "dgx_ui_load_on_page", ""), ("on", "saved", "saved", ""),
    ("on", "step", "step", "number"), ("on", "tap", "saved", ""),
    ("take", "take", "take", ""),
    ("ctx", "settings", "&settings", ""), ("ctx", "other", "&other", ""),
    ("id", "title", "title_index", ""), ("id", "slider", "slider_index", ""), ("id", "less", "less_index", ""), ("id", "go", "go_index", ""),
]
IDS = ["title", "slider", "less", "go"]
names_path = os.path.join(work, "names.txt")
with open(names_path, "w") as out:
    out.write("# the program of test_generate.c: a table, a name, its C, a count of pictures or \"number\"\n")
    for table, name, c, word in VOCABULARY:
        out.write(f"{table:8} {name:15} {c:30} {word}\n".rstrip() + "\n")


def names_of(table, rule):
    """the names a description may use: by the rule, only those whose rule leads to the program's own object"""
    taken = [name for t, name, c, _ in VOCABULARY if t == table]
    if rule:
        aliases = {"loudness", "pip", "big", "tap"}
        taken = [name for name in taken if name not in aliases]
    return taken


EVERY = r'''{"tree": {"children": [
  {"kind": "label", "text": "Volume é😀", "id": "title", "lines": 2, "style": "SMALL", "text_size": 40},
  {"kind": "slider", "value": "VOLUME", "min": -5, "max": 100, "step": 5, "want": [0, 12], "id": "slider", "tag": 7},
  {"kind": "button", "text": "Less", "on": "step", "flags": ["repeat"], "ctx": -5, "id": "less"},
  {"kind": "frame", "flags": ["row", "stretch"], "pad": 3, "gap": 4, "at": [2, -3], "want": [80, 20], "take": "take", "children": [
     {"kind": "switch", "value": "ticked", "text": "Tick", "look": "checkbox"},
     {"kind": "radio", "value": "choice", "step": 2, "text": "Two", "state": ["disabled"]}]},
  {"kind": "list", "value": "picked", "max": 2, "text": "a\nb\nc", "want": [0, 30]},
  {"kind": "dropdown", "value": "picked", "max": 2, "text": "a\nb\nc"},
  {"kind": "input", "text_size": 8, "look": "mine"},
  {"kind": "picture", "picture": "DOT"},
  {"kind": "icon", "picture": "dots", "value": "picked", "max": 1, "look": "picture"},
  {"kind": "own", "look": "ring", "value": "volume", "max": 100, "want": [30, 30], "pad": 5, "children": [{"kind": "box", "look": null}]},
  {"kind": "button", "picture": "dot", "look": "picture_button", "on": "page", "ctx": "settings", "id": "go", "state": ["hidden"]},
  {"kind": "button", "text": "Save \"q\"??= \\", "on": "SAVED", "ctx": "other"}
], "kind": "box", "flags": ["column"], "pad": 2, "gap": 2}, "dgx-ui": 1}'''
EVERY_NAMES = EVERY.replace("SMALL", "big").replace("VOLUME", "loudness").replace("DOT", "pip").replace("SAVED", "tap")
EVERY_RULE = EVERY.replace('"style": "SMALL", ', "").replace("VOLUME", "volume").replace("DOT", "dot").replace("SAVED", "saved")

# ---- trees by chance ----

TEXTED = {"label", "button", "list", "input", "dropdown", "radio", "switch"}
VALUED = {"list", "switch", "slider", "dropdown", "own", "radio", "icon"}
LEAVES = ["label", "picture", "icon", "list", "switch", "slider", "button", "input", "dropdown", "radio"]
WRONGS = ["unknown", "field", "number", "one_picture", "id_twice", "kind", "icon_past"]


def text_of(rand, kind):
    letters = "abcdefghij KLM 0123 éж"
    words = ["".join(rand.choice(letters) for _ in range(rand.randint(1, 9))) for _ in range(3 if kind in ("list", "dropdown") else 1)]
    return "\n".join(words)


def quoted(text):
    out = '"'
    for c in text:
        if c in '"\\':
            out += "\\" + c
        elif c == "\n":
            out += "\\n"
        elif ord(c) > 0x7f and ord(c) % 2:
            out += "\\u%04x" % ord(c)  # some escaped, some as UTF-8
        else:
            out += c
    return out + '"'


class Chance:
    def __init__(self, rand, rule, wrong):
        self.rand, self.rule, self.wrong, self.count, self.ids, self.ids_used = rand, rule, wrong, 0, list(IDS), []
        self.wrong_widget = rand.randint(0, 6) if wrong else -1
        self.title_again  = False  # the widget after the wrong one takes its id too
        self.injected     = False

    def widget(self, depth):
        rand, rule = self.rand, self.rule
        self.count += 1
        own_number = self.count - 1
        holds = depth < 4 and self.count < 40 and rand.randint(0, 2) == 0
        kind = rand.choice(["box", "frame", "own"]) if holds else rand.choice(LEAVES)
        keys = [f'"kind": "{kind}"']
        if rand.randint(0, 1):
            keys.append(f'"tag": {rand.randint(-100, 99)}')
        if rand.randint(0, 1):
            keys.append(f'"want": [{rand.randint(0, 59)}, {rand.randint(0, 29)}]')
        if rand.randint(0, 2) == 0:
            keys.append(f'"at": [{rand.randint(-10, 9)}, {rand.randint(0, 19)}]')
        if rand.randint(0, 3) == 0:
            keys.append('"state": ' + rand.choice(['["disabled"]', '["hidden"]', '["hidden", "disabled"]']))
        flags = [f for f in ("stretch", "rest", "repeat") if rand.randint(0, 4) == 0]
        if kind in ("box", "frame"):
            flags.append(rand.choice(["column", "row"]))
        if flags:
            keys.append('"flags": [' + ", ".join(f'"{f}"' for f in flags) + "]")
        if holds:
            keys.append(f'"pad": {rand.randint(0, 4)}')
            if rand.randint(0, 1):
                keys.append(f'"gap": {rand.randint(0, 4)}')
        if kind == "own":
            keys.append('"look": "' + rand.choice(["ring", "level"]) + '"')
        if kind in VALUED:
            keys.append('"value": "' + rand.choice(names_of("value", rule)) + '"')
            if kind == "icon":
                low = rand.randint(-3, 3)
                keys += [f'"min": {low}', f'"max": {low + rand.randint(0, 1)}', '"picture": "dots"', '"look": "picture"']
            elif kind == "radio":
                keys.append(f'"step": {rand.randint(0, 3)}')
            else:
                keys.append(f'"max": {rand.randint(1, 100)}')
                if rand.randint(0, 2) == 0:
                    keys.append(f'"min": {rand.randint(-20, 0)}')
                if kind == "slider" and rand.randint(0, 2) == 0:
                    keys.append(f'"step": {rand.randint(1, 9)}')
        if kind == "picture":
            keys.append('"picture": "' + rand.choice(names_of("picture", rule)) + '"')
        if kind in ("switch", "radio") and rand.randint(0, 3) == 0:
            keys += ['"picture": "dots"', '"look": "picture_button"']
        elif kind == "button" and rand.randint(0, 3) == 0:
            keys += ['"picture": "' + rand.choice(names_of("picture", rule)) + '"', '"look": "picture_button"']
        elif kind == "input" and rand.randint(0, 2) == 0:
            keys.append('"look": "mine"')
        elif kind == "label" and rand.randint(0, 3) == 0:
            keys.append('"look": ' + rand.choice(['null', '"text"']))
        if kind in TEXTED and (kind != "input" or rand.randint(0, 1)) and not (kind in ("switch", "radio") and '"look": "picture_button"' in keys):
            text = text_of(rand, kind)
            keys.append('"text": ' + quoted(text))
            if rand.randint(0, 2) == 0 or kind == "input":
                keys.append(f'"text_size": {len(text.encode()) + rand.randint(1, 10)}')
            if rand.randint(0, 3) == 0:
                keys.append(f'"lines": {rand.randint(1, 3)}')
            if rand.randint(0, 1):
                keys.append('"style": "' + rand.choice(names_of("style", rule)) + '"')
        elif kind == "input":
            keys.append(f'"text_size": {rand.randint(1, 12)}')
        if kind in ("button", "switch", "slider", "list", "radio") and rand.randint(0, 2) == 0:
            on = rand.choice(names_of("on", rule))
            keys.append(f'"on": "{on}"')
            if on == "step":
                keys.append(f'"ctx": {rand.randint(-5, 5)}')
            elif on == "page":
                keys.append('"ctx": "settings"')
            elif rand.randint(0, 1):
                keys.append('"ctx": "other"')
        if rand.randint(0, 9) == 0:
            keys.append('"take": "take"')
        if self.ids and rand.randint(0, 5) == 0:
            name = self.ids.pop(rand.randrange(len(self.ids)))
            self.ids_used.append(name)
            keys.append(f'"id": "{name}"')
        if self.title_again and own_number != self.wrong_widget:
            keys, self.title_again = [k for k in keys if not k.startswith('"id"')] + ['"id": "title"'], False
        if own_number == self.wrong_widget:
            keys, self.injected = self.made_wrong(kind, keys), True
        children = [self.widget(depth + 1) for _ in range(rand.randint(1, 4))] if holds else []
        if holds:
            keys.append('"children": [' + ", ".join(children) + "]")
        rand.shuffle(keys)
        return "{" + ", ".join(keys) + "}"

    def made_wrong(self, kind, keys):
        how = self.rand.choice(WRONGS)
        if how == "unknown":
            keys.append('"take": "nothing"' if not any(k.startswith('"take"') for k in keys) else '"tag": 1')
        elif how == "field":
            keys.append('"pad": 1' if kind not in ("box", "frame", "own") else '"text": "x"')
        elif how == "number":
            keys = [k for k in keys if not k.startswith('"on"') and not k.startswith('"ctx"')] + ['"on": "saved"', '"ctx": 3']
        elif how == "one_picture":
            keys = [f'"kind": "switch"', '"picture": "pip"']
        elif how == "id_twice":
            keys = [k for k in keys if not k.startswith('"id"')] + ['"id": "title"']
            self.title_again = True
            if "title" in self.ids:
                self.ids.remove("title")
        elif how == "kind":
            keys = [k for k in keys if not k.startswith('"kind"')] + ['"kind": "slidr"']
        else:
            keys = ['"kind": "icon"', '"picture": "dots"', '"max": 5', '"look": "picture"']
        return keys


def chance_tree(seed, rule, wrong):
    """a tree of the seed; one to be made wrong made again until the wrong widget is in it"""
    rand = random.Random(seed)
    while True:
        chance = Chance(rand, rule, wrong)
        tree = chance.widget(1)
        if chance.injected or not wrong:
            break
    return ('{"dgx-ui": 1, "tree": ' + tree + "}") if seed % 2 else ('{"tree": ' + tree + ', "dgx-ui": 1}'), chance.ids_used


# ---- the cases: every_names first, the one test_generate.c also adds with no room ----

cases = [("every_names", EVERY_NAMES, False, ["title", "slider", "less", "go"]), ("every_rule", EVERY_RULE, True, ["title", "slider", "less", "go"])]
for k in range(70):
    tree, used = chance_tree(1000 + k, False, k % 6 == 5)
    cases.append((f"chance_names{k}", tree, False, used))
for k in range(20):
    tree, used = chance_tree(2000 + k, True, False)
    cases.append((f"chance_rule{k}", tree, True, used))
REFUSED = [
    '{"dgx-ui": 2, "tree": {"kind": "box"}}',
    '{"tree": {"kind": "box"}}',
    '{"dgx-ui": 1, "tree": {"kind": "box"}, "fonts": 1}',
    '{"dgx-ui": 1, "tree": {"kind": "boxx"}}',
    '{"dgx-ui": 1, "tree": {"pad": 1}}',
    '{"dgx-ui": 1, "tree": {"kind": "box", "kind": "box"}}',
    '{"dgx-ui": 1, "tree": {"kind": "label", "value": "volume"}}',
    '{"dgx-ui": 1, "tree": {"kind": "box", "colour": 1}}',
    '{"dgx-ui": 1, "tree": {"kind": "box", "pad": 1.5}}',
    '{"dgx-ui": 1, "tree": {"kind": "box", "pad": 256}}',
    '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "loudnes"}}',
    '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "vol ume"}}',
    '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "aaaaaaaaaaaaaaaaaaaaaaaab"}}',
    '{"dgx-ui": 1, "tree": {"kind": "label", "text": "\\ud83d"}}',
    '{"dgx-ui": 1, "tree": {"kind": "label", "text": "\\q"}}',
    '{"dgx-ui": 1, "tree": {"kind": "label", "text": "a\udce0\udc80\udc80"}}',  # three bytes for a point of two
    '{"dgx-ui": 1, "tree": {"kind": "box"}} x',
    '{"dgx-ui": 1, "tree": {"kind": "label", "text": "abc", "text_size": 3}}',
    '{"dgx-ui": 1, "tree": {"kind": "own"}}',
    '{"dgx-ui": 1, "tree": {"kind": "icon", "picture": "dots"}}',
    '{"dgx-ui": 1, "tree": {"kind": "label", "lines": 2}}',
    '{"dgx-ui": 1, "tree": {"kind": "button", "on": "page", "ctx": 5}}',
    '{"dgx-ui": 1, "tree": {"kind": "button", "ctx": 5}}',
    '{"dgx-ui": 1, "tree": {"kind": "button", "ctx": 5, "on": "saved"}}',
    '{"dgx-ui": 1, "tree": {"kind": "switch", "picture": "pip", "value": "ticked", "look": "picture_button"}}',
    '{"dgx-ui": 1, "tree": {"kind": "radio", "value": "choice", "picture": "dot"}}',
    '{"dgx-ui": 1, "tree": {"kind": "icon", "picture": "dots", "value": "picked", "max": 2, "look": "picture"}}',
    '{"dgx-ui": 1, "tree": {"kind": "icon", "min": 3, "max": 4, "value": "picked", "picture": "dot", "look": "picture"}}',
    '{"dgx-ui": 1, "tree": {"kind": "box", "children": [{"kind": "label", "id": "go"}, {"kind": "box", "children": [{"kind": "label", "id": "go"}]}]}}',
    '{"dgx-ui": 1, "tree": {"kind": "slider", "look": "yours"}}',
    '{"dgx-ui": 1, "tree": ' + "".join('{"kind": "box", "children": [' for _ in range(24)) + '{"kind": "box"}' + "]}" * 24 + "}",
    '{"dgx-ui": 1, "tree": {"kind": "box", "x": ' + "[" * 60 + "]" * 60 + "}}",
]
for k, text in enumerate(REFUSED):
    cases.append((f"refused{k}", text, False, []))


def run_tool(name, text, rule, names=None):
    path = os.path.join(work, name + ".json")
    with open(path, "w", encoding="utf-8", errors="surrogateescape") as out:
        out.write(text)
    command = [tool, "-i", "generate_program.h", "-d", work]
    if not rule:
        command += ["-n", names or names_path]
    run = subprocess.run(command + [path, name], capture_output=True, text=True, timeout=30)
    return run.returncode, run.stderr.strip()


taken, refused = [], []
for name, text, rule, used in cases:
    status, said = run_tool(name, text, rule)
    if status == 0:
        taken.append((name, text, rule, used))
        continue
    found = re.search(r": byte (-?\d+): (DGX_UI_LOAD_[A-Z_]+)(?: \"(.*)\")?$", said)
    if rule or not found:
        failures.append(f"{name}: dgx_ui_c refused it: {said}")
        continue
    refused.append((name, text, codes.index(found.group(2)), int(found.group(1)), found.group(3) or ""))
if not any(name.startswith("refused") for name, *_ in refused) or sum(1 for name, *_ in refused if name.startswith("chance")) < 5:
    failures.append(f"too few refusals: {[name for name, *_ in refused]}")
if len([1 for name, *_ in taken if name.startswith("chance_names")]) < 50:
    failures.append("too few trees by chance taken")


def c_literal(text):
    data = text.encode("utf-8", "surrogateescape")
    return '"' + "".join(chr(b) if 0x20 <= b < 0x7f and chr(b) not in '"\\?' else "\\%03o" % b for b in data) + '"'


# ---- the first comments ----
for name, text, rule, used in taken:
    with open(os.path.join(work, name + ".c"), encoding="utf-8") as made:
        head = made.read().split("*/")[0]
    has_rule = "Made C by the rule of dgx_ui_c" in head and "Not checked for want of the program's tables" in head and "pictures' counts" in head
    if rule and not has_rule:
        failures.append(f"{name}: the first comment does not say what was not checked")
    if not rule and "rule" in head:
        failures.append(f"{name}: the first comment speaks of a rule, every table given")
def made_text(name, suffix):
    """a file dgx_ui_c made; empty when it made none"""
    path = os.path.join(work, name + suffix)
    if not os.path.exists(path):
        return ""
    with open(path, encoding="utf-8") as made:
        return made.read()


if '"Less", 1, &every_rule_style' not in made_text("every_rule", ".c") or "extern dgx_ui_text_style_t every_rule_style;" not in made_text("every_rule", ".h"):
    failures.append("every_rule: a text that names no style has not the rule's every_rule_style")

# ---- the program ----
with open(os.path.join(work, "cases.h"), "w") as out:
    out.write("/* the cases of test_generate.py: made by it */\n")
    for name, *_ in taken:
        out.write(f'#include "{name}.h"\n')
    for name, text, rule, used in taken:
        if rule:
            out.write(f"dgx_ui_text_style_t {name}_style;\n")
    out.write("static const generate_case_t cases[] = {\n")
    for name, text, rule, used in taken:
        rule_ids = ", ".join(f"&{name}_{i}" if rule and i in used else "NULL" for i in IDS)
        out.write(f"    {{\"{name}\", {c_literal(text)}, {name}_add, {('&' + name + '_style') if rule else 'NULL'}, {{{rule_ids}}}}},\n")
    out.write("};\nstatic const generate_refusal_t refusals[] = {\n")
    for name, text, *_ in refused:
        out.write(f'    {{"{name}", {c_literal(text)}}},\n')
    out.write("};\n")
program = os.path.join(work, "test_generate")
made_files = [os.path.join(work, name + ".c") for name, *_ in taken]
built = subprocess.run(compile_command + ["-I" + work, "-I" + here, "-o", program] + made_files, capture_output=True, text=True, timeout=600)
if built.returncode:
    print(built.stderr[:6000], file=sys.stderr)
    sys.exit(f"test_generate.py: the made files did not compile")
ran = subprocess.run([program], capture_output=True, text=True, timeout=300)
sys.stderr.write(ran.stderr[:6000])
if ran.returncode:
    failures.append("test_generate.c failed")
said_by_loader = dict(re.findall(r"^REFUSAL (\S+) (.*)$", ran.stdout, re.M))
for name, text, code, offset, error_name in refused:
    loader = said_by_loader.get(name)
    if loader != f'{code} {offset} "{error_name}"':
        failures.append(f"{name}: dgx_ui_c said {codes[code]} at {offset} \"{error_name}\", the loader {loader}")

# ---- what dgx_ui_c alone refuses: a name the rule cannot make C, a names file that is wrong; one of one table ----
status, said = run_tool("dash", '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "my-volume"}}', True)
if status == 0 or 'the value "my-volume" is not a name of C' not in said or "byte 50" not in said:
    failures.append(f"a name with '-' by the rule: {status} {said}")
status, said = run_tool("dash_id", '{"dgx-ui": 1, "tree": {"kind": "label", "id": "a.b"}}', True)
if status == 0 or 'the id "a.b" is not a name of C' not in said:
    failures.append(f"an id with '.' by the rule: {status} {said}")
wrong_names = os.path.join(work, "wrong_names.txt")
for line, expect in [("colour red &red", 'no table "colour"'), ("value volume &volume 3", '"3" means nothing for a value'),
                     ("picture dots dots 0", "not a count of pictures"), ("value vol-ume", "a value with no name or no C expression"),
                     ("value volume &a\nvalue volume &b", 'the value "volume" twice'), ("value vol%ume &a", "not a name a description may have")]:
    with open(wrong_names, "w") as out:
        out.write(line + "\n")
    status, said = run_tool("wrong_names", '{"dgx-ui": 1, "tree": {"kind": "box"}}', False, wrong_names)
    if status == 0 or expect not in said:
        failures.append(f"names file {line!r}: {status} {said}")
one_table = os.path.join(work, "one_table.txt")
with open(one_table, "w") as out:
    out.write("value loudness &volume\n")
status, said = run_tool("one_table", '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "loudness"}}', False, one_table)
head, _, body = made_text("one_table", ".c").partition("*/")
if status or "look, picture, style, on, take, ctx, id" not in head.replace("\n *", "") or ".value = &volume" not in body:
    failures.append(f"a names file of one table: {status} {said}")
status, said = run_tool("one_table_unknown", '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "volume"}}', False, one_table)
if status == 0 or 'DGX_UI_LOAD_UNKNOWN "volume"' not in said:
    failures.append(f"a name the file's table has not: {status} {said}")

for failure in failures:
    print("  " + failure, file=sys.stderr)
print(f"test_generate.py: {len(taken)} taken, {len(refused)} refused alike" if not failures else f"test_generate.py: {len(failures)} failed")
sys.exit(1 if failures else 0)
