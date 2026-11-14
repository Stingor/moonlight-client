#include "palette_pieces.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>

#include <imgui_internal.h>  // GetInputTextState : ce que la saisie du code vient de changer

#include "ro/core/i18n.h"
#include "ro/ui/color_code.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "ui_scale.h"

namespace ro::ui {
namespace {

// 🔴 SOUS CETTE CLARTÉ, UNE PIÈCE NE PEUT PLUS ÊTRE ÉCLAIRCIE, et on l'écrit.
// Deux cent vingt-quatre sur deux cent cinquante-cinq — au-dessus, le plafond
// ne se remarque pas. Sur les 421 corps, cela désigne environ une pièce sur
// six, assez rare pour que la mention veuille dire quelque chose, assez
// fréquent pour couvrir les cas signalés.
constexpr int kShadowCeiling = 224;
constexpr int kFullValue = 255;
constexpr float kFullChannel = 255.0F;
constexpr int kPercent = 100;
constexpr int kDegreesPerTurn = 360;
// Le décalage de teinte des nuances gardées : un demi-tour de chaque côté
// suffit à atteindre toutes les teintes.
constexpr int kHalfTurn = 180;
// Sous cette saturation, une couleur d'origine n'a aucune teinte : rien à
// tourner, et aucun rapport de saturation à tirer d'elle.
constexpr float kHuelessBelow = 0.01F;
// Le champ du code couleur : son identifiant, qui ne suit pas la langue de son
// libellé ; la place d'une saisie, préfixe compris et un peu plus qu'un code
// pour qu'un collage mal rogné s'y lise ; et le texte dont il prend la largeur
// — le code le plus large, un chiffre de marge pour le curseur.
constexpr const char* kCodeFieldId = "##code";
constexpr std::size_t kCodeFieldSize = 16;
constexpr const char* kWidestCode = "0xDDDDDDD";

// L'ambre dont s'allume la pièce que la pipette désigne, la même que celle du
// cadre d'une case choisie.
constexpr ImU32 kPipetteColor = IM_COL32(255, 209, 110, 255);
// La pastille, en points d'interface : celle des fonds de conversation.
constexpr float kSwatchPoints = 20.0F;
// Le glisser d'une pastille, entre deux pièces de la liste.
constexpr const char* kPiecePayload = "style_piece";

// ── LE NOM D'UNE COULEUR ────────────────────────────────────────────────────
//
// SEUILS EN TEINTE (degrés), SATURATION ET CLARTÉ (sur un) : ce sont ceux
// qu'un œil emploie. Sous un quart de clarté on voit du noir, sous un huitième
// de saturation du gris, et un orange sombre se lit brun.
constexpr float kBlackBelow = 0.22F;
constexpr float kGreyBelow = 0.14F;
constexpr float kWhiteAbove = 0.86F;
constexpr float kBrownBelow = 0.62F;
constexpr float kRedUpTo = 15.0F;
constexpr float kOrangeUpTo = 42.0F;
constexpr float kYellowUpTo = 70.0F;
constexpr float kGreenUpTo = 160.0F;
constexpr float kTurquoiseUpTo = 200.0F;
constexpr float kBlueUpTo = 255.0F;
constexpr float kPurpleUpTo = 290.0F;
constexpr float kPinkUpTo = 340.0F;

[[nodiscard]] std::array<float, 3> Channels(ImU32 color) {
  return {static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xFFU) / kFullChannel,
          static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xFFU) / kFullChannel,
          static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xFFU) / kFullChannel};
}

[[nodiscard]] std::uint8_t ToByte(float channel) {
  return static_cast<std::uint8_t>(std::clamp(std::lround(channel * kFullChannel), 0L, 255L));
}

// Teinte en degrés, saturation et clarté sur un.
struct HsvColor {
  float degrees = 0.0F;
  float saturation = 0.0F;
  float value = 0.0F;
};

