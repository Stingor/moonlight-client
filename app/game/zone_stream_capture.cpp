#include "game/zone_stream_capture.h"

#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/core/utf8_path.h"

namespace moonclient {
namespace {

// Le fichier de la n-ième liaison : le nom demandé pour la première, un numéro
// avant l'extension ensuite.
std::filesystem::path FileOfLink(const std::filesystem::path& first_file, std::size_t rank) {
  if (rank == 1) {
    return first_file;
  }
  std::filesystem::path numbered = first_file;
  numbered.replace_filename(ro::core::PathFromUtf8(ro::core::Utf8FromPath(first_file.stem()) + "." +
                                                   std::to_string(rank) +
                                                   ro::core::Utf8FromPath(first_file.extension())));
  return numbered;
}

}  // namespace

ZoneStreamCapture::ZoneStreamCapture(std::filesystem::path first_file)
    : shared_(std::make_shared<Shared>(Shared{.first_file = std::move(first_file)})) {}

ro::net::ReceiveObserver ZoneStreamCapture::ObserveNewLink() {
  // L'état de CETTE liaison, partagé par les copies de l'observateur : le
  // fichier s'ouvre une fois, à son premier octet.
  struct Link {
    std::ofstream sink;
    bool tried = false;
  };
  auto link = std::make_shared<Link>();
  return [shared = shared_, link](std::span<const std::uint8_t> bytes) {
    if (!link->tried) {
      link->tried = true;
      shared->links_opened += 1;
      const std::filesystem::path file = FileOfLink(shared->first_file, shared->links_opened);
      link->sink.open(file, std::ios::binary | std::ios::trunc);
      if (link->sink.is_open()) {
        ro::core::LogInfo("capture du flux de zone dans {}", ro::core::Utf8FromPath(file));
      } else {
        ro::core::LogWarning(
            "capture du flux : {} ne s'ouvre pas — cette liaison ne sera pas "
            "gardée",
            ro::core::Utf8FromPath(file));
      }
    }
    if (!link->sink.is_open()) {
      return;
    }
    link->sink.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    // Vidé à chaque bloc : une session qui se perd emporterait sinon dans son
    // tampon les octets qui expliquent pourquoi.
    link->sink.flush();
  };
}

}  // namespace moonclient
