#!/usr/bin/env python3
"""
test_mcp_mounted.py: the builder's agent for another server whose page hosts the builder, tools/dgx_ui_mcp.py's
mounted_agent(), as boards' tools/boards_mcp.py takes it. A plain ThreadingHTTPServer of the test gives the builder's
handle() (dgx_ui_serve.py's mounted()) and the agent's handle() every request first, under the prefix /x, and answers
418 itself for what they leave; a page of the test, in a thread, asks /x/agent/updates and answers /x/agent/answer as
editor/agent.js does. Held: the host's tools are the builder's six, named ui_ and theirs, page_link, wait_for_person and
say not among them, the why of an edit described as shown in the hosting page's dialog; a call runs through the page
and comes back as a result of MCP, an image as an image; an edit's why goes to said() before the page answers, and a
said() that raises leaves the edit done, the result saying the why was not shown; an argument the schema refuses, a
tool not given, a page not open, a page that never answers, each said in words, no hosted text naming a tool the host
has not; the page's calls by this Host, the token, a POST from this origin
alone; /xy/agent/... and /agent/... left to the server.
"""
import base64
import http.client
import http.server
import importlib.util
import json
import pathlib
import socket
import sys
import tempfile
import threading
import time

here = pathlib.Path(__file__).resolve().parent
TOOLS_DIR = here.parent.parent / "tools"
sys.path.insert(0, str(TOOLS_DIR))
failures = []
checks = 0


def check(held, what):
    global checks
    checks += 1
    if not held:
        failures.append(what)
    return held


spec = importlib.util.spec_from_file_location("dgx_ui_mcp", TOOLS_DIR / "dgx_ui_mcp.py")
mcp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mcp)
serve = mcp.serve
mcp.ANSWER_S = 3  # a page that does not answer is given up sooner here

work = tempfile.TemporaryDirectory()
project = pathlib.Path(work.name) / "proj"
(project / "main").mkdir(parents=True)
(project / "CMakeLists.txt").write_text("dgx_ui_tree(main/page.json)\n")
(project / "main" / "page.json").write_text('{"dgx-ui": 1, "tree": {"kind": "box"}}')
with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port = probe.getsockname()[1]
token = "t0ken"
shown, order = [], []


def said(text):
    order.append("said")
    shown.append(text)


tools, hosted_call, agent_handle = mcp.mounted_agent(str(project), token, port, "/x", "ui_", said)
texts = [json.dumps(tools)]  # every hosted text: the tools' schemas and every result, none naming a tool the host has not


def call(name, arguments):
    result = hosted_call(name, arguments)
    texts.extend(part.get("text", "") for part in result["content"] if name.startswith("ui_") and name[3:] in mcp.HOSTED)
    return result
builder_handle = serve.mounted(str(project), token, port, "/x")


class Theirs(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_):
        pass

    def own(self):
        self.send_response(418)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self):
        if not (agent_handle(self) or builder_handle(self)):
            self.own()

    def do_POST(self):
        if not (agent_handle(self) or builder_handle(self)):
            self.own()


server = http.server.ThreadingHTTPServer(("127.0.0.1", port), Theirs)
server.daemon_threads = True
threading.Thread(target=server.serve_forever, daemon=True).start()
HOST, ORIGIN = f"127.0.0.1:{port}", f"http://127.0.0.1:{port}"


def ask(method, path, body=None, host=HOST, auth=True, origin=ORIGIN):
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=40)
    headers = {"Host": host}
    if auth:
        headers["Authorization"] = f"Bearer {token}"
    if origin:
        headers["Origin"] = origin
    data = json.dumps(body).encode() if body is not None else None
    if data is not None:
        headers["Content-Length"] = str(len(data))
    connection.putrequest(method, path, skip_host=True)
    for name, value in headers.items():
        connection.putheader(name, value)
    connection.endheaders(data)
    response = connection.getresponse()
    answer = response.read()
    connection.close()
    return response.status, answer


