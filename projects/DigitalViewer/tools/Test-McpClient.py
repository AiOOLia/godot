"""Independent wire + installed Codex MCP checks; never prints credentials.

Uses only the Python standard library. Launches isolated validation processes;
does not change the user's Codex config or run model turns.
"""
import argparse
import contextlib
import ctypes
from ctypes import wintypes
import http.client
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import tempfile
import threading
import time


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


class ChildJob:
    """Keep Codex helper processes scoped to this Windows test invocation."""
    def __init__(self, process):
        self.handle = None
        if os.name != "nt":
            return
        class BasicLimits(ctypes.Structure):
            _fields_ = [("process_time", ctypes.c_int64), ("job_time", ctypes.c_int64),
                        ("flags", wintypes.DWORD), ("minimum", ctypes.c_size_t),
                        ("maximum", ctypes.c_size_t), ("processes", wintypes.DWORD),
                        ("affinity", ctypes.c_size_t), ("priority", wintypes.DWORD),
                        ("scheduling", wintypes.DWORD)]
        class Limits(ctypes.Structure):
            _fields_ = [("basic", BasicLimits), ("io", ctypes.c_uint64 * 6),
                        ("process_memory", ctypes.c_size_t), ("job_memory", ctypes.c_size_t),
                        ("peak_process", ctypes.c_size_t), ("peak_job", ctypes.c_size_t)]
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.CreateJobObjectW.restype = wintypes.HANDLE
        self.kernel.SetInformationJobObject.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD]
        self.kernel.AssignProcessToJobObject.argtypes = [wintypes.HANDLE, wintypes.HANDLE]
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.handle = self.kernel.CreateJobObjectW(None, None)
        limits = Limits()
        limits.basic.flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        require(self.handle and self.kernel.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)), "Cannot scope Codex test helpers")
        require(self.kernel.AssignProcessToJobObject(self.handle, int(process._handle)), "Cannot assign Codex test process to job")

    def close(self):
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None


@contextlib.contextmanager
def scratch_directory(parent):
    directory = Path(tempfile.mkdtemp(prefix="check-", dir=parent)).resolve()
    require(directory.is_relative_to(parent.resolve()), "Test cleanup escaped scratch root")
    try:
        yield directory
    finally:
        for attempt in range(20):
            try:
                shutil.rmtree(directory)
                break
            except PermissionError:
                if attempt == 19:
                    raise
                time.sleep(0.1)


class Mcp:
    def __init__(self, token, port):
        self.token, self.port, self.session = token, port, None

    def request(self, method, rpc_id=0, params=None):
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        headers = {"Authorization": "Bearer " + self.token,
                   "Content-Type": "application/json",
                   "Accept": "application/json, text/event-stream"}
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        body = json.dumps({"jsonrpc": "2.0", "id": rpc_id, "method": method,
                           "params": params or {}})
        try:
            connection.request("POST", "/mcp", body, headers)
            response = connection.getresponse()
            require(response.status == 200, f"MCP HTTP status {response.status}")
            result = json.loads(response.read())
            if method == "initialize":
                self.session = response.getheader("Mcp-Session-Id")
            require(type(result.get("id")) is type(rpc_id) and result["id"] == rpc_id,
                    "JSON-RPC response ID type/value changed")
            require("error" not in result, "MCP returned a protocol error")
            return result["result"]
        finally:
            connection.close()


@contextlib.contextmanager
def viewer(executable, directory):
    environment = os.environ.copy()
    environment["DV_TEST_DIRECTORY"] = str(directory)
    with (directory / "viewer.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(executable), "--headless", "--quit-after", "7200",
                                    "--", "--dv-mcp-check"], env=environment,
                                   stdout=log, stderr=log,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        try:
            token_file = directory / "mcp/bearer_token"
            deadline = time.monotonic() + 15
            while time.monotonic() < deadline:
                require(process.poll() is None, "Validation application exited during startup")
                if token_file.exists():
                    token = token_file.read_text().strip()
                    try:
                        client = Mcp(token, 18731)
                        client.request("initialize", params={"protocolVersion": "2025-03-26",
                                       "capabilities": {}, "clientInfo": {"name": "python-check", "version": "1"}})
                        yield client
                        return
                    except (ConnectionError, OSError):
                        pass
                time.sleep(0.1)
            raise RuntimeError("Validation MCP startup timed out")
        finally:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=10)


