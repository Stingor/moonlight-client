#include "ro/sim/survivors_journal.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"

using ro::core::ErrorCode;
using ro::sim::AnswerKind;
using ro::sim::DecodeSurvivorsJournal;
using ro::sim::EncodeSurvivorsJournal;
using ro::sim::FoeRank;
using ro::sim::FoeRule;
using ro::sim::JournalEntry;
using ro::sim::JournalKind;
using ro::sim::KillPoints;
using ro::sim::PackAnswer;
using ro::sim::SurvivorsAnswer;
using ro::sim::SurvivorsJournal;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsScore;
using ro::sim::SurvivorsStat;
using ro::sim::SurvivorsTally;
using ro::sim::UnpackAnswer;

namespace {

constexpr std::uint16_t kPinnedGolemCardId = 4072;
constexpr std::uint16_t kPinnedMasteringCardId = 4197;
constexpr std::uint32_t kLastStep = std::numeric_limits<std::uint32_t>::max();

// Le code d'échec du décodage, ou rien s'il réussit.
[[nodiscard]] std::optional<ErrorCode> DecodeFailure(std::span<const std::uint8_t> bytes) {
  const ro::core::Result<SurvivorsJournal> decoded = DecodeSurvivorsJournal(bytes);
  if (decoded) return std::nullopt;
  return decoded.error().code;
}

// Un petit journal dont les octets sont écrits à la main, ci-dessous.
[[nodiscard]] SurvivorsJournal PinnedJournal() {
  SurvivorsJournal journal;
  journal.ruleset_version = 1;
  journal.start.seed = 0x0102030405060708ULL;
  journal.start.class_id = 1;
  journal.start.arena_id = 2;
  journal.start.first_skill_id = 19;
  journal.start.loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kDamagePct)] = -1;
  journal.start.loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kMaxHp)] = 64;
  journal.start.loadout.rerolls = 1;
  journal.start.loadout.skips = 2;
  journal.start.loadout.banishes = 3;
  journal.start.lives = 1;
  journal.start.cursed_water = 2;
  journal.start.locked_ids = {kPinnedGolemCardId, kPinnedMasteringCardId};
  journal.entries = {
      JournalEntry{.step = 5, .kind = JournalKind::kHeading, .value = 7},
      JournalEntry{.step = 5,
                   .kind = JournalKind::kAnswer,
                   .value = PackAnswer({.kind = AnswerKind::kReroll, .offer_index = 0})},
  };
  journal.final_step = 300;
  return journal;
}

// clang-format off
const std::vector<std::uint8_t> kPinnedBytes{
    'M', 'S', 'V', 'J',
    0x02, 0x00,                                      // journal_version
    0x01, 0x00,                                      // ruleset_version
    0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,  // graine
    0x01, 0x00,                                      // classe
    0x02, 0x00,                                      // arène
    0x13, 0x00,                                      // premier skill
    0x01,                                            // kDamagePct -1 en zigzag
    0x80, 0x01,                                      // kMaxHp 64 : zigzag 128
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x02, 0x03,                                // charges
    0x01,                                            // une vie
    0x02,                                            // Cursed Water 2
    0x02, 0xE8, 0x0F, 0x65, 0x10,                    // deux Id verrouillés : 4072, 4197
    0x02,                                            // deux entrées
    0x05, 0x00, 0x07,                                // pas 5, cap 7
    0x00, 0x01, 0x10,                                // pas 5, relance
    0xA7, 0x02,                                      // pas final 300 : écart 295
};
// clang-format on
constexpr std::size_t kPinnedBonusOffset = 22;
constexpr std::size_t kPinnedCursedWaterOffset = 39;
constexpr std::size_t kPinnedLockedCountOffset = 40;
constexpr std::size_t kPinnedSecondLockedOffset = 43;
constexpr std::size_t kPinnedCountOffset = 45;
constexpr std::size_t kPinnedFirstKindOffset = 47;
constexpr std::size_t kPinnedFirstValueOffset = 48;

// Les octets du journal épinglé jusqu'au nombre d'entrées, exclu.
[[nodiscard]] std::vector<std::uint8_t> PinnedHeader() {
  return {kPinnedBytes.begin(),
          kPinnedBytes.begin() + static_cast<std::ptrdiff_t>(kPinnedCountOffset)};
}

