#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "ro/net/connection.h"
#include "ro/net/rank_notices.h"
#include "ro/ui/ranking_window.h"

#include "world/rank_notices.h"

namespace moonclient {

// ── LE TABLEAU DES CLASSEMENTS ──────────────────────────────────────────────
//
// La fenêtre montre ce que `RankNotices` tient déjà : le dernier classement
// reçu, ses noms, et s'il les attend encore. Rien n'est gardé ici que la vue de
// l'image et le dernier classement vu.
//
// 🔴 UN CLASSEMENT REÇU OUVRE LA FENÊTRE sur son onglet, qu'il vienne d'une
// commande ou d'un onglet. Un onglet changé demande son classement, dont la
// réponse ne s'écrit pas dans la conversation : le tableau la montre.

// LES PIÈCES DU TABLEAU dans les fenêtres de l'interface.
struct RankingBoardWindows {
  ro::ui::RankingWindowState window;
  // La vue de l'image ; nulle tant qu'aucun classement n'est arrivé.
  const ro::ui::RankingView* view = nullptr;
  // Ce que la fenêtre a demandé, lu à l'avancée suivante.
  ro::ui::RankingWindowAction answer;
};

// L'onglet d'un classement, et le classement d'un onglet.
[[nodiscard]] ro::ui::RankingTab RankingTabOf(ro::net::RankKind kind);
[[nodiscard]] ro::net::RankKind RankKindOf(ro::ui::RankingTab tab);

class RankingBoard {
 public:
  // À CHAQUE IMAGE : la demande de l'onglet changé, l'ouverture sur un
  // classement neuf, la vue. ⚠ ÉMET : un onglet changé demande son classement.
  void Advance(RankingBoardWindows& windows, RankNotices& notices, ro::net::Connection& link,
               std::uint32_t now_ms);

 private:
  std::uint32_t seen_serial_ = 0;
  // LA COMPOSITION : ce que la vue pointe.
  ro::ui::RankingView view_;
  std::array<std::string, ro::net::kRankingSize> names_;
  std::array<ro::ui::RankingRowView, ro::net::kRankingSize> rows_;
};

}  // namespace moonclient
