#!/usr/bin/env python3
"""Checks of the documentation of dgx-ui. Run: python3 test/docs/check_docs.py [DGX [DGX-DI [build dir]]]

 the C code of the pages compiles against the headers, every dgx_ui name a
page prints is in a header or in project_include.cmake, every public name of the headers and every call of
project_include.cmake is in the API page, the links between pages lead to files that are there and to headings that
are there, and a number a page writes beside the name of a constant is the header's value. The files the cookbook
prints for its recipe of a description made C (the description, the names file, the program's header) are made C by
dgx_ui_c itself, built here by tools/dgx_ui_c.py, and what it makes is compiled beside the page's code. A struct a page
lists is the header's, member by member; the tables of the headers name every header; the cookbook's list is its
recipes; a size a page marks is the one the headers assert for a 32-bit chip; a version a page names is the
manifests'."""
import os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.normpath(os.path.join(here, '..', '..'))
dgx = sys.argv[1] if len(sys.argv) > 1 else os.path.join(repo, '..', 'esp32-component-dgx')
dgx_di = sys.argv[2] if len(sys.argv) > 2 else os.path.join(repo, '..', 'esp32-component-dgx-di')
build = os.path.abspath(sys.argv[3] if len(sys.argv) > 3 else os.path.join(here, 'build'))  # the tool is run from other directories
os.makedirs(build, exist_ok=True)
# the fonts the cookbook's code names, made by DGX of fonts.needs as a program's build makes its needs: dgx_fonts.h and
# dgx_fonts.c in <build>/fonts, which the pages' code and the runner of the recipes are compiled with
fonts = os.path.join(build, 'fonts')
made_fonts = subprocess.run([sys.executable, os.path.join(dgx, 'font2c', 'dgx_fonts.py'), 'build', os.path.join(here, 'fonts.needs'), fonts],
                            capture_output=True, text=True)
if made_fonts.returncode:
    sys.exit('the fonts of fonts.needs were not made by DGX: ' + (made_fonts.stdout + made_fonts.stderr)[-2000:])
PRELUDE = {
    'README': '#include "dgx_ui_paint.h"\n#include "dgx_ui_post.h"\n#include "dgx_ui_text.h"\n#include "esp_timer.h"\n#include "freertos/task.h"\nint read_co2(void);\n',
    'getting-started': '',
    'how-it-works': '#include "dgx_ui_paint.h"\n#include "dgx_ui_post.h"\n#include "dgx_ui_text.h"\n#include "dgx_ui_ticker.h"\n#include "esp_timer.h"\n#include "freertos/task.h"\n',
    'cookbook': None,  # the page's first block is its includes
}
pages = [('README', 'README.md'), ('README', 'README-ru.md')]
for lang in ('en', 'ru'):
    for name in ('getting-started', 'how-it-works', 'cookbook', 'api'):
        pages.append((name, f'docs/{lang}/{name}.md'))
headers = ''.join(open(os.path.join(repo, 'include', h)).read() for h in sorted(os.listdir(os.path.join(repo, 'include'))))
known = set(re.findall(r'\b(?:dgx_ui_[a-z0-9_]+|DGX_UI_[A-Z0-9_]+)\b', headers))
# the calls of project_include.cmake, the tool they run and the directory it writes to are names a page may print too
cmake = open(os.path.join(repo, 'project_include.cmake')).read()
known |= set(re.findall(r'\bdgx_ui_[a-z0-9_]+\b', cmake))
cmake_calls = set(re.findall(r'^function\((dgx_ui_[a-z0-9_]+)', cmake, re.M))
# the programs and scripts of tools/, by their names without the extension
known |= {os.path.splitext(name)[0] for name in os.listdir(os.path.join(repo, 'tools')) if name.startswith('dgx_ui_')}
CC = ['cc', '-std=gnu11', '-fsyntax-only', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-unused-function', '-Wno-unused-variable',
      '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Werror=implicit-function-declaration', '-Werror=incompatible-pointer-types',
      '-DDGX_HOST_WINDOW', f'-I{here}/stubs', f'-I{repo}/include', f'-I{dgx}/include', f'-I{dgx}/host/stubs', f'-I{dgx}/host/window', f'-I{dgx_di}/include', f'-I{fonts}',
      '-include', f'{dgx}/host/stubs/esp_log.h']
