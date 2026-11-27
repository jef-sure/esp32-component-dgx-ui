#!/usr/bin/env python3
"""
test_serve.py: tools/dgx_ui_serve.py on a project in a temporary directory. The builder's files served, nothing else
of the component, each under the Content-Security-Policy that lets a page call nothing but the server; the project
listed, read and written by the token of the link; each refusal: another Host, for a file of the page too, no token or
another, a write from another page, a path with "..", from the root, of a backslash, a link leading out, into a folder
not there, a body too large or cut short; no answer to a question of another origin and no header that keeps the page
out of a frame. What is read: the CMakeLists, CMakePresets.json, the files the calls name, the pages and appearances
there, the fonts DGX writes, the board's entry, of a build's sdkconfig the panel's lines alone; never sdkconfig, a file
of a dot (.env, .git/config), dependencies.lock, a source, a .json of the program's. What is written: the files the
project's calls of dgx-ui name, read anew at each write, and pages and appearances already there; never a file or
folder of a dot, a file the build reads by its name (CMakeLists.txt, CMakeCache.txt, CMake*.json, sdkconfig*) in any
case of letters, nor anything under build/, managed_components/ or a folder of a build, even when a call names it. The
builder's own reading of the calls (store.js, under node) gives no file the server's leaves out. A write through a link
within the project writes the file it leads to and keeps a file's mode. A link of the page's folder leading out of it,
to a folder beside it too, not followed. The project's own copy of dgx-ui, of the registry or of git, gives its page,
nothing of it run; a copy out of managed_components/, of another API, of a link out or not as its checksums say, the
server's own page; a fixed port, and another when it is taken.
"""
import hashlib
import http.client
import importlib.util
import json
import os
import pathlib
import re
import shutil
import socket
import stat
import subprocess
import sys
import tempfile
import time

here = pathlib.Path(__file__).resolve().parent
SERVER = here.parent.parent / "tools" / "dgx_ui_serve.py"
failures = []
checks = 0
started = []  # every server started: stopped at the end, whatever stopped the test


def check(held, what):
    global checks
    checks += 1
    if not held:
        failures.append(what)
    return held


def start(project, *options, env=None, server=SERVER):
    """The server on a project: its process, the port and token of the link it prints first, and all it printed before."""
    process = subprocess.Popen([sys.executable, str(server), "--project", str(project), *options], stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, text=True, env=env)
    started.append(process)
    said, link = [], None
    for _ in range(10):  # to the last line it prints at the start
        line = process.stdout.readline()
        if not line:
            break
        said.append(line.rstrip("\n"))
        link = link or re.match(r"http://127\.0\.0\.1:(\d+)/editor/#project=([\w-]+)$", said[-1])
        if said[-1] == "Ctrl-C stops the server.":
            break
    if link:
        return process, int(link.group(1)), link.group(2), said
    process.kill()
    return process, None, None, said


def server_module():
    """The server's own functions, its file imported as a module."""
    spec = importlib.util.spec_from_file_location("dgx_ui_serve", SERVER)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


# the builder's reading of a CMakeLists, store.js's cmakeCalls() in a context as the page's (the scripts of index.html
# but app.js), on the files given: the paths of each call, its file, its names and its appearance
STORE_CALLS = r"""
const fs = require('fs'), path = require('path'), vm = require('vm');
const [editor, root, ...lists] = process.argv.slice(1);
const sandbox = { console, TextEncoder, TextDecoder, WebAssembly, atob, setTimeout, clearTimeout, queueMicrotask, Promise, URL,
                  navigator: { userAgent: '' } };
sandbox.window = sandbox;
vm.createContext(sandbox);
for (const [, script] of fs.readFileSync(path.join(editor, 'index.html'), 'utf8').matchAll(/<script src="([^"]+)">/g)) {
    if (script === 'app.js' || script === 'start.js') continue;
    const exported = { 'vendor/vue.global.prod.js': 'Vue', 'vendor/pinia.iife.prod.js': 'Pinia' }[script];
    vm.runInContext(fs.readFileSync(path.join(editor, script), 'utf8') + (exported ? `\n;window.${exported} = ${exported};` : ''), sandbox);
}
const paths = [];
for (const list of lists) {
    const said = vm.runInContext(`dgxUiBuilderStore.cmakeCalls(${JSON.stringify(list)}, ${JSON.stringify(fs.readFileSync(path.join(root, list), 'utf8'))})`, sandbox);
    for (const call of said.calls) paths.push(...[call.file, call.names, call.appearance].filter(Boolean));
}
console.log(JSON.stringify(paths));
"""


def store_paths(root, lists):
    done = subprocess.run(["node", "-e", STORE_CALLS, str(SERVER.parent.parent / "editor"), str(root), *lists], capture_output=True,
                          text=True, timeout=60)
    return json.loads(done.stdout) if done.returncode == 0 else None


def stop(process):
    process.kill()
    process.wait(timeout=10)
    process.stdout.close()