void Append(std::vector<std::uint8_t>& bytes, std::initializer_list<std::uint8_t> more) {
  bytes.insert(bytes.end(), more.begin(), more.end());
}

}  // namespace

TEST_CASE("Score survivors : les points d'un tué", "[sim][survivors]") {
  // Des valeurs écrites, pas les constantes : le serveur refait ces facteurs,
  // et les changer monte kSurvivorsRulesetVersion.
  CHECK(KillPoints(FoeRule{.ro_level = 3, .rank = FoeRank::kCommon}, false) == 3U);
  CHECK(KillPoints(FoeRule{.ro_level = 2, .rank = FoeRank::kMiniBoss}, false) == 40U);
  CHECK(KillPoints(FoeRule{.ro_level = 4, .rank = FoeRank::kBoss}, false) == 400U);
}

TEST_CASE("Score survivors : la formule que le serveur refait", "[sim][survivors]") {
  SurvivorsTally tally;
  tally.kill_points = 30;
  tally.survived_steps = 51;
  tally.level = 3;
  // 30 + 10 × (51 / 50) + 100 × 3, plus le bonus de l'issue.
  tally.outcome = SurvivorsOutcome::kSurvived;
  CHECK(SurvivorsScore(tally) == 5340U);
  tally.outcome = SurvivorsOutcome::kVictory;
  CHECK(SurvivorsScore(tally) == 25340U);
  tally.outcome = SurvivorsOutcome::kFell;
  CHECK(SurvivorsScore(tally) == 340U);
  tally.outcome = SurvivorsOutcome::kAbandoned;
  CHECK(SurvivorsScore(tally) == 340U);
  tally.outcome = SurvivorsOutcome::kRunning;
  CHECK(SurvivorsScore(tally) == 340U);

  // 10 points par SECONDE entière : une seconde entamée ne compte pas.
  tally.survived_steps = 49;
  CHECK(SurvivorsScore(tally) == 330U);
  tally.survived_steps = 50;
  CHECK(SurvivorsScore(tally) == 340U);
  tally.survived_steps = 99;
  CHECK(SurvivorsScore(tally) == 340U);
  tally.survived_steps = 100;
  CHECK(SurvivorsScore(tally) == 350U);

  // Aucun débordement, même au plus haut de chaque champ :
  // 4 294 967 295 + 10 × 85 899 345 + 100 × 65 535 + 25 000.
  tally.kill_points = std::numeric_limits<std::uint32_t>::max();
  tally.survived_steps = std::numeric_limits<std::uint32_t>::max();
  tally.level = std::numeric_limits<std::uint16_t>::max();
  tally.outcome = SurvivorsOutcome::kVictory;
  CHECK(SurvivorsScore(tally) == 5160539245ULL);
}

TEST_CASE("Journal survivors : un journal neuf porte la version courante", "[sim][survivors]") {
  const SurvivorsJournal journal;
  CHECK(journal.journal_version == ro::sim::kSurvivorsJournalVersion);
  CHECK(journal.entries.empty());
  CHECK(journal.final_step == 0U);
  CHECK(journal == SurvivorsJournal{});
}

TEST_CASE("Journal survivors : une réponse tient dans un octet", "[sim][survivors]") {
  constexpr std::array<AnswerKind, 4> kKinds{AnswerKind::kPick, AnswerKind::kReroll,
                                             AnswerKind::kSkip, AnswerKind::kBanish};
  constexpr std::uint8_t kIndexCount = 16;
  for (const AnswerKind kind : kKinds) {
    for (std::uint8_t index = 0; index < kIndexCount; ++index) {
      const SurvivorsAnswer answer{.kind = kind, .offer_index = index};
      const std::optional<SurvivorsAnswer> unpacked = UnpackAnswer(PackAnswer(answer));
      REQUIRE(unpacked.has_value());
      CHECK(*unpacked == answer);
    }
  }
  // La sorte sur les 4 bits hauts, l'index sur les 4 bas.
  CHECK(PackAnswer({.kind = AnswerKind::kBanish, .offer_index = 2}) == 0x32);
  CHECK(PackAnswer({.kind = AnswerKind::kPick, .offer_index = 0}) == 0x00);
  // Une sorte inconnue ne se déballe pas.
  CHECK_FALSE(UnpackAnswer(0x50).has_value());
  CHECK_FALSE(UnpackAnswer(0xF3).has_value());
}

