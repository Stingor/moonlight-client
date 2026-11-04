#include "ro/sim/survivors_team_play.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_match.h"

namespace ro::sim {

namespace {

constexpr std::array<std::uint8_t, 4> kSignature = {'M', 'S', 'V', 'T'};
constexpr std::uint32_t kLebPayloadBits = 7;
constexpr std::uint8_t kLebContinue = 0x80;
constexpr std::uint8_t kLebPayloadMask = 0x7F;
constexpr std::size_t kMostLebBytes = 5;
constexpr std::size_t kByteBits = 8;
constexpr std::size_t kU64Bytes = 8;
// Le décalage de FNV-1a sur 64 bits, et son multiplicateur.
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] core::Error Corrupt(std::string detail) {
  return core::Fail(core::ErrorCode::kCorruptData, "journal coop : " + std::move(detail)).error();
}

class Writer {
 public:
  void U8(std::uint8_t value) { bytes_.push_back(value); }
  void U16(std::uint16_t value) {
    U8(static_cast<std::uint8_t>(value & 0xFFU));
    U8(static_cast<std::uint8_t>(value >> kByteBits));
  }
  void U64(std::uint64_t value) {
    for (std::size_t index = 0; index < kU64Bytes; ++index) {
      U8(static_cast<std::uint8_t>((value >> (index * kByteBits)) & 0xFFU));
    }
  }
  void Leb(std::uint32_t value) {
    while (value > kLebPayloadMask) {
      U8(static_cast<std::uint8_t>((value & kLebPayloadMask) | kLebContinue));
      value >>= kLebPayloadBits;
    }
    U8(static_cast<std::uint8_t>(value));
  }
  void Zigzag(std::int32_t value) {
    const auto as_unsigned = static_cast<std::uint32_t>(value);
    Leb((as_unsigned << 1U) ^ (value < 0 ? 0xFFFFFFFFU : 0U));
  }
  [[nodiscard]] std::vector<std::uint8_t> Take() { return std::move(bytes_); }

 private:
  std::vector<std::uint8_t> bytes_;
};

class Reader {
 public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}
  [[nodiscard]] bool ok() const noexcept { return ok_; }
  [[nodiscard]] bool done() const noexcept { return at_ == bytes_.size(); }
  [[nodiscard]] std::size_t left() const noexcept { return bytes_.size() - at_; }
  [[nodiscard]] std::uint8_t U8() noexcept {
    if (at_ >= bytes_.size()) {
      ok_ = false;
      return 0;
    }
    return bytes_[at_++];
  }
  [[nodiscard]] std::uint16_t U16() noexcept {
    const std::uint32_t low = U8();
    const std::uint32_t high = U8();
    return static_cast<std::uint16_t>(low | (high << kByteBits));
  }
  [[nodiscard]] std::uint64_t U64() noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < kU64Bytes; ++index) {
      value |= std::uint64_t{U8()} << (index * kByteBits);
    }
    return value;
  }
  // Rien pour un LEB128 trop long, qui dépasse 32 bits, ou écrit plus long que
  // nécessaire. Un flux épuisé se voit par `ok()`.
  [[nodiscard]] std::optional<std::uint32_t> Leb() noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < kMostLebBytes; ++index) {
      const std::uint8_t byte = U8();
      value |= static_cast<std::uint64_t>(byte & kLebPayloadMask) << (index * kLebPayloadBits);
      if ((byte & kLebContinue) == 0) {
        if (index > 0 && byte == 0) return std::nullopt;
        if (value > 0xFFFFFFFFULL) return std::nullopt;
        return static_cast<std::uint32_t>(value);
      }
    }
    return std::nullopt;
  }
  [[nodiscard]] std::optional<std::int32_t> Zigzag() noexcept {
    const std::optional<std::uint32_t> raw = Leb();
    if (!raw) return std::nullopt;
    return static_cast<std::int32_t>((*raw >> 1U) ^ (0U - (*raw & 1U)));
  }

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t at_ = 0;
  bool ok_ = true;
};

