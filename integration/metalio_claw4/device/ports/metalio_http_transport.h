// AF3 device boundary: exposes xiaozhi's NetworkInterface as the portable
// HttpTransport used by BackendClient.
//
// WB-V53-NEXT-001 CP1 (A03): this boundary NO LONGER defers the vendor HTTP
// call onto the LVGL main loop. The previous implementation wrapped the
// blocking PerformRequest in Application::Schedule and then blocked the caller
// waiting for that callback, which meant the main loop itself executed
// DNS/connect/read/close and the UI froze for the whole network wait.
//
// The transport now runs the vendor call inline on the CALLING task (the
// backend worker task, never the LVGL callback) and enforces single-flight so
// one HttpClient is never raced or closed concurrently from two tasks.
#pragma once

#include <memory>

#include "sync/http_transport.h"

namespace claw4::metalio {

// Returns a single-flight transport that executes the vendor HTTP call on the
// caller's task. Must only be called from a worker/task context (currently the
// `learning_backend` task), never from an LVGL event callback.
std::unique_ptr<claw4::sync::HttpTransport> CreateMetalioHttpTransport();

}  // namespace claw4::metalio
