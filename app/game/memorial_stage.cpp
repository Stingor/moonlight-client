// L'étage de l'instance. Voir l'en-tête.

#include "game/memorial_stage.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/memorial_dungeon.h"

#include "chat/chat_mvp_link.h"

namespace moonclient {
namespace {

using ro::sim::MemorialEnding;
using ro::sim::MemorialOwnerKind;
using ro::sim::MemorialPhase;
using ro::ui::MemorialDestroyGate;
using ro::ui::MemorialOwnerView;

[[nodiscard]] MemorialOwnerView OwnerView(MemorialOwnerKind kind) {
  switch (kind) {
    case MemorialOwnerKind::kCharacter:
      return MemorialOwnerView::kCharacter;
    case MemorialOwnerKind::kParty:
      return MemorialOwnerView::kParty;
    case MemorialOwnerKind::kGuild:
      return MemorialOwnerView::kGuild;
    case MemorialOwnerKind::kClan:
      return MemorialOwnerView::kClan;
    case MemorialOwnerKind::kUnknown:
      break;
  }
  return MemorialOwnerView::kUnknown;
}

[[nodiscard]] const char* EndingText(MemorialEnding ending) {
  switch (ending) {
    case MemorialEnding::kReservationCancelled:
      return ro::core::Tr("La réservation de l'instance a été annulée.");
    case MemorialEnding::kExpired:
      return ro::core::Tr("La durée de l'instance est écoulée : elle a été détruite.");
    case MemorialEnding::kEntryExpired:
      return ro::core::Tr("Personne n'est entré dans l'instance à temps : elle a été détruite.");
    case MemorialEnding::kDestroyed:
      return ro::core::Tr("L'instance a été détruite.");
  }
  return ro::core::Tr("L'instance a été détruite.");
}

}  // namespace

MemorialDestroyGate MemorialDestroyGateOf(const ro::sim::MemorialDungeon& dungeon,
                                          std::int64_t now_unix_s) {
  if (dungeon.phase() == MemorialPhase::kNone) {
    return MemorialDestroyGate::kHidden;
  }
  const std::optional<ro::sim::MemorialDetails>& details = dungeon.details();
  // SANS DÉTAIL, le bouton reste là, comme dans le jeu : le serveur jugera.
  if (!details.has_value()) {
    return dungeon.DestroyPending(now_unix_s) ? MemorialDestroyGate::kPending
                                              : MemorialDestroyGate::kUnknownRights;
  }
  if (!details->destroyable) {
    return MemorialDestroyGate::kHidden;
  }
  if (dungeon.DestroyPending(now_unix_s)) {
    return MemorialDestroyGate::kPending;
  }
  if (!details->may_destroy) {
    return MemorialDestroyGate::kNotOwner;
  }
  if (details->on_instance_map) {
    return MemorialDestroyGate::kOnInstanceMap;
  }
  return MemorialDestroyGate::kAllowed;
}

void MemorialStage::StartSession(MemorialWindows& windows) {
  dungeon_.Reset();
  (void)dungeon_.TakeEndings();
  seen_arrival_ = dungeon_.arrival_serial();
  seen_phase_ = MemorialPhase::kNone;
  toggle_asked_ = false;
  windows.window.open = false;
  windows.answer = {};
}

ro::net::FrameOutcome MemorialStage::ApplyFrame(const ro::net::Frame& frame,
                                                std::int64_t now_unix_s) {
  return ro::net::ApplyMemorialDungeonFrame(frame, dungeon_, now_unix_s);
}

void MemorialStage::Advance(MemorialWindows& windows, ro::net::Connection& link, bool link_ready,
                            std::int64_t now_unix_s) {
  // 🔴 LA PORTE SE REJUGE AU « OUI » : le serveur a pu répondre pendant que la
  // boîte attendait.
  const MemorialDestroyGate gate = MemorialDestroyGateOf(dungeon_, now_unix_s);
  if (std::exchange(windows.answer, {}).destroy && link_ready &&
      (gate == MemorialDestroyGate::kAllowed || gate == MemorialDestroyGate::kUnknownRights)) {
    ro::net::RequestMemorialDungeonDestroy(link);
    dungeon_.NoteDestroyAsked(now_unix_s);
    // LE SERVEUR SE TAIT S'IL REFUSE : le journal garde de quoi le constater.
    ro::core::LogInfo("instance « {} » : destruction demandée au serveur", dungeon_.name());
  }
  if (std::exchange(toggle_asked_, false)) {
    windows.window.open = !windows.window.open;
  }
  // UNE INSTANCE QUI DISPARAÎT REFERME SA FENÊTRE, comme dans le jeu.
  if (seen_phase_ != MemorialPhase::kNone && dungeon_.phase() == MemorialPhase::kNone) {
    windows.window.open = false;
  }
  seen_phase_ = dungeon_.phase();
  if (dungeon_.arrival_serial() != seen_arrival_) {
    seen_arrival_ = dungeon_.arrival_serial();
    windows.window.open = dungeon_.phase() != MemorialPhase::kNone;
  }
  Compose(windows.view, now_unix_s);
}

void MemorialStage::Compose(ro::ui::MemorialDungeonView& view, std::int64_t now_unix_s) {
  view = {};
  if (dungeon_.phase() == MemorialPhase::kNone) {
    return;
  }
  view.present = true;
  view.waiting = dungeon_.phase() == MemorialPhase::kWaiting;
  view.name = dungeon_.name();
  view.wait_position = dungeon_.wait_position();
  view.seconds_left = dungeon_.SecondsLeft(now_unix_s);
  view.idle_seconds_left = dungeon_.IdleSecondsLeft(now_unix_s);
  const std::optional<std::int64_t> local_end = dungeon_.LocalEnd();
  end_clock_ = local_end.has_value() ? MvpClockText(*local_end) : std::string();
  view.end_clock = end_clock_;
  if (const std::optional<ro::sim::MemorialDetails>& details = dungeon_.details();
      details.has_value()) {
    view.detailed = true;
    view.hard_mode = details->hard_mode;
    view.turbo = details->turbo;
    view.owner = OwnerView(details->owner_kind);
    view.owner_name = details->owner_name;
    view.players_inside = details->players_inside;
    view.duration_s = details->duration_limit_s;
    view.idle_timeout_s = details->idle_timeout_s;
  }
  view.destroy = MemorialDestroyGateOf(dungeon_, now_unix_s);
}

std::vector<std::string> MemorialStage::TakeNotices() {
  std::vector<std::string> notices;
  for (const MemorialEnding ending : dungeon_.TakeEndings()) {
    ro::core::LogInfo("instance : fin annoncée par le serveur ({})", static_cast<int>(ending));
    notices.emplace_back(EndingText(ending));
  }
  return notices;
}

}  // namespace moonclient