// La part d'un joueur dans le journal, sous la même forme pour le premier et
// pour ses alliés.
void WritePlayer(Writer& out, std::uint16_t class_id, std::uint16_t first_skill_id,
                 const SurvivorsLoadout& loadout, std::uint8_t lives, std::uint8_t leaves,
                 const std::vector<std::uint16_t>& locked_ids) {
  out.U16(class_id);
  out.U16(first_skill_id);
  for (const std::int32_t bonus : loadout.bonus.values) out.Zigzag(bonus);
  out.U8(loadout.rerolls);
  out.U8(loadout.skips);
  out.U8(loadout.banishes);
  out.U8(lives);
  out.U8(leaves);
  out.U8(static_cast<std::uint8_t>(locked_ids.size()));
  for (const std::uint16_t id : locked_ids) out.U16(id);
}

[[nodiscard]] std::optional<SurvivorsAllyStart> ReadPlayer(Reader& in) {
  SurvivorsAllyStart player;
  player.class_id = in.U16();
  player.first_skill_id = in.U16();
  for (std::int32_t& bonus : player.loadout.bonus.values) {
    const std::optional<std::int32_t> read = in.Zigzag();
    if (!read) return std::nullopt;
    bonus = *read;
  }
  if (!LoadoutWithinBounds(player.loadout)) return std::nullopt;
  player.loadout.rerolls = in.U8();
  player.loadout.skips = in.U8();
  player.loadout.banishes = in.U8();
  player.lives = in.U8();
  player.leaves = in.U8();
  const std::size_t locked = in.U8();
  if (locked > kMostLockedIds) return std::nullopt;
  for (std::size_t index = 0; index < locked; ++index) {
    const std::uint16_t id = in.U16();
    if (index > 0 && player.locked_ids.back() >= id) return std::nullopt;
    player.locked_ids.push_back(id);
  }
  return player;
}

[[nodiscard]] bool ValidHeading(std::uint8_t heading) noexcept {
  return heading == kNoHeading || heading < kHeadingSteps;
}

// FNV-1a, octet par octet : le même résultat sur toute machine.
class Digest {
 public:
  void Add(std::uint64_t value) noexcept {
    for (std::size_t index = 0; index < kU64Bytes; ++index) {
      hash_ ^= (value >> (index * kByteBits)) & 0xFFU;
      hash_ *= kFnvPrime;
    }
  }
  void AddSigned(std::int64_t value) noexcept { Add(static_cast<std::uint64_t>(value)); }
  void AddPoint(FieldPoint point) noexcept {
    AddSigned(point.x);
    AddSigned(point.y);
  }
  [[nodiscard]] std::uint64_t value() const noexcept { return hash_; }

 private:
  std::uint64_t hash_ = kFnvOffset;
};

}  // namespace

std::uint64_t SurvivorsRulesDigest(const SurvivorsRules& rules) {
  Digest digest;
  digest.Add(rules.ruleset_version);
  digest.Add(rules.run_ms);
  digest.Add(rules.classes.size());
  for (const ClassRule& rule : rules.classes) {
    digest.Add(rule.class_id);
    digest.AddSigned(rule.max_hp);
    digest.Add(rule.walk_ms_per_cell);
    digest.Add(rule.skill_pool.size());
    digest.Add(rule.card_pool.size());
  }
  digest.Add(rules.skills.size());
  for (const SurvivorsSkillRule& rule : rules.skills) {
    digest.Add(rule.skill_id);
    digest.Add(rule.levels.size());
  }
  digest.Add(rules.foes.size());
  for (const FoeRule& rule : rules.foes) {
    digest.Add(rule.foe_id);
    digest.AddSigned(rule.hp);
    digest.AddSigned(rule.touch_damage);
    digest.Add(rule.exp);
  }
  digest.Add(rules.arenas.size());
  for (const SurvivorsTeamScale& scale : kTeamScales) {
    digest.AddSigned(scale.exp_needed_pct);
    digest.AddSigned(scale.foe_hp_pct);
    digest.AddSigned(scale.batch_pct);
  }
  return digest.value();
}