TEST_CASE("Journal survivors : le format écrit octet par octet", "[sim][survivors]") {
  const SurvivorsJournal journal = PinnedJournal();
  CHECK(EncodeSurvivorsJournal(journal) == kPinnedBytes);
  const ro::core::Result<SurvivorsJournal> decoded = DecodeSurvivorsJournal(kPinnedBytes);
  REQUIRE(decoded.has_value());
  CHECK(*decoded == journal);
}

TEST_CASE("Journal survivors : un journal fait l'aller-retour", "[sim][survivors]") {
  SurvivorsJournal journal;
  journal.ruleset_version = ro::sim::kSurvivorsRulesetVersion;
  journal.start.seed = std::numeric_limits<std::uint64_t>::max();
  journal.start.class_id = 0xFFFF;
  journal.start.arena_id = 7;
  journal.start.first_skill_id = 14;
  // Les bornes du loadout, et chaque passage d'un octet de LEB128 au suivant ;
  // les cinq octets s'éprouvent sur les pas, plus bas.
  constexpr std::array<std::int32_t, ro::sim::kSurvivorsStatCount> kBonus{-ro::sim::kMostStatBonus,
                                                                          ro::sim::kMostStatBonus,
                                                                          -1,
                                                                          0,
                                                                          1,
                                                                          63,
                                                                          -64,
                                                                          64,
                                                                          -65,
                                                                          8191,
                                                                          -8192,
                                                                          8192};
  journal.start.loadout.bonus.values = kBonus;
  journal.start.loadout.rerolls = 0xFF;
  journal.start.loadout.skips = 0;
  journal.start.loadout.banishes = 9;
  journal.entries = {
      JournalEntry{.step = 0, .kind = JournalKind::kHeading, .value = 0},
      JournalEntry{.step = 0, .kind = JournalKind::kHeading, .value = ro::sim::kNoHeading},
      JournalEntry{.step = 1, .kind = JournalKind::kHeading, .value = ro::sim::kHeadingSteps - 1},
      JournalEntry{.step = 127,
                   .kind = JournalKind::kAnswer,
                   .value = PackAnswer({.kind = AnswerKind::kPick, .offer_index = 3})},
      JournalEntry{.step = 128,
                   .kind = JournalKind::kAnswer,
                   .value = PackAnswer({.kind = AnswerKind::kBanish, .offer_index = 1})},
      JournalEntry{.step = 1000000, .kind = JournalKind::kHeading, .value = 32},
      JournalEntry{.step = kLastStep - 1, .kind = JournalKind::kAbandon, .value = 0},
  };
  journal.final_step = kLastStep;

  const std::vector<std::uint8_t> bytes = EncodeSurvivorsJournal(journal);
  const ro::core::Result<SurvivorsJournal> decoded = DecodeSurvivorsJournal(bytes);
  REQUIRE(decoded.has_value());
  CHECK(*decoded == journal);

  // Un journal vide aussi.
  const SurvivorsJournal empty;
  const ro::core::Result<SurvivorsJournal> decoded_empty =
      DecodeSurvivorsJournal(EncodeSurvivorsJournal(empty));
  REQUIRE(decoded_empty.has_value());
  CHECK(*decoded_empty == empty);

  // Tronqué à n'importe quel octet, il manque toujours la suite.
  for (std::size_t kept = 0; kept < bytes.size(); ++kept) {
    INFO("octets gardés : " << kept);
    CHECK(DecodeFailure(std::span(bytes).first(kept)) == ErrorCode::kUnexpectedEnd);
  }
}

TEST_CASE("Journal survivors : un journal tronqué à chaque octet est refusé", "[sim][survivors]") {
  for (std::size_t kept = 0; kept < kPinnedBytes.size(); ++kept) {
    INFO("octets gardés : " << kept);
    CHECK(DecodeFailure(std::span(kPinnedBytes).first(kept)) == ErrorCode::kUnexpectedEnd);
  }
}

