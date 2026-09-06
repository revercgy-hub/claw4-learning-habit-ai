#pragma once

#include <string>

#include "sync/backend_client.h"

namespace claw4::metalio {

claw4::sync::BackendClient::ChallengeSigner CreateMetalioHmacSigner(
    std::string device_secret);

}  // namespace claw4::metalio

