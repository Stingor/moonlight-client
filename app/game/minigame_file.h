#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/sim/minigame_trophies.h"

namespace moonclient {

// LES MEILLEURS SCORES ET LES TROPHÉES DES MINI-JEUX, dans `savedata\`.
//
// ⚠ UN SEUL POUR TOUTE L'INSTALLATION : c'est le joueur devant l'écran qui
// joue, pas un compte ni un personnage.
//
// Un trophée se range par sa CLÉ, jamais par son rang : le catalogue peut
// grandir, et une clé que ce client ne connaît pas se lit sans rien gagner.
// ⚠ ELLE SE RÉÉCRIT TELLE QUELLE : revenir à une version antérieure du client
// ne fait pas perdre les trophées d'une plus récente.

inline constexpr std::string_view kMinigameFileName = "minigames.yml";

[[nodiscard]] std::filesystem::path MinigameFilePath(const std::filesystem::path& client_root);

// LE TEXTE DU FICHIER, dans les deux sens, sans rien lire ni écrire. Un texte
// que le YAML refuse rend une erreur.
[[nodiscard]] ro::core::Result<ro::sim::MinigameRecord> ReadMinigameRecord(std::string_view text);
[[nodiscard]] std::string WriteMinigameRecord(const ro::sim::MinigameRecord& record);

// L'enregistrement de l'installation ; vierge quand le fichier manque ou est
// abîmé.
[[nodiscard]] ro::sim::MinigameRecord LoadMinigameRecord(const std::filesystem::path& client_root);
// Rend faux quand rien n'a été écrit.
[[nodiscard]] bool SaveMinigameRecord(const std::filesystem::path& client_root,
                                      const ro::sim::MinigameRecord& record);

}  // namespace moonclient
