// LE SCORE DU CHAMP DE BATAILLE ET LA MARQUE DES ENNEMIS : où se posent les
// emblèmes et les chiffres, et ce qui ne se dessine pas.
//
// ⚠ AUCUN ASSET DU JEU : la fonte est bâtie en mémoire. Son action N porte
// l'image N + 1, pour qu'un chiffre lu par l'index d'image au lieu de l'action
// se voie.

#include "ro/render/battle_score_hud.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/battlefield_art.h"
#include "ro/vfs/encoding.h"
#include "ro/vfs/vfs.h"

#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/sprite_builder.h"
#include "support/temp_directory.h"

namespace {

using Catch::Approx;
using ro::render::BattlefieldArt;
using ro::render::BattleScoreFigures;
using ro::render::BattleScoreHighlight;
using ro::render::ScreenQuad;
using namespace ro::test_support;

constexpr std::size_t kGlyphs = 11;
constexpr std::int32_t kGlyphSide = 4;
constexpr glm::vec2 kViewport{1280.0F, 960.0F};

[[nodiscard]] std::size_t ImageOfDigit(std::size_t digit) { return (digit + 1) % kGlyphs; }

struct Font {
  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
};

[[nodiscard]] Font MakeFont() {
  SprBuildOptions spr;
  for (std::size_t image = 0; image < kGlyphs; ++image) {
    spr.indexed.push_back(SprIndexedFrameSpec{
        kGlyphSide, kGlyphSide,
        std::vector<std::uint8_t>(static_cast<std::size_t>(kGlyphSide * kGlyphSide), 1)});
  }
  spr.palette = {SpriteColor{}, SpriteColor{.red = 255, .green = 255, .blue = 255}};
  auto sprite = ro::formats::SprSprite::Parse(BuildSpr(spr));
  REQUIRE(sprite.has_value());

  ActBuildOptions act;
  for (std::size_t digit = 0; digit < kGlyphs; ++digit) {
    ActLayerSpec layer;
    layer.frame_index = static_cast<std::int32_t>(ImageOfDigit(digit));
    act.actions.push_back(ActActionSpec{.frames = {ActFrameSpec{.layers = {layer}}}});
  }
  auto animation = ro::formats::ActAnimation::Parse(BuildAct(act));
  REQUIRE(animation.has_value());
  return Font{std::move(*sprite), std::move(*animation)};
}

[[nodiscard]] ro::formats::Image SolidImage(std::int32_t side) {
  ro::formats::Image image;
  image.width = side;
  image.height = side;
  image.pixels.assign(static_cast<std::size_t>(side * side * 4), 255);
  return image;
}

// Le chemin tel que L'ARCHIVE le range : en CP949, et non dans l'UTF-8 de nos
// sources.
[[nodiscard]] std::string ArchivePathOf(std::string_view utf8) {
  auto cp949 = ro::vfs::Utf8ToCp949(utf8);
  REQUIRE(cp949.has_value());
  return *cp949;
}

[[nodiscard]] glm::vec2 CenterOf(const ScreenQuad& quad) { return (quad.min + quad.max) / 2.0F; }

// Les quads de chiffres seulement : ceux qui désignent une image de la fonte.
[[nodiscard]] std::vector<ScreenQuad> DigitQuads(const std::vector<ScreenQuad>& quads) {
  std::vector<ScreenQuad> digits;
  for (const ScreenQuad& quad : quads) {
    if (quad.source.sprite != nullptr) digits.push_back(quad);
  }
  return digits;
}

// Le chiffre posé à ce point, ou -1.
[[nodiscard]] std::int32_t ImageAt(const std::vector<ScreenQuad>& quads, glm::vec2 center) {
  for (const ScreenQuad& quad : DigitQuads(quads)) {
    const glm::vec2 at = CenterOf(quad);
    if (at.x == Approx(center.x).margin(1.0) && at.y == Approx(center.y).margin(1.0)) {
      return quad.source.image_index;
    }
  }
  return -1;
}

}  // namespace

TEST_CASE("Score du champ : les unités s'écartent quand un score a deux chiffres",
          "[render][battleground]") {
  STATIC_REQUIRE(ro::render::kBattleUnitsWideFromRightPx < ro::render::kBattleUnitsFromRightPx);
}

TEST_CASE("Score du champ : rien tant qu'un score est inconnu", "[render][battleground]") {
  const Font font = MakeFont();
  const BattlefieldArt art;
  CHECK(ComposeBattleScore(font.sprite, font.animation, art,
                           BattleScoreFigures{.lion = -1, .eagle = 3}, kViewport)
            .empty());
  CHECK(ComposeBattleScore(font.sprite, font.animation, art,
                           BattleScoreFigures{.lion = 2, .eagle = -1}, kViewport)
            .empty());
}

