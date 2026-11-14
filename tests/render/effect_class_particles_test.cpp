// LES CLASSES À PARTICULES PROPRES : les météores des Summoners, la brume de
// Gensou, les pétales de Flower Leaf, et le givre plein écran d'une gelure.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_class.h"

using Catch::Approx;

namespace {

constexpr std::int32_t kSpriteFrames = 20;
constexpr float kViewportWidth = 800.0F;
constexpr float kViewportHeight = 600.0F;
constexpr std::int32_t kInverseSourceAlphaBlend = 6;
constexpr std::int32_t kOneBlend = 2;
constexpr std::string_view kCarrotSprite = "이팩트\\su_lunaticcarrotbeat\\su_lunaticcarrotbeat";
constexpr std::string_view kCatnipSprite = "이팩트\\su_cn_meteor\\su_cn_meteor";

ro::render::EffectClassBirth Birth(std::uint32_t effect_id, std::int32_t level = -1) {
  ro::render::EffectClassBirth birth;
  birth.effect_id = effect_id;
  birth.position = {10.0F, -3.0F, 20.0F};
  birth.level = level;
  return birth;
}

// Un tic où tout sprite a vingt images, vu de face, sur un écran de 800 × 600.
struct Ticking {
  ro::render::EffectClassView view;
  ro::render::EffectClassTick tick;
  Ticking() {
    view.viewport_width = kViewportWidth;
    view.viewport_height = kViewportHeight;
    tick.view = &view;
    tick.owner_position = glm::vec3{10.0F, -3.0F, 20.0F};
    tick.sprite_frame_count = [](std::string_view, std::int32_t) { return kSpriteFrames; };
  }
};

std::unique_ptr<ro::render::EffectClassInstance> Spawn(std::uint32_t effect_id,
                                                       std::int32_t level = -1) {
  const ro::render::EffectClassView view;
  auto instance = ro::render::SpawnEffectClass(Birth(effect_id, level), view);
  REQUIRE(instance != nullptr);
  return instance;
}

// Avance `ticks` tics, puis peint ; les évènements du DERNIER tic sont gardés.
ro::render::EffectClassOutput AdvanceAndDraw(ro::render::EffectClassInstance& instance,
                                             Ticking& ticking, std::int32_t ticks = 1) {
  ro::render::EffectClassOutput last_events;
  for (std::int32_t tick_index = 0; tick_index < ticks; ++tick_index) {
    ro::render::EffectClassOutput events;
    instance.Advance(ticking.tick, events);
    last_events = events;
  }
  ro::render::EffectClassOutput drawn;
  instance.Draw(ticking.view, drawn);
  drawn.quake_ms = last_events.quake_ms;
  return drawn;
}

glm::vec3 Centroid(const ro::render::EffectWorldQuad& quad) {
  glm::vec3 sum{0.0F};
  for (const glm::vec3& corner : quad.corners) {
    sum += corner;
  }
  return sum / static_cast<float>(quad.corners.size());
}

}  // namespace

TEST_CASE("Classe à particules : les classes tenues pour muettes sont portées",
          "[render][effect_class]") {
  for (const std::uint32_t id : {1099U, 1106U, 1111U, 1112U, 1005U, 1013U, 2272U}) {
    CHECK(ro::render::IsPortedEffectClass(id));
  }
  CHECK(ro::render::EffectClassReceivesSkillLevel(1099));
  CHECK(ro::render::EffectClassReceivesSkillLevel(1111));
  CHECK_FALSE(ro::render::EffectClassReceivesSkillLevel(1106));
  CHECK(ro::render::EffectClassOwnPlayerOnly(2272));
  CHECK_FALSE(ro::render::EffectClassOwnPlayerOnly(1005));
}

TEST_CASE(
    "Météore : Lunatic Carrot Beat sème ses carottes selon le niveau, autour du point de chute",
    "[render][effect_class]") {
  Ticking ticking;
  auto carrots = Spawn(1099, 3);
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*carrots, ticking);
  REQUIRE(drawn.sprites.size() == 5);
  for (const ro::render::EffectClassSpriteDraw& sprite : drawn.sprites) {
    CHECK(sprite.sprite == kCarrotSprite);
    CHECK(sprite.tint[3] == 100);
    CHECK_FALSE(sprite.additive);
    CHECK(sprite.keeps_layer_color);
    // Vingt-cinq unités d'étendue pour cinq carottes, centrées sur la chute.
    CHECK(std::abs(sprite.ground_position.x - 10.0F) <= 12.5F);
    CHECK(std::abs(sprite.ground_position.z + 20.0F) <= 12.5F);
    CHECK(sprite.ground_position.y == Approx(3.0F));
  }
  CHECK(AdvanceAndDraw(*Spawn(1099, 1), ticking).sprites.size() == 3);
  CHECK(AdvanceAndDraw(*Spawn(1099, 5), ticking).sprites.size() == 7);
  // Un niveau au-delà du cinquième, ou une naissance sans niveau, en vaut sept.
  CHECK(AdvanceAndDraw(*Spawn(1111, 9), ticking).sprites.size() == 7);
  CHECK(AdvanceAndDraw(*Spawn(1111, -1), ticking).sprites.size() == 7);
  // La variante claire se peint pleine.
  CHECK(AdvanceAndDraw(*Spawn(1111, 2), ticking).sprites.front().tint[3] == 255);
  // Un niveau nul ne sème rien.
  CHECK_FALSE(Spawn(1099, 0)->Alive(ticking.tick));
}

