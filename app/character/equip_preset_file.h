#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

#include "character/equip_presets.h"

namespace moonclient {

// Les presets d'équipement sur le disque, tous personnages dans un fichier.
//
// Le format est celui de `bourgeon_settings.yaml`, clé pour clé, la touche en
// code de touche virtuelle de Windows : ce fichier se relit tant que le nôtre
// n'existe pas — et ne s'écrit jamais.

inline constexpr std::string_view kEquipPresetFileName = "equip_presets.yml";
inline constexpr std::string_view kLegacySettingsFileName = "bourgeon_settings.yaml";

[[nodiscard]] std::filesystem::path EquipPresetFilePath(const std::filesystem::path& client_root);

[[nodiscard]] std::vector<EquipPreset> LoadEquipPresets(const std::filesystem::path& client_root,
                                                        std::uint32_t character_id);

// Remplace les presets de ce personnage et garde ceux des autres. Un fichier
// illisible ne se réécrit pas : il porte les presets des autres personnages.
// Rend faux quand rien n'a été écrit.
bool SaveEquipPresets(const std::filesystem::path& client_root, std::uint32_t character_id,
                      std::span<const EquipPreset> presets);

// La touche telle que le fichier l'écrit, et l'inverse. Zéro hors des touches
// qu'un preset accepte.
[[nodiscard]] int VirtualKeyOf(std::uint32_t key);
[[nodiscard]] std::uint32_t KeyOfVirtualKey(int virtual_key);

}  // namespace moonclient
