// L'INSPECTEUR D'ENTITÉ, dessiné hors jeu.
//
// Ce qui se vérifie sans écran : fermé il ne dessine rien, chaque phase se
// dessine sans rien redemander, et des titres de section entre les lignes ne
// cassent pas le tableau.

#include "ro/ui/entity_inspector_window.h"

#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::EntityInspectorAnswer;
using ro::ui::EntityInspectorPhase;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
constexpr const char* kWindowName = "###inspecteur_entite";

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

struct InspectorProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = PlainArt();
  ro::ui::EntityInspectorWindowState state;
  ro::ui::EntityInspectorView view;

  InspectorProbe() {
    state.open = true;
    view.subject = "Joueur — Alice (AID 2000123)";
    view.phase = EntityInspectorPhase::kReady;
    view.rows = {{.key = "Serveur", .value = ""},
                 {.key = "Type", .value = "BL_PC (joueur)"},
                 {.key = "Carte", .value = "prontera (150, 180)"},
                 {.key = "Personnage", .value = ""},
                 {.key = "Nom", .value = "Alice"}};
  }

  EntityInspectorAnswer Frame() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    const EntityInspectorAnswer answer = ro::ui::DrawEntityInspectorWindow(state, view, art);
    ui.EndFrame();
    return answer;
  }
};

}  // namespace

TEST_CASE("Inspecteur : fermé, rien ne se dessine", "[ui][entity_inspector]") {
  InspectorProbe probe;
  probe.state.open = false;
  for (int frame = 0; frame < 3; ++frame) {
    CHECK_FALSE(probe.Frame().refresh);
  }
  CHECK(ImGui::FindWindowByName(kWindowName) == nullptr);
}

TEST_CASE("Inspecteur : chaque phase se dessine sans rien redemander", "[ui][entity_inspector]") {
  InspectorProbe probe;
  for (const EntityInspectorPhase phase :
       {EntityInspectorPhase::kAsking, EntityInspectorPhase::kSilent, EntityInspectorPhase::kDenied,
        EntityInspectorPhase::kMissing, EntityInspectorPhase::kReady}) {
    probe.view.phase = phase;
    for (int frame = 0; frame < 3; ++frame) {
      CHECK_FALSE(probe.Frame().refresh);
    }
  }
  CHECK(ImGui::FindWindowByName(kWindowName) != nullptr);
  CHECK(probe.state.open);
}

TEST_CASE("Inspecteur : une réponse vide ou sans titre se dessine aussi",
          "[ui][entity_inspector]") {
  InspectorProbe probe;
  probe.view.rows = {{.key = "Nom", .value = "Alice"}, {.key = "Etat", .value = "assis"}};
  CHECK_FALSE(probe.Frame().refresh);
  probe.view.rows.clear();
  CHECK_FALSE(probe.Frame().refresh);
  CHECK(probe.state.open);
}

TEST_CASE("Inspecteur : une nouvelle cible remonte la fenêtre", "[ui][entity_inspector]") {
  InspectorProbe probe;
  probe.state.raise = true;
  static_cast<void>(probe.Frame());
  CHECK_FALSE(probe.state.raise);
}
