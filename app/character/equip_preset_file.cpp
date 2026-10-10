#include "character/equip_preset_file.h"

#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_keycode.h>
#include <yaml-cpp/yaml.h>

#include "ro/core/error.h"
#include "ro/core/file_write.h"
#include "ro/core/log.h"

#include "game/save_folder.h"

namespace moonclient {
namespace {

// Les clés partagées avec `bourgeon_settings.yaml` : en changer une rendrait
// chacun des deux fichiers illisible pour l'autre.
constexpr const char* kLegacyRootField = "moonlight_ui";
constexpr const char* kPresetsField = "equip_presets";
constexpr const char* kCharacterField = "cid";
constexpr const char* kNameField = "name";
constexpr const char* kKeyField = "hkvk";
constexpr const char* kCtrlField = "hkc";
constexpr const char* kAltField = "hka";
constexpr const char* kShiftField = "hks";
constexpr const char* kCostumesField = "costumes";
constexpr const char* kPiecesField = "items";
constexpr const char* kItemField = "id";
constexpr const char* kRefineField = "refine";
constexpr const char* kGradeField = "grade";
constexpr const char* kLeftHandField = "left";
constexpr const char* kFamilyField = "kind";
constexpr const char* kCardsField = "cards";

// La famille telle que le fichier la numérote ; absente ou inconnue, c'est de
// l'équipement.
constexpr int kEquipmentFamily = 0;
constexpr int kCostumeFamily = 1;
constexpr int kAmmoFamily = 2;

// Les quatre plages de touches virtuelles de Windows qu'un preset accepte.
constexpr int kVirtualKeyA = 0x41;
constexpr int kVirtualKeyZ = 0x5A;
constexpr int kVirtualKey0 = 0x30;
constexpr int kVirtualKey9 = 0x39;
constexpr int kVirtualKeyF1 = 0x70;
constexpr int kVirtualKeyF12 = 0x7B;
constexpr int kVirtualKeySpace = 0x20;

constexpr const char* kFileHeader =
    "# Les presets d'equipement de moonclient, tous personnages confondus.\n"
    "#\n"
    "# Meme format que bourgeon_settings.yaml, cle pour cle : cid est le personnage,\n"
    "# hkvk la touche du raccourci en code de touche virtuelle de Windows (0 :\n"
    "# aucune), hkc / hka / hks ce qu'on tient avec (Ctrl, Alt, Maj), costumes\n"
    "# si le preset pilote les costumes.\n"
    "#\n"
    "# Une piece : id l'objet, refine et grade, cards les quatre cartes, left une\n"
    "# arme de la main gauche, kind sa famille (0 equipement, 1 costume,\n"
    "# 2 munition).\n";

// Un preset tel que le fichier le range : avec son personnage.
struct Stored {
  std::uint32_t character_id = 0;
  EquipPreset preset;
};

template <typename T>
[[nodiscard]] T ScalarOr(const YAML::Node& node, T fallback) {
  if (!node || !node.IsScalar()) {
    return fallback;
  }
  return node.as<T>(fallback);
}

[[nodiscard]] PresetFamily FamilyOf(int number) {
  switch (number) {
    case kCostumeFamily:
      return PresetFamily::kCostume;
    case kAmmoFamily:
      return PresetFamily::kAmmo;
    default:
      return PresetFamily::kEquipment;
  }
}

[[nodiscard]] int NumberOf(PresetFamily family) {
  switch (family) {
    case PresetFamily::kEquipment:
      return kEquipmentFamily;
    case PresetFamily::kCostume:
      return kCostumeFamily;
    case PresetFamily::kAmmo:
      return kAmmoFamily;
  }
  return kEquipmentFamily;
}

[[nodiscard]] PresetPiece ReadPiece(const YAML::Node& entry) {
  PresetPiece piece;
  piece.item_id = ScalarOr<std::uint32_t>(entry[kItemField], 0);
  piece.refine_level = ScalarOr<int>(entry[kRefineField], 0);
  piece.grade = ScalarOr<int>(entry[kGradeField], 0);
  piece.left_hand = ScalarOr<bool>(entry[kLeftHandField], false);
  piece.family = FamilyOf(ScalarOr<int>(entry[kFamilyField], kEquipmentFamily));
  if (const YAML::Node cards = entry[kCardsField]; cards && cards.IsSequence()) {
    for (std::size_t slot = 0; slot < piece.cards.size() && slot < cards.size(); ++slot) {
      piece.cards[slot] = ScalarOr<std::uint32_t>(cards[slot], 0);
    }
  }
  return piece;
}

// Une entrée sans nom se saute : c'est le nom qui désigne un preset.
[[nodiscard]] std::vector<Stored> ReadList(const YAML::Node& list) {
  std::vector<Stored> all;
  if (!list || !list.IsSequence()) {
    return all;
  }
  for (const YAML::Node& entry : list) {
    if (!entry.IsMap()) {
      continue;
    }
    const YAML::Node name = entry[kNameField];
    if (!name || !name.IsScalar() || name.Scalar().empty()) {
      continue;
    }
    Stored kept;
    kept.character_id = ScalarOr<std::uint32_t>(entry[kCharacterField], 0);
    kept.preset.name = name.Scalar();
    const std::uint32_t key = KeyOfVirtualKey(ScalarOr<int>(entry[kKeyField], 0));
    // Des modificateurs sans touche ne font pas un raccourci.
    if (key != 0) {
      kept.preset.hotkey = Binding{.key = key,
                                   .ctrl = ScalarOr<bool>(entry[kCtrlField], false),
                                   .shift = ScalarOr<bool>(entry[kShiftField], false),
                                   .alt = ScalarOr<bool>(entry[kAltField], false)};
    }
    kept.preset.with_costumes = ScalarOr<bool>(entry[kCostumesField], false);
    if (const YAML::Node pieces = entry[kPiecesField]; pieces && pieces.IsSequence()) {
      for (const YAML::Node& piece : pieces) {
        if (piece.IsMap()) {
          kept.preset.pieces.push_back(ReadPiece(piece));
        }
      }
    }
    all.push_back(std::move(kept));
  }
  return all;
}

enum class FileState : std::uint8_t { kMissing, kBroken, kRead };

struct FileRead {
  FileState state = FileState::kMissing;
  std::vector<Stored> presets;
};

// La liste rangée sous `root`, s'il y en a un, puis `equip_presets`. Un
// fichier abîmé n'empêche pas de jouer : son exception s'attrape.
[[nodiscard]] FileRead ReadFile(const std::filesystem::path& path, const char* root) {
  std::error_code missing;
  if (!std::filesystem::exists(path, missing)) {
    return {};
  }
  try {
    const YAML::Node whole = YAML::LoadFile(path.string());
    // Une racine qui n'est pas une table n'a pas de champs : l'indexer lèverait.
    const YAML::Node base = root == nullptr ? whole : (whole.IsMap() ? whole[root] : YAML::Node());
    if (!base || !base.IsMap()) {
      return FileRead{.state = FileState::kRead};
    }
    return FileRead{.state = FileState::kRead, .presets = ReadList(base[kPresetsField])};
  } catch (const YAML::Exception& broken) {
    ro::core::LogWarning("presets d'équipement : {} illisible ({})", path.string(), broken.what());
    return FileRead{.state = FileState::kBroken};
  }
}

// Notre fichier, ou `bourgeon_settings.yaml` tant que le nôtre n'existe pas.
[[nodiscard]] FileRead ReadStored(const std::filesystem::path& client_root) {
  FileRead ours = ReadFile(EquipPresetFilePath(client_root), nullptr);
  if (ours.state != FileState::kMissing) {
    return ours;
  }
  FileRead theirs = ReadFile(client_root / kLegacySettingsFileName, kLegacyRootField);
  // Le sien abîmé ne bloque rien : on ne l'écrit jamais.
  if (theirs.state == FileState::kBroken) {
    theirs.state = FileState::kMissing;
  }
  return theirs;
}

void Emit(YAML::Emitter& out, std::uint32_t character_id, const EquipPreset& preset) {
  const int key = VirtualKeyOf(preset.hotkey.key);
  const bool bound = key != 0;
  out << YAML::BeginMap << YAML::Key << kCharacterField << YAML::Value << character_id << YAML::Key
      << kNameField << YAML::Value << preset.name << YAML::Key << kKeyField << YAML::Value << key
      << YAML::Key << kCtrlField << YAML::Value << (bound && preset.hotkey.ctrl) << YAML::Key
      << kAltField << YAML::Value << (bound && preset.hotkey.alt) << YAML::Key << kShiftField
      << YAML::Value << (bound && preset.hotkey.shift) << YAML::Key << kCostumesField << YAML::Value
      << preset.with_costumes << YAML::Key << kPiecesField << YAML::Value << YAML::BeginSeq;
  for (const PresetPiece& piece : preset.pieces) {
    out << YAML::BeginMap << YAML::Key << kItemField << YAML::Value << piece.item_id << YAML::Key
        << kRefineField << YAML::Value << piece.refine_level << YAML::Key << kGradeField
        << YAML::Value << piece.grade << YAML::Key << kLeftHandField << YAML::Value
        << piece.left_hand << YAML::Key << kFamilyField << YAML::Value << NumberOf(piece.family)
        << YAML::Key << kCardsField << YAML::Value << YAML::Flow << YAML::BeginSeq;
    for (const std::uint32_t card : piece.cards) {
      out << card;
    }
    out << YAML::EndSeq << YAML::EndMap;
  }
  out << YAML::EndSeq << YAML::EndMap;
}

}  // namespace

std::filesystem::path EquipPresetFilePath(const std::filesystem::path& client_root) {
  return client_root / kSaveFolder / kEquipPresetFileName;
}

std::vector<EquipPreset> LoadEquipPresets(const std::filesystem::path& client_root,
                                          std::uint32_t character_id) {
  std::vector<EquipPreset> mine;
  for (Stored& kept : ReadStored(client_root).presets) {
    if (kept.character_id == character_id) {
      mine.push_back(std::move(kept.preset));
    }
  }
  return mine;
}

bool SaveEquipPresets(const std::filesystem::path& client_root, std::uint32_t character_id,
                      std::span<const EquipPreset> presets) {
  FileRead stored = ReadStored(client_root);
  if (stored.state == FileState::kBroken) {
    ro::core::LogWarning(
        "presets d'équipement : {} n'est pas réécrit tant qu'il reste illisible — il porte "
        "ceux des autres personnages",
        EquipPresetFilePath(client_root).string());
    return false;
  }
  // Ses presets prennent la place du premier des anciens : un personnage ne
  // saute pas d'un bout du fichier à l'autre à chaque enregistrement.
  YAML::Emitter out;
  out << YAML::BeginMap << YAML::Key << kPresetsField << YAML::Value << YAML::BeginSeq;
  bool placed = false;
  const auto emit_mine = [&] {
    for (const EquipPreset& preset : presets) {
      Emit(out, character_id, preset);
    }
    placed = true;
  };
  for (const Stored& kept : stored.presets) {
    if (kept.character_id != character_id) {
      Emit(out, kept.character_id, kept.preset);
    } else if (!placed) {
      emit_mine();
    }
  }
  if (!placed) {
    emit_mine();
  }
  out << YAML::EndSeq << YAML::EndMap;
  const std::string text = std::string(kFileHeader) + out.c_str() + "\n";
  if (const ro::core::Result<void> written =
          ro::core::WriteFileAtomically(EquipPresetFilePath(client_root), text);
      !written) {
    ro::core::LogWarning("presets d'équipement : écriture impossible ({})",
                         ro::core::Describe(written.error()));
    return false;
  }
  return true;
}

int VirtualKeyOf(std::uint32_t key) {
  if (key >= SDLK_A && key <= SDLK_Z) {
    return kVirtualKeyA + static_cast<int>(key - SDLK_A);
  }
  if (key >= SDLK_0 && key <= SDLK_9) {
    return kVirtualKey0 + static_cast<int>(key - SDLK_0);
  }
  if (key >= SDLK_F1 && key <= SDLK_F12) {
    return kVirtualKeyF1 + static_cast<int>(key - SDLK_F1);
  }
  return key == SDLK_SPACE ? kVirtualKeySpace : 0;
}

std::uint32_t KeyOfVirtualKey(int virtual_key) {
  if (virtual_key >= kVirtualKeyA && virtual_key <= kVirtualKeyZ) {
    return SDLK_A + static_cast<std::uint32_t>(virtual_key - kVirtualKeyA);
  }
  if (virtual_key >= kVirtualKey0 && virtual_key <= kVirtualKey9) {
    return SDLK_0 + static_cast<std::uint32_t>(virtual_key - kVirtualKey0);
  }
  if (virtual_key >= kVirtualKeyF1 && virtual_key <= kVirtualKeyF12) {
    return SDLK_F1 + static_cast<std::uint32_t>(virtual_key - kVirtualKeyF1);
  }
  return virtual_key == kVirtualKeySpace ? static_cast<std::uint32_t>(SDLK_SPACE) : 0;
}

}  // namespace moonclient
