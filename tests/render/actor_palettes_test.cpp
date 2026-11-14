// LES PALETTES QUE PORTENT LES PIÈCES D'UN ACTEUR, chacune à la clé de ce dont
// elle dépend. Voir `ro/render/actor_palettes.h`.

#include "ro/render/actor_palettes.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/palette.h"
#include "ro/formats/spr.h"
#include "ro/render/palette_ramps.h"

#include "support/actor_client.h"
#include "support/sprite_builder.h"

using ro::formats::Palette;
using ro::formats::SprSprite;
using ro::render::ActorPalettes;
using ro::render::PaletteRecipe;
using ro::render::RampAdjust;
using ro::test_support::BodySpriteBytes;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;

namespace {

[[nodiscard]] SprSprite SpriteOf(const std::vector<std::uint8_t>& bytes) {
  auto sprite = SprSprite::Parse(bytes);
  REQUIRE(sprite.has_value());
  REQUIRE(sprite->palette().has_value());
  return std::move(*sprite);
}

// Une palette officielle qui ne peint que l'index un, en bleu : le reste est
// un trou, que la fusion remplit avec le sprite.
[[nodiscard]] Palette BlueOnFirst() {
  Palette palette;
  palette.entries[1] = {0, 0, 255, 0};
  return palette;
}

[[nodiscard]] PaletteRecipe HalfTurn() {
  PaletteRecipe recipe;
  recipe.ramps[0] = RampAdjust{.hue = 180};
  return recipe;
}

// ── L'ORDRE DES EX ÆQUO ─────────────────────────────────────────────────────

// Quarante pièces de six nuances, toutes de la même couleur, et une sur deux
// peinte deux fois plus que les autres : deux scores, vingt ex æquo à chacun.
//
// ⚠ PLUS DE TRENTE-DEUX PIÈCES, ET DEUX SCORES MÊLÉS : en deçà, ou sous un seul
// score, le tri instable des bibliothèques courantes range encore les ex æquo
// dans leur ordre, et l'essai ne mordrait pas.
constexpr int kTiedPieces = 40;
constexpr int kTiedShades = 6;
constexpr int kTiedPaintedIndices = kTiedPieces * kTiedShades;
constexpr std::size_t kPixelsPerIndex = 40;
constexpr std::size_t kTiedWidth = 80;
constexpr int kTiedBrightest = 240;
constexpr int kTiedShadeStep = 25;

[[nodiscard]] bool Favoured(int piece) { return piece % 2 == 0; }

[[nodiscard]] std::vector<std::uint8_t> TiedPiecesBytes() {
  // Chaque index une fois, puis ceux des pièces favorisées une seconde fois.
  std::vector<std::uint8_t> painted;
  for (const bool favoured_only : {false, true}) {
    for (int index = 1; index <= kTiedPaintedIndices; ++index) {
      if (favoured_only && !Favoured((index - 1) / kTiedShades)) {
        continue;
      }
      painted.insert(painted.end(), kPixelsPerIndex, static_cast<std::uint8_t>(index));
    }
  }
  REQUIRE(painted.size() % kTiedWidth == 0);
  SprBuildOptions options;
  SprIndexedFrameSpec frame;
  frame.width = static_cast<std::int32_t>(kTiedWidth);
  frame.height = static_cast<std::int32_t>(painted.size() / kTiedWidth);
  frame.indices = std::move(painted);
  options.indexed.push_back(std::move(frame));
  options.palette.push_back({0, 0, 0});
  for (int piece = 0; piece < kTiedPieces; ++piece) {
    for (int shade = 0; shade < kTiedShades; ++shade) {
      const auto level = static_cast<std::uint8_t>(kTiedBrightest - shade * kTiedShadeStep);
      options.palette.push_back({level, static_cast<std::uint8_t>(level / 6), 0});
    }
  }
  return BuildSpr(options);
}

[[nodiscard]] int FirstIndexOfPiece(int piece) { return 1 + piece * kTiedShades; }

}  // namespace

