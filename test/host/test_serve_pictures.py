#!/usr/bin/env python3
"""
test_serve_pictures.py <DGX>: the pictures of tools/dgx_ui_serve.py. A project whose main/CMakeLists calls
dgx_image_needs_file(images.txt), with a copy of the DGX given in managed_components/jef-sure__dgx and its CHECKSUMS.json
written here as the component manager writes it when it fetches the component, so that the DGX given may be the registry's
archive unpacked, a copy the manager fetched, or a checkout:

- a picture made of PNG files by the writer of that copy: the bytes of a file of DGX, the words of the writer's refusal
  for one it refuses; the words of the request but its PNG files, its depth and its options read nowhere;
- the copy's writer not as its CHECKSUMS.json says, the copy with no CHECKSUMS.json, no copy at all: refused in words;
- the writer run apart: the server's Python with -I, a fresh folder, an environment of PATH alone; one that hangs cut at
  its time, one that prints without end bounded (a copy of its own sums, as a project could write them);
- the size of each PNG read of its head before anything is written or run: refused, a side over 4096, all of them over
  4 Mpx, a PNG whose first part is not its head; 4096 by 1024 taken;
- a PNG written beside the file of needs, into images/ made there; refused: not a PNG, out of the folder, over a file no
  need names, from another page; one a need names written over;
- read: the file of needs, a PNG a need names, the files DGX made that its list names; not a PNG no need names.
"""
import base64
import hashlib
import http.client
import importlib.util
import json
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

here = pathlib.Path(__file__).resolve().parent
SERVER = here.parent.parent / "tools" / "dgx_ui_serve.py"
dgx = pathlib.Path(sys.argv[1]).resolve()
failures = []
checks = 0


def check(held, what):
    global checks
    checks += 1
    if not held:
        failures.append(what)
        print("  failed: " + what, file=sys.stderr)
    return held


def png(w, h, color):
    raw = b"".join(b"\x00" + bytes(color) * w for _ in range(h))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")


spec = importlib.util.spec_from_file_location("dgx_ui_serve", SERVER)
serve = importlib.util.module_from_spec(spec)
spec.loader.exec_module(serve)

