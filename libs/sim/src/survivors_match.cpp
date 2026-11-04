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
constexpr std::size_t kInputBytes = 2;
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
  into.reborn = into.reborn || more.reborn;
  into.gems_gathered += more.gems_gathered;
  into.hero_hurt += more.hero_hurt;
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
    : play_(std::move(run)), local_player_(local_player), next_local_tick_(input_delay_ticks) {
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
  return players_[player].left_after.has_value();
}

std::uint32_t SurvivorsMatch::lead_ticks() const noexcept {
  const std::uint32_t confirmed_tick = play_.run().field().ticks;
  return next_local_tick_ > confirmed_tick ? next_local_tick_ - confirmed_tick : 0;
}

bool SurvivorsMatch::PushLocal(const SurvivorsTeamInput& input) {
  if (lead_ticks() > kLockstepMostLeadTicks) return false;
  Player& local = players_[local_player_];
  local.pending.push_back(input);
  local.known_through = next_local_tick_ + 1U;
  ++next_local_tick_;
  sent_.push_back(input);
  while (sent_.size() > kLockstepRedundantTicks) sent_.pop_front();
  return true;
}

std::optional<std::vector<std::uint8_t>> SurvivorsMatch::TakeFrame() {
  if (sent_.empty() || next_local_tick_ < frame_due_tick_) return std::nullopt;
  frame_due_tick_ = next_local_tick_ + kLockstepFrameEveryTicks;
  std::vector<std::uint8_t> bytes;
  bytes.reserve(kFrameHeaderBytes + sent_.size() * kInputBytes + kDigestBytes);
  PutU32(bytes, next_local_tick_ - 1U);
  bytes.push_back(static_cast<std::uint8_t>(sent_.size()));
  for (const SurvivorsTeamInput& input : sent_) {
    bytes.push_back(input.heading);
    bytes.push_back(input.ballot.has_value() ? PackAnswer(*input.ballot) : kLockstepNoBallot);
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
  const std::uint64_t first_of_frame =
      std::uint64_t{frame->through_tick} + 1U - frame->inputs.size();
  for (std::size_t index = 0; index < frame->inputs.size(); ++index) {
    const std::uint64_t tick = first_of_frame + index;
    // Une entrée déjà connue s'ignore ; un trou se comble par une trame suivante.
    if (tick != other.known_through) continue;
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

void SurvivorsMatch::Leave(const LockstepLeave& leave) {
  if (leave.player >= HeroCount(play_.run().field()) || leave.player == local_player_) return;
  Player& player = players_[leave.player];
  player.left_after = leave.through_tick;
  const ClassRule* hero_class = ClassAt(play_.run().field(), leave.player);
  player.robot.emplace(hero_class != nullptr && hero_class->melee ? KnightBotProfile()
                                                                  : MageBotProfile());
}

bool SurvivorsMatch::TickReady(std::uint32_t tick) const noexcept {
  for (std::size_t player = 0; player < HeroCount(play_.run().field()); ++player) {
    const Player& each = players_[player];
    if (each.left_after.has_value() && tick > *each.left_after) continue;
    if (tick >= each.known_through) return false;
  }
  return true;
}

void SurvivorsMatch::RunConfirmedTick() {
  SurvivorsRun& run = play_.mutable_run();
  const std::uint32_t tick = run.field().ticks;
  const std::size_t count = HeroCount(run.field());
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  for (std::size_t player = 0; player < count; ++player) {
    Player& each = players_[player];
    if (each.left_after.has_value() && tick > *each.left_after) {
      // LE ROBOT REPREND : le même calcul sur chaque client, au même tic.
      run.PutHeroOnStage(player);
      inputs[player].heading = each.robot->Steer(run.field()).heading;
      if (run.field().phase == SurvivorsPhase::kChoosing) {
        inputs[player].ballot = each.robot->Choose(run.field());
      }
      continue;
    }
    if (tick < each.first_tick) continue;
    inputs[player] = each.pending.front();
    each.pending.pop_front();
    ++each.first_tick;
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
  if (confirmed.field().phase == SurvivorsPhase::kOver ||
      next_local_tick_ <= confirmed.field().ticks) {
    present_ = confirmed;
    return;
  }
  // LE PRÉSENT : les tics déjà joués par le joueur, les autres tenant leur cap. Un
  // vote ne se prédit pas : le présent s'y arrête.
  SurvivorsRun present = confirmed;
  const std::size_t count = HeroCount(present.field());
  std::array<SurvivorsTeamInput, kMostHeroes> inputs{};
  for (std::size_t player = 0; player < count; ++player) {
    inputs[player].heading = players_[player].last_heading;
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
  present_ = std::move(present);
}

void SurvivorsMatch::Advance() {
  while (play_.run().field().phase != SurvivorsPhase::kOver &&
         TickReady(play_.run().field().ticks)) {
    RunConfirmedTick();
  }
  RecomputePresent();
}

}  // namespace ro::sim
