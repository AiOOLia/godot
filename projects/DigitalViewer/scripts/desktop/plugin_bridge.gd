extends "res://scripts/desktop/mcp_server.gd"
## Non-MCP plugin transport for Codex / Harness.
## Same tool dispatch, token and permissions as the MCP server; endpoint is /bridge.

func is_plugin() -> bool:
	return true
