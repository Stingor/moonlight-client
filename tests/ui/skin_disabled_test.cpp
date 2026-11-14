// LE GRISAGE ET LA TEINTE DES WIDGETS PEINTS : le bouton et la case sous
// `BeginDisabled`, sous la teinte de l'habillage et l'opacité de la fenêtre, et
// la bulle « (?) » qui reste lisible.

#include <cstdint>
#include <utility>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_art.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::kSkinDisabledArtTint;
using ro::ui::MultiplyTint;
using ro::ui::SkinArt;
using ro::ui::SkinImage;
using ro::ui::SkinLook;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// De quoi poser un bouton large : ses trois tranches d'art ont alors toutes une
// largeur, donc toutes un quad.
constexpr float kWideEnough = 600.0F;
constexpr float kTallEnough = 200.0F;
constexpr float kButtonWidth = 200.0F;

// Les mesures du client : un bout de bouton fait six pixels sur vingt, une case
// onze pixels de côté.
constexpr float kCapPixels = 6.0F;
constexpr float kButtonPixels = 20.0F;
constexpr float kCheckBoxPixels = 11.0F;

// Un bouton peint trois tranches, une case une seule image : quatre sommets
// chacune.
constexpr int kVerticesPerImage = 4;
constexpr int kButtonArtVertices = 3 * kVerticesPerImage;
constexpr int kCheckBoxArtVertices = kVerticesPerImage;

[[nodiscard]] SkinArt WidgetArt() {
  SkinArt art;
  for (const SkinPiece piece :
       {SkinPiece::kButtonLeft, SkinPiece::kButtonMiddle, SkinPiece::kButtonRight,
        SkinPiece::kButtonHoverLeft, SkinPiece::kButtonHoverMiddle, SkinPiece::kButtonHoverRight,
        SkinPiece::kButtonPressedLeft, SkinPiece::kButtonPressedMiddle,
        SkinPiece::kButtonPressedRight}) {
    art.Set(piece, SkinImage{.id = 1, .width = kCapPixels, .height = kButtonPixels});
  }
  for (const SkinPiece piece : {SkinPiece::kCheckBoxEmpty, SkinPiece::kCheckBoxTicked}) {
    art.Set(piece, SkinImage{.id = 1, .width = kCheckBoxPixels, .height = kCheckBoxPixels});
  }
  return art;
}

// CE QU'UN WIDGET A PEINT. ⚠ L'ART D'ABORD, LE LIBELLÉ ENSUITE : c'est l'ordre
// de la peinture, et c'est par lui qu'on sépare les deux — en blanc plein, l'art
// et le texte peuvent avoir la même couleur.
struct Painted {
  // Les sommets de l'art qui portent la teinte attendue.
  int art_tinted = 0;
  // Les sommets du libellé, et ceux d'entre eux qui ont la couleur attendue.
  int text_vertices = 0;
  int text_colored = 0;
};

// Le bouton d'état RETENU montre l'image enfoncée : c'est la teinte du survol.
enum class Widget : std::uint8_t { kButton, kHeldToggle, kCheckBox };

