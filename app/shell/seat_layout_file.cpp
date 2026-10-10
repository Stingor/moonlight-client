#include "shell/seat_layout_file.h"

#include <algorithm>
#include <fstream>
#include <iterator>

#include <yaml-cpp/yaml.h>

#include "ro/core/ascii.h"
#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// LES CLÉS. ⚠ GRAVÉES, ET CE SONT CELLES DE `bourgeon_charselect_layout.yaml` :
// les changer perdrait la mise en page de tous les joueurs, et celle que ce
// fichier porte.
constexpr const char* kVersionField = "version";
constexpr const char* kBackgroundField = "background";
constexpr const char* kHideFreeField = "hide_empty_seats";
constexpr const char* kFreeAlphaField = "empty_seat_alpha";
constexpr const char* kSeatsField = "seats";
constexpr const char* kSeatXField = "nx";
constexpr const char* kSeatYField = "ny";
constexpr const char* kSeatHeightField = "scale";
constexpr const char* kSeatDirectionField = "dir";
constexpr const char* kSeatAnimationField = "anim";
constexpr const char* kSeatHeadField = "head_dir";
constexpr const char* kSeatAnimatedField = "animate";
constexpr const char* kSeatFrameField = "frame";
constexpr const char* kAnchorsField = "anchors";
constexpr const char* kAnchorNameField = "name";
constexpr const char* kPresetsField = "presets";
constexpr const char* kPresetNameField = "name";
constexpr int kFormatVersion = 1;

constexpr const char* kFileHeader =
    "# La mise en page de la tablee : sieges, poses, blocs d'interface, decor.\n"
    "# Ecrite par le mode << Personnaliser >> ; les coordonnees sont des\n"
    "# fractions de l'ecran, donc valables a toute resolution. Meme format que\n"
    "# bourgeon_charselect_layout.yaml.\n";

// L'ASCII imprimable, de l'espace au tilde.
constexpr unsigned char kPrintableLowest = 0x20;
constexpr unsigned char kPrintableHighest = 0x7e;

[[nodiscard]] bool PrintableAscii(std::string_view text) {
  return std::ranges::all_of(text, [](char letter) {
    const auto byte = static_cast<unsigned char>(letter);
    return byte >= kPrintableLowest && byte <= kPrintableHighest;
  });
}

// Le nom de fichier seul.
[[nodiscard]] std::string_view FileNameOf(std::string_view path) {
  const std::size_t cut = path.find_last_of("\\/");
  return cut == std::string_view::npos ? path : path.substr(cut + 1);
}

// Le nom sans son extension : c'est le libellé proposé.
[[nodiscard]] std::string LabelOf(std::string_view file) {
  const std::size_t dot = file.find_last_of('.');
  return std::string(dot == std::string_view::npos ? file : file.substr(0, dot));
}

// UN SIÈGE, lu par-dessus sa valeur courante.
//
// ⚠ POSITION ET TAILLE VONT ENSEMBLE : une seule hors bornes les rejette toutes
// les trois — un siège à moitié déplacé serait pire que le siège d'usine.
void ReadSeat(const YAML::Node& node, ro::ui::CharacterSeat& seat) {
  const float x = node[kSeatXField].as<float>(seat.x);
  const float y = node[kSeatYField].as<float>(seat.y);
  const float height = node[kSeatHeightField].as<float>(seat.height);
  if (ro::ui::ValidSeatFraction(x) && ro::ui::ValidSeatFraction(y) &&
      ro::ui::ValidSeatHeight(height)) {
    seat.x = x;
    seat.y = y;
    seat.height = height;
  }
  const int direction = node[kSeatDirectionField].as<int>(seat.direction);
  const int animation = node[kSeatAnimationField].as<int>(seat.animation);
  const int head = node[kSeatHeadField].as<int>(seat.head_turn);
  const int frame = node[kSeatFrameField].as<int>(seat.frame);
  if (direction >= 0 && direction < ro::ui::kSeatDirections) {
    seat.direction = direction;
  }
  if (animation >= 0 && animation <= ro::ui::kSeatAnimationHighest) {
    seat.animation = animation;
  }
  if (head >= ro::ui::kHeadAlongBody && head <= ro::ui::kHeadTurnMost) {
    seat.head_turn = head;
  }
  if (frame >= ro::ui::kAutomaticFrame && frame <= ro::ui::kSeatFrameHighest) {
    seat.frame = frame;
  }
  seat.animated = node[kSeatAnimatedField].as<bool>(seat.animated);
}

