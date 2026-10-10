#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ro::vfs {

struct ItemWords;

// LES MOTS DE LA FENÊTRE DES SUCCÈS : des textes de l'interface, posés par
// `FillAchievementWords` (ADR 0090).
//
// ⚠ LES LIBELLÉS D'ONGLETS PORTENT UN ESPACE FINAL, comme dans le client.
enum class AchievementWord : std::uint8_t {
  kWindowTitle,
  kSummaryTitle,
  kRecent,
  kFilterAll,
  kFilterUnclaimed,
  kTrackingFull,
  kCompleted,  // un %s
  // ── Les onglets et leurs sous-onglets, dans l'ordre de l'arbre ────────────
  kTabSummary,
  kTabGeneral,
  kTabGeneralCharacter,
  kTabGeneralAction,
  kTabGeneralRest,
  kTabAdventure,
  kTabAdventureMidgarts,
  kTabAdventureSchwarzwald,
  kTabAdventureArunafeltz,
  kTabAdventureNewWorld,
  kTabAdventureLocalizing,
  kTabAdventureDungeon,
  kTabBattle,
  kTabBattlePvp,
  kTabBattleTraining,
  kTabQuest,
  kTabQuestEpisode,
  kTabQuestGeneral,
  kTabMemorial,
  kTabMemorialMidgard,
  kTabMemorialNewWorld,
  kTabMemorialRest,
  kTabFeat,
  kCount,
};

inline constexpr std::size_t kAchievementWordCount =
    static_cast<std::size_t>(AchievementWord::kCount);

// Les textes posés, rangés par `AchievementWord`.
struct AchievementWords {
  std::array<std::string, kAchievementWordCount> texts;
};

// Le texte d'un mot, sans conversion. ⚠ `kCompleted` EST UN GABARIT : il se lit
// par `AchievementCompletedText`.
[[nodiscard]] std::string_view AchievementText(const ItemWords& words, AchievementWord word);

// « < nom > achieved. » — vide si le gabarit ne porte pas exactement un « %s ».
// Le nom est un ARGUMENT, jamais un gabarit.
[[nodiscard]] std::string AchievementCompletedText(const ItemWords& words, std::string_view name);

// Pose ces mots dans la langue courante.
void FillAchievementWords(AchievementWords& words);

}  // namespace ro::vfs