MADE_FILES = {'sound.json': r'^```json\n(\{"dgx-ui": 1, "name": "sound".*?)^```', 'vocab.txt': r'^```text\n(# table.*?)^```',
              'program.h': r'^```c\n(/\* program\.h: .*?)^```', 'day.json': r'^```json\n(\{"dgx-ui": 1, "name": "day".*?)^```'}
# what the recipe of an appearance adds to two of them
MORE = {'vocab.txt': r'^```text\n(# an appearance.*?)^```', 'program.h': r'^```c\n(/\* program\.h, one line more.*?)^```'}


def structs_of(text):
    """name -> (members, shortened) of every struct a text defines: a declaration a member, its spaces made one and
    none beside *, the comments out. Shortened: a comment stands after the last member, so the listing is of the first
    members only, as "the rest is the library's" says."""
    blank = re.sub(r'/\*.*?\*/|//[^\n]*', lambda comment: re.sub(r'\S', ' ', comment.group()), text, flags=re.S)
    found = {}
    for m in re.finditer(r'\bstruct\s+(\w+)\s*\{', blank):
        depth, end = 1, m.end()
        while depth and end < len(blank):
            depth += {'{': 1, '}': -1}.get(blank[end], 0)
            end += 1
        body, members, member, inner = blank[m.end():end - 1], [], '', 0
        for ch in body:
            inner += (ch in '{(') - (ch in '})')
            if ch == ';' and not inner:
                members.append(re.sub(r'\s*\*\s*', '*', ' '.join(member.split())))
                member = ''
            else:
                member += ch
        shortened = bool(re.search(r'/\*|//', text[m.end() + body.rfind(';') + 1:end - 1]))
        found.setdefault(m.group(1), ([x for x in members if x], shortened))
    return found


def slug(heading):
    """The anchor a heading gets on GitHub: small letters, what is no letter, digit, space, - or _ dropped, - for a space."""
    return re.sub(r'[^\w\- ]', '', heading.strip().lower()).replace(' ', '-')


def anchors_of(path, seen={}):
    """The anchors of a page's headings, the code blocks left out; a heading said again takes -1, -2 after it."""
    if path not in seen:
        text = re.sub(r'^```.*?^```', '', open(path).read(), flags=re.S | re.M)
        anchors, count = set(), {}
        for heading in re.findall(r'^#{1,6} (.+)$', text, re.M):
            base = slug(heading)
            anchors.add(base if base not in count else f'{base}-{count[base]}')
            count[base] = count.get(base, 0) + 1
        seen[path] = anchors
    return seen[path]


def links_wrong(path, text):
    """What is wrong with the links of a page to files of the repository and to their headings, a line each."""
    said = []
    for target, anchor in re.findall(r'\]\(([^)#\s]*)(?:#([^)\s]*))?\)', text):
        if re.match(r'https?:|mailto:', target): continue
        file = os.path.normpath(os.path.join(os.path.dirname(path), target)) if target else path
        if not os.path.exists(file):
            said.append(f'link to a file that is not there: {target}')
        elif anchor and file.endswith('.md') and anchor not in anchors_of(file):
            said.append(f'link to a heading that is not there: {target}#{anchor}')
    return said


