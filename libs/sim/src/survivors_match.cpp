#include "ro/sim/survivors_match.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "ro/sim/survivors_journal.h"

namespace ro::sim {

namespace {

constexpr std::size_t kFrameHeaderBytes = 5;
constexpr std::size_t kInputBytes = 3;
constexpr std::size_t kDigestBytes = 12;
constexpr std::size_t kU32Bytes = 4;
constexpr std::size_t kU64Bytes = 8;
constexpr std::size_t kByteBits = 8;
// Les empreintes gardées pour comparaison, en nombre.
constexpr std::size_t kMostKeptDigests = 16;

void PutU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  for (std::size_t index = 0; index < kU32Bytes; ++index) {
    out.push_back(static_cast<std::uint8_t>((value >> (index * kByteBits)) & 0xFFU));
  }
}

void PutU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
  for (std::size_t index = 0; index < kU64Bytes; ++index) {
    out.push_back(static_cast<std::uint8_t>((value >> (index * kByteBits)) & 0xFFU));
  }
}

[[nodiscard]] std::uint32_t GetU32(std::span<const std::uint8_t> bytes) {
  std::uint32_t value = 0;
  for (std::size_t index = 0; index < kU32Bytes; ++index) {
    value |= std::uint32_t{bytes[index]} << (index * kByteBits);
  }
  return value;
}

[[nodiscard]] std::uint64_t GetU64(std::span<const std::uint8_t> bytes) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < kU64Bytes; ++index) {
    value |= std::uint64_t{bytes[index]} << (index * kByteBits);
  }
  return value;
}

template <typename Item>
void Extend(std::vector<Item>& into, std::vector<Item>&& more) {
  into.insert(into.end(), std::make_move_iterator(more.begin()),
              std::make_move_iterator(more.end()));
}

}  // namespace

void AppendSurvivorsNews(SurvivorsNews& into, SurvivorsNews&& more) {
  Extend(into.hits, std::move(more.hits));
  Extend(into.falls, std::move(more.falls));
  Extend(into.casts, std::move(more.casts));
  Extend(into.warnings, std::move(more.warnings));
  Extend(into.chests, std::move(more.chests));
  Extend(into.evolutions, std::move(more.evolutions));
  Extend(into.pickups_taken, std::move(more.pickups_taken));
  Extend(into.boss_arrivals, std::move(more.boss_arrivals));
  Extend(into.boss_moves, std::move(more.boss_moves));
  into.level_ups = static_cast<std::uint16_t>(into.level_ups + more.level_ups);
  for (std::size_t index = 0; index < into.reborn_by_hero.size(); ++index) {
    into.reborn_by_hero[index] = into.reborn_by_hero[index] || more.reborn_by_hero[index];
  }
  into.gems_gathered += more.gems_gathered;
  into.hero_hurt += more.hero_hurt;
  for (std::size_t index = 0; index < into.hurt_by_hero.size(); ++index) {
    into.hurt_by_hero[index] += more.hurt_by_hero[index];
  }
  into.dropped += more.dropped;
  Extend(into.cards_found, std::move(more.cards_found));
  Extend(into.revives, std::move(more.revives));
  Extend(into.heroes_down, std::move(more.heroes_down));
  Extend(into.cards_dropped, std::move(more.cards_dropped));
}

std::optional<LockstepFrame> DecodeLockstepFrame(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kFrameHeaderBytes) return std::nullopt;
  LockstepFrame frame;
  frame.through_tick = GetU32(bytes);
  const std::size_t count = bytes[kU32Bytes];
  const std::size_t inputs_end = kFrameHeaderBytes + count * kInputBytes;
  if (bytes.size() != inputs_end && bytes.size() != inputs_end + kDigestBytes) return std::nullopt;
  if (count > std::size_t{frame.through_tick} + 1U) return std::nullopt;
  frame.inputs.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const std::uint8_t heading = bytes[kFrameHeaderBytes + index * kInputBytes];
    const std::uint8_t ballot = bytes[kFrameHeaderBytes + index * kInputBytes + 1U];
    if (heading != kNoHeading && heading >= kHeadingSteps) return std::nullopt;
    SurvivorsTeamInput input{.heading = heading};
    if (ballot != kLockstepNoBallot) {
      input.ballot = UnpackAnswer(ballot);
      if (!input.ballot.has_value()) return std::nullopt;
    }
    input.ballot_vote = bytes[kFrameHeaderBytes + index * kInputBytes + 2U];
    frame.inputs.push_back(input);
  }
  if (bytes.size() == inputs_end + kDigestBytes) {
    frame.digest_tick = GetU32(bytes.subspan(inputs_end));
    frame.digest = GetU64(bytes.subspan(inputs_end + kU32Bytes));
  }
  return frame;
}