# the host's tools: the builder's six, by the host's names
names = [tool["name"] for tool in tools]
check(names == ["ui_page_state", "ui_page_image", "ui_read_file", "ui_edit_file", "ui_select", "ui_formats"], f"the tools: {names}")
edit = next(tool for tool in tools if tool["name"] == "ui_edit_file")
check("hosts the builder" in edit["inputSchema"]["properties"]["why"]["description"], "the why: shown in the hosting page's dialog")
check(all("dialog" not in tool["description"] for tool in tools), "no tool tells of the builder's own dialog")

# no page yet: said in words a host can give
result = call("ui_page_state", {})
check(result.get("isError") and "open its tab" in result["content"][0]["text"], f"no page open: {result}")
for name, arguments in (("ui_say", {"text": "x"}), ("page_link", {}), ("ui_wait_for_person", {})):
    result = call(name, arguments)
    check(result.get("isError") and f"no tool {name}" in result["content"][0]["text"], f"{name} not given: {result}")
result = call("ui_edit_file", {"path": "main/page.json", "text": "{}"})
check(result.get("isError") and "why" in result["content"][0]["text"], f"an argument missing, said: {result}")

# a page of the test: asks for what the agent wants and answers each request
PNG = base64.b64encode(b"\x89PNG\r\n\x1a\n").decode()
stop = threading.Event()


def page():
    requests = 0
    while not stop.is_set():
        status, body = ask("GET", f"/x/agent/updates?messages=0&requests={requests}")
        if status != 200:
            time.sleep(0.2)
            continue
        updates = json.loads(body)
        for request in updates["requests"]:
            order.append("page")
            value = {"png": PNG, "width": 320, "height": 240} if request["kind"] == "image" else \
                {"verdicts": {"page": {"taken": True}}} if request["kind"] == "edit" else {"kind": request["kind"], "args": request["args"]}
            ask("POST", "/x/agent/answer", {"id": request["id"], "value": value})
        requests = updates["requested"]


threading.Thread(target=page, daemon=True).start()
for _ in range(50):
    if json.loads(call("ui_page_state", {})["content"][0]["text"]).get("kind") == "state":
        break
    time.sleep(0.1)
result = call("ui_page_state", {})
check(not result.get("isError") and json.loads(result["content"][0]["text"]) == {"kind": "state", "args": {}}, f"ui_page_state through the page: {result}")
result = call("ui_read_file", {"path": "main/page.json"})
check(json.loads(result["content"][0]["text"]) == {"kind": "read", "args": {"path": "main/page.json"}}, f"ui_read_file: {result}")
result = call("ui_page_image", {})
check(result["content"][0] == {"type": "image", "data": PNG, "mimeType": "image/png"}, f"ui_page_image as an image: {result['content'][0]}")

# an edit: the why to said() before the page answers, the verdicts back
order.clear()
result = call("ui_edit_file", {"path": "main/page.json", "text": '{"dgx-ui": 1, "tree": {"kind": "frame"}}', "why": "a frame for the page"})
check(shown == ["a frame for the page"] and order[:2] == ["said", "page"], f"the why shown first: {shown} {order}")
check(json.loads(result["content"][0]["text"]) == {"verdicts": {"page": {"taken": True}}}, f"the verdicts back: {result}")
# a said() that raises: the edit is made, the result says the why was not shown (an agent of its own, on its own server)


class Broken(Theirs):
    def do_GET(self):
        if not handle2(self):
            self.own()

    def do_POST(self):
        if not handle2(self):
            self.own()


with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port2 = probe.getsockname()[1]
tools2, call2, handle2 = mcp.mounted_agent(str(project), token, port2, "/y", "ui_", lambda text: 1 / 0)
broken = http.server.ThreadingHTTPServer(("127.0.0.1", port2), Broken)
broken.daemon_threads = True
threading.Thread(target=broken.serve_forever, daemon=True).start()


