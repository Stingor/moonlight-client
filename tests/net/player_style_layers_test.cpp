// Les teintures de calques, sur le fil : la trame que nous envoyons, celles que
// le serveur nous envoie, et ce qu'un serveur ou un intermédiaire hostile
// pourrait forger.
//
// Les entrées se lisent et s'écrivent à la main : le générateur ne modélise que
// les en-têtes. Une disposition fausse poserait des couleurs tirées d'octets mal
// découpés, sans rien dire.

#include "ro/net/player_style_layers.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_reader.h"
#include "ro/core/byte_writer.h"
#include "ro/net/connection.h"
#include "ro/net/server_rules.h"
#include "ro/net/ui_caps.h"
#include "ro/net/world_update.h"
#include "ro/sim/style.h"
#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyServerRulesFrame;
using ro::net::ApplyStyleLayersFrame;
using ro::net::ApplyStyleVerdictFrame;
using ro::net::EncodeStyleLayers;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::ReadStyleLayerPayload;
using ro::net::StyleLayerOrder;
using ro::net::StyleSlot;
using ro::sim::StyleLayerKind;
using ro::sim::StyleLayerRecipe;
using ro::sim::World;
using ro::test_support::PutU16;
using ro::test_support::PutU32;
using ro::test_support::ReadU16;
using ro::test_support::ReadU32;