// Pose le widget, grisé ou non, sous un habillage et une opacité, et relève ce
// qu'il a peint.
[[nodiscard]] Painted PaintWidget(Widget widget, bool disabled, const SkinLook& look = SkinLook{},
                                  float opacity = 1.0F) {
  ro::ui::UiContext ui;
  SkinArt art = WidgetArt();
  art.SetLook(look);
  ui.BeginFrame(kViewport, kFrameSeconds);
  // ⚠ APRÈS LE DÉBUT D'IMAGE, et assez grande : une fenêtre qui naît sans taille
  // coupe ses lignes de texte, et le libellé ne laisserait aucun sommet.
  ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
  ImGui::SetNextWindowSize(ImVec2(kWideEnough, kTallEnough));
  ImGui::Begin("essai");
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, opacity);
  // L'opacité, appliquée par ImGui lui-même : c'est sa règle que l'art doit
  // suivre. ⚠ AVANT LE BLOC GRISÉ, dont l'effacement ne touche pas l'art.
  const ImU32 tint = widget == Widget::kCheckBox     ? look.check_tint
                     : widget == Widget::kHeldToggle ? look.button_hover_tint
                                                     : look.button_tint;
  const ImU32 art_color =
      ImGui::GetColorU32(disabled ? MultiplyTint(tint, kSkinDisabledArtTint) : tint);
  ImGui::BeginDisabled(disabled);
  // ⚠ LUES DANS LE BLOC GRISÉ : `BeginDisabled` atténue l'alpha du style, et la
  // couleur attendue du libellé est celle que le bloc donne. Le libellé d'un
  // bouton a la sienne, celui d'une case est le texte du corps.
  const ImU32 text_color = disabled                      ? ImGui::GetColorU32(ImGuiCol_TextDisabled)
                           : widget != Widget::kCheckBox ? ImGui::GetColorU32(look.button_text)
                                                         : ImGui::GetColorU32(ImGuiCol_Text);
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const int before = into->VtxBuffer.Size;
  int art_vertices = 0;
  switch (widget) {
    case Widget::kButton:
      static_cast<void>(ro::ui::SkinButton("Valider", art, ImVec2(kButtonWidth, 0.0F)));
      art_vertices = kButtonArtVertices;
      break;
    case Widget::kHeldToggle:
      static_cast<void>(ro::ui::SkinToggleButton("Valider", true, art, ImVec2(kButtonWidth, 0.0F)));
      art_vertices = kButtonArtVertices;
      break;
    case Widget::kCheckBox: {
      bool ticked = true;
      static_cast<void>(ro::ui::SkinCheckBox("Activer", &ticked, art));
      art_vertices = kCheckBoxArtVertices;
      break;
    }
  }
  Painted painted;
  for (int rank = before; rank < into->VtxBuffer.Size; ++rank) {
    const ImU32 color = into->VtxBuffer[rank].col;
    if (rank < before + art_vertices) {
      painted.art_tinted += color == art_color ? 1 : 0;
    } else {
      ++painted.text_vertices;
      painted.text_colored += color == text_color ? 1 : 0;
    }
  }
  ImGui::EndDisabled();
  ImGui::PopStyleVar();
  ImGui::End();
  ui.EndFrame();
  return painted;
}

}  // namespace

TEST_CASE("Grisage : le bouton peint s'efface sous BeginDisabled", "[ui][skin]") {
  // 🔴 IMGUI NE GRISE QUE CE QU'IL DESSINE. L'art posé à la main resterait
  // éclatant sous un bouton qui ne répond plus, et le joueur cliquerait dans le
  // vide : l'art passe à 90/255, le libellé à la couleur grisée.
  REQUIRE(kSkinDisabledArtTint == IM_COL32(255, 255, 255, 90));
  const Painted grayed = PaintWidget(Widget::kButton, true);
  CHECK(grayed.art_tinted == kButtonArtVertices);
  CHECK(grayed.text_vertices > 0);
  CHECK(grayed.text_colored == grayed.text_vertices);

  // ⚠ ET SANS GRISAGE, RIEN NE CHANGE : l'art en blanc plein, le libellé dans
  // la couleur des boutons.
  const Painted plain = PaintWidget(Widget::kButton, false);
  CHECK(plain.art_tinted == kButtonArtVertices);
  CHECK(plain.text_vertices > 0);
  CHECK(plain.text_colored == plain.text_vertices);
}