work = pathlib.Path(tempfile.mkdtemp(prefix="dgx-ui-serve-"))
try:
    project, outside = work / "demo", work / "outside"
    for folder in (project / "main", project / "build", project / "managed_components" / "dgx" / "include", outside / "deep", project / "pages",
                   project / ".vscode", project / ".claude", project / "build-cyd", project / "build-x", project / "Build", project / "components" / "x"):
        folder.mkdir(parents=True)
    (outside / "secret.txt").write_text("not the project's\n")
    beside = work / "demo-beside"  # its name begins as the project's: out of it all the same
    beside.mkdir()
    (beside / "secret.json").write_text("{}\n")
    (outside / "deep" / "secret.json").write_text("{}\n")
    # calls of dgx-ui, some naming what is never written
    top_list = ("project(demo)\ndgx_ui_tree(.vscode/tasks.json)\ndgx_ui_tree(CMakePresets.json)\ndgx_ui_tree(Build/page.json)\n"
                "dgx_ui_tree(build-cyd/page.json)\ndgx_ui_names(Requirements.txt)\ndgx_ui_tree(SdkConfig.json)\n")
    main_list = ("dgx_ui_tree(home.json NAMES names.txt APPEARANCE look.json)  # dgx_ui_tree(commented.json)\n"
                 "dgx_ui_tree(new.json)\ndgx_ui_tree(linked.json)\ndgx_ui_names(cmakelists.txt)\ndgx_ui_names(CMakeCache.txt)\n"
                 "dgx_ui_tree(nothere/page.json)\n")
    (project / "CMakeLists.txt").write_text(top_list)
    (project / "sdkconfig").write_text("CONFIG_X=y\n")
    (project / "main" / "CMakeLists.txt").write_text(main_list)
    (project / "components" / "x" / "CMakeLists.txt").write_text('idf_component_register()\nDGX_UI_NAMES("other.txt" PREFIX volume)\n')
    (project / "components" / "x" / "other.txt").write_text("value volume &volume\n")
    (project / "main" / "home.json").write_text('{"dgx-ui": 1}\n')
    (project / "main" / "look.json").write_text('{"dgx-ui": 1, "colors": {}}\n')
    (project / "main" / "data.json").write_text('{"rows": [1, 2]}\n')  # a .json of the program's, no description
    (project / "main" / "linked.json").symlink_to(project / "main" / "CMakeLists.txt")  # named by a call, a link to what is never written
    (project / "main" / "cmakelists.txt").write_text("")
    (project / "pages" / "loaded.json").write_text('{"dgx-ui": 1, "tree": {"kind": "box"}}\n')  # a page loaded at run time
    (project / "pages" / "look_like.json").write_text('{"dgx-ui": 1, "colors": {}}\n')  # an appearance loaded at run time
    (project / "pages" / "look_spoiled.json").write_text('{"dgx-ui": 1, "colors": {"paper": \n')  # one that does not read, to be mended
    (project / "pages" / "versioned.json").write_text('{"dgx-ui": 1, "name": "x"}\n')  # "dgx-ui" alone: neither a page nor an appearance
    (project / "pages" / "colors_alone.json").write_text('{"colors": {}}\n')  # "colors" with no "dgx-ui"
    (project / "build-x" / "look.json").write_text('{"dgx-ui": 1, "colors": {}}\n')  # an appearance where none is looked for
    (project / ".vscode" / "look.json").write_text('{"dgx-ui": 1, "colors": {}}\n')
    (project / "build-x" / "page.json").write_text('{"dgx-ui": 1, "tree": {"kind": "box"}}\n')  # a page where pages are not looked for
    (project / ".vscode" / "page.json").write_text('{"dgx-ui": 1, "tree": {"kind": "box"}}\n')
    (project / ".vscode" / "settings.json").write_text("{}\n")
    (project / "build-cyd" / "CMakeCache.txt").write_text("# a build's\n")
    for name in ("CMakePresets.json", "package.json", "version.txt", ".mcp.json"):
        (project / name).write_text("{}\n")
    (project / "main" / "names.txt").write_text("value volume &volume\n")
    # what the builder does not read: a secret of the program's, the configuration, a tool's
    (project / "sdkconfig").write_text("CONFIG_X=y\nCONFIG_WIFI_PASSWORD=secret\n") if not (project / "sdkconfig").exists() else None
    (project / "main" / "secrets.h").write_text("#define PASSWORD \"secret\"\n")
    (project / ".env").write_text("TOKEN=secret\n")
    (project / ".git").mkdir(exist_ok=True)
    (project / ".git" / "config").write_text("[remote] url = secret\n")
    (project / "dependencies.lock").write_text("secret: 1\n")
    (project / "main" / "wifi.json").write_text('{"ssid": "secret", "password": "secret"}\n')  # a .json of the program's
    # what it does read: the preset, a build's sdkconfig of the panel's lines alone, the fonts DGX writes, the board's entry
    (project / "CMakePresets.json").write_text('{"configurePresets": []}\n')
    (project / "build-host").mkdir()
    (project / "build-host" / "CMakeCache.txt").write_text("# a build's\n")
    (project / "build-host" / "sdkconfig").write_text("CONFIG_WIFI_PASSWORD=secret\nCONFIG_BOARD_HOST_WIDTH=320\nCONFIG_BOARD_HOST_HEIGHT=240\n"
                                                      "CONFIG_BOARD_HOST_COLOR_BITS=16\n")
    (project / "build" / "cyd" / "dgx_fonts").mkdir(parents=True)
    (project / "build" / "cyd" / "dgx_fonts" / "dgx_fonts-files.txt").write_text("small small.dgxf\n")
    (project / "build" / "cyd" / "dgx_fonts" / "small.dgxf").write_bytes(b"DGXF")
    (project / "build" / "cyd" / "dgx_fonts" / "other.bin").write_bytes(b"secret")
    (project / "components" / "local-boards" / "boards" / "cyd").mkdir(parents=True)
    (project / "components" / "local-boards" / "boards" / "cyd" / "board.json").write_text('{"devices": {}}\n')
    (project / "components" / "local-boards" / "boards" / "cyd" / "notes.json").write_text('{"secret": 1}\n')
    (project / "main" / "names.txt").chmod(0o640)
    (project / "main" / "out.txt").symlink_to(outside / "secret.txt")
    (project / "main" / "out_dir").symlink_to(outside / "deep")
    (project / "main" / "out_new.json").symlink_to(outside / "made.json")  # leads to no file yet, out of the project
    (project / "main" / "same.json").symlink_to(project / "main" / "look.json")
    (project / "main" / "beside.json").symlink_to(beside / "secret.json")
    (project / ".alias.json").symlink_to(project / "main" / "look.json")  # a name of a dot, of a file the calls name
    (project / "main" / "alias.json").symlink_to(project / "main" / "CMakeLists.txt")  # a name the builder writes, of a file it does not
    (project / "main" / "into_build").symlink_to(project / "build")

    process, port, token, said = start(project, "--port", "0")
    check(port is not None, f"the link printed: {said}")
    if port is None:
        raise SystemExit(1)
    host = f"127.0.0.1:{port}"
    origin = f"http://{host}"

    def ask(method, path, body=None, headers=None, token_given=True, origin_given=True, host_given=host):
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        sent = {"Host": host_given}
        if token_given:
            sent["Authorization"] = f"Bearer {token if token_given is True else token_given}"
        if origin_given and method == "POST":
            sent["Origin"] = origin if origin_given is True else origin_given
        sent.update(headers or {})
        connection.putrequest(method, path, skip_host=True, skip_accept_encoding=True)
        for key, value in sent.items():
            connection.putheader(key, value)
        data = body.encode() if isinstance(body, str) else body
        if data is not None and "Content-Length" not in sent:
            connection.putheader("Content-Length", str(len(data)))
        connection.endheaders(data)
        response = connection.getresponse()
        answer = (response.status, response.read(), dict(response.getheaders()))
        connection.close()
        return answer

    def at(kind, rel):
        return f"/project/{kind}?path=" + "".join(c if c.isalnum() or c in "/._-" else f"%{ord(c):02X}" for c in rel)

    # the builder's files, and nothing else of the component
    status, body, headers = ask("GET", "/editor/index.html", token_given=False)
    check(status == 200 and b"dgxUiBuilderMount" in body, f"the page served: {status}")
    csp = headers.get("Content-Security-Policy", "")
    check("connect-src 'self'" in csp and "default-src 'self'" in csp and "script-src 'self' 'unsafe-eval' 'wasm-unsafe-eval'" in csp and
          "base-uri 'none'" in csp, f"the page under its Content-Security-Policy: {csp}")
    check("X-Frame-Options" not in headers and "frame-ancestors" not in headers.get("Content-Security-Policy", ""),
          f"no header keeps the page out of a frame: {headers}")
    check(ask("GET", "/editor/host_server.js", token_given=False)[0] == 200, "its host served")
    check(ask("GET", "/editor/preview/dgx_ui_preview.js", token_given=False)[0] == 200, "the preview's module served")
    check(ask("GET", "/editor/%68ost_server.js", token_given=False)[0] == 200, "a file of the page by an encoded name served")
    for path in ("/editor/../CHANGES.md", "/editor/%2e%2e/CHANGES.md", "/editor/%252e%252e/CHANGES.md", "/editor/%25252e%25252e/CHANGES.md",
                 "/editor/..%2fCHANGES.md", "/editor/%2e%2e%2fCHANGES.md", "/editor/%252e%252e/src/dgx_ui.c", "/editor/%252e%252e/tools/dgx_ui_serve.py",
                 "/editor/preview/%2e%2e%2f%2e%2e%2fCHANGES.md", "/src/dgx_ui.c", "/tools/dgx_ui_serve.py", "/", "/CHANGES.md"):
        check(ask("GET", path, token_given=False)[0] == 404, f"{path}: not served")
    check(ask("GET", "/editor/index.html", host_given=f"localhost:{port}")[0] == 403, "the page by another Host: refused")
    check(ask("GET", "/editor/index.html", host_given="evil.example")[0] == 403, "the page by a name of another site: refused")

    # the project by the token
    status, body, _ = ask("GET", "/project")
    check(status == 200 and body == b'{"name": "demo"}', f"the project's name: {status} {body}")
    check(ask("GET", "/project", token_given=False)[0] == 401, "no token: refused")
    check(ask("GET", "/project", token_given="x" + token)[0] == 401, "another token: refused")
    check(ask("GET", at("list", ""), host_given="evil.example")[0] == 403, "a call by another Host: refused")
    status, body, _ = ask("GET", at("list", ""))
    check(status == 200 and [entry["name"] for entry in json.loads(body)] == sorted(name for name in os.listdir(project) if not name.startswith(".")),
          f"the project listed, nothing of a dot: {body}")
    for rel in (".git", ".vscode", "main/../.git"):
        status, body, _ = ask("GET", at("list", rel))
        check(status == 400 and b"config" not in body and b"settings" not in body, f"{rel}: not listed ({status})")
    status, body, _ = ask("GET", at("list", "main"))
    check(status == 200 and b"home.json" in body and b"same.json" in body and b"out" not in body, f"links out not listed: {body}")
    check(ask("GET", at("list", "nothing"))[:2] == (204, b""), "a folder not there: 204, no content")
    check(ask("GET", at("list", "main/out_dir"))[0] == 400, "a folder out, by a link: refused")
    status, body, _ = ask("GET", at("file", "main/home.json"))
    check(status == 200 and body == b'{"dgx-ui": 1}\n', f"a file read: {status} {body}")
    check(ask("GET", at("file", "main/none.json"))[:2] == (204, b""), "a file not there: 204, no content")
    check(ask("GET", at("file", "main/home.json"))[2].get("Content-Security-Policy") == csp, "a file of the project under the same policy")
    # reads by a rule, as writes are
    (project / "SdkConfig.json").write_text('{"dgx-ui": 1, "tree": {"secret": 1}}\n')  # a call names it, a description in it
    (project / ".vscode" / "tasks.json").write_text('{"dgx-ui": 1, "tree": {"secret": 1}}\n')  # a call names it, in a folder of a dot
    for rel in ("sdkconfig", ".env", "main/secrets.h", ".git/config", "dependencies.lock", "main/wifi.json", "build/cyd/dgx_fonts/other.bin",
                "components/local-boards/boards/cyd/notes.json", "main/data.json", ".vscode/settings.json", ".vscode/page.json", ".vscode/tasks.json",
                "SdkConfig.json"):
        status, body, _ = ask("GET", at("file", rel))
        check(status == 400 and b"secret" not in body, f"{rel}: not read ({status} {body[:80]})")
    for rel, wanted in (("CMakeLists.txt", None), ("main/CMakeLists.txt", None), ("components/x/CMakeLists.txt", None), ("CMakePresets.json", None),
                        ("main/names.txt", None), ("main/look.json", None), ("pages/loaded.json", None), ("pages/look_like.json", None),
                        ("build/cyd/dgx_fonts/dgx_fonts-files.txt", None), ("build/cyd/dgx_fonts/small.dgxf", b"DGXF"),
                        ("components/local-boards/boards/cyd/board.json", None),
                        ("build-host/sdkconfig", b"CONFIG_BOARD_HOST_WIDTH=320\nCONFIG_BOARD_HOST_HEIGHT=240\nCONFIG_BOARD_HOST_COLOR_BITS=16\n")):
        status, body, _ = ask("GET", at("file", rel))
        check(status == 200 and (wanted is None or body == wanted), f"{rel}: read ({status} {body[:80]})")
    # the project's sdkconfig.<BOARD> of the default preset, where a preset of boards keeps it: the panel's six lines alone;
    # another board's not read; neither written
    presets = (project / "CMakePresets.json").read_text()
    (project / "CMakePresets.json").write_text('{"configurePresets": [{"name": "default", "cacheVariables": {"BOARD": "x"}}]}\n')
    six = (b"CONFIG_BOARD_DISPLAY_WIDTH=480\nCONFIG_BOARD_DISPLAY_HEIGHT=272\nCONFIG_BOARD_DISPLAY_COLOR_BITS=16\n"
           b"CONFIG_BOARD_HOST_WIDTH=480\nCONFIG_BOARD_HOST_HEIGHT=272\nCONFIG_BOARD_HOST_COLOR_BITS=16\n")
    (project / "sdkconfig.x").write_bytes(b"CONFIG_WIFI_PASSWORD=secret\n" + six)
    (project / "sdkconfig.y").write_bytes(b"CONFIG_WIFI_PASSWORD=secret\n" + six)
    status, body, _ = ask("GET", at("file", "sdkconfig.x"))
    check(status == 200 and body == six, f"sdkconfig.x of the preset's board: the six lines alone ({status} {body[:80]})")
    status, body, _ = ask("GET", at("file", "sdkconfig.y"))
    check(status == 400 and b"secret" not in body, f"sdkconfig.y of another board: not read ({status})")
    status, _, _ = ask("POST", at("file", "sdkconfig.x"), b"CONFIG_X=y\n")
    check(status == 400 and b"secret" in (project / "sdkconfig.x").read_bytes(), f"sdkconfig.x: not written ({status})")
    (project / "CMakePresets.json").write_text(presets)
    status, _, _ = ask("GET", at("file", "sdkconfig.x"))
    check(status == 400, f"sdkconfig.x with no board of that name in the preset: not read ({status})")
    # a read goes by the file a link leads to: a link of a list's name, or of a name a call gives, to another file is refused
    (project / "CMakePresets.json").unlink()
    (project / "CMakePresets.json").symlink_to(project / "main" / "secrets.h")
    (project / "components" / "linked").mkdir()
    (project / "components" / "linked" / "CMakeLists.txt").symlink_to(project / "main" / "secrets.h")
    (project / "components" / "named").mkdir()
    (project / "components" / "named" / "CMakeLists.txt").write_text("dgx_ui_tree(page.json)\n")
    (project / "components" / "named" / "page.json").symlink_to(project / "main" / "wifi.json")
    for rel in ("CMakePresets.json", "components/linked/CMakeLists.txt", "components/named/page.json"):
        status, body, _ = ask("GET", at("file", rel))
        check(status == 400 and b"secret" not in body, f"{rel}, a link to another file: not read ({status} {body[:60]})")
    (project / "CMakePresets.json").unlink()
    (project / "CMakePresets.json").write_text(presets)
    shutil.rmtree(project / "components" / "linked")
    shutil.rmtree(project / "components" / "named")
    for rel in ("../outside/secret.txt", "main/../../outside/secret.txt", str(outside / "secret.txt"), "/etc/passwd", "main\\home.json",
                "main//home.json", "./main/home.json", "main/out.txt", "main/out_dir/secret.json", "main/beside.json", "../demo-beside/secret.json"):
        status, body, _ = ask("GET", at("file", rel))
        check(status == 400 and b"secret" not in body and b"not the project" not in body, f"{rel}: not read ({status})")

    # writes: what the calls name, a file they name created, a page there; every other file of the project kept as it is
    kept = {path: path.read_bytes() for path in project.rglob("*") if path.is_file() and not path.is_symlink()}
    for rel, text in (("main/home.json", '{"dgx-ui": 1, "tree": {"kind": "label"}}\n'), ("main/look.json", '{"dgx-ui": 1, "colors": {}}\n'),
                      ("main/new.json", '{"dgx-ui": 1, "tree": {"kind": "label"}}\n'), ("components/x/other.txt", "value level &level\n"),
                      ("pages/loaded.json", '{"dgx-ui": 1, "tree": {"kind": "label"}}\n'),
                      ("pages/look_like.json", '{"dgx-ui": 1, "colors": {"paper": "#000000"}}\n'),
                      ("pages/look_spoiled.json", '{"dgx-ui": 1, "colors": {"paper": "#000000"}}\n')):
        status, body, _ = ask("POST", at("file", rel), text)
        check(status == 200 and (project / rel).read_text() == text, f"{rel} written: {status} {body}")
    status, _, _ = ask("POST", at("file", "main/names.txt"), "value level &level\n")
    names = project / "main" / "names.txt"
    check(status == 200 and names.read_text() == "value level &level\n" and stat.S_IMODE(names.stat().st_mode) == 0o640,
          "a names file written, its mode kept")
    status, _, _ = ask("POST", at("file", "main/same.json"), '{"dgx-ui": 1, "name": "same"}\n')
    check(status == 200 and (project / "main" / "same.json").is_symlink() and
          (project / "main" / "look.json").read_text() == '{"dgx-ui": 1, "name": "same"}\n', "a write through a link in the project: its file")
    check(not [name for name in os.listdir(project / "main") if name.startswith(".dgx_ui_serve.")], "no file left beside")
    refusals = [
        ("no token", dict(token_given=False), "main/a.json", 401),
        ("another token", dict(token_given=token[::-1]), "main/a.json", 401),
        ("another Host", dict(host_given="evil.example"), "main/a.json", 403),
        ("no Origin", dict(origin_given=False), "main/a.json", 403),
        ("another Origin", dict(origin_given="http://evil.example"), "main/a.json", 403),
        ("the Origin of another port", dict(origin_given=f"http://127.0.0.1:{port + 1}"), "main/a.json", 403),
        ("CMakeLists.txt", {}, "CMakeLists.txt", 400),
        ("a file no call names", {}, "main/page.json", 400),
        ("a .json of the program's", {}, "main/data.json", 400),
        ("a .json of \"dgx-ui\" alone, neither a page nor an appearance", {}, "pages/versioned.json", 400),
        ("a .json of \"colors\" with no \"dgx-ui\"", {}, "pages/colors_alone.json", 400),
        ("an appearance where none is looked for", {}, "build-x/look.json", 400),
        ("an appearance in a folder of a dot", {}, ".vscode/look.json", 400),
        ("a call's file in a comment", {}, "main/commented.json", 400),
        ("a .json no call names, a description in its body", {}, "pages/new.json", 400),
        ("a page where pages are not looked for", {}, "build-x/page.json", 400),
        ("a page in a folder of a dot", {}, ".vscode/page.json", 400),
        (".vscode/tasks.json, a call naming it", {}, ".vscode/tasks.json", 400),
        (".vscode/settings.json", {}, ".vscode/settings.json", 400),
        (".claude/settings.json", {}, ".claude/settings.json", 400),
        (".mcp.json", {}, ".mcp.json", 400),
        (".devcontainer.json", {}, ".devcontainer.json", 400),
        ("CMakePresets.json, a call naming it", {}, "CMakePresets.json", 400),
        ("CMakeUserPresets.json", {}, "CMakeUserPresets.json", 400),
        ("CMakeCache.txt", {}, "build-cyd/CMakeCache.txt", 400),
        ("CMakeCache.txt where no build is, a call naming it", {}, "main/CMakeCache.txt", 400),
        ("a folder not there, a call naming it", {}, "main/nothere/page.json", 400),
        ("a name of a dot, a link to a file the calls name", {}, ".alias.json", 400),
        ("a folder of a build, a call naming it", {}, "build-cyd/page.json", 400),
        ("package.json", {}, "package.json", 400),
        ("version.txt", {}, "version.txt", 400),
        ("Build/, a call naming it", {}, "Build/page.json", 400),
        ("Requirements.txt, a call naming it", {}, "Requirements.txt", 400),
        ("SdkConfig.json, a call naming it", {}, "SdkConfig.json", 400),
        ("cmakelists.txt, a call naming it", {}, "main/cmakelists.txt", 400),
        ("a link named by a call, to a CMakeLists.txt", {}, "main/linked.json", 400),
        ("a component's CMakeLists.txt", {}, "main/CMakeLists.txt", 400),
        ("requirements.txt", {}, "main/requirements.txt", 400),
        ("sdkconfig", {}, "sdkconfig", 400),
        ("sdkconfig.json", {}, "sdkconfig.json", 400),
        ("a source", {}, "main/main.c", 400),
        ("under build/", {}, "build/a.json", 400),
        ("under managed_components/", {}, "managed_components/dgx/include/a.json", 400),
        ("a folder not there", {}, "pages/a.json", 400),
        ("..", {}, "../a.json", 400),
        ("from the root", {}, str(work / "a.json"), 400),
        ("a link out", {}, "main/out.txt", 400),
        ("a link out to no file yet", {}, "main/out_new.json", 400),
        ("a folder out, by a link", {}, "main/out_dir/secret.json", 400),
        ("a link to a folder whose name begins as the project's", {}, "main/beside.json", 400),
        ("a link in the project to a CMakeLists.txt", {}, "main/alias.json", 400),
        ("under build/, by a link in the project", {}, "main/into_build/a.json", 400),
    ]
    for what, options, rel, code in refusals:
        status, body, _ = ask("POST", at("file", rel), '{"spoiled": true}\n', **options)
        check(status == code, f"a write, {what}: {status} {body}, not {code}")
    check((outside / "secret.txt").read_text() == "not the project's\n" and (outside / "deep" / "secret.json").read_text() == "{}\n" and
          not (outside / "made.json").exists() and not (work / "a.json").exists() and (beside / "secret.json").read_text() == "{}\n",
          "nothing written out of the project")
    written = {project / rel for rel in ("main/home.json", "main/look.json", "main/names.txt", "components/x/other.txt", "pages/loaded.json",
                                         "pages/look_like.json", "pages/look_spoiled.json")}
    changed = [str(path.relative_to(project)) for path, data in kept.items() if path not in written and path.read_bytes() != data]
    made = [str(path.relative_to(project)) for path in project.rglob("*") if path.is_file() and not path.is_symlink() and path not in kept and
            path.name != "new.json"]
    check(not changed and not made and (project / "main" / "alias.json").is_symlink() and (project / "main" / "linked.json").is_symlink(),
          f"nothing of the refused written: changed {changed}, made {made}")

    # the calls read at each write: one added to main/CMakeLists.txt, its file written with no restart
    status, _, _ = ask("POST", at("file", "main/later.json"), "{}\n")
    (project / "main" / "CMakeLists.txt").write_text(main_list + "dgx_ui_tree(later.json)\n")
    status_after, _, _ = ask("POST", at("file", "main/later.json"), "{}\n")
    check(status == 400 and status_after == 200 and (project / "main" / "later.json").read_text() == "{}\n",
          f"a call added, its file written at once: {status} then {status_after}")

    # a body shorter than its length, the connection's writing side closed: refused, nothing written
    def short_body(close):
        connection = socket.create_connection(("127.0.0.1", port), timeout=40)
        connection.sendall((f"POST {at('file', 'main/home.json')} HTTP/1.1\r\nHost: {host}\r\nAuthorization: Bearer {token}\r\n"
                            f"Origin: {origin}\r\nContent-Length: 100\r\n\r\nshort!\n").encode())
        if close:
            connection.shutdown(socket.SHUT_WR)
        answer = b""
        while chunk := connection.recv(4096):  # to its end: the server closes after its answer
            answer += chunk
        connection.close()
        return answer
    before = (project / "main" / "home.json").read_bytes()
    answer = short_body(True)
    check(answer.startswith(b"HTTP/1.0 400") and b"ended before its length" in answer and (project / "main" / "home.json").read_bytes() == before,
          f"a body cut short: {answer[:60]}")
    # the same with the connection left open: ended by the server's time, the same answer
    started_at = time.monotonic()
    answer = short_body(False)
    check(answer.startswith(b"HTTP/1.0 400") and b"ended before its length" in answer and time.monotonic() - started_at < 30 and
          (project / "main" / "home.json").read_bytes() == before, f"a body cut short, the connection left open: {answer[:60]}")

    # the builder's reading of the calls against the server's: every file the builder may write, the server lets it; on
    # the hook project's CMakeLists and on calls of every form the builder reads
    serve = server_module()
    readers = work / "readers"
    (readers / "main").mkdir(parents=True)
    (readers / "components" / "c1").mkdir(parents=True)
    (readers / "CMakeLists.txt").write_text("project(readers)\n")
    (readers / "main" / "CMakeLists.txt").write_text(
        'dgx_ui_tree("quoted page.json" NAMES "../shared/names.txt")  # dgx_ui_tree(no.json)\n'
        "dgx_ui_appearance(looks/day.json NAMES vocab.txt PREFIX day)\n"
        "dgx_ui_tree(settings.json HEADERS program.h other.h APPEARANCE looks/day.json)\n"
        "DGX_UI_NAMES(vocab.txt)\n"
        "dgx_ui_tree(\n    sound.json\n    PREFIX volume\n    NAMES vocab.txt\n)\n")
    (readers / "components" / "c1" / "CMakeLists.txt").write_text("dgx_ui_tree(pages/sound.json PREFIX volume NAMES ../../main/vocab.txt)\n")
    for root in (here.parent / "hook" / "project", readers):
        lists = [rel for rel in serve.CALL_LISTS if (root / rel).is_file()]
        lists += [f"components/{name}/CMakeLists.txt" for name in sorted(os.listdir(root / "components")) if (root / "components" / name / "CMakeLists.txt").is_file()]
        paths = store_paths(root, lists)
        allowed = serve.called_files(os.path.realpath(root))
        check(paths and not set(paths) - allowed, f"{root.name}: the builder's files {paths}, the server's {sorted(allowed)}")
    status, body, _ = ask("POST", at("file", "main/big.json"), b"", headers={"Content-Length": str(5 * 1024 * 1024)})
    check(status == 413 and not (project / "main" / "big.json").exists(), f"a body too large: {status}")
    check(ask("POST", "/project/list?path=main", "{}")[0] == 404, "a write of no file: 404")
    status, _, headers = ask("OPTIONS", at("file", "main/a.json"), headers={"Access-Control-Request-Method": "POST"},
                             origin_given="http://evil.example")
    check(status >= 400 and not any(key.lower().startswith("access-control") for key in headers), f"no answer to a question of another origin: {status}")
    stop(process)

    # a copy of the component whose editor/ holds links out of it, one to a folder beside it whose name begins as its
    # own: neither is served
    component = work / "component"
    (component / "tools").mkdir(parents=True)
    shutil.copy(SERVER, component / "tools" / "dgx_ui_serve.py")
    shutil.copytree(SERVER.parent.parent / "editor", component / "editor", ignore=shutil.ignore_patterns("vendor", "preview"))
    (component / "editor-beside").mkdir()
    (component / "editor-beside" / "secret.txt").write_text("beside the page\n")
    (component / "CHANGES.md").write_text("of the component\n")
    (component / "editor" / "beside").symlink_to(component / "editor-beside")
    (component / "editor" / "changes.md").symlink_to(component / "CHANGES.md")
    process, port, token, said = start(project, "--port", "0", server=component / "tools" / "dgx_ui_serve.py")
    host = f"127.0.0.1:{port}"
    check(ask("GET", "/editor/index.html", token_given=False, host_given=host)[0] == 200, "the copy's page served")
    for path in ("/editor/beside/secret.txt", "/editor/changes.md", "/editor-beside/secret.txt", "/editor/%2e%2e/editor-beside/secret.txt"):
        status, body, _ = ask("GET", path, token_given=False, host_given=host)
        check(status == 404 and b"beside the page" not in body and b"of the component" not in body, f"{path}: not served ({status})")
    stop(process)

    # the project's own copy, of the registry or of git: its builder's page served, nothing of it ever run; a copy out
    # of managed_components/, one whose page speaks another API, has a link out, or is not as its CHECKSUMS.json says: the
    # server's own page
    marker = work / "run-marker"
    planted = f'import pathlib\npathlib.Path({str(marker)!r}).write_text("run")\nprint("http://127.0.0.1:1/editor/#project=copy", flush=True)\n'

    def copy_of(name, api=1, sums=True):
        folder = project / "managed_components" / name
        shutil.rmtree(folder, ignore_errors=True)
        (folder / "tools").mkdir(parents=True)
        for tool in ("dgx_ui_serve.py", "dgx_ui_mcp.py"):
            (folder / "tools" / tool).write_text(planted)
        shutil.copytree(SERVER.parent.parent / "editor", folder / "editor", ignore=shutil.ignore_patterns("vendor", "preview"))
        (folder / "editor" / "marker.txt").write_text(f"the page of {name}\n")
        host_js = folder / "editor" / "host_server.js"
        host_js.write_text(re.sub(r"DGX_UI_SERVER_API = \d+", f"DGX_UI_SERVER_API = {api}", host_js.read_text()))
        (folder / "idf_component.yml").write_text('version: "9.9.9"\n')
        if sums:
            files = [{"path": str(path.relative_to(folder)), "size": path.stat().st_size, "hash": hashlib.sha256(path.read_bytes()).hexdigest()}
                     for path in sorted(folder.rglob("*")) if path.is_file()]
            (folder / "CHECKSUMS.json").write_text(json.dumps({"version": "1.0", "algorithm": "sha256", "files": files}))
        return folder

    def page_of(name):
        process, port, token, said = start(project, "--port", "0")
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=10)
        connection.request("GET", "/editor/marker.txt", headers={"Host": f"127.0.0.1:{port}"})
        response = connection.getresponse()
        served = response.read() if response.status == 200 else None
        connection.close()
        stop(process)
        return served, said

    for name in ("jef-sure__dgx-ui", "dgx-ui"):
        copy_of(name)
        served, said = page_of(name)
        check(served == f"the page of {name}\n".encode() and any("9.9.9" in line and name in line for line in said), f"{name}: its page served: {said}")
        shutil.rmtree(project / "managed_components" / name)
    folder = copy_of("jef-sure__dgx-ui", api=2)
    served, said = page_of("jef-sure__dgx-ui")
    check(served is None and any("does not speak to" in line for line in said), f"a page of another API: the server's own: {said}")
    folder = copy_of("jef-sure__dgx-ui")
    (folder / "editor" / "app.js").write_text("// spoiled\n")
    served, said = page_of("jef-sure__dgx-ui")
    check(served is None and any("CHECKSUMS.json" in line for line in said), f"a file not as its checksums say: the server's own: {said}")
    folder = copy_of("jef-sure__dgx-ui")
    (folder / "editor" / "out.txt").symlink_to(outside / "secret.txt")
    served, said = page_of("jef-sure__dgx-ui")
    check(served is None and any("link out" in line for line in said), f"a link out of its page: the server's own: {said}")
    shutil.rmtree(folder)
    elsewhere = copy_of("jef-sure__dgx-ui")
    shutil.move(str(elsewhere), str(work / "elsewhere"))
    (project / "managed_components" / "jef-sure__dgx-ui").symlink_to(work / "elsewhere")
    served, said = page_of("jef-sure__dgx-ui")
    check(served is None and any("leads out" in line for line in said), f"a copy out of managed_components: the server's own: {said}")
    (project / "managed_components" / "jef-sure__dgx-ui").unlink()
    folder = copy_of("dgx-ui", sums=False)
    served, said = page_of("dgx-ui")
    check(served == b"the page of dgx-ui\n", f"a copy of git, no checksums: its page: {said}")
    shutil.rmtree(folder)
    check(not marker.exists(), "nothing of a copy ever run")

    # the fixed port, and another when it is taken; a port asked for and taken: an error
    holder = socket.socket()
    holder.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # as the server binds: a port left in TIME_WAIT is free
    try:
        holder.bind(("127.0.0.1", 8741))
        holder.listen()
        held = True
    except OSError:
        held = False  # taken by another already: the same for the server
    process, port, token, said = start(project)
    check(port not in (None, 8741) and any("start anew" in line for line in said), f"the fixed port taken: another, said: {said}")
    stop(process)
    done = subprocess.run([sys.executable, str(SERVER), "--project", str(project), "--port", "8741"], capture_output=True, text=True, timeout=30)
    check(done.returncode != 0 and "--port 8741" in done.stderr, f"a port asked for and taken: {done.returncode} {done.stderr.strip()}")
    holder.close()
    if held:
        process, port, token, said = start(project)
        check(port == 8741, f"the fixed port when it is free: {said}")
        stop(process)
finally:
    for process in started:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=10)
    shutil.rmtree(work, ignore_errors=True)

for failure in failures:
    print("  failed:", failure, file=sys.stderr)
print(f"test_serve.py: {checks - len(failures)} of {checks} checks held")
sys.exit(1 if failures else 0)
