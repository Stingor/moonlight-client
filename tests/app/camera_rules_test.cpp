// Les règles de la caméra : bornes, pas des gestes, remises et tables par carte.

#include "input/camera_rules.h"

#include <string>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/camera.h"

namespace {

using Catch::Approx;
using moonclient::ApplyNativeView;
using moonclient::CameraDrag;
using moonclient::CameraLimits;
using moonclient::CameraMemory;
using moonclient::CameraTableKey;
using moonclient::LimitsFor;
using moonclient::MapCameraRules;
using moonclient::MapCameraTables;
using moonclient::NativeView;
using moonclient::NativeViewOf;
using moonclient::ViewpointRow;

[[nodiscard]] CameraLimits Outdoor(bool expanded = false) {
  return LimitsFor(MapCameraRules{}, expanded, /*bounded=*/true);
}

[[nodiscard]] CameraLimits Indoor() {
  return LimitsFor(MapCameraRules{.indoor = true}, false, /*bounded=*/true);
}

// La ligne de `1@infi` du fichier livré.
[[nodiscard]] ViewpointRow InfinityRow() {
  return ViewpointRow{.range = 230,
                      .scope = 170,
                      .range_on_entry = 500,
                      .rotation_from = 0,
                      .rotation_to = 0,
                      .rotation_on_entry = -65,
                      .altitude_from = -50,
                      .altitude_to = -65,
                      .altitude_on_entry = -65};
}

[[nodiscard]] ro::render::Camera CameraAt(float zoom, float altitude, float yaw) {
  ro::render::Camera camera;
  ApplyNativeView(camera,
                  NativeView{.zoom = zoom, .altitude_degrees = altitude, .yaw_degrees = yaw});
  return camera;
}

[[nodiscard]] float ZoomOf(const ro::render::Camera& camera) { return NativeViewOf(camera).zoom; }

[[nodiscard]] float AltitudeOf(const ro::render::Camera& camera) {
  return NativeViewOf(camera).altitude_degrees;
}

[[nodiscard]] float YawOf(const ro::render::Camera& camera) {
  return NativeViewOf(camera).yaw_degrees;
}

}  // namespace

TEST_CASE("Caméra : le recul du jeu est la distance, l'altitude et le lacet changent de signe",
          "[app][camera]") {
  const ro::render::Camera camera = CameraAt(300.0F, -50.0F, 30.0F);
  CHECK(camera.distance == Approx(300.0F));
  CHECK(camera.pitch_degrees == Approx(50.0F));
  CHECK(camera.yaw_degrees == Approx(-30.0F));
  CHECK(ZoomOf(camera) == Approx(300.0F));
  CHECK(AltitudeOf(camera) == Approx(-50.0F));
  CHECK(YawOf(camera) == Approx(30.0F));
}

TEST_CASE("Caméra : un cran vers l'avant recule, jusqu'à 500 dehors", "[app][camera]") {
  ro::render::Camera camera = CameraAt(490.0F, -50.0F, 0.0F);
  moonclient::WheelZoomCamera(camera, /*forward=*/true, Outdoor());
  CHECK(ZoomOf(camera) == Approx(500.0F));
  moonclient::WheelZoomCamera(camera, /*forward=*/false, Outdoor());
  CHECK(ZoomOf(camera) == Approx(500.0F - 24.3F));
}

TEST_CASE("Caméra : /zoom laisse reculer jusqu'à 1000", "[app][camera]") {
  ro::render::Camera camera = CameraAt(990.0F, -50.0F, 0.0F);
  moonclient::WheelZoomCamera(camera, /*forward=*/true, Outdoor(/*expanded=*/true));
  CHECK(ZoomOf(camera) == Approx(1000.0F));
}

TEST_CASE("Caméra : on ne s'approche pas plus près que 200", "[app][camera]") {
  ro::render::Camera camera = CameraAt(210.0F, -50.0F, 0.0F);
  moonclient::WheelZoomCamera(camera, /*forward=*/false, Outdoor());
  CHECK(ZoomOf(camera) == Approx(200.0F));
}

TEST_CASE("Caméra : dehors, l'altitude va jusqu'à −5, Ctrl ouvre jusqu'à 20", "[app][camera]") {
  ro::render::Camera camera = CameraAt(300.0F, -10.0F, 0.0F);
  moonclient::WheelTiltCamera(camera, /*forward=*/true, Outdoor(), /*ctrl_held=*/false);
  CHECK(AltitudeOf(camera) == Approx(-5.0F));
  moonclient::WheelTiltCamera(camera, /*forward=*/true, Outdoor(), /*ctrl_held=*/false);
  CHECK(AltitudeOf(camera) == Approx(-5.0F));
  moonclient::WheelTiltCamera(camera, /*forward=*/true, Outdoor(), /*ctrl_held=*/true);
  CHECK(AltitudeOf(camera) == Approx(0.0F));
}