[[nodiscard]] HsvColor HsvOf(ImU32 color) {
  const std::array<float, 3> rgb = Channels(color);
  float hue = 0.0F;
  HsvColor read;
  ImGui::ColorConvertRGBtoHSV(rgb[0], rgb[1], rgb[2], hue, read.saturation, read.value);
  read.degrees = hue * static_cast<float>(kDegreesPerTurn);
  return read;
}

// ── LE NUANCIER ─────────────────────────────────────────────────────────────

// LE CARRÉ ET LA BARRE DE TEINTE d'ImGui, sans son infobulle, son menu ni ses
// champs, tous en anglais. Rend vrai quand la couleur a changé.
[[nodiscard]] bool DrawSquare(std::array<float, 3>& rgb) {
  std::array<float, 4> picked = {rgb[0], rgb[1], rgb[2], 1.0F};
  if (!ImGui::ColorPicker4("##choix", picked.data(),
                           ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoAlpha |
                               ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions |
                               ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel |
                               ImGuiColorEditFlags_PickerHueBar)) {
    return false;
  }
  rgb = {picked[0], picked[1], picked[2]};
  return true;
}

// LE CODE DE LA COULEUR, sous le carré : `0xRRGGBB`, qu'un clic sélectionne en
// entier pour le copier, ou pour coller un autre code à sa place. Rend vrai
// quand un code entier vient d'être tapé ou collé.
//
// 🔴 HORS SAISIE, LE CHAMP DIT LA COULEUR RÉELLE : il se réécrit à chaque image
// depuis ce que la pastille montre, et ImGui ne lui préfère la saisie que tant
// qu'il tient le clavier. Une pièce qui n'a pas pu atteindre le code tapé le
// montre donc dès qu'on quitte le champ.
//
// ⚠ ON NE REPEINT QU'À LA FRAPPE. Tant que le champ est actif et diffère de la
// couleur, `InputText` rend vrai à CHAQUE image : s'y fier rejouerait le même
// code sans fin — et choisir une couleur, c'est l'imposer en couleur unique.
[[nodiscard]] bool DrawCode(std::array<float, 3>& rgb) {
  const ImGuiID field = ImGui::GetID(kCodeFieldId);
  std::array<char, kCodeFieldSize> typed{};
  const std::array<char, kColorCodeSize> code =
      FormatColorCode({ToByte(rgb[0]), ToByte(rgb[1]), ToByte(rgb[2])});
  std::copy(code.begin(), code.end(), typed.begin());
  const ImGuiStyle& style = ImGui::GetStyle();
  ImGui::SetNextItemWidth(ImGui::CalcTextSize(kWidestCode).x + style.FramePadding.x * 2.0F);
  static_cast<void>(
      ImGui::InputText(kCodeFieldId, typed.data(), typed.size(),
                       ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_CharsNoBlank));
  const bool active = ImGui::IsItemActive();
  SkinTooltip(
      core::Tr("Le code de cette couleur. Copie-le, ou colle un autre code à sa place : "
               "0xRRGGBB, #RRGGBB ou RRGGBB."));
  // Le libellé à droite du champ, comme le pose ImGui, sans entrer dans son
  // identifiant.
  ImGui::SameLine(0.0F, style.ItemInnerSpacing.x);
  ImGui::TextUnformatted(core::Tr("Code couleur"));
  const ImGuiInputTextState* const typing = active ? ImGui::GetInputTextState(field) : nullptr;
  if (typing == nullptr || !typing->EditedThisFrame) {
    return false;
  }
  const std::optional<std::array<std::uint8_t, 3>> read = ParseColorCode(typed.data());
  if (!read.has_value()) {
    return false;
  }
  rgb = {static_cast<float>((*read)[0]) / kFullChannel,
         static_cast<float>((*read)[1]) / kFullChannel,
         static_cast<float>((*read)[2]) / kFullChannel};
  return true;
}

// LE CHOIX D'UNE COULEUR, À L'ŒIL OU AU CODE : le carré, puis son code.
[[nodiscard]] bool DrawChoice(std::array<float, 3>& rgb) {
  const bool squared = DrawSquare(rgb);
  return DrawCode(rgb) || squared;
}

