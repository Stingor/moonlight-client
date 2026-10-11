#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/window_name.h"

namespace ro::ui {

// LE TABLEAU DES CLASSEMENTS — « Ranking Board » —, les dix premiers d'un
// classement : rang, nom, points, puis les points du joueur.
//
// 🔴 LA FENÊTRE NE DEMANDE RIEN ELLE-MÊME : un onglet changé par le joueur
// devient l'intention « demander ce classement », qu'`app` envoie au serveur.
// La réponse arrive quand elle arrive, et ses noms après elle : tant que la vue
// dit qu'elle les attend, ou qu'elle montre un autre classement que l'onglet
// choisi, la fenêtre dit qu'elle cherche au lieu de montrer des lignes.

inline constexpr const char* kRankingWindowId = "ranking_board";
inline constexpr const char* kRankingWindowTitle = core::TrText("Tableau des classements");

// LA TAILLE À LA PREMIÈRE OUVERTURE, en pixels d'interface : dix lignes, l'en-tête
// des colonnes, les onglets et les points du joueur.
inline constexpr float kRankingFirstWidthPixels = 300.0F;
inline constexpr float kRankingFirstHeightPixels = 340.0F;
// La colonne du rang, étroite : deux chiffres.
inline constexpr float kRankingRankColumnPixels = 44.0F;
// Celle des points.
inline constexpr float kRankingPointsColumnPixels = 70.0F;

// LES ONGLETS, dans l'ordre où la fenêtre les montre.
enum class RankingTab : std::uint8_t {
  kBlacksmith,
  kAlchemist,
  kTaekwon,
  kKiller,
};
inline constexpr std::size_t kRankingTabCount = 4;

// Le libellé d'un onglet, déjà traduit.
[[nodiscard]] const char* RankingTabLabel(RankingTab tab);

// UNE LIGNE : son nom, vide quand le serveur n'en a pas donné ou que la place
// est vide, et ses points. Le rang est sa place dans la vue.
struct RankingRowView {
  std::string_view name;
  std::uint32_t points = 0;
};

// CE QUE LA FENÊTRE MONTRE, composé par `app` à chaque image.
struct RankingView {
  // Le classement que portent les lignes.
  RankingTab tab = RankingTab::kBlacksmith;
  std::span<const RankingRowView> rows;
  std::uint32_t own_points = 0;
  // Des noms manquent encore, et le délai de leur attente n'est pas passé.
  bool waiting_names = false;
  // Le classement de cet onglet n'est encore jamais arrivé : `rows` est vide.
  bool awaiting_list = false;
};

// L'onglet qu'ImGui montre, avant toute image.
inline constexpr int kRankingNoTabShown = -1;

struct RankingWindowState {
  bool open = false;
  bool was_open = false;
  // L'ONGLET CHOISI : posé par `app` quand un classement arrive, par le joueur
  // quand il clique.
  RankingTab tab = RankingTab::kBlacksmith;
  // Celui qu'ImGui a montré à la dernière image : un écart avec `tab` impose
  // l'onglet choisi au lieu de le lire.
  int tab_shown = kRankingNoTabShown;
  WindowName name{kRankingWindowTitle, kRankingWindowId};
};

struct RankingWindowAction {
  // L'onglet que le joueur vient de choisir : le classement à demander.
  bool requested = false;
  RankingTab requested_tab = RankingTab::kBlacksmith;
};

[[nodiscard]] RankingWindowAction DrawRankingWindow(RankingWindowState& state,
                                                    const RankingView& view, const SkinArt& art);

}  // namespace ro::ui