def codex_check(executable, directory, token, port):
    codex_directory = directory / "codex-home"
    codex_directory.mkdir(exist_ok=True)
    (codex_directory / "config.toml").write_text(
        '[mcp_servers.digitalviewer]\n'
        f'url = "http://127.0.0.1:{port}/mcp"\n'
        'bearer_token_env_var = "DV_MCP_TEST_TOKEN"\n', encoding="utf-8")
    environment = os.environ.copy()
    # A separate Codex home isolates test threads/settings from the user's app.
    environment["CODEX_HOME"] = str(codex_directory)
    environment["DV_MCP_TEST_TOKEN"] = token
    environment["RUST_LOG"] = "error"
    messages = queue.Queue()
    with (directory / "codex.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(executable), "app-server", "--stdio"],
                                   env=environment, cwd=directory, stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=log, text=True,
                                   encoding="utf-8", creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        job = ChildJob(process)
        def reader():
            for line in process.stdout:
                try:
                    messages.put(json.loads(line))
                except json.JSONDecodeError:
                    pass
        threading.Thread(target=reader, daemon=True).start()
        def send(method, params, rpc_id=None):
            message = {"method": method, "params": params}
            if rpc_id is not None:
                message["id"] = rpc_id
            process.stdin.write(json.dumps(message) + "\n")
            process.stdin.flush()
            if rpc_id is None:
                return None
            deadline = time.monotonic() + 40
            while time.monotonic() < deadline:
                try:
                    response = messages.get(timeout=0.5)
                except queue.Empty:
                    require(process.poll() is None, "Codex app server exited")
                    continue
                if response.get("id") == rpc_id:
                    require("error" not in response,
                            "Codex request failed: " + method + " " + str(response.get("error", {}).get("code")))
                    return response["result"]
            raise RuntimeError("Codex timed out: " + method)
        try:
            send("initialize", {"clientInfo": {"name": "digitalviewer_check", "version": "1"},
                                "capabilities": {"experimentalApi": True}}, 1)
            send("initialized", {})
            thread = send("thread/start", {"cwd": str(directory), "ephemeral": True}, 2)
            thread_id = thread["thread"]["id"]
            inventory = send("mcpServerStatus/list", {"threadId": thread_id, "detail": "toolsAndAuthOnly"}, 3)
            servers = inventory["data"]
            server = next(item for item in servers if item["name"] == "digitalviewer")
            require(len(server["tools"]) == 7, "Codex did not load all seven DigitalViewer tools")
            for index, tool in enumerate(["viewport.list", "scene.describe", "script.inspect", "audit.list"], 4):
                result = send("mcpServer/tool/call", {"threadId": thread_id, "server": "digitalviewer",
                              "tool": tool, "arguments": {}}, index)
                require(not result.get("isError") and result.get("content"), "Codex MCP tool failed: " + tool)
            print("CODEX_MCP_OK tools=7 readonly_calls=4")
        finally:
            process.stdin.close()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.terminate()
                process.wait(timeout=10)
            job.close()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", type=Path)
    parser.add_argument("--codex", type=Path)
    parser.add_argument("--token-file", type=Path)
    parser.add_argument("--port", type=int, default=8731)
    args = parser.parse_args()
    project = Path(__file__).resolve().parents[1]
    scratch = project / "build/mcp-client"
    scratch.mkdir(parents=True, exist_ok=True)
    with scratch_directory(scratch) as directory:
        if args.token_file:
            require(args.codex is not None, "--codex is required with --token-file")
            codex_check(args.codex, directory, args.token_file.read_text().strip(), args.port)
        else:
            engine = args.engine or project / "build/validation/DigitalViewer.console.exe"
            with viewer(engine, directory) as client:
                token = client.token
                for rpc_id in [0, -1, 9007199254740993, 9223372036854775807, -9223372036854775808, "0", 'quoted"id']:
                    client.request("ping", rpc_id)
                require(len(client.request("tools/list")["tools"]) == 7, "Missing MCP tools")
                if args.codex:
                    codex_check(args.codex, directory, token, client.port)
            with viewer(engine, directory) as restarted:
                require(restarted.token == token, "Credential changed across process restart")
                require(len(restarted.request("tools/list")["tools"]) == 7, "Restarted server failed")
            print("MCP_WIRE_OK integer_ids=5 string_ids=2 process_restart=passed")


if __name__ == "__main__":
    main()