std::vector<std::uint8_t> EncodeSurvivorsCoopPart(const SurvivorsCoopPart& part) {
  Writer out;
  out.U16(part.ruleset_version);
  out.U64(part.rules_digest);
  out.U16(part.arena_id);
  out.U8(part.cursed_water);
  WritePlayer(out, part.player.class_id, part.player.first_skill_id, part.player.loadout,
              part.player.lives, part.player.leaves, part.player.locked_ids);
  return out.Take();
}

std::optional<SurvivorsCoopPart> DecodeSurvivorsCoopPart(std::span<const std::uint8_t> bytes,
                                                         std::size_t* trailing_bytes) {
  Reader in(bytes);
  SurvivorsCoopPart part;
  part.ruleset_version = in.U16();
  part.rules_digest = in.U64();
  part.arena_id = in.U16();
  part.cursed_water = in.U8();
  std::optional<SurvivorsAllyStart> player = ReadPlayer(in);
  if (!in.ok() || !player.has_value()) return std::nullopt;
  part.player = std::move(*player);
  if (trailing_bytes != nullptr) *trailing_bytes = in.left();
  return part;
}

std::optional<SurvivorsStart> TeamStartOf(std::uint64_t seed, const SurvivorsRules& rules,
                                          std::span<const SurvivorsCoopPart> parts) {
  if (parts.size() < 2 || parts.size() > kMostHeroes) return std::nullopt;
  const std::uint64_t rules_digest = SurvivorsRulesDigest(rules);
  SurvivorsStart start;
  start.seed = seed;
  start.arena_id = parts.front().arena_id;
  start.cursed_water = parts.front().cursed_water;
  for (std::size_t index = 0; index < parts.size(); ++index) {
    const SurvivorsCoopPart& part = parts[index];
    if (part.ruleset_version != rules.ruleset_version || part.rules_digest != rules_digest) {
      return std::nullopt;
    }
    start.cursed_water = std::min(start.cursed_water, part.cursed_water);
    if (index > 0) start.allies.push_back(part.player);
  }
  const SurvivorsAllyStart& first = parts.front().player;
  start.class_id = first.class_id;
  start.first_skill_id = first.first_skill_id;
  start.loadout = first.loadout;
  start.lives = first.lives;
  start.leaves = first.leaves;
  start.locked_ids = first.locked_ids;
  return start;
}

std::vector<std::uint8_t> EncodeSurvivorsTeamJournal(const SurvivorsTeamJournal& journal) {
  Writer out;
  for (const std::uint8_t byte : kSignature) out.U8(byte);
  out.U16(journal.journal_version);
  out.U16(journal.ruleset_version);
  const SurvivorsStart& start = journal.start;
  out.U64(start.seed);
  out.U16(start.arena_id);
  out.U8(start.cursed_water);
  out.U8(static_cast<std::uint8_t>(start.allies.size() + 1));
  WritePlayer(out, start.class_id, start.first_skill_id, start.loadout, start.lives, start.leaves,
              start.locked_ids);
  for (const SurvivorsAllyStart& ally : start.allies) {
    WritePlayer(out, ally.class_id, ally.first_skill_id, ally.loadout, ally.lives, ally.leaves,
                ally.locked_ids);
  }
  out.Leb(static_cast<std::uint32_t>(journal.entries.size()));
  std::uint32_t tick = 0;
  for (const TeamJournalEntry& entry : journal.entries) {
    out.Leb(entry.tick - tick);
    tick = entry.tick;
    out.U8(entry.player);
    out.U8(static_cast<std::uint8_t>(entry.kind));
    out.U8(entry.value);
  }
  out.Leb(journal.final_tick - tick);
  return out.Take();
}

