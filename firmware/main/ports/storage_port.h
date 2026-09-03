// claw4/firmware/main/ports/storage_port.h
// StoragePort platform abstraction (V4 §9 / WB-LEARNING-V4 P16).
// Durable KV persistence (NVS / SD on device — the P17 adapter owns the
// concrete backend). Portable C++17, no ESP-IDF / Metalio includes.
#pragma once

#include <optional>
#include <string>

namespace claw4 {
namespace ports {

class StoragePort {
 public:
  virtual ~StoragePort() = default;
  virtual std::optional<std::string> read(const std::string& key) = 0;
  virtual bool write(const std::string& key, const std::string& value) = 0;
  virtual bool erase(const std::string& key) = 0;
  virtual bool contains(const std::string& key) = 0;
};

}  // namespace ports
}  // namespace claw4
