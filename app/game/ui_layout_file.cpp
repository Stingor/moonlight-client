#include "game/ui_layout_file.h"

#include <array>
#include <optional>
#include <string>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/ui/anchor_side.h"

#include "game/minimap_layout_yaml.h"
#include "game/profile_yaml.h"
#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les champs du fichier. ⚠ GRAVÉS : les changer fait perdre à chacun ses
// dispositions, sans que rien ne le dise.
constexpr const char* kWindowsField = "windows";
constexpr const char* kKeyField = "key";
constexpr const char* kAnchorField = "anchor";
constexpr const char* kOffsetField = "offset";
constexpr const char* kSizeField = "size";
constexpr const char* kCollapsedField = "collapsed";
constexpr const char* kTogglesField = "toggles";
constexpr const char* kMinimapField = "minimap";

constexpr const char* kFileHeader =
    "# Les profils de disposition de l'interface.\n"
    "#\n"
    "# Sous « profiles », chaque profil nomme la place de ses fenêtres. Une place\n"
    "# s'ancre au bord le plus proche sur chaque axe — « anchor » : start, center\n"
    "# ou end, en largeur puis en hauteur — et « offset » en est la distance, en\n"
    "# pixels d'interface : divisés par l'échelle de l'interface. « size » de même.\n"
    "# « toggles » : les fenêtres ouvertes et épinglées.\n"
    "# « minimap » : sa place, sa taille, ses réglages, et si elle est affichée.\n"
    "# Un profil sans « minimap » reprend celle de navigation.yml.\n"
    "# Sous « characters », le profil que chaque personnage pose en entrant en jeu ;\n"
    "# sans fiche, c'est « Default ».\n";

[[nodiscard]] std::optional<ro::ui::AnchorSide> SideOf(const YAML::Node& node) {
  if (!node || !node.IsScalar()) {
    return std::nullopt;
  }
  return ro::ui::AnchorSideFromWord(node.Scalar());
}

[[nodiscard]] std::string WordOf(ro::ui::AnchorSide side) {
  return std::string(ro::ui::AnchorSideWord(side));
}

// Une paire de nombres, `[x, y]`.
[[nodiscard]] std::optional<std::array<float, 2>> PairOf(const YAML::Node& node) {
  if (!node || !node.IsSequence() || node.size() != 2) {
    return std::nullopt;
  }
  try {
    return std::array<float, 2>{node[0].as<float>(), node[1].as<float>()};
  } catch (const YAML::Exception&) {
    return std::nullopt;
  }
}

// Une place, ou rien quand un de ses champs manque ou se lit mal.
[[nodiscard]] std::optional<std::pair<std::string, ro::ui::WindowPlace>> ReadPlace(
    const YAML::Node& entry) {
  if (!entry.IsMap() || !entry[kKeyField] || !entry[kKeyField].IsScalar()) {
    return std::nullopt;
  }
  const YAML::Node anchor = entry[kAnchorField];
  if (!anchor || !anchor.IsSequence() || anchor.size() != 2) {
    return std::nullopt;
  }
  const std::optional<ro::ui::AnchorSide> across = SideOf(anchor[0]);
  const std::optional<ro::ui::AnchorSide> down = SideOf(anchor[1]);
  const std::optional<std::array<float, 2>> offset = PairOf(entry[kOffsetField]);
  const std::optional<std::array<float, 2>> size = PairOf(entry[kSizeField]);
  if (!across || !down || !offset || !size || (*size)[0] <= 0.0F || (*size)[1] <= 0.0F) {
    return std::nullopt;
  }
  ro::ui::WindowPlace place{.across = *across,
                            .down = *down,
                            .offset_x = (*offset)[0],
                            .offset_y = (*offset)[1],
                            .width = (*size)[0],
                            .height = (*size)[1]};
  if (const YAML::Node collapsed = entry[kCollapsedField]; collapsed && collapsed.IsScalar()) {
    place.collapsed = collapsed.as<bool>(false);
  }
  return std::pair{entry[kKeyField].Scalar(), place};
}

[[nodiscard]] std::optional<Named<ro::ui::LayoutProfile>> ReadProfile(const YAML::Node& entry) {
  if (!entry.IsMap() || !entry[kProfileNameField] || !entry[kProfileNameField].IsScalar()) {
    return std::nullopt;
  }
  Named<ro::ui::LayoutProfile> named;
  named.name = entry[kProfileNameField].Scalar();
  if (named.name.empty()) {
    return std::nullopt;
  }
  if (const YAML::Node windows = entry[kWindowsField]; windows && windows.IsSequence()) {
    for (const YAML::Node& window : windows) {
      if (auto place = ReadPlace(window)) {
        named.profile.places.insert_or_assign(std::move(place->first), place->second);
      }
    }
  }
  if (const YAML::Node toggles = entry[kTogglesField]; toggles && toggles.IsMap()) {
    for (const auto& toggle : toggles) {
      if (!toggle.first.IsScalar() || !toggle.second.IsScalar()) {
        continue;
      }
      try {
        named.profile.toggles.insert_or_assign(toggle.first.Scalar(), toggle.second.as<bool>());
      } catch (const YAML::Exception&) {
        continue;
      }
    }
  }
  // ⚠ UNE MINIMAP ILLISIBLE N'EMPORTE PAS LE PROFIL : il hérite alors, comme
  // un profil écrit avant qu'elle n'y entre.
  if (const YAML::Node minimap = entry[kMinimapField]; minimap && minimap.IsMap()) {
    ro::ui::MinimapLayout read;
    try {
      ReadMinimapLayout(minimap, read);
      named.profile.minimap = read;
    } catch (const YAML::Exception&) {
      named.profile.minimap.reset();
    }
  }
  return named;
}

