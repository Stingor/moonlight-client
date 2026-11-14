#include "support/temp_directory.h"

#include <atomic>
#include <fstream>
#include <random>
#include <string>
#include <system_error>

namespace ro::test_support {
namespace {

// Le compteur distingue les dossiers d'une même exécution ; le tirage aléatoire
// distingue deux exécutions concurrentes, qui partagent le dossier temporaire
// du système.
std::atomic<unsigned> g_counter{0};

unsigned long long RunIdentifier() {
  static const unsigned long long identifier = [] {
    std::random_device entropy;
    return (static_cast<unsigned long long>(entropy()) << 32) | entropy();
  }();
  return identifier;
}

}  // namespace

TempDirectory::TempDirectory() {
  std::error_code code;
  path_ = std::filesystem::temp_directory_path(code) /
          ("moonclient-test-" + std::to_string(RunIdentifier()) + "-" +
           std::to_string(g_counter.fetch_add(1)));
  std::filesystem::create_directories(path_, code);
}

TempDirectory::~TempDirectory() {
  std::error_code code;
  std::filesystem::remove_all(path_, code);
}

void TempDirectory::WriteFile(const std::filesystem::path& relative,
                              std::span<const std::uint8_t> bytes) const {
  const std::filesystem::path target = path_ / relative;
  std::error_code code;
  std::filesystem::create_directories(target.parent_path(), code);

  std::ofstream stream(target, std::ios::binary);
  if (!bytes.empty()) {
    stream.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
  }
}

void TempDirectory::WriteText(const std::filesystem::path& relative, std::string_view text) const {
  WriteFile(relative, std::span<const std::uint8_t>(
                          reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

}  // namespace ro::test_support
