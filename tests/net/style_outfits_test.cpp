// Les tenues sur le fil : la tenue en octets, les demandes que nous envoyons,
// et les réponses qu'un serveur, ou un intermédiaire hostile, pourrait forger.

#include "ro/net/style_outfits.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/sim/style.h"
#include "ro/sim/style_outfit.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyOutfitFrame;
using ro::net::DecodeOutfit;
using ro::net::EncodeOutfit;
using ro::net::EncodeOutfitRequest;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::NormalizeOutfitCode;
using ro::net::OutfitRequest;
using ro::net::ShowOutfitCode;
using ro::sim::OutfitAction;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRecipe;
using ro::sim::StyleOutfit;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::ReadU16;

namespace {

constexpr std::uint16_t kRequestOpcode = 0x0F61;
constexpr std::uint16_t kReplyOpcode = 0x0F62;
constexpr std::size_t kRequestHead = 15;
constexpr std::size_t kReplyHead = 17;
// Une entrée de calque à un seul réglage.
constexpr std::size_t kOneAdjustEntry = 10 + 5;
constexpr std::size_t kCodeAt = 6;
constexpr std::size_t kNameLengthAt = 14;
constexpr std::uint32_t kBody = 0xB0D1;
constexpr std::uint32_t kMount = 0xB0D2;
constexpr std::uint32_t kHeadKey = (12U << 16U) | 5U;
constexpr std::uint32_t kHatView = 17;
constexpr std::string_view kCode = "K7QM3XPA";

[[nodiscard]] StyleOutfit Sample() {
  StyleOutfit outfit;
  ro::sim::StyleRecipe body;
  body.body_palette = 12;
  body.ramps[0] = ro::sim::StyleAdjust{.hue = 120, .saturation = 40, .value = -5, .absolute = true};
  outfit.bodies.emplace_back(kBody, body);
  ro::sim::StyleRecipe mount;
  mount.ramps[7].hue = -30;
  outfit.bodies.emplace_back(kMount, mount);
  outfit.fallback_body = kBody;
  StyleLayerRecipe head{.layer = StyleLayerKind::kHead, .key = kHeadKey, .profile = 1};
  head.adjusts[2].hue = 90;
  StyleLayerRecipe hat{.layer = StyleLayerKind::kHeadgear, .key = kHatView, .profile = 2};
  hat.adjusts[12].saturation = 30;
  StyleLayerRecipe more{.layer = StyleLayerKind::kBodyMore, .key = kBody, .profile = 3};
  more.adjusts[3].value = 20;
  outfit.layers = {head, hat, more};
  return outfit;
}

[[nodiscard]] Frame Reply(OutfitAction action, std::uint8_t result, std::string_view code,
                          const std::vector<std::uint8_t>& payload) {
  Frame frame;
  frame.opcode = kReplyOpcode;
  PutU16(frame.bytes, kReplyOpcode);
  PutU16(frame.bytes, static_cast<std::uint16_t>(kReplyHead + payload.size()));
  frame.bytes.push_back(static_cast<std::uint8_t>(action));
  frame.bytes.push_back(result);
  PutU16(frame.bytes, 0);
  frame.bytes.push_back(0);
  std::string padded(code);
  padded.resize(ro::sim::kOutfitCodeLength, '\0');
  frame.bytes.insert(frame.bytes.end(), padded.begin(), padded.end());
  frame.bytes.insert(frame.bytes.end(), payload.begin(), payload.end());
  return frame;
}

}  // namespace

TEST_CASE("Tenue : l'aller-retour garde chaque corps et chaque calque", "[net][outfits]") {
  const auto raw = EncodeOutfit(Sample());
  REQUIRE(raw.has_value());
  // En-tête, deux corps de 50 octets, le nombre, trois entrées d'un réglage.
  CHECK(raw->size() ==
        ro::net::kOutfitHead + 2 * ro::net::kOutfitBodyBytes + 1 + 3 * kOneAdjustEntry);
  const auto back = DecodeOutfit(*raw);
  REQUIRE(back.has_value());
  REQUIRE(back->bodies.size() == 2);
  CHECK(back->bodies[0].first == kBody);
  CHECK(back->bodies[0].second.body_palette == 12);
  CHECK(back->bodies[0].second.ramps[0].absolute);
  CHECK(back->bodies[0].second.ramps[0].value == -5);
  CHECK(back->bodies[1].second.ramps[7].hue == -30);
  CHECK(back->fallback_body == kBody);
  REQUIRE(back->layers.size() == 3);
  CHECK(back->layers[0].adjusts[2].hue == 90);
  CHECK(back->layers[1].adjusts[12].saturation == 30);
  CHECK(back->layers[2].layer == StyleLayerKind::kBodyMore);
  CHECK(back->layers[2].adjusts[3].value == 20);
}