def page2():
    requests = 0
    while not stop.is_set():
        connection = http.client.HTTPConnection("127.0.0.1", port2, timeout=40)
        connection.request("GET", f"/y/agent/updates?messages=0&requests={requests}", headers={"Host": f"127.0.0.1:{port2}", "Authorization": f"Bearer {token}"})
        updates = json.loads(connection.getresponse().read())
        for request in updates["requests"]:
            body = json.dumps({"id": request["id"], "value": {"verdicts": {"page": {"taken": True}}}}).encode()
            answer = http.client.HTTPConnection("127.0.0.1", port2, timeout=10)
            answer.request("POST", "/y/agent/answer", body=body, headers={"Host": f"127.0.0.1:{port2}", "Authorization": f"Bearer {token}",
                                                                           "Origin": f"http://127.0.0.1:{port2}", "Content-Length": str(len(body))})
            answer.getresponse().read()
        requests = updates["requested"]


threading.Thread(target=page2, daemon=True).start()
time.sleep(0.5)
result = call2("ui_edit_file", {"path": "main/page.json", "text": "{}", "why": "w"})
value = json.loads(result["content"][0]["text"])
check(value.get("verdicts") == {"page": {"taken": True}} and "the why was not shown" in value.get("unshown", "") and "division by zero" in value["unshown"],
      f"a said() that raises: the edit made, the why said not shown: {value}")

# a page that asks and never answers: given up after ANSWER_S, in the hosted words
with socket.socket() as probe:
    probe.bind(("127.0.0.1", 0))
    port3 = probe.getsockname()[1]
tools3, call3, handle3 = mcp.mounted_agent(str(project), token, port3, "/z", "ui_", said)


class Silent(Theirs):
    def do_GET(self):
        if not handle3(self):
            self.own()

    def do_POST(self):
        if not handle3(self):
            self.own()


silent = http.server.ThreadingHTTPServer(("127.0.0.1", port3), Silent)
silent.daemon_threads = True
threading.Thread(target=silent.serve_forever, daemon=True).start()


def page3():
    while not stop.is_set():
        connection = http.client.HTTPConnection("127.0.0.1", port3, timeout=40)
        connection.request("GET", "/z/agent/updates?messages=0&requests=0", headers={"Host": f"127.0.0.1:{port3}", "Authorization": f"Bearer {token}"})
        connection.getresponse().read()


threading.Thread(target=page3, daemon=True).start()
time.sleep(0.5)
result = call3("ui_page_state", {})
texts.extend(part.get("text", "") for part in result["content"])
check(result.get("isError") and f"did not answer in {mcp.ANSWER_S} s" in result["content"][0]["text"] and "open its tab" in result["content"][0]["text"],
      f"a page that never answers: the hosted words: {result}")
check(not any(word in text for text in texts for word in ("page_link", "wait_for_person", "Give the person")),
      f"no hosted text names a tool the host has not: {[text for text in texts if 'page_link' in text or 'wait_for_person' in text][:2]}")

# the page's calls: this Host, the token, a POST from this origin; /xy and /agent of no prefix left to the server
check(ask("GET", "/x/agent/updates?messages=0&requests=0", host="localhost:1")[0] == 403, "another Host")
check(ask("GET", "/x/agent/updates?messages=0&requests=0", auth=False)[0] == 401, "no token")
check(ask("POST", "/x/agent/say", {"text": "hi"}, origin="http://example.com")[0] == 403, "a say from another page")
check(ask("POST", "/x/agent/say", {"text": "hi"}, origin=None)[0] == 403, "a say with no Origin")
check(ask("GET", "/x/agent/updates?messages=x&requests=0")[0] == 400, "counts that are not numbers")
check(ask("GET", "/xy/agent/updates?messages=0&requests=0")[0] == 418, "/xy is not of /x")
check(ask("GET", "/agent/updates?messages=0&requests=0")[0] == 418, "/agent of no prefix left to the server")
check(ask("GET", "/x/project")[0] == 200, "the builder's own calls beside the agent's")

stop.set()
server.shutdown(), broken.shutdown(), silent.shutdown()
work.cleanup()
for failure in failures:
    print("  failed: " + failure, file=sys.stderr)
print(f"test_mcp_mounted.py: {checks - len(failures)} of {checks} checks held")
sys.exit(1 if failures else 0)