TEST_CASE("Journal survivors : signature, version, sorte, cap et octets en trop",
          "[sim][survivors]") {
  std::vector<std::uint8_t> bytes = kPinnedBytes;
  bytes[0] = 'X';
  CHECK(DecodeFailure(bytes) == ErrorCode::kBadSignature);

  // La version 1, celle du ruleset 2, ne se relit plus ; une version à venir
  // non plus.
  bytes = kPinnedBytes;
  bytes[4] = 1;
  CHECK(DecodeFailure(bytes) == ErrorCode::kUnsupportedVersion);
  bytes[4] = 3;
  CHECK(DecodeFailure(bytes) == ErrorCode::kUnsupportedVersion);
  bytes[4] = 0;
  CHECK(DecodeFailure(bytes) == ErrorCode::kUnsupportedVersion);
  // La version de règles n'est pas celle du format : c'est le rejeu qui la juge.
  bytes = kPinnedBytes;
  bytes[6] = 2;
  CHECK_FALSE(DecodeFailure(bytes).has_value());

  bytes = kPinnedBytes;
  bytes[kPinnedFirstKindOffset] = 3;
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes[kPinnedFirstKindOffset] = 0xFF;
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes[kPinnedFirstKindOffset] = static_cast<std::uint8_t>(JournalKind::kAbandon);
  CHECK_FALSE(DecodeFailure(bytes).has_value());

  // Un cap vaut 0 à 63, ou kNoHeading ; rien entre les deux.
  bytes = kPinnedBytes;
  bytes[kPinnedFirstValueOffset] = ro::sim::kHeadingSteps;
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes[kPinnedFirstValueOffset] = 254;
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes[kPinnedFirstValueOffset] = ro::sim::kHeadingSteps - 1;
  CHECK_FALSE(DecodeFailure(bytes).has_value());
  bytes[kPinnedFirstValueOffset] = ro::sim::kNoHeading;
  CHECK_FALSE(DecodeFailure(bytes).has_value());

  bytes = kPinnedBytes;
  bytes.push_back(0);
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
}

