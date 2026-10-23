// Les sons de l'interface. Voir l'en-tête.

#include "game/interface_sound.h"

#include <algorithm>
#include <expected>
#include <format>
#include <fstream>
#include <iterator>
#include <ranges>
#include <system_error>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/skill_sound_table.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// L'arrivée d'un message SE LIT SUR LE DISQUE, dans le dossier des musiques de
// l'installation ; le son de bouton est celui des archives.
constexpr std::string_view kMessengerSound = "BGM/Msg_NewMsg.wav";
constexpr std::string_view kButtonSound = "버튼소리.wav";
constexpr float kInterfaceGain = 1.0F;
constexpr float kInterfacePan = 0.0F;

constexpr char kSeparator = '/';
constexpr char kWindowsSeparator = '\\';
constexpr char kDriveMark = ':';
constexpr std::string_view kParentFolder = "..";
// Sépare, dans le nom d'un son du disque, le chemin de sa taille et de sa
// date : aucun nom de fichier de Windows ne le contient.
constexpr char kStampMark = '|';

// Un fichier de l'installation, hors de toute archive ; vide s'il manque.
[[nodiscard]] std::vector<std::uint8_t> ReadInstalled(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>());
}

// ⚠ LE CHEMIN EST EN UTF-8 : construit depuis un `std::string`, Windows le
// lirait dans la page de code du système, et un nom accentué ne s'ouvrirait
// pas.
[[nodiscard]] std::filesystem::path Utf8Path(std::string_view utf8) {
  return {std::u8string(utf8.begin(), utf8.end())};
}

[[nodiscard]] std::vector<std::uint8_t> ReadSound(ro::vfs::Vfs& vfs,
                                                  const std::filesystem::path& client_root,
                                                  const InterfaceSoundFile& file) {
  if (file.place == InterfaceSoundPlace::kInstallation) {
    return ReadInstalled(client_root / Utf8Path(file.path));
  }
  auto bytes = vfs.Read(std::string(ro::render::kSoundFolder) + file.path);
  return bytes ? std::move(*bytes) : std::vector<std::uint8_t>();
}

// LE NOM SOUS LEQUEL LE MOTEUR RETIENT UN SON. Celui des archives est son
// chemin. Celui d'un fichier du disque porte son dossier, qui ne se confond
// avec aucun son des archives, puis sa taille et sa date d'écriture : un
// fichier remplacé sous le même nom change de nom de son, et se relit. Vide :
// le fichier du disque n'existe pas.
[[nodiscard]] std::string HeldSoundName(const std::filesystem::path& client_root,
                                        const InterfaceSoundFile& file) {
  if (file.place != InterfaceSoundPlace::kInstallation) {
    return file.path;
  }
  const std::filesystem::path on_disk = client_root / Utf8Path(file.path);
  std::error_code size_error;
  const std::uintmax_t size_bytes = std::filesystem::file_size(on_disk, size_error);
  std::error_code written_error;
  const std::filesystem::file_time_type written_at =
      std::filesystem::last_write_time(on_disk, written_error);
  if (size_error || written_error) {
    return {};
  }
  return std::format("{}{}{}{}{}", file.path, kStampMark, size_bytes, kStampMark,
                     written_at.time_since_epoch().count());
}

// UN FICHIER DU DISQUE N'A QU'UNE VERSION RETENUE : celle qu'on vient de
// charger. Les autres noms du même chemin sont ses versions remplacées, que
// le moteur garderait décodées jusqu'à la fermeture.
void ForgetOlderVersions(ro::audio::AudioEngine& audio, const InterfaceSoundFile& file,
                         const std::string& held_name) {
  if (file.place != InterfaceSoundPlace::kInstallation) {
    return;
  }
  const std::string versions_prefix = file.path + kStampMark;
  for (const std::string& name : audio.HeldSoundNames()) {
    if (name != held_name && name.starts_with(versions_prefix)) {
      audio.ForgetSound(name);
    }
  }
}

}  // namespace

InterfaceSoundFile MessengerSoundFile() {
  return {.place = InterfaceSoundPlace::kInstallation, .path = std::string(kMessengerSound)};
}

InterfaceSoundFile ButtonSoundFile() {
  return {.place = InterfaceSoundPlace::kSoundArchive, .path = std::string(kButtonSound)};
}

InterfaceSoundFile SaveFolderSoundFile(std::string_view typed) {
  std::string name(typed);
  std::ranges::replace(name, kWindowsSeparator, kSeparator);
  if (name.empty() || name.front() == kSeparator || name.find(kDriveMark) != std::string::npos) {
    return {};
  }
  for (const auto part : std::views::split(name, kSeparator)) {
    if (std::string_view(part.begin(), part.end()) == kParentFolder) {
      return {};
    }
  }
  return {.place = InterfaceSoundPlace::kInstallation,
          .path = std::string(kSaveFolder) + kSeparator + name};
}

ro::core::Result<void> PlayInterfaceSound(ro::audio::AudioEngine& audio, ro::vfs::Vfs& vfs,
                                          const std::filesystem::path& client_root,
                                          const InterfaceSoundFile& file,
                                          std::string* held_name_out) {
  if (file.place == InterfaceSoundPlace::kNowhere) {
    return std::unexpected(
        ro::core::Error{ro::core::ErrorCode::kInvalidArgument, "aucun fichier désigné"});
  }
  if (!audio.sound_audible()) {
    return {};
  }
  const std::string held_name = HeldSoundName(client_root, file);
  if (held_name.empty()) {
    return std::unexpected(ro::core::Error{ro::core::ErrorCode::kNotFound, file.path});
  }
  if (!audio.KnowsSound(held_name)) {
    const std::vector<std::uint8_t> bytes = ReadSound(vfs, client_root, file);
    if (bytes.empty()) {
      return std::unexpected(ro::core::Error{ro::core::ErrorCode::kNotFound, file.path});
    }
    if (auto held = audio.LoadSound(held_name, bytes); !held) {
      return std::unexpected(
          ro::core::Error{held.error().code, file.path + " — " + held.error().detail});
    }
    ForgetOlderVersions(audio, file, held_name);
  }
  audio.PlaySound(held_name, kInterfaceGain, kInterfacePan, ro::audio::SoundChannel::kInterface);
  if (held_name_out != nullptr) {
    *held_name_out = held_name;
  }
  return {};
}

void PlayInterfaceSounds(ro::audio::AudioEngine& audio, ro::vfs::Vfs* vfs,
                         const std::filesystem::path& client_root,
                         std::span<const InterfaceSoundFile> files, std::string_view from) {
  if (vfs == nullptr) {
    return;
  }
  for (const InterfaceSoundFile& file : files) {
    if (const ro::core::Result<void> played = PlayInterfaceSound(audio, *vfs, client_root, file);
        !played) {
      ro::core::LogWarning("son {} : {}", from, ro::core::Describe(played.error()));
    }
  }
}

}  // namespace moonclient
