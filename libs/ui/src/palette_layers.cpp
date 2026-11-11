#include "palette_layers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "palette_pieces.h"

namespace ro::ui {
namespace {

// Le repli du texte, en largeurs de police : la taille du texte est un réglage
// du joueur.
constexpr float kWrapInFonts = 24.0F;
// Le côté d'une pastille de teint, en hauteurs de ligne.
constexpr float kToneInFrames = 1.0F;
constexpr float kChannelMost = 255.0F;
// Le rang 1 ne désigne plus d'onglet : la peau du visage se règle sous la Tête.
constexpr std::uint8_t kRetiredSkinTab = 1;
// Le nuancier de la ligne de peau, sous l'identifiant de sa ligne.
constexpr const char* kSkinPickerId = "nuancier_peau";

// Le libellé d'un onglet : « Coiffe haute : Valkyrie », une étoile tant qu'une
// retouche attend d'être validée.
//
// 🔴 L'IDENTIFIANT IMGUI EST LE NUMÉRO DU CALQUE, ET LUI SEUL : `###`, pas
// `##`. Avec `##`, le texte affiché entre dans l'identifiant ; l'étoile d'une
// retouche ou un autre objet porté en font alors un NOUVEL onglet, l'ancien
// disparaît, et la barre retombe sur le dernier onglet visité.
[[nodiscard]] std::string TabLabel(const PaletteTab& tab) {
  std::string label = PaletteTabName(tab.kind);
  if (!tab.name.empty()) {
    label += " : ";
    label += tab.name;
  }
  if (tab.edited) {
    label += " *";
  }
  label += "###";
  label += std::to_string(static_cast<unsigned>(tab.kind));
  return label;
}

void Wrapped(const char* text) {
  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * kWrapInFonts);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
}

void Explain(const char* text) {
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    SkinTooltip(text);
  }
}

