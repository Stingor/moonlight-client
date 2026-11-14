// OÙ L'ENCRE TOMBE VRAIMENT DANS LA BOÎTE DE LIGNE.
//
// 🔴 CENTRER UN TEXTE SUR SA BOÎTE DE LIGNE NE LE CENTRE PAS. `CalcTextSizeA`
// rend la HAUTEUR DE LIGNE — la taille de police, tout simplement —, et les
// lettres n'y sont pas au milieu : elles pendent sous l'ascendante, et des
// chiffres, qui n'ont pas de descendante, laissent tout leur creux en bas.
//
// Ce banc imprime le déséquilibre, police par police et taille par taille. Il
// sert à décider du décalage, pas à le vérifier : l'essai qui MORD est dans
// `entity_bars_test.cpp`.

#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/font_setup.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Les tailles que le curseur des barres d'entité permet d'atteindre.
constexpr std::array<float, 6> kSizes = {5.0F, 7.0F, 9.0F, 11.0F, 13.0F, 24.0F};

// Ce qu'une barre écrit vraiment : des chiffres, un pour cent, une barre oblique.
constexpr std::string_view kTypical = "99%";
constexpr std::string_view kValues = "1234 / 5678";

// La boîte d'ENCRE d'un texte : le plus haut et le plus bas de ses glyphes,
// comptés depuis le point de dessin. ⚠ `Y0`/`Y1` sont déjà relatifs à ce point,
// ascendante comprise.
struct InkBox {
  float top = 0.0F;
  float bottom = 0.0F;
  bool found = false;
};

[[nodiscard]] InkBox MeasureInk(ImFontBaked& baked, std::string_view text) {
  InkBox box;
  for (const char letter : text) {
    const ImFontGlyph* glyph = baked.FindGlyph(static_cast<ImWchar>(letter));
    if (glyph == nullptr || glyph->Visible == 0) {
      continue;
    }
    box.top = box.found ? std::min(box.top, glyph->Y0) : glyph->Y0;
    box.bottom = box.found ? std::max(box.bottom, glyph->Y1) : glyph->Y1;
    box.found = true;
  }
  return box;
}

void Report(std::string_view family, std::string_view text) {
  ImFont* font = ImGui::GetFont();
  if (font == nullptr) {
    return;
  }
  std::printf("  %s, « %s »\n", std::string(family).c_str(), std::string(text).c_str());
  for (const float size : kSizes) {
    ImFontBaked* baked = font->GetFontBaked(size);
    if (baked == nullptr) {
      continue;
    }
    const InkBox ink = MeasureInk(*baked, text);
    if (!ink.found) {
      continue;
    }
    // Le creux au-dessus de l'encre, et celui en dessous, dans une boîte de
    // ligne haute de `baked->Size`. Leur écart est ce qui décentre le texte.
    const float above = ink.top;
    const float below = baked->Size - ink.bottom;
    std::printf(
        "    %5.1f px : ascendante %5.2f, descendante %5.2f · encre de %5.2f à %5.2f · "
        "creux %5.2f en haut, %5.2f en bas → à remonter de %5.2f\n",
        static_cast<double>(size), static_cast<double>(baked->Ascent),
        static_cast<double>(baked->Descent), static_cast<double>(ink.top),
        static_cast<double>(ink.bottom), static_cast<double>(above), static_cast<double>(below),
        static_cast<double>((below - above) * 0.5F));
  }
}

[[nodiscard]] bool Have(std::string_view path) {
  std::error_code ignored;
  return std::filesystem::exists(std::filesystem::path(path), ignored);
}

}  // namespace

TEST_CASE("Banc : où l'encre tombe dans la boîte de ligne", "[.banc][ui][police]") {
  {
    ro::ui::UiContext ui;
    ui.BeginFrame(kViewport, kFrameSeconds);
    std::printf("\n");
    Report("ProggyClean (le défaut d'ImGui)", kTypical);
    Report("ProggyClean (le défaut d'ImGui)", kValues);
    ui.EndFrame();
  }

  constexpr std::string_view kRealFont = "C:\\Windows\\Fonts\\malgun.ttf";
  if (!Have(kRealFont)) {
    SUCCEED("cette machine n'a pas de police complète : rien de plus à mesurer");
    return;
  }
  ro::ui::UiContext ui;
  const std::array<ro::ui::FontFamilySpec, 1> families = {
      ro::ui::FontFamilySpec{.label = "complète", .regular = kRealFont}};
  ui.UseFonts(ro::ui::FontSetup{.families = families, .chosen = 0});
  ui.BeginFrame(kViewport, kFrameSeconds);
  Report("malgun.ttf (une vraie police)", kTypical);
  Report("malgun.ttf (une vraie police)", kValues);
  ui.EndFrame();
}