namespace {

constexpr std::uint16_t kRequestOpcode = 0x0F5E;
constexpr std::uint16_t kLayersOpcode = 0x0F5F;
constexpr std::uint16_t kVerdictOpcode = 0x0F60;
constexpr std::uint16_t kRulesOpcode = 0x0F38;
constexpr std::size_t kRequestHead = 6;
constexpr std::size_t kLayersHead = 7;
constexpr std::size_t kEntryHead = 10;
constexpr std::size_t kAdjustBytes = 5;
constexpr std::uint8_t kZipped = 0x01;
constexpr std::uint32_t kGid = 2000123;
constexpr std::uint32_t kHeadKey = (12U << 16U) | 5U;
constexpr std::uint32_t kHatItem = 5022;
constexpr std::uint32_t kHatView = 17;

[[nodiscard]] StyleLayerRecipe Recipe(StyleLayerKind layer, std::uint32_t key,
                                      std::uint8_t profile = 1) {
  StyleLayerRecipe recipe;
  recipe.layer = layer;
  recipe.key = key;
  recipe.profile = profile;
  return recipe;
}

// Une entrée, écrite comme le serveur l'écrit : un réglage par rang posé.
struct WireEntry {
  std::uint8_t layer = 1;
  std::uint32_t key = kHeadKey;
  std::uint8_t profile = 1;
  std::uint16_t mask = 0;
  std::int16_t hue = 0;
  // Ajouté à la taille annoncée, pour forger un mensonge.
  int size_lie = 0;
  // Des octets inconnus en queue, comme un serveur plus récent.
  std::size_t tail = 0;
};

void PutEntry(std::vector<std::uint8_t>& into, const WireEntry& entry) {
  std::size_t adjusts = 0;
  for (std::uint16_t bits = entry.mask; bits != 0; bits = static_cast<std::uint16_t>(bits >> 1U)) {
    adjusts += bits & 1U;
  }
  const auto size = static_cast<int>(kEntryHead + adjusts * kAdjustBytes + entry.tail);
  into.push_back(static_cast<std::uint8_t>(size + entry.size_lie));
  into.push_back(entry.layer);
  PutU32(into, entry.key);
  into.push_back(entry.profile);
  into.push_back(0);
  PutU16(into, entry.mask);
  for (std::size_t rank = 0; rank < adjusts; ++rank) {
    PutU16(into, static_cast<std::uint16_t>(entry.hue));
    into.push_back(0);
    into.push_back(0);
    into.push_back(0);
  }
  into.insert(into.end(), entry.tail, 0xEE);
}

void PutBlock(std::vector<std::uint8_t>& into, std::uint32_t gid,
              std::span<const WireEntry> entries) {
  PutU32(into, gid);
  into.push_back(static_cast<std::uint8_t>(entries.size()));
  for (const WireEntry& entry : entries) {
    PutEntry(into, entry);
  }
}

[[nodiscard]] Frame LayersFrame(std::span<const std::uint8_t> body, std::uint8_t flags,
                                std::size_t raw_size) {
  Frame frame;
  frame.opcode = kLayersOpcode;
  PutU16(frame.bytes, kLayersOpcode);
  PutU16(frame.bytes, static_cast<std::uint16_t>(kLayersHead + body.size()));
  frame.bytes.push_back(flags);
  PutU16(frame.bytes, static_cast<std::uint16_t>(raw_size));
  frame.bytes.insert(frame.bytes.end(), body.begin(), body.end());
  return frame;
}

[[nodiscard]] Frame Plain(std::span<const std::uint8_t> raw) {
  return LayersFrame(raw, 0, raw.size());
}

// Un flux zlib en un bloc STOCKÉ : ce que `uncompress` lit comme un flux
// compressé, sans qu'il faille zlib pour l'écrire.
[[nodiscard]] std::vector<std::uint8_t> StoredZlib(std::span<const std::uint8_t> raw) {
  constexpr std::uint32_t kAdlerModulo = 65521;
  constexpr std::uint8_t kZlibHeader = 0x78;
  constexpr std::uint8_t kZlibFastest = 0x01;
  constexpr std::uint8_t kFinalStored = 0x01;
  std::vector<std::uint8_t> out{kZlibHeader, kZlibFastest, kFinalStored};
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

[[nodiscard]] Frame Verdict(std::uint8_t subject, std::uint8_t result, std::uint16_t wait_ms,
                            std::uint32_t detail) {
  Frame frame;
  frame.opcode = kVerdictOpcode;
  PutU16(frame.bytes, kVerdictOpcode);
  PutU16(frame.bytes, 12);
  frame.bytes.push_back(subject);
  frame.bytes.push_back(result);
  PutU16(frame.bytes, wait_ms);
  PutU32(frame.bytes, detail);
  return frame;
}

}  // namespace

TEST_CASE("Une validation part en une trame, ses réglages neutres omis", "[net][style_layers]") {
  StyleLayerOrder head{.recipe = Recipe(StyleLayerKind::kHead, kHeadKey)};
  head.recipe.adjusts[0].hue = 120;
  head.recipe.adjusts[3].saturation = -20;
  head.recipe.adjusts[5].absolute = true;
  StyleLayerOrder hat{.recipe = Recipe(StyleLayerKind::kHeadgear, kHatItem, 2),
                      .slot = StyleSlot::kCostumeHeadTop};
  hat.recipe.adjusts[12].value = 30;
  const std::array orders{head, hat};

  const auto frame = EncodeStyleLayers(orders, /*clear_character=*/false);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t>& bytes = *frame;
  CHECK(ReadU16(bytes, 0) == kRequestOpcode);
  CHECK(ReadU16(bytes, 2) == bytes.size());
  CHECK(bytes[4] == 0);
  CHECK(bytes[5] == 2);

  // La tête : trois réglages non neutres, aux rangs 0, 3 et 5.
  std::size_t at = kRequestHead;
  CHECK(bytes[at] == kEntryHead + 3 * kAdjustBytes);
  CHECK(bytes[at + 1] == 1);
  CHECK(ReadU32(bytes, at + 2) == kHeadKey);
  CHECK(bytes[at + 6] == 1);
  CHECK(ReadU16(bytes, at + 8) == ((1U << 0U) | (1U << 3U) | (1U << 5U)));
  CHECK(ReadU16(bytes, at + kEntryHead) == 120);
  CHECK(bytes[at + kEntryHead + 2 * kAdjustBytes + 4] == 1);

  // Le chapeau : l'EMPLACEMENT et l'objet attendu, jamais un identifiant global.
  at += bytes[at];
  CHECK(bytes[at] == kEntryHead + kAdjustBytes);
  CHECK(bytes[at + 1] == static_cast<std::uint8_t>(StyleSlot::kCostumeHeadTop));
  CHECK(ReadU32(bytes, at + 2) == kHatItem);
  CHECK(bytes[at + 6] == 2);
  CHECK(ReadU16(bytes, at + 8) == (1U << 12U));
  CHECK(at + bytes[at] == bytes.size());
}

TEST_CASE("Effacer la tête et la peau part sans entrée", "[net][style_layers]") {
  const auto frame = EncodeStyleLayers({}, /*clear_character=*/true);
  REQUIRE(frame.has_value());
  CHECK(frame->size() == kRequestHead);
  CHECK((*frame)[4] == 0x01);
  CHECK((*frame)[5] == 0);
  // Rien à faire : rien ne part.
  CHECK_FALSE(EncodeStyleLayers({}, /*clear_character=*/false).has_value());
}

TEST_CASE("Une teinture incohérente ne part pas", "[net][style_layers]") {
  const auto refused = [](const StyleLayerOrder& order) {
    return !EncodeStyleLayers(std::span(&order, 1), false).has_value();
  };
  SECTION("un objet sans emplacement") {
    CHECK(refused(StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kHeadgear, kHatItem)}));
  }
  SECTION("un emplacement qui ne porte pas ce calque") {
    CHECK(refused(StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kGarment, kHatItem),
                                  .slot = StyleSlot::kHeadTop}));
  }
  SECTION("une clé nulle, ou une peau qui en porte une") {
    CHECK(refused(StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kHead, 0)}));
    CHECK(refused(StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kSkin, 1)}));
  }
  SECTION("un rang que le profil n'emploie pas") {
    StyleLayerOrder skin{.recipe = Recipe(StyleLayerKind::kSkin, 0)};
    skin.recipe.adjusts[1].hue = 10;
    CHECK(refused(skin));
    StyleLayerOrder head{.recipe = Recipe(StyleLayerKind::kHead, kHeadKey)};
    head.recipe.adjusts[8].hue = 10;
    CHECK(refused(head));
    StyleLayerOrder classes{.recipe = Recipe(StyleLayerKind::kHead, kHeadKey, 2)};
    CHECK(refused(classes));
    // La suite du corps : quatre rangs, au profil 3 seulement.
    StyleLayerOrder more{.recipe = Recipe(StyleLayerKind::kBodyMore, kHeadKey, 3)};
    more.recipe.adjusts[4].hue = 10;
    CHECK(refused(more));
    StyleLayerOrder ramps{.recipe = Recipe(StyleLayerKind::kBodyMore, kHeadKey, 1)};
    CHECK(refused(ramps));
  }
  SECTION("un profil inconnu") {
    CHECK(refused(StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kHeadgear, kHatItem, 3),
                                  .slot = StyleSlot::kHeadTop}));
  }
  SECTION("un calque répété") {
    const std::array twice{
        StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kHeadgear, kHatItem),
                        .slot = StyleSlot::kHeadTop},
        StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kHeadgear, kHatItem + 1),
                        .slot = StyleSlot::kHeadTop}};
    CHECK_FALSE(EncodeStyleLayers(twice, false).has_value());
  }
  SECTION("une trame au-delà de cinq kilo-octets") {
    std::vector<StyleLayerOrder> many;
    for (std::uint32_t style = 1; style <= 120; ++style) {
      StyleLayerOrder order{.recipe = Recipe(StyleLayerKind::kHead, (style << 16U) | 1U)};
      for (std::size_t rank = 0; rank < 8; ++rank) {
        order.recipe.adjusts[rank].hue = 1;
      }
      many.push_back(order);
    }
    CHECK_FALSE(EncodeStyleLayers(many, false).has_value());
  }
}

