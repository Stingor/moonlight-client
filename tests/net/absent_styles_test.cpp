// Les styles hors de vue, sur le fil : la demande que nous envoyons, la réponse
// du serveur, et ce qu'un serveur ou un intermédiaire hostile pourrait forger.
//
// Les blocs se lisent et s'écrivent à la main : le générateur ne modélise que
// les en-têtes.

#include "ro/net/absent_styles.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/server_rules.h"
#include "ro/net/world_update.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyAbsentStylesFrame;
using ro::net::ApplyServerRulesFrame;
using ro::net::EncodeAbsentStylesRequest;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::ReadAbsentStylePayload;
using ro::sim::AbsentStyle;
using ro::sim::StyleLayerKind;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kRequestOpcode = 0x0F63;
constexpr std::uint16_t kAnswerOpcode = 0x0F64;
constexpr std::uint16_t kRulesOpcode = 0x0F38;
constexpr std::size_t kRequestHead = 5;
constexpr std::size_t kAnswerHead = 10;
constexpr std::uint8_t kZipped = 0x01;
constexpr std::uint8_t kTooSoon = 2;
constexpr std::uint32_t kCharId = 150123;
constexpr std::uint32_t kOtherCharId = 150456;
constexpr std::uint16_t kJob = 4008;
constexpr std::uint16_t kHair = 12;
constexpr std::uint16_t kHairColor = 5;
constexpr std::uint32_t kHeadKey = (12U << 16U) | 5U;
constexpr std::uint32_t kBodyKey = 0xC0FFEE01;
constexpr std::uint32_t kMountKey = 0xC0FFEE02;
constexpr std::uint8_t kWireVersion = 7;
constexpr std::uint8_t kFallbackFlag = 0x04;
constexpr std::size_t kRampCount = 8;

// Une variante v7 sans gid : 52 octets.
void PutBody(std::vector<std::uint8_t>& into, std::uint32_t body_key, std::uint8_t flags,
             std::int16_t first_hue, std::uint8_t version = kWireVersion) {
  into.push_back(version);
  into.push_back(flags);
  PutU32(into, body_key);
  PutU16(into, 42);      // palette de vêtement
  PutU16(into, 0xFFFF);  // palette de cheveux : celle du personnage
  PutU16(into, 0xFFFF);  // coupe : celle du personnage
  for (std::size_t ramp = 0; ramp < kRampCount; ++ramp) {
    PutU16(into, static_cast<std::uint16_t>(ramp == 0 ? first_hue : 0));
    into.push_back(0);
    into.push_back(0);
    into.push_back(0);
  }
}

// Une entrée de tête au seul rang 0, à la forme du 0x0F5F.
void PutHeadEntry(std::vector<std::uint8_t>& into, std::int16_t hue) {
  constexpr std::uint8_t kSize = 10 + 5;
  into.push_back(kSize);
  into.push_back(1);  // tête
  PutU32(into, kHeadKey);
  into.push_back(1);  // profil des rampes
  into.push_back(0);
  PutU16(into, 1);
  PutU16(into, static_cast<std::uint16_t>(hue));
  into.push_back(0);
  into.push_back(0);
  into.push_back(0);
}

void PutShownBlock(std::vector<std::uint8_t>& into, std::uint32_t char_id) {
  PutU32(into, char_id);
  into.push_back(1);
  PutU16(into, kJob);
  into.push_back(1);
  PutU16(into, kHair);
  PutU16(into, kHairColor);
  into.push_back(2);
  PutBody(into, kBodyKey, kFallbackFlag, 30);
  PutBody(into, kMountKey, 0, -60);
  into.push_back(1);
  PutHeadEntry(into, 90);
}

