#include "metalio_claw4/device/ports/metalio_http_transport.h"

#include <utility>

#include "application.h"
#include "board.h"
#include "http.h"
#include "network_interface.h"
#include "sync/scheduled_http_transport.h"

namespace claw4::metalio {
namespace {

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
  return std::make_unique<claw4::sync::ScheduledHttpTransport>(
      [](std::function<void()> callback) {
        Application::GetInstance().Schedule(std::move(callback));
      },
      PerformRequest);
}

}  // namespace claw4::metalio