TEST_CASE("Tenue : ce qui ne part pas", "[net][outfits]") {
  SECTION("une tenue vide") { CHECK_FALSE(EncodeOutfit(StyleOutfit{}).has_value()); }
  SECTION("un repli qui n'est pas l'un des corps") {
    StyleOutfit outfit = Sample();
    outfit.fallback_body = 0xDEAD;
    CHECK_FALSE(EncodeOutfit(outfit).has_value());
  }
  SECTION("deux fois le même corps, ou une clé nulle") {
    StyleOutfit outfit = Sample();
    outfit.bodies[1].first = kBody;
    CHECK_FALSE(EncodeOutfit(outfit).has_value());
    outfit.bodies[1].first = 0;
    CHECK_FALSE(EncodeOutfit(outfit).has_value());
  }
  SECTION("un calque répété") {
    StyleOutfit outfit = Sample();
    outfit.layers.push_back(outfit.layers.front());
    CHECK_FALSE(EncodeOutfit(outfit).has_value());
  }
  SECTION("cinq corps") {
    StyleOutfit outfit = Sample();
    for (std::uint32_t key = 1; key <= 3; ++key) {
      outfit.bodies.emplace_back(key, ro::sim::StyleRecipe{});
    }
    CHECK_FALSE(EncodeOutfit(outfit).has_value());
  }
}

TEST_CASE("Tenue : une structure forgée ne se lit pas", "[net][outfits]") {
  const auto raw = EncodeOutfit(Sample());
  REQUIRE(raw.has_value());
  SECTION("une version inconnue") {
    auto forged = *raw;
    forged[0] = 2;
    CHECK_FALSE(DecodeOutfit(forged).has_value());
  }
  SECTION("une queue en trop") {
    auto forged = *raw;
    forged.push_back(0);
    CHECK_FALSE(DecodeOutfit(forged).has_value());
  }
  SECTION("une entrée tronquée") {
    auto forged = *raw;
    forged.pop_back();
    CHECK_FALSE(DecodeOutfit(forged).has_value());
  }
  SECTION("plus de corps qu'il n'y en a") {
    auto forged = *raw;
    forged[1] = 4;
    CHECK_FALSE(DecodeOutfit(forged).has_value());
  }
}

TEST_CASE("Code de tenue : collé comme on veut, envoyé tel que le serveur l'attend",
          "[net][outfits]") {
  CHECK(NormalizeOutfitCode("k7qm-3xpa") == std::string(kCode));
  CHECK(NormalizeOutfitCode(" K7QM 3XPA ") == std::string(kCode));
  // Ni zéro, ni O, ni un, ni I, ni L : on ne devine pas ce que le joueur a voulu.
  CHECK_FALSE(NormalizeOutfitCode("K7QM3XP0").has_value());
  CHECK_FALSE(NormalizeOutfitCode("K7QM3XPAB").has_value());
  CHECK_FALSE(NormalizeOutfitCode("K7QM").has_value());
  CHECK(ShowOutfitCode(kCode) == "K7QM-3XPA");
}

TEST_CASE("Demande de tenue : tout champ inutile à l'action part nul", "[net][outfits]") {
  SECTION("demander un code") {
    const auto frame = EncodeOutfitRequest(
        OutfitRequest{.action = OutfitAction::kFetch, .code = std::string(kCode)});
    REQUIRE(frame.has_value());
    CHECK(ReadU16(*frame, 0) == kRequestOpcode);
    CHECK(frame->size() == kRequestHead);
    CHECK(ReadU16(*frame, 2) == kRequestHead);
    CHECK(std::string(frame->begin() + kCodeAt, frame->begin() + kCodeAt + 8) == kCode);
    CHECK((*frame)[5] == 0);
  }
  SECTION("ranger une tenue") {
    const auto frame = EncodeOutfitRequest(OutfitRequest{.action = OutfitAction::kWardrobeStore,
                                                         .slot = 3,
                                                         .code = std::string(kCode),
                                                         .name = "Été",
                                                         .outfit = Sample()});
    REQUIRE(frame.has_value());
    CHECK((*frame)[5] == 3);
    // Le code n'a rien à faire là : il part nul.
    CHECK((*frame)[kCodeAt] == 0);
    CHECK((*frame)[kNameLengthAt] == std::string("Été").size());
    CHECK(ReadU16(*frame, 2) == frame->size());
  }
  SECTION("ce qui ne part pas") {
    CHECK_FALSE(
        EncodeOutfitRequest(OutfitRequest{.action = OutfitAction::kFetch, .code = "k7qm3xpa"})
            .has_value());
    CHECK_FALSE(
        EncodeOutfitRequest(OutfitRequest{.action = OutfitAction::kWardrobeClear, .slot = 16})
            .has_value());
    CHECK_FALSE(EncodeOutfitRequest(OutfitRequest{.action = OutfitAction::kWardrobeStore,
                                                  .name = std::string("a\nb"),
                                                  .outfit = Sample()})
                    .has_value());
    CHECK_FALSE(EncodeOutfitRequest(OutfitRequest{.action = OutfitAction::kPublish}).has_value());
  }
}

