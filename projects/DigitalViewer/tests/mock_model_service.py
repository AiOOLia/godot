"""Loopback-only protocol fixture; no LLM, credentials, or external network."""
import json
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        messages = body["messages"]
        assert "test-only-key" not in json.dumps(body), "Credential leaked into model context"
        calls = set()
        for message in messages:
            calls.update(call["id"] for call in message.get("tool_calls", []))
            if message["role"] == "tool":
                assert message["tool_call_id"] in calls, "Orphan tool result"
        route = self.path
        status = 200
        if route in ("/slow", "/timeout"):
            time.sleep(1.3)
        if route == "/error":
            status = 401
            payload = {"error": "fixture authentication failure, secret=test-only-key"}
        elif route == "/badjson":
            payload = "not-json"
        elif route == "/empty":
            payload = {"choices": [{"message": {"role": "assistant", "content": None}}]}
        elif route == "/thinking":
            payload = {"choices": [{"message": {"content": None, "reasoning_content": "thinking"}}]}
        elif route == "/truncated":
            payload = {"choices": [{"finish_reason": "length", "message": {"content": "unfinished"}}]}
        elif route == "/wrong_protocol":
            payload = {"output": [{"text": "OK"}]}
        elif route == "/modern" and "max_completion_tokens" not in body:
            status = 400
            payload = {"error": {"message": "use max_completion_tokens instead of max_tokens"}}
        elif route == "/defaults" and ("max_tokens" in body or "max_completion_tokens" in body):
            status = 400
            payload = {"error": {"message": "unsupported max_tokens parameter"}}
        elif route == "/reasoning" and any(
            m.get("tool_calls") and m.get("reasoning_content") != "fixture reasoning"
            for m in messages
        ):
            status = 400
            payload = {"error": {"message": "tool continuation must preserve reasoning_content"}}
        elif route == "/runtime" and body.get("tools"):
            results = [m for m in messages if m["role"] == "tool"]
            phase = len(results)
            if phase >= 4:
                assert json.loads(results[-2]["content"])["ok"] is True
                assert json.loads(results[-1]["content"])["data"]["object"]["props"]["material.albedo_color"] == "#ee3344ff"
                payload = {"choices": [{"message": {"role": "assistant", "content": "已在运行场景中变红并启动自转，读取属性验证通过。"}}]}
            else:
                if phase == 0:
                    name, args = "scene_language", {}
                elif phase == 1:
                    name, args = "scene_run", {"program_json": json.dumps([
                        {"op": "set", "id": "globe", "property": "wrong_property", "value": 1}
                    ])}
                elif phase == 2:
                    assert json.loads(results[-1]["content"])["ok"] is False
                    name, args = "scene_run", {"program_json": json.dumps([
                        {"op": "checkpoint", "name": "before_model"},
                        {"op": "set", "id": "globe", "property": "material.albedo_color", "value": "#ee3344"},
                        {"op": "animate", "id": "model_spin", "duration": 0, "body": [
                            {"op": "set", "id": "earth", "property": "rotation_degrees", "value": [0, {"mul": [{"var": "t"}, 10]}, 0]}
                        ]},
                    ])}
                else:
                    name, args = "scene_inspect", {"id": "globe"}
                payload = {"choices": [{"message": {"role": "assistant", "content": None, "tool_calls": [
                    {"id": "runtime_%d" % phase, "type": "function", "function": {"name": name, "arguments": json.dumps(args)}}
                ]}}]}
        elif not body.get("tools"):
            payload = {"choices": [{"message": {"role": "assistant", "content": "OK"}}]}
        else:
            if route == "/delayed":
                time.sleep(0.4)
            results = [m for m in messages if m["role"] == "tool"]
            if results and route != "/loop" and (len(results) >= 2 or route != "/normal"):
                payload = {"choices": [{"message": {"role": "assistant", "content": "操作已完成。"}}]}
            else:
                name = "get_scene_state" if route == "/normal" and not results else "set_grid_visible"
                if route == "/loop":
                    name = "get_scene_state"
                if route == "/unknown":
                    name = "execute_script"
                args = {} if name == "get_scene_state" else {"visible": False}
                arguments = "[" if route == "/badtool" else json.dumps(args)
                call = {"id": "fixture_%s" % len(results), "type": "function",
                        "function": {"name": name, "arguments": arguments}}
                payload = {"choices": [{"message": {"role": "assistant", "content": None,
                                                   "tool_calls": [call]}}]}
                if route == "/reasoning":
                    payload["choices"][0]["message"]["reasoning_content"] = "fixture reasoning"
        encoded = payload.encode() if isinstance(payload, str) else json.dumps(payload, ensure_ascii=False).encode()
        try:
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(encoded)))
            self.end_headers()
            self.wfile.write(encoded)
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass


if __name__ == "__main__":
    with ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server:
        print("FIXTURE_URL=http://127.0.0.1:%s" % server.server_port, flush=True)
        server.serve_forever()