// LE NUANCIER D'UNE COULEUR EXACTE — la peau, une retouche : le carré et son
// code, puis cette couleur en trois curseurs, en degrés et en pour-cent. Rend
// vrai quand la couleur a changé.
[[nodiscard]] bool DrawPicker(std::array<float, 3>& rgb, const SkinArt& art) {
  bool changed = DrawChoice(rgb);
  float hue = 0.0F;
  float saturation = 0.0F;
  float value = 0.0F;
  ImGui::ColorConvertRGBtoHSV(rgb[0], rgb[1], rgb[2], hue, saturation, value);
  int degrees =
      static_cast<int>(std::lround(hue * static_cast<float>(kDegreesPerTurn))) % kDegreesPerTurn;
  int saturation_percent = static_cast<int>(std::lround(saturation * kPercent));
  int value_percent = static_cast<int>(std::lround(value * kPercent));
  bool moved =
      SkinSliderInt("##teinte", &degrees, 0, kDegreesPerTurn - 1, art, core::Tr("Teinte : %d°"));
  moved = SkinSliderInt("##saturation", &saturation_percent, 0, kPercent, art,
                        core::Tr("Saturation : %d %%")) ||
          moved;
  moved = SkinSliderInt("##clarte", &value_percent, 0, kPercent, art, core::Tr("Clarté : %d %%")) ||
          moved;
  if (moved) {
    ImGui::ColorConvertHSVtoRGB(static_cast<float>(degrees) / static_cast<float>(kDegreesPerTurn),
                                static_cast<float>(saturation_percent) / kPercent,
                                static_cast<float>(value_percent) / kPercent, rgb[0], rgb[1],
                                rgb[2]);
    changed = true;
  }
  return changed;
}

// LE MODE D'UNE PIÈCE, entre le carré et les curseurs : il dit ce que ceux-ci
// règlent. Rend le réglage passé dans l'autre mode, ou rien.
//
// 🔴 « GARDER LES NUANCES » DÉCALE LES COULEURS D'ORIGINE sans les écraser sur
// une seule, si bien qu'un tissu à reflets garde ses reflets. « COULEUR
// UNIQUE » pose une teinte et une saturation uniques : c'est elle qui colore
// un gris ou un blanc, qu'un décalage laisse gris — et c'est elle que donne
// une couleur désignée dans le carré.
[[nodiscard]] std::optional<render::RampAdjust> DrawMode(const render::RampAdjust& current,
                                                         ImU32 base_color, const SkinArt& art) {
  bool single_colour = current.absolute;
  if (SkinRadio(core::Tr("Couleur unique"), current.absolute, art)) {
    single_colour = true;
  }
  SkinTooltip(
      core::Tr("Toute la pièce prend la teinte et la saturation choisies : c'est ce qui "
               "colore aussi un gris ou un blanc."));
  ImGui::SameLine();
  if (SkinRadio(core::Tr("Garder les nuances"), !current.absolute, art)) {
    single_colour = false;
  }
  SkinTooltip(
      core::Tr("Les couleurs d'origine sont décalées ensemble : la pièce garde ses "
               "reflets et ses ombres colorées. Un gris reste gris."));
  if (single_colour == current.absolute) {
    return std::nullopt;
  }
  return PieceAdjustInMode(current, single_colour, base_color);
}

