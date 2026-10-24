#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/i18n.h"

namespace ro::sim {

class Match3Game;
class PegGame;

// ── LES TROPHÉES DES MINI-JEUX ET LES MEILLEURS SCORES ───────────────────────
//
// 🔴 TENUS PAR LE CLIENT SEUL : le serveur ne voit pas une partie de
// mini-jeu, et un trophée ne rapporte rien. Ils se rangent avec les réglages
// du joueur, dans `savedata`.
//
// ⚠ UN TROPHÉE SE GAGNE PENDANT LA PARTIE, dès que sa condition tient — une
// cascade de quatre n'attend pas la fin des coups. Le meilleur score, lui, ne
// s'inscrit qu'à la fin : une partie abandonnée ne compte pas.

enum class MinigameTrophy : std::uint8_t {
  kRojeweledFinished,
  kRojeweledScore5000,
  kRojeweledScore15000,
  kRojeweledCombo4,
  kRoggleWon,
  kRoggleFlawless,
  kRoggleThreeCatches,
  kRoggleScore20000,
};

inline constexpr std::size_t kMinigameTrophyCount = 8;

struct MinigameTrophyText {
  MinigameTrophy trophy = MinigameTrophy::kRojeweledFinished;
  // La clé de fichier, stable d'une version à l'autre.
  const char* key = "";
  // Des textes à traduire.
  const char* name = "";
  const char* description = "";
};

// Le catalogue, dans l'ordre de l'énumération.
[[nodiscard]] std::span<const MinigameTrophyText, kMinigameTrophyCount> MinigameTrophies();

struct MinigameRecord {
  int rojeweled_best = 0;
  int roggle_best = 0;
  std::array<bool, kMinigameTrophyCount> unlocked = {};
  // LES CLÉS DE TROPHÉE QUE CE CATALOGUE NE CONNAÎT PAS, telles que lues : un
  // client plus récent les a gagnées, et elles se réécrivent avec le reste.
  std::vector<std::string> foreign_trophy_keys;

  [[nodiscard]] bool operator==(const MinigameRecord&) const = default;
  [[nodiscard]] bool Has(MinigameTrophy trophy) const noexcept {
    return unlocked[static_cast<std::size_t>(trophy)];
  }
};

// LES TROPHÉES QUE L'ÉTAT DE CETTE PARTIE GAGNE, et le meilleur score d'une
// partie finie. Rend les trophées gagnés à l'instant, dans l'ordre du
// catalogue ; vide quand rien ne change.
[[nodiscard]] std::vector<MinigameTrophy> RecordRojeweled(MinigameRecord& record,
                                                          const Match3Game& game);
[[nodiscard]] std::vector<MinigameTrophy> RecordRoggle(MinigameRecord& record, const PegGame& game);

// DEUX ENREGISTREMENTS EN UN : le meilleur de chaque score, chaque trophée
// gagné de l'un ou de l'autre, chaque clé étrangère une seule fois.
void MergeMinigameRecord(MinigameRecord& into, const MinigameRecord& from);

}  // namespace ro::sim
