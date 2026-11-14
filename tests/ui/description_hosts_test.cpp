// LES LIGNES D'UNE DESCRIPTION SE DESSINENT DANS CHAQUE HÔTE : la fenêtre,
// l'aperçu au survol, l'aperçu posé dans un panneau, et la fiche dans le
// panneau de la base de données. Le bloc de faits de l'item_db voyage par ces
// lignes : ajouté, chaque hôte dessine davantage.

#include <array>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

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
constexpr int kSettleFrames = 3;
constexpr std::uint32_t kSword = 1101;
constexpr float kPanelPixels = 600.0F;

[[nodiscard]] ro::ui::SkinArt PlainArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  return art;
}

const std::vector<std::string> kTableLines = {"A basic one-handed sword.",
                                              "^0000CCWeight:^000000 50"};
const std::vector<std::string> kBlockLines = {
    "________________________",
    "^777777Données du serveur^000000",
    "^0000CCType :^000000 Épée à une main",
    "^0000CCAttaque :^000000 25",
    "^0000CCPoids :^000000 50",
};

// Les sommets que la dernière image a produits, l'hôte posé `kSettleFrames`
// fois pour que sa taille se fasse.
[[nodiscard]] int DrawnVertices(const std::vector<std::string>& lines,
                                const std::function<void(const ro::ui::DescriptionView&)>& host) {
  ro::ui::UiContext ui;
  ro::ui::DescriptionView view;
  view.item_id = kSword;
  view.name = "Sword";
  view.lines = lines;
  int vertices = 0;
  for (int frame = 0; frame < kSettleFrames; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    host(view);
    ui.EndFrame();
    vertices = ImGui::GetDrawData() != nullptr ? ImGui::GetDrawData()->TotalVtxCount : 0;
  }
  return vertices;
}

[[nodiscard]] std::vector<std::string> WithBlock() {
  std::vector<std::string> lines = kTableLines;
  lines.insert(lines.end(), kBlockLines.begin(), kBlockLines.end());
  return lines;
}

// Un panneau assez grand pour tout montrer, comme celui de la base.
void InPanel(const char* id, const std::function<void()>& body) {
  ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
  ImGui::SetNextWindowSize(ImVec2(kPanelPixels, kPanelPixels));
  if (ImGui::Begin(id)) {
    body();
  }
  ImGui::End();
}

}  // namespace

TEST_CASE("Aperçu : un texte trop haut pour l'écran s'ouvre en colonnes", "[ui][description]") {
  ro::ui::UiContext ui;
  const ro::ui::SkinArt art = PlainArt();
  constexpr ro::ui::Viewport kScreen{.width_px = 1280.0F, .height_px = 720.0F};
  constexpr float kOneFrame = 1.0F / 60.0F;
  // Plus qu'une colonne n'en tient à cet écran, moins que les trois qu'il permet.
  constexpr int kManyLines = 70;
  std::vector<std::string> lines;
  for (int rank = 0; rank < kManyLines; ++rank) {
    lines.push_back("Ligne " + std::to_string(rank));
  }
  ro::ui::DescriptionView view;
  view.item_id = 570008;
  view.name = "Guitare";
  view.lines = lines;

  const auto bubble_after_frames = [&] {
    // ⚠ UNE BULLE NE CONNAÎT LA TAILLE DE SON CONTENU QU'À L'IMAGE SUIVANTE.
    for (int frame = 0; frame < 3; ++frame) {
      ui.BeginFrame(kScreen, kOneFrame);
      ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
      ImGui::Begin("porteur");
      ro::ui::DrawDescriptionPreview(view, art);
      ImGui::End();
      ui.EndFrame();
    }
    return ImGui::FindWindowByName("##Tooltip_00");
  };

  const ImGuiWindow* long_bubble = bubble_after_frames();
  REQUIRE(long_bubble != nullptr);
  CHECK(long_bubble->Size.x > ro::ui::kDescriptionPreviewWidest);
  CHECK(long_bubble->Size.x <= kScreen.width_px * ro::ui::kDescriptionPreviewScreenShare);
  CHECK(long_bubble->Size.y <= kScreen.height_px * ro::ui::kDescriptionPreviewScreenShare);
  // 🔴 TOUT SE VOIT : sans colonnes, le bas du texte serait sous un ascenseur inerte.
  CHECK_FALSE(long_bubble->ScrollbarY);

  const std::vector<std::string> short_lines = {"Ligne courte"};
  view.lines = short_lines;
  const ImGuiWindow* short_bubble = bubble_after_frames();
  REQUIRE(short_bubble != nullptr);
  CHECK(short_bubble->Size.x <= ro::ui::kDescriptionPreviewWidest);
}

TEST_CASE("Hôtes de description : chacun dessine les lignes ajoutées", "[ui][description]") {
  const ro::ui::SkinArt art = PlainArt();
  const std::array<std::function<void(const ro::ui::DescriptionView&)>, 4> hosts = {
      // La fenêtre de description.
      [&art](const ro::ui::DescriptionView& view) {
        ro::ui::DescriptionWindowState state;
        state.open = true;
        state.item_id = view.item_id;
        static_cast<void>(ro::ui::DrawDescriptionWindow(state, view, art));
      },
      // L'aperçu au survol.
      [&art](const ro::ui::DescriptionView& view) { ro::ui::DrawDescriptionPreview(view, art); },
      // L'aperçu posé dans un panneau.
      [&art](const ro::ui::DescriptionView& view) {
        InPanel("apercu", [&] { ro::ui::DrawDescriptionPreviewPanel("##apercu", view, art); });
      },
      // La fiche dans le panneau de la base.
      [&art](const ro::ui::DescriptionView& view) {
        InPanel("base", [&] {
          ro::ui::DescriptionWindowState state;
          ro::ui::DescriptionWindowAction action;
          ro::ui::DrawItemSheetBody(state, view, art, ro::ui::ItemSheetHost::kDatabase, action);
        });
      },
  };
  const std::vector<std::string> plain = kTableLines;
  const std::vector<std::string> block = WithBlock();
  for (std::size_t rank = 0; rank < hosts.size(); ++rank) {
    INFO("hôte " << rank);
    const int without = DrawnVertices(plain, hosts[rank]);
    const int with = DrawnVertices(block, hosts[rank]);
    CHECK(without > 0);
    CHECK(with > without);
  }
}
