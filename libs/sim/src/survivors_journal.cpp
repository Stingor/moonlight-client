#include "ro/sim/survivors_journal.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/sim/survivors_geometry.h"

namespace ro::sim {

namespace {

[[nodiscard]] std::unexpected<core::Error> Undecodable(core::ErrorCode code, std::string detail) {
  return core::Fail(code, "journal survivors : " + std::move(detail));
}

[[nodiscard]] std::unexpected<core::Error> Corrupt(std::string detail) {
  return Undecodable(core::ErrorCode::kCorruptData, std::move(detail));
}

[[nodiscard]] bool ValidHeading(std::uint8_t heading) noexcept {
  return heading < kHeadingSteps || heading == kNoHeading;
}

// ── L'empaquetage d'une réponse ─────────────────────────────────────────────
// L'index tient sur 4 bits : une réponse ACCEPTÉE désigne une offre, dont
// l'index est sous kMostOffers. Un index plus grand serait tronqué sans bruit.
constexpr std::uint8_t kAnswerKindShift = 4;
constexpr std::uint8_t kAnswerIndexMask = 0x0F;
static_assert(kMostOffers <= kAnswerIndexMask + 1);

// ── Le format du journal ────────────────────────────────────────────────────
constexpr std::array<std::uint8_t, 4> kJournalSignature{'M', 'S', 'V', 'J'};

// LEB128 non signé : 7 bits de charge par octet, poids faibles en tête, le
// bit haut annonce un octet de plus. Une valeur de 32 bits tient en 5 octets.
constexpr std::uint32_t kLeb128PayloadBits = 7;
constexpr std::uint8_t kLeb128PayloadMask = 0x7F;
constexpr std::uint8_t kLeb128MoreFlag = 0x80;
constexpr std::size_t kMostLeb128Bytes = 5;
// Une entrée : un écart de pas d'un octet au moins, sa sorte et sa valeur.
constexpr std::size_t kLeastEntryBytes = 3;

constexpr std::uint32_t kSignBitShift = 31;

void WriteLeb128(core::ByteWriter& writer, std::uint32_t value) {
  while (value > kLeb128PayloadMask) {
    writer.U8(static_cast<std::uint8_t>((value & kLeb128PayloadMask) | kLeb128MoreFlag));
    value >>= kLeb128PayloadBits;
  }
  writer.U8(static_cast<std::uint8_t>(value));
}

// Zigzag : 0, -1, 1, -2… deviennent 0, 1, 2, 3…, pour qu'un petit négatif
// reste court en LEB128.
[[nodiscard]] constexpr std::uint32_t ZigzagOf(std::int32_t value) noexcept {
  return (static_cast<std::uint32_t>(value) << 1) ^
         static_cast<std::uint32_t>(value >> kSignBitShift);
}

[[nodiscard]] constexpr std::int32_t UnzigzagOf(std::uint32_t value) noexcept {
  return static_cast<std::int32_t>((value >> 1) ^ (0U - (value & 1U)));
}

// Un LEB128 de 32 bits au plus, sous sa forme la plus courte : une valeur n'a
// qu'une écriture, et un journal qu'une suite d'octets. Tronqué :
// kUnexpectedEnd ; plus de 5 octets, un dernier octet nul après un octet de
// suite, ou une valeur qui déborde 32 bits : kCorruptData.
[[nodiscard]] core::Result<std::uint32_t> ReadLeb128(core::ByteReader& reader, const char* what) {
  std::uint64_t value = 0;
  for (std::size_t index = 0; index < kMostLeb128Bytes; ++index) {
    const std::uint8_t byte = reader.U8();
    if (!reader.ok())
      return Undecodable(core::ErrorCode::kUnexpectedEnd, std::string(what) + " tronqué");
    value |= static_cast<std::uint64_t>(byte & kLeb128PayloadMask) << (kLeb128PayloadBits * index);
    if ((byte & kLeb128MoreFlag) == 0) {
      if (byte == 0 && index > 0) {
        return Undecodable(core::ErrorCode::kCorruptData,
                           std::string(what) + " écrit plus long que nécessaire");
      }
      if (value > std::numeric_limits<std::uint32_t>::max()) {
        return Undecodable(core::ErrorCode::kCorruptData,
                           std::string(what) + " au-delà de 32 bits");
      }
      return static_cast<std::uint32_t>(value);
    }
  }
  return Undecodable(
      core::ErrorCode::kCorruptData,
      std::string(what) + " de plus de " + std::to_string(kMostLeb128Bytes) + " octets");
}

// Le pas d'une entrée : le précédent plus l'écart lu, sans dépasser 32 bits.
[[nodiscard]] core::Result<std::uint32_t> ReadStep(core::ByteReader& reader, std::uint32_t previous,
                                                   const char* what) {
  core::Result<std::uint32_t> gap = ReadLeb128(reader, what);
  if (!gap) return gap;
  const std::uint64_t step = std::uint64_t{previous} + *gap;
  if (step > std::numeric_limits<std::uint32_t>::max()) {
    return Undecodable(core::ErrorCode::kCorruptData, std::string(what) + " au-delà de 32 bits");
  }
  return static_cast<std::uint32_t>(step);
}

}  // namespace

std::uint8_t PackAnswer(SurvivorsAnswer answer) noexcept {
  return static_cast<std::uint8_t>((static_cast<std::uint8_t>(answer.kind) << kAnswerKindShift) |
                                   (answer.offer_index & kAnswerIndexMask));
}

std::optional<SurvivorsAnswer> UnpackAnswer(std::uint8_t value) noexcept {
  const auto kind = static_cast<std::uint8_t>(value >> kAnswerKindShift);
  if (kind > static_cast<std::uint8_t>(AnswerKind::kBanish)) return std::nullopt;
  return SurvivorsAnswer{.kind = static_cast<AnswerKind>(kind),
                         .offer_index = static_cast<std::uint8_t>(value & kAnswerIndexMask)};
}

std::vector<std::uint8_t> EncodeSurvivorsJournal(const SurvivorsJournal& journal) {
  std::vector<std::uint8_t> bytes;
  core::ByteWriter writer(bytes);
  writer.Bytes(kJournalSignature);
  writer.U16(journal.journal_version);
  writer.U16(journal.ruleset_version);
  writer.U64(journal.start.seed);
  writer.U16(journal.start.class_id);
  writer.U16(journal.start.arena_id);
  writer.U16(journal.start.first_skill_id);
  for (const std::int32_t bonus : journal.start.loadout.bonus.values) {
    WriteLeb128(writer, ZigzagOf(bonus));
  }
  writer.U8(journal.start.loadout.rerolls);
  writer.U8(journal.start.loadout.skips);
  writer.U8(journal.start.loadout.banishes);
  writer.U8(journal.start.lives);
  writer.U8(journal.start.cursed_water);
  // Le compte tient sur un octet : un départ au-delà de kMostLockedIds est
  // refusé par Start, et son journal ne se relirait pas.
  writer.U8(static_cast<std::uint8_t>(journal.start.locked_ids.size()));
  for (const std::uint16_t id : journal.start.locked_ids) writer.U16(id);
  WriteLeb128(writer, static_cast<std::uint32_t>(journal.entries.size()));
  // Chaque pas s'écrit en écart au précédent, modulo 2^32 : un journal en
  // désordre donne un écart qui déborde, que le décodage refuse comme le
  // rejeu refuserait le désordre.
  std::uint32_t previous_step = 0;
  for (const JournalEntry& entry : journal.entries) {
    WriteLeb128(writer, entry.step - previous_step);
    writer.U8(static_cast<std::uint8_t>(entry.kind));
    writer.U8(entry.value);
    previous_step = entry.step;
  }
  WriteLeb128(writer, journal.final_step - previous_step);
  return bytes;
}

core::Result<SurvivorsJournal> DecodeSurvivorsJournal(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kJournalSignature.size()) {
    return Undecodable(core::ErrorCode::kUnexpectedEnd, "plus court que sa signature");
  }
  if (!std::equal(kJournalSignature.begin(), kJournalSignature.end(), bytes.begin())) {
    return Undecodable(core::ErrorCode::kBadSignature, "signature inconnue");
  }
  core::ByteReader reader(bytes.subspan(kJournalSignature.size()));
  SurvivorsJournal journal;
  journal.journal_version = reader.U16();
  if (!reader.ok()) return Undecodable(core::ErrorCode::kUnexpectedEnd, "version tronquée");
  if (journal.journal_version != kSurvivorsJournalVersion) {
    return Undecodable(core::ErrorCode::kUnsupportedVersion,
                       "version " + std::to_string(journal.journal_version) + ", attendu " +
                           std::to_string(kSurvivorsJournalVersion));
  }
  journal.ruleset_version = reader.U16();
  journal.start.seed = reader.U64();
  journal.start.class_id = reader.U16();
  journal.start.arena_id = reader.U16();
  journal.start.first_skill_id = reader.U16();
  if (!reader.ok()) return Undecodable(core::ErrorCode::kUnexpectedEnd, "départ tronqué");
  for (std::int32_t& bonus : journal.start.loadout.bonus.values) {
    core::Result<std::uint32_t> zigzag = ReadLeb128(reader, "bonus");
    if (!zigzag) return std::unexpected(std::move(zigzag.error()));
    bonus = UnzigzagOf(*zigzag);
  }
  if (!LoadoutWithinBounds(journal.start.loadout)) {
    return Undecodable(core::ErrorCode::kCorruptData,
                       "bonus de loadout au-delà de ±" + std::to_string(kMostStatBonus));
  }
  journal.start.loadout.rerolls = reader.U8();
  journal.start.loadout.skips = reader.U8();
  journal.start.loadout.banishes = reader.U8();
  journal.start.lives = reader.U8();
  journal.start.cursed_water = reader.U8();
  const std::uint8_t locked_count = reader.U8();
  if (!reader.ok()) return Undecodable(core::ErrorCode::kUnexpectedEnd, "charges tronquées");
  if (journal.start.cursed_water > kMostCursedWater) {
    return Corrupt("Cursed Water " + std::to_string(journal.start.cursed_water));
  }
  if (locked_count > kMostLockedIds) {
    return Corrupt(std::to_string(locked_count) + " Id verrouillés");
  }
  journal.start.locked_ids.reserve(locked_count);
  for (std::uint8_t index = 0; index < locked_count; ++index) {
    const std::uint16_t id = reader.U16();
    if (!reader.ok()) return Undecodable(core::ErrorCode::kUnexpectedEnd, "verrous tronqués");
    if (!journal.start.locked_ids.empty() && journal.start.locked_ids.back() >= id) {
      return Corrupt("Id verrouillés non croissants en " + std::to_string(id));
    }
    journal.start.locked_ids.push_back(id);
  }