[[nodiscard]] Frame Answer(std::span<const std::uint8_t> body, std::uint8_t flags,
                           std::size_t raw_size, std::uint8_t result = 0,
                           std::uint16_t wait_ms = 0) {
  Frame frame;
  frame.opcode = kAnswerOpcode;
  PutU16(frame.bytes, kAnswerOpcode);
  PutU16(frame.bytes, static_cast<std::uint16_t>(kAnswerHead + body.size()));
  frame.bytes.push_back(result);
  PutU16(frame.bytes, wait_ms);
  frame.bytes.push_back(flags);
  PutU16(frame.bytes, static_cast<std::uint16_t>(raw_size));
  frame.bytes.insert(frame.bytes.end(), body.begin(), body.end());
  return frame;
}

[[nodiscard]] Frame Plain(std::span<const std::uint8_t> raw) { return Answer(raw, 0, raw.size()); }

// Un flux zlib en un bloc STOCKÉ, que `uncompress` lit sans qu'il faille zlib
// pour l'écrire.
[[nodiscard]] std::vector<std::uint8_t> StoredZlib(std::span<const std::uint8_t> raw) {
  constexpr std::uint32_t kAdlerModulo = 65521;
  std::vector<std::uint8_t> out{0x78, 0x01, 0x01};
  const auto length = static_cast<std::uint16_t>(raw.size());
  PutU16(out, length);
  PutU16(out, static_cast<std::uint16_t>(~length));
  out.insert(out.end(), raw.begin(), raw.end());
  std::uint32_t low = 1;
  std::uint32_t high = 0;
  for (const std::uint8_t byte : raw) {
    low = (low + byte) % kAdlerModulo;
    high = (high + low) % kAdlerModulo;
  }
  const std::uint32_t adler = (high << 16U) | low;
  for (int shift = 24; shift >= 0; shift -= 8) {
    out.push_back(static_cast<std::uint8_t>(adler >> static_cast<unsigned>(shift)));
  }
  return out;
}

}  // namespace

TEST_CASE("La demande hors de vue porte ses char_id, à la longueur exacte",
          "[net][absent_styles]") {
  const std::array ids{kCharId, kOtherCharId};
  const auto frame = EncodeAbsentStylesRequest(ids);
  REQUIRE(frame.has_value());
  REQUIRE(frame->size() == kRequestHead + 2 * 4);
  CHECK(ReadU16(*frame, 0) == kRequestOpcode);
  CHECK(ReadU16(*frame, 2) == frame->size());
  CHECK((*frame)[4] == 2);
  CHECK(ReadU32(*frame, kRequestHead) == kCharId);
  CHECK(ReadU32(*frame, kRequestHead + 4) == kOtherCharId);
}

TEST_CASE("Une demande que le serveur refuserait ne part pas", "[net][absent_styles]") {
  CHECK_FALSE(EncodeAbsentStylesRequest({}).has_value());
  const std::array with_zero{kCharId, 0U};
  CHECK_FALSE(EncodeAbsentStylesRequest(with_zero).has_value());
  const std::array repeated{kCharId, kOtherCharId, kCharId};
  CHECK_FALSE(EncodeAbsentStylesRequest(repeated).has_value());
  std::vector<std::uint32_t> too_many;
  for (std::uint32_t at = 1; at <= ro::net::kAbsentStyleIdsMost + 1; ++at) {
    too_many.push_back(at);
  }
  CHECK_FALSE(EncodeAbsentStylesRequest(too_many).has_value());
  too_many.pop_back();
  CHECK(EncodeAbsentStylesRequest(too_many).has_value());
}