TEST_CASE("Palettes : sans palette officielle, la base est celle du sprite", "[render][palette]") {
  const SprSprite sprite = SpriteOf(BodySpriteBytes());
  ActorPalettes palettes;
  CHECK(&palettes.Base(sprite, nullptr) == &*sprite.palette());
}

TEST_CASE("Palettes : la base fusionnée se garde par sprite et palette officielle",
          "[render][palette]") {
  const SprSprite sprite = SpriteOf(BodySpriteBytes());
  const Palette blue = BlueOnFirst();
  const Palette other = BlueOnFirst();
  ActorPalettes palettes;

  const Palette& merged = palettes.Base(sprite, &blue);
  CHECK(&palettes.Base(sprite, &blue) == &merged);
  CHECK(&palettes.Base(sprite, &other) != &merged);
  // L'index peint vient de la palette officielle, le trou du sprite.
  CHECK(static_cast<int>(merged.entries[1][2]) == 255);
  CHECK(merged.entries[2] == sprite.palette()->entries[2]);
}

TEST_CASE("Palettes : une analyse par sprite et palette officielle", "[render][palette]") {
  const SprSprite sprite = SpriteOf(BodySpriteBytes());
  const Palette blue = BlueOnFirst();
  ActorPalettes palettes;

  const ActorPalettes::Analysis& native = palettes.Analyze(sprite, nullptr);
  CHECK(&palettes.Analyze(sprite, nullptr) == &native);
  CHECK(native.base == &*sprite.palette());
  REQUIRE_FALSE(native.ramps.empty());
  CHECK(native.pixels_total > 0);
  CHECK(native.pixels_covered <= native.pixels_total);

  // LA MÊME CHAÎNE, REFAITE À LA MAIN : fusion, emploi des index, détection.
  const ActorPalettes::Analysis& dyed = palettes.Analyze(sprite, &blue);
  CHECK(&dyed != &native);
  CHECK(dyed.base == &palettes.Base(sprite, &blue));
  const auto expected = ro::render::DetectRamps(*dyed.base, ro::render::CountIndexUsage(sprite));
  REQUIRE(dyed.ramps.size() == expected.size());
  for (std::size_t rank = 0; rank < expected.size(); ++rank) {
    CHECK(dyed.ramps[rank].start == expected[rank].start);
    CHECK(dyed.ramps[rank].length == expected[rank].length);
  }
}

TEST_CASE("Palettes : deux pièces ex æquo gardent l'ordre de leurs index", "[render][palette]") {
  // 🔴 LE RANG EST CE QUI VOYAGE : un client qui départagerait les ex æquo
  // autrement repeindrait une autre pièce sous le même réglage, et deux joueurs
  // se verraient dans des couleurs différentes sans que rien ne le signale.
  const SprSprite tied = SpriteOf(TiedPiecesBytes());
  ActorPalettes palettes;
  const ActorPalettes::Analysis& analysis = palettes.Analyze(tied, nullptr);
  REQUIRE(analysis.ramps.size() == ro::render::kMaxRamps);
  // Les pièces favorisées passent devant les autres, et entre elles, l'ordre de
  // leurs index décide.
  for (std::size_t rank = 0; rank < analysis.ramps.size(); ++rank) {
    // ⚠ EN ENTIERS : Catch2 imprime un `std::uint8_t` comme un caractère.
    CHECK(int{analysis.ramps[rank].start} == FirstIndexOfPiece(2 * static_cast<int>(rank)));
  }
}