TEST_CASE("Un bloc redéfinit les teintures de son joueur", "[net][style_layers]") {
  World world;
  std::vector<std::uint8_t> raw;
  const std::array entries{
      WireEntry{.layer = 1, .key = kHeadKey, .mask = 0b101, .hue = 40},
      WireEntry{.layer = 2, .key = kHatView, .profile = 2, .mask = 1U << 12U, .hue = -90},
      WireEntry{.layer = 4, .key = 0, .mask = 1, .hue = 15}};
  PutBlock(raw, kGid, entries);
  REQUIRE(ApplyStyleLayersFrame(Plain(raw), world) == FrameOutcome::kApplied);

  const std::vector<StyleLayerRecipe>* layers = world.styles().FindLayers(kGid);
  REQUIRE(layers != nullptr);
  REQUIRE(layers->size() == 3);
  CHECK((*layers)[0].layer == StyleLayerKind::kHead);
  CHECK((*layers)[0].adjusts[0].hue == 40);
  CHECK((*layers)[0].adjusts[1].hue == 0);
  CHECK((*layers)[0].adjusts[2].hue == 40);
  CHECK((*layers)[1].key == kHatView);
  CHECK((*layers)[1].adjusts[12].hue == -90);
  CHECK((*layers)[2].layer == StyleLayerKind::kSkin);

  // Un bloc vide efface.
  std::vector<std::uint8_t> empty;
  PutBlock(empty, kGid, {});
  REQUIRE(ApplyStyleLayersFrame(Plain(empty), world) == FrameOutcome::kApplied);
  CHECK(world.styles().FindLayers(kGid) == nullptr);
}

TEST_CASE("Une charge compressée se lit à la taille annoncée", "[net][style_layers]") {
  World world;
  std::vector<std::uint8_t> raw;
  PutBlock(raw, kGid, std::array{WireEntry{.mask = 1, .hue = 7}});
  PutBlock(raw, kGid + 1, std::array{WireEntry{.layer = 3, .key = 2, .mask = 3}});
  const std::vector<std::uint8_t> zipped = StoredZlib(raw);
  REQUIRE(ApplyStyleLayersFrame(LayersFrame(zipped, kZipped, raw.size()), world) ==
          FrameOutcome::kApplied);
  CHECK(world.styles().FindLayers(kGid) != nullptr);
  CHECK(world.styles().FindLayers(kGid + 1) != nullptr);
}

