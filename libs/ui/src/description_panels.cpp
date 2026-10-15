// LA DESCRIPTION D'UN OBJET QU'ON TIENT : ce qu'il porte en propre.
//
// Les cartes serties, les options aléatoires, la fiche du familier d'un œuf ne
// sont dans aucune table : ce sont des données d'INSTANCE. Elles se posent dans
// des panneaux accolés SOUS la fenêtre — le texte de
// l'objet garde toute sa hauteur —, et en fin d'aperçu.

#include <algorithm>
#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/ui/skin_palette.h"
#include "ro/ui/skinned_window.h"

#include "description_odds.h"

namespace ro::ui {
namespace {

// L'en-tête d'un panneau : le brun des titres de section du client.
constexpr ImU32 kPanelHeaderInk = IM_COL32(77, 61, 26, 255);
constexpr float kPanelRounding = 4.0F;
// La marge ajoutée à l'épaisseur du cadre, et l'écart entre deux panneaux.
constexpr float kPanelInsetPixels = 2.0F;
constexpr float kPanelGapPixels = 4.0F;
// L'interligne serré des listes d'un panneau.
constexpr ImVec2 kListSpacing(4.0F, 1.0F);

// La boîte d'une option : un trait gris arrondi autour de son libellé.
constexpr ImU32 kOptionBoxColor = IM_COL32(0xC2, 0xC2, 0xC2, 255);
constexpr float kOptionBoxRounding = 4.0F;
constexpr float kOptionPadXPixels = 6.0F;
constexpr float kOptionPadYPixels = 2.0F;
constexpr float kOptionGapPixels = 3.0F;
constexpr float kOptionLeastWidthPixels = 40.0F;

// LE PAPIER SOUS CE QU'ON ÉCRIT : le fond de la fenêtre en cours, que la
// description et ses panneaux posent tous deux.
[[nodiscard]] ImU32 PaperHere() {
  return ImGui::ColorConvertFloat4ToU32(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg));
}

// Le brun des en-têtes, lisible sur ce papier.
[[nodiscard]] ImVec4 HeaderColor() {
  return ImGui::ColorConvertU32ToFloat4(ReadableOn(kPanelHeaderInk, PaperHere()));
}

constexpr ImGuiWindowFlags kPanelFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;

// Le libellé d'une option : celui de la table, ou son numéro quand elle ne la
// connaît pas.
[[nodiscard]] std::string OptionLabel(const DescriptionOptionRow& option, bool with_param) {
  if (!option.text.empty()) {
    return std::string(option.text);
  }
  char label[128] = {};
  if (with_param) {
    std::snprintf(label, sizeof(label), core::Tr("Option #%d : valeur %d (param %u)"), option.id,
                  option.value, static_cast<unsigned>(option.param));
  } else {
    std::snprintf(label, sizeof(label), core::Tr("Option #%d : %d"), option.id, option.value);
  }
  return label;
}

// Ce que dit l'œuf : son familier, ou son absence.
void DrawPetFacts(const DescriptionView& view) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kListSpacing);
  if (!view.pet_inside) {
    // Un œuf sorti d'une commande de staff : le serveur le listera à
    // l'éclosion, puis le refusera.
    ImGui::TextDisabled("%s", core::Tr("Œuf vierge : aucun familier dedans."));
  } else {
    if (!view.pet_intimacy.empty()) {
      ImGui::TextDisabled("%s", core::Tr("Intimité"));
      ImGui::SameLine();
      ImGui::TextUnformatted(view.pet_intimacy.data(),
                             view.pet_intimacy.data() + view.pet_intimacy.size());
    }
    ImGui::TextDisabled(
        "%s", view.pet_renamed ? core::Tr("Déjà renommé") : core::Tr("Renommage disponible"));
  }
  ImGui::PopStyleVar();
}

// UN PANNEAU ACCOLÉ : sans barre, sans poignée, habillé du cadre des
// descriptions. Le cadre se peint APRÈS le contenu, sur le canal du dessous :
// on ne connaît la taille du panneau qu'une fois rempli.
class Panel {
 public:
  Panel(const std::string& id, ImVec2 at, float least_width, const SkinArt& art) : art_(art) {
    const float edge = DescPanelEdge(art) + kPanelInsetPixels;
    // LE PAPIER DE LA DESCRIPTION, que le panneau prolonge.
    PushPaperStyle(art.look());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, kPanelRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(edge, edge));
    ImGui::SetNextWindowPos(at, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(least_width, 0.0F), ImVec2(FLT_MAX, FLT_MAX));
    shown_ = ImGui::Begin(id.c_str(), nullptr, kPanelFlags);
    if (shown_) {
      ImGui::GetWindowDrawList()->ChannelsSplit(2);
      ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
    }
  }
  Panel(const Panel&) = delete;
  Panel& operator=(const Panel&) = delete;
  ~Panel() {
    if (shown_) {
      ImDrawList* const into = ImGui::GetWindowDrawList();
      into->ChannelsSetCurrent(0);
      DrawDescPanelFrameAroundWindow(into, art_, /*fill_bg=*/false);
      into->ChannelsMerge();
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(kPaperStyleColors);
  }

  [[nodiscard]] bool shown() const { return shown_; }
  // Le bas du panneau, écart compris : là où le suivant se pose.
  [[nodiscard]] static float NextTop() {
    return ImGui::GetWindowPos().y + ImGui::GetWindowSize().y + kPanelGapPixels;
  }
  // 🔴 LES PANNEAUX SUIVENT LA FENÊTRE AU PREMIER PLAN : ramenée devant, elle
  // les ramène avec elle — sinon une fenêtre voisine les couvrirait pendant
  // qu'on lit la description.
  static void FollowFront() { ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow()); }

 private:
  const SkinArt& art_;
  bool shown_ = false;
};