TEST_CASE(
    "Météore : Catnip Meteor tombe seul, secoue l'écran à son impact et s'en va sa chute finie",
    "[render][effect_class]") {
  Ticking ticking;
  auto meteor = Spawn(1112);
  ro::render::EffectClassOutput drawn = AdvanceAndDraw(*meteor, ticking);
  REQUIRE(drawn.sprites.size() == 1);
  CHECK(drawn.sprites.front().sprite == kCatnipSprite);
  CHECK(drawn.sprites.front().tint[3] == 255);
  CHECK(drawn.sprites.front().ground_position.x == Approx(10.0F));
  CHECK(drawn.quake_ms == 0);
  // Une image tous les quatre tics, la première changée au quatrième : la
  // douzième, celle de l'impact, arrive au quarante-neuvième tic et secoue
  // l'écran.
  drawn = AdvanceAndDraw(*meteor, ticking, 47);
  CHECK(drawn.quake_ms == 0);
  CHECK(drawn.sprites.front().frame == 11);
  drawn = AdvanceAndDraw(*meteor, ticking);
  CHECK(drawn.sprites.front().frame == 12);
  CHECK(drawn.quake_ms > 0);
  // Vingt images jouées une fois : passé la dernière, plus rien, puis la mort.
  static_cast<void>(AdvanceAndDraw(*meteor, ticking, 40));
  CHECK(AdvanceAndDraw(*meteor, ticking).sprites.empty());
  CHECK_FALSE(meteor->Alive(ticking.tick));
}

TEST_CASE("Brume de Gensou : vingt nappes qui s'éclaircissent, huit unités au-dessus du porteur",
          "[render][effect_class]") {
  Ticking ticking;
  auto fog = Spawn(1005);
  ro::render::EffectClassOutput drawn = AdvanceAndDraw(*fog, ticking);
  REQUIRE(drawn.quads.size() == 20);
  for (const ro::render::EffectClassQuadDraw& quad : drawn.quads) {
    CHECK(quad.texture == "effect\\fog1.tga");
    CHECK(quad.quad.blend_destination == kInverseSourceAlphaBlend);
    // Un cran d'alpha par tic, dès le premier.
    CHECK(quad.quad.tint[3] == 1);
    CHECK(Centroid(quad.quad).y == Approx(3.0F + 8.0F).margin(0.01F));
  }
  // Deux cents crans, puis l'alpha se tient.
  drawn = AdvanceAndDraw(*fog, ticking, 300);
  CHECK(drawn.quads.front().quad.tint[3] == 201);
  ticking.tick.owner_present = false;
  CHECK_FALSE(fog->Alive(ticking.tick));
}

TEST_CASE("Sphères élémentaires : deux orbes par sphère, à l'image de leur élément",
          "[render][effect_class]") {
  Ticking ticking;
  auto water = Spawn(737, 3);
  ro::render::EffectClassOutput drawn = AdvanceAndDraw(*water, ticking);
  REQUIRE(drawn.quads.size() == 6);
  for (std::size_t rank = 0; rank < drawn.quads.size(); ++rank) {
    const ro::render::EffectClassQuadDraw& quad = drawn.quads[rank];
    CHECK(quad.texture == "effect\\WaterOrb.bmp");
    CHECK(quad.quad.tint[3] == 1);
    // La première orbe d'un groupe se mélange par son alpha, la seconde
    // s'ajoute.
    CHECK(quad.quad.blend_destination == (rank % 2 == 0 ? kInverseSourceAlphaBlend : kOneBlend));
    // Douze unités au-dessus du porteur, à huit unités de lui, à un
    // balancement près.
    CHECK(Centroid(quad.quad).y == Approx(3.0F + 12.0F).margin(1.5F));
  }
  CHECK(AdvanceAndDraw(*Spawn(735, 1), ticking).quads.front().texture == "effect\\FireOrb.bmp");
  CHECK(AdvanceAndDraw(*Spawn(736, 1), ticking).quads.front().texture ==
        "effect\\LightningOrb.bmp");
  CHECK(AdvanceAndDraw(*Spawn(738, 1), ticking).quads.front().texture == "effect\\StoneOrb.bmp");
  // Aucune sphère, aucune orbe.
  CHECK(AdvanceAndDraw(*Spawn(737, 0), ticking).quads.empty());
}