// LES TROIS CURSEURS DU NUANCIER : le réglage lui-même. Rend le réglage
// changé, ou rien.
//
// 🔴 UN SEUL JEU DE CURSEURS, où chaque nom ne figure qu'une fois : le carré
// désigne une couleur, eux disent le réglage qui y mène, et l'affinent.
// Montrer en plus la couleur visée en curseurs écrirait deux fois la teinte et
// la saturation, sous le même nom.
//
// ⚠ EN COULEUR UNIQUE, teinte et saturation sont celles que prend la pièce ; en
// nuances gardées, des écarts à ses couleurs d'origine, écrits avec leur
// signe. LA CLARTÉ EST UN ÉCART DANS LES DEUX : c'est elle qui porte le modelé,
// et sa borne haute est le plafond de la pièce.
[[nodiscard]] std::optional<render::RampAdjust> DrawSliders(const render::RampAdjust& current,
                                                            const SkinArt& art) {
  int hue = current.hue;
  int saturation = current.saturation;
  int value = current.value;
  bool changed = false;
  if (current.absolute) {
    changed =
        SkinSliderInt("##teinte", &hue, 0, kDegreesPerTurn - 1, art, core::Tr("Teinte : %d°"));
    SkinTooltip(core::Tr("La teinte que prend toute la pièce."));
    changed = SkinSliderInt("##saturation", &saturation, 0, kPercent, art,
                            core::Tr("Saturation : %d %%")) ||
              changed;
    SkinTooltip(core::Tr("À zéro, la pièce est grise ; à cent, sa couleur est franche."));
  } else {
    changed =
        SkinSliderInt("##teinte", &hue, -kHalfTurn, kHalfTurn, art, core::Tr("Teinte : %+d°"));
    SkinTooltip(core::Tr("Fait tourner les teintes d'origine de la pièce, en degrés."));
    changed = SkinSliderInt("##saturation", &saturation, -kPercent, kPercent, art,
                            core::Tr("Saturation : %+d %%")) ||
              changed;
    SkinTooltip(core::Tr("Moins, la pièce tourne au gris ; plus, ses couleurs se font vives."));
  }
  changed =
      SkinSliderInt("##clarte", &value, -kPercent, kPercent, art, core::Tr("Clarté : %+d %%")) ||
      changed;
  SkinTooltip(
      core::Tr("Plus sombre ou plus claire que le dessin d'origine, dont la pièce garde "
               "les ombres et les reflets."));
  if (!changed) {
    return std::nullopt;
  }
  render::RampAdjust next = current;
  next.hue = static_cast<std::int16_t>(hue);
  next.saturation = static_cast<std::int8_t>(saturation);
  next.value = static_cast<std::int8_t>(value);
  return next;
}

// 🔴 UN GRIS NE SE COLORE PAS PAR UN DÉCALAGE : sans le dire, les curseurs de
// teinte et de saturation auraient l'air cassés.
void DrawGreyHint(const render::RampAdjust& current, ImU32 base_color) {
  if (current.absolute || HsvOf(base_color).saturation >= kGreyBelow) {
    return;
  }
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::CalcItemWidth());
  ImGui::TextDisabled(
      "%s", core::Tr("Un gris n'a pas de teinte à décaler : « Couleur unique » le colore."));
  ImGui::PopTextWrapPos();
}

// ── UNE PIÈCE ───────────────────────────────────────────────────────────────

// Ce qu'une pièce montre d'elle.
struct PieceShown {
  std::size_t rank = 0;
  ImU32 color = IM_COL32_WHITE;
  ImU32 base_color = IM_COL32_WHITE;
  int ceiling = kFullValue;
  render::RampAdjust adjust;
  std::uint8_t link = 0;
  bool locked = false;
  bool pointed = false;
  // Une classe de teinte du reste, et non une pièce.
  bool rest = false;
};