# the members of every struct the headers define, for a listing of a page to be held to
struct_members = {name: members for name, (members, _) in structs_of(headers).items()}
include_headers = sorted(h for h in os.listdir(os.path.join(repo, 'include')) if h.endswith('.h'))
# the sizes the headers assert for a 32-bit chip, _Static_assert(sizeof(dgx_ui_node_t) == 28, ...)
sizes = {name: int(number) for name, number in re.findall(r'_Static_assert\(sizeof\((\w+)\) == (\d+)', headers)}
# a size a page marks: "28 bytes<!-- sizeof dgx_ui_node_t -->", or "1792 bytes<!-- sizeof dgx_ui_node_t * DGX_UI_BLOCK -->"
FACTOR = r'(?:sizeof \w+|DGX_UI_[A-Z0-9_]+|\d+)'
SIZE = rf'(\d+)(?:\s*(?:bytes?|байта?))?\s*<!--\s*({FACTOR}(?:\s*\*\s*{FACTOR})*)\s*-->'
# the versions the manifests ask for: dgx-ui's own, the least DGX, the least DGX-DI of the examples
manifest = open(os.path.join(repo, 'idf_component.yml')).read()
examples = ''.join(open(os.path.join(repo, 'examples', e, 'main', 'idf_component.yml')).read()
                   for e in sorted(os.listdir(os.path.join(repo, 'examples'))) if os.path.exists(os.path.join(repo, 'examples', e, 'main', 'idf_component.yml')))
VERSIONS = [  # what a page writes, the version it names, what the manifests ask for
    ('dgx-ui', r'(?:Version|Версия) (\d+\.\d+\.\d+)', re.search(r'^version:\s*"([^"]+)"', manifest, re.M).group(1)),
    ('dgx-ui', r'^\s+version: "(\d+\.\d+\.\d+)"', re.search(r'^version:\s*"([^"]+)"', manifest, re.M).group(1)),
    ('DGX', r'DGX (\d+\.\d+\.\d+) (?:or a later|или новее)', re.search(r'jef-sure/dgx:\s*\n\s*version:\s*"\^?([\d.]+)"', manifest).group(1)),
    ('DGX-DI', r'`jef-sure/dgx-di` (?:версии )?(\d+\.\d+\.\d+)', re.search(r'jef-sure/dgx-di:\s*"\^?([\d.]+)"', examples).group(1)),
]


def made_of(text, rel):
    """The files a cookbook prints for its recipe of a description made C, written into a directory of the page's
    language and made C there by the tool: sound.c and sound.h of the description, vocab.c and vocab.h of the names
    file. Returns the directory and what went wrong, a list of lines."""
    where = os.path.join(build, 'made_' + rel.split('/')[1])
    os.makedirs(where, exist_ok=True)
    for name, pattern in MADE_FILES.items():
        found = re.findall(pattern, text, re.S | re.M)
        if len(found) != 1:
            return where, [f'the recipe of a description made C prints {name} {len(found)} times, not once']
        open(os.path.join(where, name), 'w').write(found[0])
    for name, pattern in MORE.items():
        more = re.findall(pattern, text, re.S | re.M)
        if len(more) != 1:
            return where, [f'the recipe of an appearance made C prints its lines of {name} {len(more)} times, not once']
        open(os.path.join(where, name), 'a').write(more[0])
    tool_dir = os.path.join(build, 'tool')
    built = subprocess.run([sys.executable, os.path.join(repo, 'tools', 'dgx_ui_c.py'), dgx, tool_dir], capture_output=True, text=True)
    if built.returncode:
        return where, built.stderr.splitlines()[:6]
    tool = os.path.join(tool_dir, 'dgx_ui_c')
    said = []
    # as dgx_ui_tree(), dgx_ui_names() and dgx_ui_appearance() run it; the page of the recipe made a second time, on the
    # appearance, under a prefix of its own, sound_day, so that both pages lie in one directory
    for arguments in (['-f', 'sound', 'sound.json'], ['-t', 'vocab'], ['-f', 'day', '-A', 'day.json'],
                      ['-p', 'sound_day', '-f', 'sound_day', '-a', 'day.json', '-an', 'vocab.txt', '-af', 'day', 'sound.json']):
        run = subprocess.run([tool, '-n', 'vocab.txt', '-i', 'program.h', '-d', '.'] + arguments, capture_output=True, text=True, cwd=where)
        said += run.stderr.splitlines()[:4] if run.returncode else []
    if not said:
        for made in ('sound.c', 'vocab.c', 'day.c', 'sound_day.c'):
            run = subprocess.run(CC + [f'-I{where}', os.path.join(where, made)], capture_output=True, text=True)
            said += [line for line in run.stderr.splitlines() if 'error' in line or 'warning' in line][:4]
    return where, said