TEST_CASE("Teinte : le bouton prend celle de l'habillage, et son libellé sa couleur",
          "[ui][skin]") {
  // 🔴 LE LIBELLÉ D'UN BOUTON N'EST PAS LE TEXTE DU CORPS : il est posé sur l'art
  // du bouton, presque blanc dans le skin du jeu. Un texte clair réglé pour un
  // corps sombre y disparaissait.
  SkinLook look;
  look.button_tint = IM_COL32(104, 110, 128, 255);
  look.button_text = IM_COL32(232, 234, 240, 255);
  const Painted tinted = PaintWidget(Widget::kButton, false, look);
  CHECK(tinted.art_tinted == kButtonArtVertices);
  CHECK(tinted.text_vertices > 0);
  CHECK(tinted.text_colored == tinted.text_vertices);
  // Grisé, la teinte grisée se pose sur la sienne.
  const Painted grayed = PaintWidget(Widget::kButton, true, look);
  CHECK(grayed.art_tinted == kButtonArtVertices);
  CHECK(grayed.text_colored == grayed.text_vertices);
}

TEST_CASE("Teinte : un bouton retenu prend celle du survol", "[ui][skin]") {
  // ⚠ LE SKIN DONNE AU BOUTON ENFONCÉ UNE AUTRE IMAGE, bleue et plus sombre :
  // sous la teinte du repos, le survol d'un thème sombre foncerait au lieu de
  // s'éclairer.
  SkinLook look;
  look.button_tint = IM_COL32(104, 110, 128, 255);
  look.button_hover_tint = IM_COL32(140, 150, 180, 255);
  look.button_text = IM_COL32(232, 234, 240, 255);
  const Painted held = PaintWidget(Widget::kHeldToggle, false, look);
  CHECK(held.art_tinted == kButtonArtVertices);
  CHECK(held.text_vertices > 0);
  CHECK(held.text_colored == held.text_vertices);
}

TEST_CASE("Teinte : la case prend celle des cases, et non celle des boutons", "[ui][skin]") {
  SkinLook look;
  look.button_tint = IM_COL32(104, 110, 128, 255);
  look.check_tint = IM_COL32(150, 156, 172, 255);
  const Painted tinted = PaintWidget(Widget::kCheckBox, false, look);
  CHECK(tinted.art_tinted == kCheckBoxArtVertices);
  CHECK(tinted.text_colored == tinted.text_vertices);
}

TEST_CASE("Teinte : l'opacité de la fenêtre atteint l'art", "[ui][skin]") {
  // 🔴 `AddImage` N'APPLIQUE PAS L'OPACITÉ DU STYLE : sans y veiller, la fenêtre
  // devient translucide et ses boutons restent opaques par-dessus.
  constexpr float kHalf = 0.5F;
  const Painted faded = PaintWidget(Widget::kButton, false, SkinLook{}, kHalf);
  CHECK(faded.art_tinted == kButtonArtVertices);
  // ⚠ GRISÉ, L'ART NE S'EFFACE QU'UNE FOIS : sa teinte grisée sous l'opacité,
  // sans l'effacement du grisage en plus.
  const Painted grayed = PaintWidget(Widget::kButton, true, SkinLook{}, kHalf);
  CHECK(grayed.art_tinted == kButtonArtVertices);
}

TEST_CASE("Teinte : deux teintes se multiplient, et le blanc est neutre", "[ui][skin]") {
  constexpr ImU32 kColor = IM_COL32(255, 128, 0, 200);
  CHECK(MultiplyTint(kColor, IM_COL32_WHITE) == kColor);
  CHECK(MultiplyTint(IM_COL32_WHITE, kColor) == kColor);
  CHECK(MultiplyTint(kColor, IM_COL32(128, 128, 128, 255)) == IM_COL32(128, 64, 0, 200));
  CHECK(MultiplyTint(kColor, IM_COL32(0, 0, 0, 0)) == IM_COL32(0, 0, 0, 0));
}