SurvivorsMatch::SurvivorsMatch(SurvivorsRun run, std::size_t local_player,
                               std::uint32_t input_delay_ticks)
    : play_(std::move(run)),
      local_player_(local_player),
      next_local_tick_(input_delay_ticks),
      framed_next_tick_(input_delay_ticks),
      input_delay_ticks_(input_delay_ticks) {
  // Les premiers tics, avant que la première entrée ne vaille : tout le monde à
  // l'arrêt.
  Player& local = players_[local_player_];
  local.known_through = input_delay_ticks;
  for (std::size_t player = 0; player < HeroCount(play_.run().field()); ++player) {
    players_[player].known_through = input_delay_ticks;
    players_[player].first_tick = input_delay_ticks;
  }
  play_.mutable_run().PutHeroOnStage(local_player_);
  present_ = play_.run();
}

bool SurvivorsMatch::left(std::size_t player) const noexcept {
  return players_[player].robot_from.has_value();
}

std::uint32_t SurvivorsMatch::lead_ticks() const noexcept {
  const std::uint32_t confirmed_tick = play_.run().field().ticks;
  return next_local_tick_ > confirmed_tick ? next_local_tick_ - confirmed_tick : 0;
}

bool SurvivorsMatch::PushLocal(const SurvivorsTeamInput& input) {
  const bool started = play_.run().field().ticks > input_delay_ticks_;
  if (lead_ticks() > (started ? kLockstepMostLeadTicks : kLockstepStartLeadTicks)) return false;
  Player& local = players_[local_player_];
  local.pending.push_back(input);
  local.known_through = next_local_tick_ + 1U;
  ++next_local_tick_;
  present_stale_ = true;
  sent_.push_back(input);
  // Une entrée ne s'oublie qu'une fois portée par une trame : une longue image pousse
  // plusieurs tics d'un coup, et aucun ne doit manquer aux autres.
  while (sent_.size() > kLockstepRedundantTicks &&
         next_local_tick_ - sent_.size() < framed_next_tick_) {
    sent_.pop_front();
  }
  return true;
}

std::optional<std::vector<std::uint8_t>> SurvivorsMatch::TakeFrame() {
  if (sent_.empty() || next_local_tick_ == framed_next_tick_) return std::nullopt;
  const bool over = play_.run().field().phase == SurvivorsPhase::kOver;
  if (next_local_tick_ < frame_due_tick_ && !over) return std::nullopt;
  frame_due_tick_ = next_local_tick_ + kLockstepFrameEveryTicks;
  framed_next_tick_ = next_local_tick_;
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kFrameHeaderBytes + sent_.size() * kInputBytes + kDigestBytes);
  PutU32(bytes, next_local_tick_ - 1U);
  bytes.push_back(static_cast<std::uint8_t>(sent_.size()));
  for (const SurvivorsTeamInput& input : sent_) {
    bytes.push_back(input.heading);
    bytes.push_back(input.ballot.has_value() ? PackAnswer(*input.ballot) : kLockstepNoBallot);
    bytes.push_back(input.ballot_vote);
  }
  if (digest_to_send_.has_value()) {
    PutU32(bytes, digest_to_send_->tick);
    PutU64(bytes, digest_to_send_->digest);
    digest_to_send_.reset();
  }
  return bytes;
}

bool SurvivorsMatch::Receive(std::size_t player, std::span<const std::uint8_t> bytes) {
  if (player >= HeroCount(play_.run().field()) || player == local_player_) return false;
  const std::optional<LockstepFrame> frame = DecodeLockstepFrame(bytes);
  if (!frame.has_value()) return false;
  Player& other = players_[player];
  if (other.robot_from.has_value()) return true;
  present_stale_ = true;
  const std::uint64_t first_of_frame =
      std::uint64_t{frame->through_tick} + 1U - frame->inputs.size();
  for (std::size_t index = 0; index < frame->inputs.size(); ++index) {
    const std::uint64_t tick = first_of_frame + index;
    // Une entrée déjà connue s'ignore ; un trou se comble par une trame suivante.
    if (tick != other.known_through || other.pending.size() >= kLockstepMostPendingTicks) continue;
    other.pending.push_back(frame->inputs[index]);
    other.last_heading = frame->inputs[index].heading;
    ++other.known_through;
  }
  if (frame->digest_tick.has_value()) {
    their_digests_.push_back(DigestAt{.tick = *frame->digest_tick, .digest = frame->digest});
    while (their_digests_.size() > kMostKeptDigests) their_digests_.pop_front();
    CheckDigests();
  }
  return true;
}

void SurvivorsMatch::Hear(std::size_t player, std::span<const std::uint8_t> bytes) {
  if (player >= HeroCount(play_.run().field()) || player == local_player_) return;
  const std::optional<LockstepFrame> frame = DecodeLockstepFrame(bytes);
  if (!frame.has_value() || frame->inputs.empty()) return;
  Player& other = players_[player];
  if (other.robot_from.has_value()) return;
  if (other.heard_tick.has_value() && frame->through_tick <= *other.heard_tick) return;
  other.heard_tick = frame->through_tick;
  other.heard_heading = frame->inputs.back().heading;
  present_stale_ = true;
}

