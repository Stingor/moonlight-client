// L'aperçu de l'habillage. Voir l'en-tête.

#include "ro/ui/skin_look_sample.h"

#include <array>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skinned_window.h"

#include "ui_text.h"

namespace ro::ui {
namespace {

// La marge du papier autour de son texte, et l'écart entre les blocs, en
// largeurs de police : la taille du texte est un réglage du joueur.
constexpr float kPaperPaddingInFonts = 0.6F;
constexpr float kBlockGapInFonts = 0.4F;
// Les lignes que montre le papier.
constexpr int kPaperLines = 2;
// UNE COULEUR DU JEU DANS UNE DESCRIPTION : un bleu choisi pour du blanc, que le
// papier garde lisible comme le font les descriptions.
constexpr ImU32 kDescriptionBlue = IM_COL32(0, 0, 255, 255);

// Un mot dans sa couleur, puis un blanc avant le suivant.
void Word(const char* text, ImU32 color) {
  ColoredText(color, text);
  ImGui::SameLine();
}

void Gap() { ImGui::Dummy(ImVec2(0.0F, ImGui::GetFontSize() * kBlockGapInFonts)); }

// LES TEXTES DU CORPS, chaque rôle dans sa couleur.
void DrawTexts(const SkinLook& look) {
  Word(core::Tr("Texte"), look.text);
  Word(core::Tr("Texte secondaire"), look.faint_text);
  Word(core::Tr("Lien"), look.link);
  ImGui::NewLine();
  Word(core::Tr("Réussite"), look.good_text);
  Word(core::Tr("Échec"), look.bad_text);
  Word(core::Tr("Avertissement"), look.warn_text);
  Word(core::Tr("Information"), look.info_text);
  ImGui::NewLine();
}

// LA SÉLECTION : une ligne choisie, une survolée, une au repos.
void DrawSelection(const SkinLook& look) {
  const std::array<std::pair<const char*, ImU32>, 3> lines = {{
      {core::Tr("Ligne choisie"), look.selection},
      {core::Tr("Ligne survolée"), look.SelectionHover()},
      {core::Tr("Ligne au repos"), 0},
  }};
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const float width = ImGui::GetContentRegionAvail().x;
  for (const auto& [label, fill] : lines) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (fill != 0) {
      into->AddRectFilled(at, ImVec2(at.x + width, at.y + ImGui::GetTextLineHeight()),
                          ImGui::GetColorU32(fill));
    }
    ImGui::TextUnformatted(label);
  }
}

// LE PAPIER DES BULLES ET DESCRIPTIONS, dans le cadre des bulles : son texte,
// son texte secondaire et ses liens, et une couleur du jeu.
void DrawPaper(const SkinArt& art) {
  const SkinLook& look = art.look();
  const float padding = ImGui::GetFontSize() * kPaperPaddingInFonts + DescPanelEdge(art);
  const float width = ImGui::GetContentRegionAvail().x;
  const float height =
      ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(kPaperLines) + 2.0F * padding;
  const ImVec2 top_left = ImGui::GetCursorScreenPos();
  DrawDescPanelFrame(ImGui::GetWindowDrawList(), art, top_left,
                     ImVec2(top_left.x + width, top_left.y + height), /*fill_bg=*/true);
  const float left = ImGui::GetCursorPosX() + padding;
  ImGui::SetCursorPos(ImVec2(left, ImGui::GetCursorPosY() + padding));
  Word(core::Tr("Une description, sur le papier"), look.paper_text);
  Word(core::Tr("et son texte secondaire"), ReadableOn(look.faint_text, look.paper));
  ImGui::NewLine();
  ImGui::SetCursorPosX(left);
  Word(core::Tr("un lien"), ReadableOn(look.link, look.paper));
  Word(core::Tr("une couleur du jeu"), ReadableOn(kDescriptionBlue, look.paper));
  ImGui::NewLine();
  ImGui::SetCursorScreenPos(ImVec2(top_left.x, top_left.y + height));
  ImGui::Dummy(ImVec2(width, 0.0F));
}

}  // namespace

bool DrawSkinLookSample(const SkinArt& art) {
  DrawTexts(art.look());
  Gap();
  DrawSelection(art.look());
  Gap();
  DrawPaper(art);
  return false;
}

}  // namespace ro::ui