TEST_CASE("Caméra : dehors, la caméra ne monte pas au-delà de −70", "[app][camera]") {
  ro::render::Camera camera = CameraAt(300.0F, -50.0F, 0.0F);
  moonclient::DragCamera(camera, CameraDrag{.dy_px = 400.0F, .tilt = true}, Outdoor(), false);
  CHECK(AltitudeOf(camera) == Approx(-70.0F));
}

TEST_CASE("Caméra : dedans, sans Ctrl, l'altitude se fige à −60", "[app][camera]") {
  // Le plancher (−55) passe au-dessus du plafond (−60) : le plafond gagne.
  ro::render::Camera camera = CameraAt(300.0F, -45.0F, 45.0F);
  moonclient::DragCamera(camera, CameraDrag{.dy_px = 40.0F, .tilt = true}, Indoor(), false);
  CHECK(AltitudeOf(camera) == Approx(-60.0F));
  moonclient::DragCamera(camera, CameraDrag{.dy_px = -40.0F, .tilt = true}, Indoor(), false);
  CHECK(AltitudeOf(camera) == Approx(-60.0F));
  // Ctrl tenu, la plage redevient [−55, −35].
  moonclient::DragCamera(camera, CameraDrag{.dy_px = -40.0F, .tilt = true}, Indoor(), true);
  CHECK(AltitudeOf(camera) == Approx(-40.0F));
}

TEST_CASE("Caméra : dedans, le lacet reste à vingt degrés de quarante-cinq", "[app][camera]") {
  ro::render::Camera camera = CameraAt(300.0F, -45.0F, 45.0F);
  moonclient::DragCamera(camera, CameraDrag{.dx_px = 100.0F, .orbit = true}, Indoor(), false);
  CHECK(YawOf(camera) == Approx(65.0F));
  moonclient::DragCamera(camera, CameraDrag{.dx_px = -100.0F, .orbit = true}, Indoor(), false);
  CHECK(YawOf(camera) == Approx(25.0F));
}

TEST_CASE("Caméra : dehors, le lacet tourne librement, un degré par pixel", "[app][camera]") {
  ro::render::Camera camera = CameraAt(300.0F, -50.0F, 350.0F);
  moonclient::DragCamera(camera, CameraDrag{.dx_px = 30.0F, .orbit = true}, Outdoor(), false);
  CHECK(YawOf(camera) == Approx(380.0F));
}

TEST_CASE("Caméra : Ctrl + glisser recule d'une demi-unité par pixel", "[app][camera]") {
  ro::render::Camera camera = CameraAt(300.0F, -50.0F, 0.0F);
  moonclient::DragCamera(camera, CameraDrag{.dy_px = 20.0F, .orbit = true, .zoom = true}, Outdoor(),
                         true);
  CHECK(ZoomOf(camera) == Approx(290.0F));
}

TEST_CASE("Caméra : les remises rendent la vue de départ", "[app][camera]") {
  ro::render::Camera camera = CameraAt(400.0F, -62.0F, 123.0F);
  CHECK(moonclient::ResetCameraZoom(camera, Outdoor()));
  CHECK(moonclient::ResetCameraTilt(camera, Outdoor()));
  CHECK(moonclient::ResetCameraYaw(camera, Outdoor()));
  CHECK(ZoomOf(camera) == Approx(300.0F));
  CHECK(AltitudeOf(camera) == Approx(-50.0F));
  CHECK(YawOf(camera) == Approx(0.0F));
  CHECK(camera.pitch_degrees == Approx(ro::render::kGameElevationDegrees));
}

TEST_CASE("Caméra : dedans, le double-clic seul ne remet pas le lacet", "[app][camera]") {
  ro::render::Camera camera = CameraAt(300.0F, -45.0F, 60.0F);
  CHECK_FALSE(moonclient::ResetCameraYaw(camera, Indoor()));
  CHECK(YawOf(camera) == Approx(60.0F));
}

TEST_CASE("Caméra : une carte à bornes propres refuse les remises, et impose sa vue",
          "[app][camera]") {
  const CameraLimits limits =
      LimitsFor(MapCameraRules{.viewpoint = InfinityRow()}, false, /*bounded=*/true);
  ro::render::Camera camera = CameraAt(300.0F, -50.0F, 10.0F);
  CHECK_FALSE(moonclient::ResetCameraZoom(camera, limits));
  CHECK_FALSE(moonclient::ResetCameraTilt(camera, limits));
  CHECK_FALSE(moonclient::ResetCameraYaw(camera, limits));

  moonclient::ArriveWithCamera(camera, CameraMemory(), limits);
  CHECK(ZoomOf(camera) == Approx(500.0F));
  CHECK(AltitudeOf(camera) == Approx(-65.0F));
  CHECK(YawOf(camera) == Approx(-65.0F));
  moonclient::WheelZoomCamera(camera, /*forward=*/true, limits);
  CHECK(ZoomOf(camera) == Approx(400.0F));
}

