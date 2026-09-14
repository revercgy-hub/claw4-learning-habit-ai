#include "metalio_claw4/device/ports/metalio_http_transport.h"

#include <utility>

#include "application.h"
#include "board.h"
#include "http.h"
#include "network_interface.h"
#include "sync/single_flight_http_transport.h"

namespace claw4::metalio {
namespace {

// Blocking vendor implementation. Runs on the caller's task only.
//
// ADR V53_A03_RUNTIME_CONCURRENCY_ADR §1 constraints that shape this code:
//   * EspNetwork::CreateHttp() returns an INDEPENDENT HttpClient, so a Wi-Fi
//     worker may own one exclusively — but it must never be shared or closed
//     concurrently from two tasks (hence the single-flight wrapper below);
//   * HttpClient::SetTimeout() only covers read/response waiting and
//     EspTcp::Connect() does not inherit it, so this timeout is NOT a total
//     DNS/connect/request deadline. That limitation is reported, not hidden.
sync::HttpResponse PerformRequest(
    const std::string& method, const std::string& url,
    const std::vector<std::pair<std::string, std::string>>& headers,
    const std::string& body, const int64_t timeout_ms) {
  auto* network = Board::GetInstance().GetNetwork();
  if (network == nullptr) return {};
  auto http = network->CreateHttp(0);
  if (!http) return {};
  http->SetTimeout(static_cast<int>(timeout_ms));
  http->SetKeepAlive(false);
  for (const auto& header : headers) http->SetHeader(header.first, header.second);
  http->SetContent(std::string(body));
  if (!http->Open(method, url)) {
    http->Close();
    return {};
  }
  sync::HttpResponse response;
  response.transport_ok = true;
  response.status = http->GetStatusCode();
  response.body = http->ReadAll();
  http->Close();
  return response;
}

}  // namespace

std::unique_ptr<claw4::sync::HttpTransport> CreateMetalioHttpTransport() {
  // Inline (no Application::Schedule): the blocking vendor call stays on the
  // backend worker task and can therefore never run inside the LVGL main loop.
  return std::make_unique<claw4::sync::SingleFlightHttpTransport>(PerformRequest);
}

}  // namespace claw4::metalio
