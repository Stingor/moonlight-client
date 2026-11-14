// La fenêtre de description d'une compétence : ce que son onglet « Dégâts »
// offre et demande.

#include <cstddef>
#include <cstdint>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/description_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

}  // namespace

TEST_CASE("Description : l'onglet « Dégâts » n'offre que le numéro de monstre",
          "[ui][description]") {
  // 🔴 LE SERVEUR REFUSE LA CIBLE « SOI-MÊME » : la frapper, même en simulation,
  // consommerait ses statuts. La case qui la demandait n'a plus lieu d'être, et
  // la cible demandée est toujours le numéro saisi.
  constexpr std::uint16_t kBoltSkillId = 19;
  constexpr int kPoringId = 1002;
  constexpr int kDamageTabRank = 2;
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  ro::ui::SkillDescriptionWindowState state;
  state.open = true;
  state.skill_id = kBoltSkillId;
  state.damage_target = kPoringId;
  ro::ui::SkillDescriptionView view;
  view.skill_id = kBoltSkillId;
  view.name = "Fire Bolt";

  std::string text;
  const auto frame = [&]() {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::LogToBuffer();
    const ro::ui::SkillDescriptionAction action =
        ro::ui::DrawSkillDescriptionWindow(state, view, art);
    text = GImGui->LogBuffer.c_str();
    ImGui::LogFinish();
    ui.EndFrame();
    return action;
  };
  static_cast<void>(frame());
  // Choisir l'onglet, comme un clic : la barre d'onglets le prend à l'image
  // suivante.
  bool chosen = false;
  for (int rank = 0; rank < GImGui->TabBars.GetMapSize(); ++rank) {
    ImGuiTabBar* bar = GImGui->TabBars.TryGetMapData(rank);
    if (bar != nullptr && bar->Tabs.Size > kDamageTabRank) {
      bar->NextSelectedTabId = bar->Tabs[kDamageTabRank].ID;
      chosen = true;
    }
  }
  REQUIRE(chosen);
  static_cast<void>(frame());
  const ro::ui::SkillDescriptionAction action = frame();

  REQUIRE(state.tab == kDamageTabRank);
  CHECK(text.find("ID monstre") != std::string::npos);
  CHECK(text.find("moi-même") == std::string::npos);
  CHECK(text.find("PvP") == std::string::npos);
  CHECK(action.want_damage == kBoltSkillId);
  CHECK(action.damage_target == static_cast<std::uint32_t>(kPoringId));
}
