#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/i18n.h"

#include "chat/chat_tab_view.h"

namespace moonclient {

bool ChatTabsView::IsMeter(std::uint32_t group, int local) const {
  const auto found = shown_.find(group);
  if (found == shown_.end() || local < 0 ||
      static_cast<std::size_t>(local) >= found->second.ranks.size()) {
    return false;
  }
  return tabs_.tabs()[found->second.ranks[static_cast<std::size_t>(local)]].content ==
         ChatTabContent::kDamageMeter;
}

int ChatTabsView::LocalRankOf(std::uint32_t group, std::size_t rank) const {
  const std::span<const ChatTab> tabs = tabs_.tabs();
  int local = 0;
  for (std::size_t before = 0; before < rank && before < tabs.size(); ++before) {
    const bool hidden = meter_hidden_ && tabs[before].content == ChatTabContent::kDamageMeter;
    if (tabs[before].group == group && !hidden) {
      ++local;
    }
  }
  return local;
}

std::optional<ChatTabsView::MeterPlace> ChatTabsView::MeterPlaceNow() const {
  const std::optional<std::size_t> rank = tabs_.MeterRank();
  if (!rank.has_value() || meter_hidden_) {
    return std::nullopt;
  }
  const std::uint32_t group = tabs_.tabs()[*rank].group;
  return MeterPlace{.group = group, .local = LocalRankOf(group, *rank)};
}

void ChatTabsView::KeepShownAfterMeterMove(std::optional<MeterPlace> before,
                                           ro::ui::ChatWindows& windows) const {
  const std::optional<MeterPlace> after = MeterPlaceNow();
  // ⚠ DANS LA MÊME BANDE, C'EST UN DÉPLACEMENT : montré, il reste montré.
  if (before.has_value() && after.has_value() && before->group == after->group) {
    ro::ui::ChatWindowState& state = windows.For(before->group);
    const int ahead_of = after->local > before->local ? after->local + 1 : after->local;
    state.tab = ShownAfterMove(state.tab, before->local, ahead_of);
    return;
  }
  if (before.has_value()) {
    ro::ui::ChatWindowState& state = windows.For(before->group);
    state.tab = ShownAfterLeaving(state.tab, before->local);
  }
  if (after.has_value()) {
    ro::ui::ChatWindowState& state = windows.For(after->group);
    state.tab = ShownAfterArrival(state.tab, after->local);
  }
}

void ChatTabsView::HideMeter(bool hidden, ro::ui::ChatWindows& windows) {
  const bool first = !std::exchange(meter_hidden_told_, true);
  if (first || hidden == meter_hidden_) {
    meter_hidden_ = hidden;
    return;
  }
  const std::optional<MeterPlace> before = MeterPlaceNow();
  meter_hidden_ = hidden;
  KeepShownAfterMeterMove(before, windows);
}

void ChatTabsView::DockMeter(std::uint32_t group, int local, ro::ui::ChatWindows& windows) {
  // ⚠ LE RANG SE TRADUIT DANS LA BANDE TELLE QUE LE JOUEUR LA VOYAIT : celui de
  // la languette qui occupait la place visée, ou juste après la dernière.
  std::size_t before = tabs_.count();
  if (const auto found = shown_.find(group); found != shown_.end()) {
    const std::vector<std::size_t>& ranks = found->second.ranks;
    if (local >= 0 && static_cast<std::size_t>(local) < ranks.size()) {
      before = ranks[static_cast<std::size_t>(local)];
    } else if (!ranks.empty()) {
      before = ranks.back() + 1;
    }
  }
  const std::optional<MeterPlace> was = MeterPlaceNow();
  tabs_.DockMeter(std::string(ro::core::Tr("DPS")), group, before);
  KeepShownAfterMeterMove(was, windows);
  SaveIfNeeded();
}

void ChatTabsView::UndockMeter(ro::ui::ChatWindows& windows) {
  const std::optional<MeterPlace> was = MeterPlaceNow();
  tabs_.UndockMeter();
  KeepShownAfterMeterMove(was, windows);
  SaveIfNeeded();
}

void ChatTabsView::FollowMeterDock(bool& wanted, ro::ui::ChatWindows& windows) {
  const std::optional<MeterPlace> was = MeterPlaceNow();
  wanted = tabs_.FollowMeterDock(wanted, meter_dock_seen_, std::string(ro::core::Tr("DPS")));
  meter_dock_seen_ = wanted;
  KeepShownAfterMeterMove(was, windows);
  SaveIfNeeded();
}

}  // namespace moonclient
