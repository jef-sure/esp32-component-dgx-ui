#!/usr/bin/env python3
"""
The builder of pages served from a terminal, for a browser that gives a page no folder to open, as the browsers inside
VS Code: the builder's own files, and a project's files read and written through this server.

Usage, in a project's folder:
    python3 managed_components/jef-sure__dgx-ui/tools/dgx_ui_serve.py [--project DIR] [--port N]
(managed_components/dgx-ui/ for dgx-ui taken from git).

It prints the link of the builder, which opens the project at once. The project is the current folder, or --project. A
project with its own copy of dgx-ui from the registry, managed_components/jef-sure__dgx-ui/, is served by that copy:
this one hands over to it at the start, so the builder is of the version the project builds with. Standard library
only, so the Python of ESP-IDF runs it.

Only this computer reaches it: bound to 127.0.0.1, every request of this Host. The project's calls carry a token the
link gives, a write comes from the builder's own page, and the paths lie in the project. What is written: a file of
.json or .txt, the builder's descriptions, appearances and names files, never one the build reads otherwise.
"""
import argparse
import http.server
import json
import os
import pathlib
import secrets
import sys
import tempfile
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent  # the copy of dgx-ui this runs from
ADDRESS = "127.0.0.1"  # this computer only: bound to it, and the Host of every request
SERVED = ROOT / "editor"  # the builder's files: nothing else of the component is served
DEFAULT_PORT = 8741  # fixed, so that the page's origin, and the settings it keeps in the browser, stay from run to run
LONGEST_BODY = 4 * 1024 * 1024
HANDED = "DGX_UI_SERVE_HANDED"

# what the builder writes, and what it never does: a file the build reads by its name, though it ends so
WRITTEN_SUFFIXES = (".json", ".txt")
NEVER_WRITTEN = ("CMakeLists.txt", "requirements.txt")  # by name; dependencies.lock and sdkconfig* below, of no such suffix
NEVER_WRITTEN_PREFIXES = ("sdkconfig", "dependencies.lock")
NEVER_UNDER = ("build", "managed_components", ".git")


def inside(root, path):
    """Whether a real path lies in the real root, or is it."""
    return path == root or path.startswith(root + os.sep)


def problem_of(root, rel, writing=False, listing=False):
    """What is wrong with a path of the project a call names, or None: relative, of no empty part, no "." or "..", no
    backslash and no NUL, leading nowhere out of the project, links followed; a write by the rule of what is written,
    into a folder that is there."""
    if not isinstance(rel, str) or "\\" in rel or "\0" in rel:
        return "a path of the project, by / and of no backslash"
    if rel == "" and listing:
        return None
    if rel.startswith("/"):
        return "a path within the project, not from the root of the disk"
    parts = rel.split("/")
    if any(part in ("", ".", "..") for part in parts):
        return "a path of no empty part, no . and no .."
    real = os.path.realpath(os.path.join(root, rel))
    if not inside(root, real):
        return "a path that leads out of the project"
    if writing:
        # the rule for the file the path leads to, its links followed: the file that would be written
        written = os.path.relpath(real, root).split(os.sep)
        if not written[-1].endswith(WRITTEN_SUFFIXES):
            return "the builder writes descriptions and appearances of .json and names files of .txt only"
        if written[-1] in NEVER_WRITTEN or written[-1].startswith(NEVER_WRITTEN_PREFIXES):
            return f"{written[-1]} is the build's own: not written by the builder"
        if any(part in NEVER_UNDER for part in written[:-1]):
            return "nothing is written under build/, managed_components/ or .git/"
        if not os.path.isdir(os.path.dirname(real)):
            return "a file of a folder that is there: no folder is made"
    return None


def write_file(root, rel, data):
    """The bytes into the file the path names, its links followed: a file beside it first, put in its place at once."""
    target = os.path.realpath(os.path.join(root, rel))
    mode = os.stat(target).st_mode & 0o7777 if os.path.isfile(target) else None
    handle, temporary = tempfile.mkstemp(dir=os.path.dirname(target), prefix=".dgx_ui_serve.")
    try:
        with os.fdopen(handle, "wb") as out:
            out.write(data)
        if mode is not None:
            os.chmod(temporary, mode)
        os.replace(temporary, target)
    except BaseException:
        if os.path.exists(temporary):
            os.remove(temporary)
        raise


