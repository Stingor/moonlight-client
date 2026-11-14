#include "ro/render/screen_overlay.h"

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/gat.h"
#include "ro/formats/spr.h"
#include "ro/render/cursor.h"

#include "support/sprite_builder.h"

using ro::formats::ActAnimation;
using ro::formats::SprSprite;
using ro::render::ComposeScreenSprite;
using ro::render::CursorAction;
using ro::render::CursorShape;
using ro::render::ScreenQuad;
using ro::test_support::ActActionSpec;
using ro::test_support::ActBuildOptions;
using ro::test_support::ActFrameSpec;
using ro::test_support::ActLayerSpec;
using ro::test_support::BuildAct;
using ro::test_support::BuildSpr;
using ro::test_support::SprBuildOptions;
using ro::test_support::SprIndexedFrameSpec;
using ro::test_support::SpriteColor;

namespace {

constexpr std::int32_t kWidth = 8;
constexpr std::int32_t kHeight = 10;

SprSprite OneImage() {
  SprBuildOptions options;
  options.indexed.push_back(SprIndexedFrameSpec{
      kWidth, kHeight, std::vector<std::uint8_t>(static_cast<std::size_t>(kWidth * kHeight), 1)});
  options.palette = {SpriteColor{}, SpriteColor{.red = 200, .green = 200, .blue = 200}};
  auto sprite = SprSprite::Parse(BuildSpr(options));
  REQUIRE(sprite.has_value());
  return std::move(*sprite);
}

// Deux actions d'une image : ce qu'il faut pour vérifier qu'on lit la bonne.
ActAnimation TwoActions() {
  ActActionSpec first;
  first.frames.push_back(ActFrameSpec{.layers = {ActLayerSpec{}}});
  ActActionSpec second;
  second.frames.push_back(ActFrameSpec{.layers = {ActLayerSpec{}}});
  ActBuildOptions options;
  options.actions = {first, second};
  options.speeds = {4.0F, 4.0F};
  auto animation = ActAnimation::Parse(BuildAct(options));
  REQUIRE(animation.has_value());
  return std::move(*animation);
}

}  // namespace

TEST_CASE("Superposition : un sprite se pose sur un point de l'écran", "[render][overlay]") {
  // L'ordonnée d'un sprite descend, comme celle d'une fenêtre : il n'y a rien à
  // retourner, et le point donné est celui où va l'ORIGINE du sprite.
  const SprSprite sprite = OneImage();
  const ActAnimation animation = TwoActions();

  const std::vector<ScreenQuad> quads =
      ComposeScreenSprite(sprite, animation, 0, 0, glm::vec2{100.0F, 50.0F});
  REQUIRE(quads.size() == 1);
  // L'image est centrée sur l'origine : quatre pixels à gauche, cinq en haut.
  CHECK(quads[0].min.x == Catch::Approx(100.0F - 4.0F));
  CHECK(quads[0].min.y == Catch::Approx(50.0F - 5.0F));
  CHECK(quads[0].max.x - quads[0].min.x == Catch::Approx(static_cast<float>(kWidth)));
  CHECK(quads[0].max.y - quads[0].min.y == Catch::Approx(static_cast<float>(kHeight)));
  CHECK(quads[0].source.sprite == &sprite);
}

TEST_CASE("Superposition : l'échelle grandit autour du point donné", "[render][overlay]") {
  // Elle multiplie les écarts au point d'ancrage, jamais le point lui-même :
  // un curseur doublé garde sa pointe sous la souris.
  const SprSprite sprite = OneImage();
  const ActAnimation animation = TwoActions();

  const std::vector<ScreenQuad> quads =
      ComposeScreenSprite(sprite, animation, 0, 0, glm::vec2{200.0F, 300.0F}, 2.0F);
  REQUIRE(quads.size() == 1);
  CHECK(quads[0].min.x == Catch::Approx(200.0F - 8.0F));
  CHECK(quads[0].min.y == Catch::Approx(300.0F - 10.0F));
  CHECK(quads[0].max.x - quads[0].min.x == Catch::Approx(2.0F * static_cast<float>(kWidth)));
}

