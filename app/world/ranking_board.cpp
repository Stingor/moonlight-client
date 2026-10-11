// Le tableau des classements. Voir l'en-tête.

#include "world/ranking_board.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace moonclient {

ro::ui::RankingTab RankingTabOf(ro::net::RankKind kind) {
  switch (kind) {
    case ro::net::RankKind::kBlacksmith:
      return ro::ui::RankingTab::kBlacksmith;
    case ro::net::RankKind::kAlchemist:
      return ro::ui::RankingTab::kAlchemist;
    case ro::net::RankKind::kTaekwon:
      return ro::ui::RankingTab::kTaekwon;
    case ro::net::RankKind::kKiller:
      return ro::ui::RankingTab::kKiller;
  }
  return ro::ui::RankingTab::kBlacksmith;
}

ro::net::RankKind RankKindOf(ro::ui::RankingTab tab) {
  switch (tab) {
    case ro::ui::RankingTab::kBlacksmith:
      return ro::net::RankKind::kBlacksmith;
    case ro::ui::RankingTab::kAlchemist:
      return ro::net::RankKind::kAlchemist;
    case ro::ui::RankingTab::kTaekwon:
      return ro::net::RankKind::kTaekwon;
    case ro::ui::RankingTab::kKiller:
      return ro::net::RankKind::kKiller;
  }
  return ro::net::RankKind::kBlacksmith;
}

void RankingBoard::Advance(RankingBoardWindows& windows, RankNotices& notices,
                           ro::net::Connection& link, std::uint32_t now_ms) {
  // L'ONGLET CHANGÉ À L'IMAGE PRÉCÉDENTE. Hors ligne, rien à demander : la
  // fenêtre reste sur « récupération ».
  if (std::exchange(windows.answer, {}).requested && link.is_open()) {
    const ro::net::RankKind kind = RankKindOf(windows.window.tab);
    notices.ExpectQuiet(kind, now_ms);
    ro::net::RequestRanking(link, kind);
  }
  const std::optional<ro::net::RankingList>& latest = notices.latest();
  if (!latest.has_value()) {
    windows.view = nullptr;
    return;
  }
  // UN CLASSEMENT NEUF ouvre la fenêtre sur son onglet.
  if (notices.list_serial() != seen_serial_) {
    seen_serial_ = notices.list_serial();
    windows.window.open = true;
    windows.window.tab = RankingTabOf(latest->kind);
  }
  windows.view = &view_;
  // Fermée, la fenêtre ne lit pas sa vue : rien à recopier.
  if (!windows.window.open) {
    return;
  }
  names_ = notices.NamesOf(*latest);
  for (std::size_t rank = 0; rank < ro::net::kRankingSize; ++rank) {
    rows_[rank] = ro::ui::RankingRowView{.name = names_[rank], .points = latest->points[rank]};
  }
  view_ = ro::ui::RankingView{.tab = RankingTabOf(latest->kind),
                              .rows = rows_,
                              .own_points = latest->own_points,
                              .waiting_names = notices.waiting_names()};
}

}  // namespace moonclient
