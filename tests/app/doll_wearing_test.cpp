// CE QUE PORTE LE PORTEUR D'UN PANTIN, et où son effet tombe : ce qui fait
// renaître le porteur — corps montré, monture, carte, ensemble —, rien sur un
// modèle 3D, et le cadrage imposé du portrait transmis jusqu'à la caméra de
// l'effet.

#include "items/doll_wearing.h"

#include <cstdint>
#include <string_view>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/portrait_framing.h"
#include "ro/render/shown_job.h"
#include "ro/render/sprite_billboard.h"
#include "ro/render/sprite_vignette.h"
#include "ro/sim/actor.h"

#include "effects/doll_camera.h"

namespace {

using Catch::Approx;
using moonclient::DollCanvas;
using moonclient::DollWearing;
using ro::sim::HatEffectId;

// Trois costumes.
constexpr HatEffectId kRing = 7;
constexpr HatEffectId kAura = 31;
constexpr HatEffectId kRain = 120;

constexpr std::uint32_t kKnight = 7;
constexpr std::uint32_t kPecoKnight = 13;
constexpr std::string_view kProntera = "prontera";
constexpr std::string_view kGeffen = "geffen";

ro::sim::Actor Knight() {
  ro::sim::Actor knight;
  knight.id = 150'001;
  knight.kind = ro::sim::ActorKind::kPlayer;
  knight.look.job = static_cast<std::uint16_t>(kKnight);
  knight.hat_effects = {kRing, kAura};
  return knight;
}

// Une case quelconque d'une carte, loin de l'origine du monde.
constexpr glm::vec3 kGround(812.5F, -14.0F, -377.5F);

// Le cadre d'origine du portrait, moins un pixel de chaque côté.
constexpr float kInset = 1.0F;
constexpr glm::vec2 kDrawnBox(98.0F, 98.0F);
constexpr float kHalf = 0.5F;
constexpr float kEdgesPerAxis = 2.0F;
// Une tête de vingt pixels de sprite sur un corps de trente, pieds à l'origine.
constexpr float kHeadCentreAboveFeetPx = 40.0F;

ro::render::PortraitBoxes Character() {
  ro::render::PortraitBoxes boxes;
  boxes.head =
      ro::render::SpriteBounds{.min = glm::vec2(-10.0F, -50.0F), .max = glm::vec2(10.0F, -30.0F)};
  boxes.body =
      ro::render::SpriteBounds{.min = glm::vec2(-15.0F, -30.0F), .max = glm::vec2(15.0F, 0.0F)};
  boxes.has_head = true;
  boxes.has_body = true;
  return boxes;
}

// Où un point du monde tombe sur la toile, en pixels depuis le coin haut-gauche.
glm::vec2 OnCanvas(const ro::render::Camera& camera, glm::vec2 canvas, glm::vec3 point) {
  const glm::vec4 clip =
      camera.ProjectionMatrix(canvas.x / canvas.y) * camera.ViewMatrix() * glm::vec4(point, 1.0F);
  const glm::vec2 ndc(clip.x / clip.w, clip.y / clip.w);
  return {(ndc.x + 1.0F) * kHalf * canvas.x, (1.0F - ndc.y) * kHalf * canvas.y};
}

}  // namespace

TEST_CASE("Portrait : aucun effet sur une classe en modèle 3D", "[doll_wearing]") {
  ro::sim::Actor self = Knight();
  CHECK(moonclient::PortraitHatEffects(self, true).empty());
  CHECK(moonclient::PortraitHatEffects(self, false).size() == self.hat_effects.size());
}

TEST_CASE("Pantin : monter son Peco fait renaître le porteur, au corps monté", "[doll_wearing]") {
  ro::sim::Actor knight = Knight();
  const DollWearing on_foot = moonclient::WearingOf(knight, {kRing, kAura}, kProntera);
  CHECK(on_foot.shown_job == kKnight);
  knight.option = ro::render::kOptionPeco;
  const DollWearing mounted = moonclient::WearingOf(knight, {kRing, kAura}, kProntera);
  // 🔴 LE CORPS MONTRÉ A CHANGÉ, le métier exercé non : l'ancre d'un chapeau
  // monte avec la monture, et le porteur doit renaître pour la suivre.
  CHECK(mounted.shown_job == kPecoKnight);
  CHECK(knight.look.job == kKnight);
  CHECK(mounted != on_foot);
}