TEST_CASE("Score du champ : un chiffre par score, à la colonne étroite", "[render][battleground]") {
  const Font font = MakeFont();
  const BattlefieldArt art;
  const auto quads = ComposeBattleScore(font.sprite, font.animation, art,
                                        BattleScoreFigures{.lion = 3, .eagle = 7}, kViewport);
  REQUIRE(DigitQuads(quads).size() == 2);
  const float units_x = kViewport.x - ro::render::kBattleUnitsFromRightPx;
  CHECK(ImageAt(quads, {units_x, ro::render::kLionDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(3)));
  CHECK(ImageAt(quads, {units_x, ro::render::kEagleDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(7)));
}

TEST_CASE("Score du champ : deux chiffres poussent les deux colonnes d'unités",
          "[render][battleground]") {
  const Font font = MakeFont();
  const BattlefieldArt art;
  const auto quads = ComposeBattleScore(font.sprite, font.animation, art,
                                        BattleScoreFigures{.lion = 12, .eagle = 4}, kViewport);
  REQUIRE(DigitQuads(quads).size() == 3);
  const float units_x = kViewport.x - ro::render::kBattleUnitsWideFromRightPx;
  const float tens_x = units_x - ro::render::kBattleDigitPitchPx;
  CHECK(ImageAt(quads, {units_x, ro::render::kLionDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(2)));
  CHECK(ImageAt(quads, {tens_x, ro::render::kLionDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(1)));
  // L'aigle n'a qu'un chiffre, mais il suit la colonne large : pas de zéro de tête.
  CHECK(ImageAt(quads, {units_x, ro::render::kEagleDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(4)));
  CHECK(ImageAt(quads, {tens_x, ro::render::kEagleDigitsTopPx}) == -1);
}

TEST_CASE("Score du champ : au-delà de 99, les deux derniers chiffres", "[render][battleground]") {
  const Font font = MakeFont();
  const BattlefieldArt art;
  const auto quads = ComposeBattleScore(font.sprite, font.animation, art,
                                        BattleScoreFigures{.lion = 153, .eagle = 105}, kViewport);
  REQUIRE(DigitQuads(quads).size() == 4);
  const float units_x = kViewport.x - ro::render::kBattleUnitsWideFromRightPx;
  const float tens_x = units_x - ro::render::kBattleDigitPitchPx;
  CHECK(ImageAt(quads, {units_x, ro::render::kLionDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(3)));
  CHECK(ImageAt(quads, {tens_x, ro::render::kLionDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(5)));
  // 105 se lit « 05 » : le zéro des dizaines se dessine.
  CHECK(ImageAt(quads, {units_x, ro::render::kEagleDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(5)));
  CHECK(ImageAt(quads, {tens_x, ro::render::kEagleDigitsTopPx}) ==
        static_cast<std::int32_t>(ImageOfDigit(0)));
}

TEST_CASE("Score du champ : les deux emblèmes dans leur rectangle", "[render][battleground]") {
  const Font font = MakeFont();
  BattlefieldArt art;
  art.lion = SolidImage(2);
  art.eagle = SolidImage(2);
  const auto quads = ComposeBattleScore(font.sprite, font.animation, art,
                                        BattleScoreFigures{.lion = 0, .eagle = 0}, kViewport);
  const ScreenQuad* lion = nullptr;
  const ScreenQuad* eagle = nullptr;
  for (const ScreenQuad& quad : quads) {
    if (quad.atlas == &art.lion) lion = &quad;
    if (quad.atlas == &art.eagle) eagle = &quad;
  }
  REQUIRE(lion != nullptr);
  REQUIRE(eagle != nullptr);
  CHECK(lion->min.x == Approx(kViewport.x - ro::render::kBattleEmblemLeftFromRightPx));
  CHECK(lion->max.x == Approx(kViewport.x - ro::render::kBattleEmblemRightFromRightPx));
  CHECK(lion->min.y == Approx(ro::render::kLionEmblemTopPx));
  CHECK(lion->max.y == Approx(ro::render::kLionEmblemBottomPx));
  CHECK(eagle->min.y == Approx(ro::render::kEagleEmblemTopPx));
  CHECK(eagle->max.y == Approx(ro::render::kEagleEmblemBottomPx));
}

TEST_CASE("Score du champ : le camp du joueur est surligné, derrière sa rangée",
          "[render][battleground]") {
  const Font font = MakeFont();
  BattlefieldArt art;
  art.lion = SolidImage(2);
  art.eagle = SolidImage(2);
  const BattleScoreFigures plain{.lion = 5, .eagle = 6};
  const auto bare = ComposeBattleScore(font.sprite, font.animation, art, plain, kViewport);

  BattleScoreFigures eagle_side = plain;
  eagle_side.highlight = BattleScoreHighlight::kEagle;
  const auto lit = ComposeBattleScore(font.sprite, font.animation, art, eagle_side, kViewport);
  REQUIRE(lit.size() > bare.size());
  const std::size_t added = lit.size() - bare.size();
  // Le cartouche passe en tête : des quads unis, qui couvrent la rangée de
  // l'aigle et laissent celle du lion.
  float top = lit.front().min.y;
  float bottom = lit.front().max.y;
  for (std::size_t index = 0; index < added; ++index) {
    CHECK(lit[index].atlas == nullptr);
    CHECK(lit[index].source.sprite == nullptr);
    top = std::min(top, lit[index].min.y);
    bottom = std::max(bottom, lit[index].max.y);
  }
  CHECK(top < ro::render::kEagleEmblemTopPx);
  CHECK(bottom > ro::render::kEagleEmblemBottomPx);
  CHECK(top > ro::render::kLionEmblemBottomPx);
}

TEST_CASE("Marque d'ennemi : coin haut-gauche aux pieds, décalé selon l'écran et la hauteur",
          "[render][battleground]") {
  const ro::formats::Image marker = SolidImage(24);
  const glm::vec2 feet{500.0F, 400.0F};
  const ScreenQuad quad = ro::render::ComposeEnemyMarker(marker, feet, kViewport, 1.0F);
  CHECK(quad.atlas == &marker);
  // −5/640 de 1280, −100/480 de 960.
  CHECK(quad.min.x == Approx(490.0F));
  CHECK(quad.min.y == Approx(200.0F));
  CHECK(quad.max.x - quad.min.x == Approx(24.0F));
  CHECK(quad.max.y - quad.min.y == Approx(24.0F));

  const ScreenQuad taller = ro::render::ComposeEnemyMarker(marker, feet, kViewport, 2.0F);
  CHECK(taller.min.x == Approx(480.0F));
  CHECK(taller.min.y == Approx(0.0F));
  CHECK(taller.max.x - taller.min.x == Approx(24.0F));
}

TEST_CASE("Marque d'ennemi : la fraction d'écran et le coin se tronquent au pixel",
          "[render][battleground]") {
  // 5 × 1366 / 640 = 10,67 et 100 × 768 / 480 = 160 : la largeur perd sa part
  // décimale.
  constexpr glm::vec2 kWideViewport{1366.0F, 768.0F};
  const glm::vec2 shift = ro::render::EnemyMarkerShiftPx(kWideViewport);
  CHECK(shift.x == -10.0F);
  CHECK(shift.y == -160.0F);

  const ro::formats::Image marker = SolidImage(24);
  // Les pieds se tronquent d'abord, puis le coin mis à l'échelle.
  const ScreenQuad quad =
      ro::render::ComposeEnemyMarker(marker, {500.7F, 400.9F}, kWideViewport, 1.5F);
  CHECK(quad.min.x == 485.0F);
  CHECK(quad.min.y == 160.0F);
  const ScreenQuad odd =
      ro::render::ComposeEnemyMarker(marker, {500.0F, 400.0F}, kWideViewport, 1.25F);
  CHECK(odd.min.x == 487.0F);
  CHECK(odd.min.y == 200.0F);
}

TEST_CASE("Score : les emblèmes à fond blanc se posent un peu transparents",
          "[render][battleground]") {
  ro::render::BattlefieldArt art;
  art.lion = SolidImage(36);
  art.eagle = SolidImage(36);
  const Font font = MakeFont();
  const auto quads = ro::render::ComposeBattleScore(font.sprite, font.animation, art,
                                                    {.lion = 1, .eagle = 2}, kViewport);
  std::size_t emblems = 0;
  for (const ScreenQuad& quad : quads) {
    if (quad.atlas != &art.lion && quad.atlas != &art.eagle) continue;
    ++emblems;
    CHECK(quad.tint[3] == ro::render::kBattleEmblemAlpha);
  }
  CHECK(emblems == 2);
}

TEST_CASE("Marque d'ennemi : sans image, un quad d'aire nulle", "[render][battleground]") {
  const ro::formats::Image missing;
  const ScreenQuad quad =
      ro::render::ComposeEnemyMarker(missing, {500.0F, 400.0F}, kViewport, 1.0F);
  CHECK(quad.atlas == nullptr);
  CHECK(quad.min == quad.max);
}

TEST_CASE("Images du champ : lues par leur chemin coréen, le magenta retiré",
          "[render][battleground]") {
  constexpr BmpPixel kMagenta{.red = 255, .green = 0, .blue = 255};
  constexpr BmpPixel kRed{.red = 200, .green = 20, .blue = 20};
  const TempDirectory client;
  client.WriteFile("data.grf", BuildGrf({
                                   GrfFileSpec{ArchivePathOf(ro::render::kEnemyMarkerPath),
                                               BuildBmp(2, 1, {kRed, kMagenta})},
                                   GrfFileSpec{ArchivePathOf(ro::render::kEagleEmblemPath),
                                               BuildBmp(3, 3, std::vector<BmpPixel>(9, kRed))},
                               }));
  auto vfs = ro::vfs::Vfs::Mount({.root = client.path(), .archives = {"data.grf"}});
  REQUIRE(vfs.has_value());
  const BattlefieldArt art = ro::render::LoadBattlefieldArt(*vfs);

  REQUIRE_FALSE(art.enemy_marker.empty());
  CHECK(art.enemy_marker.width == 2);
  CHECK(art.enemy_marker.pixels[3] == 255);
  CHECK(art.enemy_marker.pixels[7] == 0);
  CHECK(art.eagle.width == 3);
  // Le lion manque : son image est vide, et rien d'autre ne casse.
  CHECK(art.lion.empty());
}
