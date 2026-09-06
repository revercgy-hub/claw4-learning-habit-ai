#include "metalio_claw4/device/ports/metalio_hmac_signer.h"

#include <array>
#include <string>

#include "mbedtls/base64.h"
#include "mbedtls/md.h"

namespace claw4::metalio {
namespace {

std::string Sign(const std::string& secret, const std::string& device_id,
                 const std::string& challenge_id, const std::string& nonce) {
  const std::string message = device_id + "|" + challenge_id + "|" + nonce;
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (md == nullptr) return {};
  std::array<unsigned char, 32> mac{};
  if (mbedtls_md_hmac(md,
                      reinterpret_cast<const unsigned char*>(secret.data()),
                      secret.size(),
                      reinterpret_cast<const unsigned char*>(message.data()),
                      message.size(), mac.data()) != 0) {
    return {};
  }
  std::array<unsigned char, 64> encoded{};
  size_t olen = 0;
  if (mbedtls_base64_encode(encoded.data(), encoded.size(), &olen, mac.data(),
                            mac.size()) != 0) {
    return {};
  }
  return std::string(reinterpret_cast<const char*>(encoded.data()), olen);
}

}  // namespace

claw4::sync::BackendClient::ChallengeSigner CreateMetalioHmacSigner(
    std::string device_secret) {
  return [secret = std::move(device_secret)](
             const std::string& device_id, const std::string& challenge_id,
             const std::string& nonce) {
    return Sign(secret, device_id, challenge_id, nonce);
  };
}

}  // namespace claw4::metalio

