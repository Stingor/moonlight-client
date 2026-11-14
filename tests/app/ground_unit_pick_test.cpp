// CE QU'UNE UNITÉ AU SOL OFFRE À LA SOURIS : un bloc d'Ice Wall ou un piège se
// désigne par le carré de sa case, montre l'épée ou la flèche, prend le clic ;
// et l'établi du staff en montre la zone.

#include "world/ground_unit_pick.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"
#include "ro/ui/pick_boxes.h"
#include "ro/ui/staff_tools.h"

#include "world/pick_box_view.h"
#include "world/talkbox_bubbles.h"

using moonclient::GroundUnitClick;
using moonclient::GroundUnitPick;
using moonclient::PickGroundUnit;

namespace {

constexpr std::uint32_t kIceWallUnit = 141;
constexpr std::uint32_t kFireWallUnit = 127;
constexpr std::uint32_t kSkidTrapUnit = 144;
constexpr std::uint32_t kIceBlockId = 880'001;
constexpr std::uint32_t kTrapId = 880'002;
constexpr std::uint32_t kPlayerId = 2'000'001;
constexpr std::uint32_t kMonsterId = 110'000;
constexpr float kViewportWidth = 1024.0F;
constexpr float kViewportHeight = 768.0F;
constexpr float kAspectRatio = kViewportWidth / kViewportHeight;
constexpr glm::vec3 kGround{1500.0F, 0.0F, -900.0F};
constexpr float kCameraDistance = 600.0F;
constexpr float kCameraYawDegrees = 37.0F;
// Le plancher des unités dans cette fenêtre : trente-quatre fois 1024 / 640.
constexpr float kGroundFloorPixels = 54.0F;

[[nodiscard]] ro::render::Camera GameCamera() {
  ro::render::Camera camera;
  camera.look_at = kGround;
  camera.distance = kCameraDistance;
  camera.pitch_degrees = ro::render::kGameElevationDegrees;
  camera.yaw_degrees = kCameraYawDegrees;
  return camera;
}

[[nodiscard]] glm::vec2 FootOf(const ro::render::Camera& camera, glm::vec3 ground) {
  const std::optional<glm::vec2> foot =
      ro::render::ProjectToScreen(camera, kAspectRatio, ground, kViewportWidth, kViewportHeight);
  REQUIRE(foot.has_value());
  return *foot;
}

[[nodiscard]] ro::sim::Actor ActorOf(std::uint32_t id, ro::sim::ActorKind kind) {
  ro::sim::Actor actor;
  actor.id = id;
  actor.kind = kind;
  return actor;
}

}  // namespace

TEST_CASE("Planchers de clic : la largeur de la fenêtre, la famille, le diviseur",
          "[app][ground_unit_pick]") {
  using ro::render::PickFloorPixels;
  // Cent pixels pour un acteur, quatre-vingt-cinq pour une unité, en 1600 de
  // large : la valeur que l'établi de référence relève sur le client.
  CHECK(PickFloorPixels(ro::render::kActorPickFloorFactor, 1600.0F, 0) == 100.0F);
  CHECK(PickFloorPixels(ro::render::kGroundPickFloorFactor, 1600.0F, 0) == 85.0F);
  CHECK(PickFloorPixels(ro::render::kGroundPickFloorFactor, kViewportWidth, 0) ==
        kGroundFloorPixels);
  // Divisé en entier, jamais sous un pixel, et le cran borné à un seizième.
  CHECK(PickFloorPixels(ro::render::kGroundPickFloorFactor, 1600.0F, 1) == 42.0F);
  CHECK(PickFloorPixels(ro::render::kGroundPickFloorFactor, 1600.0F, 9) == 5.0F);
  CHECK(PickFloorPixels(ro::render::kActorPickFloorFactor, 1.0F, 0) == 1.0F);
}