TEST_CASE("Palettes : un style peint une fois par sprite et palette officielle",
          "[render][palette]") {
  const SprSprite sprite = SpriteOf(BodySpriteBytes());
  const Palette blue = BlueOnFirst();
  ActorPalettes palettes;
  const std::uint32_t style = palettes.NewStyleId();
  REQUIRE(palettes.SetRecipe(style, HalfTurn()));

  const ActorPalettes::StyledPalette* native = palettes.StyledFor(style, sprite, nullptr);
  REQUIRE(native != nullptr);
  CHECK(native->serial != 0);
  CHECK(palettes.StyledFor(style, sprite, nullptr) == native);

  const ActorPalettes::StyledPalette* dyed = palettes.StyledFor(style, sprite, &blue);
  REQUIRE(dyed != nullptr);
  CHECK(dyed != native);
  CHECK(dyed->serial != native->serial);
  // Ce que la recette peint sur la base fusionnée, et sur elle seule.
  const ActorPalettes::Analysis& analysis = palettes.Analyze(sprite, &blue);
  CHECK(dyed->palette.entries ==
        ro::render::ApplyRecipe(*analysis.base, analysis.ramps, HalfTurn()).entries);

  // Un style inconnu ne peint rien : l'acteur garde son apparence native.
  CHECK(palettes.StyledFor(style + 1, sprite, nullptr) == nullptr);
}

TEST_CASE("Palettes : un style oublié publie les séries de ses palettes", "[render][palette]") {
  const SprSprite sprite = SpriteOf(BodySpriteBytes());
  const Palette blue = BlueOnFirst();
  ActorPalettes palettes;
  const std::uint32_t kept = palettes.NewStyleId();
  const std::uint32_t gone = palettes.NewStyleId();
  REQUIRE(palettes.SetRecipe(kept, HalfTurn()));
  REQUIRE(palettes.SetRecipe(gone, HalfTurn()));
  const std::uint32_t kept_serial = palettes.StyledFor(kept, sprite, nullptr)->serial;
  const std::uint32_t native_serial = palettes.StyledFor(gone, sprite, nullptr)->serial;
  const std::uint32_t dyed_serial = palettes.StyledFor(gone, sprite, &blue)->serial;
  CHECK(palettes.TakeRetiredSerials().empty());

  palettes.Forget(gone);
  CHECK_FALSE(palettes.Knows(gone));
  std::vector<std::uint32_t> retired = palettes.TakeRetiredSerials();
  std::ranges::sort(retired);
  CHECK(retired == std::vector<std::uint32_t>{native_serial, dyed_serial});
  // Une fois rendues, elles ne reviennent pas ; l'autre style n'y figurait pas.
  CHECK(palettes.TakeRetiredSerials().empty());
  CHECK(palettes.Knows(kept));
  CHECK(palettes.StyledFor(kept, sprite, nullptr)->serial == kept_serial);
}

TEST_CASE("Palettes : une palette toute faite vaut pour tous les sprites", "[render][palette]") {
  const SprSprite sprite = SpriteOf(BodySpriteBytes());
  const Palette blue = BlueOnFirst();
  ActorPalettes palettes;
  const std::uint32_t style = palettes.NewStyleId();
  REQUIRE(palettes.SetPalette(style, BlueOnFirst()));
  // 🔴 UN IDENTIFIANT NE SE RÉÉCRIT JAMAIS, ni d'une recette ni d'une autre
  // palette.
  CHECK_FALSE(palettes.SetRecipe(style, HalfTurn()));
  CHECK_FALSE(palettes.SetPalette(style, Palette{}));

  const ActorPalettes::StyledPalette* ready = palettes.StyledFor(style, sprite, nullptr);
  REQUIRE(ready != nullptr);
  CHECK(palettes.StyledFor(style, sprite, &blue) == ready);
  CHECK(ready->palette.entries == blue.entries);

  const std::uint32_t serial = ready->serial;
  palettes.Forget(style);
  CHECK(palettes.TakeRetiredSerials() == std::vector<std::uint32_t>{serial});
  // Oubliée, elle libère son identifiant.
  CHECK(palettes.SetRecipe(style, HalfTurn()));
}

// ── LA SUITE DU CORPS ───────────────────────────────────────────────────────

