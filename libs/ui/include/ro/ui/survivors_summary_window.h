#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/survivors_album_window.h"
#include "ro/ui/survivors_prepare_window.h"
#include "ro/ui/survivors_ranking_window.h"

namespace ro::ui {

// LA FIN D'UNE PARTIE, en deux temps.
//
//   1. Le voile de la mort (U15) — « Vous êtes mort » sur l'image passée en
//      noir et blanc par la teinte d'écran d'`app` — ou le bandeau
//      « Victoire ! » (Q7). L'un comme l'autre se passent d'un clic.
//   2. L'écran de fin (U16) : temps, niveau, job atteint ; tués par espèce ;
//      dégâts par skill en barres ; cartes portées ; Zeny ; Rejouer, Retour.
//
// AVEC LE CRÉDIT DU CARNET (`credit`), il dit aussi ce que la partie a
// rapporté : Zeny versés et solde, record, cartes tombées (« NOUVELLE » pour
// une première), exploits atteints ou leur progression, et un bouton
// « Église ». Les succès survivors gagnés (Q13 : une ligne par succès, son
// icône) se montrent avec ou sans lui.
//
// AVEC LE CLASSEMENT OUVERT PAR LE SERVEUR (`ranking`), la marque « Partie
// classée » ou « Partie libre » en tête (U16) ; pour une partie classée, le
// score du serveur, la ligne du rang qui passe de « Envoi au serveur… » au
// verdict (Q5), et un bouton « Classement » qui ouvre le tableau mondial sur
// l'onglet de la partie quand il peut s'ouvrir. Sans `ranking`, rien de cela
// ne paraît.

enum class SurvivorsEndPhase : std::uint8_t { kDeathVeil, kVictoryBanner, kSummary };
enum class SurvivorsEndOutcome : std::uint8_t { kDied, kVictory, kAbandoned };

// Les lignes de l'écran de fin, déjà résolues par `app`, dans l'ordre du
// récapitulatif.
struct SurvivorsFoeRow {
  const char* name = "";  // nom de l'espèce, tiré des tables du jeu
  std::uint64_t kills = 0;
};
struct SurvivorsSkillDamageRow {
  ImTextureID icon = 0;
  const char* name = "";
  std::uint64_t damage = 0;
};
struct SurvivorsCardRow {
  ImTextureID illustration = 0;
  ImVec2 illustration_size = ImVec2(0.0F, 0.0F);
  const char* name = "";
  int level = 0;
};

// Une carte tombée et ramassée dans la partie.
struct SurvivorsDroppedCardRow {
  ImTextureID illustration = 0;
  ImVec2 illustration_size = ImVec2(0.0F, 0.0F);
  const char* name = "";
  bool first_time = false;  // entrée dans l'Album par cette partie
};
// Un succès survivors gagné dans la partie.
struct SurvivorsTrophyRow {
  ImTextureID icon = 0;
  const char* name = "";  // ⚠ EN FRANÇAIS, marqué par `TrText` : traduit ici
};

// Ce que la partie devient au tableau des records.
enum class SurvivorsRecordNews : std::uint8_t {
  kPlaced,      // inscrite, à `record_rank`
  kBelowBoard,  // hors des dix meilleures
  kNotRanked,   // abandonnée ou interrompue : jamais aux records
  kRefused,     // un défi au départ non neutre, ou d'une autre semaine
};

struct SurvivorsCreditRows {
  std::uint64_t zeny_gained = 0;
  std::uint64_t zeny_balance = 0;
  SurvivorsBoardTab board = SurvivorsBoardTab::kFree;
  SurvivorsRecordNews record = SurvivorsRecordNews::kBelowBoard;
  int record_rank = 0;                         // 1 en tête
  std::optional<std::uint64_t> score_to_beat;  // le dixième, lu hors du tableau
  std::span<const SurvivorsDroppedCardRow> dropped;
  std::span<const SurvivorsFeatEntry> feats;
};

// Ce que l'écran de fin dit du classement.
struct SurvivorsSummaryRanking {
  bool ranked = false;  // « Partie classée » ; faux : « Partie libre », et rien d'autre
  SurvivorsRankingTab tab = SurvivorsRankingTab::kRanked;  // le tableau de la partie
  std::optional<std::uint64_t> server_score;               // le score que le serveur a recalculé
  SurvivorsRankStanding standing;
  // Le tableau mondial peut s'ouvrir : sans lui, ni bouton « Classement ».
  bool board_reachable = false;
};

struct SurvivorsSummaryView {
  SurvivorsEndPhase phase = SurvivorsEndPhase::kSummary;
  SurvivorsEndOutcome outcome = SurvivorsEndOutcome::kDied;
  bool accepts_input = false;
  std::uint64_t survived_ms = 0;
  int level = 1;
  const char* job_name = "";  // tiré des tables du jeu
  std::span<const SurvivorsFoeRow> foes;
  std::span<const SurvivorsSkillDamageRow> skills;
  std::span<const SurvivorsCardRow> cards;
  std::uint32_t zeny = 0;
  std::optional<SurvivorsCreditRows> credit;
  // Les succès survivors gagnés (Q13), montrés même sans crédit.
  std::span<const SurvivorsTrophyRow> trophies;
  std::optional<SurvivorsSummaryRanking> ranking;
};

struct SurvivorsSummaryAsk {
  bool proceed = false;  // le voile ou le bandeau passé d'un clic
  bool replay = false;
  bool back = false;
  bool church = false;                              // ouvrir l'Église par-dessus l'écran de fin
  std::optional<SurvivorsRankingTab> open_ranking;  // le tableau mondial, sur cet onglet
};

[[nodiscard]] SurvivorsSummaryAsk DrawSurvivorsSummaryWindow(const SurvivorsSummaryView& view,
                                                             const SkinArt& art);

}  // namespace ro::ui