TEST_CASE("Une trame forgée n'applique rien", "[net][style_layers]") {
  World world;
  std::vector<std::uint8_t> good;
  PutBlock(good, kGid, std::array{WireEntry{.mask = 1, .hue = 7}});

  SECTION("une taille brute qui ment") {
    CHECK(ApplyStyleLayersFrame(LayersFrame(good, 0, good.size() + 1), world) ==
          FrameOutcome::kUndecodable);
  }
  SECTION("une bombe : le flux donne plus que la taille annoncée") {
    std::vector<std::uint8_t> big(good);
    big.insert(big.end(), 4000, 0);
    CHECK(ApplyStyleLayersFrame(LayersFrame(StoredZlib(big), kZipped, good.size()), world) ==
          FrameOutcome::kUndecodable);
  }
  SECTION("une taille brute au-delà du plafond") {
    CHECK(ApplyStyleLayersFrame(
              LayersFrame(StoredZlib(good), kZipped, ro::net::kStyleLayersRawMost + 1), world) ==
          FrameOutcome::kUndecodable);
  }
  SECTION("un flux illisible") {
    const std::vector<std::uint8_t> junk(32, 0xAB);
    CHECK(ApplyStyleLayersFrame(LayersFrame(junk, kZipped, good.size()), world) ==
          FrameOutcome::kUndecodable);
  }
  SECTION("une entrée plus courte que ses réglages") {
    std::vector<std::uint8_t> raw;
    PutBlock(raw, kGid, std::array{WireEntry{.mask = 0b111, .size_lie = -5}});
    CHECK(ApplyStyleLayersFrame(Plain(raw), world) == FrameOutcome::kUndecodable);
  }
  SECTION("une entrée qui déborde du bloc") {
    std::vector<std::uint8_t> raw;
    PutBlock(raw, kGid, std::array{WireEntry{.mask = 1, .size_lie = 40}});
    CHECK(ApplyStyleLayersFrame(Plain(raw), world) == FrameOutcome::kUndecodable);
  }
  SECTION("un bloc qui annonce plus d'entrées qu'il n'en porte") {
    std::vector<std::uint8_t> raw(good);
    raw[4] = 3;
    CHECK(ApplyStyleLayersFrame(Plain(raw), world) == FrameOutcome::kUndecodable);
  }
  SECTION("un bloc tronqué après un bon : RIEN n'est appliqué") {
    std::vector<std::uint8_t> raw(good);
    raw.push_back(0x01);
    raw.push_back(0x02);
    CHECK(ApplyStyleLayersFrame(Plain(raw), world) == FrameOutcome::kUndecodable);
  }
  SECTION("un drapeau inconnu") {
    CHECK(ApplyStyleLayersFrame(LayersFrame(good, 0x80, good.size()), world) ==
          FrameOutcome::kIgnored);
  }
  CHECK(world.styles().FindLayers(kGid) == nullptr);
}

TEST_CASE("Une entrée qu'on ne sait pas lire s'enjambe", "[net][style_layers]") {
  std::vector<std::uint8_t> raw;
  const std::array entries{// Un calque inconnu, un profil inconnu, un rang hors profil, une peau à
                           // deux réglages : enjambés.
                           WireEntry{.layer = 9, .mask = 1},
                           WireEntry{.layer = 2, .key = kHatView, .profile = 7, .mask = 1},
                           WireEntry{.layer = 1, .profile = 1, .mask = 1U << 9U},
                           WireEntry{.layer = 4, .key = 0, .mask = 0b11},
                           // Un champ en queue, ajouté par un serveur plus récent : lue.
                           WireEntry{.layer = 3, .key = 4, .mask = 1, .hue = 11, .tail = 6}};
  PutBlock(raw, kGid, entries);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE(blocks->size() == 1);
  REQUIRE((*blocks)[0].layers.size() == 1);
  CHECK((*blocks)[0].layers[0].layer == StyleLayerKind::kGarment);
  CHECK((*blocks)[0].layers[0].adjusts[0].hue == 11);
}