void SurvivorsMatch::Leave(std::size_t player_index) {
  if (player_index >= HeroCount(play_.run().field()) || player_index == local_player_) return;
  Player& player = players_[player_index];
  if (player.robot_from.has_value()) return;
  player.robot_from = player.known_through;
  const ClassRule* hero_class = ClassAt(play_.run().field(), player_index);
  player.robot.emplace(hero_class != nullptr && hero_class->melee ? KnightBotProfile()
                                                                  : MageBotProfile());
}

bool SurvivorsMatch::TickReady(std::uint32_t tick) const noexcept {
  for (std::size_t player = 0; player < HeroCount(play_.run().field()); ++player) {
    const Player& each = players_[player];
    if (each.robot_from.has_value() && tick >= *each.robot_from) continue;
    if (tick >= each.known_through) return false;
  }
  return true;
}

void SurvivorsMatch::RunConfirmedTick() {
  present_stale_ = true;
  SurvivorsRun& run = play_.mutable_run();
  const std::uint32_t tick = run.field().ticks;
  const std::size_t count = HeroCount(run.field());
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  for (std::size_t player = 0; player < count; ++player) {
    Player& each = players_[player];
    if (each.robot_from.has_value() && tick >= *each.robot_from) {
      // LE ROBOT REPREND : le même calcul sur chaque client, au même tic. Il marche et
      // combat, mais ne vote pas : seuls les humains décident.
      if (!run.field().voter_gone[player]) play_.MarkLeft(player);
      run.PutHeroOnStage(player);
      inputs[player].heading = each.robot->Steer(run.field()).heading;
      continue;
    }
    if (tick < each.first_tick) continue;
    inputs[player] = each.pending.front();
    each.pending.pop_front();
    ++each.first_tick;
    // Un bulletin déposé pour un autre vote ne compte pour rien. Le journal n'en sait
    // rien : il ne garde que ce qui a compté.
    if (inputs[player].ballot_vote != run.field().vote_serial) inputs[player].ballot.reset();
    inputs[player].ballot_vote = 0;
  }
  run.PutHeroOnStage(0);
  play_.Tick(std::span<const SurvivorsTeamInput>(inputs.data(), count));
  SurvivorsNews news = play_.TakeNews();
  AppendSurvivorsNews(news_, std::move(news));
  run.PutHeroOnStage(local_player_);
  const std::uint32_t done_tick = run.field().ticks;
  if (done_tick % kLockstepDigestEveryTicks == 0) {
    const std::uint64_t digest = SurvivorsDigest(run.field());
    own_digests_.push_back(DigestAt{.tick = done_tick, .digest = digest});
    while (own_digests_.size() > kMostKeptDigests) own_digests_.pop_front();
    digest_to_send_ = DigestAt{.tick = done_tick, .digest = digest};
    CheckDigests();
  }
}

void SurvivorsMatch::CheckDigests() {
  for (const DigestAt& theirs : their_digests_) {
    for (const DigestAt& own : own_digests_) {
      if (own.tick == theirs.tick && own.digest != theirs.digest) diverged_ = true;
    }
  }
}

void SurvivorsMatch::RecomputePresent() {
  const SurvivorsRun& confirmed = play_.run();
  // L'AFFECTATION GARDE LES TAMPONS DE L'IMAGE D'AVANT, et l'adresse que le dessin suit.
  *present_ = confirmed;
  // Un vote ou une pause de coffre ne se prédisent pas : le présent les attend.
  if (confirmed.field().phase != SurvivorsPhase::kRunning ||
      confirmed.field().hold_ticks_left > 0 || next_local_tick_ <= confirmed.field().ticks) {
    return;
  }
  // LE PRÉSENT : les tics déjà joués par le joueur, les autres tenant leur cap. Un
  // vote ne se prédit pas : le présent s'y arrête.
  SurvivorsRun& present = *present_;
  const std::size_t count = HeroCount(present.field());
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  for (std::size_t player = 0; player < count; ++player) {
    const Player& each = players_[player];
    const bool heard_newer = each.heard_tick.has_value() && *each.heard_tick >= each.known_through;
    inputs[player].heading = heard_newer ? each.heard_heading : each.last_heading;
  }
  const Player& local = players_[local_player_];
  const std::uint32_t first = local.first_tick;
  for (std::size_t offset = 0; offset < local.pending.size(); ++offset) {
    if (first + offset < present.field().ticks) continue;
    if (present.field().phase != SurvivorsPhase::kRunning) break;
    inputs[local_player_].heading = local.pending[offset].heading;
    present.PutHeroOnStage(0);
    present.Tick(std::span<const SurvivorsTeamInput>(inputs.data(), count));
    (void)present.TakeNews();
  }
  present.PutHeroOnStage(local_player_);
}

void SurvivorsMatch::Advance() {
  while (play_.run().field().phase != SurvivorsPhase::kOver &&
         TickReady(play_.run().field().ticks)) {
    RunConfirmedTick();
  }
  if (!present_stale_) return;
  RecomputePresent();
  present_stale_ = false;
}

}  // namespace ro::sim
