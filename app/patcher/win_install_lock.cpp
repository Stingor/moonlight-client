#include <string>
#include <utility>

#include "patcher/install_lock.h"

// clang-format off
#include <windows.h>
// clang-format on

namespace moonclient::patcher {

ro::core::Result<InstallLock> InstallLock::Take(const std::filesystem::path& root) {
  const std::filesystem::path lock_path = root / kInstallLockFileName;
  // ⚠ SANS PARTAGE, et effacé à la fermeture : la poignée EST le verrou.
  HANDLE handle = CreateFileW(lock_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    const DWORD code = GetLastError();
    if (code == ERROR_SHARING_VIOLATION) {
      return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                            "un autre client met déjà ce dossier à jour");
    }
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure, "impossible de verrouiller " +
                                                               lock_path.string() + " (erreur " +
                                                               std::to_string(code) + ")");
  }
  return InstallLock(reinterpret_cast<std::intptr_t>(handle));
}

InstallLock::InstallLock(InstallLock&& other) noexcept
    : handle_(std::exchange(other.handle_, kNoHandle)) {}

InstallLock& InstallLock::operator=(InstallLock&& other) noexcept {
  if (this != &other) {
    Release();
    handle_ = std::exchange(other.handle_, kNoHandle);
  }
  return *this;
}

InstallLock::~InstallLock() { Release(); }

void InstallLock::Release() noexcept {
  if (handle_ != kNoHandle) {
    CloseHandle(reinterpret_cast<HANDLE>(handle_));
    handle_ = kNoHandle;
  }
}

}  // namespace moonclient::patcher
