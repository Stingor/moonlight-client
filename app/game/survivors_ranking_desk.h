#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ro/sim/survivors_rules.h"
#include "ro/ui/survivors_prepare_window.h"
#include "ro/ui/survivors_ranking_window.h"
#include "ro/ui/survivors_summary_window.h"

#include "game/survivors_meta_desk.h"
#include "game/survivors_ranking_mode.h"
#include "survivors/survivors_ranking_session.h"
#include "survivors/survivors_stage.h"

namespace moonclient {

// ── LE CLASSEMENT À L'ÉCRAN : ce que la session dit, et ce que les fenêtres y font ──
//
// Les vues des écrans du classement se lisent ici dans la session et dans ses
// résultats en attente ; les intentions des fenêtres (ouvrir le tableau,
// actualiser, changer d'onglet) y repartent vers la session.
//
// 🔴 SERVICE FERMÉ, AUCUNE VUE NE PORTE LE CLASSEMENT : ni onglet, ni bouton,
// ni compteur, ni ligne « hors ligne », et la fenêtre du tableau se ferme.
// Seul l'écran de fin d'une partie déjà envoyée garde sa ligne « en attente »,
// ce que le serveur d'aujourd'hui ne peut jamais produire.

// LES LIGNES ET LES NOMS des vues, gardés d'une image à l'autre : les vues
// pointent dedans, jamais dans la session, dont une page reçue remplace
// l'autre.
struct SurvivorsRankingRowsHeld {
  std::vector<std::string> ranked_names;
  std::vector<ro::ui::SurvivorsRankingRow> ranked_rows;
  std::vector<std::string> challenge_names;
  std::vector<ro::ui::SurvivorsRankingRow> challenge_rows;
  std::vector<std::string> pending_names;
  std::vector<ro::ui::SurvivorsPendingRow> pending_rows;
};

// LE TABLEAU MONDIAL : les deux pages tenues, leur âge à l'instant `now`, la
// semaine du défi comptée à l'heure du serveur, et la cadence.
[[nodiscard]] ro::ui::SurvivorsRankingView RankingViewOf(const SurvivorsRankingSession& session,
                                                         const SurvivorsRankingNow& now,
                                                         SurvivorsRankingRowsHeld& held);

// Les résultats en attente du serveur connecté, tous personnages confondus.
[[nodiscard]] std::size_t PendingCountOf(const SurvivorsRankingSession& session);

// L'ACCUEIL : le meilleur rang du tableau classé et les résultats en attente ;
// rien service fermé.
[[nodiscard]] std::optional<ro::ui::SurvivorsPrepareRanking> PrepareRankingOf(
    const SurvivorsRankingSession& session, SurvivorsRankingRowsHeld& held);

// Le verdict d'une partie envoyée s'attend 5 s ; au-delà, l'écran de fin la
// dit en attente, renvoyée à la prochaine connexion.
inline constexpr std::int64_t kSurvivorsVerdictWaitS = 5;

// L'ÉCRAN DE FIN de la partie jouée au tableau `board` : classée quand elle
// court avec son jeton ou qu'elle est partie au serveur, « Partie libre » pour
// la seule partie libre ; rien service fermé, sauf pour une partie déjà
// envoyée, ni pour un défi joué en local. Le bouton du tableau ne paraît que
// service ouvert.
[[nodiscard]] std::optional<ro::ui::SurvivorsSummaryRanking> SummaryRankOf(
    const SurvivorsRankingSession& session, SurvivorsBoardKind board);

// La marque « Classée » du HUD : la partie court avec son jeton.
[[nodiscard]] bool HudRankedOf(const SurvivorsRankingSession& session,
                               SurvivorsBoardKind board) noexcept;

// LA LIGNE DE CONVERSATION d'une ligne de la session, ou rien quand elle reste
// au journal. `on_summary` : l'écran de fin de la partie envoyée est montré,
// et dit déjà son verdict (Q5).
//   - refus ou silence du serveur au départ (Q3) : la partie part libre, ou le
//     défi se joue en local ;
//   - verdict arrivé écran fermé (Q5) ;
//   - résultat perdu ou oublié (Q6), une fois.
// Le service absent, les règles non classées et le désaccord de score restent
// au journal.
[[nodiscard]] std::optional<std::string> RankingLineOf(const SurvivorsRankingNotice& notice,
                                                       const SurvivorsRankingSession& session,
                                                       bool on_summary);

// L'écran de fin d'une partie est montré, voile ou bandeau compris.
[[nodiscard]] bool SummaryShown(const SurvivorsStage& stage) noexcept;

// LES LIGNES DE LA SESSION, prises et dites dans la conversation.
void SayRankingNotices(SurvivorsRankingMode& ranking, const SurvivorsStage& stage,
                       const SurvivorsSay& say);

// LES INTENTIONS DU CLASSEMENT d'une image : ouvrir le tableau depuis
// l'accueil (sur l'onglet retenu) ou depuis l'écran de fin (sur l'onglet de la
// partie), actualiser, changer d'onglet. Chacune demande la page de l'onglet,
// une fois dans la cadence, et une demande que la cadence retient part dès
// qu'elle le permet, fenêtre ouverte sur son onglet. À appeler à chaque
// image ; service fermé, rien ne s'ouvre.
struct SurvivorsRankingAsks {
  bool open = false;
  std::optional<ro::ui::SurvivorsRankingTab> open_on;
  ro::ui::SurvivorsRankingAsk window;
};
void FollowRankingAsks(SurvivorsRankingMode& ranking, ro::ui::SurvivorsRankingState& state,
                       const SurvivorsRankingAsks& asks, const ro::sim::SurvivorsRules& rules);

// LA FENÊTRE DU TABLEAU, composée quand elle est ouverte ; fermée service fermé.
void ComposeRankingWindow(const SurvivorsRankingMode& ranking, ro::ui::SurvivorsRankingState& state,
                          ro::ui::SurvivorsRankingView& view, SurvivorsRankingRowsHeld& held);

}  // namespace moonclient