TEST_CASE("Caméra : sans bornes, les gestes vont où l'on veut", "[app][camera]") {
  const CameraLimits free = LimitsFor(MapCameraRules{}, false, /*bounded=*/false);
  ro::render::Camera camera = CameraAt(400.0F, -50.0F, 0.0F);
  moonclient::WheelZoomCamera(camera, /*forward=*/true, free);
  CHECK(ZoomOf(camera) > 400.0F);
  moonclient::DragCamera(camera, CameraDrag{.dy_px = -200.0F, .tilt = true}, free, false);
  CHECK(AltitudeOf(camera) > 20.0F);
  // Le zénith reste interdit : la matrice de vue y dégénère.
  moonclient::DragCamera(camera, CameraDrag{.dy_px = 1000.0F, .tilt = true}, free, false);
  CHECK(camera.pitch_degrees == Approx(ro::render::kMaxPitchDegrees));
}

TEST_CASE("Caméra : dehors et dedans se retiennent chacun", "[app][camera]") {
  CameraMemory memory;
  ro::render::Camera camera = CameraAt(380.0F, -60.0F, 90.0F);
  const float outside_yaw = YawOf(camera);
  moonclient::RememberCamera(memory, camera, Outdoor());
  moonclient::ArriveWithCamera(camera, memory, Indoor());
  CHECK(YawOf(camera) == Approx(45.0F));
  CHECK(ZoomOf(camera) == Approx(300.0F));
  moonclient::RememberCamera(memory, camera, Indoor());
  moonclient::ArriveWithCamera(camera, memory, Outdoor());
  CHECK(ZoomOf(camera) == Approx(380.0F));
  CHECK(AltitudeOf(camera) == Approx(-60.0F));
  // 🔴 LE LACET DE L'INTÉRIEUR NE SORT PAS : dehors, la vue reprend celui
  // d'avant l'entrée.
  CHECK(YawOf(camera) == Approx(outside_yaw));
}

TEST_CASE("Caméra : d'un dehors à l'autre, le lacet reste où on l'a mis", "[app][camera]") {
  CameraMemory memory;
  ro::render::Camera camera = CameraAt(380.0F, -60.0F, 90.0F);
  const float turned = YawOf(camera);
  moonclient::RememberCamera(memory, camera, Outdoor());
  moonclient::ArriveWithCamera(camera, memory, Outdoor());
  CHECK(YawOf(camera) == Approx(turned));
}

TEST_CASE("Caméra : les deux tables de l'installation se lisent", "[app][camera]") {
  // Les intérieurs tels que `indoor_maps.yml` les donne, déjà en clés de table.
  const std::vector<std::string> indoor = {"vr_gen", "moc_para0a"};
  const MapCameraTables tables = MapCameraTables::Parse(
      indoor,
      "// nom range scope ...\n"
      "//ex_map02.rsw#\t250#\t250#\t500#\t0#\t90#\t45#\t-40#\t-65#\t-65#\n"
      "1@infi.rsw#            230#\t170#\t500#\t\t  0#\t\t  0#\t   -65#\t\t-50#   -65#   -65#\n"
      "eclage.rsw#     250#    250#    500#  10#  80#  45#  -25#  -60#  -35#\n"
      "abime.rsw#  12#\n");
  CHECK(tables.indoor_count() == 2);
  CHECK(tables.viewpoint_count() == 2);
  CHECK(tables.RulesFor("vr_gen").indoor);
  CHECK(tables.RulesFor("MOC_PARA0A").indoor);
  CHECK_FALSE(tables.RulesFor("prontera").indoor);
  CHECK_FALSE(tables.RulesFor("ex_map02").viewpoint.has_value());
  REQUIRE(tables.RulesFor("1@infi").viewpoint.has_value());
  CHECK(*tables.RulesFor("1@infi").viewpoint == InfinityRow());
  REQUIRE(tables.RulesFor("eclage").viewpoint.has_value());
  CHECK(tables.RulesFor("eclage").viewpoint->altitude_to == -60);
}

TEST_CASE("Caméra : la clé d'une carte retaille le préfixe d'instance", "[app][camera]") {
  CHECK(CameraTableKey("Prontera") == "prontera.rsw");
  CHECK(CameraTableKey("1@infi") == "1@infi.rsw");
  CHECK(CameraTableKey("1@infi.rsw") == "1@infi.rsw");
  CHECK(CameraTableKey("0011@infi") == "1@infi.rsw");
}

TEST_CASE("Caméra : l'œil ne descend pas sous son plancher", "[app][camera]") {
  ro::render::Camera camera;
  camera.look_at = {0.0F, 0.0F, 0.0F};
  camera.distance = 100.0F;
  camera.pitch_degrees = -30.0F;
  CHECK(camera.Position().y < 0.0F);
  camera.eye_floor_height = 5.0F;
  CHECK(camera.Position().y == Approx(5.0F));
}
