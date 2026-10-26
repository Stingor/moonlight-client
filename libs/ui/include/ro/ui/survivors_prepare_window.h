#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LA PRÉPARATION D'UNE PARTIE, dessinée dans l'image du jeu : le premier bolt
// parmi les trois (D2), l'aide des contrôles avec les lettres que porte le
// clavier du joueur (U10), et « Partir », grisé avec la raison du refus tant
// que la garde d'entrée refuse (U3).
//
// AVEC LA MÉTA (`meta`), ELLE DEVIENT L'ACCUEIL DU MODE (Q1) : deux onglets,
// « Partie libre » (le bolt, la Cursed Water et son bonus, le résumé du
// départ acheté) et « Défi de la semaine » (sa semaine, le temps qui reste, le
// bolt imposé, le départ neutre, le meilleur score) ; en pied le solde et
// quatre boutons qui ouvrent l'Église, la Kafra, le Codex et les records.
// Sans méta, la préparation reste celle d'avant.
//
// AVEC LE CLASSEMENT OUVERT PAR LE SERVEUR (`meta->ranking`, Q2, Q4, Q6), un
// troisième onglet « Partie classée » (départ neutre, le bolt au choix, la
// graine du serveur, le meilleur rang du joueur), la ligne « Classé si le
// serveur répond » au défi, la ligne d'état « Classement en ligne » sous les
// onglets, un cinquième bouton « Classement » et, en pied, le compteur des
// résultats en attente d'envoi (le personnage et la date de chacun au survol).
// 🔴 Sans `ranking`, AUCUN de ces éléments ne paraît : pas de « hors ligne »
// pour le joueur d'un serveur qui ne classe pas.
//
// Touches, fenêtre au premier plan : Tab change d'onglet, Entrée part.

// Un bolt proposé, déjà résolu par `app`.
struct SurvivorsBoltChoice {
  std::uint16_t skill_id = 0;
  ImTextureID icon = 0;
  const char* name = "";         // nom affiché, tiré des tables du jeu
  const char* description = "";  // ⚠ EN FRANÇAIS, marqué par `TrText` : traduit ici
};

enum class SurvivorsBoardTab : std::uint8_t { kFree, kChallenge, kRanked };
// Les lieux de la méta qu'ouvre l'accueil.
enum class SurvivorsMetaPlace : std::uint8_t { kChurch, kKafra, kCodex, kRecords };

// Un résultat de partie classée gardé au carnet, pas encore compté.
struct SurvivorsPendingRow {
  const char* character = "";  // le nom du personnage, jamais traduit
  std::int64_t ended_at_utc_s = 0;
};

// Ce que l'accueil montre du classement, présent seulement quand le serveur
// l'a ouvert pour ces règles.
struct SurvivorsPrepareRanking {
  int best_rank = 0;  // 0 : aucune partie acceptée sur le tableau classé
  std::uint64_t best_score = 0;
  // La raison qui grise l'onglet classé, dite au survol. ⚠ EN FRANÇAIS,
  // marquée par `TrText` ; nulle quand l'onglet s'ouvre.
  const char* refusal = nullptr;
  std::span<const SurvivorsPendingRow> pending;
};

// Ce que l'accueil montre de la méta, déjà calculé par `app`.
struct SurvivorsPrepareMeta {
  SurvivorsBoardTab board = SurvivorsBoardTab::kFree;
  std::uint64_t zeny = 0;  // le solde
  // Partie libre.
  int cursed_water = 0;            // le rang choisi
  int most_cursed_water = 0;       // 0 : aucune fiole
  int cursed_water_bonus_pct = 0;  // le score et les Zeny majorés au rang choisi
  int blessing_ranks = 0;          // les rangs bénis, toutes Bénédictions confondues
  int lives = 0;
  int charges = 0;  // les charges Kafra par partie, tous services confondus
  // Défi de la semaine.
  int week_year = 0;
  int week_number = 0;
  std::int64_t week_left_s = 0;  // jusqu'au lundi 00:00 UTC
  SurvivorsBoltChoice challenge_bolt;
  std::optional<std::uint64_t> week_best_score;
  std::optional<SurvivorsPrepareRanking> ranking;
};

struct SurvivorsPrepareView {
  std::span<const SurvivorsBoltChoice> bolts;
  std::uint16_t chosen_skill_id = 0;
  // La raison du refus d'entrée. ⚠ EN FRANÇAIS, marquée par `TrText` ; nulle
  // quand la partie peut commencer.
  const char* refusal = nullptr;
  // Les lettres aux places de W, A, S et D sur le clavier du joueur.
  const char* key_north = "W";
  const char* key_west = "A";
  const char* key_south = "S";
  const char* key_east = "D";
  std::optional<SurvivorsPrepareMeta> meta;
};

struct SurvivorsPrepareState {
  bool open = false;
};

struct SurvivorsPrepareAsk {
  std::optional<std::uint16_t> choose_skill_id;
  std::optional<SurvivorsBoardTab> choose_board;
  std::optional<int> choose_cursed_water;
  std::optional<SurvivorsMetaPlace> open_place;
  bool open_ranking = false;  // le classement mondial
  bool start = false;
};

inline constexpr const char* kSurvivorsPrepareId = "survivors_prepare";

[[nodiscard]] SurvivorsPrepareAsk DrawSurvivorsPrepareWindow(SurvivorsPrepareState& state,
                                                             const SurvivorsPrepareView& view,
                                                             const SkinArt& art);

}  // namespace ro::ui