TEST_CASE("Un bloc montrable se range sous le char_id, avec sa révision", "[net][absent_styles]") {
  World world;
  std::vector<std::uint8_t> raw;
  PutShownBlock(raw, kCharId);
  PutU32(raw, kOtherCharId);
  raw.push_back(0);
  constexpr std::uint32_t kNowMs = 12345;
  REQUIRE(ApplyAbsentStylesFrame(Plain(raw), world, kNowMs) == FrameOutcome::kApplied);

  const AbsentStyle* style = world.styles().FindAbsent(kCharId);
  REQUIRE(style != nullptr);
  CHECK(style->shown);
  CHECK(style->look.job == kJob);
  CHECK(style->look.sex == 1);
  CHECK(style->look.hair == kHair);
  CHECK(style->look.hair_color == kHairColor);
  CHECK(style->heard_at_ms == kNowMs);
  REQUIRE(style->variants.by_body.size() == 2);
  CHECK(style->variants.fallback_body == kBodyKey);
  CHECK(style->variants.For(kBodyKey)->ramps[0].hue == 30);
  CHECK(style->variants.For(kMountKey)->ramps[0].hue == -60);
  REQUIRE(style->layers.size() == 1);
  CHECK(style->layers[0].layer == StyleLayerKind::kHead);
  CHECK(style->layers[0].key == kHeadKey);
  CHECK(style->layers[0].adjusts[0].hue == 90);
  const std::uint32_t first_revision = style->revision;
  CHECK(first_revision != 0);

  // « Rien à montrer » se garde aussi : on ne redemande pas à chaque image.
  const AbsentStyle* nothing = world.styles().FindAbsent(kOtherCharId);
  REQUIRE(nothing != nullptr);
  CHECK_FALSE(nothing->shown);
  CHECK(nothing->layers.empty());

  // Une nouvelle réponse remplace, sous une révision neuve.
  std::vector<std::uint8_t> again;
  PutU32(again, kCharId);
  again.push_back(0);
  REQUIRE(ApplyAbsentStylesFrame(Plain(again), world, kNowMs + 1) == FrameOutcome::kApplied);
  CHECK_FALSE(world.styles().FindAbsent(kCharId)->shown);
  CHECK(world.styles().FindAbsent(kCharId)->revision > first_revision);
}

TEST_CASE("Une réponse compressée se lit à la taille annoncée", "[net][absent_styles]") {
  World world;
  std::vector<std::uint8_t> raw;
  PutShownBlock(raw, kCharId);
  const std::vector<std::uint8_t> zipped = StoredZlib(raw);
  REQUIRE(ApplyAbsentStylesFrame(Answer(zipped, kZipped, raw.size()), world, 0) ==
          FrameOutcome::kApplied);
  CHECK(world.styles().FindAbsent(kCharId) != nullptr);
  // La même, annoncée plus grosse qu'elle ne l'est : rien.
  World other;
  CHECK(ApplyAbsentStylesFrame(Answer(zipped, kZipped, raw.size() + 1), other, 0) ==
        FrameOutcome::kUndecodable);
  CHECK(other.styles().FindAbsent(kCharId) == nullptr);
}

TEST_CASE("Une réponse forgée n'applique rien", "[net][absent_styles]") {
  std::vector<std::uint8_t> good;
  PutShownBlock(good, kCharId);

  SECTION("un bloc tronqué") {
    World world;
    std::vector<std::uint8_t> cut = good;
    cut.pop_back();
    CHECK(ApplyAbsentStylesFrame(Plain(cut), world, 0) == FrameOutcome::kUndecodable);
    CHECK(world.styles().FindAbsent(kCharId) == nullptr);
  }
  SECTION("un état inconnu, même derrière un bon bloc") {
    World world;
    std::vector<std::uint8_t> forged = good;
    PutU32(forged, kOtherCharId);
    forged.push_back(7);
    CHECK(ApplyAbsentStylesFrame(Plain(forged), world, 0) == FrameOutcome::kUndecodable);
    CHECK(world.styles().FindAbsent(kCharId) == nullptr);
  }
  SECTION("plus de corps annoncés que la charge n'en porte") {
    World world;
    std::vector<std::uint8_t> forged;
    PutU32(forged, kCharId);
    forged.push_back(1);
    PutU16(forged, kJob);
    forged.push_back(1);
    PutU16(forged, kHair);
    PutU16(forged, kHairColor);
    forged.push_back(200);
    PutBody(forged, kBodyKey, 0, 10);
    forged.push_back(0);
    CHECK(ApplyAbsentStylesFrame(Plain(forged), world, 0) == FrameOutcome::kUndecodable);
  }
  SECTION("une taille brute qui ment") {
    World world;
    CHECK(ApplyAbsentStylesFrame(Answer(good, 0, good.size() - 1), world, 0) ==
          FrameOutcome::kUndecodable);
  }
  SECTION("un drapeau inconnu") {
    World world;
    CHECK(ApplyAbsentStylesFrame(Answer(good, 0x02, good.size()), world, 0) ==
          FrameOutcome::kIgnored);
    CHECK(world.styles().FindAbsent(kCharId) == nullptr);
  }
}