// LA PASTILLE : elle ouvre le nuancier, se glisse sur une autre, reçoit une
// autre. Rend vrai quand la souris la survole.
bool DrawSwatch(const PieceShown& piece, PaletteEditorState& state, const PaletteEditorView& view,
                PaletteEditorAction& action) {
  const float side = kSwatchPoints * Scale();
  const std::array<float, 3> rgb = Channels(piece.color);
  // ⚠ SANS LE GLISSER NI L'INFOBULLE D'IMGUI : le premier n'est reçu par
  // aucune cible du client, la seconde est en anglais. On pose les nôtres.
  if (ImGui::ColorButton("##pastille", ImVec4(rgb[0], rgb[1], rgb[2], 1.0F),
                         ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoDragDrop |
                             ImGuiColorEditFlags_NoTooltip,
                         ImVec2(side, side))) {
    ImGui::OpenPopup("nuancier");
  }
  const bool hovered = ImGui::IsItemHovered();
  // 🔴 LA PIPETTE OUVRE LE NUANCIER DE LA PIÈCE QU'ELLE DÉSIGNE : un clic sur
  // la manche du pantin vaut un clic sur sa pastille. ⚠ PAS AVANT QUE LA LISTE
  // SOIT CELLE DE SON ONGLET : un clic sur un chapeau depuis le corps ouvre
  // d'abord l'onglet du chapeau.
  if (state.open_piece == static_cast<int>(piece.rank) && state.tab == view.tab) {
    ImGui::OpenPopup("nuancier");
    state.open_piece = -1;
  }

  // ── GLISSER UNE PASTILLE SUR UNE AUTRE ────────────────────────────────────
  if (ImGui::BeginDragDropSource()) {
    const int from = static_cast<int>(piece.rank);
    ImGui::SetDragDropPayload(kPiecePayload, &from, sizeof(from));
    ImGui::ColorButton("##glisse", ImVec4(rgb[0], rgb[1], rgb[2], 1.0F),
                       ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoTooltip,
                       ImVec2(side, side));
    ImGui::SameLine();
    ImGui::TextUnformatted(core::Tr("Lâche sur une pièce pour copier. Maj : lier."));
    ImGui::EndDragDropSource();
  }
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kPiecePayload)) {
      const int from = *static_cast<const int*>(dropped->Data);
      const auto to = static_cast<int>(piece.rank);
      if (from != to && from >= 0 && static_cast<std::size_t>(from) < view.colors.size()) {
        if (ImGui::GetIO().KeyShift) {
          action.link_from = from;
          action.link_to = to;
        } else {
          const std::array<float, 3> source = Channels(view.colors[static_cast<std::size_t>(from)]);
          action.picked_ramp = to;
          action.picked_color = {ToByte(source[0]), ToByte(source[1]), ToByte(source[2])};
        }
      }
    }
    ImGui::EndDragDropTarget();
  }
  return hovered;
}

// LE NOM : « Pièce 3 — rouge », et ce qui la distingue.
//
// 🔴 LA COULEUR NOMMÉE EST CELLE D'ORIGINE, ET SEULEMENT TANT QU'ELLE SE
// MONTRE : c'est par elle que le joueur reconnaît sa manche, mais une pièce
// réglée ne la porte plus, et le nom mentirait sur ce que la pastille montre.
struct PieceLabel {
  std::array<char, 128> text{};
};

PieceLabel LabelOf(const PieceShown& piece) {
  PieceLabel made;
  char* const label = made.text.data();
  const std::size_t size = made.text.size();
  const char* kind = piece.rest ? core::Tr("Reste") : core::Tr("Pièce");
  int written = 0;
  if (!piece.adjust.neutral()) {
    written = piece.rest ? std::snprintf(label, size, "%s", kind)
                         : std::snprintf(label, size, "%s %zu", kind, piece.rank + 1);
  } else {
    written = piece.rest
                  ? std::snprintf(label, size, "%s — %s", kind, PieceColorName(piece.base_color))
                  : std::snprintf(label, size, "%s %zu — %s", kind, piece.rank + 1,
                                  PieceColorName(piece.base_color));
  }
  const auto append = [label, size, &written](const char* tail) {
    if (written >= 0 && static_cast<std::size_t>(written) < size) {
      written +=
          std::snprintf(label + written, size - static_cast<std::size_t>(written), " %s", tail);
    }
  };
  const bool in_shadow = piece.ceiling < kShadowCeiling;
  if (in_shadow) {
    append(core::Tr("(ombre)"));
  }
  if (piece.link != 0) {
    append(core::Tr("(liée)"));
  }
  if (piece.locked) {
    append(core::Tr("(fixe)"));
  }
  return made;
}