TEST_CASE("Ice Wall : l'emprise est le carré du plancher, posé sur la case",
          "[app][ground_unit_pick]") {
  const ro::render::Camera camera = GameCamera();
  const glm::vec2 foot = FootOf(camera, kGround);
  const std::optional<ro::render::ScreenRect> rect = ro::render::GroundUnitScreenRect(
      camera, kAspectRatio, kGround, kGroundFloorPixels, kViewportWidth, kViewportHeight);
  REQUIRE(rect.has_value());
  // Un carré du plancher, à la moitié entière près du manque, CENTRÉ en largeur
  // sur le point de la case — et non la boîte d'un modèle.
  const glm::vec2 side = rect->max - rect->min;
  CHECK(std::abs(side.x - kGroundFloorPixels) <= 1.0F);
  CHECK(std::abs(side.y - kGroundFloorPixels) <= 1.0F);
  CHECK(std::abs((rect->min.x + rect->max.x) * 0.5F - foot.x) < 0.01F);
  // À peine au-dessus du sol : le bas du rectangle de départ est sur la case.
  const float centre_y = (rect->min.y + rect->max.y) * 0.5F;
  CHECK(centre_y <= foot.y);
  CHECK(foot.y - centre_y < kGroundFloorPixels / 4.0F);
}

TEST_CASE("Ice Wall : un bloc sans modèle se désigne par sa case", "[app][ground_unit_pick]") {
  const ro::render::Camera camera = GameCamera();
  const std::array<GroundUnitPick, 1> picks = {
      GroundUnitPick{.id = kIceBlockId, .unit_id = kIceWallUnit, .ground = kGround}};
  const glm::vec2 foot = FootOf(camera, kGround);
  const std::optional<std::uint32_t> picked =
      PickGroundUnit(camera, kAspectRatio, picks, foot.x, foot.y - 1.0F, kGroundFloorPixels,
                     kViewportWidth, kViewportHeight);
  REQUIRE(picked.has_value());
  CHECK(*picked == kIceBlockId);
  // Juste hors du carré, rien : la moitié du plancher plus deux pixels.
  CHECK_FALSE(PickGroundUnit(camera, kAspectRatio, picks, foot.x + kGroundFloorPixels / 2.0F + 2.0F,
                             foot.y - 1.0F, kGroundFloorPixels, kViewportWidth, kViewportHeight)
                  .has_value());
}

TEST_CASE("Les unités désignables : visibles et de la liste du client", "[app][ground_unit_pick]") {
  ro::sim::World world;
  world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kIceBlockId, .unit_id = kIceWallUnit});
  world.PlaceSkillUnit(ro::sim::SkillUnit{.id = kTrapId, .unit_id = kSkidTrapUnit});
  CHECK(moonclient::GroundUnitOffersPick(world, kIceBlockId));
  CHECK(moonclient::GroundUnitOffersPick(world, kTrapId));
  // Un mur de feu laisse passer la souris ; une unité cachée n'offre rien.
  world.PlaceSkillUnit(
      ro::sim::SkillUnit{.id = kTrapId, .unit_id = kSkidTrapUnit, .visible = false});
  CHECK_FALSE(moonclient::GroundUnitOffersPick(world, kTrapId));
  CHECK_FALSE(moonclient::GroundUnitTakesPick(kFireWallUnit));
}

TEST_CASE("L'épée : sur le bloc de glace partout, sur un piège en siège seulement",
          "[app][ground_unit_pick]") {
  CHECK(moonclient::GroundUnitTakesPick(kIceWallUnit));
  CHECK(moonclient::GroundUnitClickAttacks(kIceWallUnit));
  CHECK(moonclient::GroundUnitShowsSword(kIceWallUnit, /*siege_map=*/false));
  CHECK(moonclient::GroundUnitTakesPick(kSkidTrapUnit));
  CHECK_FALSE(moonclient::GroundUnitClickAttacks(kSkidTrapUnit));
  CHECK_FALSE(moonclient::GroundUnitShowsSword(kSkidTrapUnit, /*siege_map=*/false));
  CHECK(moonclient::GroundUnitShowsSword(kSkidTrapUnit, /*siege_map=*/true));
}