void DrawOptionBoxes(const DescriptionView& view) {
  const float line = ImGui::GetTextLineHeight();
  float widest = kOptionLeastWidthPixels;
  for (const DescriptionOptionRow& option : view.options) {
    widest = std::max(widest, ImGui::CalcTextSize(OptionLabel(option, true).c_str()).x);
  }
  const float box_width = widest + 2.0F * kOptionPadXPixels;
  const float box_height = line + 2.0F * kOptionPadYPixels;
  ImDrawList* const into = ImGui::GetWindowDrawList();
  const ImU32 ink = ImGui::GetColorU32(ImGuiCol_Text);
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, kOptionGapPixels));
  for (const DescriptionOptionRow& option : view.options) {
    const std::string label = OptionLabel(option, true);
    const ImVec2 top_left = ImGui::GetCursorScreenPos();
    into->AddRect(top_left, ImVec2(top_left.x + box_width, top_left.y + box_height),
                  kOptionBoxColor, kOptionBoxRounding);
    into->AddText(ImVec2(top_left.x + kOptionPadXPixels, top_left.y + kOptionPadYPixels), ink,
                  label.c_str());
    ImGui::Dummy(ImVec2(box_width, box_height));
  }
  ImGui::PopStyleVar();
}

void DrawCardRows(DescriptionWindowState& state, const DescriptionView& view,
                  DescriptionWindowAction& action) {
  if (view.card_slots > 0) {
    ImGui::TextColored(HeaderColor(), core::Tr("Cartes / Enchants (%d emplacement%s)"),
                       view.card_slots, view.card_slots > 1 ? "s" : "");
  } else {
    ImGui::TextColored(HeaderColor(), "%s", core::Tr("Cartes / Enchants"));
  }
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, kListSpacing);
  int row_index = 0;
  for (const DescriptionCardRow& card : view.cards) {
    ImGui::PushID(row_index++);
    if (card.id == 0) {
      ImGui::TextDisabled("%s", core::Tr("[Emplacement vide]"));
    } else {
      char shown[160] = {};
      if (card.name.empty()) {
        std::snprintf(shown, sizeof(shown), "#%u", static_cast<unsigned>(card.id));
      } else {
        std::snprintf(shown, sizeof(shown), core::Tr("%s (#%u)"), std::string(card.name).c_str(),
                      static_cast<unsigned>(card.id));
      }
      // ⚠ LES GESTES DE TOUT LIEN : décrire une carte REMPLACE l'objet affiché,
      // et « < Retour » y ramène.
      DrawDescriptionLink(state,
                          DescriptionLinkCell{.id = card.id,
                                              .name = card.name,
                                              .shown = shown,
                                              .with_icon = true,
                                              .icon = card.icon},
                          action);
    }
    ImGui::PopID();
  }
  ImGui::PopStyleVar();
}

}  // namespace

void DrawDescriptionPanels(DescriptionWindowState& state, const DescriptionView& view,
                           const SkinArt& art, ImVec2 under, float width, bool in_front,
                           const char* tag, DescriptionWindowAction& action) {
  ImVec2 at(under.x, under.y + kPanelGapPixels);
  const std::string suffix(tag);
  if (!view.options.empty()) {
    const Panel panel("##panneau_options_description_" + suffix, at, width, art);
    if (panel.shown()) {
      DrawOptionBoxes(view);
      at.y = Panel::NextTop();
      if (in_front) {
        Panel::FollowFront();
      }
    }
  }
  if (view.pet_egg) {
    const Panel panel("##panneau_familier_description_" + suffix, at, width, art);
    if (panel.shown()) {
      ImGui::TextColored(HeaderColor(), "%s", core::Tr("Pet"));
      DrawPetFacts(view);
      if (in_front) {
        Panel::FollowFront();
      }
    }
  } else if (!view.cards.empty()) {
    const Panel panel("##panneau_cartes_description_" + suffix, at, width, art);
    if (panel.shown()) {
      DrawCardRows(state, view, action);
      if (in_front) {
        Panel::FollowFront();
      }
    }
  }
}

