#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/window_name.h"

namespace ro::ui {

// LA RÉPUTATION : les factions, leurs points, et la jauge de chacune.
//
// Une faction monte d'un PALIER tous les mille points, vers le haut comme vers le
// bas : une lampe par palier possible, allumée en bleu pour une réputation
// positive, en rouge pour une négative. Sous les lampes, la barre dit où en est le
// palier suivant.
//
// 🔴 LA FENÊTRE NE FILTRE RIEN ELLE-MÊME : le groupe choisi et le texte cherché
// restent dans son état, et `app` compose à chaque image les factions qui y
// répondent. Choisir un groupe vide la recherche, comme chez le client d'origine.

inline constexpr const char* kReputationWindowId = "reputation";
inline constexpr const char* kReputationWindowTitle = core::TrText("Réputation");

// LA TAILLE À LA PREMIÈRE OUVERTURE, en pixels d'interface.
inline constexpr float kReputationFirstWidthPixels = 340.0F;
inline constexpr float kReputationFirstHeightPixels = 420.0F;

// Les points d'un palier.
inline constexpr std::int64_t kReputationPointsPerStep = 1000;

// Le texte cherché, octet nul compris.
inline constexpr std::size_t kReputationSearchBytes = 32;

// LA JAUGE D'UNE FACTION : ses paliers possibles du côté de ses points, ceux
// qu'elle a atteints, et ce qu'elle a fait du palier suivant.
struct ReputationGauge {
  bool negative = false;
  std::uint32_t steps = 0;
  std::uint32_t lit_steps = 0;
  // Les points au-delà du dernier palier atteint, de zéro à `kReputationPointsPerStep`.
  std::int64_t into_step = 0;
  // Tous les paliers sont atteints : la barre n'a plus rien à remplir.
  bool full = false;
};

// ⚠ LES POINTS SE BORNENT À LA FACTION : au-delà, la table et le serveur ne disent
// plus la même chose, et la jauge ne déborde pas.
[[nodiscard]] ReputationGauge ReputationGaugeOf(std::int64_t points, std::int64_t least,
                                                std::int64_t most);

// UNE FACTION, telle que la fenêtre la montre.
struct ReputationRowView {
  std::string_view name;
  std::int64_t points = 0;
  // Les bornes de ses points : `least` négatif ou nul, `most` positif ou nul.
  std::int64_t least = 0;
  std::int64_t most = 0;
  // La faction qu'un script vient de désigner.
  bool highlighted = false;
};

// CE QUE LA FENÊTRE MONTRE, composé par `app` à chaque image.
struct ReputationView {
  // Les noms des groupes, dans l'ordre de la liste ; le rang zéro de la liste est
  // « toutes les factions », que la fenêtre ajoute elle-même.
  std::span<const std::string_view> groups;
  std::span<const ReputationRowView> rows;
  // La somme des points du groupe choisi ; sans objet sous « toutes ».
  std::int64_t group_points = 0;
  // Les points ne sont pas encore arrivés du serveur.
  bool awaiting_points = false;
  // La table des factions manque à l'installation.
  bool book_missing = false;
};

// Le rang du groupe « toutes les factions » dans la liste.
inline constexpr int kReputationAllGroups = 0;

struct ReputationWindowState {
  bool open = false;
  bool was_open = false;
  // Le rang choisi dans la liste : `kReputationAllGroups`, ou le groupe
  // `groups[group - 1]` de la vue.
  int group = kReputationAllGroups;
  std::array<char, kReputationSearchBytes> search{};
  WindowName name{kReputationWindowTitle, kReputationWindowId};
};

void DrawReputationWindow(ReputationWindowState& state, const ReputationView& view,
                          const SkinArt& art);

}  // namespace ro::ui
