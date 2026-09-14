// Compiles the real adapters against a narrow in-memory NVS API shim.
// These checks do not establish ESP-IDF ABI, Flash atomicity or hardware behavior.
#include "nvs.h"
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include "metalio_claw4/device/core/outbox_codec.h"
#include "metalio_claw4/device/ports/nvs_outbox_storage.h"
#include "metalio_claw4/device/ports/nvs_backend_provisioning.h"

static int open_result = ESP_OK, read_result = ESP_OK, open_mode = -1;
static int writes = 0, closes = 0;
static std::string blob;
static std::map<std::string, std::string> fields;
int nvs_open(const char*, int mode, nvs_handle_t* h) { open_mode = mode; *h = 1; return open_result; }
void nvs_close(nvs_handle_t) { ++closes; }
int nvs_get_blob(nvs_handle_t, const char*, void* dest, size_t* len) {
  if (read_result != ESP_OK) return read_result;
  if (dest) std::memcpy(dest, blob.data(), blob.size());
  *len = blob.size(); return ESP_OK;
}
int nvs_get_str(nvs_handle_t, const char* key, char* dest, size_t* len) {
  if (read_result != ESP_OK) return read_result;
  auto found = fields.find(key);
  if (found == fields.end()) return ESP_ERR_NVS_NOT_FOUND;
  if (dest) std::memcpy(dest, found->second.c_str(), found->second.size() + 1);
  *len = found->second.size() + 1; return ESP_OK;
}
int nvs_set_blob(nvs_handle_t, const char*, const void*, size_t) { ++writes; return ESP_FAIL; }
int nvs_set_str(nvs_handle_t, const char*, const char*) { ++writes; return ESP_FAIL; }
int nvs_commit(nvs_handle_t) { ++writes; return ESP_FAIL; }
int nvs_erase_all(nvs_handle_t) { ++writes; return ESP_FAIL; }
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " << #x << '\n'; return 1; } } while (false)
int main() {
  using claw4::metalio::NvsOutboxStorage;
  using Status = NvsOutboxStorage::StateReadStatus;
  using claw4::sync::ProvisioningStatus;
  NvsOutboxStorage storage;
  claw4::sync::OutboxState state;
  state.next_sequence = 42;
  open_result = ESP_ERR_NVS_NOT_FOUND;
  CHECK(storage.loadWithPresence(state) == Status::Missing);
  CHECK(state.next_sequence == 1 && open_mode == NVS_READONLY);
  open_result = ESP_FAIL;
  CHECK(storage.loadWithPresence(state) == Status::Error);
  open_result = ESP_OK; read_result = ESP_ERR_NVS_NOT_FOUND;
  CHECK(storage.loadWithPresence(state) == Status::Missing);
  CHECK(closes == 1);
  read_result = ESP_FAIL;
  CHECK(storage.loadWithPresence(state) == Status::Error);
  read_result = ESP_OK; blob = "corrupt";
  CHECK(storage.loadWithPresence(state) == Status::Error);
  state.next_sequence = 42; state.last_acked_sequence = 41;
  CHECK(claw4::metalio::encodeOutboxState(state, blob));
  claw4::sync::OutboxState loaded;
  CHECK(storage.loadWithPresence(loaded) == Status::Present);
  CHECK(loaded.next_sequence == 42 && loaded.last_acked_sequence == 41);
  CHECK(open_mode == NVS_READONLY && writes == 0);
  claw4::metalio::NvsBackendProvisioning config_store;
  claw4::sync::ProvisionedBackendConfig config;
  open_result = ESP_ERR_NVS_NOT_FOUND;
  CHECK(config_store.load(config) == ProvisioningStatus::NotConfigured);
  open_result = ESP_FAIL;
  CHECK(config_store.load(config) == ProvisioningStatus::StorageError);
  open_result = ESP_OK; fields.clear();
  CHECK(config_store.load(config) == ProvisioningStatus::Invalid);
  fields = {{"base_url", "http://synthetic.invalid"}, {"device_id", "fake-device"},
            {"child_id", "fake-child"}, {"device_secret", "synthetic-only"}};
  CHECK(config_store.load(config) == ProvisioningStatus::Ready);
  read_result = ESP_FAIL;
  CHECK(config_store.load(config) == ProvisioningStatus::StorageError);
  CHECK(open_mode == NVS_READONLY && writes == 0);
  std::cout << "NVS read contracts: PASS (synthetic shim; zero writes)\n";
}
