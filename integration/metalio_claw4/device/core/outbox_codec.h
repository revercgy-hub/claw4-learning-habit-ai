// claw4/integration/metalio_claw4/device/core/outbox_codec.h
// WB-LEARNING-V4-L1 — deterministic text codec for OutboxState persistence.
// Pure C++17, NO ESP-IDF / NVS / Metalio includes: host-testable roundtrip.
// The NVS adapter (nvs_outbox_storage) stores the encoded blob under one key.
//
// Format (line-oriented, one record per line):
//   magic "C4L1OUTBOX" line, then one header line per section:
//     D|<device_state:int>
//     T|<task_count>            followed by `t|` task rows
//     S|<0|1>                   followed by one `s|` row when 1
//     E|<pending_count>         followed by `p|` rows
//     Q|<next_sequence>  A|<last_acked>  F|<diag_fail>  R|<diag_rec>
//   Scalar/string fields inside a row are separated by '\t'; the three
//   characters '\\' '\t' '\n' are escaped inside string fields ("\\","\t","\n").
//   The pending payload map is serialized as k \x1f v pairs joined by \x1e.
//   Unknown rows / bad counts / truncation fail decode (false) so an adapter
//   never silently resurrects corrupt state.
#pragma once

#include <string>

#include "sync/outbox_storage.h"

namespace claw4 {
namespace metalio {

constexpr const char* kOutboxCodecMagic = "C4L1OUTBOX";
constexpr int kOutboxCodecVersion = 1;
// Safety cap so a corrupted/gigantic blob cannot exhaust RAM during decode.
constexpr size_t kOutboxCodecMaxBytes = 16384;

// Encodes `st` into `out`. Returns false when the encoded size exceeds the cap
// (caller maps to CommitStatus::StorageError — old state stays visible).
bool encodeOutboxState(const claw4::sync::OutboxState& st, std::string& out);

// Decodes `in`. Returns false on magic/version/length/structure errors.
bool decodeOutboxState(const std::string& in, claw4::sync::OutboxState& out);

}  // namespace metalio
}  // namespace claw4