TEST_CASE("Au-delà de ce qu'un joueur affiche, les entrées s'ignorent", "[net][style_layers]") {
  std::vector<std::uint8_t> raw;
  std::vector<WireEntry> entries;
  for (std::uint32_t view = 1; view <= 20; ++view) {
    entries.push_back(WireEntry{.layer = 2, .key = view, .mask = 1});
  }
  PutBlock(raw, kGid, entries);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  CHECK((*blocks)[0].layers.size() == ro::sim::kStyleLayersShownMax);
}

TEST_CASE("Le verdict se range par objet, et l'atelier s'annonce", "[net][style_layers]") {
  World world;
  constexpr std::uint32_t kNow = 50000;
  constexpr std::uint32_t kEvicted = 0xCAFE;
  REQUIRE(ApplyStyleVerdictFrame(Verdict(0, 0, 0, kEvicted), world, kNow) ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyStyleVerdictFrame(Verdict(1, 6, 0, 0), world, kNow) == FrameOutcome::kApplied);
  const ro::sim::StyleVerdict& body = world.styles().LastVerdict(ro::sim::StyleSubject::kBody);
  const ro::sim::StyleVerdict& layers = world.styles().LastVerdict(ro::sim::StyleSubject::kLayers);
  CHECK(body.result == ro::sim::StyleVerdictResult::kAccepted);
  CHECK(body.evicted_body_key == kEvicted);
  CHECK(layers.result == ro::sim::StyleVerdictResult::kNoAtelier);
  CHECK(layers.sequence > body.sequence);

  // Un résultat inconnu se lit comme un refus.
  REQUIRE(ApplyStyleVerdictFrame(Verdict(1, 200, 0, 0), world, kNow) == FrameOutcome::kApplied);
  CHECK(world.styles().LastVerdict(ro::sim::StyleSubject::kLayers).result ==
        ro::sim::StyleVerdictResult::kUnavailable);

  constexpr std::uint32_t kOpenMs = 600000;
  REQUIRE(ApplyStyleVerdictFrame(Verdict(2, 1, 0, kOpenMs), world, kNow) == FrameOutcome::kApplied);
  CHECK(world.styles().atelier().OpenAt(kNow + kOpenMs - 1));
  CHECK_FALSE(world.styles().atelier().OpenAt(kNow + kOpenMs));
  REQUIRE(ApplyStyleVerdictFrame(Verdict(2, 0, 0, 0), world, kNow) == FrameOutcome::kApplied);
  CHECK_FALSE(world.styles().atelier().OpenAt(kNow));

  CHECK(ApplyStyleVerdictFrame(Verdict(9, 0, 0, 0), world, kNow) == FrameOutcome::kIgnored);
}

TEST_CASE("Les règles disent la cadence des calques et les bornes", "[net][style_layers]") {
  World world;
  Frame frame;
  frame.opcode = kRulesOpcode;
  PutU16(frame.bytes, kRulesOpcode);
  PutU16(frame.bytes, 0);
  const auto rule = [&frame](std::uint16_t key, std::uint32_t value) {
    PutU16(frame.bytes, key);
    PutU32(frame.bytes, value);
  };
  rule(11, 1000);
  rule(13, 16);
  rule(15, 80);
  rule(17, 251);
  rule(21, 5);
  frame.bytes[2] = static_cast<std::uint8_t>(frame.bytes.size());
  REQUIRE(ApplyServerRulesFrame(frame, world) == FrameOutcome::kApplied);
  CHECK(world.session().style_layers_cadence_ms == 1000);
  CHECK(world.session().style_layers_max == 16);
  CHECK(world.session().hair_style_max == 80);
  CHECK(world.session().hair_color_max == 251);
  CHECK(world.session().style_atelier_cells == 5);

  // Le serveur qui cesse d'exiger l'atelier le dit par un zéro, qui se garde.
  Frame again;
  again.opcode = kRulesOpcode;
  PutU16(again.bytes, kRulesOpcode);
  PutU16(again.bytes, 0);
  PutU16(again.bytes, 21);
  PutU32(again.bytes, 0);
  again.bytes[2] = static_cast<std::uint8_t>(again.bytes.size());
  REQUIRE(ApplyServerRulesFrame(again, world) == FrameOutcome::kApplied);
  CHECK(world.session().style_atelier_cells == 0);
}

TEST_CASE("Le bit des calques s'annonce : la table connaît leurs ZC", "[net][style_layers]") {
  CHECK(ro::net::StyleLayerCaps() == ro::net::kUiCapStyleLayers);
}

// ── LE PROFIL MIXTE : LA QUEUE D'UNE ENTRÉE ─────────────────────────────────

