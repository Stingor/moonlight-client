#include <cerrno>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

#include <unistd.h>

#include "patcher/relaunch.h"

namespace moonclient::patcher {
namespace {

// Ce que le noyau expose du processus qui tourne.
constexpr const char* kSelfExecutable = "/proc/self/exe";
constexpr const char* kSelfCommandLine = "/proc/self/cmdline";

// Le code de sortie d'un enfant qui n'a pas pu se remplacer par le client.
constexpr int kExecFailedExitCode = 127;

}  // namespace

ro::core::Result<std::filesystem::path> RunningExecutablePath() {
  std::error_code failed;
  std::filesystem::path executable = std::filesystem::read_symlink(kSelfExecutable, failed);
  if (failed) {
    return ro::core::Fail(ro::core::ErrorCode::kIoFailure,
                          "impossible de savoir quel exécutable tourne (" + failed.message() + ")");
  }
  return executable;
}

ro::core::Result<void> RelaunchSelf() {
  auto executable = RunningExecutablePath();
  if (!executable) {
    return std::unexpected(executable.error());
  }
  // ⚠ LA LIGNE DE COMMANDE D'ORIGINE, telle quelle : la relance retombe sur
  // la forme de lancement qui a appliqué la mise à jour. Le noyau la garde en
  // arguments séparés par des octets nuls.
  std::ifstream source(kSelfCommandLine, std::ios::binary);
  const std::string command_line((std::istreambuf_iterator<char>(source)),
                                 std::istreambuf_iterator<char>());
  std::vector<std::string> arguments;
  std::string current;
  for (const char letter : command_line) {
    if (letter == '\0') {
      arguments.push_back(current);
      current.clear();
    } else {
      current.push_back(letter);
    }
  }
  if (arguments.empty()) {
    arguments.push_back(executable->string());
  }
  // 🔴 TOUT SE PRÉPARE AVANT `fork` : dans l'enfant d'un programme à plusieurs
  // fils, seuls des appels sûrs en signal sont permis — `chdir` et `execv` en
  // sont, une allocation n'en est pas.
  std::vector<char*> argv;
  argv.reserve(arguments.size() + 1);
  for (std::string& argument : arguments) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);
  const std::string program = executable->string();
  const std::string working_directory = executable->parent_path().string();

  const pid_t child = fork();
  if (child < 0) {
    return ro::core::Fail(
        ro::core::ErrorCode::kIoFailure,
        std::string("impossible de relancer le client (") + std::strerror(errno) + ")");
  }
  if (child == 0) {
    if (chdir(working_directory.c_str()) == 0) {
      execv(program.c_str(), argv.data());
    }
    _exit(kExecFailedExitCode);
  }
  return {};
}

}  // namespace moonclient::patcher
