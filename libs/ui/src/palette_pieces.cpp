#include "palette_pieces.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>

#include "ro/core/i18n.h"
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
// La rotation relative du mode avancé : un demi-tour de chaque côté suffit à
// atteindre toutes les teintes.
constexpr int kHalfTurn = 180;

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

// ── LE NUANCIER ─────────────────────────────────────────────────────────────

// LE NUANCIER D'UNE PIÈCE : le carré et la barre de teinte d'ImGui, sans son
// infobulle, son menu ni ses champs, tous en anglais ; puis nos trois
// curseurs, en degrés et en pour-cent. Rend vrai quand la couleur a changé.
[[nodiscard]] bool DrawPicker(std::array<float, 3>& rgb, const SkinArt& art) {
  bool changed = false;
  std::array<float, 4> picked = {rgb[0], rgb[1], rgb[2], 1.0F};
  if (ImGui::ColorPicker4("##choix", picked.data(),
                          ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoAlpha |
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions |
                              ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoLabel |
                              ImGuiColorEditFlags_PickerHueBar)) {
    rgb = {picked[0], picked[1], picked[2]};
    changed = true;
  }
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

// LE RÉGLAGE ABSOLU, à portée de main sous le nuancier : c'est lui qui colore
// un gris ou un blanc, qu'une rotation de teinte laisse gris.
//
// 🔴 LA ROTATION RELATIVE GARDE LA MATIÈRE : elle tourne les teintes de la
// pièce sans les écraser sur une seule, si bien qu'un tissu à reflets garde
// ses reflets. Le mode absolu, lui, pose une teinte et une saturation
// uniques. Rend le réglage changé, ou rien.
[[nodiscard]] std::optional<render::RampAdjust> DrawAbsoluteToggle(
    const render::RampAdjust& current, ImU32 base_color, const SkinArt& art) {
  render::RampAdjust next = current;
  bool changed = false;
  bool absolute = current.absolute;
  if (SkinCheckBox(core::Tr("Couleur unique (absolu)"), &absolute, art)) {
    next.absolute = absolute;
    if (absolute) {
      // On part de la couleur d'origine : basculer ne doit rien repeindre.
      const std::array<float, 3> rgb = Channels(base_color);
      float hue = 0.0F;
      float saturation = 0.0F;
      float value = 0.0F;
      ImGui::ColorConvertRGBtoHSV(rgb[0], rgb[1], rgb[2], hue, saturation, value);
      next.hue = static_cast<std::int16_t>(std::lround(hue * static_cast<float>(kDegreesPerTurn)) %
                                           kDegreesPerTurn);
      next.saturation = static_cast<std::int8_t>(std::lround(saturation * kPercent));
    } else {
      next.hue = 0;
      next.saturation = 0;
    }
    changed = true;
  }
  if (ImGui::IsItemHovered()) {
    SkinTooltip(
        core::Tr("Décoché : la teinte tourne, et la pièce garde sa matière — ses reflets, "
                 "ses ombres colorées. Coché : une teinte et une saturation uniques, qui "
                 "colorent aussi les gris et les blancs."));
  }
  if (!changed) {
    return std::nullopt;
  }
  return next;
}

// LE MODE AVANCÉ : le réglage lui-même, plutôt qu'une couleur à atteindre.
// Rend le réglage changé, ou rien.
[[nodiscard]] std::optional<render::RampAdjust> DrawAdvanced(const render::RampAdjust& current,
                                                             const SkinArt& art) {
  render::RampAdjust next = current;
  bool changed = false;
  int hue = next.hue;
  int saturation = next.saturation;
  int value = next.value;
  if (next.absolute) {
    changed = SkinSliderInt("##teinte_abs", &hue, 0, kDegreesPerTurn - 1, art,
                            core::Tr("Teinte : %d°")) ||
              changed;
    changed =
        SkinSliderInt("##sat_abs", &saturation, 0, kPercent, art, core::Tr("Saturation : %d %%")) ||
        changed;
  } else {
    changed = SkinSliderInt("##rotation", &hue, -kHalfTurn, kHalfTurn, art,
                            core::Tr("Rotation : %+d°")) ||
              changed;
    changed = SkinSliderInt("##sat_rel", &saturation, -kPercent, kPercent, art,
                            core::Tr("Saturation : %+d %%")) ||
              changed;
  }
  changed = SkinSliderInt("##clarte_rel", &value, -kPercent, kPercent, art,
                          core::Tr("Clarté : %+d %%")) ||
            changed;
  if (!changed) {
    return std::nullopt;
  }
  next.hue = static_cast<std::int16_t>(hue);
  next.saturation = static_cast<std::int8_t>(saturation);
  next.value = static_cast<std::int8_t>(value);
  return next;
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
bool DrawName(const PieceShown& piece) {
  char label[128] = {};
  const char* kind = piece.rest ? core::Tr("Reste") : core::Tr("Pièce");
  int written = 0;
  if (!piece.adjust.neutral()) {
    written = piece.rest ? std::snprintf(label, sizeof(label), "%s", kind)
                         : std::snprintf(label, sizeof(label), "%s %zu", kind, piece.rank + 1);
  } else {
    written = piece.rest ? std::snprintf(label, sizeof(label), "%s — %s", kind,
                                         PieceColorName(piece.base_color))
                         : std::snprintf(label, sizeof(label), "%s %zu — %s", kind, piece.rank + 1,
                                         PieceColorName(piece.base_color));
  }
  const auto append = [&label, &written](const char* tail) {
    if (written >= 0 && static_cast<std::size_t>(written) < sizeof(label)) {
      written += std::snprintf(label + written, sizeof(label) - static_cast<std::size_t>(written),
                               " %s", tail);
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

// Le nuancier de la pièce, s'il est ouvert. Rend vrai quand il l'est.
bool DrawPiecePopup(const PieceShown& piece, PaletteEditorState& state, const SkinArt& art,
                    PaletteEditorAction& action) {
  if (!BeginSkinPopup("nuancier")) {
    return false;
  }
  std::array<float, 3> rgb = Channels(piece.color);
  if (DrawPaletteColorPicker(rgb, art)) {
    action.picked_ramp = static_cast<int>(piece.rank);
    action.picked_color = {ToByte(rgb[0]), ToByte(rgb[1]), ToByte(rgb[2])};
  }
  if (const std::optional<render::RampAdjust> adjust =
          DrawAbsoluteToggle(piece.adjust, piece.base_color, art)) {
    action.adjusted_ramp = static_cast<int>(piece.rank);
    action.adjust = *adjust;
  }
  // D13 : replié par défaut, il ne s'adresse qu'à qui le cherche.
  ImGui::SetNextItemOpen(state.advanced);
  state.advanced = ImGui::TreeNode(core::Tr("Mode avancé"));
  if (state.advanced) {
    if (const std::optional<render::RampAdjust> adjust = DrawAdvanced(piece.adjust, art)) {
      action.adjusted_ramp = static_cast<int>(piece.rank);
      action.adjust = *adjust;
    }
    ImGui::TreePop();
  }
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
  picker_open = DrawPiecePopup(piece, state, art, action) || picker_open;
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

bool DrawPaletteColorPicker(std::array<float, 3>& rgb, const SkinArt& art) {
  return DrawPicker(rgb, art);
}

const char* PieceColorName(ImU32 color) {
  const std::array<float, 3> rgb = Channels(color);
  float hue = 0.0F;
  float saturation = 0.0F;
  float value = 0.0F;
  ImGui::ColorConvertRGBtoHSV(rgb[0], rgb[1], rgb[2], hue, saturation, value);
  const float degrees = hue * static_cast<float>(kDegreesPerTurn);
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