work = pathlib.Path(tempfile.mkdtemp(prefix="dgx-ui-pictures-"))
started = []
try:
    project = work / "demo"
    (project / "main" / "art").mkdir(parents=True)
    (project / "managed_components").mkdir()
    (project / "build" / "dgx_images").mkdir(parents=True)
    (project / "CMakeLists.txt").write_text("project(demo)\n")
    (project / "main" / "CMakeLists.txt").write_text("idf_component_register()\ndgx_image_needs_file(images.txt)\n")
    (project / "main" / "images.txt").write_text('logo PNG art/logo.png "art/two words.png" BITS 4\n')
    (project / "main" / "art" / "logo.png").write_bytes(png(4, 3, (255, 0, 0, 255)))
    (project / "main" / "art" / "other.png").write_bytes(png(4, 3, (0, 0, 255, 255)))
    (project / "build" / "dgx_images" / "dgx_images-files.txt").write_text("logo logo.dgxi\n")
    (project / "build" / "dgx_images" / "logo.dgxi").write_bytes(b"DGXI")
    (project / "build" / "dgx_images" / "stray.dgxi").write_bytes(b"DGXI")
    copy = project / "managed_components" / "jef-sure__dgx"
    shutil.copytree(dgx, copy, ignore=shutil.ignore_patterns(".git", "CHECKSUMS.json"))
    # the sums of every file of the copy, by the keys the manager writes and the server reads
    listed = [{"path": one.relative_to(copy).as_posix(), "size": one.stat().st_size, "hash": hashlib.sha256(one.read_bytes()).hexdigest()}
              for one in sorted(copy.rglob("*")) if one.is_file()]
    (copy / "CHECKSUMS.json").write_text(json.dumps({"version": "1.0", "algorithm": "sha256", "files": listed}))
    root = str(project.resolve())
    logo = base64.b64encode(png(6, 5, (0, 200, 0, 255))).decode()

    # the writer of the project's copy: a file of DGX, of the depth asked; a refusal in the writer's words
    made, said = serve.make_picture(root, {"pngs": [logo, logo], "bits": 4, "reduce": False, "dither": True})
    check(made and made[:4] == b"DGXI" and made[6] == 4 and made[8] == 2, f"a picture made by the project's DGX: {said}")
    marker = work / "marker"
    (work / "evil.py").write_text(f"open({str(marker)!r}, 'w').write('run')\n")
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 1, "tool": str(work / "evil.py"), "path": str(work / "evil.py"), "dgx": str(work)})
    check(made and made[6] == 1 and not marker.exists(), "a request's own path read nowhere")
    many = base64.b64encode(png(5, 5, (0, 0, 0, 255))[:-12] + b"").decode()
    made, said = serve.make_picture(root, {"pngs": [many], "bits": 4})
    check(made is None and said, f"a spoiled PNG: the writer's words: {said}")
    for asked, words in (({"pngs": "x", "bits": 4}, "1 to 64, each as base64"), ({"pngs": [logo], "bits": 2}, "bits 1, 4, 8 or 16"),
                         ({"pngs": [logo], "bits": 4, "ink": "red"}, "ink dark or light"), ({"pngs": [logo], "bits": 4, "background": "red"}, "six hex digits"),
                         ({"pngs": [base64.b64encode(b"GIF89a").decode()], "bits": 4}, "is not a PNG")):
        made, said = serve.make_picture(root, asked)
        check(made is None and words in said, f"{asked}: refused with \"{words}\": {said}")

    # the copy held to its sums: a byte of the writer changed, no CHECKSUMS.json, no copy
    tool = copy / "tools" / "dgx_images.py"
    original = tool.read_bytes()
    tool.write_bytes(original + b"\n# changed\n")
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 4})
    check(made is None and said == "managed_components/jef-sure__dgx/tools/dgx_images.py is not as its CHECKSUMS.json says: the picture is not made", said)
    tool.write_bytes(original)
    sums = (copy / "CHECKSUMS.json").read_bytes()
    (copy / "CHECKSUMS.json").unlink()
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 4})
    check(made is None and said == "DGX here is not the registry's copy (no CHECKSUMS.json in managed_components/jef-sure__dgx): the picture is not "
          "made; take jef-sure/dgx from the registry", said)
    (copy / "CHECKSUMS.json").write_bytes(sums)
    shutil.move(str(copy), str(work / "away"))
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 4})
    check(made is None and said.startswith("the project has no copy of DGX of the registry in managed_components/jef-sure__dgx"), said)
    shutil.move(str(work / "away"), str(copy))

    # the writer run apart: a copy whose writer is another, its sums written for it as a project could
    def writer(text):
        tool.write_text(text)
        listed = json.loads(sums)
        for entry in listed["files"]:
            if entry["path"] == "tools/dgx_images.py":
                entry["hash"] = hashlib.sha256(tool.read_bytes()).hexdigest()
        (copy / "CHECKSUMS.json").write_text(json.dumps(listed))
    writer("import os, sys\nsys.stderr.write('env ' + ','.join(sorted(os.environ)) + ' isolated ' + str(sys.flags.isolated) + ' cwd ' + os.getcwd())\nsys.exit(1)\n")
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 4})
    found = re.search(r"env (\S*) isolated (\d) cwd (\S+)", said or "")
    check(found and set(found.group(1).split(",")) <= {"PATH", "LC_CTYPE"} and "PATH" in found.group(1) and found.group(2) == "1" and
          "dgx-ui-picture-" in found.group(3) and not os.path.exists(found.group(3)), f"the environment of PATH alone, -I, a fresh folder gone after: {said}")
    writer("import sys\nwhile True: sys.stderr.write('x' * 65536)\n")
    serve.WRITER_S = 3
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 4})
    check(made is None and said == "DGX's writer did not finish in 3 s: the picture is not made", f"one printing without end cut at its time: {said[:200]}")
    writer("import sys\nsys.stderr.write('y' * 5000000)\nsys.exit(1)\n")
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 4})
    check(made is None and len(said) <= serve.WRITER_SAID and said.startswith("yyy"), f"what it printed bounded: {len(said or '')} characters")
    writer("import time\ntime.sleep(60)\n")
    made, said = serve.make_picture(root, {"pngs": [logo], "bits": 4})
    check(made is None and "did not finish in 3 s" in said, f"a hang cut at its time: {said}")

    # the size read of the PNG's head before anything is written or run: a writer that marks it ran, the folders made
    # counted; a head alone, IHDR, of no pixels
    ran = work / "ran"
    writer(f"open({str(ran)!r}, 'w').write('ran')\nimport sys\nsys.exit(1)\n")
    folders = []
    made_folder = serve.tempfile.TemporaryDirectory

    def counted(*given, **named):
        folders.append(named.get("prefix"))
        return made_folder(*given, **named)
    serve.tempfile.TemporaryDirectory = counted

    def head(w, h):
        ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
        return base64.b64encode(b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 13) + b"IHDR" + ihdr + struct.pack(">I", zlib.crc32(b"IHDR" + ihdr))).decode()
    for asked, words in (([head(20000, 20000)], "a picture of 20000x20000: the builder takes pictures of up to 4096 pixels a side and 4 Mpx in all"),
                         ([head(16, 4097)], "a picture of 16x4097: "),
                         ([head(300, 300)] * 64, "64 pictures of 5760000 pixels in all: the builder takes"),
                         ([base64.b64encode(b"\x89PNG\r\n\x1a\n" + struct.pack(">I", 0) + b"IEND").decode()], "its head, IHDR, is not its first part")):
        made, said = serve.make_picture(root, {"pngs": asked, "bits": 4})
        check(made is None and said and words in said and not ran.exists() and not folders,
              f"{words}: refused before a file is written or the writer runs: {said}, ran {ran.exists()}, folders {folders}")
    made, said = serve.make_picture(root, {"pngs": [head(4096, 1024)], "bits": 4})
    check(ran.exists() and folders, f"4096x1024, 4 Mpx and 4096 a side: the writer runs: {said}")
    serve.tempfile.TemporaryDirectory = made_folder
    tool.write_bytes(original)
    (copy / "CHECKSUMS.json").write_bytes(sums)

    # the server: the picture made over HTTP, PNG files written, files read
    process = subprocess.Popen([sys.executable, str(SERVER), "--project", str(project), "--port", "0"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    started.append(process)
    port = token = None
    for _ in range(10):
        line = process.stdout.readline()
        found = re.match(r"http://127\.0\.0\.1:(\d+)/editor/#project=([\w-]+)$", line.strip())
        if found:
            port, token = int(found.group(1)), found.group(2)
            break

    def call(method, path, body=None, origin=True):
        connection = http.client.HTTPConnection("127.0.0.1", port, timeout=60)
        headers = {"Host": f"127.0.0.1:{port}", "Authorization": f"Bearer {token}"}
        if origin:
            headers["Origin"] = f"http://127.0.0.1:{port}"
        if body is not None:
            headers["Content-Length"] = str(len(body))
        connection.request(method, path, body=body, headers=headers)
        answer = connection.getresponse()
        result = answer.status, answer.read()
        connection.close()
        return result
    check(port is not None, "the server started")
    status, body = call("POST", "/project/image", json.dumps({"pngs": [logo], "bits": 8}).encode())
    told = json.loads(body) if status == 200 else {}
    check(status == 200 and base64.b64decode(told.get("dgxi", ""))[:4] == b"DGXI" and told.get("length") == len(base64.b64decode(told["dgxi"])), f"made over HTTP: {status} {body[:200]}")
    status, body = call("POST", "/project/image", b"not json")
    check(status == 400, "a request not JSON refused")
    status, body = call("POST", "/project/binary?path=main/images/new.png", png(2, 2, (1, 2, 3, 255)))
    check(status == 200 and (project / "main" / "images" / "new.png").exists(), f"a PNG into images/ made beside the needs: {body}")
    for path, data, words in (("main/images/x.png", b"GIF89a....", "as a PNG"), ("main/images/x.txt", png(1, 1, (0, 0, 0, 255)), "as a PNG"),
                              ("pages/x.png", png(1, 1, (0, 0, 0, 255)), "beside the file of pictures' needs"),
                              ("main/art/other.png", png(1, 1, (0, 0, 0, 255)), "no need names it"),
                              ("main/../x.png", png(1, 1, (0, 0, 0, 255)), "no . and no .."),
                              ("build/x.png", png(1, 1, (0, 0, 0, 255)), "under build/ or managed_components/"),
                              ("main/deep/x.png", png(1, 1, (0, 0, 0, 255)), "a folder that is there")):
        status, body = call("POST", "/project/binary?path=" + path, data)
        check(status == 400 and words in body.decode(), f"{path}: refused with \"{words}\": {status} {body}")
    status, body = call("POST", "/project/binary?path=main/art/logo.png", png(3, 3, (9, 9, 9, 255)))
    check(status == 200 and (project / "main" / "art" / "logo.png").read_bytes() == png(3, 3, (9, 9, 9, 255)), f"a PNG a need names written over: {body}")
    status, body = call("POST", "/project/binary?path=main/images/new2.png", png(2, 2, (1, 2, 3, 255)), origin=False)
    check(status == 403 and not (project / "main" / "images" / "new2.png").exists(), "a PNG from another page refused")
    status, body = call("POST", "/project/file?path=main/images.txt", b"logo PNG art/logo.png BITS 1\n")
    check(status == 200 and (project / "main" / "images.txt").read_text() == "logo PNG art/logo.png BITS 1\n", f"the file of needs written: {body}")
    for path, held in (("main/images.txt", True), ("main/art/logo.png", True), ("build/dgx_images/dgx_images-files.txt", True),
                       ("build/dgx_images/logo.dgxi", True), ("build/dgx_images/stray.dgxi", False), ("main/art/other.png", False)):
        status, body = call("GET", "/project/file?path=" + path)
        check((status == 200) == held, f"{path} read: {held}, {status} {body[:100]}")
finally:
    for process in started:
        process.kill()
        process.wait(timeout=10)
        process.stdout.close()
    shutil.rmtree(work, ignore_errors=True)

if failures:
    print(f"test_serve_pictures.py: {len(failures)} of {checks} checks failed", file=sys.stderr)
    sys.exit(1)
print(f"test_serve_pictures.py: {checks} checks held")
