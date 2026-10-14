#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "ro/net/connection.h"
#include "ro/net/rank_notices.h"
#include "ro/ui/ranking_window.h"

#include "world/rank_notices.h"

namespace moonclient {

// ── LE TABLEAU DES CLASSEMENTS ──────────────────────────────────────────────
//
// La fenêtre montre ce que `RankNotices` reçoit : le dernier classement de
// chaque onglet, ses noms, et s'il les attend encore. Rien n'est gardé ici que
// ces classements, la vue de l'image et le dernier classement vu.
//
// 🔴 UN CLASSEMENT REÇU N'OUVRE PAS LA FENÊTRE. `/blacksmith`, `/alchemist` et
// `/taekwon` écrivent dans la conversation, comme le client d'origine ; le
// tableau, que celui-ci n'a pas, s'ouvre par `/ranking` ou par son icône, sur
// le dernier onglet vu. Un onglet dont le classement n'est jamais arrivé le
// demande, et la réponse ne s'écrit pas dans la conversation : le tableau la
// montre.

// LES PIÈCES DU TABLEAU dans les fenêtres de l'interface.
struct RankingBoardWindows {
  ro::ui::RankingWindowState window;
  // La vue de l'image ; nulle avant la première avancée.
  const ro::ui::RankingView* view = nullptr;
  // Ce que la fenêtre a demandé, lu à l'avancée suivante.
  ro::ui::RankingWindowAction answer;
  // UNE OUVERTURE DEMANDÉE — `/ranking`, l'icône —, lue à l'avancée suivante.
  bool open_asked = false;
  // LA QUESTION DE DÉSIGNATION de Feeling, qui vit avec les avis d'étoile.
  StarPlaceBox star_place;
};

// L'onglet d'un classement, rien pour un genre que le tableau ne montre pas ;
// et le classement d'un onglet.
[[nodiscard]] std::optional<ro::ui::RankingTab> RankingTabOf(ro::net::RankKind kind);
[[nodiscard]] ro::net::RankKind RankKindOf(ro::ui::RankingTab tab);

// L'ICÔNE DU MENU : ferme la fenêtre ouverte, en demande l'ouverture sinon.
void ToggleRankingBoard(RankingBoardWindows& windows);

class RankingBoard {
 public:
  // À CHAQUE IMAGE : la demande de l'onglet changé, les classements reçus,
  // l'ouverture demandée, la vue. ⚠ ÉMET : un onglet changé, ou ouvert sans
  // classement, demande le sien.
  void Advance(RankingBoardWindows& windows, RankNotices& notices, ro::net::Connection& link,
               std::uint32_t now_ms);

 private:
  std::uint32_t seen_serial_ = 0;
  // Le dernier classement reçu de chaque onglet.
  std::array<std::optional<ro::net::RankingList>, ro::ui::kRankingTabCount> lists_;
  // LA COMPOSITION : ce que la vue pointe.
  ro::ui::RankingView view_;
  std::array<std::string, ro::net::kRankingSize> names_;
  std::array<ro::ui::RankingRowView, ro::net::kRankingSize> rows_;
};

}  // namespace moonclient