TEST_CASE("Superposition : ce qui n'existe pas ne se dessine pas", "[render][overlay]") {
  const SprSprite sprite = OneImage();
  const ActAnimation animation = TwoActions();

  // Une action au-delà de ce que le fichier porte, et une échelle nulle.
  CHECK(ComposeScreenSprite(sprite, animation, 9, 0, glm::vec2{0.0F, 0.0F}).empty());
  CHECK(ComposeScreenSprite(sprite, animation, 0, 0, glm::vec2{0.0F, 0.0F}, 0.0F).empty());
}

TEST_CASE("Curseur : chaque forme nomme son action", "[render][overlay]") {
  // Les numéros sont ceux du fichier du jeu, relus image par image : la flèche
  // en tête, puis la bulle, la main qui pointe, l'épée, la porte, la main qui
  // saisit, l'anneau au sol.
  CHECK(CursorAction(CursorShape::kArrow) == 0);
  CHECK(CursorAction(CursorShape::kTalk) == 1);
  CHECK(CursorAction(CursorShape::kPoint) == 2);
  CHECK(CursorAction(CursorShape::kAttack) == 5);
  CHECK(CursorAction(CursorShape::kWarp) == 7);
  CHECK(CursorAction(CursorShape::kPickUp) == 9);
  CHECK(CursorAction(CursorShape::kGroundTarget) == 10);
  CHECK(CursorAction(CursorShape::kTargetLocked) == 11);
  CHECK(CursorAction(CursorShape::kForbidden) == 13);

  // Dix et onze sont DEUX actions, et le fichier y répète le même dessin : les
  // confondre en une seule perdrait la cadence, qui est tout ce qui les sépare.
  CHECK(CursorAction(CursorShape::kTargetLocked) != CursorAction(CursorShape::kGroundTarget));
}

TEST_CASE("Curseur : deux types de cellule seulement sont refusés", "[render][overlay]") {
  using ro::formats::kGatWaterFlag;
  using ro::render::CellRefusesWalk;

  // Le client compare le type à UN et à CINQ, et à rien d'autre. Les autres
  // types infranchissables — l'eau profonde, type deux — gardent la flèche, et
  // une marchabilité générale les refuserait à tort.
  CHECK(CellRefusesWalk(1));
  CHECK(CellRefusesWalk(5));
  CHECK_FALSE(CellRefusesWalk(0));
  CHECK_FALSE(CellRefusesWalk(2));
  CHECK_FALSE(CellRefusesWalk(3));
  CHECK_FALSE(CellRefusesWalk(4));
  CHECK_FALSE(CellRefusesWalk(6));

  // Le drapeau d'eau ne change pas un mur en passage : sans le retirer, un mur
  // écrit 0x80000001 — un demi-million de cellules sur les cartes réelles —
  // passerait pour franchissable.
  CHECK(CellRefusesWalk(static_cast<std::int32_t>(kGatWaterFlag | 1U)));
  CHECK(CellRefusesWalk(static_cast<std::int32_t>(kGatWaterFlag | 5U)));
  CHECK_FALSE(CellRefusesWalk(static_cast<std::int32_t>(kGatWaterFlag)));
}