TEST_CASE("Une variante d'une autre version s'enjambe, la bonne se lit", "[net][absent_styles]") {
  std::vector<std::uint8_t> raw;
  PutU32(raw, kCharId);
  raw.push_back(1);
  PutU16(raw, kJob);
  raw.push_back(0);
  PutU16(raw, kHair);
  PutU16(raw, kHairColor);
  raw.push_back(2);
  PutBody(raw, kMountKey, kFallbackFlag, 11, 6);
  PutBody(raw, kBodyKey, 0, 22);
  raw.push_back(0);
  const auto blocks = ReadAbsentStylePayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE(blocks->size() == 1);
  const AbsentStyle& style = (*blocks)[0].style;
  REQUIRE(style.variants.by_body.size() == 1);
  CHECK(style.variants.fallback_body == kBodyKey);
  CHECK(style.variants.For(kBodyKey)->ramps[0].hue == 22);
}

TEST_CASE("Un refus se range au livre, avec son attente", "[net][absent_styles]") {
  World world;
  CHECK(world.styles().absent_refusal().sequence == 0);
  REQUIRE(ApplyAbsentStylesFrame(Answer({}, 0, 0, kTooSoon, 700), world, 500) ==
          FrameOutcome::kApplied);
  const ro::sim::AbsentRefusal& refusal = world.styles().absent_refusal();
  CHECK(refusal.sequence == 1);
  CHECK(refusal.result == kTooSoon);
  CHECK(refusal.wait_ms == 700);
  CHECK(refusal.heard_at_ms == 500);
  CHECK(world.styles().absent().empty());
}

TEST_CASE("Le livre garde au plus 256 personnages hors de vue, le plus ancien part",
          "[net][absent_styles]") {
  ro::sim::StyleBook book;
  for (std::uint32_t char_id = 1; char_id <= ro::sim::kAbsentStylesMost; ++char_id) {
    book.RememberAbsent(char_id, AbsentStyle{});
  }
  // Le premier, redit, devient le plus récent.
  book.RememberAbsent(1, AbsentStyle{});
  book.RememberAbsent(ro::sim::kAbsentStylesMost + 1, AbsentStyle{});
  CHECK(book.absent().size() == ro::sim::kAbsentStylesMost);
  CHECK(book.FindAbsent(1) != nullptr);
  CHECK(book.FindAbsent(2) == nullptr);
  CHECK(book.FindAbsent(ro::sim::kAbsentStylesMost + 1) != nullptr);
  // Les départs de la vue ne touchent pas ce qui vient d'ailleurs.
  book.ForgetDepartedExcept(0);
  CHECK(book.absent().size() == ro::sim::kAbsentStylesMost);
}

TEST_CASE("Les règles 26 et 27 annoncent les styles hors de vue", "[net][absent_styles]") {
  World world;
  CHECK(world.session().style_absent_cadence_ms == 0);
  Frame frame;
  frame.opcode = kRulesOpcode;
  PutU16(frame.bytes, kRulesOpcode);
  PutU16(frame.bytes, 0);
  PutU16(frame.bytes, 26);
  PutU32(frame.bytes, 1000);
  PutU16(frame.bytes, 27);
  PutU32(frame.bytes, 16);
  frame.bytes[2] = static_cast<std::uint8_t>(frame.bytes.size());
  REQUIRE(ApplyServerRulesFrame(frame, world) == FrameOutcome::kApplied);
  CHECK(world.session().style_absent_cadence_ms == 1000);
  CHECK(world.session().style_absent_ids_most == 16);
}