def handler_of(project, token, port):
    root, served = os.path.realpath(project), os.path.realpath(SERVED)
    own_origin = f"http://{ADDRESS}:{port}"

    class Page(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, directory=str(ROOT), **kwargs)

        def log_message(self, *_):
            pass

        def end_headers(self):
            # every file asked again: a page cached from another copy or version would run one script of each. No
            # X-Frame-Options and no frame-ancestors: the browser of VS Code shows the page in a frame of its own
            if not any(line.lower().startswith(b"cache-control") for line in getattr(self, "_headers_buffer", [])):
                self.send_header("Cache-Control", "no-cache")
            super().end_headers()

        def answer(self, code, value=None, body=None, kind="application/json"):
            body = body if body is not None else json.dumps({} if value is None else value).encode()
            self.send_response(code)
            if code != 204:
                self.send_header("Content-Type", kind)
                self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def allowed(self):
            """Only this address: another site in the browser can reach 127.0.0.1, by a name of its own that leads here,
            but not with this Host."""
            return self.headers.get("Host") == f"{ADDRESS}:{port}"

        def by_token(self):
            return secrets.compare_digest(self.headers.get("Authorization", ""), f"Bearer {token}")

        def from_own_page(self):
            """A page of another site can send to 127.0.0.1 without reading the answer: a write comes from this one. In
            the frame of VS Code's browser the page keeps this origin."""
            return self.headers.get("Origin") == own_origin

        def path_asked(self, address, **rule):
            rel = urllib.parse.parse_qs(address.query).get("path", [""])[0]
            return rel, problem_of(root, rel, **rule)

        def do_GET(self):
            if not self.allowed():
                return self.answer(403, {"error": "another Host"})
            address = urllib.parse.urlsplit(self.path)
            if address.path == "/project" or address.path.startswith("/project/"):
                if not self.by_token():
                    return self.answer(401, {"error": "the token of the link"})
                if address.path == "/project":
                    return self.answer(200, {"name": os.path.basename(root)})
                if address.path == "/project/list":
                    rel, problem = self.path_asked(address, listing=True)
                    if problem:
                        return self.answer(400, {"error": problem})
                    folder = os.path.join(root, rel)
                    if not os.path.isdir(folder):
                        return self.answer(204, body=b"")  # none: not an error, which a browser would show in red
                    entries = []
                    with os.scandir(folder) as found:
                        for entry in found:
                            if entry.is_symlink() and not inside(root, os.path.realpath(entry.path)):
                                continue  # a link out of the project is not shown
                            entries.append({"name": entry.name, "directory": entry.is_dir()})
                    return self.answer(200, sorted(entries, key=lambda entry: entry["name"]))
                if address.path == "/project/file":
                    rel, problem = self.path_asked(address)
                    if problem:
                        return self.answer(400, {"error": problem})
                    path = os.path.join(root, rel)
                    if not os.path.isfile(path):
                        return self.answer(204, body=b"")
                    with open(path, "rb") as data:
                        return self.answer(200, body=data.read(), kind="application/octet-stream")
                return self.answer(404)
            # the file that will be read, by the base class's own reading of the path, its links followed: of the
            # builder's folder, whatever the path's encoding
            if not inside(served, os.path.realpath(self.translate_path(self.path))):
                return self.answer(404)
            return super().do_GET()

        def do_HEAD(self):
            return self.answer(405)

        def do_POST(self):
            if not self.allowed():
                return self.answer(403, {"error": "another Host"})
            if not self.by_token():
                return self.answer(401, {"error": "the token of the link"})
            if not self.from_own_page():
                return self.answer(403, {"error": "a write from another page"})
            address = urllib.parse.urlsplit(self.path)
            if address.path != "/project/file":
                return self.answer(404)
            length = self.headers.get("Content-Length", "")
            if not length.isdigit():
                return self.answer(411)
            if int(length) > LONGEST_BODY:
                return self.answer(413, {"error": f"a file of at most {LONGEST_BODY} bytes"})
            data = self.rfile.read(int(length))
            rel, problem = self.path_asked(address, writing=True)
            if problem:
                return self.answer(400, {"error": problem})
            try:
                write_file(root, rel, data)
            except OSError as error:
                # a folder that may not be written, a full disk: the page is told, and the server goes on
                return self.answer(500, {"error": f"{rel}: {error.strerror or error}"})
            return self.answer(200)

    return Page


def own_copy(project):
    """The server of the project's own copy of dgx-ui from the registry, when it has one and this is not it."""
    if os.environ.get(HANDED):
        return None
    theirs = project / "managed_components" / "jef-sure__dgx-ui" / "tools" / "dgx_ui_serve.py"
    return theirs if theirs.is_file() and theirs.resolve() != pathlib.Path(__file__).resolve() else None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--project", type=pathlib.Path, default=pathlib.Path.cwd(), help="the project's folder; the current one by default")
    parser.add_argument("--port", type=int, default=None, help=f"the port; {DEFAULT_PORT} by default, any free one when it is taken")
    args = parser.parse_args()
    if not args.project.is_dir():
        parser.error(f"--project {args.project}: no such folder")
    theirs = own_copy(args.project.resolve())
    if theirs:
        print(f"dgx_ui_serve.py: the project's own copy serves it: {theirs}", file=sys.stderr, flush=True)
        os.environ[HANDED] = "1"
        os.execv(sys.executable, [sys.executable, str(theirs), *sys.argv[1:]])

    try:
        server = http.server.ThreadingHTTPServer((ADDRESS, DEFAULT_PORT if args.port is None else args.port), None)
    except OSError as error:
        if args.port is not None:
            parser.error(f"--port {args.port}: {error.strerror or error}")
        server = http.server.ThreadingHTTPServer((ADDRESS, 0), None)
        print(f"Port {DEFAULT_PORT} is taken: served on another, where the builder's settings in the browser start anew.", flush=True)
    port = server.server_address[1]
    token = secrets.token_urlsafe(24)
    server.RequestHandlerClass = handler_of(args.project, token, port)
    server.daemon_threads = True
    print(f"http://{ADDRESS}:{port}/editor/#project={token}", flush=True)
    print(f"The builder reads and writes {args.project.resolve()}", flush=True)
    print("Ctrl-C stops the server.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