TEST_CASE("Journal survivors : un LEB128 trop long ou un pas qui déborde est refusé",
          "[sim][survivors]") {
  // Le nombre d'entrées sur six octets.
  std::vector<std::uint8_t> bytes = PinnedHeader();
  Append(bytes, {0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // Cinq octets, mais au-delà de 32 bits.
  bytes = PinnedHeader();
  Append(bytes, {0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // Un bonus sur six octets.
  bytes.assign(kPinnedBytes.begin(),
               kPinnedBytes.begin() + static_cast<std::ptrdiff_t>(kPinnedBonusOffset));
  Append(bytes, {0x80, 0x80, 0x80, 0x80, 0x80, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // Une valeur n'a qu'une écriture : écrite plus longue que nécessaire, elle
  // est refusée, pour le nombre d'entrées, un bonus, un écart d'entrée ou le
  // pas final.
  bytes = PinnedHeader();
  Append(bytes, {0x80, 0x00, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes = PinnedHeader();
  Append(bytes, {0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes = kPinnedBytes;
  bytes[kPinnedBonusOffset] = 0x81;
  bytes.insert(bytes.begin() + static_cast<std::ptrdiff_t>(kPinnedBonusOffset) + 1, 0x00);
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes = PinnedHeader();
  Append(bytes, {0x01, 0x85, 0x00, 0x00, 0x00, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes = PinnedHeader();
  Append(bytes, {0x00, 0x80, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  // Sous sa forme courte, le même journal se lit, et se réécrit à l'identique.
  bytes = PinnedHeader();
  Append(bytes, {0x01, 0x05, 0x00, 0x00, 0x00});
  const ro::core::Result<SurvivorsJournal> short_form = DecodeSurvivorsJournal(bytes);
  REQUIRE(short_form.has_value());
  CHECK(EncodeSurvivorsJournal(*short_form) == bytes);

  // Une entrée au dernier pas de 32 bits, puis une au-delà.
  bytes = PinnedHeader();
  Append(bytes, {0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // Le pas final au-delà de 32 bits.
  bytes = PinnedHeader();
  Append(bytes, {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x01});
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // Au dernier pas de 32 bits tout juste : valide.
  bytes = PinnedHeader();
  Append(bytes, {0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x00, 0x00, 0x00});
  const ro::core::Result<SurvivorsJournal> last = DecodeSurvivorsJournal(bytes);
  REQUIRE(last.has_value());
  REQUIRE(last->entries.size() == 1U);
  CHECK(last->entries[0].step == kLastStep);
  CHECK(last->final_step == kLastStep);

  // Un journal en désordre s'encode, mais son écart déborde au décodage.
  SurvivorsJournal disordered;
  disordered.entries = {JournalEntry{.step = 10}, JournalEntry{.step = 5}};
  disordered.final_step = 20;
  CHECK(DecodeFailure(EncodeSurvivorsJournal(disordered)) == ErrorCode::kCorruptData);
}

TEST_CASE("Journal survivors : un nombre d'entrées mensonger n'alloue rien", "[sim][survivors]") {
  // 4 294 967 295 entrées annoncées, aucune présente : la réserve suit les
  // octets restants, et le décodage s'arrête sur la troncature.
  std::vector<std::uint8_t> bytes = PinnedHeader();
  Append(bytes, {0xFF, 0xFF, 0xFF, 0xFF, 0x0F});
  CHECK(DecodeFailure(bytes) == ErrorCode::kUnexpectedEnd);
  // Suivi d'une entrée valide, puis la fin : la même troncature.
  Append(bytes, {0x05, 0x00, 0x07});
  CHECK(DecodeFailure(bytes) == ErrorCode::kUnexpectedEnd);
}

TEST_CASE("Journal survivors : un bonus de loadout hors bornes est refusé", "[sim][survivors]") {
  for (const std::int32_t bonus : {ro::sim::kMostStatBonus, -ro::sim::kMostStatBonus}) {
    SurvivorsJournal journal = PinnedJournal();
    journal.start.loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kMaxHp)] = bonus;
    const ro::core::Result<SurvivorsJournal> decoded =
        DecodeSurvivorsJournal(EncodeSurvivorsJournal(journal));
    REQUIRE(decoded.has_value());
    CHECK(*decoded == journal);
  }
  for (const std::int32_t bonus :
       {ro::sim::kMostStatBonus + 1, -ro::sim::kMostStatBonus - 1,
        std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min()}) {
    SurvivorsJournal journal = PinnedJournal();
    journal.start.loadout.bonus.values[static_cast<std::size_t>(SurvivorsStat::kArmor)] = bonus;
    CHECK(DecodeFailure(EncodeSurvivorsJournal(journal)) == ErrorCode::kCorruptData);
  }
}

TEST_CASE("Journal survivors : la Cursed Water et les Id verrouillés hors bornes sont refusés",
          "[sim][survivors]") {
  std::vector<std::uint8_t> bytes = kPinnedBytes;
  bytes[kPinnedCursedWaterOffset] = ro::sim::kMostCursedWater;
  CHECK_FALSE(DecodeFailure(bytes).has_value());
  bytes[kPinnedCursedWaterOffset] = ro::sim::kMostCursedWater + 1;
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // Deux Id égaux, puis décroissants : un verrou n'a qu'une écriture.
  bytes = kPinnedBytes;
  bytes[kPinnedSecondLockedOffset] = 0xE8;
  bytes[kPinnedSecondLockedOffset + 1] = 0x0F;
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);
  bytes[kPinnedSecondLockedOffset] = 0x01;
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // Plus de kMostLockedIds annoncés : refusé avant de lire un seul Id.
  bytes = kPinnedBytes;
  bytes[kPinnedLockedCountOffset] = static_cast<std::uint8_t>(ro::sim::kMostLockedIds + 1);
  CHECK(DecodeFailure(bytes) == ErrorCode::kCorruptData);

  // kMostLockedIds Id croissants font l'aller-retour.
  SurvivorsJournal journal = PinnedJournal();
  journal.start.locked_ids.clear();
  for (std::uint16_t id = 1; id <= ro::sim::kMostLockedIds; ++id) {
    journal.start.locked_ids.push_back(id);
  }
  const ro::core::Result<SurvivorsJournal> decoded =
      DecodeSurvivorsJournal(EncodeSurvivorsJournal(journal));
  REQUIRE(decoded.has_value());
  CHECK(*decoded == journal);
}
