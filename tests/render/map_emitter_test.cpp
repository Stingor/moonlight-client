#include "ro/render/map_emitter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"
#include "ro/render/map_effect_math.h"
#include "ro/render/sprite_billboard.h"

using Catch::Approx;
using ro::render::EffectWorldQuad;
using ro::render::MapEffectFrame;
using ro::render::MapEmitter;
using ro::render::MapEmitterSetting;

namespace {

// Un émetteur SANS HASARD : bornes égales, rayon et directions nuls. Une
// particule par pas, dix secondes de vie.
MapEmitterSetting Still() {
  MapEmitterSetting setting;
  setting.max_count = 5;
  setting.first_direction = {0.0F, 0.0F, 0.0F};
  setting.second_direction = {0.0F, 0.0F, 0.0F};
  setting.gravity = {0.0F, 0.0F, 0.0F};
  setting.radius = {0.0F, 0.0F, 0.0F};
  setting.rate = {60.0F, 60.0F};
  setting.size = {2.0F, 2.0F};
  setting.life = {10.0F, 10.0F};
  setting.texture = "t.bmp";
  return setting;
}

constexpr glm::vec3 kEmitterPlace{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kPlayerNear{0.0F, 0.0F, 0.0F};
constexpr glm::vec3 kPlayerFar{1000.0F, 0.0F, 0.0F};
constexpr MapEffectFrame kFrame{200, 200};

std::vector<EffectWorldQuad> QuadsOf(const MapEmitter& emitter,
                                     const ro::render::BillboardBasis& basis = {}) {
  std::vector<EffectWorldQuad> quads;
  emitter.AppendQuads(kFrame, basis, quads);
  return quads;
}

// Le plus grand alpha parmi les quads : celui de la seule particule vivante.
std::uint8_t BrightestAlpha(const std::vector<EffectWorldQuad>& quads) {
  std::uint8_t alpha = 0;
  for (const EffectWorldQuad& quad : quads) {
    alpha = std::max(alpha, quad.tint[3]);
  }
  return alpha;
}

}  // namespace

TEST_CASE("le tirage du client suit une grille d'un dixième, depuis la première borne") {
  std::uint32_t draws = 1;
  CHECK(ro::render::EffectToolRandomBetween(draws, 2.5F, 2.5F) == 2.5F);
  for (int attempt = 0; attempt < 200; ++attempt) {
    const float value = ro::render::EffectToolRandomBetween(draws, 0.3F, 0.7F);
    CHECK(value >= Approx(0.3F));
    CHECK(value <= Approx(0.7F));
    CHECK(std::abs(value * 10.0F - std::round(value * 10.0F)) < 1e-4F);
    // 🔴 UNE SECONDE BORNE PLUS PETITE NE SE REJOINT PAS : de 1 vers le haut.
    const float reversed = ro::render::EffectToolRandomBetween(draws, 1.0F, -1.0F);
    CHECK(reversed >= Approx(1.0F));
    CHECK(reversed <= Approx(3.0F));
  }
}

TEST_CASE("la table de degrés du client lit un négatif un degré trop loin") {
  constexpr float kDegree = 3.141592F / 180.0F;
  CHECK(ro::render::NativeTableRadians(90) == Approx(90.0F * kDegree));
  CHECK(ro::render::NativeTableRadians(-90) == Approx(271.0F * kDegree));
  CHECK(ro::render::NativeTableRadians(-360) == Approx(1.0F * kDegree));
  CHECK(ro::render::NativeTableRadians(725) == Approx(5.0F * kDegree));
}

TEST_CASE("la matrice du client s'applique à un vecteur ligne, rotation puis place") {
  ro::render::NativeMatrix matrix = ro::render::kNativeIdentity;
  ro::render::MultiplyRotationX(matrix, 3.141592F / 2.0F);
  ro::render::MultiplyTranslation(matrix, {10.0F, 20.0F, 30.0F});
  const glm::vec3 moved = ro::render::Transform({0.0F, 1.0F, 0.0F}, matrix);
  CHECK(moved.x == Approx(10.0F));
  CHECK(moved.y == Approx(20.0F).margin(1e-4));
  CHECK(moved.z == Approx(31.0F));

  const glm::vec3 native{12.0F, -3.0F, 7.0F};
  const glm::vec3 back = kFrame.ToNative(kFrame.ToWorld(native));
  CHECK(back.x == Approx(native.x));
  CHECK(back.y == Approx(native.y));
  CHECK(back.z == Approx(native.z));
}

TEST_CASE("la réserve est fixe : jamais plus de particules que max_count") {
  MapEmitterSetting setting = Still();
  setting.rate = {600.0F, 600.0F};
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, setting, draws);
  for (int step = 0; step < 3; ++step) {
    emitter.Tick(kPlayerNear, draws);
  }
  CHECK(emitter.living() == 5);
  CHECK(emitter.particle_count() == 5);
}

