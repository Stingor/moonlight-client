#include "ro/render/actor_pick.h"

#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/camera.h"
#include "ro/render/ground_pick.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/sprite_frame.h"

#include "support/sprite_builder.h"

using ro::render::ActorScreenBounds;
using ro::render::ActorScreenRect;
using ro::render::Camera;
using ro::render::ComposedActor;
using ro::render::DrawnActor;
using ro::render::HasImage;
using ro::render::kMinimumPickSizePixels;
using ro::render::kSpritePixelsPerWorldUnit;
using ro::render::MakeScreenRay;
using ro::render::PickActor;
using ro::render::PickActorAmong;
using ro::render::ProjectToScreen;
using ro::render::ResolveImage;
using ro::render::ScreenRay;
using ro::render::ScreenRect;
using ro::render::SpriteImageKey;
using ro::render::SpriteQuad;

namespace {

constexpr float kViewportWidth = 800.0F;
constexpr float kViewportHeight = 600.0F;
constexpr float kAspectRatio = kViewportWidth / kViewportHeight;

bool NearlyEqual(float actual, float expected, float tolerance = 1e-2F) {
  return std::abs(actual - expected) < tolerance;
}

// Une caméra qui plonge sur le point donné, de face.
Camera Looking(const glm::vec3& target, float pitch_degrees = 45.0F) {
  Camera camera;
  camera.look_at = target;
  camera.distance = 100.0F;
  camera.pitch_degrees = pitch_degrees;
  camera.yaw_degrees = 0.0F;
  camera.fov_degrees = 45.0F;
  return camera;
}

// Un quad de la largeur et de la hauteur données, en pixels, dont l'origine
// est en BAS au milieu — comme un sprite d'acteur, dont le repère part des
// pieds.
SpriteQuad StandingQuad(float width, float height) {
  SpriteQuad quad;
  const float half = width * 0.5F;
  quad.corners = {glm::vec2{-half, -height}, glm::vec2{half, -height}, glm::vec2{half, 0.0F},
                  glm::vec2{-half, 0.0F}};
  return quad;
}

// Un sprite d'une seule image 2×2. Il ne sert pas à regarder mais à EXISTER :
// l'emprise ne compte que les calques qui portent une vraie image, et un quad
// sans provenance n'est jamais peint.
const ro::formats::SprSprite& PaintedSprite() {
  static const ro::formats::SprSprite sprite = [] {
    ro::test_support::SprBuildOptions options;
    options.indexed = {
        ro::test_support::SprIndexedFrameSpec{.width = 2, .height = 2, .indices = {1, 1, 1, 1}}};
    options.palette = {ro::test_support::SpriteColor{},
                       ro::test_support::SpriteColor{.red = 200, .green = 100, .blue = 50}};
    auto parsed = ro::formats::SprSprite::Parse(ro::test_support::BuildSpr(options));
    return std::move(*parsed);
  }();
  return sprite;
}

ComposedActor Standing(float width, float height) {
  ComposedActor composed;
  composed.quads.push_back(StandingQuad(width, height));
  composed.sources.push_back(SpriteImageKey{&PaintedSprite(), nullptr, 0, 0});
  return composed;
}

// Combien de pixels d'écran couvre une unité du monde au point visé, pour une
// caméra à plat : la moitié de la hauteur, sur la tangente du demi-champ,
// sur le recul.
float PixelsPerWorldUnit(const Camera& camera) {
  return (kViewportHeight * 0.5F) /
         (std::tan(glm::radians(camera.fov_degrees) * 0.5F) * camera.distance);
}

}  // namespace

