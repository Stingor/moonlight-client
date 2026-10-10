// Les lignes détachées du carnet de chasse MVP, sur le disque. Voir l'en-tête.

#include "mvp/mvp_line_file.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS, et en anglais : les changer ferait perdre à
// chacun ses lignes posées, sans que rien ne le dise.
constexpr const char* kLinesField = "lines";
constexpr const char* kMobField = "mob";
constexpr const char* kMapField = "map";
constexpr const char* kXField = "x";
constexpr const char* kYField = "y";

constexpr const char* kFileHeader =
    "# Les lignes détachées du carnet de chasse MVP : une ligne par créneau, posée à l'écran.\n"
    "# mob : le monstre (0 pour un créneau scripté, que la carte identifie) ; map : la carte ;\n"
    "# x, y : la place de la ligne, en pixels.\n";

[[nodiscard]] std::filesystem::path LinePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kMvpLineFileName;
}

// UNE ENTRÉE, ou rien : sans carte, à une place négative, ou d'un monstre hors
// des seize bits du fil, elle ne désigne aucune ligne qu'on puisse poser.
[[nodiscard]] std::optional<MvpPinnedLine> ReadOne(const YAML::Node& entry) {
  if (!entry.IsMap()) {
    return std::nullopt;
  }
  const YAML::Node mob = entry[kMobField];
  const YAML::Node map = entry[kMapField];
  const YAML::Node x = entry[kXField];
  const YAML::Node y = entry[kYField];
  if (!mob || !mob.IsScalar() || !map || !map.IsScalar() || !x || !x.IsScalar() || !y ||
      !y.IsScalar()) {
    return std::nullopt;
  }
  MvpPinnedLine line;
  try {
    const int mob_id = mob.as<int>();
    if (mob_id < 0 || mob_id > std::numeric_limits<std::uint16_t>::max()) {
      return std::nullopt;
    }
    line.mob_id = static_cast<std::uint16_t>(mob_id);
    line.map = map.Scalar();
    line.x = x.as<int>();
    line.y = y.as<int>();
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
  if (line.map.empty() || line.x < 0 || line.y < 0) {
    return std::nullopt;
  }
  return line;
}

}  // namespace

std::string WriteMvpLines(const std::vector<MvpPinnedLine>& lines) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kLinesField << YAML::Value << YAML::BeginSeq;
  for (const MvpPinnedLine& line : lines) {
    out << YAML::BeginMap;
    out << YAML::Key << kMobField << YAML::Value << line.mob_id;
    out << YAML::Key << kMapField << YAML::Value << YAML::DoubleQuoted << line.map;
    out << YAML::Key << kXField << YAML::Value << line.x;
    out << YAML::Key << kYField << YAML::Value << line.y;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
  out << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

bool ReadMvpLines(std::string_view text, std::vector<MvpPinnedLine>& into) {
  YAML::Node whole;
  try {
    whole = YAML::Load(std::string(text));
  } catch (const YAML::Exception&) {
    return false;
  }
  std::vector<MvpPinnedLine> read;
  // UN TEXTE VIDE N'EST PAS ILLISIBLE : c'est un fichier sans ligne.
  if (!whole || whole.IsNull()) {
    into = std::move(read);
    return true;
  }
  if (!whole.IsMap()) {
    return false;
  }
  const YAML::Node lines = whole[kLinesField];
  if (lines && !lines.IsNull()) {
    if (!lines.IsSequence()) {
      return false;
    }
    for (const YAML::Node& entry : lines) {
      std::optional<MvpPinnedLine> line = ReadOne(entry);
      // ⚠ UN CRÉNEAU, UNE LIGNE : un doublon écrit à la main garde la première.
      if (line.has_value() && std::ranges::none_of(read, [&line](const MvpPinnedLine& kept) {
            return kept.mob_id == line->mob_id && kept.map == line->map;
          })) {
        read.push_back(std::move(*line));
      }
    }
  }
  into = std::move(read);
  return true;
}

bool LoadMvpLines(const std::filesystem::path& client_root, std::vector<MvpPinnedLine>& into) {
  const std::filesystem::path path = LinePath(client_root);
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    into.clear();
    return true;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    ro::core::LogWarning("carnet MVP : {} ne s'ouvre pas, il ne sera pas réécrit", path.string());
    return false;
  }
  std::ostringstream text;
  text << file.rdbuf();
  if (!ReadMvpLines(text.str(), into)) {
    ro::core::LogWarning("carnet MVP : {} illisible, il ne sera pas réécrit", path.string());
    return false;
  }
  return true;
}

bool SaveMvpLines(const std::filesystem::path& client_root,
                  const std::vector<MvpPinnedLine>& lines) {
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(LinePath(client_root), WriteMvpLines(lines));
      !written) {
    ro::core::LogWarning("carnet MVP : lignes détachées non écrites ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
