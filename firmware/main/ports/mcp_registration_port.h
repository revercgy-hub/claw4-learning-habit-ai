// claw4/firmware/main/ports/mcp_registration_port.h
// McpRegistrationPort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// Registers learning.* tools with the platform MCP server. The P17 adapter
// bridges this to Metalio's McpServer::AddTool; the handler receives the raw
// JSON-lite arguments string and returns the raw result string (MVP).
// Portable C++17.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace claw4 {
namespace ports {

class McpRegistrationPort {
 public:
  virtual ~McpRegistrationPort() = default;
  using ToolHandler = std::function<std::string(const std::string& args_json)>;

  // Returns false when the name is already registered.
  virtual bool registerTool(const std::string& name, ToolHandler handler) = 0;
  virtual std::vector<std::string> registeredNames() = 0;
};

}  // namespace ports
}  // namespace claw4
