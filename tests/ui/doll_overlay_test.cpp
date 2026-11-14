// L'EFFET DU PANTIN : la bulle glisse le rappel de `app` dans sa liste de
// dessin DEUX fois, derrière puis devant le personnage, avec le rectangle où il
// se peint et la passe que chacun peint.

#include <cstring>
#include <vector>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/description_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Un identifiant de texture quelconque : le banc ne peint rien.
constexpr ImTextureID kDollTexture = 7;

// Le rappel, jamais appelé ici : c'est son adresse qu'on retrouve.
void PaintNothing(const ImDrawList* /*list*/, const ImDrawCmd* /*command*/) {}

// Ce que la liste de dessin garde de la bulle, dans l'ordre : les rappels et
// l'image du pantin.
struct Seen {
  std::vector<ro::ui::DollOverlayCall> calls;
  // Le nombre de rappels vus quand l'image est passée.
  std::size_t calls_before_image = 0;
  bool image = false;
};

Seen Look(const ro::ui::DescriptionDollState& doll) {
  ro::ui::UiContext ui;
  ImGui::GetIO().AddMousePosEvent(640.0F, 500.0F);
  // ⚠ DEUX IMAGES : une bulle neuve se cache la première, le temps de se
  // mesurer, et sa liste de dessin n'est pas rendue.
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DrawItemDollTooltip(doll);
    ui.EndFrame();
  }
  Seen seen;
  const ImDrawData* drawn = ImGui::GetDrawData();
  REQUIRE(drawn != nullptr);
  for (const ImDrawList* list : drawn->CmdLists) {
    for (const ImDrawCmd& command : list->CmdBuffer) {
      if (command.UserCallback == &PaintNothing) {
        ro::ui::DollOverlayCall call;
        std::memcpy(&call, command.UserCallbackData, sizeof(call));
        seen.calls.push_back(call);
      } else if (command.UserCallback == nullptr && command.GetTexID() == kDollTexture &&
                 !seen.image) {
        seen.image = true;
        seen.calls_before_image = seen.calls.size();
      }
    }
  }
  return seen;
}

ro::ui::DescriptionDollState Doll() {
  ro::ui::DescriptionDollState doll;
  doll.image = ro::ui::SkinImage{.id = kDollTexture,
                                 .width = ro::ui::kDescriptionDollWidthPixels,
                                 .height = ro::ui::kDescriptionDollHeightPixels};
  static int painter = 0;
  doll.overlay = ro::ui::DollOverlay{.paint = &PaintNothing, .painter = &painter};
  return doll;
}

}  // namespace

TEST_CASE("Pantin : la passe de derrière avant le personnage, celle de devant après",
          "[ui][doll_overlay]") {
  const Seen seen = Look(Doll());
  REQUIRE(seen.calls.size() == 2);
  REQUIRE(seen.image);
  CHECK(seen.calls_before_image == 1);
  CHECK(seen.calls[0].pass == ro::ui::DollOverlayPass::kBehind);
  CHECK(seen.calls[1].pass == ro::ui::DollOverlayPass::kFront);
  // Le rectangle est celui du pantin, à sa taille, pour les deux.
  for (const ro::ui::DollOverlayCall& call : seen.calls) {
    CHECK(call.painter != nullptr);
    CHECK(call.max.x - call.min.x == ro::ui::kDescriptionDollWidthPixels);
    CHECK(call.max.y - call.min.y == ro::ui::kDescriptionDollHeightPixels);
  }
}

TEST_CASE("Pantin : sans effet, aucun rappel", "[ui][doll_overlay]") {
  ro::ui::DescriptionDollState doll = Doll();
  doll.overlay = ro::ui::DollOverlay{};
  const Seen seen = Look(doll);
  CHECK(seen.calls.empty());
  CHECK(seen.image);
}
