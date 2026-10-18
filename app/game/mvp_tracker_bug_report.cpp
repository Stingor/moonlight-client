#include "game/mvp_tracker_bug_report.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/text.h"

namespace moonclient {
namespace {

using ro::ui::MvpSlotView;
using ro::ui::MvpTrackerView;
using ro::ui::MvpTrackerWindowState;
// Les premières rangées dans l'ordre montré : assez pour reconnaître un tri,
// trop peu pour peser.
constexpr std::size_t kMostRowsReported = 4;
// Le plus long texte repris d'un créneau : un nom de MVP, un nom de carte.
constexpr std::size_t kMostTextBytes = 48;

// Un texte borné, coupé sur une frontière de caractère, prêt pour le JSON.
[[nodiscard]] std::string Bounded(std::string_view text) {
  return JsonEscaped(ro::core::WithoutDanglingUtf8(text.substr(0, kMostTextBytes)));
}

[[nodiscard]] const MvpSlotView* FindSlot(const MvpTrackerView& view, std::uint16_t slot_id) {
  if (slot_id == ro::ui::kMvpNoSlotView) {
    return nullptr;
  }
  const auto found = std::ranges::find_if(
      view.slots, [slot_id](const MvpSlotView& slot) { return slot.slot_id == slot_id; });
  return found != view.slots.end() ? &*found : nullptr;
}

// LE CRÉNEAU VISÉ, avec ce qui fait son minuteur : délais, fenêtre de retour,
// observation. `why` dit pourquoi c'est lui.
[[nodiscard]] std::string SlotFields(const MvpSlotView& slot, const char* why) {
  return std::format(
      R"("slot":{{"why":"{}","slot_id":{},"mob_id":{},"label":"{}","map":"{}","favorite":{},)"
      R"("delay_ms":[{},{}],"window":{{"known":{},"from_s":{},"to_s":{},"exact":{}}},)"
      R"("observed":{},"source":{},"kill_time_s":{},"exact_respawn_s":{},"tomb":[{},{}],)"
      R"("reported_at_s":{}}})",
      why, slot.slot_id, slot.mob_id, Bounded(slot.label), Bounded(slot.map),
      JsonFlag(slot.favorite), slot.delay1_ms, slot.delay2_ms, JsonFlag(slot.window_known),
      slot.window_from_s, slot.window_to_s, JsonFlag(slot.exact), JsonFlag(slot.observed),
      slot.source_raw, slot.kill_time_s, slot.exact_respawn_s, slot.tomb_x, slot.tomb_y,
      slot.reported_at_s);
}

// LE GROUPE, le dernier refus, l'invitation : ce que le panneau du haut disait.
[[nodiscard]] std::string GroupFields(const MvpTrackerView& view) {
  const auto online = std::ranges::count_if(
      view.members, [](const ro::ui::MvpMemberView& member) { return member.online; });
  return std::format(
      R"("group":{{"group_id":{},"members":{},"online":{},"owner_known":{},"i_own":{}}},)"
      R"("result":{{"code":{},"shown":{}}},)"
      R"("invite":{{"pending":{},"waiting":{},"refusal":{},"gone":{}}})",
      view.group_id, view.members.size(), online, JsonFlag(view.owner_known), JsonFlag(view.i_own),
      view.result_code, JsonFlag(view.result_shown), view.pending_invite,
      JsonFlag(view.invite_waiting), view.invite_refusal, JsonFlag(view.invite_gone));
}

// Les premières rangées dans l'ordre montré, par créneau.
[[nodiscard]] std::string FirstRows(const MvpTrackerWindowState& window,
                                    const MvpTrackerView& view) {
  std::string rows = "[";
  const std::size_t shown = std::min(window.order.size(), kMostRowsReported);
  for (std::size_t rank = 0; rank < shown; ++rank) {
    const std::size_t index = window.order[rank];
    if (index >= view.slots.size()) {
      break;
    }
    rows += std::format("{}{}", rank == 0 ? "" : ",", view.slots[index].slot_id);
  }
  rows += ']';
  return rows;
}

}  // namespace

BugReportContext MvpTrackerBugReport(const MvpTrackerWindowState& window,
                                     const MvpTrackerView& view) {
  // ⚠ LE FILTRE NE VAUT QUE MONTRÉ : masqué, la table l'ignore.
  const std::string_view filter =
      window.look.show_filter ? std::string_view(window.filter.data()) : std::string_view();
  const auto favorites =
      std::ranges::count_if(view.slots, [](const MvpSlotView& slot) { return slot.favorite; });
  std::string fields = std::format(
      R"("mvp_tracker":{{"enabled":{},"link_ready":{},"catalog_known":{},"collapsed":{},)"
      R"("server_now_s":{},"slots":{},"rows":{},"favorites":{},"sort":"{}","filter":"{}",)"
      R"("delay_column":{},"lines":{},"alerts":{},"first_rows":{},)",
      JsonFlag(view.enabled), JsonFlag(view.link_ready), JsonFlag(view.catalog_known),
      JsonFlag(window.was_collapsed), view.server_now_s, view.slots.size(), window.order.size(),
      favorites, ro::ui::MvpSortWord(window.sort_key), Bounded(filter),
      JsonFlag(window.look.show_delay), view.lines.size(), view.alerts.size(),
      FirstRows(window, view));
  fields += GroupFields(view);

  // Le créneau sur lequel le carnet vient de s'ouvrir, sinon le dernier dont le
  // nom a ouvert le menu.
  const MvpSlotView* slot = FindSlot(view, window.focus_slot);
  const char* why = "focus";
  if (slot == nullptr) {
    slot = FindSlot(view, window.menu_slot);
    why = "last_menu";
  }
  if (slot != nullptr) {
    fields += ',';
    fields += SlotFields(*slot, why);
  }
  fields += '}';

  // Le libellé : le créneau visé, sinon le filtre, sinon le compte des rangées.
  std::array<char, kBugReportLabelBytes> label{};
  if (slot != nullptr) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Carnet MVP : %s sur %s (créneau %u)"),
                  slot->label.empty() ? "?" : std::string(slot->label).c_str(),
                  std::string(ro::ui::MvpMapShown(*slot)).c_str(),
                  static_cast<unsigned>(slot->slot_id));
  } else if (!filter.empty()) {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Carnet MVP : filtre « %s »"),
                  std::string(filter).c_str());
  } else {
    std::snprintf(label.data(), label.size(), ro::core::Tr("Carnet MVP : %u créneaux"),
                  static_cast<unsigned>(view.slots.size()));
  }
  return FieldsBugReport(std::string(ro::core::WithoutDanglingUtf8(label.data())), fields);
}

}  // namespace moonclient
