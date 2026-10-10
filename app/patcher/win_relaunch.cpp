#include <string>
#include <vector>

#include "patcher/relaunch.h"

// clang-format off
#include <windows.h>
// clang-format on

namespace moonclient::patcher {
namespace {

// Un chemin Windows tient dans 32 767 caractères ; on part de la limite
// historique et on double tant que le système tronque.
constexpr DWORD kInitialPathCapacity = MAX_PATH;
constexpr DWORD kPathCapacityLimit = 32'768;

}  // namespace

ro::core::Result<std::filesystem::path> RunningExecutablePath() {
  std::vector<wchar_t> buffer(kInitialPathCapacity);
  while (buffer.size() <= kPathCapacityLimit) {
    const DWORD written =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (written == 0) {
      break;
    }
    if (written < buffer.size()) {
      return std::filesystem::path(std::wstring(buffer.data(), written));
    }
    buffer.resize(buffer.size() * 2);
  }
  return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                        "impossible de savoir quel exécutable tourne (erreur " +
                            std::to_string(GetLastError()) + ")");
}

ro::core::Result<void> RelaunchSelf() {
  auto executable = RunningExecutablePath();
  if (!executable) {
    return std::unexpected(executable.error());
  }
  // ⚠ LA LIGNE DE COMMANDE D'ORIGINE, telle quelle : la relance retombe sur
  // la forme de lancement qui a appliqué la mise à jour.
  std::wstring command_line = GetCommandLineW();
  const std::wstring working_directory = executable->parent_path().wstring();
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION started = {};
  if (CreateProcessW(executable->c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                     working_directory.c_str(), &startup, &started) == FALSE) {
    return ro::core::Fail(
        ro::core::ErrorCode::kIoFailure,
        "impossible de relancer le client (erreur " + std::to_string(GetLastError()) + ")");
  }
  CloseHandle(started.hThread);
  CloseHandle(started.hProcess);
  return {};
}

}  // namespace moonclient::patcher
