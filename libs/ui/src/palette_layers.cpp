#include "palette_layers.h"

#include <cstddef>
#include <cstdio>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// Le repli du texte, en largeurs de police : la taille du texte est un réglage
// du joueur.
constexpr float kWrapInFonts = 24.0F;
// Le côté d'une pastille de teint, en hauteurs de ligne.
constexpr float kToneInFrames = 1.0F;
constexpr float kChannelMost = 255.0F;

// Le nom d'un calque, quand ce n'est pas un objet qui le nomme.
[[nodiscard]] const char* KindName(PaletteTabKind kind) {
  switch (kind) {
    case PaletteTabKind::kBody:
      return core::Tr("Corps");
    case PaletteTabKind::kSkin:
      return core::Tr("Peau");
    case PaletteTabKind::kHair:
      return core::Tr("Cheveux");
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

// Le libellé d'un onglet : « Coiffe haute : Valkyrie », une étoile tant qu'une
// retouche attend d'être validée.
//
// 🔴 L'IDENTIFIANT IMGUI EST LE NUMÉRO DU CALQUE, ET LUI SEUL : `###`, pas
// `##`. Avec `##`, le texte affiché entre dans l'identifiant ; l'étoile d'une
// retouche ou un autre objet porté en font alors un NOUVEL onglet, l'ancien
// disparaît, et la barre retombe sur le dernier onglet visité.
[[nodiscard]] std::string TabLabel(const PaletteTab& tab) {
  std::string label = KindName(tab.kind);
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

// LA PEAU : une rangée de teints, du plus clair au plus foncé.
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
    const ImU32 tone = view.skin_tones[rank];
    const ImVec4 color(static_cast<float>((tone >> IM_COL32_R_SHIFT) & 0xFFU) / kChannelMost,
                       static_cast<float>((tone >> IM_COL32_G_SHIFT) & 0xFFU) / kChannelMost,
                       static_cast<float>((tone >> IM_COL32_B_SHIFT) & 0xFFU) / kChannelMost, 1.0F);
    if (ImGui::ColorButton("##teint", color, ImGuiColorEditFlags_NoTooltip, ImVec2(side, side))) {
      action.skin_tone = static_cast<int>(rank);
    }
    ImGui::PopID();
  }
  Wrapped(core::Tr("Le visage et les bras se teignent ensemble : la pastille règle les deux."));
}

}  // namespace

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
        // ⚠ UN ONGLET OUVERT QUI DEVIENT NON RÉGLABLE — on monte en selle sur
        // l'onglet de la peau — REND LA MAIN À CELUI QUE LA VUE MONTRE : la
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
  } else if (tab->kind == PaletteTabKind::kSkin) {
    DrawSkinTones(view, action);
  }
}

}  // namespace ro::ui