TEST_CASE("Une unité passe devant tout acteur, sauf un monstre", "[app][ground_unit_pick]") {
  const ro::sim::Actor player = ActorOf(kPlayerId, ro::sim::ActorKind::kPlayer);
  const ro::sim::Actor npc = ActorOf(kPlayerId, ro::sim::ActorKind::kNonPlayer);
  const ro::sim::Actor monster = ActorOf(kMonsterId, ro::sim::ActorKind::kMonster);
  CHECK(moonclient::GroundUnitOutranks(nullptr));
  CHECK(moonclient::GroundUnitOutranks(&player));
  CHECK(moonclient::GroundUnitOutranks(&npc));
  CHECK_FALSE(moonclient::GroundUnitOutranks(&monster));
}

TEST_CASE("Le clic sur un piège s'arrête sur lui ; sur la glace, il frappe",
          "[app][ground_unit_pick]") {
  using moonclient::GroundUnitClickOf;
  using moonclient::SkillAim;
  // Un piège hors siège : ni marche ni frappe — le clic s'arrête là.
  CHECK(GroundUnitClickOf(kSkidTrapUnit, nullptr, SkillAim::kNothing) ==
        GroundUnitClick::kSwallowed);
  CHECK(GroundUnitClickOf(kIceWallUnit, nullptr, SkillAim::kNothing) == GroundUnitClick::kAttack);
  // Un monstre par-dessus prend le clic ; une compétence armée le garde.
  const ro::sim::Actor monster = ActorOf(kMonsterId, ro::sim::ActorKind::kMonster);
  CHECK(GroundUnitClickOf(kSkidTrapUnit, &monster, SkillAim::kNothing) ==
        GroundUnitClick::kNotTaken);
  CHECK(GroundUnitClickOf(kIceWallUnit, nullptr, SkillAim::kTarget) == GroundUnitClick::kNotTaken);
  CHECK(GroundUnitClickOf(kSkidTrapUnit, nullptr, SkillAim::kGround) == GroundUnitClick::kNotTaken);
  CHECK(GroundUnitClickOf(std::nullopt, nullptr, SkillAim::kNothing) == GroundUnitClick::kNotTaken);
}

TEST_CASE("Talkie Box : le message déclenché flotte au-dessus de sa case, dans sa couleur",
          "[app][ground_unit_pick]") {
  constexpr std::uint32_t kTalkieBoxUnit = 153;
  constexpr std::uint32_t kSaidAtMs = 1000;
  constexpr std::uint32_t kLaterMs = 1500;
  ro::sim::World world;
  ro::sim::SkillUnit box;
  box.id = kTrapId;
  box.unit_id = kTalkieBoxUnit;
  world.PlaceSkillUnit(box);
  REQUIRE(world.SaySkillUnit(kTrapId, "Coucou", kSaidAtMs));

  const ro::render::Camera camera = GameCamera();
  const std::array<GroundUnitPick, 1> speakers = {
      GroundUnitPick{.id = kTrapId, .unit_id = kTalkieBoxUnit, .ground = kGround}};
  ro::ui::ChatBubbleLook look;
  look.client_timing = true;
  const std::vector<ro::ui::ChatBubbleView> bubbles = moonclient::TalkboxBubblesOnScreen(
      camera, kAspectRatio, speakers, world, kLaterMs, kViewportWidth, kViewportHeight, look);
  REQUIRE(bubbles.size() == 1);
  CHECK(bubbles[0].text == "Coucou");
  CHECK(bubbles[0].text_color == moonclient::kTalkboxBubbleColor);
  CHECK(bubbles[0].elapsed_ms == static_cast<float>(kLaterMs - kSaidAtMs));
  // Au-dessus du point de la case, d'une part fixe de la hauteur d'écran.
  const glm::vec2 foot = FootOf(camera, kGround);
  CHECK(bubbles[0].head.x == foot.x);
  CHECK(std::abs(bubbles[0].head.y -
                 (foot.y - kViewportHeight * moonclient::kTalkboxBubbleLiftOn480 /
                               moonclient::kTalkboxBubbleReferenceHeight)) < 1e-3F);

  // Passé les cinq secondes du client, elle ne paraît plus.
  CHECK(moonclient::TalkboxBubblesOnScreen(
            camera, kAspectRatio, speakers, world,
            kSaidAtMs + static_cast<std::uint32_t>(ro::ui::kClientBubbleMs), kViewportWidth,
            kViewportHeight, look)
            .empty());
}

