// AF3 device boundary: exposes xiaozhi's NetworkInterface as the portable
// HttpTransport used by BackendClient. The implementation schedules every
// vendor HTTP call through Application::Schedule (main loop).
#pragma once

#include <memory>

#include "sync/http_transport.h"

namespace claw4::metalio {

std::unique_ptr<claw4::sync::HttpTransport> CreateMetalioHttpTransport();

}  // namespace claw4::metalio
