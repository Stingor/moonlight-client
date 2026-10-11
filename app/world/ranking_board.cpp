// Le tableau des classements. Voir l'en-tête.

#include "world/ranking_board.h"

#include <cstddef>
#include <optional>
#include <utility>

namespace moonclient {
namespace {

// Le classement d'un onglet, demandé sans que la réponse s'écrive dans la
// conversation. Hors ligne, rien à demander : la fenêtre reste sur
// « récupération ».
void AskQuietly(ro::ui::RankingTab tab, RankNotices& notices, ro::net::Connection& link,
                std::uint32_t now_ms) {
  if (!link.is_open()) {
    return;
  }
  const ro::net::RankKind kind = RankKindOf(tab);
  notices.ExpectQuiet(kind, now_ms);
  ro::net::RequestRanking(link, kind);
}

}  // namespace

std::optional<ro::ui::RankingTab> RankingTabOf(ro::net::RankKind kind) {
  switch (kind) {
    case ro::net::RankKind::kBlacksmith:
      return ro::ui::RankingTab::kBlacksmith;
    case ro::net::RankKind::kAlchemist:
      return ro::ui::RankingTab::kAlchemist;
    case ro::net::RankKind::kTaekwon:
      return ro::ui::RankingTab::kTaekwon;
    case ro::net::RankKind::kKiller:
      // Sans onglet : la liste reçue (par « /pk ») ne s'écrit que dans la
      // discussion.
      return std::nullopt;
  }
  return std::nullopt;
}

ro::net::RankKind RankKindOf(ro::ui::RankingTab tab) {
  switch (tab) {
    case ro::ui::RankingTab::kBlacksmith:
      return ro::net::RankKind::kBlacksmith;
    case ro::ui::RankingTab::kAlchemist:
      return ro::net::RankKind::kAlchemist;
    case ro::ui::RankingTab::kTaekwon:
      return ro::net::RankKind::kTaekwon;
  }
  return ro::net::RankKind::kBlacksmith;
}

void ToggleRankingBoard(RankingBoardWindows& windows) {
  if (windows.window.open) {
    windows.window.open = false;
    return;
  }
  windows.open_asked = true;
}

void RankingBoard::Advance(RankingBoardWindows& windows, RankNotices& notices,
                           ro::net::Connection& link, std::uint32_t now_ms) {
  // L'ONGLET CHANGÉ À L'IMAGE PRÉCÉDENTE redemande toujours le sien : un
  // classement change pendant qu'on le regarde.
  if (std::exchange(windows.answer, {}).requested) {
    AskQuietly(windows.window.tab, notices, link, now_ms);
  }
  // LES CLASSEMENTS REÇUS se rangent sous leur onglet, sans rien ouvrir.
  const std::optional<ro::net::RankingList>& latest = notices.latest();
  if (notices.list_serial() != seen_serial_ && latest.has_value()) {
    seen_serial_ = notices.list_serial();
    if (const std::optional<ro::ui::RankingTab> tab = RankingTabOf(latest->kind)) {
      lists_[static_cast<std::size_t>(*tab)] = *latest;
    }
  }
  // L'OUVERTURE DEMANDÉE, sur le dernier onglet vu ; son classement se demande
  // s'il n'est jamais arrivé.
  const std::size_t shown = static_cast<std::size_t>(windows.window.tab);
  if (std::exchange(windows.open_asked, false)) {
    windows.window.open = true;
    if (!lists_[shown].has_value()) {
      AskQuietly(windows.window.tab, notices, link, now_ms);
    }
  }
  windows.view = &view_;
  // Fermée, la fenêtre ne lit pas sa vue : rien à recopier.
  if (!windows.window.open) {
    return;
  }
  const std::optional<ro::net::RankingList>& list = lists_[shown];
  if (!list.has_value()) {
    view_ = ro::ui::RankingView{.tab = windows.window.tab, .awaiting_list = true};
    return;
  }
  names_ = notices.NamesOf(*list);
  for (std::size_t rank = 0; rank < ro::net::kRankingSize; ++rank) {
    rows_[rank] = ro::ui::RankingRowView{.name = names_[rank], .points = list->points[rank]};
  }
  // LES NOMS S'ATTENDENT pour le dernier classement reçu seulement : celui
  // d'un autre onglet a déjà fini d'attendre.
  const bool waiting = notices.waiting_names() && latest.has_value() && latest->kind == list->kind;
  view_ = ro::ui::RankingView{.tab = windows.window.tab,
                              .rows = rows_,
                              .own_points = list->own_points,
                              .waiting_names = waiting};
}

}  // namespace moonclient