TEST_CASE("Pantin : la monture du shop, la carte et l'ensemble font renaître le porteur",
          "[doll_wearing]") {
  ro::sim::Actor knight = Knight();
  const DollWearing before = moonclient::WearingOf(knight, {kRing, kAura}, kProntera);
  // Rien n'a bougé : le porteur reste, ses effets continuent de jouer.
  CHECK(moonclient::WearingOf(knight, {kRing, kAura}, kProntera) == before);
  // Un effet ajouté ou retiré : il renaît, rien ne se greffe.
  CHECK(moonclient::WearingOf(knight, {kRing, kAura, kRain}, kProntera) != before);
  CHECK(moonclient::WearingOf(knight, {kRing}, kProntera) != before);
  CHECK(moonclient::WearingOf(knight, {kRing, kAura}, kGeffen) != before);
  knight.status_effects.insert(ro::sim::kStatusAllRiding);
  const DollWearing riding = moonclient::WearingOf(knight, {kRing, kAura}, kProntera);
  CHECK(riding.mounted);
  CHECK(riding != before);
}

TEST_CASE("Pantin : la marge de la toile déplace les pieds, pas l'échelle", "[doll_wearing]") {
  ro::render::VignetteFraming framing;
  framing.scale = 1.5F;
  framing.origin = glm::vec2(40.0F, 70.0F);
  const DollCanvas wide{.box_px = kDrawnBox, .margin_boxes = 1.0F};
  const ro::render::VignetteFraming moved = moonclient::OnDollCanvas(framing, wide);
  CHECK(moved.scale == framing.scale);
  CHECK(moved.origin == framing.origin + kDrawnBox);
  CHECK(wide.size_px() == kDrawnBox * (1.0F + kEdgesPerAxis * wide.margin_boxes));
  // Sans marge, la toile est la boîte, et rien ne bouge.
  const DollCanvas tight{.box_px = kDrawnBox, .margin_boxes = 0.0F};
  CHECK(moonclient::OnDollCanvas(framing, tight).origin == framing.origin);
  CHECK(tight.size_px() == kDrawnBox);
  CHECK_FALSE(DollCanvas().usable());
}

TEST_CASE("Portrait : l'effet suit le cadrage IMPOSÉ, tête seule, zoom et décalage compris",
          "[doll_wearing]") {
  ro::render::PortraitAim aim;
  aim.head_only = true;
  aim.zoom = 1.5F;
  aim.offset = glm::vec2(0.25F, -0.125F);
  aim.inset_px = kInset;
  const ro::render::VignetteFraming framing =
      ro::render::FramePortrait(Character(), aim, kDrawnBox);
  REQUIRE(framing.usable());
  const DollCanvas canvas{.box_px = kDrawnBox, .margin_boxes = 0.0F};
  const ro::render::Camera camera = moonclient::DollCamera(
      kGround, moonclient::OnDollCanvas(framing, canvas), canvas.size_px().x, canvas.size_px().y);
  // Le sol du porteur tombe sur les pieds du personnage cadré…
  const glm::vec2 feet = OnCanvas(camera, canvas.size_px(), kGround);
  CHECK(feet.x == Approx(framing.origin.x).margin(0.01));
  CHECK(feet.y == Approx(framing.origin.y).margin(0.01));
  // … et le centre de sa tête, là où le cadre la vise : au centre du cadre
  // entier, décalé en tailles de ce cadre, compté depuis le coin de la boîte
  // dessinée.
  const ro::render::BillboardBasis basis = ro::render::MakeBillboardBasis(camera);
  const glm::vec3 head =
      kGround + basis.up * (kHeadCentreAboveFeetPx / ro::render::kSpritePixelsPerWorldUnit);
  const glm::vec2 frame = kDrawnBox + glm::vec2(kEdgesPerAxis * kInset);
  const glm::vec2 aimed = frame * (glm::vec2(kHalf) + aim.offset) - glm::vec2(kInset);
  const glm::vec2 seen = OnCanvas(camera, canvas.size_px(), head);
  CHECK(seen.x == Approx(aimed.x).margin(0.05));
  CHECK(seen.y == Approx(aimed.y).margin(0.05));
}
