#include "ro/ui/skin_popup.h"

#include <array>
#include <cstddef>
#include <tuple>

#include <imgui.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_art.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPiecePixels = 8.0F;

// Un habillage complet, sous un habillage réglé. Les identifiants ne désignent
// aucune texture : rien n'est peint ici.
[[nodiscard]] ro::ui::SkinArt DressedArt(const ro::ui::SkinLook& look) {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPiecePixels, .height = kPiecePixels});
  }
  art.SetLook(look);
  return art;
}

[[nodiscard]] ImU32 StyleColorNow(ImGuiCol which) {
  return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(which));
}

// UN CONTEXTE IMGUI COMPLET, le temps d'une image : un surgissant ne s'ouvre
// que dans une image, et c'est ce qu'il pose sur la pile qu'on vient lire.
struct OneFrame {
  ImGuiContext* context = ImGui::CreateContext();

  OneFrame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800.0F, 600.0F);
    io.DeltaTime = 1.0F / 60.0F;
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    ImGui::NewFrame();
  }
  ~OneFrame() {
    ImGui::EndFrame();
    ImGui::DestroyContext(context);
  }
  OneFrame(const OneFrame&) = delete;
  OneFrame& operator=(const OneFrame&) = delete;
};

[[nodiscard]] ImU32 PopupBodyNow() {
  return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_PopupBg));
}

// Ouvre le menu dans une fenêtre hôte, et rend le fond qu'il porte.
[[nodiscard]] ImU32 BodyOfMenuOpenedHere() {
  ImU32 inside = 0;
  ImGui::Begin("##hote");
  ImGui::OpenPopup("##menu");
  if (ro::ui::BeginSkinPopup("##menu")) {
    inside = PopupBodyNow();
    ro::ui::EndSkinPopup();
  }
  ImGui::End();
  return inside;
}

}  // namespace

TEST_CASE("Surgissant habillé : un fond sombre se juge sombre", "[ui][popup]") {
  CHECK(ro::ui::SkinPopupNeedsBody(IM_COL32(20, 20, 20, 240)));
  CHECK(ro::ui::SkinPopupNeedsBody(IM_COL32(14, 14, 18, 255)));
  CHECK_FALSE(ro::ui::SkinPopupNeedsBody(ro::ui::kSkinPopupColor));
  CHECK_FALSE(ro::ui::SkinPopupNeedsBody(ro::ui::kSkinBodyColor));
}

TEST_CASE("Surgissant habillé : ouvert sur un fond sombre, il prend le corps clair",
          "[ui][popup]") {
  // 🔴 HORS D'UNE FENÊTRE HABILLÉE, LE FOND EST CELUI D'IMGUI, SOMBRE : c'est
  // là que le texte sombre de l'habillage devenait illisible — le menu du
  // compteur de dégâts, ouvert depuis son HUD.
  const OneFrame frame;
  REQUIRE(ro::ui::SkinPopupNeedsBody(PopupBodyNow()));
  CHECK(BodyOfMenuOpenedHere() == ro::ui::kSkinPopupColor);
  // Et la pile est rendue telle quelle : le fond sombre revient après.
  CHECK(ro::ui::SkinPopupNeedsBody(PopupBodyNow()));
}

TEST_CASE("Surgissant habillé : un fond clair posé exprès est respecté", "[ui][popup]") {
  const OneFrame frame;
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ro::ui::kSkinBodyColor);
  CHECK(BodyOfMenuOpenedHere() == ro::ui::kSkinBodyColor);
  ImGui::PopStyleColor();
}

TEST_CASE("Surgissant habillé : dans une fenêtre habillée, il prend l'habillage, même sombre",
          "[ui][popup]") {
  // 🔴 « MENUS ET LISTES » RÉGLÉ SOMBRE RESTAIT SANS EFFET : le fond sombre se
  // jugeait sombre, et le menu reprenait le corps clair du jeu. Dans une fenêtre
  // habillée, c'est l'habillage du joueur qui fait foi — fond, texte, texte
  // grisé et sélection.
  ro::ui::SkinLook look;
  look.popup = IM_COL32(44, 46, 54, 255);
  look.border = IM_COL32(90, 94, 110, 255);
  look.text = IM_COL32(226, 228, 235, 255);
  look.faint_text = IM_COL32(150, 154, 166, 255);
  look.selection = IM_COL32(90, 120, 190, 140);
  const ro::ui::SkinArt art = DressedArt(look);
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImU32 body = 0;
  ImU32 text = 0;
  ImU32 faint = 0;
  ImU32 chosen = 0;
  if (ro::ui::BeginSkinnedWindow("hote", nullptr, art)) {
    ImGui::OpenPopup("##menu");
    if (ro::ui::BeginSkinPopup("##menu")) {
      body = StyleColorNow(ImGuiCol_PopupBg);
      text = StyleColorNow(ImGuiCol_Text);
      faint = StyleColorNow(ImGuiCol_TextDisabled);
      chosen = StyleColorNow(ImGuiCol_Header);
      ro::ui::EndSkinPopup();
    }
  }
  ro::ui::EndSkinnedWindow();
  ui.EndFrame();
  CHECK(body == look.popup);
  CHECK(text == look.text);
  CHECK(faint == look.faint_text);
  CHECK(chosen == look.selection);
}

