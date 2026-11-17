#!/usr/bin/env python3
"""Checks of the documentation of dgx-ui. Run: python3 test/docs/check_docs.py [DGX [DGX-DI [build dir]]]

 the C code of the pages compiles against the headers, every dgx_ui name a
page prints is in a header, every public name of the headers is in the API page, and the links between pages lead
to files that are there."""
import os, re, subprocess, sys
here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.normpath(os.path.join(here, '..', '..'))
dgx = sys.argv[1] if len(sys.argv) > 1 else os.path.join(repo, '..', 'esp32-component-dgx')
dgx_di = sys.argv[2] if len(sys.argv) > 2 else os.path.join(repo, '..', 'esp32-component-dgx-di')
build = sys.argv[3] if len(sys.argv) > 3 else os.path.join(here, 'build')
os.makedirs(build, exist_ok=True)
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
        cmd = ['cc', '-std=gnu11', '-fsyntax-only', '-Wall', '-Wextra', '-Wno-unused-parameter', '-Wno-unused-function', '-Wno-unused-variable',
               '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers', '-Werror=implicit-function-declaration', '-Werror=incompatible-pointer-types',
               '-DDGX_HOST_WINDOW', f'-I{here}/stubs', f'-I{repo}/include', f'-I{dgx}/include', f'-I{dgx}/host/stubs', f'-I{dgx}/host/window', f'-I{dgx_di}/include',
               '-include', f'{dgx}/host/stubs/esp_log.h', out]
        r = subprocess.run(cmd, capture_output=True, text=True)
        msgs = [l for l in r.stderr.splitlines() if 'error' in l or 'warning' in l]
        print(f'{"ok  " if r.returncode == 0 and not msgs else "FAIL"} {rel}: {len(blocks)} blocks of C' + ''.join('\n     ' + m[-200:] for m in msgs[:12]))
        ok &= r.returncode == 0 and not msgs
    # 2. the names
    plain = re.sub(r'\(#[^)]*\)', '()', text)  # anchors of links are not names
    used = set(re.findall(r'\b(?:dgx_ui_[a-z0-9_]+|DGX_UI_[A-Z0-9_]+)\b', plain))
    unknown = sorted(n for n in used - known if not n.endswith('_') and n not in ('dgx_ui_', 'DGX_UI_'))
    if unknown:
        print(f'     names not in the headers: {", ".join(unknown)}'); ok = False
    # 3. the links to files
    for target in re.findall(r'\]\(([^)#\s]+)(?:#[^)]*)?\)', text):
        if re.match(r'https?:', target): continue
        if not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(path), target))):
            print(f'     link to a file that is not there: {target}'); ok = False
    # 4. the API page names everything public
    if kind == 'api':
        public = set(re.findall(r'^(?:[a-z_ ]+[ \*])?(dgx_ui_[a-z0-9_]+)\(', headers, re.M)) | set(re.findall(r'\b(DGX_UI_[A-Z0-9_]+)\b', headers)) \
                 | set(re.findall(r'\}\s*(dgx_ui_[a-z0-9_]+_t);', headers)) | set(re.findall(r'extern const dgx_ui_look_t ([^;]+);', headers) and
                   [n.strip() for group in re.findall(r'extern const dgx_ui_look_t ([^;]+);', headers) for n in group.split(',')])
        missing = sorted(n for n in public - used if n not in ('DGX_UI_STATIC_ASSERT', 'DGX_UI_DI_ASSERT') and not n.endswith('_'))
        print(f'{"ok  " if not missing else "FAIL"} {rel}: {len(public)} public names' + (f'; missing: {", ".join(missing)}' if missing else ''))
        ok &= not missing
print('ALL OK' if ok else 'SOME CHECKS FAILED')
sys.exit(0 if ok else 1)