TEST_CASE("Palettes : la suite du corps prolonge le classement des huit premières",
          "[render][palette]") {
  const SprSprite tied = SpriteOf(TiedPiecesBytes());
  ActorPalettes palettes;
  const ActorPalettes::Analysis& analysis = palettes.Analyze(tied, nullptr);
  REQUIRE(analysis.continuation.size() == ro::render::kBodyContinuationRamps);
  // Les rangs 9 à 12 viennent après les huit premiers, dans le même ordre.
  for (std::size_t rank = 0; rank < analysis.continuation.size(); ++rank) {
    const int expected = FirstIndexOfPiece(2 * static_cast<int>(ro::render::kMaxRamps + rank));
    CHECK(int{analysis.continuation[rank].start} == expected);
  }
  CHECK(analysis.continuation_pixels > 0);
}

TEST_CASE("Palettes : la suite du corps ne peint que ses rampes, sous la clé du corps",
          "[render][palette]") {
  constexpr std::uint32_t kBodyKey = 0xC0FFEE;
  const SprSprite tied = SpriteOf(TiedPiecesBytes());
  ActorPalettes palettes;
  ro::render::StyleSet set;
  ro::render::LayerDye more;
  more.layer = ro::render::StyleLayer::kBodyMore;
  more.key = kBodyKey;
  more.profile = ro::render::kBodyContinuationProfileNumber;
  more.adjusts[0] = RampAdjust{.hue = 180};
  set.layers.push_back(more);
  const std::uint32_t style = palettes.NewStyleId();
  REQUIRE(palettes.SetStyle(style, set));

  const ActorPalettes::Analysis& analysis = palettes.Analyze(tied, nullptr);
  const ActorPalettes::StyledPalette* painted = palettes.StyledFor(
      style, tied, nullptr,
      ActorPalettes::Piece{.layer = ro::render::StyleLayer::kBody, .key = kBodyKey});
  REQUIRE(painted != nullptr);
  const Palette& base = *analysis.base;
  // La neuvième rampe a tourné ; les huit premières, et la dixième, non.
  const ro::render::PaletteRamp& ninth = analysis.continuation.front();
  CHECK(painted->palette.entries[ninth.start] != base.entries[ninth.start]);
  for (const ro::render::PaletteRamp& ramp : analysis.ramps) {
    CHECK(painted->palette.entries[ramp.start] == base.entries[ramp.start]);
  }
  const ro::render::PaletteRamp& tenth = analysis.continuation[1];
  CHECK(painted->palette.entries[tenth.start] == base.entries[tenth.start]);

  // Un autre corps n'a pas de suite : il garde ses couleurs.
  CHECK(palettes.StyledFor(style, tied, nullptr,
                           ActorPalettes::Piece{.layer = ro::render::StyleLayer::kBody,
                                                .key = kBodyKey + 1}) == nullptr);
}

TEST_CASE("Palettes : une suite d'un profil inconnu est ignorée", "[render][palette]") {
  constexpr std::uint32_t kBodyKey = 0xC0FFEE;
  const SprSprite tied = SpriteOf(TiedPiecesBytes());
  ActorPalettes palettes;
  ro::render::StyleSet set;
  ro::render::LayerDye more;
  more.layer = ro::render::StyleLayer::kBodyMore;
  more.key = kBodyKey;
  more.profile = ro::render::kLayerProfileNumber;
  more.adjusts[0] = RampAdjust{.hue = 180};
  set.layers.push_back(more);
  const std::uint32_t style = palettes.NewStyleId();
  REQUIRE(palettes.SetStyle(style, set));
  CHECK(palettes.StyledFor(style, tied, nullptr,
                           ActorPalettes::Piece{.layer = ro::render::StyleLayer::kBody,
                                                .key = kBodyKey}) == nullptr);
}

