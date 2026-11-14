// La fenêtre de réputation. Voir l'en-tête.

#include "world/reputation_board.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/core/ascii.h"

namespace moonclient {
namespace {

// Le rang d'un groupe dans la liste de la fenêtre, « toutes » pour un groupe que
// le livre ne connaît pas ou pour le groupe zéro.
[[nodiscard]] int GroupRankOf(const ReputationBook& book, std::uint64_t group_id) {
  if (group_id == 0) {
    return ro::ui::kReputationAllGroups;
  }
  const std::vector<ReputationGroup>& groups = book.groups();
  const auto found = std::ranges::find(groups, group_id, &ReputationGroup::group_id);
  return found == groups.end() ? ro::ui::kReputationAllGroups
                               : static_cast<int>(found - groups.begin()) + 1;
}

[[nodiscard]] std::int64_t PointsOf(const ro::sim::CharacterLedger& ledger,
                                    std::uint64_t faction_id) {
  const auto found = ledger.reputation().find(faction_id);
  return found == ledger.reputation().end() ? 0 : found->second;
}

[[nodiscard]] bool Shown(const ReputationFaction& faction, std::int64_t points) {
  switch (faction.shown) {
    case ReputationShown::kAlways:
      return true;
    case ReputationShown::kNever:
      return false;
    case ReputationShown::kWithPoints:
      return points != 0;
  }
  return true;
}

}  // namespace

void ToggleReputationWindow(ReputationWindows& windows) {
  if (windows.window.open) {
    windows.window.open = false;
    return;
  }
  windows.open_asked = true;
}

void DrawReputationWindows(ReputationWindows& windows, const ro::ui::SkinArt& art,
                           ro::ui::EscapeStack& escape) {
  if (windows.view == nullptr) {
    return;
  }
  ro::ui::DrawReputationWindow(windows.window, *windows.view, art);
  // Échap baisse l'interrupteur : la fenêtre se ferme, sans un octet.
  escape.Register(ro::ui::kReputationWindowId, &windows.window.open);
}

void ReputationBoard::Advance(ReputationWindows& windows, ro::sim::CharacterLedger& ledger,
                              const ReputationBook& book) {
  ro::ui::ReputationWindowState& window = windows.window;
  // LE SCRIPT choisit le groupe et vide la recherche : la faction désignée doit
  // être dans la liste.
  if (const std::optional<ro::sim::ReputationOpening> opening = ledger.TakeReputationOpening()) {
    window.open = true;
    window.group = GroupRankOf(book, opening->group_id);
    window.search.fill('\0');
    highlighted_faction_id_ = opening->faction_id;
    highlighted_group_ = window.group;
  }
  if (std::exchange(windows.open_asked, false)) {
    window.open = true;
  }
  windows.view = &view_;
  // FERMÉE OU SOUS UN AUTRE GROUPE, la faction désignée cesse de l'être.
  if (!window.open || window.group != highlighted_group_) {
    highlighted_faction_id_ = 0;
  }
  // Fermée, la fenêtre ne lit pas sa vue : rien à recomposer.
  if (window.open) {
    Compose(windows, ledger, book);
  }
}

void ReputationBoard::Compose(const ReputationWindows& windows,
                              const ro::sim::CharacterLedger& ledger, const ReputationBook& book) {
  const ro::ui::ReputationWindowState& window = windows.window;
  group_names_.clear();
  for (const ReputationGroup& group : book.groups()) {
    group_names_.push_back(group.name);
  }
  // LES FACTIONS DU GROUPE CHOISI, dans son ordre ; toutes celles du livre sinon.
  std::vector<std::uint64_t> wanted;
  const int group_count = static_cast<int>(book.groups().size());
  if (window.group > ro::ui::kReputationAllGroups && window.group <= group_count) {
    wanted = book.groups()[static_cast<std::size_t>(window.group - 1)].faction_ids;
  } else {
    for (const ReputationFaction& faction : book.factions()) {
      wanted.push_back(faction.faction_id);
    }
  }
  const std::string_view searched(window.search.data());
  rows_.clear();
  std::int64_t total = 0;
  for (const std::uint64_t faction_id : wanted) {
    const ReputationFaction* faction = book.FindFaction(faction_id);
    if (faction == nullptr) {
      continue;
    }
    const std::int64_t points = PointsOf(ledger, faction_id);
    if (!Shown(*faction, points) || !ro::core::AsciiContainsFolded(faction->name, searched)) {
      continue;
    }
    total += points;
    rows_.push_back(ro::ui::ReputationRowView{
        .name = faction->name,
        .points = points,
        .least = faction->least,
        .most = faction->most,
        .highlighted = highlighted_faction_id_ != 0 && faction_id == highlighted_faction_id_});
  }
  view_ = ro::ui::ReputationView{.groups = group_names_,
                                 .rows = rows_,
                                 .group_points = total,
                                 .awaiting_points = !ledger.reputation_received(),
                                 .book_missing = !book.loaded()};
}

}  // namespace moonclient