TEST_CASE("Curseur : la cadence vient du client, pas du fichier", "[render][overlay]") {
  // Le client demande son délai au fichier, puis n'en fait rien :
  // il compte des tics de vingt-quatre millisecondes et divise par un facteur
  // écrit en dur. Suivre le fichier fait tourner la flèche deux fois trop vite.
  using ro::render::CursorMotionOf;
  using ro::render::CursorShape;

  CHECK(CursorMotionOf(CursorShape::kArrow).ticks_per_frame == 4.0F);
  CHECK(CursorMotionOf(CursorShape::kAttack).ticks_per_frame == 20.0F);
  CHECK(CursorMotionOf(CursorShape::kWarp).ticks_per_frame == 10.0F);
  CHECK(CursorMotionOf(CursorShape::kGroundTarget).ticks_per_frame == 6.0F);
  // L'anneau d'une cible atteignable tourne DEUX FOIS plus vite que celui qui
  // cherche encore : c'est le seul signe qui les distingue à l'écran.
  CHECK(CursorMotionOf(CursorShape::kTargetLocked).ticks_per_frame == 3.0F);
  CHECK_FALSE(CursorMotionOf(CursorShape::kTargetLocked).plays_once);
  CHECK(CursorMotionOf(CursorShape::kPickUp).ticks_per_frame == 1.0F);
  CHECK(CursorMotionOf(CursorShape::kForbidden).ticks_per_frame == 2.0F);

  // La flèche et l'épée rebouclent ; la bulle et les deux mains ne jouent
  // qu'une fois, et deux d'entre elles attendent le bouton.
  CHECK_FALSE(CursorMotionOf(CursorShape::kArrow).plays_once);
  CHECK_FALSE(CursorMotionOf(CursorShape::kAttack).plays_once);
  CHECK(CursorMotionOf(CursorShape::kTalk).plays_once);
  CHECK(CursorMotionOf(CursorShape::kPickUp).plays_once);
  CHECK(CursorMotionOf(CursorShape::kPickUp).while_pressed);
  CHECK(CursorMotionOf(CursorShape::kPoint).while_pressed);
  CHECK_FALSE(CursorMotionOf(CursorShape::kTalk).while_pressed);

  // Et chaque forme garde l'action qu'elle nomme.
  CHECK(CursorMotionOf(CursorShape::kWarp).action == 7);
}

TEST_CASE("Cadre d'écran : les traits restent DEDANS et ne se recouvrent pas",
          "[render][overlay]") {
  // Un cadre qui déborderait mentirait sur ce qu'il entoure : deux cadres
  // emboîtés — une emprise et la boîte de clic qui la contient — ne se
  // liraient plus l'un dans l'autre.
  const std::vector<ro::render::ScreenQuad> frame = ro::render::ComposeScreenOutline(
      glm::vec2{10.0F, 20.0F}, glm::vec2{110.0F, 80.0F}, 2.0F, {255, 0, 0, 255});
  REQUIRE(frame.size() == 4);
  for (const ro::render::ScreenQuad& quad : frame) {
    CHECK(quad.min.x >= 10.0F);
    CHECK(quad.min.y >= 20.0F);
    CHECK(quad.max.x <= 110.0F);
    CHECK(quad.max.y <= 80.0F);
    // Un quad UNI : rien ne le texture, c'est sa teinte qui le peint.
    CHECK(quad.atlas == nullptr);
  }
  // Les montants s'arrêtent au ras des traverses : superposés, les coins
  // porteraient deux fois l'opacité et paraîtraient soulignés.
  const float area = [&frame] {
    float total = 0.0F;
    for (const ro::render::ScreenQuad& quad : frame) {
      total += (quad.max.x - quad.min.x) * (quad.max.y - quad.min.y);
    }
    return total;
  }();
  // 100 × 60 moins le trou de 96 × 56.
  CHECK(area == Catch::Approx(100.0F * 60.0F - 96.0F * 56.0F));
}

TEST_CASE("Cadre d'écran : un trait plus épais que la boîte ne la remplit pas",
          "[render][overlay]") {
  // Les deux bords opposés se croiseraient, et le cadre deviendrait un aplat
  // qui masquerait ce qu'il est censé cerner.
  const std::vector<ro::render::ScreenQuad> frame = ro::render::ComposeScreenOutline(
      glm::vec2{0.0F, 0.0F}, glm::vec2{6.0F, 4.0F}, 50.0F, {255, 255, 255, 255});
  REQUIRE(frame.size() == 4);
  for (const ro::render::ScreenQuad& quad : frame) {
    CHECK(quad.max.x <= 6.0F);
    CHECK(quad.max.y <= 4.0F);
    CHECK(quad.min.x >= 0.0F);
    CHECK(quad.min.y >= 0.0F);
  }
  // Un rectangle sans surface ne rend aucun trait, plutôt qu'un trait à
  // l'envers.
  CHECK(ro::render::ComposeScreenOutline(glm::vec2{5.0F, 5.0F}, glm::vec2{5.0F, 9.0F}, 1.0F,
                                         {255, 255, 255, 255})
            .empty());
}