// 🔴 L'ORDRE DU PROFIL MIXTE SUR UN CORPS : la recette v7, la suite, le reste
// hors des pièces et de la peau, la peau, puis les retouches, qui l'emportent
// sur tout.
TEST_CASE("Palettes : le reste du corps évite pièces et peau, les retouches passent en dernier",
          "[render][palette][rest]") {
  constexpr std::uint32_t kBodyKey = 0xC0FFEE;
  // Un index de la vingtième pièce, impaire donc peu peinte : hors des douze
  // que règlent la v7 et la suite, hors de la peau (129 à 135).
  constexpr std::uint8_t kRestIndex = 115;
  constexpr std::uint8_t kSkinIndex = 130;
  constexpr std::array<std::uint8_t, 3> kExact = {10, 200, 30};
  const SprSprite tied = SpriteOf(TiedPiecesBytes());
  ActorPalettes palettes;
  ro::render::StyleSet set;
  set.body = HalfTurn();
  ro::render::LayerDye more;
  more.layer = ro::render::StyleLayer::kBodyMore;
  more.key = kBodyKey;
  more.profile = ro::render::kBodyContinuationProfileNumber;
  more.rest.fill(RampAdjust{.hue = 120});
  const ActorPalettes::Analysis& analysis = palettes.Analyze(tied, nullptr);
  const std::uint8_t v7_index = analysis.ramps.front().start;
  more.retouches = {ro::render::PaletteRetouch{.index = v7_index, .rgb = kExact},
                    ro::render::PaletteRetouch{.index = kSkinIndex, .rgb = kExact}};
  set.layers.push_back(more);
  ro::render::LayerDye skin;
  skin.layer = ro::render::StyleLayer::kSkin;
  skin.adjusts[0] = RampAdjust{.hue = 200, .saturation = 50, .absolute = true};
  set.layers.push_back(skin);
  const std::uint32_t style = palettes.NewStyleId();
  REQUIRE(palettes.SetStyle(style, set));

  const ActorPalettes::StyledPalette* painted = palettes.StyledFor(
      style, tied, nullptr,
      ActorPalettes::Piece{.layer = ro::render::StyleLayer::kBody, .key = kBodyKey, .skin = true});
  REQUIRE(painted != nullptr);
  const Palette& base = *analysis.base;
  // Le reste a tourné.
  CHECK(painted->palette.entries[kRestIndex] != base.entries[kRestIndex]);
  // Les retouches l'emportent sur la recette v7 et sur la peau.
  for (const std::uint8_t index : {v7_index, kSkinIndex}) {
    CHECK(painted->palette.entries[index][0] == kExact[0]);
    CHECK(painted->palette.entries[index][1] == kExact[1]);
    CHECK(painted->palette.entries[index][2] == kExact[2]);
  }
  // Un autre index de peau a pris la peau, pas le reste.
  Palette skinned = base;
  ro::render::ApplySkin(skinned, base, ro::render::SkinRace::kHuman, skin.adjusts[0]);
  CHECK(painted->palette.entries[kSkinIndex + 1] == skinned.entries[kSkinIndex + 1]);
  // L'index zéro reste le transparent.
  CHECK(painted->palette.entries[0] == base.entries[0]);
}

TEST_CASE("Palettes : la palette toute faite reçoit la suite et le reste du corps",
          "[render][palette][rest]") {
  constexpr std::uint32_t kBodyKey = 0xC0FFEE;
  constexpr std::uint8_t kRestIndex = 115;
  const SprSprite tied = SpriteOf(TiedPiecesBytes());
  ActorPalettes palettes;
  const Palette& base = *palettes.Analyze(tied, nullptr).base;
  ro::render::StyleSet set{.body_palette = base};
  ro::render::LayerDye more;
  more.layer = ro::render::StyleLayer::kBodyMore;
  more.key = kBodyKey;
  more.profile = ro::render::kBodyContinuationProfileNumber;
  more.rest.fill(RampAdjust{.hue = 120});
  set.layers.push_back(more);
  const std::uint32_t style = palettes.NewStyleId();
  REQUIRE(palettes.SetStyle(style, std::move(set)));
  const ActorPalettes::StyledPalette* painted = palettes.StyledFor(
      style, tied, nullptr,
      ActorPalettes::Piece{.layer = ro::render::StyleLayer::kBody, .key = kBodyKey});
  REQUIRE(painted != nullptr);
  CHECK(painted->palette.entries[kRestIndex] != base.entries[kRestIndex]);
}