void DrawItemDollTooltip(const DescriptionDollState& doll) {
  // Juste au-dessus du curseur, un peu à gauche : le pantin se tient au bas
  // de sa boîte, ses pieds près du curseur.
  constexpr ImVec2 kDollOffset(-16.0F, -10.0F);
  if (!doll.image.usable()) {
    return;
  }
  const ImVec2 mouse = ImGui::GetMousePos();
  ImGui::SetNextWindowPos(
      ImVec2(mouse.x + kDollOffset.x, mouse.y + kDollOffset.y - kDescriptionDollHeightPixels),
      ImGuiCond_Always);
  // ⚠ NI FOND NI BORD : seul le personnage se montre.
  ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32_BLACK_TRANS);
  ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32_BLACK_TRANS);
  if (ImGui::BeginTooltip()) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    DollOverlayCall call{
        .painter = doll.overlay.painter,
        .min = at,
        .max = ImVec2(at.x + kDescriptionDollWidthPixels, at.y + kDescriptionDollHeightPixels),
        .pass = DollOverlayPass::kBehind};
    const bool overlaid = doll.overlay.paint != nullptr;
    if (overlaid) {
      PaintDollOverlay(doll.overlay, call);
    }
    const ImVec2 doll_size(kDescriptionDollWidthPixels, kDescriptionDollHeightPixels);
    if (overlaid && doll.body_overlaid) {
      ImGui::Dummy(doll_size);
    } else {
      ImGui::Image(doll.image.id, doll_size);
    }
    if (overlaid) {
      call.pass = DollOverlayPass::kFront;
      PaintDollOverlay(doll.overlay, call);
    }
    ImGui::EndTooltip();
  }
  ImGui::PopStyleColor(2);
}

void DrawViewIdLine(DescriptionDollState& doll, const DescriptionView& view) {
  // Le bleu des liens de la description.
  constexpr ImU32 kViewIdColor = IM_COL32(41, 107, 224, 255);
  ImGui::BeginGroup();
  ImGui::PushStyleColor(ImGuiCol_Text, ReadableOn(kViewIdColor, PaperHere()));
  ImGui::Text(core::Tr("ViewID : %d"), static_cast<int>(view.view_id));
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::TextDisabled("%s", core::Tr("(molette : tourner)"));
  ImGui::EndGroup();
  if (ImGui::IsItemHovered()) {
    ShowDollOver(doll, ImGui::GetItemID());
  }
}

void ShowDollOver(DescriptionDollState& doll, ImGuiID wheel_id) {
  constexpr int kDirections = 8;
  // ⚠ ON DEMANDE, ON N'EFFACE PAS : la fenêtre remet la demande à zéro à chaque
  // image, et deux surfaces la partagent — la ligne et l'illustration.
  doll.wanted = true;
  ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  // ⚠ LA MOLETTE TOURNE LE PANTIN, PAS LA FENÊTRE : le portail en prend
  // possession tant que la surface est survolée.
  const float notches = WheelOver(doll.wheel, wheel_id, true);
  if (notches != 0.0F) {
    const int step = notches > 0.0F ? 1 : kDirections - 1;
    doll.direction = static_cast<std::uint8_t>((doll.direction + step) % kDirections);
  }
  DrawItemDollTooltip(doll);
}

void DrawPreviewInstance(const DescriptionView& view) {
  if (view.pet_egg && view.pet_inside) {
    ImGui::Separator();
    ImGui::TextColored(HeaderColor(), "%s", core::Tr("Pet"));
    DrawPetFacts(view);
  }
  const bool any_card =
      std::ranges::any_of(view.cards, [](const DescriptionCardRow& card) { return card.id != 0; });
  if (!view.pet_egg && any_card) {
    ImGui::Separator();
    ImGui::TextColored(HeaderColor(), "%s", core::Tr("Cartes / Enchants"));
    const float side = ImGui::GetTextLineHeight();
    for (const DescriptionCardRow& card : view.cards) {
      if (card.id == 0) {
        continue;
      }
      if (card.icon.usable()) {
        ImGui::Image(card.icon.id, ImVec2(side, side));
        ImGui::SameLine(0.0F, kListSpacing.x);
      }
      if (card.name.empty()) {
        ImGui::Text("#%u", static_cast<unsigned>(card.id));
      } else {
        ImGui::TextUnformatted(card.name.data(), card.name.data() + card.name.size());
      }
    }
  }
  if (!view.options.empty()) {
    ImGui::Separator();
    ImGui::TextColored(HeaderColor(), "%s", core::Tr("Options"));
    for (const DescriptionOptionRow& option : view.options) {
      ImGui::TextUnformatted(OptionLabel(option, false).c_str());
    }
  }
}

}  // namespace ro::ui
