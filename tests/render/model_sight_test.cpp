#include "ro/render/model_sight.h"

#include <cmath>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "ro/formats/gnd.h"
#include "ro/formats/rsw.h"
#include "ro/render/model_mesh.h"

#include "support/map_builder.h"

using ro::formats::GndMap;
using ro::formats::RswModel;
using ro::render::ClientFrame;
using ro::render::ClientFrameOf;
using ro::render::FrustumSide;
using ro::render::ModelInstanceMatrix;
using ro::render::ModelMesh;
using ro::render::SegmentMeetsBox;
using ro::render::SightBox;
using ro::render::SightSegment;
using ro::test_support::BuildGnd;
using ro::test_support::GndBuildOptions;

namespace {

constexpr float kTolerance = 1e-3F;
constexpr std::int32_t kFirstFamily = 0x0104;

[[nodiscard]] bool Near(float actual, float expected) {
  return std::abs(actual - expected) < kTolerance;
}

[[nodiscard]] GndMap Ground(std::int32_t width, std::int32_t height) {
  GndBuildOptions options;
  options.width = width;
  options.height = height;
  auto gnd = GndMap::Parse(BuildGnd(options));
  REQUIRE(gnd.has_value());
  return std::move(*gnd);
}

// Une boîte alignée sur les axes du client.
[[nodiscard]] SightBox Cube(const glm::vec3& center, float half) {
  SightBox box;
  box.center = center;
  box.half_extents = {half, half, half};
  box.axes = {glm::vec3(1.0F, 0.0F, 0.0F), glm::vec3(0.0F, 1.0F, 0.0F),
              glm::vec3(0.0F, 0.0F, 1.0F)};
  box.lowest = center - glm::vec3(half);
  box.highest = center + glm::vec3(half);
  return box;
}

}  // namespace

TEST_CASE("Regard : une pose sans emprise retombe sur sa place au .rsw", "[render][veil]") {
  // Le repère du client : l'origine au centre de la carte, l'ordonnée qui
  // descend, la profondeur qui croît avec la rangée. Une place du .rsw y est
  // déjà écrite, et c'est ce qu'on doit retrouver en sortant de notre monde.
  const GndMap gnd = Ground(4, 6);
  const ClientFrame frame = ClientFrameOf(gnd);
  RswModel placement;
  placement.position = {3.0F, -7.0F, 11.0F};
  const ModelMesh empty;
  const glm::mat4 instance = ModelInstanceMatrix(placement, 4, 6, empty, kFirstFamily);

  const glm::vec3 client = frame.PointOf(glm::vec3(instance * glm::vec4(0.0F, 0.0F, 0.0F, 1.0F)));
  CHECK(Near(client.x, 3.0F));
  CHECK(Near(client.y, -7.0F));
  CHECK(Near(client.z, 11.0F));
  CHECK(ClientFrame::DirectionOf(glm::vec3(1.0F, 2.0F, 3.0F)) == glm::vec3(1.0F, -2.0F, -3.0F));
}

TEST_CASE("Regard : la boîte d'une pose est son emprise portée par la pose", "[render][veil]") {
  // Un modèle de quatre sur deux sur six, étiré du double en X. Le client
  // recentre la première famille en X et en Z, et la pose par sa base : sa
  // boîte s'étend donc de la place vers le HAUT, c'est-à-dire vers les
  // ordonnées négatives du client.
  const GndMap gnd = Ground(4, 6);
  ModelMesh mesh;
  mesh.bounds_min = {-1.0F, -2.0F, -3.0F};
  mesh.bounds_max = {1.0F, 0.0F, 3.0F};
  RswModel placement;
  placement.position = {10.0F, 0.0F, 20.0F};
  placement.scale = {2.0F, 1.0F, 1.0F};
  const glm::mat4 instance = ModelInstanceMatrix(placement, 4, 6, mesh, kFirstFamily);

  const SightBox box = ro::render::SightBoxOf(instance, mesh, ClientFrameOf(gnd));
  CHECK(Near(box.half_extents[0], 2.0F));
  CHECK(Near(box.half_extents[1], 1.0F));
  CHECK(Near(box.half_extents[2], 3.0F));
  CHECK(Near(box.center.x, 10.0F));
  CHECK(Near(box.center.y, -1.0F));
  CHECK(Near(box.center.z, 20.0F));
  CHECK(Near(std::abs(box.axes[0].x), 1.0F));
  CHECK(Near(std::abs(box.axes[1].y), 1.0F));
  CHECK(Near(std::abs(box.axes[2].z), 1.0F));
  CHECK(Near(box.lowest.x, 8.0F));
  CHECK(Near(box.lowest.y, -2.0F));
  CHECK(Near(box.highest.y, 0.0F));
  CHECK(Near(box.highest.z, 23.0F));
}