// L'OBJET TEINT : la teinture lui appartient, et le suit.
void DrawItemNotes(const PaletteTab& tab, const PaletteEditorView& view, const SkinArt& art,
                   PaletteEditorAction& action) {
  Wrapped(
      core::Tr("Cette teinture appartient à cet item : elle le suit à l'échange, à la "
               "vente, au storage et au courrier."));
  Explain(
      core::Tr("Un costume et un chapeau ordinaire de même apparence ont chacun la leur, "
               "et deux exemplaires du même chapeau peuvent porter deux couleurs."));
  if (!view.atelier_open) {
    ImGui::TextDisabled("%s", core::Tr("Une teinture d'item se valide auprès du styliste."));
    Explain(
        core::Tr("L'aperçu est libre partout. Pour teindre ou déteindre l'item pour de "
                 "bon, parle au styliste : il t'ouvre son atelier pour quelques minutes."));
  }
  if (!tab.dyed) {
    return;
  }
  ImGui::BeginDisabled(!view.connected || !view.atelier_open);
  if (SkinSmallButton(core::Tr("Retirer la teinture de cet item"), art)) {
    ImGui::OpenPopup("retirer");
  }
  ImGui::EndDisabled();
  Explain(view.atelier_open ? core::Tr("L'item retrouve ses couleurs d'origine, pour toi comme "
                                       "pour les autres.")
                            : core::Tr("Seulement à l'atelier du styliste."));
  if (BeginSkinPopup("retirer")) {
    Wrapped(
        core::Tr("Retirer la teinture de cet item ? Il retrouve ses couleurs d'origine, "
                 "où qu'il aille ensuite."));
    if (SkinButton(core::Tr("Retirer"), art)) {
      action.clear_item_dye = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (SkinButton(core::Tr("Annuler"), art)) {
      ImGui::CloseCurrentPopup();
    }
    EndSkinPopup();
  }
}

// LES CHEVEUX : une couleur pour toutes les pièces, et la coupe neuve.
void DrawHairTools(const PaletteEditorView& view, const SkinArt& art, PaletteEditorAction& action) {
  if (view.ramps.size() > 1) {
    if (SkinSmallButton(core::Tr("Une couleur pour tous les cheveux"), art)) {
      action.one_hair_color = true;
    }
    Explain(
        core::Tr("Lie toutes les pièces des cheveux et leur donne la couleur de la "
                 "première : régler l'une règle ensuite toutes les autres."));
  }
  if (view.can_adapt_cut) {
    if (view.ramps.size() > 1) {
      ImGui::SameLine();
    }
    if (SkinSmallButton(core::Tr("Adapter à ma nouvelle coupe"), art)) {
      action.adapt_cut = true;
    }
    Explain(
        core::Tr("Reprend la teinture de ta coupe précédente sur celle-ci, pièce par "
                 "pièce, en rapprochant les teintes. Une teinture reste rangée sous sa "
                 "coupe et sa couleur : elle revient si tu les retrouves."));
  }
}

[[nodiscard]] std::array<float, 3> Channels(ImU32 color) {
  return {static_cast<float>((color >> IM_COL32_R_SHIFT) & 0xFFU) / kChannelMost,
          static_cast<float>((color >> IM_COL32_G_SHIFT) & 0xFFU) / kChannelMost,
          static_cast<float>((color >> IM_COL32_B_SHIFT) & 0xFFU) / kChannelMost};
}

[[nodiscard]] ImVec4 ToVec4(ImU32 color) {
  const std::array<float, 3> rgb = Channels(color);
  return {rgb[0], rgb[1], rgb[2], 1.0F};
}

[[nodiscard]] std::uint8_t ToByte(float channel) {
  return static_cast<std::uint8_t>(
      std::clamp(std::lround(channel * kChannelMost), 0L, static_cast<long>(kChannelMost)));
}

// Les teints proposés, du plus clair au plus foncé.
void DrawSkinTones(const PaletteEditorView& view, PaletteEditorAction& action) {
  if (view.skin_tones.empty()) {
    return;
  }
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(core::Tr("Teints :"));
  const float side = ImGui::GetFrameHeight() * kToneInFrames;
  for (std::size_t rank = 0; rank < view.skin_tones.size(); ++rank) {
    ImGui::SameLine();
    ImGui::PushID(static_cast<int>(rank));
    if (ImGui::ColorButton("##teint", ToVec4(view.skin_tones[rank]), ImGuiColorEditFlags_NoTooltip,
                           ImVec2(side, side))) {
      action.skin_tone = static_cast<int>(rank);
    }
    ImGui::PopID();
  }
}

// Le nuancier de la peau, s'il est ouvert. Rend vrai quand il l'est.
bool DrawSkinPopup(const PaletteSkinLine& line, const SkinArt& art, PaletteEditorAction& action) {
  if (!BeginSkinPopup(kSkinPickerId)) {
    return false;
  }
  std::array<float, 3> rgb = Channels(line.color);
  if (DrawPaletteColorPicker(rgb, art)) {
    action.skin_picked = true;
    action.skin_color = {ToByte(rgb[0]), ToByte(rgb[1]), ToByte(rgb[2])};
  }
  if (SkinButton(core::Tr("Fermer"), art)) {
    ImGui::CloseCurrentPopup();
  }
  EndSkinPopup();
  return true;
}

// Le retour de la ligne : la peau du dessin pour le visage, celle du visage
// pour un corps qui a la sienne.
void DrawSkinReset(const PaletteSkinLine& line, const SkinArt& art, PaletteEditorAction& action) {
  if (line.body ? !line.own : line.adjust.neutral()) {
    return;
  }
  ImGui::SameLine();
  if (line.body) {
    if (SkinSmallButton(core::Tr("Reprendre celle du visage"), art)) {
      action.skin_reset = true;
    }
    Explain(core::Tr("Le corps retire sa peau propre et suit de nouveau celle du visage."));
    return;
  }
  // « x » et non une flèche, comme sur une pièce : la police de l'interface
  // ne couvre pas les icônes.
  if (SkinSmallButton("x", art)) {
    action.skin_reset = true;
  }
  Explain(core::Tr("Rend au visage la peau du dessin."));
}

}  // namespace

std::optional<PaletteTabKind> PaletteTabOfNumber(std::uint8_t number) noexcept {
  if (number == 0 || number > kPaletteTabKinds) {
    return std::nullopt;
  }
  const auto rank = static_cast<std::uint8_t>(number - 1);
  if (rank == kRetiredSkinTab) {
    return PaletteTabKind::kHair;
  }
  return static_cast<PaletteTabKind>(rank);
}

const char* PaletteTabName(PaletteTabKind kind) {
  switch (kind) {
    case PaletteTabKind::kBody:
      return core::Tr("Corps");
    case PaletteTabKind::kHair:
      return core::Tr("Tête");
    case PaletteTabKind::kHeadgearTop:
      return core::Tr("Coiffe haute");
    case PaletteTabKind::kHeadgearMiddle:
      return core::Tr("Coiffe du milieu");
    case PaletteTabKind::kHeadgearLow:
      return core::Tr("Coiffe basse");
    case PaletteTabKind::kGarment:
      return core::Tr("Cape");
  }
  return "";
}

