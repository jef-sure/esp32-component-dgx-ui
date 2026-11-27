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
take, a switch of one picture, an id twice, a kind unknown, an icon past its pictures) and by the rule, each named by
the description or by -p; texts in buffers of several sizes, none buffered, none at all; the descriptions test_load
refuses, with -p and without; a name the rule cannot make C, a names file that is wrong, a names file of one table;
the prefix, the tree's name or -p's, and the names the tool makes itself refused as ids and as the lead of a name; a
file of the program's not written over, one the tool made written again, one of the other kind refused; a prefix of the
libraries' refused. The loader's tables made of the names file by -t (some of its lines the library's words with no
expression) are compiled into test_generate.c with the cases, and so are those of a file of two tables, whose cases are
made and loaded on its tables alone; the made header is read by C++; the refusals of -t and of the names file's limits.
The library's action command, by the rule and in a names file's tables.
"""
import json
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
                 "picture_button", "box", "frame", "scroll"]
VOCABULARY = [("look", word, "&dgx_ui_look_" + word, "") for word in LIBRARY_LOOKS] + [
    ("look", "mine", "&mine", ""),
    ("value", "volume", "&volume", ""), ("value", "choice", "&choice", ""), ("value", "ticked", "&ticked", ""),
    ("value", "picked", "&picked", ""), ("value", "loudness", "&volume", ""),
    ("picture", "dot", "dot", "1"), ("picture", "dots", "dots", "2"), ("picture", "pip", "dot", "1"),
    ("style", "text", "&text", ""), ("style", "small", "&small", ""), ("style", "big", "&small", ""),
    ("on_event", "back", "dgx_ui_action_back", ""), ("on_event", "page", "dgx_ui_action_page", ""), ("on_event", "saved", "saved", ""),
    ("on_event", "step", "step", "number"), ("on_event", "tap", "saved", ""),
    ("take", "take", "take", ""),
    ("ctx", "settings", "&settings", "page"), ("ctx", "other", "&other", ""),
    ("id", "title", "title_index", ""), ("id", "slider", "slider_index", ""), ("id", "less", "less_index", ""), ("id", "go", "go_index", ""),
]
IDS = ["title", "slider", "less", "go"]
names_path = os.path.join(work, "names.txt")
with open(names_path, "w") as out:
    out.write("# the program of test_generate.c: a table, a name, its C, a count of pictures, \"number\" or \"page\"\n")
    for table, name, c, word in VOCABULARY:
        # a look of the library's every other one, and back, by the word alone
        if (table == "look" and name in LIBRARY_LOOKS and LIBRARY_LOOKS.index(name) % 2 == 0) or (table, name) == ("on_event", "back"):
            c = ""
        out.write(f"{table:8} {name:15} {c:30} {word}\n".rstrip() + "\n")
# a names file of two tables: the cases made by it are loaded on its tables alone
partial_path = os.path.join(work, "partial.txt")
with open(partial_path, "w") as out:
    out.write("look slider\nvalue volume &volume\n")
PARTIAL = {"partial_slider", "partial_unknown_on", "partial_no_style", "partial_no_look"}


def make_tables(names, prefix, *options):
    """dgx_ui_c -t: the loader's tables of a names file"""
    run = subprocess.run([tool, "-n", names, "-i", "generate_program.h", "-d", work, *options, "-t", prefix], capture_output=True, text=True, timeout=30)
    return run.returncode, run.stderr.strip()