  const core::Result<std::uint32_t> count = ReadLeb128(reader, "nombre d'entrées");
  if (!count) return std::unexpected(count.error());
  // On ne réserve que les entrées que le reste du tampon peut porter.
  journal.entries.reserve(std::min<std::size_t>(*count, reader.remaining() / kLeastEntryBytes));
  std::uint32_t step = 0;
  for (std::uint32_t index = 0; index < *count; ++index) {
    core::Result<std::uint32_t> entry_step = ReadStep(reader, step, "pas d'entrée");
    if (!entry_step) return std::unexpected(std::move(entry_step.error()));
    step = *entry_step;
    const std::uint8_t kind = reader.U8();
    const std::uint8_t value = reader.U8();
    if (!reader.ok()) return Undecodable(core::ErrorCode::kUnexpectedEnd, "entrée tronquée");
    if (kind > static_cast<std::uint8_t>(JournalKind::kAbandon)) {
      return Undecodable(core::ErrorCode::kCorruptData, "sorte d'entrée " + std::to_string(kind) +
                                                            " au pas " + std::to_string(step));
    }
    if (static_cast<JournalKind>(kind) == JournalKind::kHeading && !ValidHeading(value)) {
      return Undecodable(core::ErrorCode::kCorruptData,
                         "cap " + std::to_string(value) + " au pas " + std::to_string(step));
    }
    journal.entries.push_back(
        JournalEntry{.step = step, .kind = static_cast<JournalKind>(kind), .value = value});
  }
  core::Result<std::uint32_t> final_step = ReadStep(reader, step, "pas final");
  if (!final_step) return std::unexpected(std::move(final_step.error()));
  journal.final_step = *final_step;
  if (reader.remaining() != 0) {
    return Undecodable(core::ErrorCode::kCorruptData,
                       std::to_string(reader.remaining()) + " octets en trop");
  }
  return journal;
}