TEST_CASE("la cadence est par seconde : soixante font une particule par pas") {
  MapEmitterSetting setting = Still();
  setting.max_count = 100;
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, setting, draws);
  emitter.Tick(kPlayerNear, draws);
  CHECK(emitter.living() == 1);
  for (int step = 0; step < 9; ++step) {
    emitter.Tick(kPlayerNear, draws);
  }
  CHECK(emitter.living() == 10);
}

TEST_CASE("toute la réserve se peint, née ou non") {
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, Still(), draws);
  CHECK(QuadsOf(emitter).empty());  // Jamais mis à jour : rien à peindre.
  emitter.Tick(kPlayerNear, draws);
  CHECK(emitter.living() == 1);
  CHECK(QuadsOf(emitter).size() == 5);
}

TEST_CASE("le fondu monte de quatre par pas, la troncature mange le quart") {
  MapEmitterSetting setting = Still();
  setting.max_count = 1;
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, setting, draws);
  emitter.Tick(kPlayerNear, draws);
  CHECK(BrightestAlpha(QuadsOf(emitter)) == 4);
  emitter.Tick(kPlayerNear, draws);
  CHECK(BrightestAlpha(QuadsOf(emitter)) == 8);
}

TEST_CASE("une éternelle prend l'alpha de sa couleur, sans fondu") {
  MapEmitterSetting setting = Still();
  setting.max_count = 1;
  setting.eternal = true;
  setting.color = {255, 255, 255, 200};
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, setting, draws);
  emitter.Tick(kPlayerNear, draws);
  CHECK(BrightestAlpha(QuadsOf(emitter)) == 200);
}

TEST_CASE("loin du joueur, l'émetteur se tait, puis se cache sa réserve vide") {
  MapEmitterSetting setting = Still();
  setting.life = {0.1F, 0.1F};
  std::uint32_t draws = 1;

  MapEmitter far_away(kEmitterPlace, setting, draws);
  far_away.Tick(kPlayerFar, draws);
  CHECK(far_away.living() == 0);
  CHECK_FALSE(far_away.drawn());

  MapEmitter walked_away(kEmitterPlace, setting, draws);
  walked_away.Tick(kPlayerNear, draws);
  REQUIRE(walked_away.living() == 1);
  walked_away.Tick(kPlayerFar, draws);
  // Encore une particule à finir : il vit toujours.
  CHECK(walked_away.drawn());
  for (int step = 0; step < 10; ++step) {
    walked_away.Tick(kPlayerFar, draws);
  }
  CHECK(walked_away.living() == 0);
  CHECK_FALSE(walked_away.drawn());
}

TEST_CASE("une particule naît dix unités au-dessus de son émetteur") {
  MapEmitterSetting setting = Still();
  setting.max_count = 1;
  setting.flat = true;
  setting.size = {1.0F, 1.0F};
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, setting, draws);
  emitter.Tick(kPlayerNear, draws);
  const std::vector<EffectWorldQuad> quads = QuadsOf(emitter);
  REQUIRE(quads.size() == 1);
  float height = 0.0F;
  for (const glm::vec3& corner : quads.front().corners) {
    height += corner.y / 4.0F;
  }
  CHECK(height == Approx(kFrame.ToWorld(kEmitterPlace).y + 10.0F));
}

TEST_CASE("une particule couchée par ses angles reste de niveau") {
  MapEmitterSetting setting = Still();
  setting.max_count = 1;
  setting.flat = true;
  setting.rotation_degrees = {90.0F, 0.0F, 0.0F};
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, setting, draws);
  emitter.Tick(kPlayerNear, draws);
  const std::vector<EffectWorldQuad> quads = QuadsOf(emitter);
  REQUIRE(quads.size() == 1);
  for (const glm::vec3& corner : quads.front().corners) {
    CHECK(corner.y == Approx(quads.front().corners[0].y).margin(1e-3));
  }
  // Et elle a une étendue : deux fois sa taille sur chaque axe du sol.
  CHECK(std::abs(quads.front().corners[0].x - quads.front().corners[1].x) == Approx(4.0F));
}

TEST_CASE("face à l'œil, le haut de l'image va au coin du BAS") {
  ro::render::Camera camera;
  camera.pitch_degrees = 45.0F;
  camera.look_at = kFrame.ToWorld(kEmitterPlace);
  MapEmitterSetting setting = Still();
  setting.max_count = 1;
  std::uint32_t draws = 1;
  MapEmitter emitter(kEmitterPlace, setting, draws);
  emitter.Tick(kPlayerNear, draws);
  const std::vector<EffectWorldQuad> quads =
      QuadsOf(emitter, ro::render::MakeBillboardBasis(camera));
  REQUIRE(quads.size() == 1);
  const EffectWorldQuad& quad = quads.front();
  float image_top = 0.0F;
  float image_bottom = 0.0F;
  for (std::size_t corner = 0; corner < quad.corners.size(); ++corner) {
    if (quad.texture_uv[corner] == glm::vec2{0.0F, 0.0F}) {
      image_top = quad.corners[corner].y;
    }
    if (quad.texture_uv[corner] == glm::vec2{0.0F, 1.0F}) {
      image_bottom = quad.corners[corner].y;
    }
  }
  CHECK(image_top < image_bottom);
}
