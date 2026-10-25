#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/ui/skin_art.h"
#include "ro/ui/survivors_summary_window.h"

namespace ro::ui {

// LES RECORDS LOCAUX DU MODE SURVIVORS (U22, Q11), en deux onglets :
//
//   - « Parties libres » : les dix meilleures, avec leur Cursed Water ;
//   - « Défi de la semaine » : sa semaine, ses dix meilleures, puis le
//     meilleur score de chaque semaine passée ;
//   - « Parties classées » (Q8), les dix meilleures parties classées au
//     départ neutre, seulement quand la vue le demande : tant que le serveur
//     n'a pas ouvert le classement, l'onglet n'existe pas.
//
// Une ligne : rang, score, temps, niveau, issue, tués, Cursed Water (parties
// libres), date. La dernière partie jouée est surlignée ; une partie d'une
// version de règles plus ancienne est grisée, « ancien équilibrage » au
// survol. Le nom `survivors_ranking_window` reste au classement du serveur.
//
// La fenêtre se lit seulement : elle ne rend aucune intention. Tab change
// d'onglet, fenêtre au premier plan.

struct SurvivorsRecordRow {
  std::uint64_t score = 0;
  std::uint64_t survived_ms = 0;
  int level = 1;
  SurvivorsEndOutcome outcome = SurvivorsEndOutcome::kDied;
  std::uint32_t kills = 0;
  int cursed_water = 0;
  std::int64_t played_at_utc_s = 0;  // secondes depuis le 1970-01-01 UTC
  bool latest = false;               // la dernière partie jouée
  bool older_rules = false;          // jouée sous une version de règles plus ancienne
  // La semaine ISO d'un défi ; lue pour les semaines passées.
  int week_year = 0;
  int week_number = 0;
};

enum class SurvivorsRecordsTab : std::uint8_t { kFree, kChallenge, kRanked };

struct SurvivorsRecordsView {
  std::span<const SurvivorsRecordRow> free_runs;  // rangées, dix au plus
  std::span<const SurvivorsRecordRow> week_runs;  // la semaine en cours, rangées
  // L'onglet des parties classées : montré seulement si `ranked_shown`.
  bool ranked_shown = false;
  std::span<const SurvivorsRecordRow> ranked_runs;  // rangées, dix au plus
  // Une ligne par semaine passée, la plus récente d'abord.
  std::span<const SurvivorsRecordRow> past_weeks;
  // La semaine ISO en cours.
  int week_year = 0;
  int week_number = 0;
};

struct SurvivorsRecordsState {
  bool open = false;
  SurvivorsRecordsTab tab = SurvivorsRecordsTab::kFree;
};

inline constexpr const char* kSurvivorsRecordsId = "survivors_records";

void DrawSurvivorsRecordsWindow(SurvivorsRecordsState& state, const SurvivorsRecordsView& view,
                                const SkinArt& art);

// « Semaine 43 de 2026 », traduit.
inline constexpr std::size_t kSurvivorsWeekTextSize = 48;
[[nodiscard]] std::array<char, kSurvivorsWeekTextSize> SurvivorsWeekText(int year, int week);

}  // namespace ro::ui
