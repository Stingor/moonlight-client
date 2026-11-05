#include "game/survivors_presence_mode.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "ro/net/ui_caps.h"
#include "ro/proto/packet_lengths.h"
#include "ro/sim/survivors_play.h"

#include "game/survivors_presence_slot.h"

namespace moonclient {
namespace {

namespace rn = ro::net;

constexpr std::int64_t kMsPerSecond = 1000;
// Le serveur fait expirer une présence silencieuse : on la redit à ce rythme.
constexpr std::int64_t kSurvivorsPresenceRefreshMs = 5 * 60 * kMsPerSecond;

}  // namespace

rn::SurvivorsPresenceAsk PresenceOfStage(const SurvivorsStage& stage) {
  rn::SurvivorsPresenceAsk ask;
  switch (stage.screen()) {
    case SurvivorsScreen::kArriving:
    case SurvivorsScreen::kPlaying:
    case SurvivorsScreen::kChoosing:
    case SurvivorsScreen::kChest:
      ask.state = rn::SurvivorsPresenceState::kPlaying;
      break;
    case SurvivorsScreen::kPaused:
      ask.state = rn::SurvivorsPresenceState::kPaused;
      break;
    // LA FIN TIENT ENCORE L'ÉCRAN : le joueur n'est plus en partie pour les autres, mais
    // il ne verrait pas ce qu'on lui demande.
    case SurvivorsScreen::kDying:
    case SurvivorsScreen::kWon:
    case SurvivorsScreen::kSummary:
      ask.state = rn::SurvivorsPresenceState::kPaused;
      ask.flags = rn::kSurvivorsPresenceHidden;
      break;
    case SurvivorsScreen::kClosed:
    case SurvivorsScreen::kPreparing:
    case SurvivorsScreen::kLoading:
      return ask;
  }
  const ro::sim::SurvivorsPlay* play = stage.play();
  if (play == nullptr) return rn::SurvivorsPresenceAsk{};
  const ro::sim::SurvivorsStart& start = play->run().field().start;
  ask.arena_id = start.arena_id;
  ask.class_id = start.class_id;
  switch (stage.board().kind) {
    case SurvivorsBoardKind::kRanked:
      ask.flags |= rn::kSurvivorsPresenceRanked;
      break;
    case SurvivorsBoardKind::kChallenge:
      ask.flags |= rn::kSurvivorsPresenceChallenge;
      break;
    case SurvivorsBoardKind::kFree:
      break;
  }
  return ask;
}

SurvivorsPresenceMode::SurvivorsPresenceMode(const PacketLengthOf& length_of) {
  const PacketLengthOf lengths = length_of ? length_of : PacketLengthOf(ro::proto::PacketLength);
  caps_ = lengths(rn::kSurvivorsPresenceOpcode).has_value() ? rn::kUiCapSurvivorsPresence : 0U;
}

rn::FrameOutcome SurvivorsPresenceMode::ApplyFrame(const rn::Frame& frame,
                                                   std::int64_t received_at_ms) {
  if (frame.opcode != rn::kSurvivorsPresenceOpcode) return rn::FrameOutcome::kUnhandled;
  const std::optional<rn::SurvivorsPresence> presence = rn::DecodeSurvivorsPresence(frame.bytes);
  if (!presence.has_value()) return rn::FrameOutcome::kUndecodable;
  // L'ANNONCE DU SERVICE, redite à chaque annonce des capacités : le serveur
  // nous tient alors pour dehors, et l'état courant repart.
  if (presence->account_id == 0) {
    announced_ = true;
    sent_ = rn::SurvivorsPresenceAsk{};
    sent_at_ms_.reset();
    return rn::FrameOutcome::kApplied;
  }
  const auto at =
      std::ranges::lower_bound(seen_, presence->account_id, {}, &SurvivorsPresenceSeen::account_id);
  const bool known = at != seen_.end() && at->account_id == presence->account_id;
  if (presence->state == rn::SurvivorsPresenceState::kOut) {
    if (known) seen_.erase(at);
    return rn::FrameOutcome::kApplied;
  }
  const SurvivorsPresenceSeen seen{
      .account_id = presence->account_id,
      .char_id = presence->char_id,
      .state = presence->state,
      .arena_id = presence->arena_id,
      .class_id = presence->class_id,
      .entered_at_ms = received_at_ms - std::int64_t{presence->elapsed_s} * kMsPerSecond};
  if (known) {
    *at = seen;
  } else {
    seen_.insert(at, seen);
  }
  return rn::FrameOutcome::kApplied;
}

std::optional<std::vector<std::uint8_t>> SurvivorsPresenceMode::Due(
    const SurvivorsPresenceFacts& facts) {
  if (!facts.link_open || !facts.verified) {
    Forget();
    return std::nullopt;
  }
  if (!announced_) return std::nullopt;
  rn::SurvivorsPresenceAsk wanted = facts.own;
  const bool in_arena = wanted.state != rn::SurvivorsPresenceState::kOut;
  if (in_arena && !facts.shown) wanted.flags |= rn::kSurvivorsPresenceHidden;
  const bool refresh_due = in_arena && sent_at_ms_.has_value() &&
                           facts.now_ms >= *sent_at_ms_ + kSurvivorsPresenceRefreshMs;
  if (wanted == sent_ && !refresh_due) return std::nullopt;
  if (sent_at_ms_.has_value() && facts.now_ms < *sent_at_ms_ + kSurvivorsPresenceSendEveryMs) {
    return std::nullopt;
  }
  sent_ = wanted;
  sent_at_ms_ = facts.now_ms;
  return rn::EncodeSurvivorsPresenceAsk(wanted);
}

void SurvivorsPresenceMode::Pump(rn::Connection& link, const SurvivorsPresenceFacts& facts) {
  if (const std::optional<std::vector<std::uint8_t>> bytes = Due(facts)) link.Send(*bytes);
}

const SurvivorsPresenceSeen* SurvivorsPresenceMode::Of(std::uint32_t account_id) const noexcept {
  const auto at =
      std::ranges::lower_bound(seen_, account_id, {}, &SurvivorsPresenceSeen::account_id);
  return at != seen_.end() && at->account_id == account_id ? &*at : nullptr;
}

SurvivorsPresenceSlot::SurvivorsPresenceSlot() : presence_(std::make_unique<SurvivorsPresence>()) {}

SurvivorsPresenceSlot::~SurvivorsPresenceSlot() = default;

void SurvivorsPresenceMode::Forget() {
  announced_ = false;
  sent_ = rn::SurvivorsPresenceAsk{};
  sent_at_ms_.reset();
  seen_.clear();
}

}  // namespace moonclient
