// claw4/integration/metalio_claw4/device/ports/nvs_outbox_storage.cpp
// WB-LEARNING-V4-L1 — NVS-backed OutboxStorage (see header).
#include "metalio_claw4/device/ports/nvs_outbox_storage.h"

#include <algorithm>
#include <vector>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "metalio_claw4/device/core/outbox_codec.h"

namespace claw4 {
namespace metalio {
namespace {

constexpr const char* TAG = "LearningNvs";
constexpr const char* kNvsNamespace = "learning";
constexpr const char* kStateKey = "st";

// Every operation opens/closes its own handle: the coordinator is the single
// caller and the cost is negligible next to the flash write itself.
bool OpenReadWrite(nvs_handle_t& h) {
  if (nvs_open(kNvsNamespace, NVS_READWRITE, &h) != ESP_OK) {
    ESP_LOGE(TAG, "nvs_open(%s) failed", kNvsNamespace);
    return false;
  }
  return true;
}

}  // namespace

bool NvsOutboxStorage::load(claw4::sync::OutboxState& out) {
  nvs_handle_t h;
  if (!OpenReadWrite(h)) return false;
  size_t len = 0;
  const esp_err_t peek = nvs_get_blob(h, kStateKey, nullptr, &len);
  if (peek == ESP_ERR_NVS_NOT_FOUND) {
    nvs_close(h);
    out = claw4::sync::OutboxState{};  // first boot: pristine state
    return true;
  }
  if (peek != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_blob len err=%d", peek);
    nvs_close(h);
    return false;
  }
  if (len == 0 || len > kOutboxCodecMaxBytes) {
    ESP_LOGE(TAG, "stored blob size invalid: %u", (unsigned)len);
    nvs_close(h);
    return false;
  }
  std::vector<char> buf(len);
  const esp_err_t rc = nvs_get_blob(h, kStateKey, buf.data(), &len);
  nvs_close(h);
  if (rc != ESP_OK) {
    ESP_LOGE(TAG, "nvs_get_blob read err=%d", rc);
    return false;
  }
  if (!decodeOutboxState(std::string(buf.data(), len), out)) {
    ESP_LOGE(TAG, "stored state failed to decode (schema drift?)");
    return false;
  }
  return true;
}

claw4::sync::CommitStatus NvsOutboxStorage::Save(claw4::sync::OutboxState cur) {
  std::string blob;
  if (!encodeOutboxState(cur, blob)) {
    ESP_LOGE(TAG, "encode failed / over size cap");
    return claw4::sync::CommitStatus::StorageError;
  }
  nvs_handle_t h;
  if (!OpenReadWrite(h)) return claw4::sync::CommitStatus::StorageError;
  const esp_err_t set = nvs_set_blob(h, kStateKey, blob.data(), blob.size());
  esp_err_t commit = ESP_OK;
  if (set == ESP_OK) commit = nvs_commit(h);
  nvs_close(h);
  if (set != ESP_OK || commit != ESP_OK) {
    ESP_LOGE(TAG, "nvs set/commit err set=%d commit=%d", set, commit);
    return claw4::sync::CommitStatus::StorageError;
  }
  return claw4::sync::CommitStatus::Committed;
}

claw4::sync::CommitStatus NvsOutboxStorage::commit(
    const claw4::domain::DomainState& next_domain,
    const std::vector<claw4::sync::PendingEvent>& appended,
    int64_t next_sequence) {
  claw4::sync::OutboxState cur;
  if (!load(cur)) return claw4::sync::CommitStatus::StorageError;
  cur.domain = next_domain;
  for (const auto& row : appended) cur.pending.push_back(row);
  cur.next_sequence = next_sequence;
  return Save(std::move(cur));
}

claw4::sync::CommitStatus NvsOutboxStorage::commitDiagnostic(bool sync_failed,
                                                             bool sync_recovered) {
  claw4::sync::OutboxState cur;
  if (!load(cur)) return claw4::sync::CommitStatus::StorageError;
  cur.diagnostic.sync_failed = sync_failed;
  cur.diagnostic.sync_recovered = sync_recovered;
  return Save(std::move(cur));
}

claw4::sync::CommitStatus NvsOutboxStorage::removeAcked(int64_t up_to_sequence) {
  claw4::sync::OutboxState cur;
  if (!load(cur)) return claw4::sync::CommitStatus::StorageError;
  auto& pending = cur.pending;
  pending.erase(std::remove_if(pending.begin(), pending.end(),
                               [&](const claw4::sync::PendingEvent& p) {
                                 return p.sequence <= up_to_sequence;
                               }),
                pending.end());
  cur.last_acked_sequence =
      std::max(cur.last_acked_sequence, up_to_sequence);
  return Save(std::move(cur));
}

claw4::sync::CommitStatus NvsOutboxStorage::markDeadLetter(
    const claw4::domain::EventId& event_id, const std::string& reason) {
  claw4::sync::OutboxState cur;
  if (!load(cur)) return claw4::sync::CommitStatus::StorageError;
  for (auto& p : cur.pending) {
    if (p.event_id == event_id) {
      p.dead_letter_reason = reason;  // row retained for replay
      break;
    }
  }
  return Save(std::move(cur));
}

bool NvsOutboxStorage::eraseAll() {
  nvs_handle_t h;
  if (nvs_open(kNvsNamespace, NVS_READWRITE, &h) != ESP_OK) return false;
  const esp_err_t rc = nvs_erase_all(h);
  const esp_err_t commit = (rc == ESP_OK) ? nvs_commit(h) : rc;
  nvs_close(h);
  return rc == ESP_OK && commit == ESP_OK;
}

bool NvsOutboxStorage::hasState() {
  nvs_handle_t h;
  if (!OpenReadWrite(h)) return false;
  size_t len = 0;
  const esp_err_t rc = nvs_get_blob(h, kStateKey, nullptr, &len);
  nvs_close(h);
  return rc == ESP_OK && len > 0;
}

}  // namespace metalio
}  // namespace claw4
