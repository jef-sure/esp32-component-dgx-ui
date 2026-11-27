#!/usr/bin/env python3
"""
python3 test_mcp.py: tools/dgx_ui_mcp.py, the builder's server with an agent beside the page, on projects in a temporary
directory, its MCP over stdio as a client speaks it and its page played by a fake one over HTTP.
 - MCP: initialize with the component's version, the tools listed, each call's arguments held to its schema, a method
   unknown, a row not JSON, a notification answered by nothing;
 - the relay: a tool's request goes to the page in order and comes back with what the page answers; the agent's why and
   say shown to the page, what the person writes back to wait_for_person; the page's image given as MCP's image; with no
   page open a tool answers at once, and a page that does not answer is said in time;
 - the page's calls by the token, the Host and, for a POST, the builder's own origin;
 - a folder of no dgx-ui: no tools;
 - --install: to Claude Code by its command, its words checked, with the skill dgx-ui-builder copied beside it, written
   again the same, under CLAUDE_CONFIG_DIR too, one of the person's own not written over; with none, into .mcp.json
   beside the servers there, and no skill;
 - the skill: its frontmatter, its tools the server's both ways and those of the server's instructions among them, the
   sections of the documentation it names there, no path of a machine in it;
 - formats: the version of the copy that runs and the folder of its documentation, with a page and with none;
 - --serve: the builder's link printed, as dgx_ui_serve.py prints it;
 - nothing of a project's own copy of dgx-ui ever run.
"""
import base64
import importlib.util
import re
import http.client
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse

HERE = pathlib.Path(__file__).resolve().parent
SERVER = HERE.parent.parent / "tools" / "dgx_ui_mcp.py"
SKILL = HERE.parent.parent / "tools" / "skills" / "dgx-ui-builder" / "SKILL.md"
DOCS = HERE.parent.parent / "docs" / "en"
failures = checks = 0


def check(held, what):
    global failures, checks
    checks += 1
    if not held:
        print(f"  failed: {what}", file=sys.stderr)
        failures += 1
    return held