namespace {

// LA PARTIE REJOUÉE ET CE QUE LE JOURNAL EN A CONSOMMÉ.
struct Replayed {
  SurvivorsRun run;
  std::size_t next_entry = 0;
};

// Rejoue jusqu'au pas `stop_step`, au plus le pas final du journal, ou jusqu'à
// la fin de la partie : sur la carte `arena`, ou sans elle sur le sol généré
// de l'arène du journal.
core::Result<Replayed> ReplayUntil(const SurvivorsRules& rules, std::optional<WalkMap> arena,
                                   const SurvivorsJournal& journal, std::uint32_t stop_step) {
  if (journal.ruleset_version != rules.ruleset_version) {
    return Undecodable(core::ErrorCode::kUnsupportedVersion,
                       "règles " + std::to_string(journal.ruleset_version) + ", attendu " +
                           std::to_string(rules.ruleset_version));
  }
  core::Result<SurvivorsRun> started =
      arena ? SurvivorsRun::Start(rules, std::move(*arena), journal.start)
            : SurvivorsRun::Start(rules, journal.start);
  if (!started) return std::unexpected(std::move(started.error()));
  SurvivorsRun& run = *started;
  const std::uint32_t last_step = std::min(stop_step, journal.final_step);

  std::uint8_t heading = kNoHeading;
  std::size_t next_entry = 0;
  // Les entrées d'un pas s'appliquent avant lui, celles du pas final comprises :
  // c'est là que s'inscrit un abandon.
  for (;;) {
    const std::uint32_t step = run.field().step;
    for (; next_entry < journal.entries.size() && journal.entries[next_entry].step <= step;
         ++next_entry) {
      const JournalEntry& entry = journal.entries[next_entry];
      if (entry.step < step) {
        return Corrupt("entrée du pas " + std::to_string(entry.step) + " lue au pas " +
                       std::to_string(step));
      }
      if (run.field().phase == SurvivorsPhase::kOver) {
        return Corrupt("entrée après la fin, au pas " + std::to_string(step));
      }
      switch (entry.kind) {
        case JournalKind::kHeading:
          if (!ValidHeading(entry.value)) {
            return Corrupt("cap " + std::to_string(entry.value) + " au pas " +
                           std::to_string(step));
          }
          heading = entry.value;
          break;
        case JournalKind::kAnswer: {
          const std::optional<SurvivorsAnswer> answer = UnpackAnswer(entry.value);
          if (!answer || !run.Answer(*answer)) {
            return Corrupt("réponse refusée au pas " + std::to_string(step));
          }
          break;
        }
        case JournalKind::kAbandon:
          run.Abandon();
          break;
        default:
          return Corrupt("sorte d'entrée inconnue au pas " + std::to_string(step));
      }
    }
    if (run.field().phase == SurvivorsPhase::kOver || step >= last_step) break;
    if (run.field().phase == SurvivorsPhase::kChoosing) {
      return Corrupt("choix sans réponse au pas " + std::to_string(step));
    }
    run.Step(SurvivorsInput{.heading = heading});
  }
  return Replayed{.run = std::move(run), .next_entry = next_entry};
}

// Le bilan d'un journal rejoué jusqu'à son pas final, qui doit l'avoir lu en
// entier et s'y être arrêté.
core::Result<SurvivorsTally> ReplayWhole(const SurvivorsRules& rules, std::optional<WalkMap> arena,
                                         const SurvivorsJournal& journal) {
  core::Result<Replayed> replayed =
      ReplayUntil(rules, std::move(arena), journal, journal.final_step);
  if (!replayed) return std::unexpected(std::move(replayed.error()));
  if (replayed->next_entry != journal.entries.size()) {
    return Corrupt("entrées après le pas final " + std::to_string(journal.final_step));
  }
  if (replayed->run.field().step != journal.final_step) {
    return Corrupt("partie finie au pas " + std::to_string(replayed->run.field().step) +
                   ", journal au pas " + std::to_string(journal.final_step));
  }
  return replayed->run.tally();
}

core::Result<SurvivorsRun> ReplayPart(const SurvivorsRules& rules, std::optional<WalkMap> arena,
                                      const SurvivorsJournal& journal, std::uint32_t stop_step) {
  core::Result<Replayed> replayed = ReplayUntil(rules, std::move(arena), journal, stop_step);
  if (!replayed) return std::unexpected(std::move(replayed.error()));
  return std::move(replayed->run);
}

}  // namespace

core::Result<SurvivorsTally> ReplaySurvivors(const SurvivorsRules& rules, WalkMap arena,
                                             const SurvivorsJournal& journal) {
  return ReplayWhole(rules, std::move(arena), journal);
}

core::Result<SurvivorsTally> ReplaySurvivors(const SurvivorsRules& rules,
                                             const SurvivorsJournal& journal) {
  return ReplayWhole(rules, std::nullopt, journal);
}

core::Result<SurvivorsRun> ReplaySurvivorsTo(const SurvivorsRules& rules, WalkMap arena,
                                             const SurvivorsJournal& journal,
                                             std::uint32_t stop_step) {
  return ReplayPart(rules, std::move(arena), journal, stop_step);
}

core::Result<SurvivorsRun> ReplaySurvivorsTo(const SurvivorsRules& rules,
                                             const SurvivorsJournal& journal,
                                             std::uint32_t stop_step) {
  return ReplayPart(rules, std::nullopt, journal, stop_step);
}

}  // namespace ro::sim