namespace {

[[nodiscard]] StyleLayerRecipe MixedHat() {
  StyleLayerRecipe recipe;
  recipe.layer = StyleLayerKind::kHeadgear;
  recipe.key = kHatView;
  recipe.profile = 1;
  recipe.adjusts[0].hue = 30;
  recipe.rest[2].hue = -45;
  recipe.rest[12].absolute = true;
  recipe.rest[12].hue = 200;
  recipe.retouches = {ro::sim::StyleRetouch{.index = 3, .red = 1, .green = 2, .blue = 3},
                      ro::sim::StyleRetouch{.index = 200, .red = 250, .green = 0, .blue = 250}};
  return recipe;
}

}  // namespace

TEST_CASE("Le reste et les retouches font l'aller-retour", "[net][style_layers][rest]") {
  const StyleLayerRecipe sent = MixedHat();
  std::vector<std::uint8_t> raw;
  ro::core::ByteWriter writer(raw);
  REQUIRE(ro::net::WriteShownStyleLayer(writer, sent).has_value());
  // 10 + 1 réglage, puis 3 + 2 réglages du reste + 2 retouches.
  CHECK(raw.size() == kEntryHead + kAdjustBytes + 3 + 2 * kAdjustBytes + 2 * 4);
  CHECK(raw[0] == raw.size());
  CHECK(raw[7] == 0x02);
  ro::core::ByteReader reader(raw);
  const auto read = ro::net::ReadShownStyleLayer(reader);
  REQUIRE(read.has_value());
  REQUIRE(read->has_value());
  const StyleLayerRecipe& got = **read;
  CHECK(got.adjusts[0].hue == 30);
  CHECK(got.rest[2].hue == -45);
  CHECK(got.rest[12].absolute);
  CHECK(got.rest[12].hue == 200);
  CHECK(got.retouches == sent.retouches);
}

TEST_CASE("Une teinture sans reste ni retouche garde l'entrée d'avant, à l'octet",
          "[net][style_layers][rest]") {
  StyleLayerRecipe plain = MixedHat();
  plain.rest = {};
  plain.retouches.clear();
  std::vector<std::uint8_t> raw;
  ro::core::ByteWriter writer(raw);
  REQUIRE(ro::net::WriteShownStyleLayer(writer, plain).has_value());
  CHECK(raw.size() == kEntryHead + kAdjustBytes);
  CHECK(raw[7] == 0);
}

TEST_CASE("Un lecteur qui ignore la queue lit les pièces seules", "[net][style_layers][rest]") {
  // Ce que lirait un client d'avant : l'entrée privée de son drapeau de queue
  // s'enjambe par sa taille.
  std::vector<std::uint8_t> raw;
  ro::core::ByteWriter writer(raw);
  REQUIRE(ro::net::WriteShownStyleLayer(writer, MixedHat()).has_value());
  raw[7] = 0;
  ro::core::ByteReader reader(raw);
  const auto read = ro::net::ReadShownStyleLayer(reader);
  REQUIRE(read.has_value());
  REQUIRE(read->has_value());
  CHECK((**read).adjusts[0].hue == 30);
  CHECK((**read).rest[2].hue == 0);
  CHECK((**read).retouches.empty());
  CHECK(reader.remaining() == 0);
}

TEST_CASE("Des retouches interdites ne partent pas", "[net][style_layers][rest]") {
  StyleLayerRecipe transparent = MixedHat();
  transparent.retouches.front().index = 0;
  StyleLayerRecipe magenta = MixedHat();
  magenta.retouches.back() = ro::sim::StyleRetouch{.index = 9, .red = 255, .green = 0, .blue = 255};
  StyleLayerRecipe disordered = MixedHat();
  std::swap(disordered.retouches.front(), disordered.retouches.back());
  StyleLayerRecipe crowded = MixedHat();
  crowded.retouches.clear();
  for (std::size_t index = 1; index <= ro::sim::kStyleRetouchesMost + 1; ++index) {
    crowded.retouches.push_back(ro::sim::StyleRetouch{.index = static_cast<std::uint8_t>(index)});
  }
  StyleLayerRecipe skin;
  skin.layer = StyleLayerKind::kSkin;
  skin.profile = 1;
  skin.retouches = {ro::sim::StyleRetouch{.index = 130, .red = 1}};
  StyleLayerRecipe classes = MixedHat();
  classes.profile = 2;
  for (const StyleLayerRecipe& refused :
       {transparent, magenta, disordered, crowded, skin, classes}) {
    std::vector<std::uint8_t> raw;
    ro::core::ByteWriter writer(raw);
    CHECK_FALSE(ro::net::WriteShownStyleLayer(writer, refused).has_value());
  }
  // Au profil 2, les retouches valent, sans reste.
  StyleLayerRecipe classes_touched = MixedHat();
  classes_touched.profile = 2;
  classes_touched.rest = {};
  std::vector<std::uint8_t> raw;
  ro::core::ByteWriter writer(raw);
  CHECK(ro::net::WriteShownStyleLayer(writer, classes_touched).has_value());
}