class Client:
    """An MCP client over the server's stdio."""

    def __init__(self, project, *extra, env=None):
        self.process = subprocess.Popen([sys.executable, str(SERVER), "--project", str(project), "--port", "0", *extra],
                                        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", env=env)
        self.ident = 0

    def send(self, row):
        self.process.stdin.write(row + "\n")
        self.process.stdin.flush()

    def ask(self, method, params=None, notification=False):
        self.ident += 1
        request = {"jsonrpc": "2.0", "method": method, **({} if notification else {"id": self.ident}), **({"params": params} if params is not None else {})}
        self.send(json.dumps(request))
        return None if notification else json.loads(self.process.stdout.readline())

    def tool(self, name, arguments=None):
        return self.ask("tools/call", {"name": name, "arguments": arguments or {}})

    def close(self):
        self.process.stdin.close()
        self.process.wait(timeout=10)


def value_of(answer):
    return json.loads(answer["result"]["content"][0]["text"])


class FakePage:
    """The builder's page as the server sees it: it asks for what the agent wants and answers it."""

    def __init__(self, link, answers):
        address = urllib.parse.urlsplit(link)
        self.port, self.token = address.port, urllib.parse.parse_qs(address.fragment)["project"][0]
        self.answers, self.messages, self.requests, self.got, self.stopped = answers, 0, 0, [], False
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def call(self, method, path, body=None, token=True, origin=True, host=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=40)
        headers = {"Host": host or f"127.0.0.1:{self.port}"}
        if token:
            headers["Authorization"] = f"Bearer {self.token}"
        if origin and method == "POST":
            headers["Origin"] = f"http://127.0.0.1:{self.port}"
        data = json.dumps(body).encode() if body is not None else None
        if data is not None:
            headers["Content-Type"], headers["Content-Length"] = "application/json", str(len(data))
        connection.request(method, path, body=data, headers=headers)
        response = connection.getresponse()
        answer = response.status, response.read()
        connection.close()
        return answer

    def run(self):
        while not self.stopped:
            try:
                status, body = self.call("GET", f"/agent/updates?messages={self.messages}&requests={self.requests}")
            except OSError:  # the server stopped
                return
            if status != 200:
                time.sleep(0.2)
                continue
            said = json.loads(body)
            self.got += said["messages"]
            self.messages += len(said["messages"])
            for request in said["requests"]:
                self.got.append(request)
                answer = self.answers(request)
                if answer is not None:
                    self.call("POST", "/agent/answer", {"id": request["id"], "value": answer})
            self.requests = said["requested"]


def project_in(folder, planted=None):
    (folder / "main").mkdir(parents=True)
    (folder / "CMakeLists.txt").write_text("project(demo)\n")
    (folder / "main" / "CMakeLists.txt").write_text("dgx_ui_tree(home.json)\n")
    (folder / "main" / "home.json").write_text('{"dgx-ui": 1, "tree": {"kind": "box"}}\n')
    if planted:
        for name in ("jef-sure__dgx-ui", "dgx-ui"):
            tools = folder / "managed_components" / name / "tools"
            tools.mkdir(parents=True)
            for tool in ("dgx_ui_mcp.py", "dgx_ui_serve.py"):
                (tools / tool).write_text(f"import pathlib\npathlib.Path({str(planted)!r}).write_text('run')\n")
    return folder


def main():
    work = pathlib.Path(tempfile.mkdtemp(prefix="dgx-ui-mcp-"))
    marker = work / "run-marker"
    try:
        project = project_in(work / "demo", planted=marker)
        client = Client(project)

        # MCP
        said = client.ask("initialize", {"protocolVersion": "2025-06-18", "capabilities": {}})
        version = next(line.split('"')[1] for line in (HERE.parent.parent / "idf_component.yml").read_text().splitlines() if line.startswith("version:"))
        check(said["result"]["serverInfo"] == {"name": "dgx-ui", "version": version} and "page_link" in said["result"]["instructions"],
              f"initialize: {said['result']['serverInfo']}")
        check(client.ask("notifications/initialized", notification=True) is None, "a notification answered by nothing")
        tools = [tool["name"] for tool in client.ask("tools/list")["result"]["tools"]]
        check(tools == ["page_link", "page_state", "page_image", "read_file", "edit_file", "select", "formats", "wait_for_person", "say"],
              f"the tools: {tools}")
        for name, arguments, why in (("say", {}, "arguments has no text"), ("say", {"text": 1}, "arguments.text is not a string"),
                                     ("wait_for_person", {"seconds": 0}, "out of"), ("select", {"path": "a", "widget": [-1]}, "out of"),
                                     ("edit_file", {"path": "a", "text": "b"}, "has no why"), ("no_such_tool", {}, "name of a tool")):
            answer = client.tool(name, arguments)
            check(answer.get("error", {}).get("code") == -32602 and why in answer["error"]["message"], f"{name} {arguments}: {answer}")
        check(client.ask("no/such")["error"]["code"] == -32601, "a method unknown")
        client.send("{not json")
        check(json.loads(client.process.stdout.readline())["error"]["code"] == -32700, "a row not JSON")
        check("tools" in client.ask("tools/list")["result"], "answered after it")
        link = value_of(client.tool("page_link"))["link"]
        check(link.startswith("http://127.0.0.1:") and "/editor/#project=" in link and link.endswith("&agent=1"), f"the link: {link}")
        began = time.monotonic()
        answer = client.tool("edit_file", {"path": "main/home.json", "text": "{}", "why": "a try"})
        check(answer["result"].get("isError") and "not open" in value_of(answer)["error"] and time.monotonic() - began < 2,
              f"no page open: said at once: {answer}")
        own = {"version": version, "docs": str(DOCS.resolve())}
        answer = value_of(client.tool("formats"))
        check("not open" in answer.get("error", "") and {key: answer.get(key) for key in own} == own, f"formats with no page: {answer}")

        # the relay, by a page that answers
        png = base64.b64encode(b"\x89PNG\r\n\x1a\n").decode()

        def answers(request):
            if request["kind"] == "edit":
                return {"path": request["args"]["path"], "verdicts": {"page": {"taken": True}}}
            if request["kind"] == "state":
                return {"open": "main/home.json", "verdicts": {"page": {"taken": True}}}
            if request["kind"] == "image":
                return {"png": png, "width": 320, "height": 240}
            if request["kind"] == "read":
                return None  # a page that does not answer
            return {"kind": request["kind"]}

        page = FakePage(link, answers)
        for _ in range(50):
            if client.tool("page_state")["result"].get("isError") is None:
                break
            time.sleep(0.1)
        state = value_of(client.tool("page_state"))
        check(state == {"open": "main/home.json", "verdicts": {"page": {"taken": True}}}, f"page_state from the page: {state}")
        edited = value_of(client.tool("edit_file", {"path": "main/home.json", "text": '{"dgx-ui": 1, "tree": {"kind": "frame"}}', "why": "a frame"}))
        check(edited == {"path": "main/home.json", "verdicts": {"page": {"taken": True}}}, f"edit_file answered by the page: {edited}")
        request = next((one for one in page.got if isinstance(one, dict) and one.get("kind") == "edit"), None)
        check(request and request["args"]["text"] == '{"dgx-ui": 1, "tree": {"kind": "frame"}}', f"the page asked the whole text: {request}")
        check({"from": "agent", "text": "a frame"} in page.got, "the edit's why shown to the person")
        image = client.tool("page_image")["result"]["content"]
        check(image[0] == {"type": "image", "data": png, "mimeType": "image/png"} and json.loads(image[1]["text"]) == {"width": 320, "height": 240},
              f"the panel as MCP's image: {image[1]}")
        check(value_of(client.tool("select", {"path": "main/home.json", "widget": [0]})) == {"kind": "select"}, "select")
        check(value_of(client.tool("formats")) == {"kind": "formats", **own} and (DOCS / "api.md").is_file(), "formats: the page's, the version and the docs")
        kinds = [one["kind"] for one in page.got if isinstance(one, dict) and "kind" in one]
        check(kinds == ["state", "state", "edit", "image", "select", "formats"] or kinds[-4:] == ["edit", "image", "select", "formats"],
              f"the requests in their order: {kinds}")
        client.tool("say", {"text": "Hello, person"})
        time.sleep(0.5)
        check({"from": "agent", "text": "Hello, person"} in page.got, "say shown on the page")
        check(page.call("POST", "/agent/say", {"text": "a frame, please"}) == (200, b"{}"), "the person writes")
        check(value_of(client.tool("wait_for_person", {"seconds": 5})) == {"messages": ["a frame, please"]}, "wait_for_person gives it")
        began = time.monotonic()
        check(value_of(client.tool("wait_for_person", {"seconds": 1})) == {"messages": []} and time.monotonic() - began >= 0.9, "nothing more in a second")
        began = time.monotonic()
        answer = client.tool("read_file", {"path": "main/home.json"})
        check(answer["result"].get("isError") and "did not answer" in value_of(answer)["error"] and 9 <= time.monotonic() - began < 15,
              f"a page that does not answer: said in time: {answer}")

        # a page opened again after the edits, as a reload, a second tab or the link opened again opens it: none of the
        # requests answered or given up comes to it, nor goes a late answer of one through; a new request comes to it
        check(page.call("POST", "/agent/answer", {"id": request["id"], "value": {"late": True}}) == (200, b"{}"), "a late answer taken quietly")
        page.stopped = True
        again = FakePage(link, answers)
        time.sleep(1)
        check(not [one for one in again.got if isinstance(one, dict) and "kind" in one], f"no past request to a page opened again: {again.got}")
        check({"from": "agent", "text": "Hello, person"} in again.got, "the dialog shown to it")
        check(value_of(client.tool("select", {"path": "main/home.json", "widget": [0]})) == {"kind": "select"}, "a new request answered by it")
        kinds = [one["kind"] for one in again.got if isinstance(one, dict) and "kind" in one]
        check(kinds == ["select"], f"only the new request came to it: {kinds}")
        page = again

        # the page's calls by the token, the Host and its own origin
        check(page.call("GET", "/agent/updates?messages=0&requests=0", token=False)[0] == 401, "updates with no token refused")
        check(page.call("GET", "/agent/updates?messages=0&requests=0", host="evil.example")[0] == 403, "updates by another Host refused")
        check(page.call("GET", "/agent/updates?messages=x&requests=0")[0] == 400, "updates of no count refused")
        check(page.call("POST", "/agent/say", {"text": "x"}, origin=False)[0] == 403, "a message from another page refused")
        check(page.call("POST", "/agent/say", {"text": "x"}, token=False)[0] == 401, "a message with no token refused")
        check(page.call("POST", "/agent/say", {"words": "x"})[0] == 400 and page.call("POST", "/agent/answer", {"id": "1"})[0] == 400,
              "a body of the wrong form refused")
        status, body = page.call("GET", "/editor/index.html", token=False)
        check(status == 200 and b"start.js" in body, "the builder's page served beside the agent")
        status, body = page.call("GET", "/project/file?path=main/home.json")
        check(status == 200 and b"dgx-ui" in body, "the project read by the rules of dgx_ui_serve.py")
        check(page.call("POST", "/project/file?path=CMakeLists.txt", "spoiled")[0] == 400, "and written by them")
        page.stopped = True
        client.close()

        # the server under a code page that holds no Cyrillic, as Windows with no UTF-8 mode runs it: rows in UTF-8 both ways
        coded = Client(project, env={**os.environ, "PYTHONIOENCODING": "cp1252"})
        coded_page = FakePage(value_of(coded.tool("page_link"))["link"], answers)
        check(value_of(coded.tool("say", {"text": "Рамка — готово"})) is not None, "the agent's Cyrillic taken")
        time.sleep(0.5)
        check({"from": "agent", "text": "Рамка — готово"} in coded_page.got, "and shown on the page as it was")
        check(coded_page.call("POST", "/agent/say", {"text": "Рамку, пожалуйста"}) == (200, b"{}"), "the person writes in Cyrillic")
        check(value_of(coded.tool("wait_for_person", {"seconds": 5})) == {"messages": ["Рамку, пожалуйста"]}, "the agent reads it as it was")
        coded_page.stopped = True
        coded.close()

        # a folder of no dgx-ui: no tools
        empty = work / "empty"
        empty.mkdir()
        (empty / "notes.json").write_text('{"a": 1}\n')
        idle = Client(empty)
        said = idle.ask("initialize", {})
        check("offers nothing here" in said["result"]["instructions"] and idle.ask("tools/list")["result"]["tools"] == [], "a folder of no dgx-ui: no tools")
        idle.close()
        loose = work / "loose"
        (loose / "pages").mkdir(parents=True)
        (loose / "pages" / "a.json").write_text('{"dgx-ui": 1, "tree": {"kind": "box"}}\n')
        found = Client(loose)
        check(len(found.ask("tools/list")["result"]["tools"]) == 9, "a folder of a description and no call: the tools")
        found.close()

        # --install: by Claude Code's command, or into .mcp.json
        home, bin_dir = work / "home", work / "bin"
        home.mkdir(), bin_dir.mkdir()
        argv_file = work / "claude-argv"
        (bin_dir / "claude").write_text(f"#!{sys.executable}\nimport json, sys\nopen({str(argv_file)!r}, 'w').write(json.dumps(sys.argv[1:]))\nprint('added')\n")
        (bin_dir / "claude").chmod(0o755)
        environment = {**os.environ, "HOME": str(home), "PATH": f"{bin_dir}{os.pathsep}/usr/bin:/bin"}
        environment.pop("CLAUDE_CONFIG_DIR", None)
        done = subprocess.run([sys.executable, str(SERVER), "--install"], cwd=work, env=environment, capture_output=True, text=True, timeout=30)
        check(done.returncode == 0 and json.loads(argv_file.read_text()) == ["mcp", "add", "-s", "user", "dgx-ui", "--", sys.executable, str(SERVER.resolve())],
              f"--install by Claude Code's command: {argv_file.read_text() if argv_file.exists() else done.stderr}")
        # the skill beside it: the package's, the version under its title; again the same; under CLAUDE_CONFIG_DIR; not over another's
        installed = home / ".claude" / "skills" / "dgx-ui-builder" / "SKILL.md"
        source = skill_of(SKILL.read_text(encoding="utf-8"), hosted=False)  # this server's own lines, no marker
        title = re.search(r"^# .*\n", source, re.M)
        expected = source[:title.end()] + f"\nWritten by dgx_ui_mcp.py --install of dgx-ui {version}.\n" + source[title.end():]
        check(installed.exists() and installed.read_text(encoding="utf-8") == expected and str(installed) in done.stdout,
              f"--install: the skill: {done.stdout} {installed.read_text()[:300] if installed.exists() else 'none'}")
        written = installed.stat().st_mtime_ns if installed.exists() else 0
        time.sleep(0.05)
        done = subprocess.run([sys.executable, str(SERVER), "--install"], cwd=work, env=environment, capture_output=True, text=True, timeout=30)
        check(done.returncode == 0 and installed.exists() and installed.read_text(encoding="utf-8") == expected and installed.stat().st_mtime_ns == written,
              f"--install again: the skill the same, not written: {done.stdout}")
        installed.write_text(expected.replace(f"dgx-ui {version}.", "dgx-ui 0.0.1."), encoding="utf-8")
        done = subprocess.run([sys.executable, str(SERVER), "--install"], cwd=work, env=environment, capture_output=True, text=True, timeout=30)
        check(done.returncode == 0 and installed.read_text(encoding="utf-8") == expected, f"--install over an older copy: {done.stdout}")
        config_dir = work / "claude-config"
        done = subprocess.run([sys.executable, str(SERVER), "--install"], cwd=work, env={**environment, "CLAUDE_CONFIG_DIR": str(config_dir)},
                              capture_output=True, text=True, timeout=30)
        check(done.returncode == 0 and (config_dir / "skills" / "dgx-ui-builder" / "SKILL.md").read_text(encoding="utf-8") == expected,
              f"--install under CLAUDE_CONFIG_DIR: {done.stdout}")
        own_skill = "---\nname: dgx-ui-builder\ndescription: the person's own\n---\n# Mine\n"
        installed.write_text(own_skill, encoding="utf-8")
        done = subprocess.run([sys.executable, str(SERVER), "--install"], cwd=work, env=environment, capture_output=True, text=True, timeout=30)
        check(done.returncode == 1 and installed.read_text(encoding="utf-8") == own_skill and "left as it is" in done.stdout,
              f"--install: a skill of the person's own not written over: {done.returncode} {done.stdout}")
        # in a project of jef-sure/boards, by each of its signs: nothing given, boards' server named, its copy's tool when it
        # has one; --standalone gives it there all the same
        signs = {"copy": ("managed_components/jef-sure__boards/tools/boards_mcp.py", "python3 managed_components/jef-sure__boards/tools/boards_mcp.py --install"),
                 "bare copy": ("managed_components/jef-sure__boards/idf_component.yml", "the MCP server of jef-sure/boards"),
                 "local": ("components/local-boards/CMakeLists.txt", "the MCP server of jef-sure/boards"),
                 "preset": ("CMakePresets.json", "the MCP server of jef-sure/boards")}
        for sign, (file, named) in signs.items():
            boards = work / f"boards-{sign.replace(' ', '-')}"
            (boards / file).parent.mkdir(parents=True, exist_ok=True)
            (boards / file).write_text('{"configurePresets": [{"name": "default", "cacheVariables": {"BOARD": "cyd"}}]}\n' if sign == "preset" else "#\n")
            fresh = boards / "home"
            fresh.mkdir()
            argv_file.unlink(missing_ok=True)
            done = subprocess.run([sys.executable, str(SERVER), "--install"], cwd=boards, env={**environment, "HOME": str(fresh)}, capture_output=True, text=True, timeout=30)
            check(done.returncode == 0 and "Not installed beside it" in done.stdout and named in done.stdout and "--standalone" in done.stdout and
                  not argv_file.exists() and not (fresh / ".claude").exists() and not (boards / ".mcp.json").exists(),
                  f"--install in a project of boards ({sign}): nothing given, boards named: {done.returncode} {done.stdout}")
        done = subprocess.run([sys.executable, str(SERVER), "--install", "--standalone"], cwd=work / "boards-copy",
                              env={**environment, "HOME": str(work / "boards-copy" / "home")}, capture_output=True, text=True, timeout=30)
        check(done.returncode == 0 and argv_file.exists() and (work / "boards-copy" / "home" / ".claude" / "skills" / "dgx-ui-builder" / "SKILL.md").exists(),
              f"--install --standalone in a project of boards: given: {done.stdout}")
        (bin_dir / "claude").unlink()
        folder = work / "agent-folder"
        folder.mkdir()
        (folder / ".mcp.json").write_text(json.dumps({"mcpServers": {"boards": {"command": "x", "args": []}}}))
        done = subprocess.run([sys.executable, str(SERVER), "--install"], cwd=folder, env={**environment, "PATH": "/usr/bin:/bin"},
                              capture_output=True, text=True, timeout=30)
        config = json.loads((folder / ".mcp.json").read_text())
        check(done.returncode == 0 and set(config["mcpServers"]) == {"boards", "dgx-ui"} and
              config["mcpServers"]["dgx-ui"] == {"command": sys.executable, "args": [str(SERVER.resolve())]}, f"--install into .mcp.json: {config}")
        check(not (folder / ".claude").exists() and str(SKILL.resolve()) in done.stdout, f"--install into .mcp.json: no skill written, its path said: {done.stdout}")

        # --serve: the builder's link, as dgx_ui_serve.py prints it
        served = subprocess.Popen([sys.executable, str(SERVER), "--serve", "--project", str(project), "--port", "0"], stdout=subprocess.PIPE, text=True)
        lines = [served.stdout.readline() for _ in range(2)]
        line = next((one for one in lines if one.startswith("http")), "")
        check(line.startswith("http://127.0.0.1:") and "/editor/#project=" in line and "agent" not in line and
              any("this server's own page" in one for one in lines), f"--serve prints the note of the copy and the link: {lines}")
        served.terminate()
        served.wait(timeout=10)
        check(not marker.exists(), "nothing of the project's own copy ever run")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    check_skill()
    if failures:
        print(f"test_mcp.py: {failures} of {checks} checks failed", file=sys.stderr)
        return 1
    print(f"test_mcp.py: {checks} of {checks} checks held")
    return 0


def skill_of(text, hosted):
    """The skill's text for one way of serving, by the markers: the other's blocks dropped, the markers too."""
    other = "standalone" if hosted else "hosted"
    text = re.sub(rf"^<!-- {other} -->\n.*?^<!-- /{other} -->\n", "", text, flags=re.M | re.S)
    return re.sub(r"^<!-- /?(?:standalone|hosted) -->\n", "", text, flags=re.M)


def check_skill():
    """The skill: its frontmatter, the server's tools in it both ways, the instructions' among them, the sections it names
    of the documentation, no path of a machine"""
    raw = SKILL.read_text(encoding="utf-8")
    text = skill_of(raw, hosted=False)
    front = re.match(r"---\n(.*?)\n---\n", text, re.S)
    keys = dict(line.split(": ", 1) for line in front.group(1).splitlines()) if front else {}
    check(set(keys) == {"name", "description"} and keys["name"] == SKILL.parent.name and re.fullmatch(r"[a-z0-9-]{1,64}", keys["name"])
          and 0 < len(keys["description"]) <= 1024 and "<" not in keys["description"], f"the skill's frontmatter: {keys}")
    spec = importlib.util.spec_from_file_location("dgx_ui_mcp", SERVER)
    server = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(server)
    tools = {tool["name"] for tool in server.TOOLS}
    # every tool of the server named in the skill; every step of its order of work begins with tools of the server
    order = text.split("## The order of work", 1)[-1].split("\n## ", 1)[0]
    steps = re.findall(r"^\d+\. `([a-z_]+)`", order, re.M)
    check(tools <= set(re.findall(r"`([a-z_]+)`", text)), f"the server's tools not in the skill: {sorted(tools - set(re.findall(r'`([a-z_]+)`', text)))}")
    check(len(steps) >= 5 and set(steps) <= tools, f"the skill's order of work, by the server's tools: {steps}")
    check(set(re.findall(r"\b([a-z]+_[a-z_]+)\b", server.INSTRUCTIONS)) <= tools, "the instructions' tools are the server's")
    headings = {}
    for page in ("api.md", "cookbook.md"):
        check((DOCS / page).is_file(), f"the skill's {page} in docs/en")
        headings[page] = set(re.findall(r"^#+ (.*)$", (DOCS / page).read_text(encoding="utf-8"), re.M)) if (DOCS / page).is_file() else set()
    for page, section in re.findall(r"`(\w+\.md)`, section \"([^\"]+)\"", text):
        check(section in headings.get(page, set()), f"the skill's section {page} \"{section}\"")
    check(not re.search(r"/home/|/Users/|[A-Z]:\\|~/|managed_components/|\.espressif", text), "no path of a machine in the skill")
    # the markers: each block closed, none inside another
    check(re.sub(r"^<!-- (standalone|hosted) -->\n(?:(?!<!--).*\n)*?<!-- /\1 -->\n", "", raw, flags=re.M).count("<!--") == 0, "the skill's markers in pairs")
    # the part for a server that hosts the builder: no frontmatter, the hosted lines, the six tools by the host's names and no
    # other word touched, the headings a level down, no marker; a tool the host has not, never named
    part = server.skill_part("ui_")
    hosted_names = {tool["name"] for tool in server.mounted_agent(str(HERE), "t", 1, "/x", "ui_")[0]}
    check(not part.startswith("---") and part.startswith("## ") and "<!--" not in part and "<!--" not in server.skill_text(False), "skill_part: no frontmatter, no marker")
    check(set(re.findall(r"`(ui_[a-z_]+)`", part)) == hosted_names, f"skill_part: every builder's tool by its host's name: {sorted(set(re.findall(r'`(ui_[a-z_]+)`', part)))}")
    check(not set(re.findall(r"`([a-z_]+)`", part)) & tools, f"skill_part: no tool by its own name: {sorted(set(re.findall(r'`([a-z_]+)`', part)) & tools)}")
    undone = re.sub(r"^#(#+ )", r"\1", re.sub(r"`ui_([a-z_]+)`", r"`\1`", part), flags=re.M)
    hosted = re.sub(r"\A---\n.*?\n---\n+", "", skill_of(raw, hosted=True), count=1, flags=re.S)
    check(undone == hosted, "skill_part: nothing but the names and the headings changed")
    check(server.skill_text(False) == text, "the skill installed: this server's lines alone")


if __name__ == "__main__":
    sys.exit(main())