// UNE MISE EN PAGE, lue par-dessus `layout` : la racine du fichier ou une
// entrée enregistrée, qui portent les mêmes clés.
void ReadLayout(const YAML::Node& node, ro::ui::SeatLayout& layout) {
  if (const YAML::Node background = node[kBackgroundField]) {
    const std::string path = background.as<std::string>("");
    if (ro::ui::ValidBackgroundPath(path)) {
      layout.background = path;
    } else if (!path.empty()) {
      ro::core::LogWarning("tablée : décor ignoré (chemin refusé) — {}", path);
    }
  }
  layout.hide_free_seats = node[kHideFreeField].as<bool>(layout.hide_free_seats);
  const float alpha = node[kFreeAlphaField].as<float>(layout.free_seat_alpha);
  if (alpha >= 0.0F && alpha <= 1.0F) {
    layout.free_seat_alpha = alpha;
  }
  if (const YAML::Node seats = node[kSeatsField]; seats && seats.IsSequence()) {
    const std::size_t count = std::min<std::size_t>(seats.size(), ro::ui::kSeatCount);
    for (std::size_t rank = 0; rank < count; ++rank) {
      if (seats[rank].IsMap()) {
        ReadSeat(seats[rank], layout.seats[rank]);
      }
    }
  }
  if (const YAML::Node anchors = node[kAnchorsField]; anchors && anchors.IsSequence()) {
    // ⚠ LE NŒUD FAIT FOI : une mise en page enregistrée remplace le registre.
    layout.anchors.clear();
    for (const YAML::Node& entry : anchors) {
      if (layout.anchors.size() >= ro::ui::kSeatAnchorsMax || !entry.IsMap()) {
        continue;
      }
      const std::string name = entry[kAnchorNameField].as<std::string>("");
      const float x = entry[kSeatXField].as<float>(0.0F);
      const float y = entry[kSeatYField].as<float>(0.0F);
      if (name.empty() || name.size() > ro::ui::kSeatAnchorNameMax ||
          !ro::ui::ValidSeatFraction(x) || !ro::ui::ValidSeatFraction(y)) {
        continue;
      }
      layout.anchors.push_back(ro::ui::SeatAnchor{.name = name, .x = x, .y = y});
    }
  }
}