bool DrawSkinLine(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art,
                  PaletteEditorAction& action) {
  const PaletteSkinLine& line = view.skin;
  const bool blocked = line.blocked != PaletteTabBlock::kNone;
  // La demande de la pipette vaut pour la ligne de SON onglet : elle attend que
  // la vue en soit faite, et tombe si la ligne n'y est pas réglable.
  const bool asked = state.skin_open == view.tab;
  if (asked) {
    state.skin_open.reset();
  }
  if (!line.shown) {
    return false;
  }
  ImGui::PushID(line.body ? "peau_corps" : "peau_visage");
  ImGui::BeginDisabled(blocked);
  const float side = ImGui::GetFrameHeight() * kToneInFrames;
  if (ImGui::ColorButton("##peau", ToVec4(line.color), ImGuiColorEditFlags_NoTooltip,
                         ImVec2(side, side)) ||
      (asked && !blocked)) {
    ImGui::OpenPopup(kSkinPickerId);
  }
  bool hovered = ImGui::IsItemHovered();
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(line.body ? core::Tr("Peau du corps") : core::Tr("Peau du visage"));
  hovered = ImGui::IsItemHovered() || hovered;
  if (!blocked) {
    Explain(line.body ? core::Tr("Les bras et les jambes de ce corps, comme un vêtement porté "
                                 "par-dessus : un corps sans peau propre prend celle du visage.")
                      : core::Tr("Le visage, et le corps tant qu'il n'a pas sa propre peau."));
  }
  if (line.body && !line.own) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", core::Tr("(celle du visage)"));
  }
  DrawSkinReset(line, art, action);
  DrawSkinTones(view, action);
  const bool open = !blocked && DrawSkinPopup(line, art, action);
  ImGui::EndDisabled();
  if (blocked) {
    ImGui::TextDisabled("%s", TabBlockReason(line.blocked));
  }
  ImGui::PopID();
  state.skin_focus = hovered || open;
  return open;
}

const char* TabBlockReason(PaletteTabBlock blocked) {
  switch (blocked) {
    case PaletteTabBlock::kNone:
      return "";
    case PaletteTabBlock::kNoPiece:
      return core::Tr("Aucune pièce assez grande pour se régler.");
    case PaletteTabBlock::kGmOutfit:
      return core::Tr("L'habit du GM garde ses couleurs.");
    case PaletteTabBlock::kOrcHead:
      return core::Tr("La tête d'orc garde ses couleurs.");
    case PaletteTabBlock::kSkinOnMount:
      return core::Tr("Peau non réglable sur cette monture : elle peint aussi l'animal.");
    case PaletteTabBlock::kNotFound:
      return core::Tr("Cet item n'est pas dans ton équipement.");
    case PaletteTabBlock::kBodySkinNotServed:
      return core::Tr("Le serveur ne range pas encore la peau du corps : il suit celle du visage.");
  }
  return "";
}

const PaletteTab* OpenTab(const PaletteEditorView& view) {
  for (const PaletteTab& tab : view.tabs) {
    if (tab.kind == view.tab) {
      return &tab;
    }
  }
  return nullptr;
}

void DrawLayerTabs(PaletteEditorState& state, const PaletteEditorView& view, const SkinArt& art) {
  if (view.tabs.size() < 2 ||
      !BeginSkinTabBar("calques", art, ImGuiTabBarFlags_FittingPolicyScroll)) {
    return;
  }
  const bool forced = std::exchange(state.tab_asked, false);
  for (const PaletteTab& tab : view.tabs) {
    const bool blocked = tab.blocked != PaletteTabBlock::kNone;
    const ImGuiTabItemFlags flags =
        forced && state.tab == tab.kind ? ImGuiTabItemFlags_SetSelected : 0;
    ImGui::BeginDisabled(blocked);
    const std::string label = TabLabel(tab);
    if (ImGui::BeginTabItem(label.c_str(), nullptr, flags)) {
      if (blocked && !forced) {
        // ⚠ UN ONGLET OUVERT QUI DEVIENT NON RÉGLABLE — on enfile l'habit du
        // GM sous la Tête — REND LA MAIN À CELUI QUE LA VUE MONTRE : la
        // barre ne doit pas désigner un calque pendant que la liste est celle
        // d'un autre.
        state.tab = view.tab;
        state.tab_asked = true;
      } else if (!forced) {
        state.tab = tab.kind;
      }
      ImGui::EndTabItem();
    }
    ImGui::EndDisabled();
    if (blocked) {
      Explain(TabBlockReason(tab.blocked));
    } else if (tab.item) {
      Explain(core::Tr("La teinture de cet onglet appartient à l'item porté."));
    }
  }
  EndSkinTabBar();
}

void DrawLayerNotes(const PaletteEditorView& view, const SkinArt& art,
                    PaletteEditorAction& action) {
  const PaletteTab* tab = OpenTab(view);
  if (tab == nullptr) {
    return;
  }
  if (tab->item) {
    DrawItemNotes(*tab, view, art, action);
  }
  if (tab->kind == PaletteTabKind::kHair) {
    DrawHairTools(view, art, action);
  }
}

}  // namespace ro::ui
