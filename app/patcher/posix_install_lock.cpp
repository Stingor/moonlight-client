#include <cerrno>
#include <cstring>
#include <string>
#include <utility>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

#include "patcher/install_lock.h"

namespace moonclient::patcher {
namespace {

// Lecture et écriture pour le seul propriétaire : le fichier ne porte rien.
constexpr mode_t kLockFileMode = 0600;

}  // namespace

ro::core::Result<InstallLock> InstallLock::Take(const std::filesystem::path& root) {
  const std::filesystem::path lock_path = root / kInstallLockFileName;
  const int descriptor = open(lock_path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, kLockFileMode);
  if (descriptor < 0) {
    return ro::core::Fail(
        ro::core::ErrorCode::kIoFailure,
        "impossible de verrouiller " + lock_path.string() + " (" + std::strerror(errno) + ")");
  }
  // ⚠ EXCLUSIF ET SANS ATTENTE : le verrou tient tant que le descripteur est
  // ouvert, et le système le lâche à la mort du processus.
  //
  // ⚠ LE FICHIER RESTE APRÈS NOUS, au contraire de Windows qui l'efface : sous
  // Linux, l'effacer pendant qu'un autre client l'attend lui laisserait
  // verrouiller un fichier que plus personne ne voit. Il est vide, et c'est le
  // verrou, non sa présence, qui compte.
  if (flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
    const int code = errno;
    close(descriptor);
    if (code == EWOULDBLOCK) {
      return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                            "un autre client met déjà ce dossier à jour");
    }
    return ro::core::Fail(
        ro::core::ErrorCode::kIoFailure,
        "impossible de verrouiller " + lock_path.string() + " (" + std::strerror(code) + ")");
  }
  return InstallLock(descriptor);
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
    close(static_cast<int>(handle_));
    handle_ = kNoHandle;
  }
}

}  // namespace moonclient::patcher
