// LE TEXTE D'UNE BARRE EST-IL VRAIMENT AU MILIEU DE SA BARRE ?
//
// 🔴 CETTE QUESTION DEMANDE UNE VRAIE POLICE, donc un contexte ImGui : ce qui
// décentre le texte est la façon dont l'encre pend dans sa boîte de ligne, et
// cela ne se sait qu'en interrogeant les glyphes cuits. Les autres essais des
// barres s'en passent ; celui-ci ne le peut pas.
//
// 🔴 CE QU'IL ATTRAPE, UN ÉCRAN LE MONTRE MAL. Un pixel de trop vers le bas se
// voit comme « le texte touche le cadre », et l'on corrige alors la hauteur de
// la barre ou l'épaisseur du cadre — deux réglages qui n'y sont pour rien.

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/entity_bars.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Ce qu'une barre écrit vraiment, dans les trois formes que le réglage permet.
constexpr std::array<std::string_view, 4> kCaptions = {"99%", "7%", "1234 / 5678",
                                                       "40 / 100  (40%)"};

// La boîte d'ENCRE d'un texte posé à `origin` : le plus haut et le plus bas de
// ses glyphes, en coordonnées d'écran.
struct Ink {
  float top = 0.0F;
  float bottom = 0.0F;
  bool found = false;
};

[[nodiscard]] Ink InkOf(ImFont& font, float pixels, float origin_y, std::string_view text) {
  Ink ink;
  ImFontBaked* baked = font.GetFontBaked(pixels);
  if (baked == nullptr) {
    return ink;
  }
  const char* stop = text.data() + text.size();
  for (const char* letter = text.data(); letter < stop;) {
    unsigned int codepoint = 0;
    const int eaten = ImTextCharFromUtf8(&codepoint, letter, stop);
    letter += eaten > 0 ? eaten : 1;
    const ImFontGlyph* glyph = baked->FindGlyph(static_cast<ImWchar>(codepoint));
    if (glyph == nullptr || glyph->Visible == 0) {
      continue;
    }
    const float top = origin_y + glyph->Y0;
    const float bottom = origin_y + glyph->Y1;
    ink.top = ink.found ? std::min(ink.top, top) : top;
    ink.bottom = ink.found ? std::max(ink.bottom, bottom) : bottom;
    ink.found = true;
  }
  return ink;
}

}  // namespace

TEST_CASE("Barre d'entité : l'encre du texte est au milieu de la barre", "[ui][barres][police]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImFont* font = ImGui::GetFont();
  REQUIRE(font != nullptr);

  // ⚠ LA BARRE DE LA CAPTURE : soixante sur six, et un texte de sept — le cas
  // qui a fait voir le défaut. On balaie ensuite tout ce que le curseur permet.
  for (float pixels = ro::ui::kSmallestEntityBarFont; pixels <= ro::ui::kLargestEntityBarFont;
       pixels += 1.0F) {
    for (const std::string_view caption : kCaptions) {
      // Une barre assez haute pour contenir ce texte : ce qui se vérifie ici est
      // le CENTRAGE, pas le débordement d'un texte plus grand que sa barre.
      const float height = pixels * 2.0F;
      const ImVec2 top_left(100.0F, 200.0F);
      const ImVec2 bottom_right(top_left.x + 60.0F, top_left.y + height);

      const ImVec2 at =
          ro::ui::EntityBarTextOrigin(*font, pixels, top_left, bottom_right, caption.data());
      const Ink ink = InkOf(*font, pixels, at.y, caption);
      REQUIRE(ink.found);

      const float above = ink.top - top_left.y;
      const float below = bottom_right.y - ink.bottom;
      INFO("police " << pixels << " px, « " << caption << " » : " << above << " au-dessus, "
                     << below << " en dessous");
      // 🔴 CE QUI SE TIENT : les deux creux ne diffèrent que de l'arrondi au
      // pixel. Centrer sur la BOÎTE DE LIGNE donnait jusqu'à cinq pixels d'écart
      // — deux au-dessus et zéro en dessous sur la barre de la capture.
      CHECK(std::abs(above - below) <= 1.0F);
    }
  }
  ui.EndFrame();
}

TEST_CASE("Barre d'entité : le point de dessin tombe sur le pixel", "[ui][barres][police]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImFont* font = ImGui::GetFont();
  REQUIRE(font != nullptr);

  // ⚠ UNE BARRE POSÉE ENTRE DEUX PIXELS, comme l'est une entité qui marche :
  // c'est le cas où l'ombre portée se décollait du texte.
  const ImVec2 top_left(100.37F, 200.62F);
  const ImVec2 bottom_right(160.37F, 214.62F);
  const ImVec2 at = ro::ui::EntityBarTextOrigin(*font, 11.0F, top_left, bottom_right, "99%");
  CHECK(at.x == std::floor(at.x));
  CHECK(at.y == std::floor(at.y));
  ui.EndFrame();
}