core::Result<SurvivorsTeamJournal> DecodeSurvivorsTeamJournal(std::span<const std::uint8_t> bytes) {
  Reader in(bytes);
  for (const std::uint8_t byte : kSignature) {
    const std::uint8_t read = in.U8();
    if (!in.ok()) return core::Fail(core::ErrorCode::kUnexpectedEnd, "journal coop tronqué");
    if (read != byte) return core::Fail(core::ErrorCode::kBadSignature, "pas un journal coop");
  }
  SurvivorsTeamJournal journal;
  journal.journal_version = in.U16();
  if (in.ok() && journal.journal_version != kSurvivorsTeamJournalVersion) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "journal coop de version " + std::to_string(journal.journal_version));
  }
  journal.ruleset_version = in.U16();
  SurvivorsStart& start = journal.start;
  start.seed = in.U64();
  start.arena_id = in.U16();
  start.cursed_water = in.U8();
  const std::size_t players = in.U8();
  if (!in.ok()) return core::Fail(core::ErrorCode::kUnexpectedEnd, "journal coop tronqué");
  if (players < 2 || players > kMostHeroes) {
    return std::unexpected(Corrupt(std::to_string(players) + " joueurs"));
  }
  for (std::size_t player = 0; player < players; ++player) {
    std::optional<SurvivorsAllyStart> read = ReadPlayer(in);
    if (!in.ok()) return core::Fail(core::ErrorCode::kUnexpectedEnd, "journal coop tronqué");
    if (!read) return std::unexpected(Corrupt("départ du joueur " + std::to_string(player)));
    if (player == 0) {
      start.class_id = read->class_id;
      start.first_skill_id = read->first_skill_id;
      start.loadout = read->loadout;
      start.lives = read->lives;
      start.leaves = read->leaves;
      start.locked_ids = std::move(read->locked_ids);
    } else {
      start.allies.push_back(std::move(*read));
    }
  }
  const std::optional<std::uint32_t> entries = in.Leb();
  if (!in.ok()) return core::Fail(core::ErrorCode::kUnexpectedEnd, "journal coop tronqué");
  if (!entries) return std::unexpected(Corrupt("nombre d'entrées illisible"));
  std::uint64_t tick = 0;
  // La réserve suit les octets, jamais le nombre annoncé.
  for (std::uint32_t index = 0; index < *entries; ++index) {
    const std::optional<std::uint32_t> gap = in.Leb();
    const std::uint8_t player = in.U8();
    const std::uint8_t kind = in.U8();
    const std::uint8_t value = in.U8();
    if (!in.ok()) return core::Fail(core::ErrorCode::kUnexpectedEnd, "journal coop tronqué");
    if (!gap) return std::unexpected(Corrupt("écart de tic illisible"));
    tick += *gap;
    if (tick > 0xFFFFFFFFULL) return std::unexpected(Corrupt("tic au-delà de 32 bits"));
    if (player >= players) return std::unexpected(Corrupt("joueur hors de la partie"));
    if (kind == static_cast<std::uint8_t>(TeamJournalKind::kHeading)) {
      if (!ValidHeading(value)) return std::unexpected(Corrupt("cap invalide"));
    } else if (kind == static_cast<std::uint8_t>(TeamJournalKind::kBallot)) {
      if (!UnpackAnswer(value)) return std::unexpected(Corrupt("bulletin illisible"));
    } else {
      return std::unexpected(Corrupt("sorte d'entrée inconnue"));
    }
    journal.entries.push_back(TeamJournalEntry{.tick = static_cast<std::uint32_t>(tick),
                                               .player = player,
                                               .kind = static_cast<TeamJournalKind>(kind),
                                               .value = value});
  }
  const std::optional<std::uint32_t> final_gap = in.Leb();
  if (!in.ok()) return core::Fail(core::ErrorCode::kUnexpectedEnd, "journal coop tronqué");
  if (!final_gap) return std::unexpected(Corrupt("écart du tic final illisible"));
  tick += *final_gap;
  if (tick > 0xFFFFFFFFULL) return std::unexpected(Corrupt("tic final au-delà de 32 bits"));
  journal.final_tick = static_cast<std::uint32_t>(tick);
  if (!in.done()) return std::unexpected(Corrupt("octets en trop"));
  return journal;
}