TEST_CASE("Remove Trap et Spring Trap armés : le clic sur un piège le désigne",
          "[app][ground_unit_pick]") {
  using moonclient::GroundUnitClickOf;
  using moonclient::SkillAim;
  // 🔴 LA VISÉE D'UN PIÈGE PREND L'UNITÉ SOUS LE CLIC, quelle qu'elle soit : un
  // piège hors siège, que le clic ordinaire avale, comme un bloc de glace.
  CHECK(GroundUnitClickOf(kSkidTrapUnit, nullptr, SkillAim::kTrap) == GroundUnitClick::kCastSkill);
  CHECK(GroundUnitClickOf(kIceWallUnit, nullptr, SkillAim::kTrap) == GroundUnitClick::kCastSkill);
  // Un monstre par-dessus reste devant, et rien sous le clic ne désigne rien.
  const ro::sim::Actor monster = ActorOf(kMonsterId, ro::sim::ActorKind::kMonster);
  CHECK(GroundUnitClickOf(kSkidTrapUnit, &monster, SkillAim::kTrap) == GroundUnitClick::kNotTaken);
  CHECK(GroundUnitClickOf(std::nullopt, nullptr, SkillAim::kTrap) == GroundUnitClick::kNotTaken);
}

TEST_CASE("Le cadre de la case se peint sous les unités au sol, après les zones",
          "[app][ground_unit_pick]") {
  using moonclient::GroundPaint;
  using moonclient::PaintsBefore;
  // La planche d'un piège recouvre le cadre de sa case : il se peignait après.
  CHECK(PaintsBefore(GroundPaint::kCellCursor, GroundPaint::kGroundUnits));
  CHECK_FALSE(PaintsBefore(GroundPaint::kGroundUnits, GroundPaint::kCellCursor));
  CHECK(PaintsBefore(GroundPaint::kSkillAreas, GroundPaint::kCellCursor));
  // Le message d'un Graffiti reste sous tout ce qu'on pose sur le sol.
  CHECK(PaintsBefore(GroundPaint::kGraffiti, GroundPaint::kSkillAreas));
  CHECK(PaintsBefore(GroundPaint::kGreyWorld, GroundPaint::kGraffiti));
  // La trace du guidage : sur les zones, sous le cadre et sous les unités.
  CHECK(PaintsBefore(GroundPaint::kSkillAreas, GroundPaint::kNaviTrail));
  CHECK(PaintsBefore(GroundPaint::kNaviTrail, GroundPaint::kCellCursor));
  CHECK(PaintsBefore(GroundPaint::kNaviTrail, GroundPaint::kGroundUnits));
}

TEST_CASE("Le cadre de la case reste où il était sous l'emprise d'une unité",
          "[app][ground_unit_pick]") {
  std::optional<ro::sim::CellPosition> held;
  const ro::sim::CellPosition before{.x = 30, .y = 24};
  const ro::sim::CellPosition under{.x = 31, .y = 24};
  CHECK(moonclient::CellCursorOf(before, /*over_unit=*/false, held) == before);
  // Sur le piège, la souris désigne sa case, mais le cadre ne la suit pas.
  CHECK(moonclient::CellCursorOf(under, /*over_unit=*/true, held) == before);
  CHECK(moonclient::CellCursorOf(under, /*over_unit=*/false, held) == under);
}