void WritePlace(YAML::Emitter& out, const std::string& key, const ro::ui::WindowPlace& place) {
  out << YAML::BeginMap;
  out << YAML::Key << kKeyField << YAML::Value << YAML::DoubleQuoted << key;
  out << YAML::Key << kAnchorField << YAML::Value << YAML::Flow << YAML::BeginSeq
      << WordOf(place.across) << WordOf(place.down) << YAML::EndSeq;
  out << YAML::Key << kOffsetField << YAML::Value << YAML::Flow << YAML::BeginSeq << place.offset_x
      << place.offset_y << YAML::EndSeq;
  out << YAML::Key << kSizeField << YAML::Value << YAML::Flow << YAML::BeginSeq << place.width
      << place.height << YAML::EndSeq;
  if (place.collapsed) {
    out << YAML::Key << kCollapsedField << YAML::Value << true;
  }
  out << YAML::EndMap;
}

}  // namespace

std::filesystem::path UiLayoutPath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kUiLayoutFileName;
}

std::string WriteUiLayouts(const UiLayoutBook& book) {
  YAML::Emitter out;
  out << YAML::BeginMap;
  out << YAML::Key << kProfileVersionField << YAML::Value << kUiLayoutFileVersion;
  out << YAML::Key << kProfilesField << YAML::Value << YAML::BeginSeq;
  for (const Named<ro::ui::LayoutProfile>& named : book.profiles) {
    out << YAML::BeginMap;
    out << YAML::Key << kProfileNameField << YAML::Value << named.name;
    out << YAML::Key << kWindowsField << YAML::Value << YAML::BeginSeq;
    for (const auto& [key, place] : named.profile.places) {
      WritePlace(out, key, place);
    }
    out << YAML::EndSeq;
    out << YAML::Key << kTogglesField << YAML::Value << YAML::BeginMap;
    for (const auto& [name, on] : named.profile.toggles) {
      out << YAML::Key << name << YAML::Value << on;
    }
    out << YAML::EndMap;
    if (named.profile.minimap.has_value()) {
      out << YAML::Key << kMinimapField << YAML::Value;
      WriteMinimapLayout(out, *named.profile.minimap);
    }
    out << YAML::EndMap;
  }
  out << YAML::EndSeq;
  WriteBindings(out, book.characters);
  out << YAML::EndMap;
  return std::string(kFileHeader) + out.c_str() + "\n";
}

bool ReadUiLayouts(std::string_view text, UiLayoutBook& into) {
  const std::optional<YAML::Node> whole = OpenProfileText(std::string(text), kUiLayoutFileVersion);
  if (!whole.has_value()) {
    return false;
  }
  UiLayoutBook book;
  if (!*whole) {
    into = UiLayoutBook{};
    return true;
  }
  if (const YAML::Node profiles = (*whole)[kProfilesField]; profiles && profiles.IsSequence()) {
    for (const YAML::Node& entry : profiles) {
      std::optional<Named<ro::ui::LayoutProfile>> named = ReadProfile(entry);
      // ⚠ UN NOM EN DOUBLE GARDE LE PREMIER, et le plafond tient.
      if (named && book.Find(named->name) == nullptr && book.profiles.size() < kProfileMost) {
        book.profiles.push_back(std::move(*named));
      }
    }
  }
  book.characters = ReadBindings(*whole);
  into = std::move(book);
  return true;
}

bool LoadUiLayouts(const std::filesystem::path& client_root, UiLayoutBook& into) {
  const std::filesystem::path path = UiLayoutPath(client_root);
  const std::optional<std::string> text = ReadProfileFile(path);
  if (!text.has_value()) {
    ro::core::LogWarning("dispositions : {} ne s'ouvre pas", ro::core::Utf8FromPath(path));
    return false;
  }
  if (!ReadUiLayouts(*text, into)) {
    ro::core::LogWarning(
        "dispositions : {} illisible ou d'une version inconnue, il ne sera pas réécrit",
        ro::core::Utf8FromPath(path));
    return false;
  }
  return true;
}

bool SaveUiLayouts(const std::filesystem::path& client_root, const UiLayoutBook& from) {
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(UiLayoutPath(client_root), WriteUiLayouts(from));
      !written) {
    ro::core::LogWarning("dispositions : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

}  // namespace moonclient