TEST_CASE("Désignation : le point visé se projette au centre de l'écran", "[render][pick]") {
  const glm::vec3 target{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(target);
  const auto center =
      ProjectToScreen(camera, kAspectRatio, target, kViewportWidth, kViewportHeight);
  REQUIRE(center.has_value());
  CHECK(NearlyEqual(center->x, kViewportWidth / 2.0F));
  CHECK(NearlyEqual(center->y, kViewportHeight / 2.0F));
}

TEST_CASE("Désignation : la projection est l'inverse du rayon de pointage", "[render][pick]") {
  // Le rayon qui traverse le pixel où un point se projette doit passer par ce
  // point : sinon le clic sur un acteur et le clic sur le sol ne parlent pas
  // de la même image.
  const Camera camera = Looking({50.0F, 0.0F, 50.0F});
  const glm::vec3 point{62.0F, 7.0F, 41.0F};
  const auto pixel = ProjectToScreen(camera, kAspectRatio, point, kViewportWidth, kViewportHeight);
  REQUIRE(pixel.has_value());

  const ScreenRay ray =
      MakeScreenRay(camera, kAspectRatio, pixel->x, pixel->y, kViewportWidth, kViewportHeight);
  const glm::vec3 toward = glm::normalize(point - ray.origin);
  CHECK(NearlyEqual(ray.direction.x, toward.x, 1e-3F));
  CHECK(NearlyEqual(ray.direction.y, toward.y, 1e-3F));
  CHECK(NearlyEqual(ray.direction.z, toward.z, 1e-3F));
}

TEST_CASE("Désignation : un point derrière l'œil ne se projette pas", "[render][pick]") {
  const Camera camera = Looking({50.0F, 0.0F, 50.0F});
  const glm::vec3 behind = camera.Position() + (camera.Position() - camera.look_at);
  CHECK_FALSE(
      ProjectToScreen(camera, kAspectRatio, behind, kViewportWidth, kViewportHeight).has_value());
}

TEST_CASE("Désignation : l'emprise d'un acteur est celle de son sprite à l'écran",
          "[render][pick]") {
  // Caméra à plat : le panneau est vertical et sa hauteur en pixels se
  // calcule à la main, sans que l'élargissement minimal n'intervienne.
  const glm::vec3 ground{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(ground, 0.0F);
  const ComposedActor composed = Standing(70.0F, 140.0F);
  const DrawnActor drawn{&composed, nullptr, ground};

  const auto rect = ActorScreenRect(camera, kAspectRatio, drawn, kViewportWidth, kViewportHeight);
  REQUIRE(rect.has_value());
  const float expected_height = 140.0F / kSpritePixelsPerWorldUnit * PixelsPerWorldUnit(camera);
  const float expected_width = 70.0F / kSpritePixelsPerWorldUnit * PixelsPerWorldUnit(camera);
  CHECK(NearlyEqual(rect->max.y - rect->min.y, expected_height, 0.5F));
  CHECK(NearlyEqual(rect->max.x - rect->min.x, expected_width, 0.5F));
  // Les pieds sont au point visé, donc au centre ; la tête au-dessus.
  CHECK(NearlyEqual(rect->max.y, kViewportHeight / 2.0F, 0.5F));
  CHECK(rect->min.y < kViewportHeight / 2.0F);
}

TEST_CASE("Désignation : une emprise trop petite s'élargit autour de son centre",
          "[render][pick]") {
  // Un Poring de loin fait quelques pixels : sans cet élargissement, il
  // serait plus facile à rater qu'à cliquer.
  const glm::vec3 ground{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(ground, 0.0F);
  const ComposedActor composed = Standing(2.0F, 2.0F);
  const DrawnActor drawn{&composed, nullptr, ground};

  const auto rect = ActorScreenRect(camera, kAspectRatio, drawn, kViewportWidth, kViewportHeight);
  REQUIRE(rect.has_value());
  CHECK(NearlyEqual(rect->max.x - rect->min.x, kMinimumPickSizePixels));
  CHECK(NearlyEqual(rect->max.y - rect->min.y, kMinimumPickSizePixels));
  CHECK(NearlyEqual((rect->min.x + rect->max.x) * 0.5F, kViewportWidth / 2.0F));
}

TEST_CASE("Emprise : un calque SANS IMAGE ne l'étire pas", "[render][pick]") {
  // Les fichiers du jeu portent des calques d'image vide ou absente, et leur
  // .act leur donne quand même un décalage. Le renderer ne les peint pas — il
  // saute tout calque dont l'image ne se résout pas — et les compter ferait
  // descendre la boîte d'un acteur bien plus bas que ce qu'on voit de lui.
  const glm::vec3 ground{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(ground, 0.0F);

  const ComposedActor visible = Standing(70.0F, 140.0F);
  ComposedActor with_empty = Standing(70.0F, 140.0F);
  SpriteQuad below = StandingQuad(70.0F, 140.0F);
  for (glm::vec2& corner : below.corners) {
    corner.y += 400.0F;
  }
  with_empty.quads.push_back(below);
  // Index hors de sa section : l'image ne se résout pas.
  with_empty.sources.push_back(SpriteImageKey{&PaintedSprite(), nullptr, 0, 7});

  const DrawnActor plain{&visible, nullptr, ground};
  const DrawnActor padded{&with_empty, nullptr, ground};
  const auto plain_rect =
      ActorScreenBounds(camera, kAspectRatio, plain, kViewportWidth, kViewportHeight);
  const auto padded_rect =
      ActorScreenBounds(camera, kAspectRatio, padded, kViewportWidth, kViewportHeight);
  REQUIRE(plain_rect.has_value());
  REQUIRE(padded_rect.has_value());
  CHECK(NearlyEqual(padded_rect->min.y, plain_rect->min.y));
  CHECK(NearlyEqual(padded_rect->max.y, plain_rect->max.y));
}

TEST_CASE("Emprise : un acteur dont RIEN ne se peint n'en a pas", "[render][pick]") {
  // Sans cette sortie, le rectangle sentinelle — infini à l'envers — serait
  // rendu tel quel, et l'acteur deviendrait attrapable partout à l'écran.
  const glm::vec3 ground{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(ground, 0.0F);
  ComposedActor empty;
  empty.quads.push_back(StandingQuad(70.0F, 140.0F));
  empty.sources.push_back(SpriteImageKey{&PaintedSprite(), nullptr, 0, 7});
  const DrawnActor drawn{&empty, nullptr, ground};
  CHECK_FALSE(
      ActorScreenBounds(camera, kAspectRatio, drawn, kViewportWidth, kViewportHeight).has_value());
}

TEST_CASE("Emprise de parole : une coiffe animée ne déplace pas la bulle", "[render][pick]") {
  // Le haut d'une coiffe ou d'une cape animée change d'une image à l'autre ; la
  // bulle ancrée au corps et à la tête reste en place.
  const glm::vec3 ground{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(ground, 0.0F);
  const auto dressed = [](float headgear_height, float garment_height) {
    ComposedActor composed = Standing(70.0F, 140.0F);
    composed.parts.push_back(ro::render::ActorPart::kBody);
    for (const auto& [part, height] :
         {std::pair{ro::render::ActorPart::kHeadgearTop, headgear_height},
          std::pair{ro::render::ActorPart::kGarment, garment_height}}) {
      composed.quads.push_back(StandingQuad(40.0F, height));
      composed.sources.push_back(SpriteImageKey{&PaintedSprite(), nullptr, 0, 0});
      composed.parts.push_back(part);
    }
    return composed;
  };
  const ComposedActor low = dressed(170.0F, 150.0F);
  const ComposedActor high = dressed(210.0F, 190.0F);
  const auto low_rect = ro::render::ActorSpeakerScreenBounds(
      camera, kAspectRatio, DrawnActor{&low, nullptr, ground}, kViewportWidth, kViewportHeight);
  const auto high_rect = ro::render::ActorSpeakerScreenBounds(
      camera, kAspectRatio, DrawnActor{&high, nullptr, ground}, kViewportWidth, kViewportHeight);
  REQUIRE(low_rect.has_value());
  REQUIRE(high_rect.has_value());
  CHECK(NearlyEqual(low_rect->min.y, high_rect->min.y));
  // L'emprise entière, elle, suit le costume.
  const auto whole = ActorScreenBounds(camera, kAspectRatio, DrawnActor{&high, nullptr, ground},
                                       kViewportWidth, kViewportHeight);
  REQUIRE(whole.has_value());
  CHECK(whole->min.y < high_rect->min.y);
}

TEST_CASE("Emprise de parole : sans corps ni tête peints, c'est l'emprise entière",
          "[render][pick]") {
  const glm::vec3 ground{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(ground, 0.0F);
  ComposedActor composed = Standing(70.0F, 140.0F);
  composed.parts.push_back(ro::render::ActorPart::kGarment);
  const DrawnActor drawn{&composed, nullptr, ground};
  const auto speaker = ro::render::ActorSpeakerScreenBounds(camera, kAspectRatio, drawn,
                                                            kViewportWidth, kViewportHeight);
  const auto whole =
      ActorScreenBounds(camera, kAspectRatio, drawn, kViewportWidth, kViewportHeight);
  REQUIRE(speaker.has_value());
  REQUIRE(whole.has_value());
  CHECK(NearlyEqual(speaker->min.y, whole->min.y));
}

TEST_CASE("Image d'un calque : HasImage répond comme ResolveImage", "[render][pick]") {
  // Les deux DOIVENT s'accorder : l'une décide de ce qui est peint, l'autre de
  // ce qui compte dans l'emprise. Les laisser diverger ferait entrer dans la
  // boîte de clic un calque que personne ne voit.
  const ro::formats::SprSprite& sprite = PaintedSprite();
  const std::vector<SpriteImageKey> keys = {
      SpriteImageKey{&sprite, nullptr, 0, 0},  SpriteImageKey{&sprite, nullptr, 0, 7},
      SpriteImageKey{&sprite, nullptr, 0, -1}, SpriteImageKey{&sprite, nullptr, 1, 0},
      SpriteImageKey{nullptr, nullptr, 0, 0},
  };
  for (const SpriteImageKey& key : keys) {
    CHECK(HasImage(key) == !ResolveImage(key).empty());
  }
  // Et le cas nominal est bien vrai : un essai qui ne verrait que des faux
  // passerait aussi.
  CHECK(HasImage(keys.front()));
}

TEST_CASE("Désignation : sans quad, pas d'emprise", "[render][pick]") {
  const Camera camera = Looking({50.0F, 0.0F, 50.0F});
  const ComposedActor nothing;
  const DrawnActor drawn{&nothing, nullptr, {50.0F, 0.0F, 50.0F}};
  CHECK_FALSE(
      ActorScreenRect(camera, kAspectRatio, drawn, kViewportWidth, kViewportHeight).has_value());
}

TEST_CASE("Désignation : le pixel désigne le plus proche de deux acteurs superposés",
          "[render][pick]") {
  const Camera camera = Looking({50.0F, 0.0F, 50.0F});
  const ComposedActor composed = Standing(70.0F, 140.0F);
  // Le second est trois unités plus près de l'œil, sur la ligne de visée.
  const glm::vec3 far_ground{50.0F, 0.0F, 50.0F};
  const glm::vec3 near_ground = far_ground + glm::normalize(camera.Position() - far_ground) * 3.0F;
  const std::vector<DrawnActor> actors = {DrawnActor{&composed, nullptr, far_ground},
                                          DrawnActor{&composed, nullptr, near_ground}};

  const auto picked = PickActor(camera, kAspectRatio, actors, kViewportWidth / 2.0F,
                                kViewportHeight / 2.0F - 10.0F, kViewportWidth, kViewportHeight);
  REQUIRE(picked.has_value());
  CHECK(*picked == 1);

  // Loin de tout, rien.
  CHECK_FALSE(PickActor(camera, kAspectRatio, actors, 5.0F, 5.0F, kViewportWidth, kViewportHeight)
                  .has_value());
}

TEST_CASE("Désignation : un acteur écarté ne cache pas celui qui se tient derrière lui",
          "[render][pick]") {
  const Camera camera = Looking({50.0F, 0.0F, 50.0F});
  const ComposedActor composed = Standing(70.0F, 140.0F);
  const glm::vec3 far_ground{50.0F, 0.0F, 50.0F};
  const glm::vec3 near_ground = far_ground + glm::normalize(camera.Position() - far_ground) * 3.0F;
  const std::vector<DrawnActor> actors = {DrawnActor{&composed, nullptr, far_ground},
                                          DrawnActor{&composed, nullptr, near_ground}};
  constexpr std::size_t kBehind = 0;
  constexpr float kPointerX = kViewportWidth / 2.0F;
  constexpr float kPointerY = kViewportHeight / 2.0F - 10.0F;

  const auto picked =
      PickActorAmong(camera, kAspectRatio, actors, kPointerX, kPointerY, kViewportWidth,
                     kViewportHeight, [](std::size_t index) { return index == kBehind; });
  REQUIRE(picked.has_value());
  CHECK(*picked == kBehind);

  CHECK_FALSE(PickActorAmong(camera, kAspectRatio, actors, kPointerX, kPointerY, kViewportWidth,
                             kViewportHeight, [](std::size_t /*index*/) { return false; })
                  .has_value());
}

TEST_CASE("Désignation : le recul d'un genre l'emporte sur la distance", "[render][pick]") {
  // Un joueur devant un PNJ : le client donne au joueur quatre cents de recul
  // et cent au PNJ, et c'est le PNJ qu'on désigne — un joueur planté sur un
  // marchand ne l'empêche pas de vendre.
  const Camera camera = Looking({50.0F, 0.0F, 50.0F});
  const ComposedActor composed = Standing(70.0F, 140.0F);
  const glm::vec3 far_ground{50.0F, 0.0F, 50.0F};
  const glm::vec3 near_ground = far_ground + glm::normalize(camera.Position() - far_ground) * 3.0F;
  constexpr float kNpcBias = 100.0F;
  constexpr float kPlayerBias = 400.0F;
  const std::vector<DrawnActor> actors = {
      DrawnActor{&composed, nullptr, far_ground, {255, 255, 255, 255}, kNpcBias},
      DrawnActor{&composed, nullptr, near_ground, {255, 255, 255, 255}, kPlayerBias}};

  const auto picked = PickActor(camera, kAspectRatio, actors, kViewportWidth / 2.0F,
                                kViewportHeight / 2.0F - 10.0F, kViewportWidth, kViewportHeight);
  REQUIRE(picked.has_value());
  CHECK(*picked == 0);
}

namespace {

// Un warp : rien de peint, un disque au sol du rayon de ses anneaux.
DrawnActor OnGround(const glm::vec3& ground, float radius) {
  DrawnActor drawn{nullptr, nullptr, ground};
  drawn.ground_pick_radius = radius;
  return drawn;
}

glm::vec2 Projected(const Camera& camera, const glm::vec3& point) {
  const std::optional<glm::vec2> screen =
      ProjectToScreen(camera, kAspectRatio, point, kViewportWidth, kViewportHeight);
  REQUIRE(screen.has_value());
  return *screen;
}

bool Picks(const Camera& camera, const std::vector<DrawnActor>& actors, glm::vec2 pointer) {
  return PickActor(camera, kAspectRatio, actors, pointer.x, pointer.y, kViewportWidth,
                   kViewportHeight)
      .has_value();
}

}  // namespace

TEST_CASE("Warp : il se désigne par le disque qu'il occupe au sol", "[render][pick][warp]") {
  const glm::vec3 center{50.0F, 0.0F, 50.0F};
  constexpr float kRadius = 10.0F;
  const Camera camera = Looking(center);
  const std::vector<DrawnActor> actors = {OnGround(center, kRadius)};

  CHECK(Picks(camera, actors, Projected(camera, center)));
  // Juste dedans et juste dehors, le long des deux axes du sol.
  CHECK(Picks(camera, actors, Projected(camera, center + glm::vec3(9.5F, 0.0F, 0.0F))));
  CHECK(Picks(camera, actors, Projected(camera, center + glm::vec3(0.0F, 0.0F, -9.5F))));
  CHECK_FALSE(Picks(camera, actors, Projected(camera, center + glm::vec3(10.5F, 0.0F, 0.0F))));
  CHECK_FALSE(Picks(camera, actors, Projected(camera, center + glm::vec3(0.0F, 0.0F, 10.5F))));
  // 🔴 LE COIN DU RECTANGLE N'EST PAS LE DISQUE : c'est ce qui sépare notre
  // emprise d'un rectangle d'écran, qui prend le clic sur du vide.
  CHECK_FALSE(Picks(camera, actors, Projected(camera, center + glm::vec3(9.0F, 0.0F, 9.0F))));
}

TEST_CASE("Warp : son disque suit la rotation de la caméra", "[render][pick][warp]") {
  const glm::vec3 center{50.0F, 0.0F, 50.0F};
  Camera camera = Looking(center);
  camera.yaw_degrees = 90.0F;
  const std::vector<DrawnActor> actors = {OnGround(center, 10.0F)};
  CHECK(Picks(camera, actors, Projected(camera, center + glm::vec3(0.0F, 0.0F, 9.5F))));
  CHECK_FALSE(Picks(camera, actors, Projected(camera, center + glm::vec3(0.0F, 0.0F, 10.5F))));
}

TEST_CASE("Warp : l'acteur qui s'y tient l'emporte, même plus loin de l'œil",
          "[render][pick][warp]") {
  const glm::vec3 center{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(center);
  const ComposedActor standing = Standing(70.0F, 140.0F);
  // Le warp est même un peu plus PRÈS de l'œil : ce n'est pas la distance qui
  // tranche, c'est qu'il est au sol, sous l'acteur.
  const glm::vec3 warp_center = center + glm::normalize(camera.Position() - center) * 3.0F;
  const std::vector<DrawnActor> actors = {OnGround(warp_center, 10.0F),
                                          DrawnActor{&standing, nullptr, center}};
  const glm::vec2 pointer = Projected(camera, center) - glm::vec2(0.0F, 10.0F);
  const auto picked = PickActor(camera, kAspectRatio, actors, pointer.x, pointer.y, kViewportWidth,
                                kViewportHeight);
  REQUIRE(picked.has_value());
  CHECK(*picked == 1);

  // Sur le disque, loin de l'acteur, c'est le warp.
  const glm::vec2 aside = Projected(camera, warp_center + glm::vec3(8.0F, 0.0F, 0.0F));
  const auto on_warp =
      PickActor(camera, kAspectRatio, actors, aside.x, aside.y, kViewportWidth, kViewportHeight);
  REQUIRE(on_warp.has_value());
  CHECK(*on_warp == 0);
}

TEST_CASE("Warp : vu de loin, il garde l'emprise plancher", "[render][pick][warp]") {
  const glm::vec3 center{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(center);
  // Un disque minuscule : son contour tient dans quelques pixels.
  const std::vector<DrawnActor> actors = {OnGround(center, 0.05F)};
  const glm::vec2 middle = Projected(camera, center);
  const float half = kMinimumPickSizePixels * 0.5F;
  CHECK(Picks(camera, actors, middle + glm::vec2(half - 2.0F, 0.0F)));
  CHECK_FALSE(Picks(camera, actors, middle + glm::vec2(half + 2.0F, 0.0F)));
}

TEST_CASE("Warp : son disque ne sert qu'à désigner, rien ne s'y ancre", "[render][pick][warp]") {
  const glm::vec3 center{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(center);
  CHECK_FALSE(ActorScreenBounds(camera, kAspectRatio, OnGround(center, 10.0F), kViewportWidth,
                                kViewportHeight)
                  .has_value());
}

namespace {

// Un PNJ caché : rien de peint, un point au sol.
DrawnActor AtPoint(const glm::vec3& ground, float floor_pixels) {
  DrawnActor drawn{nullptr, nullptr, ground};
  drawn.point_pick = true;
  drawn.pick_floor_pixels = floor_pixels;
  return drawn;
}

}  // namespace

TEST_CASE("PNJ caché : sans image, il se désigne par le carré du plancher autour de ses pieds",
          "[render][pick][hidden_npc]") {
  const glm::vec3 feet{50.0F, 0.0F, 50.0F};
  const Camera camera = Looking(feet);
  constexpr float kFloorPixels = 40.0F;
  const std::vector<DrawnActor> actors = {AtPoint(feet, kFloorPixels)};
  const glm::vec2 foot = Projected(camera, feet);

  const std::optional<ScreenRect> rect =
      ActorScreenRect(camera, kAspectRatio, actors.front(), kViewportWidth, kViewportHeight);
  REQUIRE(rect.has_value());
  CHECK(NearlyEqual(rect->max.x - rect->min.x, kFloorPixels));
  CHECK(NearlyEqual(rect->max.y - rect->min.y, kFloorPixels));
  CHECK(NearlyEqual((rect->min.x + rect->max.x) * 0.5F, foot.x));
  CHECK(NearlyEqual((rect->min.y + rect->max.y) * 0.5F, foot.y));

  CHECK(Picks(camera, actors, foot));
  CHECK(Picks(camera, actors, foot + glm::vec2(18.0F, -18.0F)));
  CHECK(Picks(camera, actors, foot + glm::vec2(-18.0F, 18.0F)));
  CHECK_FALSE(Picks(camera, actors, foot + glm::vec2(22.0F, 0.0F)));
  CHECK_FALSE(Picks(camera, actors, foot + glm::vec2(0.0F, -22.0F)));
  // Rien ne s'y ancre : ni nom, ni jauge.
  CHECK_FALSE(
      ActorScreenBounds(camera, kAspectRatio, actors.front(), kViewportWidth, kViewportHeight)
          .has_value());
}
