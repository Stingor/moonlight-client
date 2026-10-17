#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/core/error.h"
#include "ro/sim/stat_presets.h"

namespace moonclient {

// LES PRÉRÉGLAGES DE CARACTÉRISTIQUES SUR LE DISQUE, un fichier pour tous les
// comptes de l'installation, dans `savedata\`.
//
// ⚠ PAR COMPTE, ET NON PAR PERSONNAGE : un préréglage décrit une répartition
// — « AGI-crit 99 » —, que tous les personnages du compte peuvent viser. La
// clé est l'identifiant de compte que le serveur de connexion a donné.

inline constexpr std::string_view kStatPresetFileName = "stat_presets.yml";

// Les préréglages de chaque compte, par identifiant.
using StatPresetShelf = std::map<std::uint32_t, std::vector<ro::sim::StatPreset>>;

[[nodiscard]] std::filesystem::path StatPresetFilePath(const std::filesystem::path& client_root);

// LE TEXTE DU FICHIER, dans les deux sens, sans rien lire ni écrire.
//
// Une entrée sans nom, ou à laquelle manque une des six valeurs, se saute :
// un préréglage incomplet viserait zéro. Un texte que le YAML refuse rend une
// erreur, et le fichier ne doit alors pas être réécrit.
[[nodiscard]] ro::core::Result<StatPresetShelf> ReadStatPresetShelf(std::string_view text);
[[nodiscard]] std::string WriteStatPresetShelf(const StatPresetShelf& shelf);

// Les préréglages de ce compte ; aucun quand le fichier manque ou est abîmé.
[[nodiscard]] std::vector<ro::sim::StatPreset> LoadStatPresets(
    const std::filesystem::path& client_root, std::uint32_t account_id);

// Remplace les préréglages de ce compte et garde ceux des autres. Un fichier
// abîmé ne se réécrit pas : il porte ceux des autres comptes. Rend faux quand
// rien n'a été écrit.
[[nodiscard]] bool SaveStatPresets(const std::filesystem::path& client_root,
                                   std::uint32_t account_id,
                                   std::span<const ro::sim::StatPreset> presets);

}  // namespace moonclient