TEST_CASE("Regard : le segment coupe une boîte avant sa longueur", "[render][veil]") {
  const SightBox box = Cube(glm::vec3(0.0F), 1.0F);
  const glm::vec3 east{1.0F, 0.0F, 0.0F};
  CHECK(SegmentMeetsBox(SightSegment{glm::vec3(-5.0F, 0.0F, 0.0F), east, 10.0F}, box));
  // L'entrée tombe à quatre : un segment de trois s'arrête avant.
  CHECK_FALSE(SegmentMeetsBox(SightSegment{glm::vec3(-5.0F, 0.0F, 0.0F), east, 3.0F}, box));
  // Parallèle à la tranche Y et au-dessus d'elle.
  CHECK_FALSE(SegmentMeetsBox(SightSegment{glm::vec3(-5.0F, 2.0F, 0.0F), east, 10.0F}, box));
  // Derrière l'origine.
  CHECK_FALSE(SegmentMeetsBox(SightSegment{glm::vec3(5.0F, 0.0F, 0.0F), east, 10.0F}, box));
}

TEST_CASE("Regard : un œil DANS la boîte exige que la sortie tombe avant la longueur",
          "[render][veil]") {
  // La règle du client, et non celle d'un segment ordinaire : l'œil enfermé
  // dans un modèle ne le voile que si le joueur est au-delà de sa paroi.
  const SightBox box = Cube(glm::vec3(0.0F), 1.0F);
  const glm::vec3 east{1.0F, 0.0F, 0.0F};
  CHECK_FALSE(SegmentMeetsBox(SightSegment{glm::vec3(0.0F), east, 0.5F}, box));
  CHECK(SegmentMeetsBox(SightSegment{glm::vec3(0.0F), east, 2.0F}, box));
}

TEST_CASE("Regard : le segment part de l'œil et vise le torse du joueur", "[render][veil]") {
  const GndMap gnd = Ground(4, 6);
  const ClientFrame frame = ClientFrameOf(gnd);
  constexpr std::int32_t kWidth = 800;
  constexpr std::int32_t kHeight = 600;
  constexpr float kAspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
  const glm::vec3 player{20.0F, 3.0F, 40.0F};
  ro::render::Camera camera;
  camera.look_at = player;
  camera.distance = 300.0F;
  camera.pitch_degrees = 50.0F;

  const SightSegment segment =
      ro::render::PlayerSightSegment(camera, kAspect, kWidth, kHeight, player, frame);
  CHECK(segment.origin == frame.PointOf(camera.Position()));
  // L'ordonnée du joueur dans le repère du client, plus le recul, moins seize.
  CHECK(Near(segment.length, -3.0F + 300.0F - 16.0F));

  // Le joueur est au centre : son ancre tombe au pixel (400, 300) — ou 299 quand
  // le flottant la pose un rien au-dessus, et la troncature du client la suit —,
  // et le point visé vingt-huit pixels trois quarts plus haut, tronqué.
  const glm::mat4 view_projection = camera.ProjectionMatrix(kAspect) * camera.ViewMatrix();
  const auto pixel_of = [&view_projection](const glm::vec3& world) {
    const glm::vec4 clip = view_projection * glm::vec4(world, 1.0F);
    return glm::vec2((clip.x / clip.w + 1.0F) * 0.5F * static_cast<float>(kWidth),
                     (1.0F - clip.y / clip.w) * 0.5F * static_cast<float>(kHeight));
  };
  const auto anchor_y = static_cast<std::int32_t>(pixel_of(player).y);
  REQUIRE(anchor_y >= 299);
  REQUIRE(anchor_y <= 300);
  const auto aimed_y = static_cast<float>(static_cast<std::int32_t>(
      static_cast<float>(anchor_y) - static_cast<float>(kHeight) / 480.0F * 23.0F));

  const glm::vec3 direction = ClientFrame::DirectionOf(segment.direction);
  const glm::vec2 aimed = pixel_of(camera.Position() + direction * camera.distance);
  CHECK(std::abs(aimed.x - 400.0F) < 0.01F);
  CHECK(std::abs(aimed.y - aimed_y) < 0.01F);
}

TEST_CASE("Regard : une boîte se range dedans, dehors ou à cheval de la vue", "[render][veil]") {
  const GndMap gnd = Ground(4, 6);
  const ClientFrame frame = ClientFrameOf(gnd);
  ro::render::Camera camera;
  camera.look_at = glm::vec3(20.0F, 0.0F, 40.0F);
  camera.distance = 300.0F;
  const ro::render::SightFrustum frustum = ro::render::SightFrustumOf(camera, 1.0F, frame);

  CHECK(ro::render::BoxAgainstFrustum(frustum, Cube(frame.PointOf(camera.look_at), 1.0F)) ==
        FrustumSide::kInside);
  const glm::vec3 behind = camera.Position() + (camera.Position() - camera.look_at);
  CHECK(ro::render::BoxAgainstFrustum(frustum, Cube(frame.PointOf(behind), 1.0F)) ==
        FrustumSide::kOutside);
  const glm::vec3 edge = camera.look_at + glm::vec3(1000.0F, 0.0F, 0.0F);
  CHECK(ro::render::BoxAgainstFrustum(frustum, Cube(frame.PointOf(edge), 990.0F)) ==
        FrustumSide::kAcross);
}