TEST_CASE("Une queue forgée s'ignore, les pièces restent", "[net][style_layers][rest]") {
  std::vector<std::uint8_t> raw;
  ro::core::ByteWriter writer(raw);
  REQUIRE(ro::net::WriteShownStyleLayer(writer, MixedHat()).has_value());
  // La dernière retouche passe à l'index zéro.
  raw[raw.size() - 4] = 0;
  ro::core::ByteReader reader(raw);
  const auto read = ro::net::ReadShownStyleLayer(reader);
  REQUIRE(read.has_value());
  REQUIRE(read->has_value());
  CHECK((**read).adjusts[0].hue == 30);
  CHECK((**read).retouches.empty());
  CHECK((**read).rest[2].hue == 0);
}

TEST_CASE("Le pire CZ des calques tient sous le plafond", "[net][style_layers][rest]") {
  StyleLayerRecipe full = MixedHat();
  for (std::size_t rank = 0; rank < 8; ++rank) {
    full.adjusts[rank].hue = 1;
  }
  for (ro::sim::StyleAdjust& adjust : full.rest) {
    adjust.hue = 1;
  }
  full.retouches.clear();
  for (std::size_t index = 1; index <= ro::sim::kStyleRetouchesMost; ++index) {
    full.retouches.push_back(ro::sim::StyleRetouch{.index = static_cast<std::uint8_t>(index)});
  }
  std::vector<StyleLayerOrder> orders;
  for (std::uint8_t slot = 10; slot <= 17; ++slot) {
    StyleLayerOrder order{.recipe = full, .slot = static_cast<StyleSlot>(slot)};
    order.recipe.layer =
        slot == 13 || slot == 17 ? StyleLayerKind::kGarment : StyleLayerKind::kHeadgear;
    order.recipe.key = kHatItem;
    orders.push_back(order);
  }
  StyleLayerOrder head{.recipe = full};
  head.recipe.layer = StyleLayerKind::kHead;
  head.recipe.key = kHeadKey;
  orders.push_back(head);
  const auto frame = EncodeStyleLayers(orders, false);
  REQUIRE(frame.has_value());
  CHECK(frame->size() <= ro::net::kStyleLayersRequestMost);
  CHECK(ro::net::kStyleLayerEntryMost == 246);
}

TEST_CASE("La règle 24 annonce le profil mixte", "[net][style_layers][rest]") {
  World world;
  CHECK(world.session().style_layer_retouches_most == 0);
  Frame frame;
  frame.opcode = kRulesOpcode;
  PutU16(frame.bytes, kRulesOpcode);
  PutU16(frame.bytes, 0);
  PutU16(frame.bytes, 24);
  PutU32(frame.bytes, 32);
  frame.bytes[2] = static_cast<std::uint8_t>(frame.bytes.size());
  REQUIRE(ApplyServerRulesFrame(frame, world) == FrameOutcome::kApplied);
  CHECK(world.session().style_layer_retouches_most == 32);
}

// ── LA PEAU DU CORPS ─────────────────────────────────────────────────────────

namespace {

constexpr std::uint32_t kBodyKey = 0x5A17C0DEU;
constexpr std::uint8_t kBodySkinWire = 6;

}  // namespace

TEST_CASE("La peau du corps part sous la clé du corps, et se relit", "[net][style_layers][skin]") {
  StyleLayerOrder face{.recipe = Recipe(StyleLayerKind::kSkin, 0)};
  face.recipe.adjusts[0].hue = 20;
  StyleLayerOrder body{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey)};
  body.recipe.adjusts[0].hue = 200;
  const std::array orders{face, body};
  const auto frame = EncodeStyleLayers(orders, /*clear_character=*/false);
  REQUIRE(frame.has_value());
  const std::vector<std::uint8_t>& bytes = *frame;
  std::size_t at = kRequestHead;
  CHECK(bytes[at + 1] == 4);
  CHECK(ReadU32(bytes, at + 2) == 0);
  at += bytes[at];
  // Un seul réglage, au calque 6, sous la clé du corps.
  CHECK(bytes[at] == kEntryHead + kAdjustBytes);
  CHECK(bytes[at + 1] == kBodySkinWire);
  CHECK(ReadU32(bytes, at + 2) == kBodyKey);
  CHECK(bytes[at + 6] == 1);
  CHECK(ReadU16(bytes, at + 8) == 1);
  CHECK(ReadU16(bytes, at + kEntryHead) == 200);
  CHECK(at + bytes[at] == bytes.size());

  // Le même octet à octet dans un bloc du serveur : il se relit tel quel.
  std::vector<std::uint8_t> raw;
  PutBlock(raw, kGid,
           std::array{WireEntry{.layer = 4, .key = 0, .mask = 1, .hue = 20},
                      WireEntry{.layer = kBodySkinWire, .key = kBodyKey, .mask = 1, .hue = 200}});
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE((*blocks)[0].layers.size() == 2);
  CHECK((*blocks)[0].layers[1].layer == StyleLayerKind::kBodySkin);
  CHECK((*blocks)[0].layers[1].key == kBodyKey);
  CHECK((*blocks)[0].layers[1].adjusts[0].hue == 200);
}