TEST_CASE("Surgissant habillé : un cadre non habillé prête son habillage, le temps du cadre",
          "[ui][popup]") {
  // 🔴 LA CONVERSATION N'EST PAS UNE FENÊTRE HABILLÉE : son menu d'onglet
  // restait clair sous un thème sombre. Elle prête son habillage à ses menus.
  ro::ui::SkinLook look;
  look.popup = IM_COL32(44, 46, 54, 255);
  look.text = IM_COL32(226, 228, 235, 255);
  const ro::ui::SkinArt art = DressedArt(look);
  const OneFrame frame;
  {
    const ro::ui::LentSkinArt lent(art);
    CHECK(ro::ui::PaintingSkinArt() == &art);
    ImU32 text = 0;
    ImGui::Begin("##cadre");
    ImGui::OpenPopup("##menu");
    if (ro::ui::BeginSkinPopup("##menu")) {
      text = StyleColorNow(ImGuiCol_Text);
      ro::ui::EndSkinPopup();
    }
    ImGui::End();
    CHECK(text == look.text);
  }
  CHECK(ro::ui::PaintingSkinArt() == nullptr);
  // ⚠ UN HABILLAGE QUI N'HABILLE PAS LES FENÊTRES NE SE PRÊTE PAS.
  const ro::ui::SkinArt bare;
  const ro::ui::LentSkinArt nothing(bare);
  CHECK(ro::ui::PaintingSkinArt() == nullptr);
}

namespace {

// UN CONTEXTE QUI VIT PLUSIEURS IMAGES : un clic s'étale sur deux, l'enfoncement
// puis le relâchement.
struct ManyFrames {
  ImGuiContext* context = ImGui::CreateContext();

  ManyFrames() {
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800.0F, 600.0F);
    io.DeltaTime = kFrameSeconds;
    io.IniFilename = nullptr;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
  }
  ~ManyFrames() { ImGui::DestroyContext(context); }
  ManyFrames(const ManyFrames&) = delete;
  ManyFrames& operator=(const ManyFrames&) = delete;
};

constexpr ImVec2 kHostAt(100.0F, 100.0F);
constexpr ImVec2 kCellSize(32.0F, 32.0F);
// Le curseur, posé dans la case.
constexpr ImVec2 kMouseAt(120.0F, 140.0F);

// Une image : une case et son menu de clic droit. Rend vrai si le menu s'y
// est dessiné.
[[nodiscard]] bool CellFrame() {
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(kHostAt);
  ImGui::Begin("##hote", nullptr, ImGuiWindowFlags_NoMove);
  ImGui::Button("##case", kCellSize);
  const bool open = ro::ui::BeginSkinPopupContextItem("menu");
  if (open) {
    ImGui::TextUnformatted("Description");
    ro::ui::EndSkinPopup();
  }
  ImGui::End();
  ImGui::EndFrame();
  return open;
}

// Le bouton droit enfoncé, puis relâché, une image chacun. Rend l'état du menu
// à l'image du relâchement.
[[nodiscard]] bool RightClickCell() {
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  std::ignore = CellFrame();
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  return CellFrame();
}

}  // namespace

TEST_CASE("Surgissant habillé : un second clic droit sur l'élément referme son menu",
          "[ui][popup]") {
  const ManyFrames frames;
  ImGui::GetIO().AddMousePosEvent(kMouseAt.x, kMouseAt.y);
  REQUIRE_FALSE(CellFrame());
  REQUIRE_FALSE(CellFrame());
  CHECK(RightClickCell());
  CHECK(CellFrame());
  // ⚠ REFERMÉ, ET IL LE RESTE : ImGui le rouvrait au relâchement.
  CHECK_FALSE(RightClickCell());
  CHECK_FALSE(CellFrame());
  // Le geste suivant le rouvre : la fermeture ne mange pas l'ouverture.
  CHECK(RightClickCell());
}

namespace {

// Deux cases en colonne, chacune avec un menu SANS NOM : il porte l'identifiant
// de sa case. Rend lesquels se sont dessinés.
[[nodiscard]] std::array<bool, 2> TwoCellsFrame() {
  std::array<bool, 2> open{};
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(kHostAt);
  ImGui::Begin("##hote", nullptr, ImGuiWindowFlags_NoMove);
  for (std::size_t rank = 0; rank < open.size(); ++rank) {
    ImGui::PushID(static_cast<int>(rank));
    ImGui::Button("##case", kCellSize);
    open[rank] = ro::ui::BeginSkinPopupContextItem(nullptr);
    if (open[rank]) {
      ImGui::TextUnformatted("Description");
      ro::ui::EndSkinPopup();
    }
    ImGui::PopID();
  }
  ImGui::End();
  ImGui::EndFrame();
  return open;
}

}  // namespace

TEST_CASE("Surgissant habillé : un menu sans nom n'ouvre que celui de son élément", "[ui][popup]") {
  const ManyFrames frames;
  ImGui::GetIO().AddMousePosEvent(kMouseAt.x, kMouseAt.y);
  std::ignore = TwoCellsFrame();
  std::ignore = TwoCellsFrame();
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, true);
  std::ignore = TwoCellsFrame();
  ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, false);
  const std::array<bool, 2> open = TwoCellsFrame();
  CHECK(open[0]);
  CHECK_FALSE(open[1]);
}
