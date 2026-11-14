// LES EFFETS DE CHAPEAU DU PORTRAIT DE STATUT : le cadre du portrait glisse le
// rappel de `app` dans sa liste de dessin DEUX fois, derrière puis devant
// l'image, dans le rectangle même où l'image tombe — l'intérieur du cadre.

#include <cstddef>
#include <cstring>
#include <vector>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/align_grid.h"
#include "ro/ui/doll_overlay.h"
#include "ro/ui/hud_frame.h"
#include "ro/ui/status_portrait.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Un identifiant de texture quelconque : le banc ne peint rien.
constexpr ImTextureID kPortraitTexture = 11;
// Le côté de l'image : le cadre d'origine, moins un pixel de chaque côté.
constexpr float kImageSide = 98.0F;

// Le rappel, jamais appelé ici : c'est son adresse qu'on retrouve.
void PaintNothing(const ImDrawList* /*list*/, const ImDrawCmd* /*command*/) {}

struct Seen {
  std::vector<ro::ui::DollOverlayCall> calls;
  // Le nombre de rappels vus quand l'image est passée.
  std::size_t calls_before_image = 0;
  bool image = false;
  // Le rectangle de l'image, en coordonnées d'écran de l'interface.
  ImVec2 image_min{0.0F, 0.0F};
  ImVec2 image_max{0.0F, 0.0F};
};

Seen Look(const ro::ui::StatusPortraitView& view) {
  ro::ui::UiContext ui;
  ro::ui::StatusPortraitLook look;
  // Le portrait seul : les étiquettes, éteintes, n'ont rien à voir ici.
  look.group(ro::ui::PortraitGroup::kHead).shown = true;
  const ro::ui::AlignGrid grid;
  ro::ui::HudGrip grip;
  ro::ui::StatusPortraitAnswer answer;
  // ⚠ DEUX IMAGES : une fenêtre neuve peut se cacher la première, le temps de
  // se mesurer.
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ro::ui::DrawStatusPortrait(view, grid, look, grip, answer);
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
      } else if (command.UserCallback == nullptr && command.TexRef._TexData == nullptr &&
                 command.TexRef._TexID == kPortraitTexture && !seen.image) {
        // ⚠ LE CHAMP, ET NON `GetTexID()` : le fond du cadre désigne l'atlas de
        // la police, qu'aucun renderer n'a téléversé sur ce banc, et `GetTexID`
        // s'arrête alors sur une assertion.
        seen.image = true;
        seen.calls_before_image = seen.calls.size();
        // Le quad de l'image : ses deux coins opposés, dans l'ordre d'`AddImage`.
        const ImDrawVert* corners = list->VtxBuffer.Data + command.VtxOffset;
        const ImDrawIdx* indices = list->IdxBuffer.Data + command.IdxOffset;
        seen.image_min = corners[indices[0]].pos;
        seen.image_max = corners[indices[2]].pos;
      }
    }
  }
  return seen;
}

ro::ui::StatusPortraitView Portrait() {
  ro::ui::StatusPortraitView view;
  view.present = true;
  view.portrait =
      ro::ui::SkinImage{.id = kPortraitTexture, .width = kImageSide, .height = kImageSide};
  static int painter = 0;
  view.overlay = ro::ui::DollOverlay{.paint = &PaintNothing, .painter = &painter};
  return view;
}

}  // namespace

TEST_CASE("Portrait : l'effet passe derrière puis devant l'image, dans son rectangle",
          "[ui][portrait][doll_overlay]") {
  const Seen seen = Look(Portrait());
  REQUIRE(seen.calls.size() == 2);
  REQUIRE(seen.image);
  CHECK(seen.calls_before_image == 1);
  CHECK(seen.calls[0].pass == ro::ui::DollOverlayPass::kBehind);
  CHECK(seen.calls[1].pass == ro::ui::DollOverlayPass::kFront);
  // 🔴 LE MÊME RECTANGLE QUE L'IMAGE : c'est celui où le cadrage a été calculé,
  // et donc celui où l'effet doit tomber — et se découper.
  for (const ro::ui::DollOverlayCall& call : seen.calls) {
    CHECK(call.painter != nullptr);
    CHECK(call.min.x == seen.image_min.x);
    CHECK(call.min.y == seen.image_min.y);
    CHECK(call.max.x == seen.image_max.x);
    CHECK(call.max.y == seen.image_max.y);
    CHECK(call.max.x - call.min.x == kImageSide);
    CHECK(call.max.y - call.min.y == kImageSide);
  }
}

TEST_CASE("Portrait : un corps changé par un effet remplace l'image",
          "[ui][portrait][doll_overlay]") {
  ro::ui::StatusPortraitView view = Portrait();
  view.body_overlaid = true;
  const Seen seen = Look(view);
  CHECK(seen.calls.size() == 2);
  CHECK_FALSE(seen.image);
}

TEST_CASE("Portrait : sans effet, aucun rappel et l'image seule", "[ui][portrait][doll_overlay]") {
  ro::ui::StatusPortraitView view = Portrait();
  view.overlay = ro::ui::DollOverlay{};
  // Un drapeau resté levé ne cache rien sans rappel pour peindre le corps.
  view.body_overlaid = true;
  const Seen seen = Look(view);
  CHECK(seen.calls.empty());
  CHECK(seen.image);
}