TEST_CASE("Grisage : la case peinte s'efface comme le bouton", "[ui][skin]") {
  const Painted grayed = PaintWidget(Widget::kCheckBox, true);
  CHECK(grayed.art_tinted == kCheckBoxArtVertices);
  CHECK(grayed.text_vertices > 0);
  CHECK(grayed.text_colored == grayed.text_vertices);

  const Painted plain = PaintWidget(Widget::kCheckBox, false);
  CHECK(plain.art_tinted == kCheckBoxArtVertices);
  CHECK(plain.text_vertices > 0);
  CHECK(plain.text_colored == plain.text_vertices);
}

TEST_CASE("Grisage : la bulle « (?) » répond sur une rangée grisée", "[ui][skin]") {
  // ⚠ C'EST SOUVENT ELLE QUI DIT POURQUOI LA RANGÉE NE RÉPOND PAS. Muette sous
  // le grisage, elle laisserait le joueur devant un réglage figé sans raison.
  ro::ui::UiContext ui;
  ImVec2 middle(0.0F, 0.0F);
  bool bubble_shown = false;
  // ⚠ TROIS IMAGES : la première fait naître la fenêtre, la deuxième pose le
  // marqueur là où il restera, la troisième le survole — ImGui ne connaît la
  // fenêtre survolée qu'à l'image qui suit son dessin.
  constexpr int kFrames = 3;
  for (int frame = 0; frame < kFrames; ++frame) {
    ImGui::GetIO().MousePos = middle;
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(kWideEnough, kTallEnough));
    ImGui::Begin("rangee");
    ImGui::BeginDisabled();
    ro::ui::SkinHelpMarker("Pourquoi ce réglage est grisé.");
    middle = ImVec2((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5F,
                    (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5F);
    ImGui::EndDisabled();
    ImGui::End();
    const ImGuiWindow* bubble = ImGui::FindWindowByName("##Tooltip_00");
    bubble_shown = bubble != nullptr && bubble->Active;
    ui.EndFrame();
  }
  CHECK(bubble_shown);
}

TEST_CASE("Papier : une pièce peinte sur du blanc prend sa variante sur un papier sombre",
          "[ui][skin]") {
  constexpr ImTextureID kOnWhite = 1;
  constexpr ImTextureID kOnDark = 2;
  constexpr ImU32 kCream = IM_COL32(252, 248, 236, 255);
  constexpr ImU32 kDarkPaper = IM_COL32(40, 42, 50, 255);
  const SkinImage image{.id = kOnWhite,
                        .width = kCheckBoxPixels,
                        .height = kCheckBoxPixels,
                        .dark_paper_id = kOnDark};
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
  ImGui::SetNextWindowSize(ImVec2(kWideEnough, kTallEnough));
  ImGui::Begin("papier");
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const auto painted = [into, &image](ImU32 paper) {
    ro::ui::PaintOnPaper(into, image, ImVec2(0.0F, 0.0F), ImVec2(kCheckBoxPixels, kCheckBoxPixels),
                         paper);
    // ⚠ LA DERNIÈRE COMMANDE QUI DESSINE : derrière l'image, ImGui en rouvre
    // une vide pour la police, que l'essai n'a pas téléversée.
    ImTextureID texture{};
    for (int rank = into->CmdBuffer.Size - 1; rank >= 0; --rank) {
      if (into->CmdBuffer[rank].ElemCount > 0) {
        texture = into->CmdBuffer[rank].TexRef.GetTexID();
        break;
      }
    }
    return std::pair(texture, into->VtxBuffer.back().col);
  };
  // Sur un papier clair, l'image d'origine, multipliée par lui.
  const auto [light_id, light_tint] = painted(kCream);
  CHECK(light_id == kOnWhite);
  CHECK(light_tint == kCream);
  // Sur un sombre, sa variante, telle.
  const auto [dark_id, dark_tint] = painted(kDarkPaper);
  CHECK(dark_id == kOnDark);
  CHECK(dark_tint == IM_COL32_WHITE);
  ImGui::End();
  ui.EndFrame();
}
