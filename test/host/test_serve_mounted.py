#!/usr/bin/env python3
"""
test_serve_mounted.py: the builder mounted by another server, tools/dgx_ui_serve.py's mounted(), as boards' page
mounts it. A plain http.server of the test on a free port gives handle() every request first, under the prefix /x, and
answers 418 itself for what handle() leaves. Held: the prefix at a boundary of the path's parts (/xy is not /x's);
the builder's files under it, of the types of a fixed table and not sniffed, under the Content-Security-Policy,
/x/editor/ its index.html and /x/editor 301 to it, no other folder and no listing, nothing outside editor/ (an encoded
"..", once and twice, and a ".." of a part); the project's calls under it by the token of the link, of this Host, a
write from this origin alone, by the same rules of reading and writing as the server's own; the agent's /x/agent/... and
another path of /x left to the server; the project's name as boards' page names it, by its project(). And the server's
own handler, the same calls without a prefix: /editor/ and /editor.
"""
import http.client
import http.server
import importlib.util
import json
import os
import pathlib
import socket
import sys
import tempfile
import threading

here = pathlib.Path(__file__).resolve().parent
SERVER = here.parent.parent / "tools" / "dgx_ui_serve.py"
failures = []
checks = 0


def check(held, what):
    global checks
    checks += 1
    if not held:
        failures.append(what)
    return held


spec = importlib.util.spec_from_file_location("dgx_ui_serve", SERVER)
serve = importlib.util.module_from_spec(spec)
spec.loader.exec_module(serve)

work = tempfile.TemporaryDirectory()
project = pathlib.Path(work.name) / "proj"
(project / "main").mkdir(parents=True)
(project / "CMakeLists.txt").write_text("cmake_minimum_required(VERSION 3.16)\ndgx_ui_tree(main/page.json)\n")
(project / "main" / "page.json").write_text('{"dgx-ui": 1, "tree": {"kind": "box"}}')

with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
token = "t0ken"
handle = serve.mounted(str(project), token, port, "/x")


class Theirs(http.server.BaseHTTPRequestHandler):
    """Another server's handler: the builder's first, then its own answer."""

    def log_message(self, *_):
        pass

    def own(self):
        self.send_response(418)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        if not handle(self):
            self.own()

    def do_POST(self):
        if not handle(self):
            self.own()


theirs = http.server.ThreadingHTTPServer(("127.0.0.1", port), Theirs)
threading.Thread(target=theirs.serve_forever, daemon=True).start()
HOST = f"127.0.0.1:{port}"
ORIGIN = f"http://{HOST}"


def ask(method, path, body=None, host=HOST, auth=True, origin=ORIGIN, server_port=None):
    connection = http.client.HTTPConnection("127.0.0.1", server_port or port, timeout=10)
    headers = {"Host": host}
    if auth:
        headers["Authorization"] = f"Bearer {token}"
    if origin:
        headers["Origin"] = origin
    if body is not None:
        headers["Content-Length"] = str(len(body))
    connection.putrequest(method, path, skip_host=True)
    for name, value in headers.items():
        connection.putheader(name, value)
    connection.endheaders(body)
    response = connection.getresponse()
    data = response.read()
    connection.close()
    return response.status, dict(response.getheaders()), data


# the prefix at a boundary
status, _, _ = ask("GET", "/xy/editor/index.html")
check(status == 418, f"/xy is not of /x: {status}")
status, _, _ = ask("GET", "/editor/index.html")
check(status == 418, f"/editor without the prefix is the server's: {status}")
for path in ("/x/agent/updates?messages=0&requests=0", "/x/README.md", "/x", "/x/"):
    status, _, _ = ask("GET", path)
    check(status == 418, f"{path}: left to the server, {status}")

# the builder's files: the page, its scripts, types of the table, not sniffed, under CSP; the folder and no other
index = (serve.ROOT / "editor" / "index.html").read_bytes()
status, headers, data = ask("GET", "/x/editor/")
check(status == 200 and data == index and headers.get("Content-Type") == "text/html; charset=utf-8", f"/x/editor/: {status} {headers.get('Content-Type')}")
check(headers.get("Content-Security-Policy") == serve.CSP and headers.get("X-Content-Type-Options") == "nosniff", "the page under CSP, not sniffed")
status, headers, data = ask("GET", "/x/editor/index.html?a=1")
check(status == 200 and data == index, f"the query not read: {status}")
status, headers, _ = ask("GET", "/x/editor")
check(status == 301 and headers.get("Location") == "/x/editor/", f"/x/editor: 301 to the folder, {status} {headers.get('Location')}")
for name, kind in (("app.js", "text/javascript; charset=utf-8"), ("builder.css", "text/css; charset=utf-8"), ("README.md", "text/plain; charset=utf-8"),
                   ("vendor/LICENSE.vue", "application/octet-stream")):
    status, headers, data = ask("GET", f"/x/editor/{name}")
    check(status == 200 and headers.get("Content-Type") == kind and data == (serve.ROOT / "editor" / name).read_bytes(), f"/x/editor/{name}: {status} {headers.get('Content-Type')}")