bool DrawName(const PieceShown& piece) {
  const PieceLabel made = LabelOf(piece);
  const char* const label = made.text.data();
  const bool in_shadow = piece.ceiling < kShadowCeiling;
  // 🔴 `AlignTextToFramePadding`, ET SURTOUT PAS UN CURSEUR POSÉ À LA MAIN :
  // les pièces se dessineraient toutes au même endroit, empilées.
  ImGui::AlignTextToFramePadding();
  if (piece.pointed) {
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(kPipetteColor), "%s", label);
  } else {
    ImGui::TextUnformatted(label);
  }
  const bool hovered = ImGui::IsItemHovered();
  if (hovered) {
    char text[512] = {};
    int at = 0;
    // ⚠ L'EXPLICATION NE S'AFFICHE QUE POUR LES PIÈCES CONCERNÉES : une bulle
    // qui dirait « pas de limite » sur les autres noierait celle qui compte.
    if (in_shadow) {
      at = std::snprintf(text, sizeof(text),
                         core::Tr("Zone d'ombre du sprite : la clarté de cette pièce plafonne à "
                                  "%d %%. Les réglages gardent le modelé du dégradé, donc une "
                                  "pièce peinte sombre le reste — tu peux changer sa teinte, pas "
                                  "l'éclairer."),
                         (piece.ceiling * kPercent) / kFullValue);
    }
    const char* more = core::Tr("Clic droit : verrouiller, délier.");
    if (at >= 0 && static_cast<std::size_t>(at) < sizeof(text)) {
      std::snprintf(text + at, sizeof(text) - static_cast<std::size_t>(at), "%s%s",
                    at > 0 ? "\n" : "", more);
    }
    SkinTooltip(text);
  }
  return hovered;
}

// LE MENU D'UNE PIÈCE, au clic droit sur son nom.
void DrawPieceMenu(const PieceShown& piece, PaletteEditorAction& action) {
  if (!BeginSkinPopupContextItem("menu")) {
    return;
  }
  const auto rank = static_cast<int>(piece.rank);
  if (ImGui::MenuItem(piece.locked ? core::Tr("Libérer pour l'inspiration")
                                   : core::Tr("Garder pendant l'inspiration"))) {
    action.toggle_lock = rank;
  }
  if (piece.link != 0 && ImGui::MenuItem(core::Tr("Délier cette pièce"))) {
    action.unlink = rank;
  }
  if (!piece.adjust.neutral() && ImGui::MenuItem(core::Tr("Réinitialiser cette pièce"))) {
    action.reset_ramp = rank;
  }
  EndSkinPopup();
}

// Le nuancier de la pièce, s'il est ouvert : le carré, le mode, les trois
// curseurs. Rend vrai quand il l'est.
bool DrawPiecePopup(const PieceShown& piece, const SkinArt& art, PaletteEditorAction& action) {
  if (!BeginSkinPopup("nuancier")) {
    return false;
  }
  // 🔴 LE NUANCIER DIT QUELLE PIÈCE IL RÈGLE : il recouvre la liste, et l'on
  // perdrait de vue la ligne cliquée.
  ImGui::TextUnformatted(LabelOf(piece).text.data());
  ImGui::Separator();
  std::array<float, 3> rgb = Channels(piece.color);
  if (DrawChoice(rgb)) {
    action.picked_ramp = static_cast<int>(piece.rank);
    action.picked_color = {ToByte(rgb[0]), ToByte(rgb[1]), ToByte(rgb[2])};
  }
  std::optional<render::RampAdjust> adjust = DrawMode(piece.adjust, piece.base_color, art);
  if (const std::optional<render::RampAdjust> slid = DrawSliders(piece.adjust, art)) {
    adjust = slid;
  }
  if (adjust.has_value()) {
    action.adjusted_ramp = static_cast<int>(piece.rank);
    action.adjust = *adjust;
  }
  DrawGreyHint(piece.adjust, piece.base_color);
  if (SkinButton(core::Tr("Fermer"), art)) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
  return true;
}

