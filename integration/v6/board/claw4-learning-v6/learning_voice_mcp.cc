#include "learning_voice_mcp.h"

#ifdef CLAW4_LEARNING_PREVIEW
#include <array>
#include <charconv>
#include <memory>

#include "mbedtls/base64.h"
#include "mbedtls/md.h"
#include "mcp_server.h"

namespace claw4::v6 {
namespace {

std::optional<int64_t> PositiveInteger(const std::string& text) {
  int64_t value = 0;
  const auto* end = text.data() + text.size();
  const auto result = std::from_chars(text.data(), end, value);
  if (result.ec != std::errc{} || result.ptr != end || value <= 0 || text.front() == '0')
    return std::nullopt;
  return value;
}

}  // namespace

application::LearningVoiceMac CreateLearningVoiceSigner(std::string key) {
  if (key.size() < 32) return {};
  return [key = std::move(key)](std::string_view message) -> std::string {
    std::array<unsigned char, 32> mac{};
    const auto* algorithm = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!algorithm || mbedtls_md_hmac(algorithm,
          reinterpret_cast<const unsigned char*>(key.data()), key.size(),
          reinterpret_cast<const unsigned char*>(message.data()), message.size(), mac.data()) != 0)
      return {};
    std::array<unsigned char, 45> encoded{};
    std::size_t size = 0;
    if (mbedtls_base64_encode(encoded.data(), encoded.size(), &size, mac.data(), mac.size()) != 0)
      return {};
    return std::string(reinterpret_cast<const char*>(encoded.data()), size);
  };
}

void RegisterLearningVoiceTool(
    application::LearningVoiceCommands& commands, application::LearningVoiceMac signer,
    std::function<std::optional<LearningVoiceCallContext>()> current,
    std::function<void()> show_confirmation) {
  if (!signer || !current || !show_confirmation) return;
  McpServer::GetInstance().AddTool(
      "self.claw4.learning_session", "Prove the paired learning device on the current connection",
      PropertyList({Property("challenge", kPropertyTypeString).SetMaxLength(32)}),
      [signer, current](const PropertyList& props) -> ToolResult {
        const auto scope = current();
        if (!scope) return std::unexpected("Learning session unavailable");
        const auto mac = application::signLearningVoiceSession(
            scope->binding, scope->connection_id, props["challenge"].value<std::string>(), signer);
        if (mac.size() != 44) return std::unexpected("Learning session unavailable");
        std::unique_ptr<cJSON, decltype(&cJSON_Delete)> json(cJSON_CreateObject(), cJSON_Delete);
        if (!json ||
            !cJSON_AddStringToObject(json.get(), "session_epoch", std::to_string(scope->binding.session_epoch).c_str()) ||
            !cJSON_AddStringToObject(json.get(), "mac", mac.c_str()))
          return std::unexpected("Learning session unavailable");
        return json.release();
      });
  McpServer::GetInstance().AddTool(
      "self.claw4.learning", "Paired learning command exchange",
      PropertyList({Property("command_id", kPropertyTypeString).SetMaxLength(64),
                    Property("kind", kPropertyTypeString).SetMaxLength(32),
                    Property("session_epoch", kPropertyTypeString).SetMaxLength(19),
                    Property("expires_at_ms", kPropertyTypeString).SetMaxLength(19),
                    Property("mac", kPropertyTypeString).SetMaxLength(44)}),
      [&commands, signer = std::move(signer), current = std::move(current),
       show_confirmation = std::move(show_confirmation)](const PropertyList& props) -> ToolResult {
        const auto session = PositiveInteger(props["session_epoch"].value<std::string>());
        const auto expiry = PositiveInteger(props["expires_at_ms"].value<std::string>());
        const auto scope = current();
        if (!session || !expiry || !scope) return std::unexpected("Learning request rejected");
        const application::LearningVoiceEnvelope envelope{
            props["command_id"].value<std::string>(), props["kind"].value<std::string>(),
            *session, *expiry};
        cJSON* json = cJSON_CreateObject();
        if (!json) return std::unexpected("Learning reply unavailable");
        const auto result = application::exchangeLearningVoice(
            commands, envelope, props["mac"].value<std::string>(), scope->binding, signer,
            scope->reducer, scope->now, scope->snapshot, scope->presented_reminder);
        if (!result ||
            !cJSON_AddStringToObject(json, "command_id", envelope.command_id.c_str()) ||
            !cJSON_AddStringToObject(json, "status", result->status.c_str()) ||
            !cJSON_AddStringToObject(json, "mac", result->mac.c_str()) ||
            (result->value && !cJSON_AddNumberToObject(json, "value", *result->value)) ||
            (result->snapshot_epoch_ms && !cJSON_AddNumberToObject(json, "snapshot_epoch_ms",
                                                                 *result->snapshot_epoch_ms))) {
          cJSON_Delete(json);
          return std::unexpected("Learning reply unavailable");
        }
        if (result->status == "CONFIRMATION_REQUIRED") show_confirmation();
        return json;
      });
}

}  // namespace claw4::v6
#endif
