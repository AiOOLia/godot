"""Exercise both DigitalViewer transports against a source checkout.

No MCP SDK is used for the plugin test; both are plain HTTP/JSON-RPC.
Requires the custom Godot console executable and a writable temp directory.
"""
import argparse
import http.client
import json
import os
from pathlib import Path
import socket
import subprocess
import time


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


class Client:
    def __init__(self, token, port, path):
        self.token, self.port, self.path, self.session = token, port, path, None

    def request(self, method, params=None, rpc_id=1, token=None, path=None):
        headers = {
            "Authorization": "Bearer " + (token if token is not None else self.token),
            "Content-Type": "application/json",
        }
        if self.session:
            headers["Mcp-Session-Id"] = self.session
        body = json.dumps({"jsonrpc": "2.0", "id": rpc_id, "method": method, "params": params or {}})
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=8)
        try:
            connection.request("POST", path or self.path, body, headers)
            response = connection.getresponse()
            raw = response.read()
            if method == "initialize":
                self.session = response.getheader("Mcp-Session-Id")
            return response.status, json.loads(raw) if raw else {}
        finally:
            connection.close()

    def tool(self, method, params=None):
        status, response = self.request(method, params)
        require(status == 200, f"{method}: HTTP {status}")
        require("error" not in response, f"{method}: protocol error {response.get('error')}")
        result = response.get("result", {})
        if "content" in result:
            text = result["content"][0]["text"]
            return json.loads(text), result.get("isError", False)
        return result, False


def launch(engine, project, mode):
    directory = Path(project) / "build" / "validation" / "scratch"
    directory.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["DV_TEST_DIRECTORY"] = str(directory)
    args = [str(engine), "--path", str(project), "--headless", "--", f"--dv-{mode}-check"]
    process = subprocess.Popen(args, cwd=project, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                               creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
    token_path = directory / "mcp" / "bearer_token"
    port = 18732 if mode == "plugin" else 18731
    deadline = time.monotonic() + 25
    while time.monotonic() < deadline:
        require(process.poll() is None, f"{mode} application exited during startup")
        token_ready = token_path.exists() and bool(token_path.read_text().strip())
        port_ready = False
        if token_ready:
            try:
                with socket.create_connection(("127.0.0.1", port), timeout=0.25):
                    port_ready = True
            except OSError:
                pass
        if token_ready and port_ready:
            return process, directory, token_path.read_text().strip(), port
        time.sleep(0.1)
    process.terminate()
    raise RuntimeError(f"{mode} bridge startup timed out")


def test_plugin(engine, project):
    process, directory, token, port = launch(engine, project, "plugin")
    try:
        client = Client(token, port, "/bridge")
        status, caps = client.request("bridge.capabilities")
        require(status == 200 and caps["result"]["protocol"] == "dv.bridge.v1", "plugin capabilities failed")
        require(len(caps["result"]["tools"]) == 7, "plugin tool count")
        status, mcp = client.request("initialize", {"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "x", "version": "1"}})
        require(status == 200 and "error" in mcp, "plugin endpoint must not initialize an MCP session")
        scene, error = client.tool("scene.describe")
        require(not error and scene["viewports"], "plugin scene.describe failed")
        draft, error = client.tool("script.propose", {"source": "extends Node\n\nfunc run(viewer) -> void:\n\tpass\n", "filename": "plugin_test.gd"})
        require(not error and draft["status"] == "review_required" and not draft["executed"], "plugin script.propose failed")
        inspected, error = client.tool("script.inspect")
        require(not error and any(item["name"] == "plugin_test.gd" for item in inspected["scripts"]), "plugin draft not visible")
        denied, error = client.tool("script.run")
        require(error and "permission denied" in denied["error"].lower(), "plugin script.run permission check failed")
        audit, error = client.tool("audit.list")
        require(not error and audit["entries"], "plugin audit failed")
        bad_status, _ = client.request("ping", token="bad-token")
        require(bad_status == 401, "plugin token check failed")
        status, _ = client.request("ping", path="/mcp")
        require(status == 404, "plugin endpoint leaked MCP")
        print("PLUGIN_BRIDGE_OK tools=7 propose=review permission_denied=1 audit=1")
    finally:
        process.terminate()
        process.wait(timeout=10)


def test_mcp(engine, project):
    process, directory, token, port = launch(engine, project, "mcp")
    try:
        client = Client(token, port, "/mcp")
        status, initialized = client.request("initialize", {"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "x", "version": "1"}})
        require(status == 200 and client.session, "MCP initialize failed")
        status, tools = client.request("tools/list")
        require(status == 200 and len(tools["result"]["tools"]) == 7, "MCP tools/list failed")
        status, scene = client.request("tools/call", {"name": "scene.describe", "arguments": {}})
        require(status == 200 and not scene["result"].get("isError"), "MCP scene.describe failed")
        status, _ = client.request("ping", path="/bridge")
        require(status == 404, "MCP endpoint leaked plugin bridge")
        print("MCP_WIRE_OK tools=7 initialize=1 scene_describe=1")
    finally:
        process.terminate()
        process.wait(timeout=10)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--project", type=Path, required=True)
    args = parser.parse_args()
    test_plugin(args.engine, args.project)
    test_mcp(args.engine, args.project)


if __name__ == "__main__":
    main()
