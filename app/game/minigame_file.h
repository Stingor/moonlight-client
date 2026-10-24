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
// vide est un enregistrement vierge ; un texte que le YAML refuse, ou dont la
// racine n'est pas une table, rend une erreur.
[[nodiscard]] ro::core::Result<ro::sim::MinigameRecord> ReadMinigameRecord(std::string_view text);
[[nodiscard]] std::string WriteMinigameRecord(const ro::sim::MinigameRecord& record);

// L'ENREGISTREMENT D'UNE PARTIE, et ce qu'on sait de son fichier.
struct MinigameRecordHeld {
  ro::sim::MinigameRecord record;
  // ⚠ LE FICHIER EXISTE SANS S'ÊTRE LAISSÉ LIRE (abîmé, ou tenu un instant par
  // un autre programme) : `record` est parti de zéro, et l'écrire effacerait
  // les trophées que le fichier porte.
  bool file_unread = false;
};

// Vierge, sans un mot, quand le fichier n'existe pas : c'est la première
// partie.
[[nodiscard]] MinigameRecordHeld LoadMinigameRecord(const std::filesystem::path& client_root);
// Un fichier encore illisible ne s'écrase pas : il se relit d'abord, et ce
// qu'il porte se fond dans `held.record` avant d'écrire. Rend faux quand rien
// n'a été écrit.
[[nodiscard]] bool SaveMinigameRecord(const std::filesystem::path& client_root,
                                      MinigameRecordHeld& held);

}  // namespace moonclient