TEST_CASE("Une peau du corps incohérente ne part pas", "[net][style_layers][skin]") {
  const auto refused = [](const StyleLayerOrder& order) {
    return !EncodeStyleLayers(std::span(&order, 1), false).has_value();
  };
  // Sans clé de corps.
  CHECK(refused(StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kBodySkin, 0)}));
  // Un second réglage, un autre profil.
  StyleLayerOrder two{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey)};
  two.recipe.adjusts[1].hue = 10;
  CHECK(refused(two));
  CHECK(refused(StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey, 2)}));
  // Ni reste ni retouche.
  StyleLayerOrder rest{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey)};
  rest.recipe.rest[0].hue = 10;
  CHECK(refused(rest));
  StyleLayerOrder touched{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey)};
  touched.recipe.retouches.push_back(ro::sim::StyleRetouch{.index = 130, .red = 1});
  CHECK(refused(touched));
  // Le même corps deux fois.
  const std::array twice{StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey)},
                         StyleLayerOrder{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey)}};
  CHECK_FALSE(EncodeStyleLayers(twice, false).has_value());
}

TEST_CASE("Une peau du corps forgée s'enjambe, la bonne se lit", "[net][style_layers][skin]") {
  std::vector<std::uint8_t> raw;
  const std::array entries{
      // Un autre profil, deux réglages, un rang hors du premier : enjambés.
      WireEntry{.layer = kBodySkinWire, .key = kBodyKey, .profile = 2, .mask = 1},
      WireEntry{.layer = kBodySkinWire, .key = kBodyKey, .mask = 0b11},
      WireEntry{.layer = kBodySkinWire, .key = kBodyKey, .mask = 0b10},
      WireEntry{.layer = kBodySkinWire, .key = kBodyKey + 1, .mask = 1, .hue = 33}};
  PutBlock(raw, kGid, entries);
  const auto blocks = ReadStyleLayerPayload(raw);
  REQUIRE(blocks.has_value());
  REQUIRE((*blocks)[0].layers.size() == 1);
  CHECK((*blocks)[0].layers[0].key == kBodyKey + 1);
  CHECK((*blocks)[0].layers[0].adjusts[0].hue == 33);
}

TEST_CASE("Le pire CZ tient avec la peau des quatre corps", "[net][style_layers][skin]") {
  std::vector<StyleLayerOrder> orders;
  for (std::uint32_t body = 1; body <= ro::sim::kStyleBodyVariantsMost; ++body) {
    StyleLayerOrder order{.recipe = Recipe(StyleLayerKind::kBodySkin, kBodyKey + body)};
    order.recipe.adjusts[0].hue = 1;
    orders.push_back(order);
  }
  const auto frame = EncodeStyleLayers(orders, false);
  REQUIRE(frame.has_value());
  CHECK(frame->size() ==
        kRequestHead + ro::sim::kStyleBodyVariantsMost * (kEntryHead + kAdjustBytes));
  CHECK(ro::net::kStyleSkinEntryMost == kEntryHead + kAdjustBytes);
}

TEST_CASE("La règle 25 annonce la peau du corps", "[net][style_layers][skin]") {
  World world;
  CHECK(world.session().style_layer_last == 0);
  Frame frame;
  frame.opcode = kRulesOpcode;
  PutU16(frame.bytes, kRulesOpcode);
  PutU16(frame.bytes, 0);
  PutU16(frame.bytes, 25);
  PutU32(frame.bytes, kBodySkinWire);
  frame.bytes[2] = static_cast<std::uint8_t>(frame.bytes.size());
  REQUIRE(ApplyServerRulesFrame(frame, world) == FrameOutcome::kApplied);
  CHECK(world.session().style_layer_last == kBodySkinWire);
}
