#include <cstdio>

#include "sync/backend_provisioning.h"
#include "../../fakes/fake_backend_provisioning.h"

namespace {

using claw4::sync::FakeBackendProvisioning;
using claw4::sync::ProvisionedBackendConfig;
using claw4::sync::ProvisioningStatus;

int g_fail = 0;
#define CHECK(x)                                                        \
  do {                                                                  \
    if (!(x)) {                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);        \
      ++g_fail;                                                         \
    }                                                                   \
  } while (0)

ProvisionedBackendConfig Config() {
  return {"http://192.168.1.10:8000", "dev-test", "child-test", "secret"};
}

void MissingConfigIsNotReady() {
  FakeBackendProvisioning store;
  ProvisionedBackendConfig out;
  CHECK(store.load(out) == ProvisioningStatus::NotConfigured);
  CHECK(!out.complete());
}

void SaveAndReloadKeepsConfig() {
  FakeBackendProvisioning store;
  CHECK(store.save(Config()) == ProvisioningStatus::Ready);
  ProvisionedBackendConfig out;
  CHECK(store.load(out) == ProvisioningStatus::Ready);
  CHECK(out.base_url == "http://192.168.1.10:8000");
  CHECK(out.device_id == "dev-test");
  CHECK(out.child_id == "child-test");
  CHECK(out.device_secret == "secret");
}

void InvalidSaveDoesNotCreateConfig() {
  FakeBackendProvisioning store;
  CHECK(store.save({"http://host:8000", "dev", "child", ""}) ==
        ProvisioningStatus::Invalid);
  ProvisionedBackendConfig out;
  CHECK(store.load(out) == ProvisioningStatus::NotConfigured);
}

void ClearRemovesConfig() {
  FakeBackendProvisioning store;
  CHECK(store.save(Config()) == ProvisioningStatus::Ready);
  CHECK(store.clear() == ProvisioningStatus::NotConfigured);
  ProvisionedBackendConfig out;
  CHECK(store.load(out) == ProvisioningStatus::NotConfigured);
}

void FailureKeepsOldConfig() {
  FakeBackendProvisioning store;
  CHECK(store.save(Config()) == ProvisioningStatus::Ready);
  store.fail_save = true;
  CHECK(store.save({"http://new:8000", "new", "child", "new-secret"}) ==
        ProvisioningStatus::StorageError);
  ProvisionedBackendConfig out;
  CHECK(store.load(out) == ProvisioningStatus::Ready);
  CHECK(out.device_id == "dev-test");
  CHECK(out.device_secret == "secret");
}

}  // namespace

int main() {
  MissingConfigIsNotReady();
  SaveAndReloadKeepsConfig();
  InvalidSaveDoesNotCreateConfig();
  ClearRemovesConfig();
  FailureKeepsOldConfig();
  if (g_fail == 0) {
    std::printf("backend_provisioning_tests: all PASS\n");
    return 0;
  }
  std::printf("backend_provisioning_tests: %d FAILURES\n", g_fail);
  return 1;
}