TEST_CASE("Réponse de tenue : un code reçu se range, une tenue forgée s'ignore", "[net][outfits]") {
  World world;
  const auto raw = EncodeOutfit(Sample());
  REQUIRE(raw.has_value());
  REQUIRE(ApplyOutfitFrame(Reply(OutfitAction::kFetch, 0, kCode, *raw), world) ==
          FrameOutcome::kApplied);
  const ro::sim::OutfitReply& got = world.outfits().last();
  CHECK(got.sequence == 1);
  CHECK(got.code == kCode);
  REQUIRE(got.outfit.has_value());
  CHECK(got.outfit->bodies.size() == 2);

  // Un code inconnu : une réponse sans tenue.
  REQUIRE(ApplyOutfitFrame(Reply(OutfitAction::kFetch, 10, kCode, {}), world) ==
          FrameOutcome::kApplied);
  CHECK_FALSE(world.outfits().last().accepted());
  CHECK_FALSE(world.outfits().last().outfit.has_value());

  // Une tenue tronquée ne se range pas.
  auto truncated = *raw;
  truncated.pop_back();
  CHECK(ApplyOutfitFrame(Reply(OutfitAction::kFetch, 0, kCode, truncated), world) ==
        FrameOutcome::kUndecodable);
  CHECK(world.outfits().last().sequence == 2);
}

TEST_CASE("Réponse de tenue : la liste remplace la garde-robe connue", "[net][outfits]") {
  World world;
  const auto raw = EncodeOutfit(Sample());
  REQUIRE(raw.has_value());
  std::vector<std::uint8_t> list{2};
  for (const std::uint8_t slot : {std::uint8_t{0}, std::uint8_t{5}}) {
    list.push_back(slot);
    list.push_back(3);
    list.insert(list.end(), {'A', 'b', 'c'});
    PutU16(list, static_cast<std::uint16_t>(raw->size()));
    list.insert(list.end(), raw->begin(), raw->end());
  }
  REQUIRE(ApplyOutfitFrame(Reply(OutfitAction::kWardrobeList, 0, {}, list), world) ==
          FrameOutcome::kApplied);
  REQUIRE(world.outfits().wardrobe().size() == 2);
  CHECK(world.outfits().wardrobe_sequence() == 1);
  REQUIRE(world.outfits().Slot(5) != nullptr);
  CHECK(world.outfits().Slot(5)->name == "Abc");
  CHECK(world.outfits().Slot(1) == nullptr);

  // Un rangement qui déborde de la trame : rien ne change.
  auto overflow = list;
  overflow.pop_back();
  CHECK(ApplyOutfitFrame(Reply(OutfitAction::kWardrobeList, 0, {}, overflow), world) ==
        FrameOutcome::kUndecodable);
  CHECK(world.outfits().wardrobe().size() == 2);

  // Une liste vide vide la garde-robe.
  REQUIRE(ApplyOutfitFrame(Reply(OutfitAction::kWardrobeList, 0, {}, {0}), world) ==
          FrameOutcome::kApplied);
  CHECK(world.outfits().wardrobe().empty());
}

TEST_CASE("Une tenue trop grosse perd ses retouches, puis son reste", "[net][outfits][rest]") {
  StyleOutfit outfit;
  for (std::uint32_t view = 1; view <= 6; ++view) {
    StyleLayerRecipe hat;
    hat.layer = StyleLayerKind::kHeadgear;
    hat.key = view;
    hat.profile = 1;
    hat.adjusts[0].hue = 10;
    for (ro::sim::StyleAdjust& adjust : hat.rest) {
      adjust.hue = 20;
    }
    for (std::size_t index = 1; index <= ro::sim::kStyleRetouchesMost; ++index) {
      hat.retouches.push_back(ro::sim::StyleRetouch{.index = static_cast<std::uint8_t>(index)});
    }
    outfit.layers.push_back(hat);
  }
  // Six entrées de 10 + 5 + 3 + 65 + 128 octets : la tenue déborde ; sans
  // retouches, elle tient.
  const auto raw = EncodeOutfit(outfit);
  REQUIRE(raw.has_value());
  CHECK(raw->size() <= 1024);
  const auto back = DecodeOutfit(*raw);
  REQUIRE(back.has_value());
  REQUIRE(back->layers.size() == 6);
  CHECK(back->layers[0].retouches.empty());
  CHECK(back->layers[0].rest[3].hue == 20);
  CHECK(back->layers[0].adjusts[0].hue == 10);

  // Une tenue qui tient garde tout.
  StyleOutfit small;
  small.layers.push_back(outfit.layers.front());
  const auto whole = EncodeOutfit(small);
  REQUIRE(whole.has_value());
  const auto read = DecodeOutfit(*whole);
  REQUIRE(read.has_value());
  CHECK(read->layers.front().retouches.size() == ro::sim::kStyleRetouchesMost);
}