std::uint64_t SurvivorsDigest(const SurvivorsField& field) {
  Digest digest;
  digest.Add(field.ticks);
  digest.Add(field.step);
  digest.Add(static_cast<std::uint64_t>(field.phase));
  digest.Add(field.next_serial);
  digest.Add(field.leaves_left);
  digest.Add(field.rerolls_left);
  digest.Add(field.skips_left);
  digest.Add(field.banishes_left);
  digest.Add(field.pending_level_ups);
  digest.Add(field.spawn_random.state());
  digest.Add(field.offer_random.state());
  digest.Add(field.combat_random.state());
  digest.Add(field.loot_random.state());
  digest.Add(field.field_random.state());
  digest.Add(field.card_random.state());
  for (std::size_t index = 0; index < HeroCount(field); ++index) {
    const SurvivorsHero& hero = HeroAt(field, index);
    digest.AddPoint(hero.position);
    digest.AddSigned(hero.hp);
    digest.AddSigned(hero.max_hp);
    digest.Add(hero.level);
    digest.Add(hero.exp);
    digest.Add(hero.down ? 1U : 0U);
    digest.Add(hero.skills.size());
    digest.Add(hero.cards.size());
  }
  digest.Add(field.foes.size());
  for (const SurvivorsFoe& foe : field.foes) {
    digest.Add(foe.serial);
    digest.AddPoint(foe.position);
    digest.AddSigned(foe.hp);
  }
  digest.Add(field.gems.size());
  digest.Add(field.pickups.size());
  digest.Add(field.plants.size());
  return digest.value();
}

SurvivorsTeamPlay::SurvivorsTeamPlay(SurvivorsRun run) : run_(std::move(run)) {
  journal_.ruleset_version = run_.field().rules->ruleset_version;
  journal_.start = run_.field().start;
  journal_.final_tick = run_.field().ticks;
  last_headings_.fill(kNoHeading);
}

void SurvivorsTeamPlay::Tick(std::span<const SurvivorsTeamInput> inputs) {
  const std::uint32_t tick = run_.field().ticks;
  const std::size_t players = std::min(inputs.size(), kMostHeroes);
  for (std::size_t player = 0; player < players; ++player) {
    const std::uint8_t heading = inputs[player].heading;
    if (heading == last_headings_[player]) continue;
    last_headings_[player] = heading;
    journal_.entries.push_back(TeamJournalEntry{.tick = tick,
                                                .player = static_cast<std::uint8_t>(player),
                                                .kind = TeamJournalKind::kHeading,
                                                .value = heading});
  }
  for (std::size_t player = 0; player < players; ++player) {
    if (!inputs[player].ballot.has_value() || !run_.BallotValid(*inputs[player].ballot)) continue;
    journal_.entries.push_back(TeamJournalEntry{.tick = tick,
                                                .player = static_cast<std::uint8_t>(player),
                                                .kind = TeamJournalKind::kBallot,
                                                .value = PackAnswer(*inputs[player].ballot)});
  }
  run_.Tick(inputs);
  journal_.final_tick = run_.field().ticks;
}

core::Result<std::uint64_t> ReplaySurvivorsTeam(const SurvivorsRules& rules,
                                                const SurvivorsTeamJournal& journal) {
  if (journal.ruleset_version != rules.ruleset_version) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      "journal coop des règles " + std::to_string(journal.ruleset_version));
  }
  core::Result<SurvivorsRun> started = SurvivorsRun::Start(rules, journal.start);
  if (!started) return std::unexpected(std::move(started.error()));
  SurvivorsRun& run = *started;
  const std::size_t players = HeroCount(run.field());
  std::vector<SurvivorsTeamInput> inputs(players);
  std::size_t next_entry = 0;
  for (std::uint32_t tick = 0; tick < journal.final_tick; ++tick) {
    for (SurvivorsTeamInput& input : inputs) input.ballot.reset();
    for (; next_entry < journal.entries.size() && journal.entries[next_entry].tick == tick;
         ++next_entry) {
      const TeamJournalEntry& entry = journal.entries[next_entry];
      if (entry.player >= players) return std::unexpected(Corrupt("joueur hors de la partie"));
      switch (entry.kind) {
        case TeamJournalKind::kHeading:
          inputs[entry.player].heading = entry.value;
          break;
        case TeamJournalKind::kBallot:
          inputs[entry.player].ballot = UnpackAnswer(entry.value);
          break;
      }
    }
    run.Tick(inputs);
  }
  if (next_entry != journal.entries.size()) {
    return std::unexpected(Corrupt("entrées après le tic final"));
  }
  return SurvivorsDigest(run.field());
}

}  // namespace ro::sim