for path in ("/x/editor/preview/", "/x/editor/vendor", "/x/editor/%2e%2e/CHANGES.md", "/x/editor/%252e%252e/CHANGES.md", "/x/editor/..%2fCHANGES.md",
             "/x/editor/../CHANGES.md", "/x/editor/nothing.js"):
    status, _, data = ask("GET", path)
    check(status == 404 and b"Changes" not in data, f"{path}: none, {status}")

# the project's calls: this Host, the token, a write from this origin; the rules of reading and writing the server's
status, _, _ = ask("GET", "/x/editor/app.js", host="localhost:1")
check(status == 403, f"another Host for a file: {status}")
status, _, _ = ask("GET", "/x/project", host=f"localhost:{port}")
check(status == 403, f"another Host: {status}")
status, _, _ = ask("GET", "/x/project", auth=False)
check(status == 401, f"no token: {status}")
status, _, data = ask("GET", "/x/project")
check(status == 200 and json.loads(data) == {"name": "proj"}, f"/x/project: {status} {data!r}")
# the project's name as boards' page names it: the first project(<name>) of the root CMakeLists, else the folder's
lists = (project / "CMakeLists.txt").read_text()
for text, name in (("project(hook)\n", "hook"), ("  project(  spaced VERSION 1)\nproject(second)\n", "spaced"), ("# project(no)\n", "proj")):
    (project / "CMakeLists.txt").write_text(lists + text)
    status, _, data = ask("GET", "/x/project")
    check(status == 200 and json.loads(data) == {"name": name}, f"the name of {text!r}: {data!r}")
(project / "CMakeLists.txt").write_text(lists)
status, _, data = ask("GET", "/x/project/file?path=main/page.json")
check(status == 200 and data == (project / "main" / "page.json").read_bytes(), f"a page read: {status}")
status, _, _ = ask("GET", "/x/project/file?path=main/../../x")
check(status == 400, f"a path of ..: {status}")
status, _, data = ask("GET", "/x/project/list?path=main")
check(status == 200 and json.loads(data) == [{"name": "page.json", "directory": False}], f"a folder listed: {status} {data!r}")
written = b'{"dgx-ui": 1, "tree": {"kind": "frame"}}'
status, _, _ = ask("POST", "/x/project/file?path=main/page.json", body=written, origin="http://example.com")
check(status == 403 and (project / "main" / "page.json").read_bytes() != written, f"a write from another page: {status}")
status, _, _ = ask("POST", "/x/project/file?path=main/page.json", body=written)
check(status == 200 and (project / "main" / "page.json").read_bytes() == written, f"a write of a called file: {status}")
status, _, _ = ask("POST", "/x/project/file?path=CMakeLists.txt", body=b"x")
check(status == 400 and b"x" != (project / "CMakeLists.txt").read_bytes(), f"the build's own not written: {status}")
status, _, _ = ask("POST", "/x/editor/app.js", body=b"x")
check(status == 404, f"no write to the builder's files: {status}")

# the server's own handler: the same calls with no prefix
own = http.server.ThreadingHTTPServer(("127.0.0.1", 0), serve.handler_of(str(project), token, 0))
own_port = own.server_address[1]
own.RequestHandlerClass = serve.handler_of(str(project), token, own_port)
threading.Thread(target=own.serve_forever, daemon=True).start()
status, headers, data = ask("GET", "/editor/", host=f"127.0.0.1:{own_port}", server_port=own_port)
check(status == 200 and data == index and headers.get("X-Content-Type-Options") == "nosniff", f"the server's /editor/: {status}")
status, headers, _ = ask("GET", "/editor", host=f"127.0.0.1:{own_port}", server_port=own_port)
check(status == 301 and headers.get("Location") == "/editor/", f"the server's /editor: {status} {headers.get('Location')}")
status, _, _ = ask("GET", "/x/editor/", host=f"127.0.0.1:{own_port}", server_port=own_port)
check(status == 404, f"the server's own has no prefix: {status}")

theirs.shutdown(), own.shutdown()
work.cleanup()
for failure in failures:
    print("  failed: " + failure, file=sys.stderr)
print(f"test_serve_mounted.py: {checks - len(failures)} of {checks} checks held")
sys.exit(1 if failures else 0)
