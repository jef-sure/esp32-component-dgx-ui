#!/usr/bin/env python3
"""
test_serve.py: tools/dgx_ui_serve.py on a project in a temporary directory. The builder's files served, nothing else
of the component; the project listed, read and written by the token of the link; each refusal: another Host, for a
file of the page too, no token or another, a write from another page, a path with "..", from the root, of a
backslash, a link leading out, a file the build reads by its name (CMakeLists.txt, sdkconfig), one of another kind,
one under build/ or managed_components/, into a folder not there, a body too large; no answer to a question of
another origin and no header that keeps the page out of a frame. A write through a link within the project writes the
file it leads to and keeps a file's mode. A link of the page's folder leading out of it, to a folder beside it too, not
followed. The project's own copy from the registry served by it, no other looked for;
a fixed port, and another when it is taken.
"""
import http.client
import os
import pathlib
import re
import shutil
import socket
import stat
import subprocess
import sys
import tempfile

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
    said = []
    for _ in range(10):
        line = process.stdout.readline()
        if not line:
            break
        said.append(line.rstrip("\n"))
        link = re.match(r"http://127\.0\.0\.1:(\d+)/editor/#project=([\w-]+)$", said[-1])
        if link:
            return process, int(link.group(1)), link.group(2), said
    process.kill()
    return process, None, None, said


def stop(process):
    process.kill()
    process.wait(timeout=10)
    process.stdout.close()


work = pathlib.Path(tempfile.mkdtemp(prefix="dgx-ui-serve-"))
try:
    project, outside = work / "demo", work / "outside"
    for folder in (project / "main", project / "build", project / "managed_components" / "dgx" / "include", outside / "deep"):
        folder.mkdir(parents=True)
    (outside / "secret.txt").write_text("not the project's\n")
    beside = work / "demo-beside"  # its name begins as the project's: out of it all the same
    beside.mkdir()
    (beside / "secret.json").write_text("{}\n")
    (outside / "deep" / "secret.json").write_text("{}\n")
    (project / "CMakeLists.txt").write_text("project(demo)\n")
    (project / "sdkconfig").write_text("CONFIG_X=y\n")
    (project / "main" / "CMakeLists.txt").write_text("dgx_ui_tree(home.json NAMES names.txt)\n")
    (project / "main" / "home.json").write_text('{"dgx-ui": 1}\n')
    (project / "main" / "names.txt").write_text("value volume &volume\n")
    (project / "main" / "names.txt").chmod(0o640)
    (project / "main" / "out.txt").symlink_to(outside / "secret.txt")
    (project / "main" / "out_dir").symlink_to(outside / "deep")
    (project / "main" / "out_new.json").symlink_to(outside / "made.json")  # leads to no file yet, out of the project
    (project / "main" / "same.json").symlink_to(project / "main" / "home.json")
    (project / "main" / "beside.json").symlink_to(beside / "secret.json")
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
    check(status == 200 and body == b'[{"name": "CMakeLists.txt", "directory": false}, {"name": "build", "directory": true}, '
          b'{"name": "main", "directory": true}, {"name": "managed_components", "directory": true}, {"name": "sdkconfig", "directory": false}]',
          f"the project listed: {body}")
    status, body, _ = ask("GET", at("list", "main"))
    check(status == 200 and b"home.json" in body and b"same.json" in body and b"out" not in body, f"links out not listed: {body}")
    check(ask("GET", at("list", "nothing"))[:2] == (204, b""), "a folder not there: 204, no content")
    check(ask("GET", at("list", "main/out_dir"))[0] == 400, "a folder out, by a link: refused")
    status, body, _ = ask("GET", at("file", "main/home.json"))
    check(status == 200 and body == b'{"dgx-ui": 1}\n', f"a file read: {status} {body}")
    check(ask("GET", at("file", "main/none.json"))[:2] == (204, b""), "a file not there: 204, no content")
    for rel in ("../outside/secret.txt", "main/../../outside/secret.txt", str(outside / "secret.txt"), "/etc/passwd", "main\\home.json",
                "main//home.json", "./main/home.json", "main/out.txt", "main/out_dir/secret.json", "main/beside.json", "../demo-beside/secret.json"):
        status, body, _ = ask("GET", at("file", rel))
        check(status == 400 and b"secret" not in body and b"not the project" not in body, f"{rel}: not read ({status})")

    # writes
    status, _, _ = ask("POST", at("file", "main/page.json"), '{"dgx-ui": 1, "name": "page"}\n')
    check(status == 200 and (project / "main" / "page.json").read_text() == '{"dgx-ui": 1, "name": "page"}\n', f"a file written: {status}")
    status, _, _ = ask("POST", at("file", "main/names.txt"), "value level &level\n")
    names = project / "main" / "names.txt"
    check(status == 200 and names.read_text() == "value level &level\n" and stat.S_IMODE(names.stat().st_mode) == 0o640,
          "a names file written, its mode kept")
    status, _, _ = ask("POST", at("file", "main/same.json"), '{"dgx-ui": 1, "name": "same"}\n')
    check(status == 200 and (project / "main" / "same.json").is_symlink() and
          (project / "main" / "home.json").read_text() == '{"dgx-ui": 1, "name": "same"}\n', "a write through a link in the project: its file")
    check(not [name for name in os.listdir(project / "main") if name.startswith(".dgx_ui_serve.")], "no file left beside")
    refusals = [
        ("no token", dict(token_given=False), "main/a.json", 401),
        ("another token", dict(token_given=token[::-1]), "main/a.json", 401),
        ("another Host", dict(host_given="evil.example"), "main/a.json", 403),
        ("no Origin", dict(origin_given=False), "main/a.json", 403),
        ("another Origin", dict(origin_given="http://evil.example"), "main/a.json", 403),
        ("the Origin of another port", dict(origin_given=f"http://127.0.0.1:{port + 1}"), "main/a.json", 403),
        ("CMakeLists.txt", {}, "CMakeLists.txt", 400),
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
    check((project / "CMakeLists.txt").read_text() == "project(demo)\n" and (project / "sdkconfig").read_text() == "CONFIG_X=y\n" and
          (project / "main" / "CMakeLists.txt").read_text() == "dgx_ui_tree(home.json NAMES names.txt)\n" and
          not (project / "main" / "a.json").exists() and not (project / "build" / "a.json").exists() and
          (project / "main" / "alias.json").is_symlink(), "nothing of the refused written")
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

    # the project's own copy from the registry serves it; a copy elsewhere in managed_components/ is not looked for
    copy = project / "managed_components" / "jef-sure__dgx-ui" / "tools"
    copy.mkdir(parents=True)
    (copy / "dgx_ui_serve.py").write_text('import sys\nprint("http://127.0.0.1:1/editor/#project=copy", flush=True)\nprint(sys.argv[1:], flush=True)\n')
    process, port, token, said = start(project, "--port", "0")
    check(token == "copy" and any("own copy" in line for line in said), f"the project's copy serves it: {said}")
    stop(process)
    shutil.rmtree(project / "managed_components" / "jef-sure__dgx-ui")
    other = project / "managed_components" / "dgx-ui" / "tools"
    other.mkdir(parents=True)
    (other / "dgx_ui_serve.py").write_text('print("http://127.0.0.1:1/editor/#project=other", flush=True)\n')
    process, port, token, said = start(project, "--port", "0")
    check(token not in (None, "other") and not any("own copy" in line for line in said), f"a copy of another folder not handed to: {said}")
    stop(process)
    shutil.rmtree(project / "managed_components" / "dgx-ui")

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