TEST_CASE("Sphère du sort : une orbe bleue et une jaune par sphère", "[render][effect_class]") {
  Ticking ticking;
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*Spawn(228, 2), ticking);
  REQUIRE(drawn.quads.size() == 4);
  for (std::size_t rank = 0; rank < drawn.quads.size(); ++rank) {
    const ro::render::EffectClassQuadDraw& quad = drawn.quads[rank];
    CHECK(quad.texture == "effect\\thunder_center.bmp");
    if (rank % 2 == 0) {
      CHECK(quad.quad.tint[0] == 0);
      CHECK(quad.quad.tint[1] == 0);
      CHECK(quad.quad.tint[2] == 255);
    } else {
      CHECK(quad.quad.tint[0] == 255);
      CHECK(quad.quad.tint[1] == 255);
      CHECK(quad.quad.tint[2] == 0);
    }
  }
}

TEST_CASE("Pétales de Flower Leaf : ils sortent de terre pleins, peints deux fois en addition",
          "[render][effect_class]") {
  Ticking ticking;
  auto petals = Spawn(1013);
  // Sous terre, rien ne se voit.
  CHECK(AdvanceAndDraw(*petals, ticking).quads.empty());
  bool seen = false;
  for (std::int32_t tick_index = 0; tick_index < 700; ++tick_index) {
    const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*petals, ticking);
    CHECK(drawn.quads.size() % 2 == 0);
    for (const ro::render::EffectClassQuadDraw& quad : drawn.quads) {
      seen = true;
      CHECK(quad.texture == "effect\\cherry_circle.bmp");
      CHECK(quad.quad.blend_destination == kOneBlend);
      CHECK(quad.quad.tint[3] > 0);
      // Jamais plus haut que trente unités : au-delà, le pétale retourne sous terre.
      CHECK(Centroid(quad.quad).y <= 3.0F + 30.0F + 0.6F);
    }
  }
  CHECK(seen);
  ticking.tick.owner_present = false;
  CHECK_FALSE(petals->Alive(ticking.tick));
}

TEST_CASE("Givre plein écran : quatre quadrants qui reprennent l'image en miroir",
          "[render][effect_class]") {
  Ticking ticking;
  auto frost = Spawn(2272);
  CHECK(frost->reused_on_same_owner());
  const ro::render::EffectClassOutput drawn = AdvanceAndDraw(*frost, ticking);
  REQUIRE(drawn.screen_quads.size() == 4);
  for (const ro::render::EffectClassScreenDraw& screen : drawn.screen_quads) {
    CHECK(screen.texture ==
          "effect\\state_effect\\frostbite\\frostbite_screen\\frostbite_screen.tga");
  }
  // Le quadrant haut-gauche : l'image lue de droite à gauche.
  const ro::render::EffectScreenQuad& top_left = drawn.screen_quads[0].quad;
  CHECK(top_left.corners[0] == glm::vec2(0.0F, 0.0F));
  CHECK(top_left.corners[1] == glm::vec2(0.0F, 300.0F));
  CHECK(top_left.corners[2] == glm::vec2(400.0F, 300.0F));
  CHECK(top_left.corners[3] == glm::vec2(400.0F, 0.0F));
  CHECK(top_left.texture_uv[0] == glm::vec2(1.0F, 0.0F));
  CHECK(top_left.texture_uv[2] == glm::vec2(0.0F, 1.0F));
  // Le quadrant haut-droit : l'image telle quelle.
  const ro::render::EffectScreenQuad& top_right = drawn.screen_quads[1].quad;
  CHECK(top_right.corners[0] == glm::vec2(400.0F, 0.0F));
  CHECK(top_right.texture_uv[0] == glm::vec2(0.0F, 0.0F));
  CHECK(top_right.texture_uv[2] == glm::vec2(1.0F, 1.0F));
  // Le quadrant bas-droit : l'image lue de bas en haut.
  const ro::render::EffectScreenQuad& bottom_right = drawn.screen_quads[3].quad;
  CHECK(bottom_right.corners[2] == glm::vec2(800.0F, 600.0F));
  CHECK(bottom_right.texture_uv[0] == glm::vec2(0.0F, 1.0F));
  CHECK(bottom_right.texture_uv[2] == glm::vec2(1.0F, 0.0F));
  // Sans écran connu, rien ne se peint.
  ro::render::EffectClassView blind;
  ro::render::EffectClassOutput nothing;
  frost->Draw(blind, nothing);
  CHECK(nothing.screen_quads.empty());
  ticking.tick.owner_present = false;
  CHECK_FALSE(frost->Alive(ticking.tick));
}