// UNE RANGÉE : pastille, nom, croix. Rend vrai quand la souris la survole.
bool DrawPiece(const PieceShown& piece, PaletteEditorState& state, const PaletteEditorView& view,
               const SkinArt& art, PaletteEditorAction& action, bool& picker_open) {
  ImGui::PushID(static_cast<int>(piece.rank));
  bool hovered = DrawSwatch(piece, state, view, action);
  picker_open = DrawPiecePopup(piece, art, action) || picker_open;
  // 🔴 SUR LA MÊME LIGNE QUE LA PASTILLE : sans cet appel, chaque pièce prend
  // deux rangées, et le pantin, dont le côté vaut la hauteur calculée de la
  // liste, se retrouve deux fois trop petit.
  ImGui::SameLine();
  hovered = DrawName(piece) || hovered;
  DrawPieceMenu(piece, action);

  // 🔴 LA CROIX N'APPARAÎT QUE SI LA PIÈCE A ÉTÉ RETOUCHÉE : posée partout,
  // elle se lirait « supprimer la pièce ». « x » et non une flèche : la police
  // de l'interface ne couvre pas les icônes.
  if (!piece.adjust.neutral()) {
    ImGui::SameLine();
    if (SkinSmallButton("x", art)) {
      action.reset_ramp = static_cast<int>(piece.rank);
    }
    if (ImGui::IsItemHovered()) {
      hovered = true;
      SkinTooltip(core::Tr("Réinitialiser cette pièce"));
    }
  }
  ImGui::PopID();
  return hovered;
}

}  // namespace

bool DrawPaletteColorChoice(std::array<float, 3>& rgb) { return DrawChoice(rgb); }

bool DrawPaletteColorPicker(std::array<float, 3>& rgb, const SkinArt& art) {
  return DrawPicker(rgb, art);
}

render::RampAdjust PieceAdjustInMode(const render::RampAdjust& current, bool single_colour,
                                     ImU32 base_color) {
  if (current.absolute == single_colour) {
    return current;
  }
  const HsvColor base = HsvOf(base_color);
  const auto full_turn = static_cast<float>(kDegreesPerTurn);
  render::RampAdjust next = current;
  next.absolute = single_colour;
  if (single_colour) {
    // La couleur que les écarts donnaient à la pièce devient celle qu'on lui
    // impose. ⚠ LE RESTE D'UNE DIVISION GARDE LE SIGNE DE SON DIVIDENDE : une
    // teinte décalée vers le bas sortirait négative.
    float turned = std::fmod(base.degrees + static_cast<float>(current.hue), full_turn);
    if (turned < 0.0F) {
      turned += full_turn;
    }
    next.hue = static_cast<std::int16_t>(std::lround(turned) % kDegreesPerTurn);
    const float saturation = base.saturation * (1.0F + static_cast<float>(current.saturation) /
                                                           static_cast<float>(kPercent));
    next.saturation = static_cast<std::int8_t>(
        std::lround(std::clamp(saturation, 0.0F, 1.0F) * static_cast<float>(kPercent)));
    return next;
  }
  // Une couleur d'origine sans teinte n'a rien à tourner ni à raviver.
  if (base.saturation < kHuelessBelow) {
    next.hue = 0;
    next.saturation = 0;
    return next;
  }
  // L'écart le plus court vers la teinte imposée, d'un côté ou de l'autre.
  const auto half_turn = static_cast<float>(kHalfTurn);
  float turn = std::fmod(static_cast<float>(current.hue) - base.degrees, full_turn);
  if (turn > half_turn) {
    turn -= full_turn;
  } else if (turn < -half_turn) {
    turn += full_turn;
  }
  next.hue = static_cast<std::int16_t>(std::lround(turn));
  const float factor =
      static_cast<float>(current.saturation) / static_cast<float>(kPercent) / base.saturation -
      1.0F;
  next.saturation = static_cast<std::int8_t>(
      std::clamp(std::lround(factor * static_cast<float>(kPercent)), -static_cast<long>(kPercent),
                 static_cast<long>(kPercent)));
  return next;
}

