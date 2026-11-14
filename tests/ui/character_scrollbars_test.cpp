// Les cadres de la fiche qui débordent peignent leur ascenseur : la peau rend
// celui d'ImGui transparent, et sans le leur une liste défilerait sans rien
// montrer.

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skin_pieces.h"
#include "ro/ui/character_window.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::CharacterTab;
using ro::ui::CompanionCaseView;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 900.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPlainPiecePixels = 8.0F;
// L'ascenseur du client : treize pixels de large, des flèches carrées.
constexpr float kScrollPiecePixels = 13.0F;
// Les pièces de l'ascenseur ont leur propre texture : c'est à elle qu'on le
// reconnaît dans ce qu'un cadre a dessiné.
constexpr ImTextureID kScrollTexture = 7;
// La hauteur qu'on demande pour tomber sur le plancher de la fiche.
constexpr float kFloorProbePixels = 1.0F;
constexpr int kManyTitles = 80;
// Un jour vertical entre deux éléments assez grand pour que le volet du
// mannequin déborde à son plancher : le plancher loge la fiche au jour du
// style, pas à celui-ci.
constexpr float kTallItemSpacingPixels = 60.0F;

[[nodiscard]] ro::ui::SkinArt ScrollArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPlainPiecePixels, .height = kPlainPiecePixels});
  }
  for (const SkinPiece piece :
       {SkinPiece::kScrollUp, SkinPiece::kScrollMiddle, SkinPiece::kScrollDown,
        SkinPiece::kScrollGrabTop, SkinPiece::kScrollGrabMiddle, SkinPiece::kScrollGrabBottom}) {
    art.Set(piece,
            ro::ui::SkinImage{
                .id = kScrollTexture, .width = kScrollPiecePixels, .height = kScrollPiecePixels});
  }
  return art;
}

// La fiche ouverte sur un onglet, dessinée image par image.
struct SheetProbe {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = ScrollArt();
  ro::ui::CharacterWindowState state;
  ro::ui::CharacterWindowView view;
  ro::ui::CharacterHeaderView header;
  std::array<ro::ui::EquipSlotView, ro::ui::kEquipSlotCount> slots{};
  ro::ui::CharacterStatsView stats;
  ro::ui::PresetTabView presets;
  std::vector<ro::ui::CharacterTitleView> owned;
  ro::ui::GuildView guild;
  std::array<CompanionCaseView, 3> companions = {
      CompanionCaseView{.rank = 0}, CompanionCaseView{.rank = 1}, CompanionCaseView{.rank = 2}};

  explicit SheetProbe(CharacterTab tab) {
    state.open = true;
    state.tab = tab;
    state.tab_request = tab;
  }

  void Frame() {
    view.companions = companions;
    ui.BeginFrame(kViewport, kFrameSeconds);
    static_cast<void>(ro::ui::DrawCharacterWindow(state, view, header, slots, stats, presets,
                                                  ro::ui::CharacterTitlesView{.owned = owned},
                                                  guild, {}, ro::ui::SkillPointTally{}, art));
    ui.EndFrame();
  }

  void Settle() {
    for (int frame = 0; frame < 4; ++frame) {
      Frame();
    }
  }
};

// Le cadre de la fiche qui porte ce nom.
[[nodiscard]] const ImGuiWindow* Pane(std::string_view name) {
  const ImGuiWindow* sheet = ImGui::FindWindowByName("###character");
  if (sheet == nullptr) {
    return nullptr;
  }
  const std::string wanted = "/" + std::string(name) + "_";
  for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
    if (window->ParentWindow == sheet &&
        std::string_view(window->Name).find(wanted) != std::string_view::npos) {
      return window;
    }
  }
  return nullptr;
}

// ⚠ PAS `GetTexID()` : il assertionne sur la texture de la police, qu'aucun
// moteur n'a chargée dans un banc. Une image posée par identifiant n'a pas de
// données de texture, seulement son identifiant.
[[nodiscard]] bool PaintsScrollbar(const ImGuiWindow& pane) {
  for (const ImDrawCmd& command : pane.DrawList->CmdBuffer) {
    if (command.TexRef._TexData == nullptr && command.TexRef._TexID == kScrollTexture) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST_CASE("Fiche : la liste des titres qui déborde peint son ascenseur", "[ui][fiche][ascenseur]") {
  SheetProbe probe(CharacterTab::kTitles);
  for (int title = 1; title <= kManyTitles; ++title) {
    probe.owned.push_back(ro::ui::CharacterTitleView{.title_id = title, .label = "Titre"});
  }
  probe.Settle();
  const ImGuiWindow* pane = Pane("titres");
  REQUIRE(pane != nullptr);
  REQUIRE(pane->ScrollMax.y > 0.0F);
  CHECK(PaintsScrollbar(*pane));
}

TEST_CASE("Fiche : le mannequin qui déborde à son plancher peint son ascenseur",
          "[ui][fiche][ascenseur]") {
  // Le plancher loge le mannequin et ses trois compagnons : seul un jour
  // démesuré entre les éléments le fait déborder.
  SheetProbe probe(CharacterTab::kEquipment);
  ImGui::GetStyle().ItemSpacing.y = kTallItemSpacingPixels;
  probe.Settle();
  const ImGuiWindow* sheet = ImGui::FindWindowByName("###character");
  REQUIRE(sheet != nullptr);
  ImGui::SetWindowSize("###character", ImVec2(sheet->Size.x, kFloorProbePixels));
  probe.Settle();
  const ImGuiWindow* pane = Pane("mannequin");
  REQUIRE(pane != nullptr);
  REQUIRE(pane->ScrollMax.y > 0.0F);
  CHECK(PaintsScrollbar(*pane));
}
