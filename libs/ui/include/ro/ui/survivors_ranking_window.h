#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include <imgui.h>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE CLASSEMENT MONDIAL DU MODE SURVIVORS (Q4), tenu par le serveur : une
// fenêtre à part, que l'accueil ouvre par « Classement » et l'écran de fin
// d'une partie classée par le sien.
//
//   - deux onglets : « Partie classée » et « Défi de la semaine », la semaine
//     en tête du second ;
//   - vingt lignes au plus : rang, nom du personnage, score, temps survécu,
//     niveau. La ligne du joueur est surlignée ; plus loin, elle s'épingle en
//     pied (« Vous : 57e — 31 870 ») ; sans partie acceptée, « Vous n'êtes pas
//     encore classé » ;
//   - en pied « Actualiser », grisé tant que la cadence du serveur ne permet
//     pas de redemander, « Fermer », et l'âge de la page ;
//   - la version des règles en petit sous le titre.
//
// Les noms viennent du serveur et se posent tels quels : un nom de joueur ne
// se traduit jamais. La fenêtre ne connaît aucun type de `sim` ni de `net` :
// `app` résout la page et ses codes en cette vue, et route les intentions.
//
// Touches, fenêtre au premier plan : Tab change d'onglet. Échap la ferme par
// le registre d'`app`.

enum class SurvivorsRankingTab : std::uint8_t { kRanked, kChallenge };

// Les lignes d'une page, au plus : le serveur rend les vingt premiers.
inline constexpr std::size_t kSurvivorsRankingMostRows = 20;
// Le fond de la ligne du joueur dans le tableau.
inline constexpr ImU32 kSurvivorsOwnRowBack = IM_COL32(255, 215, 90, 60);

// Une ligne du tableau, dans l'ordre du serveur.
struct SurvivorsRankingRow {
  int rank = 0;           // 1 en tête
  const char* name = "";  // tel que le serveur l'a figé, en UTF-8
  std::uint64_t score = 0;
  std::uint64_t survived_ms = 0;
  int level = 1;
};

// La page d'un onglet.
struct SurvivorsRankingPage {
  // Faux : aucune page reçue ; sa demande part dès que la cadence le permet.
  bool received = false;
  std::span<const SurvivorsRankingRow> rows;
  int own_rank = 0;  // 0 : aucune partie acceptée sur ce tableau
  std::uint64_t own_score = 0;
  std::int64_t age_s = 0;  // depuis la réception de la page
};

struct SurvivorsRankingView {
  SurvivorsRankingPage ranked;
  SurvivorsRankingPage challenge;
  int ruleset = 0;
  // La semaine ISO du défi telle que le serveur la compte.
  int week_year = 0;
  int week_number = 0;
  bool refresh_ready = true;  // faux pendant la cadence du serveur
};

struct SurvivorsRankingState {
  bool open = false;
  SurvivorsRankingTab tab = SurvivorsRankingTab::kRanked;
};

struct SurvivorsRankingAsk {
  bool refresh = false;
  bool tab_changed = false;  // l'onglet `state.tab` vient d'être choisi : sa page se demande
};

inline constexpr const char* kSurvivorsRankingId = "survivors_ranking";

[[nodiscard]] SurvivorsRankingAsk DrawSurvivorsRankingWindow(SurvivorsRankingState& state,
                                                             const SurvivorsRankingView& view,
                                                             const SkinArt& art);

// OÙ SE LIT LA PLACE DU JOUEUR dans une page reçue : sur sa ligne du tableau,
// épinglée en pied quand elle est plus loin, ou nulle part sans partie
// acceptée.
enum class SurvivorsOwnPlace : std::uint8_t { kInTable, kPinned, kUnranked };
[[nodiscard]] SurvivorsOwnPlace SurvivorsOwnPlaceOf(const SurvivorsRankingPage& page) noexcept;

// ───────────── Les textes du classement, par code ─────────────
//
// Les codes du fil sont résolus par `app` en ces énumérations ; `ui` en tient
// les phrases, un format littéral par code. Ils servent aux fenêtres comme aux
// lignes de conversation qu'`app` dit.

// Pourquoi une partie demandée classée part libre.
enum class SurvivorsTicketRefusal : std::uint8_t {
  kPlace,          // le lieu est refusé par le serveur
  kUnlinked,       // le compte n'est rattaché à aucun joueur
  kRules,          // version des règles ou tableau inconnus du serveur
  kTooSoon,        // une demande trop tôt après la précédente
  kSilent,         // aucune réponse dans le délai
  kChallengeSeed,  // la graine rendue n'est pas celle de la semaine
  kUnknown,        // un refus sans raison connue
};
[[nodiscard]] const char* SurvivorsTicketRefusalText(SurvivorsTicketRefusal refusal);

// Où en est le classement d'une partie classée finie.
enum class SurvivorsRankStatus : std::uint8_t {
  kSending,       // l'envoi est parti, le verdict n'est pas venu
  kPending,       // gardé au carnet, renvoyé à la prochaine connexion du personnage
  kAccepted,      // classée, à `rank`
  kPersonalBest,  // classée, et meilleur score du joueur sur ce tableau
  kLost,          // déjà comptée ou oubliée par le serveur
  kImplausible,   // refusée : partie invraisemblable
  kRefused,       // refusée sans raison connue
};
struct SurvivorsRankStanding {
  SurvivorsRankStatus status = SurvivorsRankStatus::kSending;
  int rank = 0;                  // 0 : hors tableau
  std::uint64_t best_score = 0;  // le meilleur score du joueur sur ce tableau
};

// « 14e », traduit.
inline constexpr std::size_t kSurvivorsRankTextSize = 16;
[[nodiscard]] std::array<char, kSurvivorsRankTextSize> SurvivorsRankText(int rank);

// La ligne de l'écran de fin : « Classement : 14e — meilleur personnel ! »,
// « Classement : 31e (votre meilleur : 48 210) », l'état d'un envoi ou la
// raison d'un refus.
inline constexpr std::size_t kSurvivorsStandingTextSize = 160;
[[nodiscard]] std::array<char, kSurvivorsStandingTextSize> SurvivorsStandingText(
    const SurvivorsRankStanding& standing);

}  // namespace ro::ui
