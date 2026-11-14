#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>

namespace ro::test_support {

// Dossier temporaire supprimé à la destruction.
//
// Nécessaire pour tester le VFS : lui donner un dossier client complet — des
// archives, un dossier data\ — est le SEUL moyen de vérifier
// l'ordre de résolution, qui est justement ce que la couche apporte.
class TempDirectory {
 public:
  TempDirectory();
  ~TempDirectory();

  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  TempDirectory(TempDirectory&&) = delete;
  TempDirectory& operator=(TempDirectory&&) = delete;

  [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

  // Crée les dossiers parents au besoin.
  void WriteFile(const std::filesystem::path& relative, std::span<const std::uint8_t> bytes) const;
  void WriteText(const std::filesystem::path& relative, std::string_view text) const;

 private:
  std::filesystem::path path_;
};

}  // namespace ro::test_support