void EmitLayout(YAML::Emitter& out, const ro::ui::SeatLayout& layout) {
  out << YAML::Key << kBackgroundField << YAML::Value << layout.background;
  out << YAML::Key << kHideFreeField << YAML::Value << layout.hide_free_seats;
  out << YAML::Key << kFreeAlphaField << YAML::Value << layout.free_seat_alpha;
  out << YAML::Key << kSeatsField << YAML::Value << YAML::BeginSeq;
  for (const ro::ui::CharacterSeat& seat : layout.seats) {
    out << YAML::Flow << YAML::BeginMap;
    out << YAML::Key << kSeatXField << YAML::Value << seat.x;
    out << YAML::Key << kSeatYField << YAML::Value << seat.y;
    out << YAML::Key << kSeatHeightField << YAML::Value << seat.height;
    out << YAML::Key << kSeatDirectionField << YAML::Value << seat.direction;
    out << YAML::Key << kSeatAnimationField << YAML::Value << seat.animation;
    out << YAML::Key << kSeatHeadField << YAML::Value << seat.head_turn;
    out << YAML::Key << kSeatAnimatedField << YAML::Value << seat.animated;
    out << YAML::Key << kSeatFrameField << YAML::Value << seat.frame;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
  out << YAML::Key << kAnchorsField << YAML::Value << YAML::BeginSeq;
  for (const ro::ui::SeatAnchor& anchor : layout.anchors) {
    out << YAML::Flow << YAML::BeginMap;
    out << YAML::Key << kAnchorNameField << YAML::Value << anchor.name;
    out << YAML::Key << kSeatXField << YAML::Value << anchor.x;
    out << YAML::Key << kSeatYField << YAML::Value << anchor.y;
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
}

// Le nom sans ses espaces de bordure : « salon » et « salon  » ne doivent pas
// faire deux entrées que le joueur ne saurait pas distinguer.
[[nodiscard]] std::string_view Trimmed(std::string_view name) {
  const auto blank = [](char letter) { return letter == ' ' || letter == '\t'; };
  while (!name.empty() && blank(name.front())) {
    name.remove_prefix(1);
  }
  while (!name.empty() && blank(name.back())) {
    name.remove_suffix(1);
  }
  return name;
}

[[nodiscard]] bool ReadText(const std::filesystem::path& path, std::string& text) {
  std::error_code code;
  if (!std::filesystem::exists(path, code)) {
    return false;
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
  return true;
}

}  // namespace

SeatLayoutBook ParseSeatLayout(std::string_view yaml_text) {
  SeatLayoutBook book;
  try {
    const YAML::Node root = YAML::Load(std::string(yaml_text));
    if (!root.IsMap()) {
      return book;
    }
    ReadLayout(root, book.current);
    if (const YAML::Node presets = root[kPresetsField]; presets && presets.IsSequence()) {
      for (const YAML::Node& entry : presets) {
        if (book.presets.size() >= ro::ui::kPresetsMax || !entry.IsMap()) {
          continue;
        }
        const std::string name = entry[kPresetNameField].as<std::string>("");
        if (name.empty() || name.size() > kPresetNameMax) {
          continue;
        }
        // ⚠ SUR UN SOCLE D'USINE : une entrée amputée reste cohérente.
        SeatLayoutPreset preset{.name = name, .layout = {}};
        ReadLayout(entry, preset.layout);
        book.presets.push_back(std::move(preset));
      }
    }
  } catch (const std::exception& trouble) {
    ro::core::LogWarning("tablée : mise en page illisible ({}), retour à l'origine",
                         trouble.what());
    return SeatLayoutBook{};
  }
  return book;
}

std::string EmitSeatLayout(const SeatLayoutBook& book) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kVersionField << YAML::Value << kFormatVersion;
  EmitLayout(out, book.current);
  out << YAML::Key << kPresetsField << YAML::Value << YAML::BeginSeq;
  for (const SeatLayoutPreset& preset : book.presets) {
    out << YAML::BeginMap;
    out << YAML::Key << kPresetNameField << YAML::Value << preset.name;
    EmitLayout(out, preset.layout);
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
  out << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

std::filesystem::path SeatLayoutPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kSeatLayoutFileName;
}

std::filesystem::path UserBackgroundDirectory(const std::filesystem::path& client_root) {
  return client_root / "data" / "texture" / "lobby";
}

std::filesystem::path MisplacedBackgroundDirectory(const std::filesystem::path& client_root) {
  return client_root / "data" / "lobby";
}

SeatLayoutBook LoadSeatLayout(const std::filesystem::path& client_root) {
  std::string text;
  if (ReadText(SeatLayoutPath(client_root), text)) {
    return ParseSeatLayout(text);
  }
  // 🔴 `bourgeon_charselect_layout.yaml`, TANT QUE LE NÔTRE N'EXISTE PAS : la
  // salle qu'il porte suit le joueur ici. Le nôtre prend le relais dès la
  // première écriture, et l'autre n'est jamais touché.
  if (ReadText(client_root / kLegacySeatLayoutFileName, text)) {
    ro::core::LogInfo("tablée : mise en page reprise de {}", kLegacySeatLayoutFileName);
    return ParseSeatLayout(text);
  }
  return SeatLayoutBook{};
}

bool SaveSeatLayout(const std::filesystem::path& client_root, const SeatLayoutBook& book) {
  const std::filesystem::path path = SeatLayoutPath(client_root);
  std::error_code unused;
  std::filesystem::create_directories(path.parent_path(), unused);
  if (auto written = ro::core::WriteFileAtomically(path, EmitSeatLayout(book)); !written) {
    ro::core::LogWarning("tablée : {}", ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

int StorePreset(std::vector<SeatLayoutPreset>& presets, std::string_view name,
                const ro::ui::SeatLayout& current) {
  const std::string_view kept = Trimmed(name);
  if (kept.empty() || kept.size() > kPresetNameMax) {
    return -1;
  }
  for (std::size_t rank = 0; rank < presets.size(); ++rank) {
    if (presets[rank].name == kept) {
      presets[rank].layout = current;
      return static_cast<int>(rank);
    }
  }
  if (presets.size() >= ro::ui::kPresetsMax) {
    ro::core::LogWarning("tablée : {} mises en page enregistrées, limite atteinte",
                         ro::ui::kPresetsMax);
    return -1;
  }
  presets.push_back(SeatLayoutPreset{.name = std::string(kept), .layout = current});
  return static_cast<int>(presets.size()) - 1;
}

bool ApplyPreset(const std::vector<SeatLayoutPreset>& presets, int index,
                 ro::ui::SeatLayout& into) {
  if (index < 0 || index >= static_cast<int>(presets.size())) {
    return false;
  }
  const ro::ui::SeatLayout& source = presets[static_cast<std::size_t>(index)].layout;
  into.seats = source.seats;
  into.anchors = source.anchors;
  into.background = source.background;
  return true;
}

bool DeletePreset(std::vector<SeatLayoutPreset>& presets, int index) {
  if (index < 0 || index >= static_cast<int>(presets.size())) {
    return false;
  }
  presets.erase(presets.begin() + index);
  return true;
}

BackgroundGallery ComposeGallery(std::span<const std::string> disk_files,
                                 std::span<const std::string> packed_files) {
  BackgroundGallery gallery;
  gallery.entries.push_back(
      ro::ui::LayoutBackground{.path = std::string(ro::ui::kBanquetBackground),
                               .label = {},
                               .origin = ro::ui::BackgroundOrigin::kFactory});

  std::vector<ro::ui::LayoutBackground> found;
  // Les clés déjà proposées : le nom de fichier seul, en minuscules.
  std::vector<std::string> seen;
  const auto offer = [&](std::string_view file, ro::ui::BackgroundOrigin origin) {
    if (file.empty() || !PrintableAscii(file) || found.size() >= kBackgroundsMax) {
      return false;
    }
    std::string key = ro::core::AsciiLower(file);
    if (!key.ends_with(kBackgroundSuffix) || std::ranges::find(seen, key) != seen.end()) {
      return false;
    }
    seen.push_back(std::move(key));
    found.push_back(
        ro::ui::LayoutBackground{.path = std::string(kUserBackgroundPrefix) + std::string(file),
                                 .label = LabelOf(file),
                                 .origin = origin});
    return true;
  };
  for (const std::string& file : disk_files) {
    offer(file, ro::ui::BackgroundOrigin::kDisk);
  }
  const std::string packed_folder = ro::core::AsciiLower(kUserBackgroundFolder);
  for (const std::string& path : packed_files) {
    // ⚠ PAS LES SOUS-DOSSIERS : un décor se nomme `lobby\<fichier>`, et un
    // chemin plus profond ne se retrouverait pas.
    const std::string lowered = ro::core::AsciiLower(path);
    if (!lowered.starts_with(packed_folder) ||
        lowered.find_first_of("\\/", packed_folder.size()) != std::string::npos) {
      continue;
    }
    if (offer(FileNameOf(path), ro::ui::BackgroundOrigin::kPacked)) {
      ++gallery.packed;
    }
  }
  std::ranges::sort(
      found, [](const ro::ui::LayoutBackground& left, const ro::ui::LayoutBackground& right) {
        return left.label < right.label;
      });
  gallery.entries.insert(gallery.entries.end(), found.begin(), found.end());
  return gallery;
}

}  // namespace moonclient
