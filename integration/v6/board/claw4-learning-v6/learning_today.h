#pragma once

#ifdef CLAW4_LEARNING_PREVIEW
#include <optional>
#include <string_view>

#include "sync/backend_client.h"
#include "sync/backend_provisioning.h"
#include "application/coordinator.h"

namespace claw4::v6 {

struct LearningTodayUpdate {
  std::optional<sync::wire::TodayResponse> snapshot;
};

using LearningUploadResult = std::function<void(application::SyncRequestEnvelope, sync::SyncClient::Response)>;

bool LearningTodayMatches(const sync::wire::TodayResponse& today, const domain::ChildId& child,
                          int64_t trusted_epoch_ms);
bool LearningTodayDateMatches(std::string_view date, int64_t trusted_epoch_ms);
bool StartLearningTodayWorker(sync::ProvisionedBackendConfig config,
                              std::function<void(LearningTodayUpdate)> apply,
                              LearningUploadResult uploaded);
bool QueueLearningUpload(application::SyncRequestEnvelope envelope);

}  // namespace claw4::v6
#endif