const char* PieceColorName(ImU32 color) {
  const HsvColor read = HsvOf(color);
  const float degrees = read.degrees;
  const float saturation = read.saturation;
  const float value = read.value;
  if (value < kBlackBelow) {
    return core::Tr("noir");
  }
  if (saturation < kGreyBelow) {
    return value > kWhiteAbove ? core::Tr("blanc") : core::Tr("gris");
  }
  if (degrees >= kRedUpTo && degrees < kYellowUpTo && value < kBrownBelow) {
    return core::Tr("brun");
  }
  if (degrees < kRedUpTo || degrees >= kPinkUpTo) {
    return core::Tr("rouge");
  }
  if (degrees < kOrangeUpTo) {
    return core::Tr("orange");
  }
  if (degrees < kYellowUpTo) {
    return core::Tr("jaune");
  }
  if (degrees < kGreenUpTo) {
    return core::Tr("vert");
  }
  if (degrees < kTurquoiseUpTo) {
    return core::Tr("turquoise");
  }
  if (degrees < kBlueUpTo) {
    return core::Tr("bleu");
  }
  if (degrees < kPurpleUpTo) {
    return core::Tr("violet");
  }
  return core::Tr("rose");
}

PiecesAnswer DrawPalettePieces(PaletteEditorState& state, const PaletteEditorView& view,
                               const SkinArt& art, PaletteEditorAction& action) {
  PiecesAnswer answer;
  ImGui::BeginGroup();
  // 🔴 UN CALQUE PORTE SES RÉGLAGES À PART, et peut en avoir treize : ceux du
  // corps sont ceux de la recette v7, huit au plus.
  const bool layer = !view.adjusts.empty();
  const std::size_t count = layer ? std::min(view.ramps.size(), view.adjusts.size())
                                  : std::min(view.ramps.size(), render::kMaxRamps);
  // 🔴 LES PIÈCES SONT CELLES DE LEUR ONGLET, jusque dans leur identifiant : un
  // nuancier ouvert sur la pièce 1 d'un chapeau ne doit pas devenir celui de la
  // pièce 1 du corps si l'onglet change sous lui. Il disparaît avec sa liste.
  ImGui::PushID(static_cast<int>(view.tab));
  for (std::size_t rank = 0; rank < count; ++rank) {
    // 🔴 LE RESTE SOUS SON INTITULÉ : ce ne sont pas des pièces, mais chaque
    // couleur qu'aucune pièce ne couvre, réglée par sa famille de teinte.
    if (static_cast<int>(rank) == view.rest_first) {
      ImGui::AlignTextToFramePadding();
      ImGui::TextDisabled("%s", core::Tr("Le reste"));
      if (ImGui::IsItemHovered()) {
        SkinTooltip(
            core::Tr("Les couleurs qu'aucune pièce ne règle, par famille de teinte : chaque "
                     "pastille teint toutes celles de sa famille, et aucune pièce."));
      }
    }
    PieceShown piece;
    piece.rest = view.rest_first >= 0 && static_cast<int>(rank) >= view.rest_first;
    piece.rank = rank;
    piece.color = rank < view.colors.size() ? view.colors[rank] : IM_COL32_WHITE;
    piece.base_color = rank < view.base_colors.size() ? view.base_colors[rank] : piece.color;
    piece.ceiling = rank < view.ceilings.size() ? view.ceilings[rank] : kFullValue;
    piece.adjust = layer ? view.adjusts[rank] : view.recipe.ramps[rank];
    piece.link = rank < view.links.size() ? view.links[rank] : 0;
    piece.locked = (view.locked & (1U << rank)) != 0;
    piece.pointed = view.pipette_ramp == static_cast<int>(rank);
    if (DrawPiece(piece, state, view, art, action, answer.picker_open)) {
      answer.hovered = static_cast<int>(rank);
    }
  }
  ImGui::PopID();
  ImGui::EndGroup();
  return answer;
}

}  // namespace ro::ui