TEST_CASE("Établi : le piège a sa zone orange, désignée en jaune avec son étiquette",
          "[app][ground_unit_pick][staff]") {
  const ro::render::Camera camera = GameCamera();
  ro::sim::World world;
  world.set_player_id(kPlayerId);
  world.Spawn(ActorOf(kPlayerId, ro::sim::ActorKind::kPlayer));
  const std::array<GroundUnitPick, 2> picks = {
      GroundUnitPick{.id = kTrapId, .unit_id = kSkidTrapUnit, .ground = kGround},
      GroundUnitPick{
          .id = kIceBlockId, .unit_id = kIceWallUnit, .ground = kGround + glm::vec3{50, 0, 0}}};
  const glm::vec2 foot = FootOf(camera, kGround);
  const moonclient::PickScene scene{.ground_units = picks,
                                    .ground_unit_floor_pixels = kGroundFloorPixels};
  const ro::ui::PickBoxesView view =
      moonclient::PickBoxesOnScreen(camera, kAspectRatio, scene, world, std::nullopt,
                                    foot - glm::vec2{0.0F, 1.0F}, kViewportWidth, kViewportHeight);
  REQUIRE(view.boxes.size() == 2);
  REQUIRE(view.hovered.has_value());
  const ro::ui::PickBoxView& trap = view.boxes[*view.hovered];
  CHECK(trap.id == kTrapId);
  CHECK(trap.job == kSkidTrapUnit);
  CHECK(trap.category == ro::ui::PickCategory::kSkillUnit);
  CHECK(ro::ui::PickBoxTag(trap) == "AID 880002  job 144  cat 2");
  CHECK(ro::ui::PickBoxColor(view, *view.hovered) ==
        std::array<std::uint8_t, 4>{255, 235, 60, 255});
  const std::size_t other = *view.hovered == 0 ? 1 : 0;
  CHECK(ro::ui::PickBoxColor(view, other) == std::array<std::uint8_t, 4>{255, 170, 70, 200});
  CHECK(ro::ui::PickBoxCount(view) == "2 zones — 1 sous le curseur");
}

TEST_CASE("Établi : le réglage des zones règle la vraie désignation",
          "[app][ground_unit_pick][staff]") {
  ro::ui::PickTuningLook look;
  // Le client tel quel : cent et quatre-vingt-cinq pixels en 1600 de large.
  ro::ui::PickTuning tuning = ro::ui::PickTuningOf(look, /*staff_tools=*/false, 1600.0F, 0);
  CHECK(tuning.actor_floor_pixels == 100.0F);
  CHECK(tuning.ground_floor_pixels == 85.0F);
  // 🔴 LE DIVISEUR EST AU JOUEUR : il vaut sans le droit de l'établi.
  tuning = ro::ui::PickTuningOf(look, /*staff_tools=*/false, 1600.0F, 1);
  CHECK(tuning.actor_floor_pixels == 50.0F);
  CHECK(tuning.ground_floor_pixels == 42.0F);
  // Le plancher FORCÉ, lui, reste à l'établi.
  look.actor_floor_forced = true;
  look.actor_floor_pixels = 12;
  CHECK(ro::ui::PickTuningOf(look, false, 1600.0F, 0).actor_floor_pixels == 100.0F);
  CHECK(ro::ui::PickTuningOf(look, true, 1600.0F, 0).actor_floor_pixels == 12.0F);
  // Le correctif des numéros négatifs vaut pour tous : un quart par défaut.
  CHECK(ro::ui::PickTuningOf(look, false, 1600.0F, 0).negative_id_share == 0.25F);

  // Posé sur la scène : l'objet au sol prend le plancher des unités, l'acteur
  // au numéro négatif sa part.
  ro::sim::World world;
  const auto ghost_id = static_cast<std::uint32_t>(-static_cast<std::int32_t>(kPlayerId));
  world.Spawn(ActorOf(kPlayerId, ro::sim::ActorKind::kPlayer));
  world.Spawn(ActorOf(kMonsterId, ro::sim::ActorKind::kItem));
  world.Spawn(ActorOf(ghost_id, ro::sim::ActorKind::kMonster));
  std::vector<ro::render::DrawnActor> drawn(3);
  const std::array<std::uint32_t, 3> ids = {kPlayerId, kMonsterId, ghost_id};
  moonclient::ApplyPickTuning(tuning, drawn, ids, world);
  CHECK(drawn[0].pick_floor_pixels == tuning.actor_floor_pixels);
  CHECK(drawn[0].pick_share == 1.0F);
  CHECK(drawn[1].pick_floor_pixels == tuning.ground_floor_pixels);
  CHECK(drawn[2].pick_share == tuning.negative_id_share);
}