TEST_CASE("Rectangle arrondi : il tient dans sa boîte et ses coins sont creux", "[render][ecran]") {
  // 🔴 LA COUCHE D'ÉCRAN NE SAIT PEINDRE QUE DES RECTANGLES. L'arrondi se
  // fait en bandes d'un pixel, et la seule chose qui compte est qu'aucune bande
  // ne sorte de la boîte : une bulle qui déborde de son cadre se voit tout de
  // suite, et personne ne saurait d'où vient le pixel de trop.
  constexpr float kRadius = 6.0F;
  const glm::vec2 top_left(100.0F, 50.0F);
  const glm::vec2 bottom_right(300.0F, 90.0F);
  const std::array<std::uint8_t, 4> tint = {10, 20, 30, 200};
  const std::vector<ro::render::ScreenQuad> quads =
      ro::render::ComposeRoundedRect(top_left, bottom_right, kRadius, tint);

  REQUIRE_FALSE(quads.empty());
  bool touches_top = false;
  bool touches_bottom = false;
  for (const ro::render::ScreenQuad& quad : quads) {
    CHECK(quad.min.x >= top_left.x);
    CHECK(quad.min.y >= top_left.y);
    CHECK(quad.max.x <= bottom_right.x);
    CHECK(quad.max.y <= bottom_right.y);
    CHECK(quad.tint == tint);
    touches_top = touches_top || quad.min.y == top_left.y;
    touches_bottom = touches_bottom || quad.max.y == bottom_right.y;
  }
  // Les deux bords horizontaux sont bien atteints : sans eux la boîte serait
  // simplement plus petite, pas arrondie.
  CHECK(touches_top);
  CHECK(touches_bottom);

  // ET LE COIN EST CREUX : rien ne couvre le pixel du coin exact.
  const auto covers = [&quads](float x, float y) {
    for (const ro::render::ScreenQuad& quad : quads) {
      if (x >= quad.min.x && x < quad.max.x && y >= quad.min.y && y < quad.max.y) {
        return true;
      }
    }
    return false;
  };
  CHECK_FALSE(covers(top_left.x + 0.5F, top_left.y + 0.5F));
  CHECK_FALSE(covers(bottom_right.x - 0.5F, bottom_right.y - 0.5F));
  // Le milieu, lui, est plein.
  CHECK(covers((top_left.x + bottom_right.x) * 0.5F, (top_left.y + bottom_right.y) * 0.5F));
}

TEST_CASE("Rectangle arrondi : un rayon déraisonnable se ramène", "[render][ecran]") {
  // ⚠ UN COIN NE PEUT PAS MORDRE LE COIN D'EN FACE. Sans ce bornage, un rayon
  // poussé au maximum sur une bulle d'une ligne rendrait des bandes de largeur
  // négative — donc rien du tout, et la bulle disparaîtrait.
  const glm::vec2 top_left(0.0F, 0.0F);
  const glm::vec2 bottom_right(40.0F, 10.0F);
  const std::array<std::uint8_t, 4> tint = {255, 255, 255, 255};
  const std::vector<ro::render::ScreenQuad> huge =
      ro::render::ComposeRoundedRect(top_left, bottom_right, 500.0F, tint);
  REQUIRE_FALSE(huge.empty());
  for (const ro::render::ScreenQuad& quad : huge) {
    CHECK(quad.max.x > quad.min.x);
    CHECK(quad.max.y > quad.min.y);
  }
  // Un rectangle plat ou inversé ne rend rien plutôt qu'un quad à l'envers.
  CHECK(ro::render::ComposeRoundedRect(bottom_right, top_left, 2.0F, tint).empty());
}