for names, prefix in [(names_path, "program"), (partial_path, "partial")]:
    status, said = make_tables(names, prefix)
    if status:
        sys.exit(f"test_generate.py: the tables of {names} not made: {said}")


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
  {"kind": "button", "text": "Less", "on_event": "step", "flags": ["repeat"], "ctx": -5, "id": "less"},
  {"kind": "frame", "flags": ["row", "stretch"], "pad": 3, "gap": 4, "at": [2, -3], "want": [80, 20], "take": "take", "children": [
     {"kind": "switch", "value": "ticked", "text": "Tick", "look": "checkbox"},
     {"kind": "radio", "value": "choice", "step": 2, "text": "Two", "state": ["disabled"]}]},
  {"kind": "list", "value": "picked", "max": 2, "text": "a\nb\nc", "want": [0, 30]},
  {"kind": "dropdown", "value": "picked", "max": 2, "text": "a\nb\nc"},
  {"kind": "input", "text_size": 8, "look": "mine"},
  {"kind": "picture", "picture": "DOT"},
  {"kind": "icon", "picture": "dots", "value": "picked", "max": 1, "look": "picture"},
  {"kind": "own", "look": "ring", "value": "volume", "max": 100, "want": [30, 30], "pad": 5, "children": [{"kind": "box", "look": null}]},
  {"kind": "button", "picture": "dot", "look": "picture_button", "on_event": "page", "ctx": "settings", "id": "go", "state": ["hidden"]},
  {"kind": "button", "text": "Save \"q\"??= \\", "on_event": "SAVED", "ctx": "other"}
], "kind": "box", "flags": ["column"], "pad": 2, "gap": 2}, "dgx-ui": 1}'''
EVERY_NAMES = EVERY.replace("SMALL", "big").replace("VOLUME", "loudness").replace("DOT", "pip").replace("SAVED", "tap").replace(
    '"dgx-ui": 1}', '"dgx-ui": 1, "name": "every_names"}')
EVERY_RULE = EVERY.replace('"style": "SMALL", ', "").replace("VOLUME", "volume").replace("DOT", "dot").replace("SAVED", "saved")

# ---- trees by chance ----

TEXTED = {"label", "button", "list", "input", "dropdown", "radio", "switch"}
VALUED = {"list", "switch", "slider", "dropdown", "own", "radio", "icon"}
LEAVES = ["label", "picture", "icon", "list", "switch", "slider", "button", "input", "dropdown", "radio"]
WRONGS = ["unknown", "field", "number", "page_ctx", "one_picture", "id_twice", "kind", "icon_past"]


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

    def number_or_percent(self, low, high, percent_low):
        """points of low..high, or now and then a percent of percent_low % to 127.99 %, of 0, 1 or 2 decimals"""
        rand = self.rand
        if rand.randint(0, 3):
            return str(rand.randint(low, high))
        hundredths = rand.randint(percent_low * 100, 12799)
        sign, whole, part = "-" if hundredths < 0 else "", abs(hundredths) // 100, abs(hundredths) % 100
        decimals = rand.choice(["", f".{part // 10}", f".{part:02d}"]) if part else rand.choice(["", ".0", ".00"])
        return f'"{sign}{whole}{decimals}%"'

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
            keys.append(f'"want": [{self.number_or_percent(0, 59, 0)}, {self.number_or_percent(0, 29, 0)}]')
        if rand.randint(0, 2) == 0:
            keys.append(f'"at": [{self.number_or_percent(-10, 9, -128)}, {self.number_or_percent(0, 19, -128)}]')
        if rand.randint(0, 3) == 0:
            keys.append('"state": ' + rand.choice(['["disabled"]', '["hidden"]', '["hidden", "disabled"]']))
        flags = [f for f in ("stretch", "rest", "repeat") if rand.randint(0, 4) == 0]
        if kind in ("box", "frame"):
            flags.append(rand.choice(["column", "row"]))
        # a scrolling box, by the rule, whose table has the look of the word "scroll": its offset named or its own
        scrolls = rule and kind == "box" and "column" in flags and rand.randint(0, 2) == 0
        if scrolls:
            flags.append("scroll")
            if rand.randint(0, 1):
                keys.append('"value": "' + rand.choice(names_of("value", rule)) + '"')
            if rand.randint(0, 2) == 0:
                keys.append(f'"step": {rand.randint(1, 9)}')
        if flags:
            keys.append('"flags": [' + ", ".join(f'"{f}"' for f in flags) + "]")
        if holds:
            keys.append(f'"pad": {self.number_or_percent(0, 4, 0)}')
            if rand.randint(0, 1):
                keys.append(f'"gap": {self.number_or_percent(0, 4, 0)}')
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
            on = rand.choice(names_of("on_event", rule))
            keys.append(f'"on_event": "{on}"')
            if on == "step":
                keys.append(f'"ctx": {rand.choice([rand.randint(-5, 5), -2147483648, 2147483647])}')
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
            keys = [k for k in keys if not k.startswith('"on_event"') and not k.startswith('"ctx"')] + ['"on_event": "saved"', '"ctx": 3']
        elif how == "page_ctx":  # the page action given a context that is no page's tree
            keys = [k for k in keys if not k.startswith('"on_event"') and not k.startswith('"ctx"')] + ['"on_event": "page"', '"ctx": "other"']
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


def chance_tree(seed, rule, wrong, name):
    """a tree of the seed, named by chance at a chance place among the top keys; one to be made wrong made again until
    the wrong widget is in it"""
    rand = random.Random(seed)
    while True:
        chance = Chance(rand, rule, wrong)
        tree = chance.widget(1)
        if chance.injected or not wrong:
            break
    top = ['"dgx-ui": 1', '"tree": ' + tree] if seed % 2 else ['"tree": ' + tree, '"dgx-ui": 1']
    place = rand.randint(0, 3)
    if place < 3:
        top.insert(place, f'"name": "{name}"')
    return "{" + ", ".join(top) + "}", chance.ids_used


# ---- the cases: every_names first, the one test_generate.c also adds with no room ----

cases = [("every_names", EVERY_NAMES, False, ["title", "slider", "less", "go"]), ("every_rule", EVERY_RULE, True, ["title", "slider", "less", "go"])]
for k in range(70):
    tree, used = chance_tree(1000 + k, False, k % 6 == 5, f"chance_names{k}")
    cases.append((f"chance_names{k}", tree, False, used))
for k in range(20):
    tree, used = chance_tree(2000 + k, True, False, f"chance_rule{k}")
    cases.append((f"chance_rule{k}", tree, True, used))
# the texts: in buffers of several sizes, one of a byte, the last widget's buffered; none buffered; none at all; a tree
# named otherwise than its prefix
cases += [
    ("buffers", '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "label", "text": "Hello", "text_size": 12}, '
                '{"kind": "input", "text_size": 1}, {"kind": "button", "text": "Go", "on_event": "saved"}, {"kind": "label", "text": "a\\nb", "lines": 2}, '
                '{"kind": "label", "text": "Last one", "text_size": 30, "style": "small"}]}}', False, []),
    ("constants", '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "label", "text": "a"}, {"kind": "button", "text": "b"}]}}',
     True, []),
    ("textless", '{"dgx-ui": 1, "tree": {"kind": "box", "children": [{"kind": "slider", "value": "volume", "max": 9}]}}', True, []),
    ("scrolling", '{"dgx-ui": 1, "tree": {"kind": "box", "flags": ["column"], "children": [{"kind": "box", "flags": ["column", "scroll"], "value": "volume", '
                  '"step": 3, "want": [0, 40], "children": [{"kind": "label", "text": "a"}, {"kind": "button", "text": "b"}]}, {"kind": "box", "flags": ["scroll", '
                  '"column"], "want": [0, 20], "children": [{"kind": "label", "text": "c"}]}, {"kind": "box", "flags": ["column", "scroll"], '
                  '"want": [0, 20], "children": [{"kind": "label", "text": "d"}]}]}}', True, []),
    ("over_name", '{"dgx-ui": 1, "name": "my-page", "tree": {"kind": "label", "text": "x", "id": "title"}}', True, ["title"]),
    # by the file of two tables: an id with no table passed over; a handler, a style, a look the file has not
    ("partial_slider", '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "volume", "max": 9, "id": "title"}}', False, []),
    ("partial_unknown_on", '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "volume", "on_event": "saved"}}', False, []),
    ("partial_no_style", '{"dgx-ui": 1, "tree": {"kind": "label", "text": "x"}}', False, []),
    ("partial_no_look", '{"dgx-ui": 1, "tree": {"kind": "button"}}', False, []),
]
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
    '{"dgx-ui": 1, "tree": {"kind": "box", "pad": 32768}}',
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
    '{"dgx-ui": 1, "tree": {"kind": "button", "on_event": "page", "ctx": 5}}',
    '{"dgx-ui": 1, "tree": {"kind": "button", "ctx": 5}}',
    '{"dgx-ui": 1, "tree": {"kind": "button", "ctx": 5, "on_event": "saved"}}',
    '{"dgx-ui": 1, "tree": {"kind": "switch", "picture": "pip", "value": "ticked", "look": "picture_button"}}',
    '{"dgx-ui": 1, "tree": {"kind": "radio", "value": "choice", "picture": "dot"}}',
    '{"dgx-ui": 1, "tree": {"kind": "icon", "picture": "dots", "value": "picked", "max": 2, "look": "picture"}}',
    '{"dgx-ui": 1, "tree": {"kind": "icon", "min": 3, "max": 4, "value": "picked", "picture": "dot", "look": "picture"}}',
    '{"dgx-ui": 1, "tree": {"kind": "box", "children": [{"kind": "label", "id": "go"}, {"kind": "box", "children": [{"kind": "label", "id": "go"}]}]}}',
    '{"dgx-ui": 1, "tree": {"kind": "slider", "look": "yours"}}',
    '{"dgx-ui": 1, "tree": ' + "".join('{"kind": "box", "children": [' for _ in range(24)) + '{"kind": "box"}' + "]}" * 24 + "}",
    '{"dgx-ui": 1, "tree": {"kind": "box", "x": ' + "[" * 60 + "]" * 60 + "}}",
    '{"dgx-ui": 1, "name": "", "tree": {"kind": "box"}}',
    '{"dgx-ui": 1, "name": "a/b", "tree": {"kind": "box"}}',
    '{"dgx-ui": 1, "name": null, "tree": {"kind": "box"}}',
    '{"dgx-ui": 1, "name": "a", "name": "b", "tree": {"kind": "box"}}',
    '{"name": "page", "fonts": 1, "dgx-ui": 2, "tree": {"kind": "box"}}',
    '{"dgx-ui": 1, "name": "my-page", "tree": {"kind": "boxx"}}',
]
for k, text in enumerate(REFUSED):
    cases.append((f"refused{k}", text, False, []))


def named(text):
    """the tree's name a description gives; None when it gives none or is not JSON"""
    try:
        top = json.loads(text)
    except ValueError:
        return None
    return top.get("name") if isinstance(top, dict) else None


def run_tool(name, text, rule, names=None, prefix=None):
    """dgx_ui_c on the text: the files named by the case, by -p unless the description names its tree so; prefix ""
    gives no -p, another prefix that one"""
    path = os.path.join(work, name + ".json")
    with open(path, "w", encoding="utf-8", errors="surrogateescape") as out:
        out.write(text)
    command = [tool, "-i", "generate_program.h", "-d", work]
    if not rule:
        command += ["-n", names or names_path]
    if prefix is None and named(text) != name:
        prefix = name
    if prefix:
        command += ["-p", prefix]
    run = subprocess.run(command + [path], capture_output=True, text=True, timeout=30)
    return run.returncode, run.stderr.strip()


LOADER_SAID = r": byte (-?\d+): (DGX_UI_LOAD_[A-Z_]+)(?: \"(.*)\")?$"


taken, refused = [], []
for name, text, rule, used in cases:
    status, said = run_tool(name, text, rule, partial_path if name in PARTIAL else None)
    if status == 0:
        taken.append((name, text, rule, used))
        continue
    found = re.search(LOADER_SAID, said)
    if rule or not found:
        failures.append(f"{name}: dgx_ui_c refused it: {said}")
        continue
    refused.append((name, text, codes.index(found.group(2)), int(found.group(1)), found.group(3) or ""))
    # the loader's verdict first: with no prefix given, whatever the name, the same
    bare_status, bare_said = run_tool(name + "_bare", text, rule, partial_path if name in PARTIAL else None, prefix="")
    bare = re.search(LOADER_SAID, bare_said)
    if not bare or bare.groups() != found.groups():
        failures.append(f"{name}: with no -p dgx_ui_c said {bare_said}, with -p {said}")
# the scrolling boxes of no place named: each its own place of the array, the array as long as they are many
with open(os.path.join(work, "scrolling.c")) as made:
    scrolling = made.read()
own = re.findall(r"&scrolling_offsets\[(\d+)\]", scrolling)
if sorted(own) != ["0", "1"] or "static int32_t          scrolling_offsets[2];" not in scrolling:
    failures.append(f"scrolling: the boxes' own offsets {own}")
if not any(name.startswith("refused") for name, *_ in refused) or sum(1 for name, *_ in refused if name.startswith("chance")) < 5:
    failures.append(f"too few refusals: {[name for name, *_ in refused]}")
if len([1 for name, *_ in taken if name.startswith("chance_names")]) < 50:
    failures.append("too few trees by chance taken")
if [name for name, *_ in taken if name in PARTIAL] != ["partial_slider"] or {name for name, *_ in refused if name in PARTIAL} != PARTIAL - {"partial_slider"}:
    failures.append("the cases of the file of two tables not taken and refused as they should be")


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
# the arrays of the texts and the buffers only when there is something in them; the tree's name said when it is not
# the prefix
for name, texts, buffers in [("buffers", True, True), ("constants", True, False), ("textless", False, False)]:
    made = made_text(name, ".c")
    if not made or (f"{name}_texts[" in made) != texts or (f"{name}_buffers[" in made) != buffers:
        failures.append(f"{name}: the arrays of the texts {texts}, of the buffers {buffers}, not as made")
if "buffers_buffers + 13, 30, 1, &small" not in made_text("buffers", ".c"):
    failures.append("buffers: the last buffer not where the sizes before it put it")
if 'its tree "my-page" named over_name in C' not in made_text("over_name", ".c").partition("*/")[0].replace("\n *", ""):
    failures.append("over_name: the first comment does not name the tree and its prefix")

# ---- the program ----
with open(os.path.join(work, "cases.h"), "w") as out:
    out.write("/* the cases of test_generate.py: made by it */\n#include \"program.h\"\n#include \"partial.h\"\n")
    for name, *_ in taken:
        out.write(f'#include "{name}.h"\n')
    for name, text, rule, used in taken:
        if rule:
            out.write(f"dgx_ui_text_style_t {name}_style;\n")
    out.write("static const generate_case_t cases[] = {\n")
    for name, text, rule, used in taken:
        rule_ids = ", ".join(f"&{name}_{i}" if rule and i in used else "NULL" for i in IDS)
        tables = "&partial_names" if name in PARTIAL else "NULL"
        out.write(f"    {{\"{name}\", {c_literal(text)}, {name}_add, {('&' + name + '_style') if rule else 'NULL'}, {{{rule_ids}}}, {tables}}},\n")
    out.write("};\nstatic const generate_refusal_t refusals[] = {\n")
    for name, text, *_ in refused:
        tables = "&partial_names" if name in PARTIAL else "NULL"
        out.write(f'    {{"{name}", {c_literal(text)}, {tables}}},\n')
    out.write("};\n")
program = os.path.join(work, "test_generate")
made_files = [os.path.join(work, name + ".c") for name, *_ in taken] + [os.path.join(work, "program.c"), os.path.join(work, "partial.c")]
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

# the made header read by C++
cxx_flags, take_next = [], False
for word in compile_command:
    if take_next or word.startswith("-I") or word.startswith("-D"):
        cxx_flags.append(word)
    take_next = word == "-include"
    if take_next:
        cxx_flags.append(word)
cxx = subprocess.run(["c++", "-x", "c++", "-fsyntax-only", "-Wall", "-Wextra", *cxx_flags, "-I" + work, os.path.join(work, "program.h")],
                     capture_output=True, text=True, timeout=120)
if cxx.returncode:
    failures.append(f"program.h read by C++: {cxx.stderr[:2000]}")

# ---- what dgx_ui_c alone refuses: a name the rule cannot make C, a names file that is wrong; one of one table ----
status, said = run_tool("dash", '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "my-volume"}}', True)
if status == 0 or 'the value "my-volume" is not a name of C' not in said or "byte 50" not in said:
    failures.append(f"a name with '-' by the rule: {status} {said}")
status, said = run_tool("dash_id", '{"dgx-ui": 1, "tree": {"kind": "label", "id": "a.b"}}', True)
if status == 0 or 'the id "a.b" is not a name of C' not in said:
    failures.append(f"an id with '.' by the rule: {status} {said}")
wrong_names = os.path.join(work, "wrong_names.txt")
for line, expect in [("colour red &red", 'no table "colour"'), ("value volume &volume 3", '"3" means nothing for a value'),
                     ("picture dots dots 0", "not a count of pictures"),
                     ("value vol-ume", 'the value "vol-ume" with no C expression: only a look of the library\'s and the actions back and page go without one'),
                     ("look mine", 'the look "mine" with no C expression'), ("on_event saved", 'the on_event "saved" with no C expression'), ("look", "a look with no name"),
                     ("value v &a + 1", "more than a table, a name, an expression and a word: an expression is written with no spaces"),
                     ("value v &a +", '"+" means nothing for a value: an expression is written with no spaces'),
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
if status == 0 or "DGX_UI_LOAD_NO_LOOK" not in said:
    failures.append(f"a names file of one table, no look in it: {status} {said}")
status, said = run_tool("one_table_unknown", '{"dgx-ui": 1, "tree": {"kind": "slider", "value": "volume"}}', False, one_table)
if status == 0 or 'DGX_UI_LOAD_UNKNOWN "volume"' not in said:
    failures.append(f"a name the file's table has not: {status} {said}")

# the limits of a table, at the line past them
many = os.path.join(work, "many.txt")
for lines, expect in [([f"value v{k} &volume" for k in range(65536)], ":65536: a value past the 65535 a table may have"),
                      ([f"id i{k} title_index" for k in range(257)], ":257: an id past the 256 a table may have")]:
    with open(many, "w") as out:
        out.write("\n".join(lines) + "\n")
    status, said = make_tables(many, "many")
    if status == 0 or many + expect not in said:
        failures.append(f"the limit {expect}: {status} {said[:300]}")
# -t's refusals
for options, expect in [(["-t", "tabled", os.path.join(work, "every_rule.json")], "-t makes the tables of a names file alone: no description with it"),
                        (["-n", names_path, "-p", "page", "-t", "tabled"], "-t names the tables' prefix: no -p with it"),
                        (["-t", "tabled"], "-t wants the names file, -n <names file>"),
                        (["-n", names_path, "-t", "1x"], '-t "1x" is not a name of C'),
                        (["-n", names_path, "-t", "dgx_x"], '-t "dgx_x" begins with dgx_')]:
    run = subprocess.run([tool, "-d", work, *options], capture_output=True, text=True, timeout=30)
    if run.returncode == 0 or expect not in run.stderr:
        failures.append(f"dgx_ui_c {' '.join(options)}: {run.returncode} {run.stderr.strip()}")
# -f: the files' name apart from the prefix, of letters, digits, '_', '-' and '.', neither '-' nor '.' first
named_page = os.path.join(work, "files_named.json")
with open(named_page, "w") as out:
    out.write('{"dgx-ui": 1, "name": "page", "tree": {"kind": "box"}}')
for files in ["-dash", ".dot", "a b", "a/b", ""]:
    run = subprocess.run([tool, "-d", work, "-f", files, named_page], capture_output=True, text=True, timeout=30)
    if run.returncode == 0 or f'-f "{files}" is not a name for the files: letters, digits, \'_\', \'-\' and \'.\', neither \'-\' nor \'.\' first' not in run.stderr:
        failures.append(f"dgx_ui_c -f {files!r}: {run.returncode} {run.stderr.strip()}")
run = subprocess.run([tool, "-d", work, "-f", "my-page.v2", named_page], capture_output=True, text=True, timeout=30)
if run.returncode or "int page_add(dgx_ui_t *ui, int parent);" not in made_text("my-page.v2", ".h"):
    failures.append(f"dgx_ui_c -f my-page.v2: {run.returncode} {run.stderr.strip()}")

# ---- editors' objects, at the top and in widgets, change nothing the tool makes: the same files byte for byte ----
EDITED = ('{"dgx-ui": 1, "editor": {"dgx-ui": {"zoom": 1.5, "screen": [800, 480], "time": 1763712000000}}, "name": "sound",'
          ' "tree": {"kind": "box", "flags": ["column"], "pad": 4, "editor": {"dgx-ui": {"note": "the page", "lock": true}},'
          ' "children": [{"kind": "label", "text": "Volume", "id": "title", "editor": {"dgx-ui": {"guide": [0.5, 12.25e1]}}},'
          ' {"kind": "slider", "max": 10, "value": "volume", "editor": {}}]}}')
PLAIN = ('{"dgx-ui": 1, "name": "sound", "tree": {"kind": "box", "flags": ["column"], "pad": 4,'
         ' "children": [{"kind": "label", "text": "Volume", "id": "title"}, {"kind": "slider", "max": 10, "value": "volume"}]}}')
made_of = {}
for kind, text in [("edited", EDITED), ("plain", PLAIN)]:
    directory = os.path.join(work, kind)
    os.makedirs(directory, exist_ok=True)
    for suffix in (".c", ".h"):
        if os.path.exists(os.path.join(directory, "sound" + suffix)):
            os.remove(os.path.join(directory, "sound" + suffix))
    with open(os.path.join(directory, "sound.json"), "w") as out:  # one name, so that the files' heads are the same
        out.write(text)
    run = subprocess.run([tool, "-i", "generate_program.h", "-n", names_path, "-d", directory, "sound.json"], capture_output=True, text=True, timeout=30, cwd=directory)
    made = []
    for suffix in (".c", ".h"):
        with open(os.path.join(directory, "sound" + suffix), encoding="utf-8") if run.returncode == 0 else open(os.devnull) as made_file:
            made.append(made_file.read())
    made_of[kind] = (run.returncode, run.stderr.strip(), made)
if made_of["edited"][0] or made_of["plain"][0] or made_of["edited"][2] != made_of["plain"][2]:
    failures.append(f"a description with editors made other files: {made_of['edited'][:2]} {made_of['plain'][:2]}")
# an editor's object before the tree as deep as the loader takes, 51 brackets open with the top's: the tool reads on to
# the tree and finds its names by the rule
deepest = '{"dgx-ui": 1, "name": "deepest", "editor": {"a": ' + "[" * 49 + "]" * 49 + '}, "tree": {"kind": "slider", "value": "volume"}}'
status, said = run_tool("deepest", deepest, True, prefix="")
if status or "&volume" not in made_text("deepest", ".c"):
    failures.append(f"an editor's object 51 brackets deep before the tree: {status} {said}")

# ---- the prefix: the tree's name, else -p; a name the rule makes that is the tool's own ----
for text, prefix, expect in [
    ('{"dgx-ui": 1, "tree": {"kind": "box"}}', "", 'has no "name": give the tree one there, or a prefix with -p'),
    ('{"dgx-ui": 1, "name": "my-page", "tree": {"kind": "box"}}', "", 'the name "my-page" is not a name of C: give the tree a prefix of C with -p'),
    ('{"dgx-ui": 1, "name": "2nd", "tree": {"kind": "box"}}', "", 'the name "2nd" is not a name of C'),
    ('{"dgx-ui": 1, "name": "page", "tree": {"kind": "box"}}', "1x", '-p "1x" is not a name of C'),
]:
    status, said = run_tool("prefix", text, True, prefix=prefix)
    if status == 0 or expect not in said:
        failures.append(f"the prefix of {text} by -p {prefix!r}: {status} {said}")
status, said = run_tool("named", '{"dgx-ui": 1, "name": "named", "tree": {"kind": "box"}}', True, prefix="")
if status or "int named_add(dgx_ui_t *ui, int parent);" not in made_text("named", ".h"):
    failures.append(f"the tree's name as the prefix: {status} {said}")
for word in ["add", "style", "widgets", "parents", "texts", "buffers"]:
    text = '{"dgx-ui": 1, "name": "page", "tree": {"kind": "label", "id": "' + word + '"}}'
    status, said = run_tool("made_id", text, True, prefix="")
    if status == 0 or f'byte {text.index(chr(34) + word)}: the id "{word}" would be page_{word}, a name dgx_ui_c makes' not in said:
        failures.append(f"the id {word}: {status} {said}")
USES = [("value", '{"kind": "slider", "value": "%s"}'), ("picture", '{"kind": "picture", "picture": "%s"}'),
        ("style", '{"kind": "label", "text": "x", "style": "%s"}'), ("on_event", '{"kind": "button", "on_event": "%s"}'), ("take", '{"kind": "box", "take": "%s"}'),
        ("ctx", '{"kind": "button", "on_event": "saved", "ctx": "%s"}'), ("look", '{"kind": "slider", "look": "%s"}')]
for array in ["widgets", "parents", "texts", "buffers"]:
    for table, use in USES:
        for lead in [f"page_{array}", f"page_{array}.x"] if table in ("value", "style", "ctx") else [f"page_{array}"]:
            text = '{"dgx-ui": 1, "name": "page", "tree": ' + use % lead + "}"
            status, said = run_tool("made_lead", text, True, prefix="")
            if status == 0 or f'byte {text.index(chr(34) + lead)}: the {table} "{lead}" leads with an array of page.c that dgx_ui_c makes' not in said:
                failures.append(f"the {table} {lead}: {status} {said}")
status, said = run_tool("made_lead", '{"dgx-ui": 1, "name": "page", "tree": {"kind": "slider", "value": "other_texts"}}', True, prefix="other")
if status == 0 or 'the value "other_texts" leads with an array of other.c' not in said:
    failures.append(f"an array of -p's prefix: {status} {said}")
for text in ['{"dgx-ui": 1, "name": "page", "tree": {"kind": "label", "text": "x", "id": "text1"}}',
             '{"dgx-ui": 1, "name": "page", "tree": {"kind": "slider", "value": "page_texts_x"}}',
             '{"dgx-ui": 1, "name": "page", "tree": {"kind": "slider", "value": "page_textsx"}}',
             '{"dgx-ui": 1, "name": "page", "tree": {"kind": "slider", "value": "other_texts"}}',
             '{"dgx-ui": 1, "name": "page", "tree": {"kind": "button", "on_event": "page_add", "ctx": "page_style"}}']:
    status, said = run_tool("near", text, True, prefix="")
    if status:
        failures.append(f"{text}: refused, not a name the tool makes: {said}")

# ---- no file of the program's written over; a file the tool made written again ----
def tool_in(directory, text, *options):
    path = os.path.join(directory, "page.json")
    with open(path, "w") as out:
        out.write(text)
    run = subprocess.run([tool, "-d", directory, *options, path], capture_output=True, text=True, timeout=30)
    return run.returncode, run.stderr.strip()


def file_text(path):
    with open(path, encoding="utf-8") as made:
        return made.read()


own = os.path.join(work, "own")
os.makedirs(own, exist_ok=True)
for leftover in ["main.c", "main.h", "page.c", "page.h", "again.c", "again.h", "other.c", "other.h", "edited.c", "edited.h"]:
    if os.path.exists(os.path.join(own, leftover)):
        os.remove(os.path.join(own, leftover))
PROGRAM_C = "int main(void)\n{\n    return 0;\n}\n"
PROGRAM_H = "/*\n * page.h: made by hand, the program's own, a description of format 1. Not to be edited: change the description and make it again.\n */\n"
REFUSED_OVER = ": there, and not made by dgx_ui_c: not written over; give the tree another prefix with -p, or the files another directory with -d"
with open(os.path.join(own, "main.c"), "w") as out:
    out.write(PROGRAM_C)
status, said = tool_in(own, '{"dgx-ui": 1, "name": "main", "tree": {"kind": "box"}}')
if status == 0 or os.path.join(own, "main.c") + REFUSED_OVER not in said or file_text(os.path.join(own, "main.c")) != PROGRAM_C or os.path.exists(os.path.join(own, "main.h")):
    failures.append(f"the program's main.c: {status} {said}")
with open(os.path.join(own, "page.h"), "w") as out:
    out.write(PROGRAM_H)
status, said = tool_in(own, '{"dgx-ui": 1, "name": "page", "tree": {"kind": "box"}}')
if status == 0 or os.path.join(own, "page.h") + REFUSED_OVER not in said or file_text(os.path.join(own, "page.h")) != PROGRAM_H or os.path.exists(os.path.join(own, "page.c")):
    failures.append(f"the program's page.h with no page.c: {status} {said}")
status, said = tool_in(own, '{"dgx-ui": 1, "name": "main", "tree": {"kind": "box"}}', "-p", "other")
if status or file_text(os.path.join(own, "main.c")) != PROGRAM_C or "int other_add(" not in file_text(os.path.join(own, "other.h")):
    failures.append(f"the program's main.c kept, the tree made other by -p: {status} {said}")
for pad in [1, 2]:
    status, said = tool_in(own, '{"dgx-ui": 1, "name": "again", "tree": {"kind": "box", "pad": %d}}' % pad)
    if status or f".pad = {pad}" not in file_text(os.path.join(own, "again.c")):
        failures.append(f"a file the tool made, written again with pad {pad}: {status} {said}")
# percents written as the description says them, DGX_UI_PERCENT() of its hundredths with their flags, and every
# hundredth of -128 % to 127.99 % given back by them: the tool's writing of each, compiled, against its value
cases_of_percent = [("33.33%", "DGX_UI_PERCENT(33.33)"), ("50%", "DGX_UI_PERCENT(50)"), ("2.5%", "DGX_UI_PERCENT(2.5)"),
                    ("-0.05%", "DGX_UI_PERCENT(-0.05)"), ("127.99%", "DGX_UI_PERCENT(127.99)"), ("-128%", "DGX_UI_PERCENT(-128)")]
status, said = tool_in(own, '{"dgx-ui": 1, "name": "percents", "tree": {"kind": "box", "flags": ["row"], "pad": "2.5%", "gap": 3, '
                            '"children": [{"kind": "box", "want": ["33.33%", "50%"], "at": ["-0.05%", "127.99%"]}, '
                            '{"kind": "box", "at": ["-128%", 0]}]}}')
made = file_text(os.path.join(own, "percents.c")) if status == 0 else ""
expected = [".flags = DGX_UI_ROW | DGX_UI_PERCENT_PAD, .pad = DGX_UI_PERCENT(2.5), .gap = 3",
            ".flags = DGX_UI_PERCENT_X | DGX_UI_PERCENT_Y | DGX_UI_PERCENT_W | DGX_UI_PERCENT_H, .at_x = DGX_UI_PERCENT(-0.05), "
            ".at_y = DGX_UI_PERCENT(127.99), .want_w = DGX_UI_PERCENT(33.33), .want_h = DGX_UI_PERCENT(50)",
            ".flags = DGX_UI_PERCENT_X, .at_x = DGX_UI_PERCENT(-128)"]
for line in expected:
    if line not in made:
        failures.append(f"percents not written as said: {line}: {status} {said}")
# every hundredth through the tool's writing of it, in one description of a column of boxes, and the C compiled
values = list(range(-12800, 12800))
compile_dir = os.path.join(work, "all_percents")
os.makedirs(compile_dir, exist_ok=True)
for chunk_at in range(0, len(values), 1600):
    chunk = values[chunk_at:chunk_at + 1600]
    as_text = lambda h: ("-" if h < 0 else "") + f"{abs(h) // 100}" + (f".{abs(h) % 100:02d}" if abs(h) % 100 else "") + "%"
    children = ", ".join(f'{{"kind": "box", "at": ["{as_text(h)}", 0]}}' for h in chunk)
    status, said = tool_in(compile_dir, '{"dgx-ui": 1, "name": "every", "tree": {"kind": "box", "children": [%s]}}' % children)
    made = file_text(os.path.join(compile_dir, "every.c")) if status == 0 else ""
    written = re.findall(r"\.at_x = (DGX_UI_PERCENT\([-0-9.]+\))", made)
    if status or len(written) != len(chunk) - (1 if 0 in chunk else 0):
        failures.append(f"the hundredths from {chunk[0]}: {status} {said[:200]}, {len(written)} written")
        break
    probe = os.path.join(compile_dir, "probe.c")
    with open(probe, "w") as out:
        out.write('#include <stdio.h>\n#include "dgx_ui.h"\nstatic const int16_t made[] = {%s};\n' % ", ".join(written))
        wanted = [(h * 256 + 50) // 100 if h >= 0 else -((-h * 256 + 50) // 100) for h in chunk if h]
        out.write('static const int16_t wanted[] = {%s};\n' % ", ".join(map(str, wanted)))
        out.write("int main(void)\n{\n    for (unsigned k = 0; k < sizeof made / sizeof *made; ++k)\n"
                  '        if (made[k] != wanted[k]) return printf("%u: %d, not %d\\n", k, made[k], wanted[k]), 1;\n    return 0;\n}\n')
    paths = [f for f in compile_command if f.startswith("-I") or f.startswith("-include") or f.endswith(".h")]
    built = subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-Werror", "-I", os.path.join(root, "include"), *paths,
                            "-o", os.path.join(compile_dir, "probe"), probe], capture_output=True, text=True, timeout=120)
    ran = subprocess.run([os.path.join(compile_dir, "probe")], capture_output=True, text=True, timeout=30) if built.returncode == 0 else built
    if built.returncode or ran.returncode:
        failures.append(f"the hundredths from {chunk[0]} through DGX_UI_PERCENT(): {(built.stderr + ran.stdout)[:300]}")
        break

# a file the tool made of the other kind: a tree's files for tables, the tables' for a tree
for leftover in ["kindtree.c", "kindtree.h", "kindtables.c", "kindtables.h"]:
    if os.path.exists(os.path.join(own, leftover)):
        os.remove(os.path.join(own, leftover))
status, said = tool_in(own, '{"dgx-ui": 1, "name": "kindtree", "tree": {"kind": "box"}}')
tree_c = file_text(os.path.join(own, "kindtree.c")) if status == 0 else ""
run = subprocess.run([tool, "-n", names_path, "-d", own, "-t", "kindtree"], capture_output=True, text=True, timeout=30)
if (status or run.returncode == 0 or "kindtree.c: made by dgx_ui_c of a description, not of a names file: not written over; give the tables another prefix with -t"
        not in run.stderr or file_text(os.path.join(own, "kindtree.c")) != tree_c):
    failures.append(f"tables over a tree's files: {status} {said} {run.returncode} {run.stderr.strip()}")
run = subprocess.run([tool, "-n", names_path, "-d", own, "-t", "kindtables"], capture_output=True, text=True, timeout=30)
again = subprocess.run([tool, "-n", names_path, "-d", own, "-t", "kindtables"], capture_output=True, text=True, timeout=30)
tables_c = file_text(os.path.join(own, "kindtables.c")) if run.returncode == 0 else ""
status, said = tool_in(own, '{"dgx-ui": 1, "name": "kindtables", "tree": {"kind": "box"}}')
if (run.returncode or again.returncode or status == 0 or "kindtables.c: made by dgx_ui_c of a names file, not of a description: not written over; give the tree another prefix with -p"
        not in said or file_text(os.path.join(own, "kindtables.c")) != tables_c):
    failures.append(f"a tree over the tables' files: {run.returncode} {again.returncode} {again.stderr.strip()} {status} {said}")
# the kind by the first of its words: a description whose file's name holds the tables' words is still a tree's
odd = os.path.join(own, "odd, a names file: Not to be edited: change the names file.json")
with open(odd, "w") as out:
    out.write('{"dgx-ui": 1, "name": "oddtree", "tree": {"kind": "box"}}')
runs = [subprocess.run([tool, "-d", own, odd], capture_output=True, text=True, timeout=30) for _ in range(2)]
if any(run.returncode for run in runs):
    failures.append(f"a tree made again from a file named with the tables' words: {runs[1].stderr.strip()}")
# a file that begins as the tool's but whose first comment says nothing of what it was made of: the program's
EDITED = "/*\n * edited.c: made by dgx_ui_c from page.json, then by hand\n */\nint edited;\n"
with open(os.path.join(own, "edited.c"), "w") as out:
    out.write(EDITED)
status, said = tool_in(own, '{"dgx-ui": 1, "name": "edited", "tree": {"kind": "box"}}')
if status == 0 or os.path.join(own, "edited.c") + REFUSED_OVER not in said or file_text(os.path.join(own, "edited.c")) != EDITED:
    failures.append(f"a file begun as the tool's, then by hand: {status} {said}")
# a prefix of the libraries': the made files would stand for their headers
status, said = tool_in(own, '{"dgx-ui": 1, "name": "dgx_ui", "tree": {"kind": "box"}}')
if status == 0 or 'the name "dgx_ui" begins with dgx_, as the libraries\' names and headers do: give the tree a prefix with -p' not in said:
    failures.append(f"a tree named dgx_ui: {status} {said}")
status, said = tool_in(own, '{"dgx-ui": 1, "name": "page", "tree": {"kind": "box"}}', "-p", "dgx_ui_text")
if status == 0 or '-p "dgx_ui_text" begins with dgx_' not in said:
    failures.append(f"-p dgx_ui_text: {status} {said}")

# the library's action command: by the rule its ctx a number cast to a pointer; by a names file "command" alone, its
# handler's entry of a number alone (the loader refuses a context of the program's for it); test_command.c runs it
commands = os.path.join(work, "commands")
os.makedirs(commands, exist_ok=True)
status, said = tool_in(commands, '{"dgx-ui": 1, "name": "cmd", "tree": {"kind": "button", "text": "Go", "on_event": "command", "ctx": 7}}')
if status != 0 or ".on_event = dgx_ui_action_command, .ctx = (void *)(intptr_t)7" not in file_text(os.path.join(commands, "cmd.c")):
    failures.append(f"a button of the action command by the rule: {status} {said}")
with open(os.path.join(commands, "names.txt"), "w") as out:
    out.write("on_event command\n")
run = subprocess.run([tool, "-d", commands, "-n", os.path.join(commands, "names.txt"), "-t", "cmdt"], capture_output=True, text=True, timeout=30)
if run.returncode != 0 or '{"command", dgx_ui_action_command, DGX_UI_CTX_NUMBER_ONLY}' not in file_text(os.path.join(commands, "cmdt.c")):
    failures.append(f"the action command in a names file's tables: {run.returncode} {run.stderr.strip()}")

for failure in failures:
    print("  " + failure, file=sys.stderr)
print(f"test_generate.py: {len(taken)} taken, {len(refused)} refused alike" if not failures else f"test_generate.py: {len(failures)} failed")
sys.exit(1 if failures else 0)
