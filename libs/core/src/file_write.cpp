#include "ro/core/file_write.h"

#include <fstream>
#include <system_error>

namespace ro::core {
namespace {

// Le suffixe du voisin temporaire. Il reste dans le dossier de la cible, et
// c'est ce qui rend le renommage atomique : deux dossiers peuvent être sur deux
// volumes, auquel cas le système copie au lieu de renommer.
constexpr const char* kPendingSuffix = ".tmp";

}  // namespace

Result<void> WriteFileAtomically(const std::filesystem::path& path, std::string_view content) {
  std::filesystem::path pending = path;
  pending += kPendingSuffix;

  std::error_code failed;
  if (const std::filesystem::path folder = path.parent_path(); !folder.empty()) {
    std::filesystem::create_directories(folder, failed);
  }

  {
    // ⚠ EN BINAIRE : la fin de ligne s'écrit telle qu'on la donne. Le contraire
    // rendrait un fichier dont les octets dépendent du système qui l'a écrit,
    // et les comparaisons d'un test ne tiendraient plus.
    std::ofstream file(pending, std::ios::binary | std::ios::trunc);
    if (!file) {
      return Fail(ErrorCode::kIoFailure, pending.string());
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    file.flush();
    if (!file) {
      // Disque plein ou support perdu. On retire le temporaire et on laisse la
      // cible telle qu'elle était.
      file.close();
      std::filesystem::remove(pending, failed);
      return Fail(ErrorCode::kIoFailure, pending.string());
    }
  }  // fermeture du flux — donc vidage complet — AVANT le remplacement

  std::filesystem::rename(pending, path, failed);
  if (failed) {
    std::error_code ignored;
    std::filesystem::remove(pending, ignored);
    return Fail(ErrorCode::kIoFailure, path.string() + " : " + failed.message());
  }
  return {};
}

Result<void> WriteFileAtomicallyWith(const std::filesystem::path& path,
                                     const std::function<Result<void>(ByteSink&)>& fill) {
  std::filesystem::path pending = path;
  pending += kPendingSuffix;

  std::error_code failed;
  if (const std::filesystem::path folder = path.parent_path(); !folder.empty()) {
    std::filesystem::create_directories(folder, failed);
  }

  auto filled = [&]() -> Result<void> {
    auto sink = FileByteSink::Create(pending);
    if (!sink) {
      return std::unexpected(sink.error());
    }
    if (auto written = fill(**sink); !written) {
      return written;
    }
    return (*sink)->Close();
  }();
  std::error_code ignored;
  if (!filled) {
    std::filesystem::remove(pending, ignored);
    return filled;
  }

  std::filesystem::rename(pending, path, failed);
  if (failed) {
    std::filesystem::remove(pending, ignored);
    return Fail(ErrorCode::kIoFailure, path.string() + " : " + failed.message());
  }
  return {};
}

}  // namespace ro::core