# the constants that are a plain number, DGX_UI_BLOCK = 64; a flag, 1u << 3, is not one
values = {name: int(number, 0) for name, number in re.findall(r'\b(DGX_UI_[A-Z0-9_]+)\s*=\s*(0x[0-9a-fA-F]+|\d+)\b(?!u?\s*<<)', headers)}
values.update({name: int(number, 0)
               for name, number in re.findall(r'^\s*#define\s+(DGX_UI_[A-Z0-9_]+)\s+\(?(0x[0-9a-fA-F]+|\d+)u?\)?\s*(?:/[/*].*)?$', headers, re.M)})
BESIDE = r'`(DGX_UI_[A-Z0-9_]+)`(?:\s*(?:,|=|:|\||is|of|—|–|равно|равен|это|то есть|, то есть)\s*|\s+)(0x[0-9a-fA-F]+|\d+)\b'
ok = True
for kind, rel in pages:
    path = os.path.join(repo, rel)
    if not os.path.exists(path):
        print(f'-- {rel}: not written yet'); continue
    text = open(path).read()
    # 1. the code
    if kind != 'api':
        blocks = re.findall(r'^```c\n(.*?)^```', text, re.S | re.M)
        src = (PRELUDE[kind] or '') + '\n'.join(blocks)
        out = os.path.join(build, rel.replace('/', '_') + '.c')
        open(out, 'w').write(src)
        made_dir, made_said = made_of(text, rel) if kind == 'cookbook' else (None, [])
        r = subprocess.run(CC + ([f'-I{made_dir}'] if made_dir else []) + [out], capture_output=True, text=True)
        msgs = ['dgx_ui_c: ' + line for line in made_said] + [l for l in r.stderr.splitlines() if 'error' in l or 'warning' in l]
        print(f'{"ok  " if r.returncode == 0 and not msgs else "FAIL"} {rel}: {len(blocks)} blocks of C' + ''.join('\n     ' + m[-200:] for m in msgs[:12]))
        ok &= r.returncode == 0 and not msgs
    # 2. the names
    plain = re.sub(r'\(#[^)]*\)', '()', text)  # anchors of links are not names
    used = set(re.findall(r'\b(?:dgx_ui_[a-z0-9_]+|DGX_UI_[A-Z0-9_]+)\b', plain))
    unknown = sorted(n for n in used - known if not n.endswith('_') and n not in ('dgx_ui_', 'DGX_UI_'))
    if unknown:
        print(f'     names not in the headers: {", ".join(unknown)}'); ok = False
    # 3. the links to files and to their headings
    for said in links_wrong(path, text):
        print(f'     {said}'); ok = False
    # 4. a number beside the name of a constant: `DGX_UI_POPUP_DEPTH`, 8
    for match in re.finditer(BESIDE, text):
        name, number = match.group(1), int(match.group(2), 0)
        if name in values and number != values[name]:
            line = text.count('\n', 0, match.start()) + 1
            print(f'     line {line}: {name} is {values[name]} in the header, the page says {number}'); ok = False
    # 5. the API page names everything public
    if kind == 'api':
        public = cmake_calls | set(re.findall(r'^(?:[a-z_0-9 ]+[ \*])?(dgx_ui_[a-z0-9_]+)\(', headers, re.M)) | set(re.findall(r'\b(DGX_UI_[A-Z0-9_]+)\b', headers)) \
                 | set(re.findall(r'\}\s*(dgx_ui_[a-z0-9_]+_t);', headers)) | set(re.findall(r'extern const dgx_ui_look_t ([^;]+);', headers) and
                   [n.strip() for group in re.findall(r'extern const dgx_ui_look_t ([^;]+);', headers) for n in group.split(',')])
        missing = sorted(n for n in public - used if n not in ('DGX_UI_STATIC_ASSERT', 'DGX_UI_DI_ASSERT') and not n.endswith('_'))
        print(f'{"ok  " if not missing else "FAIL"} {rel}: {len(public)} public names' + (f'; missing: {", ".join(missing)}' if missing else ''))
        ok &= not missing
    # 6. a struct a page lists is the header's, member by member; a listing whose last member is followed by a comment
    #    of its own lists the first members only
    listed_structs = 0
    for block in re.findall(r'^```c\n(.*?)^```', text, re.S | re.M):
        for name, (listed, shortened) in structs_of(block).items():
            if name not in struct_members: continue
            listed_structs += 1
            real = struct_members[name]
            if listed != (real[:len(listed)] if shortened else real):
                k = next((k for k, (a, b) in enumerate(zip(listed, real)) if a != b), min(len(listed), len(real)))
                print(f'     the listing of {name} differs from the header at its member {k + 1}: the page '
                      f'"{listed[k] if k < len(listed) else "nothing"}", the header "{real[k] if k < len(real) else "nothing"}"'); ok = False
    # 7. the table of the headers names every header of include/
    if kind in ('how-it-works', 'api'):
        missing = [h for h in include_headers if h not in re.findall(r'^\|\s*\[?`(dgx_ui[a-z_]*\.h)`', text, re.M)]
        if missing:
            print(f'     the table of the headers has not: {", ".join(missing)}'); ok = False
    # 8. the cookbook's list at the top is its recipes, in their order, each leading to its own heading
    if kind == 'cookbook':
        listed = re.findall(r'^- \[(.+?)\]\(#([^)]+)\)', text, re.M)
        recipes = re.findall(r'^## (.+)$', re.sub(r'^```.*?^```', '', text, flags=re.S | re.M), re.M)
        for title in [t for t in recipes if t not in [t for t, _ in listed]]:
            print(f'     the list at the top has not the recipe "{title}"'); ok = False
        for title in [t for t, _ in listed if t not in recipes]:
            print(f'     the list at the top names "{title}", which is no recipe'); ok = False
        if [t for t, _ in listed if t in recipes] != [t for t in recipes if t in [t for t, _ in listed]]:
            print('     the list at the top is not in the order of the recipes'); ok = False
        for title, anchor in listed:
            if anchor != slug(title):
                print(f'     "{title}" of the list leads to #{anchor}, its heading is #{slug(title)}'); ok = False
    # 9. a size a page marks, "28 bytes<!-- sizeof dgx_ui_node_t -->", is the one the headers assert for a 32-bit chip
    marked = 0
    for match in re.finditer(SIZE, text):
        marked += 1
        number, said, value = int(match.group(1)), match.group(2), 1
        for factor in (f.strip() for f in said.split('*')):
            got = sizes.get(factor[7:]) if factor.startswith('sizeof ') else values.get(factor) if factor.startswith('DGX_UI_') else int(factor)
            value = None if got is None or value is None else value * got
        line = text.count('\n', 0, match.start()) + 1
        if value is None:
            print(f'     line {line}: {said}: no size the headers assert, or no constant'); ok = False
        elif number != value:
            print(f'     line {line}: {said} is {value}, the page says {number}'); ok = False
    # 10. a version a page names is the one the manifests ask for
    for what, pattern, wanted in VERSIONS:
        for match in re.finditer(pattern, text, re.M):
            if match.group(1) != wanted:
                line = text.count('\n', 0, match.start()) + 1
                print(f'     line {line}: {what} {match.group(1)}, the manifest asks for {wanted}'); ok = False
    if listed_structs or marked:
        print(f'     {listed_structs} structs listed, {marked} sizes marked')
# the pages of the tools, which have no code of the program: their links
for rel in ('editor/README.md', 'editor/preview/README.md', 'tools/README.md', 'test/docs/README.md'):
    path = os.path.join(repo, rel)
    said = links_wrong(path, open(path).read())
    print(f'{"ok  " if not said else "FAIL"} {rel}: links' + ''.join('\n     ' + s for s in said))
    ok &= not said
print('ALL OK' if ok else 'SOME CHECKS FAILED')
sys.exit(0 if ok else 1)
